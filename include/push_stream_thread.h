// Copyright (C) 2025 wwhai
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#ifndef PUSH_RTMP_H
#define PUSH_RTMP_H

extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>
#include <libavutil/error.h>
#include <libavutil/avutil.h>
}
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "frame_queue.h" // 自定义队列头文件
#include "thread_args.h"
// 定义推流的 RTMP 地址
#define RTMP_URL "rtmp://192.168.10.7:1935/live/tlive001"

// 初始化 AVFormatContext 和 AVCodecContext
int init_av_contexts(const char *input_url, const char *output_url,
                     AVFormatContext **input_format_ctx, AVFormatContext **output_format_ctx,
                     AVCodecContext **input_codec_ctx, AVStream **input_stream, AVStream **output_stream);
// 将 AVFrame 编码为 AVPacket，并推送到 RTMP 服务器
int push_rtmp_frame(AVCodecContext *codec_ctx, AVFrame *frame, AVPacket *pkt, AVFormatContext *output_ctx);

// 推流线程处理函数
void *push_rtmp_handler_thread(void *arg);

#endif // PUSH_RTMP_H
