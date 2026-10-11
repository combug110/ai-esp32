#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "esp_lcd_ili9341.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "axp2101.h"
#include "esp32_camera.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <wifi_station.h>

#define TAG "QuandongS3DevBoard"

// AXP2101 寄存器：摄像头三路供电相关（与 dsh/sparkbot 的 bot_hw_power.c 一致）
#define AXP2101_REG_LDO_ONOFF0  0x90  // bit1=ALDO2, bit4=BLDO1, bit5=BLDO2
#define AXP2101_REG_ALDO2_VOL   0x93  // 摄像头 I/O
#define AXP2101_REG_BLDO1_VOL   0x96  // AVDD
#define AXP2101_REG_BLDO2_VOL   0x97  // DVDD
#define AXP2101_LDO_ALDO2       0x02
#define AXP2101_LDO_BLDO1       0x10
#define AXP2101_LDO_BLDO2       0x20

// ILI9341 厂家自定义初始化序列
static const ili9341_lcd_init_cmd_t vendor_specific_init[] = {
    {0xCF, (uint8_t []){0x00, 0xC1, 0x30}, 3, 0},
    {0xED, (uint8_t []){0x64, 0x03, 0x12, 0x81}, 4, 0},
    {0xE8, (uint8_t []){0x85, 0x00, 0x78}, 3, 0},
    {0xCB, (uint8_t []){0x39, 0x2C, 0x00, 0x34, 0x02}, 5, 0},
    {0xF7, (uint8_t []){0x20}, 1, 0},
    {0xEA, (uint8_t []){0x00, 0x00}, 2, 0},
    {0xC0, (uint8_t []){0x13}, 1, 0},
    {0xC1, (uint8_t []){0x13}, 1, 0},
    {0xC5, (uint8_t []){0x22, 0x35}, 2, 0},
    {0xC7, (uint8_t []){0xBD}, 1, 0},
    {0, (uint8_t []){0}, 0x11, 0},
    {0x36, (uint8_t []){0x08}, 1, 0},
    {0xB6, (uint8_t []){0x0A, 0xA2}, 2, 0},
    {0x3A, (uint8_t []){0x55}, 1, 0},
    {0xF6, (uint8_t []){0x01, 0x30}, 2, 0},
    {0xB1, (uint8_t []){0x00, 0x1B}, 2, 0},
    {0xF2, (uint8_t []){0x00}, 1, 0},
    {0x26, (uint8_t []){0x01}, 1, 0},
    {0xE0, (uint8_t []){0x0F, 0x35, 0x31, 0x0B, 0x0E, 0x06, 0x49, 0xA7, 0x33, 0x07, 0x0F, 0x03, 0x0C, 0x0A, 0x00}, 15, 0},
    {0xE1, (uint8_t []){0x00, 0x0A, 0x0F, 0x04, 0x11, 0x08, 0x36, 0x58, 0x4D, 0x07, 0x10, 0x0C, 0x32, 0x34, 0x0F}, 15, 0},
    {0, (uint8_t []){0}, 0x11, 0},
    {0, (uint8_t []){0}, 0x29, 0},
};

// 控制器初始化函数声明
extern void InitializeElectronBotController();
extern void ElectronBotOnEmotionChanged(const char* emotion);

