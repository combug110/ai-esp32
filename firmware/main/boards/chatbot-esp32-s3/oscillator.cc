#include "oscillator.h"

#include <driver/ledc.h>
#include <esp_timer.h>
#include "esp_private/esp_gpio_reserve.h"

#include <algorithm>
#include <cmath>

extern unsigned long IRAM_ATTR millis();

static const char *TAG = "Osc";

Oscillator::Oscillator(int trim) {
    trim_ = trim;
    diff_limit_ = 0;
    continuous_ = false;
    // 18 -> ~1.30/1.70ms pulses: gentle rotation speed, clearly below the
    // 1.0/2.0ms full speed while still outside the typical servo deadband.
    rotation_speed_degree_ = 18;
    // Keep the per-move rotation short so the body/head turns are small.
    rotation_ms_per_degree_ = 3;
    accumulated_deg_ = 0;
    is_attached_ = false;

    sampling_period_ = 30;
    period_ = 2000;
    number_samples_ = period_ / sampling_period_;
    inc_ = 2 * M_PI / number_samples_;

    amplitude_ = 45;
    phase_ = 0;
    phase0_ = 0;
    offset_ = 0;
    stop_ = false;
    rev_ = false;

    pos_ = 90;
    previous_millis_ = 0;
}

Oscillator::~Oscillator() {
    Detach();
}

uint32_t Oscillator::AngleToCompare(int angle) {
    return (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) /
               (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) +
           SERVO_MIN_PULSEWIDTH_US;
}

bool Oscillator::NextSample() {
    current_millis_ = millis();

    if (current_millis_ - previous_millis_ > sampling_period_) {
        previous_millis_ = current_millis_;
        return true;
    }

    return false;
}

