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
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "frame_queue.h" // 自定义队列头文件
#include "thread_args.h"
typedef struct
{
    AVFormatContext *output_ctx;
    AVStream *video_stream;
    AVCodecContext *codec_ctx;
    AVStream *input_stream;
} RtmpStreamContext;

/// @brief
/// @param ctx
/// @param output_url
/// @param width
/// @param height
/// @param fps
/// @return
int init_rtmp_stream(RtmpStreamContext *ctx, const char *output_url, int width, int height, int fps);
/// @brief
/// @param ctx
/// @param frame
void push_stream(RtmpStreamContext *ctx, AVFrame *frame);

/// @brief
/// @param arg
/// @return
void *push_rtmp_handler_thread(void *arg);

#endif // PUSH_RTMP_H
