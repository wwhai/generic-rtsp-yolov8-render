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

#ifndef VIDEO_RENDERER_H
#define VIDEO_RENDERER_H

extern "C"
{
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
}
#include "frame_queue.h"
#include "libav_utils.h"
#include "sdl_utils.h"
#include "thread_args.h"

void *video_renderer_thread(void *arg);

#endif // VIDEO_RENDERER_H
