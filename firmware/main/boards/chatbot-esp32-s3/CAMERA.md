# Chatbot ESP32-S3 摄像头说明

摄像头硬件与 `dsh/sparkbot` 是同一块板：**OV2640 (DVP 8-bit)**。接线、供电
照搬 sparkbot，驱动用 xiaozhi 的 `Esp32Camera`（底层 `espressif/esp32-camera`），
输出 **VGA RGB565**，由固件编码成 JPEG 上传，配合服务端 `vision` 能力完成
「拍照 + AI 识图」，并在 LCD 上回显。

## 引脚

| 信号 | GPIO | 说明 |
|---|---|---|
| XCLK | 8 | 主时钟，**由本固件用 LEDC 产生 20MHz**（见下） |
| PCLK | 16 | 像素时钟 |
| VSYNC | 3 | 场同步 |
| HREF | 46 | 行有效 |
| PWDN | 48 | 掉电控制 |
| RESET | NC | 未接 |
| SIOD/SIOC | 复用 I2C0 | SDA=GPIO1 / SCL=GPIO2，与 ES8311 / AXP2101 同一条总线 |
| D0..D7 | 7, 5, 4, 6, 15, 17, 18, 9 | 8 位并口 |

### SCCB 复用已有 I2C 总线

摄像头的 SIOD/SIOC 与音频 codec、PMIC 接在同一条 I2C0 上。esp32-camera
在 IDF ≥ 5.4 下默认编译 `driver/sccb-ng.c`（新 `i2c_master` 驱动），因此
设置 `pin_sccb_sda = -1` + `sccb_i2c_port = I2C_NUM_0` 即可直接挂到板上
已经建好的 I2C0，不需要（也不能）再单独 init 一次。

## XCLK 为什么要自己用 LEDC 产生

在 ESP32-S3 上，esp32-camera 把 XCLK 交给 LCD_CAM 产生（`cam_clk_sel = 3`，
源是 160MHz PLL），`camera_config_t.ledc_timer/channel` 在 S3 上是被忽略的。

实测在本固件里**LCD_CAM 的 cam_clk 并没有输出到 GPIO8**：复刻它的全部寄存器
配置后，OV2640 依然完全不响应 SCCB（总线上扫不到 0x30）。而一旦用 LEDC 在
GPIO8 上输出时钟，传感器立刻恢复（`i2c_master_probe(0x30) == ESP_OK`，
能读到正确 PID `0x26/0x42`，`MID 0x7f/0xa2`）。

所以这里改用 LEDC 输出 XCLK，并把 `config.pin_xclk` 设为 `-1`，让
esp32-camera 不要去接管该引脚（否则它会把 GPIO8 从 LEDC 抢回 LCD_CAM）。
XCLK 必须全程保持。

### 频率取 20MHz 而不是 24MHz

不同 XCLK 频率下统计「探测 / 读 PID 是否读对」各 10 次：

| XCLK | probe(0x30) | PID 读对 |
|---|---|---|
| 20MHz | 10/10 | 10/10 |
| **24MHz** | 10/10 | **5/10** |
| 16MHz | 10/10 | 10/10 |
| 10MHz | 10/10 | 10/10 |

24MHz 时 LEDC 从 80MHz 分频得到的分频比是 1.667（非整数，输出带抖动），
会让 OV2640 的 SCCB 读数据出错：地址能 ACK（probe 通过），但读回的 PID 是
乱码，于是表现为 `Detected camera not supported`。20MHz 分频比是整数
（80/4），读数据稳定，所以这里取 **20MHz**（也是 OV2640 手册的典型值）。

## 供电（AXP2101）

三路 LDO 由 PMIC 提供，顺序必须是「先设电压、再使能」——冷启动时 DVDD 的
电压寄存器停在 2.8V 档，而 OV2640 的 DVDD 只要 1.2V。见
`chatbot_esp32_s3_board.cc` 的 `Pmic::CameraPowerOn()`。

| LDO | 目标电压 | 用途 |
|---|---|---|
| ALDO2 | 2800mV | VDDCAM_3V3（摄像头 I/O） |
| BLDO1 | 2800mV | AVDD（模拟） |
| BLDO2 | 1200mV | DVDD（数字内核） |

## LEDC 资源分配

ESP32-S3 只有 8 个 LEDC 通道，必须显式错开：

| 用途 | Timer | Channel |
|---|---|---|
| LCD 背光 | LEDC_TIMER_0 | CH0 |
| Body/Head 舵机 | LEDC_TIMER_1 | CH1~CH6 |
| 相机 XCLK | LEDC_TIMER_2 | CH7 |

因此 `oscillator.cc` 里舵机的通道轮转只在 1~6 之间进行，把 CH7 让给相机。

## 镜像

默认不做镜像/翻转（模组方向按实拍为准；如需可在 `InitializeCamera()` 里用
`camera_->SetHMirror()/SetVFlip()` 调整）。

## 用法

设备连接小智服务端后，`vision` 能力会下发 `explain` 地址，MCP 会自动注册
`self.camera.take_photo` 工具：用户要求「看看这是什么」时，固件抓一帧
RGB565，编码成 JPEG 上传到服务端识图，并在 LCD 上回显。没有插摄像头时
`GetCamera()` 返回 `nullptr`，该工具不会注册。