class Pmic : public Axp2101 {
public:
    Pmic(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : Axp2101(i2c_bus, addr) {
        // Enable battery detection so the charging state (STATUS1 bit3) is
        // accurate, otherwise the charger may report charging without battery.
        uint8_t value = ReadReg(0x68);
        WriteReg(0x68, value | 0x01);
    }

    // 打开 OV2640 的三路供电。硬件与 dsh/sparkbot 相同：
    //   ALDO2 = VDDCAM_3V3(摄像头 I/O)，BLDO1 = AVDD，BLDO2 = DVDD。
    //
    // 顺序必须是"先设电压，再使能"：本板冷启动时 DVDD 的电压寄存器停在
    // 2.8V 档，而 OV2640 的 DVDD 只要 1.2V。若直接使能，模组会以错误电压
    // 上电（可能损坏，或表现为 SCCB 探测不到 / "Detected camera not
    // supported"）。供电没起来时会误判成排线或模组故障，所以这里一次性
    // 把三路都配好。
    void CameraPowerOn() {
        SetLdoVoltage(AXP2101_REG_ALDO2_VOL, CAMERA_POWER_IO_MV);
        SetLdoVoltage(AXP2101_REG_BLDO1_VOL, CAMERA_POWER_AVDD_MV);
        SetLdoVoltage(AXP2101_REG_BLDO2_VOL, CAMERA_POWER_DVDD_MV);

        const uint8_t need = AXP2101_LDO_ALDO2 | AXP2101_LDO_BLDO1 | AXP2101_LDO_BLDO2;
        uint8_t onoff = ReadReg(AXP2101_REG_LDO_ONOFF0);
        if ((onoff & need) != need) {
            WriteReg(AXP2101_REG_LDO_ONOFF0, onoff | need);
            // LDO 稳定需要时间；不等的话 SCCB 探测会随机失败。
            vTaskDelay(pdMS_TO_TICKS(80));
        }
        ESP_LOGI(TAG, "Camera power: ALDO2=%dmV BLDO1=%dmV BLDO2=%dmV (0x90=0x%02X)",
                 CAMERA_POWER_IO_MV, CAMERA_POWER_AVDD_MV, CAMERA_POWER_DVDD_MV,
                 ReadReg(AXP2101_REG_LDO_ONOFF0));
    }

private:
    // AXP2101 LDO 电压寄存器：低 5 位 N，电压 = 500 + N*100 (mV)，高 3 位保留。
    void SetLdoVoltage(uint8_t reg, int millivolt) {
        if (millivolt < 500 || millivolt > 3500) {
            ESP_LOGW(TAG, "LDO %d mV out of range", millivolt);
            return;
        }
        int n = (millivolt - 500) / 100;
        uint8_t cur = ReadReg(reg);
        uint8_t target = (uint8_t)((cur & 0xE0) | (n & 0x1F));
        if (target != cur) {
            WriteReg(reg, target);
        }
    }
};

class QuandongS3DevBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_;
    Button boot_button_;
    LcdDisplay* display_ = nullptr;
    Pmic* pmic_ = nullptr;
    Esp32Camera* camera_ = nullptr;

