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

#ifndef THREAD_ARGS
#define THREAD_ARGS
#include "frame_queue.h"
#include "context.h"

typedef struct
{
    const char *rtsp_url;
    FrameQueue *video_queue;
    FrameQueue *detection_queue;
    FrameQueue *box_queue;
    Context *ctx;

} ThreadArgs;

#endif