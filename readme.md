<!--
 Copyright (C) 2025 wwhai

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU Affero General Public License as
 published by the Free Software Foundation, either version 3 of the
 License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Affero General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>.
-->
# RTSP Monitor

## 项目简介
RTSP Monitor 是一个基于 **libav*** 和 **SDL2** 的轻量级监控软件，支持从 RTSP 流拉取视频并渲染到图形界面，同时对每一帧进行图像处理。

### 功能特性
1. **RTSP 拉流**：通过提供 RTSP URL，从流媒体服务器拉取视频流。
2. **多线程处理**：
   - 线程 A：负责实时播放原始视频帧。
   - 线程 B：负责对视频帧进行图像识别处理。
3. **动态叠加检测框**：在播放器界面上实时绘制检测出的矩形框及标记文本：
   - 矩形框为红色，标记文本为绿色。
4. **硬件加速支持**：SDL2 硬件加速提升渲染性能。

---

## 项目结构
```
.
├── include/              # 头文件目录
│   ├── frame_queue.h      # 帧队列相关接口
│   ├── video_renderer.h   # 视频渲染接口
│   ├── video_processor.h  # 视频处理接口
│   ├── rtsp_client.h      # RTSP 拉流接口
├── src/                  # 源代码目录
│   ├── frame_queue.c      # 帧队列实现
│   ├── video_renderer.c   # 视频渲染实现
│   ├── video_processor.c  # 视频处理实现
│   ├── rtsp_client.c      # RTSP 拉流实现
│   ├── main.c             # 主程序入口
├── obj/                  # 中间文件存放目录
├── Makefile              # 构建脚本
├── README.md             # 项目说明文档
```

---

## 环境要求
- **开发语言**：C
- **依赖库**：
  - libavformat
  - libavcodec
  - libavutil
  - libswscale
  - SDL2
- **系统环境**：支持 Linux、Windows 等平台

---

## 编译与运行
### 编译项目
确保系统已安装依赖库，并通过 `pkg-config` 正确配置库路径。执行以下命令编译项目：

```bash
make
```

### 清理文件
删除生成的中间文件和可执行文件：
```bash
make clean
```

### 运行程序
编译成功后，运行以下命令启动程序：
```bash
./rtsp_monitor <RTSP_URL>
```
示例：
```bash
./rtsp_monitor rtsp://192.168.1.10:554/live
```

---

## 核心模块说明
### 1. RTSP 拉流模块 (`rtsp_client.c`)
负责通过 RTSP URL 拉取视频流，解析为帧并推送到帧队列。

### 2. 视频渲染模块 (`video_renderer.c`)
负责从队列中读取原始视频帧，并通过 SDL2 硬件加速渲染到图形界面。

### 3. 视频处理模块 (`video_processor.c`)
负责对帧进行识别处理（具体识别逻辑由用户实现），并生成矩形框及标记信息。

### 4. 帧队列模块 (`frame_queue.c`)
实现一个线程安全的队列，用于存储从 RTSP 流中拉取的视频帧。

---

## 注意事项
1. **硬件加速**：
   - 确保系统支持 SDL2 的硬件加速功能，并正确启用相关依赖。
2. **依赖库安装**：
   - 在 Linux 上安装依赖库：
     ```bash
     sudo apt install libsdl2-dev libavcodec-dev libavformat-dev libavutil-dev libswscale-dev
     ```
3. **多线程安全**：
   - 本项目中的帧队列模块使用 `pthread` 实现线程安全，确保多线程访问不冲突。

---

## TODO
1. 优化队列内存管理，支持动态扩容。
2. 增加支持多种流媒体协议（如 HTTP、HLS）。
3. 提供更多检测结果的展示方式。

---

## 许可证
本项目遵循 [MIT License](LICENSE) 协议开源。

---

## 联系方式
如果有任何问题或建议，请联系作者：
- **邮箱**：cnwwhai@gmail.com