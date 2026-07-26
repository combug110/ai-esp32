# ai-esp32 firmware

An open-source AI voice assistant for ESP32 — built for developers outside China.

## Status

**Early-stage / work in progress.** This directory currently contains a vendored
copy of [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) (MIT licensed) as
a starting point. Large parts of it — board support, localization assets, default
service endpoints — still reflect the upstream project and will be reworked as
this project's own direction solidifies.

## About

This project aims to bring a low-cost, open, ESP32-based voice assistant to an
international audience. The on-device firmware stays intentionally thin: it
handles audio capture/playback, wake-word detection, and device state — the
heavy lifting (ASR, LLM, TTS) happens on a backend server over WebSocket or
MQTT+UDP, following the same protocol design as the upstream project.

## Attribution

The initial codebase is forked from [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32),
licensed under the MIT License. The original license and copyright notice are
preserved in [`LICENSE.xiaozhi-esp32`](LICENSE.xiaozhi-esp32), as required by
the MIT License. This project's own code is licensed under the top-level
[`LICENSE`](../LICENSE) (also MIT).

## Development Environment

- [ESP-IDF](https://github.com/espressif/esp-idf) v6.0 or later (v6.0.2 preferred)
- Cursor or VS Code with the ESP-IDF extension
- Linux is recommended over Windows for faster builds and fewer driver issues

## Building

```sh
idf.py set-target esp32s3   # or esp32, esp32c3, esp32c6, esp32p4, ...
idf.py menuconfig           # select board / configure options
idf.py build
idf.py flash monitor
```

## Repository Layout

This `firmware/` directory is part of the larger `ai-esp32` project repo:

```
ai-esp32-repo/
├── firmware/       ← you are here
├── hardware/       ← PCB, schematics, gerber, BOM
├── mechanical/     ← 3D models
└── docs/           ← project-level technical docs
```

See the top-level [`README.md`](../README.md) for the full project layout.

## License

MIT — see [`LICENSE`](../LICENSE).
