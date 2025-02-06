# 仓库介绍

从 RTSP 流拉取视频并渲染到图形界面，同时对每一帧进行YOLOV8推理，最后融合到视频输出。
![P1](image/readme/1736772995737.png)

## 环境要求

- **开发语言**：C
- **依赖库**：
      - ffmpeg/n7.1
      - sdl2/2.30.11
      - opencv/4.11.0
- **系统环境**：支持 Linux、Windows 等平台

## 编译与运行

### 编译项目

确保系统已安装依赖库，并通过 `pkg-config` 正确配置库路径。执行以下命令编译项目：

```bash
make
```

### 清理文件

```bash
make clean
```

### 运行程序

编译成功后，运行以下命令启动程序：
```bash
./generic-rtsp-yolov8-render.exe rtsp://192.168.10.8:554/av0_0
```

### Docker 环境
```sh
docker run --rm -it -p 1935:1935 -p 1985:1985 -p 8080:8080 ossrs/srs:5
```