void Oscillator::Attach(int pin, bool rev) {
    // Idempotent: if we are already driving this pin, do nothing. Previously
    // every AttachServos() call tore the channel down and re-created it with
    // a different LEDC channel, which spammed "GPIO not usable" warnings and
    // left old channels behind.
    if (is_attached_ && pin_ == pin) {
        return;
    }
    if (is_attached_) {
        Detach();
    }

    pin_ = pin;
    rev_ = rev;

    ledc_timer_config_t ledc_timer = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                      .duty_resolution = LEDC_TIMER_13_BIT,
                                      .timer_num = LEDC_TIMER_1,
                                      .freq_hz = 50,
                                      .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    static int last_channel = 0;
    last_channel = (last_channel + 1) % 7 + 1;
    ledc_channel_ = (ledc_channel_t)last_channel;

    ledc_channel_config_t ledc_channel = {.gpio_num = pin_,
                                          .speed_mode = LEDC_LOW_SPEED_MODE,
                                          .channel = ledc_channel_,
                                          .intr_type = LEDC_INTR_DISABLE,
                                          .timer_sel = LEDC_TIMER_1,
                                          .duty = 0,
                                          .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ledc_speed_mode_ = LEDC_LOW_SPEED_MODE;

    previous_servo_command_millis_ = millis();

    is_attached_ = true;
    // (Re)attach starts from the 90 stop point, so the virtual position
    // resets too; otherwise repeated actions could be skipped by the
    // "already at position" dedup after an idle detach.
    pos_ = 90;
    accumulated_deg_ = 0;
    // Output the neutral 90 (1.5ms) pulse immediately after attach: with a
    // 0% duty the servo would otherwise snap to its end stop on power-up.
    Write(90);
}

void Oscillator::Detach() {
    if (!is_attached_)
        return;

    ESP_ERROR_CHECK(ledc_stop(ledc_speed_mode_, ledc_channel_, 0));
    // ledc_stop() does not release the GPIO reservation made on attach, so
    // re-attaching the same pin later would log "GPIO not usable, maybe
    // conflict with others". Revoke it here to keep detach/reattach cycles
    // warning-free.
    esp_gpio_revoke(1ULL << pin_);

    is_attached_ = false;
}

void Oscillator::SetT(unsigned int T) {
    period_ = T;

    number_samples_ = period_ / sampling_period_;
    inc_ = 2 * M_PI / number_samples_;
}

void Oscillator::SetPosition(int position) {
    if (!is_attached_)
        return;

    if (!continuous_) {
        Write(position);
        return;
    }

    if (position == pos_) {
        // Already at this virtual position: do not re-trigger a rotation.
        // This prevents unrelated actions (e.g. moving the head) from
        // re-commanding the body's current position and spinning it again.
        Write(90);
        return;
    }

    // 360-degree servo: the virtual position (0..180, 90 = center/stop)
    // maps to a physical rotation tracked by accumulated_deg_. Rotate by the
    // DIFFERENCE between the requested virtual position and the current one,
    // so a sequence like 60 -> 90 really bows down and then returns to the
    // center. Rotation amount is limited to +/-90 degrees total.
    int delta = std::max(-90, std::min(90, (position - 90) - accumulated_deg_));
    if (rev_)
        delta = -delta;
    pos_ = position;

    // Hard limit: the physical rotation may never leave +/-90 degrees in
    // TOTAL from the initial (stop) position, no matter how many actions ask
    // for the same direction. accumulated_deg_ tracks the true rotation.
    if (delta > 0) {
        delta = std::min(delta, 90 - accumulated_deg_);
    } else {
        delta = std::max(delta, -90 - accumulated_deg_);
    }
    if (delta == 0) {
        Write(90);
        return;
    }

    int speed_angle = (delta > 0) ? (90 + rotation_speed_degree_)
                                  : (90 - rotation_speed_degree_);
    int hold_ms = std::abs(delta) * rotation_ms_per_degree_;

    ESP_LOGI(TAG, "gpio%d ch%d rotate: pos=%d delta=%d pulse=%d hold=%dms", pin_, ledc_channel_,
             position, delta, speed_angle, hold_ms);
    Write(speed_angle);
    vTaskDelay(pdMS_TO_TICKS(hold_ms));
    Write(90);  // stop
    accumulated_deg_ += delta;
}

void Oscillator::Home() {
    if (!is_attached_ || !continuous_) {
        return;
    }
    if (accumulated_deg_ == 0) {
        Write(90);
        return;
    }

    // Rotate back by the total accumulated amount to return to the initial
    // (stop) position. Always within +/-90 by construction.
    int delta = -accumulated_deg_;
    int speed_angle = (delta > 0) ? (90 + rotation_speed_degree_)
                                  : (90 - rotation_speed_degree_);
    int hold_ms = std::abs(delta) * rotation_ms_per_degree_;
    ESP_LOGI(TAG, "gpio%d home: back %d deg", pin_, delta);
    Write(speed_angle);
    vTaskDelay(pdMS_TO_TICKS(hold_ms));
    Write(90);  // stop
    accumulated_deg_ = 0;
    pos_ = 90;
}

void Oscillator::Refresh() {
    if (NextSample()) {
        if (!stop_) {
            if (amplitude_ > 0) {
                int pos = std::round(amplitude_ * std::sin(phase_ + phase0_) + offset_);
                if (rev_)
                    pos = -pos;
                Write(pos + 90);
            }
            // Servos with zero amplitude are not part of this oscillation and
            // must not receive a speed pulse.
        }

        phase_ = phase_ + inc_;
    }
}

void Oscillator::Write(int position) {
    if (!is_attached_)
        return;

    if (continuous_) {
        // Continuous servo: position acts as a speed command centered on 90.
        // Trim is intentionally ignored so the stop pulse is exactly 1.5ms.
        int angle = std::min(std::max(position, 0), 180);
        uint32_t duty = (uint32_t)(((angle / 180.0) * 2.0 + 0.5) * 8191 / 20.0);
        ESP_ERROR_CHECK(ledc_set_duty(ledc_speed_mode_, ledc_channel_, duty));
        ESP_ERROR_CHECK(ledc_update_duty(ledc_speed_mode_, ledc_channel_));
        return;
    }

    long currentMillis = millis();
    if (diff_limit_ > 0) {
        int limit = std::max(
            1, (((int)(currentMillis - previous_servo_command_millis_)) * diff_limit_) / 1000);
        if (abs(position - pos_) > limit) {
            pos_ += position < pos_ ? -limit : limit;
        } else {
            pos_ = position;
        }
    } else {
        pos_ = position;
    }
    previous_servo_command_millis_ = currentMillis;

    int angle = pos_ + trim_;

    angle = std::min(std::max(angle, 0), 180);

    uint32_t duty = (uint32_t)(((angle / 180.0) * 2.0 + 0.5) * 8191 / 20.0);

    ESP_ERROR_CHECK(ledc_set_duty(ledc_speed_mode_, ledc_channel_, duty));
    ESP_ERROR_CHECK(ledc_update_duty(ledc_speed_mode_, ledc_channel_));
}
