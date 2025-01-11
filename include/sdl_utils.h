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
#ifndef SDL_UTILS_H
#define SDL_UTILS_H
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
}
#include "frame_queue.h"
/// @brief
/// @param y_plane
/// @param uv_plane
/// @param width
/// @param height
/// @param y_pitch
/// @param uv_pitch
/// @param rgb_buffer
void NV12ToRGB(uint8_t *y_plane, uint8_t *uv_plane, int width,
               int height, int y_pitch, int uv_pitch, uint8_t *rgb_buffer);

/// @brief
/// @param renderer
/// @param texture
/// @param frame
void SDLDisplayNV12Frame(SDL_Renderer *renderer, SDL_Texture *texture, AVFrame *frame);

/// @brief
/// @param renderer
/// @param texture
/// @param font
/// @param text
/// @param x
/// @param y
void SDLDrawText(SDL_Renderer *renderer, SDL_Texture *texture, TTF_Font *font,
                 const char *text, int x, int y);

/// @brief
/// @param renderer
/// @param texture
/// @param font
/// @param text
/// @param x
/// @param y
void SDLDrawLabel(SDL_Renderer *renderer, TTF_Font *font, const char *text,
                  int x, int y, SDL_Color textColor, SDL_Color backgroundColor);
/// @brief
/// @param renderer
/// @param font
/// @param text
/// @param x
/// @param y
/// @param w
/// @param h
/// @param thickness
void SDLDrawBox(SDL_Renderer *renderer, TTF_Font *font, const char *text,
                int x, int y, int w, int h, int thickness);

/// @brief
/// @param renderer
/// @param box
void RenderBox(SDL_Renderer *renderer, Box *box);

#endif