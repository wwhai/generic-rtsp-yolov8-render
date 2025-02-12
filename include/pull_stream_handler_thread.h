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

#ifndef STREAM_HANDLER_H
#define STREAM_HANDLER_H
#include "thread_args.h"
void clone_and_enqueue(AVFrame *src_frame, FrameQueue *queue);
void handle_error(const char *message, int ret, AVFormatContext **fmt_ctx, AVPacket **origin_packet, AVCodecContext **codec_ctx);
void *pull_stream_handler_thread(void *arg);

#endif // STREAM_HANDLER_H
