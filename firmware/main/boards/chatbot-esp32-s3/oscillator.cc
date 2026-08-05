#include "oscillator.h"

#include <driver/ledc.h>
#include <esp_timer.h>

#include <algorithm>
#include <cmath>

extern unsigned long IRAM_ATTR millis();

Oscillator::Oscillator(int trim) {
    trim_ = trim;
    diff_limit_ = 0;
    continuous_ = false;
    rotation_speed_degree_ = 30;
    rotation_ms_per_degree_ = 12;
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

    // pos_ = 90;
    // Write(pos_);
    previous_servo_command_millis_ = millis();

    is_attached_ = true;
    if (continuous_) {
        Write(90);  // output stop pulse immediately after attach
    }
}

void Oscillator::Detach() {
    if (!is_attached_)
        return;

    ESP_ERROR_CHECK(ledc_stop(ledc_speed_mode_, ledc_channel_, 0));

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

    // 360-degree servo: position is a rotation amount relative to the stop
    // point (90). Rotate at a fixed speed for a duration proportional to the
    // amount, then always return to the 90 (1.5ms) stop pulse.
    int delta = std::max(-60, std::min(60, position - 90));
    if (rev_)
        delta = -delta;
    pos_ = position;

    if (delta == 0) {
        Write(90);
        return;
    }

    int speed_angle = (delta > 0) ? (90 + rotation_speed_degree_)
                                  : (90 - rotation_speed_degree_);
    int hold_ms = std::abs(delta) * rotation_ms_per_degree_;

    Write(speed_angle);
    vTaskDelay(pdMS_TO_TICKS(hold_ms));
    Write(90);  // stop
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