    void InitializeI2c() {
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_SCK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    // 板上音频功放使能引脚，需置为低电平
    void InitializeAudioPaEnable() {
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << AUDIO_PA_ENABLE_PIN);
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&io_conf);
        gpio_set_level(AUDIO_PA_ENABLE_PIN, 1);
    }

    void InitializeButtons() {
        // 与 bread-compact-wifi 一致：启动期间按下 BOOT 进入配网，否则切换对话状态
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

   void InitializeController() { InitializeElectronBotController(); }

    // 用 LEDC 在相机 XCLK 脚上持续输出 24MHz。
    // 见 config.h / CAMERA.md：S3 上 esp32-camera 交给 LCD_CAM 产生 XCLK，
    // 但实测 cam_clk 没有输出，传感器拿不到时钟就不响应 SCCB。
    void StartCameraXclk() {
        ledc_timer_config_t timer = {};
        timer.speed_mode = LEDC_LOW_SPEED_MODE;
        timer.duty_resolution = LEDC_TIMER_1_BIT;
        timer.timer_num = CAMERA_LEDC_TIMER;
        timer.freq_hz = CAMERA_XCLK_FREQ_HZ;
        timer.clk_cfg = LEDC_AUTO_CLK;
        ESP_ERROR_CHECK(ledc_timer_config(&timer));

        ledc_channel_config_t channel = {};
        channel.gpio_num = CAMERA_PIN_XCLK;
        channel.speed_mode = LEDC_LOW_SPEED_MODE;
        channel.channel = CAMERA_LEDC_CHANNEL;
        channel.intr_type = LEDC_INTR_DISABLE;
        channel.timer_sel = CAMERA_LEDC_TIMER;
        channel.duty = 1;
        channel.hpoint = 0;
        ESP_ERROR_CHECK(ledc_channel_config(&channel));
    }

    void InitializeCamera() {
        // 先给摄像头上电，否则 esp_camera_init 探测不到传感器。
        if (pmic_ != nullptr) {
            pmic_->CameraPowerOn();
        }

        // XCLK：实测 S3 上 esp32-camera 走 LCD_CAM 的 cam_clk 没有输出到
        // XCLK 脚（传感器 SCCB 完全不响应）。这里用 LEDC 自己产生 24MHz，
        // 并让 esp32-camera 不接管该引脚（pin_xclk = -1），XCLK 一直保持。
        StartCameraXclk();
        // OV2640 DVP，接线与 dsh/sparkbot 一致。
        camera_config_t config = {};
        config.pin_pwdn       = CAMERA_PIN_PWDN;
        config.pin_reset      = CAMERA_PIN_RESET;
        config.pin_xclk       = -1;  // XCLK 由 LEDC 提供，别让相机驱动接管
        config.pin_sccb_sda   = CAMERA_PIN_SIOD;  // -1: 复用已有的 I2C0
        config.pin_sccb_scl   = CAMERA_PIN_SIOC;
        config.sccb_i2c_port  = CAMERA_SCCB_I2C_PORT;
        config.pin_d0         = CAMERA_PIN_D0;
        config.pin_d1         = CAMERA_PIN_D1;
        config.pin_d2         = CAMERA_PIN_D2;
        config.pin_d3         = CAMERA_PIN_D3;
        config.pin_d4         = CAMERA_PIN_D4;
        config.pin_d5         = CAMERA_PIN_D5;
        config.pin_d6         = CAMERA_PIN_D6;
        config.pin_d7         = CAMERA_PIN_D7;
        config.pin_vsync      = CAMERA_PIN_VSYNC;
        config.pin_href       = CAMERA_PIN_HREF;
        config.pin_pclk       = CAMERA_PIN_PCLK;

        config.xclk_freq_hz   = CAMERA_XCLK_FREQ_HZ;
        // LEDC 资源分配（S3 只有 8 个通道）：背光占 TIMER_0/CH0，
        // 舵机占 TIMER_1/CH1..6，所以相机 XCLK 单独用 TIMER_2/CH7。
        config.ledc_timer     = LEDC_TIMER_2;
        config.ledc_channel   = CAMERA_LEDC_CHANNEL;
        config.pixel_format   = PIXFORMAT_RGB565;
        config.frame_size     = FRAMESIZE_VGA;
        config.jpeg_quality   = 12;   // RGB565 模式下不生效，仅 JPEG 模式使用
        config.fb_count       = CAMERA_FB_COUNT;
        config.fb_location    = CAMERA_FB_IN_PSRAM;
        config.grab_mode      = CAMERA_GRAB_LATEST;

        camera_ = new Esp32Camera(config);

        // esp_camera_init 失败时 Esp32Camera 内部不会初始化传感器。
        // 没插摄像头时把相机当"不存在"，避免上层注册出永远失败的工具。
        if (esp_camera_sensor_get() == nullptr) {
            ESP_LOGW(TAG, "Camera not detected, photo tools disabled");
            delete camera_;
            camera_ = nullptr;
            return;
        }

        // 预热：上电后 AEC/AGC/AWB 是从默认值逐帧收敛的，头几帧增益拉满、
        // 白平衡没收敛。这里先丢几帧再交给上层，避免第一次拍照发暗/偏色。
        for (int i = 0; i < CAMERA_WARMUP_FRAMES; i++) {
            camera_fb_t *fb = esp_camera_fb_get();
            if (fb == nullptr) {
                // 拿不到帧（最多等 4s）就直接放弃预热，别拖住启动。
                ESP_LOGW(TAG, "Camera warmup aborted: no frame");
                break;
            }
            esp_camera_fb_return(fb);
            vTaskDelay(pdMS_TO_TICKS(20));
        }

    }

    void InitializeIli9341Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        ESP_LOGD(TAG, "Install LCD driver");
        const ili9341_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(ili9341_lcd_init_cmd_t),
        };

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.flags.reset_active_high = 1;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = (void *)&vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);

        display_ = new SpiLcdDisplay(panel_io, panel,
            DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
    }

public:
    QuandongS3DevBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        pmic_ = new Pmic(codec_i2c_bus_, AXP2101_I2C_ADDR);
        InitializeSpi();
        InitializeAudioPaEnable();
        InitializeIli9341Display();
        InitializeButtons();
        InitializeController();
        InitializeCamera();
        GetBacklight()->SetBrightness(100);
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(
            codec_i2c_bus_,
            I2C_NUM_0,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR,
            true,
            true);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }

    virtual bool GetBatteryLevel(int &level, bool& charging, bool& discharging) override {
        if (pmic_ == nullptr) {
            return false;
        }
        bool read_ok = true;
        charging = pmic_->IsCharging();
        read_ok &= pmic_->LastTransactionOk();
        discharging = pmic_->IsDischarging();
        read_ok &= pmic_->LastTransactionOk();
        level = pmic_->GetBatteryLevel();
        read_ok &= pmic_->LastTransactionOk();
        if (!read_ok || level < 0 || level > 100) {
            return false;
        }
        return true;
    }

    // Server emotion -> servo choreography
    void OnEmotionChanged(const char* emotion) override {
        ElectronBotOnEmotionChanged(emotion);
    }
};

DECLARE_BOARD(QuandongS3DevBoard);
