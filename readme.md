# 仓库介绍

拉取视频流并渲染到图形界面，同时对每一帧进行YOLOV8推理，最后融合到视频输出。

---

## 拓扑
![2](image/1739374151379.png)

## 效果
![1](image/1739373972986.png)

## 环境要求

- **开发语言**：C
- **依赖库**：
    - ffmpeg/7
    - sdl2/2.30.11
    - opencv/4.11.0
    - libcurl/7
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

### RTSP

```bash
./generic-stream-yolov8-render rtsp://192.168.10.6:554/av0_0 rtmp://192.168.10.5:1935/live/tlive001
```

### 摄像头
```sh
./generic-stream-yolov8-render "1080P USB Camera"  "rtmp://192.168.10.7:1935/live/tlive001"
```

### Docker 环境
```sh
docker run --rm -it -p 1935:1935 -p 1985:1985 -p 8080:8080 ossrs/srs:5
```
