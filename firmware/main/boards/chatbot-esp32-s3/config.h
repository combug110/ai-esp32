#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <driver/ledc.h>

#define Right_Pitch_Pin GPIO_NUM_NC  
#define Right_Roll_Pin GPIO_NUM_NC   
#define Left_Pitch_Pin GPIO_NUM_NC
#define Left_Roll_Pin GPIO_NUM_NC
#define Body_Pin GPIO_NUM_11    // body 
#define Head_Pin GPIO_NUM_10	// head

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_38
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_13
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_14
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_12
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_45

#define AUDIO_CODEC_USE_PCA9557
#define AUDIO_CODEC_PA_PIN       GPIO_NUM_NC
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_2
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_1
#define AXP2101_I2C_ADDR         0x34
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_ES7210_ADDR  0x82

// GPIO1 拉高用于使能板载音频功放
#define AUDIO_PA_ENABLE_PIN      GPIO_NUM_47

#define BUILTIN_LED_GPIO        GPIO_NUM_NC
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

#define DISPLAY_SPI_SCK_PIN     GPIO_NUM_41
#define DISPLAY_SPI_MOSI_PIN    GPIO_NUM_40
#define DISPLAY_DC_PIN          GPIO_NUM_39
#define DISPLAY_SPI_CS_PIN      GPIO_NUM_21

/******横屏显示******/
#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X true
#define DISPLAY_MIRROR_Y true
#define DISPLAY_SWAP_XY false

/******竖屏显示（可选）******/
/*
#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  320
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false
*/

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#define DISPLAY_BACKLIGHT_PIN          GPIO_NUM_42
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT true

/* ==================================================================
 * 摄像头 (OV2640, DVP)
 *
 * 接线与 dsh/sparkbot 是同一块硬件，引脚定义直接照搬：
 *   - SCCB (SIOD/SIOC) 与 ES8311 / AXP2101 复用同一条 I2C0 (SDA=1, SCL=2)
 *   - XCLK 用 20MHz（实测见下）：24MHz 时 LEDC 分频比为 1.667，时钟抖动会让
 *     OV2640 的 SCCB 读取出错（探针 10/10 能过，但 PID 只有 5/10 读对），
 *     导致 "Detected camera not supported"；20/16/10MHz 都是 10/10。
 *   - 三路供电挂在 AXP2101 上（见 board.cc 的 CameraPowerOn）：
 *       ALDO2  VDDCAM_3V3  摄像头 I/O
 *       BLDO1  AVDD        模拟电源
 *       BLDO2  DVDD        数字内核
 * ================================================================== */
#define CAMERA_PIN_XCLK        GPIO_NUM_8
#define CAMERA_PIN_PCLK        GPIO_NUM_16
#define CAMERA_PIN_VSYNC       GPIO_NUM_3
#define CAMERA_PIN_HREF        GPIO_NUM_46
#define CAMERA_PIN_PWDN        GPIO_NUM_48
#define CAMERA_PIN_RESET       GPIO_NUM_NC

/* -1 表示不新建 SCCB 总线，改用下面这个已初始化的 I2C 端口 (I2C_NUM_0) */
#define CAMERA_PIN_SIOD        (-1)
#define CAMERA_PIN_SIOC        (-1)
#define CAMERA_SCCB_I2C_PORT   0

#define CAMERA_PIN_D0          GPIO_NUM_7
#define CAMERA_PIN_D1          GPIO_NUM_5
#define CAMERA_PIN_D2          GPIO_NUM_4
#define CAMERA_PIN_D3          GPIO_NUM_6
#define CAMERA_PIN_D4          GPIO_NUM_15
#define CAMERA_PIN_D5          GPIO_NUM_17
#define CAMERA_PIN_D6          GPIO_NUM_18
#define CAMERA_PIN_D7          GPIO_NUM_9

#define CAMERA_XCLK_FREQ_HZ    20000000

/*
 * 相机 XCLK 由本固件用 LEDC 自己产生（原因见 CAMERA.md）：
 * S3 上 esp32-camera 把 XCLK 交给 LCD_CAM，但实测 LCD_CAM 的 cam_clk
 * 并没有输出到 XCLK 脚，OV2640 因此拿不到时钟、SCCB 完全不响应。
 * 所以这里用 LEDC 输出 XCLK（20MHz，理由见上），并让 esp32-camera 不要接管该引脚
 * （pin_xclk = -1）。
 *
 * S3 只有 8 个 LEDC 通道：背光占 TIMER_0/CH0，舵机占 TIMER_1/CH1~CH6
 * （见 oscillator.cc），相机固定占 TIMER_2/CH7，互不干扰。
 */
#define CAMERA_LEDC_TIMER      LEDC_TIMER_2
#define CAMERA_LEDC_CHANNEL    LEDC_CHANNEL_7

/*
 * VGA + RGB565：一帧 = 640*480*2 ≈ 600KB，双缓冲放 PSRAM（约 1.2MB）。
 * RGB565 交给上层编码成 JPEG 上传（Esp32Camera 会自动做，并在屏上回显预览）。
 */
#define CAMERA_FB_COUNT        2

/* 上电预热：丢弃前若干帧，等 AEC/AGC/AWB 收敛 */
#define CAMERA_WARMUP_FRAMES   10

/* AXP2101 摄像头供电目标电压 (mV)：500 + N*100 */
#define CAMERA_POWER_IO_MV     2800  /* ALDO2  VDDCAM_3V3 */
#define CAMERA_POWER_AVDD_MV   2800  /* BLDO1  AVDD */
#define CAMERA_POWER_DVDD_MV   1200  /* BLDO2  DVDD */

#endif // _BOARD_CONFIG_H_
