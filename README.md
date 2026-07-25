# ai-esp32

TODO: 一句话项目简介。

## 目录结构

```
.
├── firmware/          # 源代码（单片机固件等）
├── hardware/
│   ├── pcb/           # PCB 设计文件（如 KiCad、Altium 工程文件）
│   ├── schematic/     # 原理图
│   ├── gerber/        # 生产用 Gerber 文件
│   └── bom/           # 物料清单 (Bill of Materials)
├── mechanical/
│   └── 3d-models/     # 3D 图纸（STEP、STL、SolidWorks 文件等）
├── docs/              # 技术文档
└── README.md
```

## 环境准备

本仓库使用 [Git LFS](https://git-lfs.com/) 管理 PCB / 3D 模型等二进制大文件，clone 后请先执行：

```sh
git lfs install
git lfs pull
```

## 快速开始

TODO: 固件如何编译烧录、硬件版本对应关系等。
