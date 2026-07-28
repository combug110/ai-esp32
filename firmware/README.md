# ai-esp32 firmware

An open-source AI companion robot for ESP32 — voice interaction plus physical
movement — built for developers outside China.

一款开源的 ESP32 AI 陪伴机器人固件——语音交互 + 肢体动作——面向海外开发者。

## Status / 状态

**Early-stage / work in progress.** This directory currently contains a vendored
copy of [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) (MIT licensed) as
a starting point. Large parts of it — board support, localization assets, default
service endpoints — still reflect the upstream project and will be reworked as
this project's own direction solidifies.

**早期阶段,开发中。** 本目录目前是从 [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)
(MIT 协议) fork 过来的固件代码,作为起点使用。其中大部分内容(板型支持、本地化资源、默认服务端点等)
仍然是上游项目的原样,会随着本项目自己的方向明确而逐步重写。

## About / 关于

This project aims to bring a low-cost, open, ESP32-based AI companion robot to
an international audience — not just a voice assistant, but one that can also
move (servo-driven gestures/expressions), following the direction started in
the `chatbot-esp32-s3` board (see `main/boards/chatbot-esp32-s3/`).

The on-device firmware stays intentionally thin on the AI side: it handles
audio capture/playback, wake-word detection, device state, and servo/motion
control — the heavy lifting (ASR, LLM, TTS) happens on a backend server over
WebSocket or MQTT+UDP, following the same protocol design as the upstream
project.

本项目希望做一款低成本、开源、基于 ESP32 的 AI 陪伴机器人,面向海外用户——不只是语音助手,
还能通过舵机做出动作/表情,延续 `chatbot-esp32-s3` 板型(见 `main/boards/chatbot-esp32-s3/`)
已经开始的方向。

设备端固件在 AI 能力上保持轻量:只负责录音/播放、唤醒词检测、设备状态管理,以及舵机/动作控制——
真正繁重的部分(ASR、LLM、TTS)跑在后端服务器上,通过 WebSocket 或 MQTT+UDP 通信,协议设计沿用上游项目。

## Attribution / 版权与致谢

The initial codebase is forked from [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32),
licensed under the MIT License. The original license and copyright notice are
preserved in [`LICENSE.xiaozhi-esp32`](LICENSE.xiaozhi-esp32), as required by
the MIT License. This project's own code is licensed under the top-level
[`LICENSE`](../LICENSE) (also MIT).

初始代码 fork 自 [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32),使用 MIT 协议。
原始的版权声明保留在 [`LICENSE.xiaozhi-esp32`](LICENSE.xiaozhi-esp32) 中,这是 MIT 协议要求的。
本项目自己新增的代码使用顶层 [`LICENSE`](../LICENSE)(同样是 MIT)。

## Development Environment / 开发环境

- [ESP-IDF](https://github.com/espressif/esp-idf): general boards target v6.0 or
  later (v6.0.2 preferred); the `chatbot-esp32-s3` board has been noted by its
  contributor as requiring **v5.5.4 or later** — this version mismatch hasn't
  been reconciled yet, check which board you're building for before picking an
  IDF version.
- Cursor or VS Code with the ESP-IDF extension
- Linux is recommended over Windows for faster builds and fewer driver issues

- [ESP-IDF](https://github.com/espressif/esp-idf):大部分板型面向 v6.0 及以上版本
  (推荐 v6.0.2);但 `chatbot-esp32-s3` 板型的贡献者备注需要 **v5.5.4 及以上** ——
  这个版本差异目前还没有统一,编译前请先确认自己要编的是哪个板型。
- 推荐用带 ESP-IDF 插件的 Cursor 或 VS Code
- 推荐用 Linux 而不是 Windows,编译更快、驱动问题更少

## Building / 编译

```sh
idf.py fullclean            # optional, recommended after switching boards/targets
idf.py set-target esp32s3   # or esp32, esp32c3, esp32c6, esp32p4, ...
idf.py menuconfig           # select board / configure options
idf.py build
```

```sh
idf.py fullclean            # 可选,切换板型/target 后建议执行
idf.py set-target esp32s3   # 或 esp32, esp32c3, esp32c6, esp32p4 等
idf.py menuconfig           # 选择板型 / 配置选项
idf.py build
```

## Flashing / 烧录

```sh
idf.py -p <PORT> flash monitor   # e.g. -p COM3 on Windows, -p /dev/ttyUSB0 on Linux/macOS
```

```sh
idf.py -p <端口> flash monitor   # 例如 Windows 下 -p COM3,Linux/macOS 下 -p /dev/ttyUSB0
```

## Wi-Fi Provisioning (BluFi) / 配网(BluFi)

1. **Trigger provisioning**: on first boot, or whenever no Wi-Fi credentials
   are saved, the device automatically enters provisioning mode.
2. **On your phone**: open the BluFi app (or another BluFi-compatible client),
   scan for and connect to the device, choose whether to encrypt the session,
   then enter your Wi-Fi SSID/password and send.
3. **Check the result**:
   - Success: BluFi reports a successful connection and the device joins your Wi-Fi.
   - Failure: BluFi reports a failure status — resend, or check your router.

1. **触发配网**:设备首次启动、或没有已保存的 Wi-Fi 信息时,会自动进入配网模式。
2. **手机端操作**:打开 BluFi App(或其他 BluFi 客户端),搜索并连接设备,可以选择是否加密,
   按提示输入 Wi-Fi SSID/密码并发送。
3. **查看结果**:
   - 成功:BluFi 报告连接成功,设备自动连接 Wi-Fi。
   - 失败:BluFi 返回失败状态,可重新发送或检查路由器。

## Repository Layout / 仓库结构

This `firmware/` directory is part of the larger `ai-esp32` project repo:
本 `firmware/` 目录是 `ai-esp32` 项目仓库的一部分:

```
ai-esp32-repo/
├── firmware/       ← you are here / 当前位置
├── hardware/       ← PCB, schematics, gerber, BOM
├── mechanical/     ← 3D models
└── docs/           ← project-level technical docs
```

See the top-level [`README.md`](../README.md) for the full project layout.
完整项目结构见顶层 [`README.md`](../README.md)。

## License / 许可证

MIT — see [`LICENSE`](../LICENSE).

MIT 协议——见 [`LICENSE`](../LICENSE)。
