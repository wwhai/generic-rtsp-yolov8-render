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

#ifndef PUSH_RTSP_H
#define PUSH_RTSP_H
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
#include "thread_args.h"
/// @brief 打开AVFormatContext
/// @param fmt_ctx
/// @param rtsp_url
/// @return
int OpenOutputAVFormatContext(AVFormatContext **fmt_ctx, const char *rtsp_url);
/// @brief 打开AVCodecContext
/// @param fmt_ctx
/// @param codec_ctx
/// @return
int OpenAVCodecContext(AVFormatContext *fmt_ctx, AVCodecContext **codec_ctx);
/// @brief 推送帧到 RTSP 服务器的函数
/// @param fmt_ctx
/// @param codec_ctx
/// @param frame
/// @return
int PushFrameToRTSPServer(AVFormatContext *fmt_ctx, AVCodecContext *codec_ctx, AVFrame *frame);

/// @brief 推送RTSP线程处理函数
/// @param arg
/// @return
void *push_rtsp_handler_thread(void *arg);
#endif