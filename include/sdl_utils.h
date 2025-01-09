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

// @param y_plane Y平面
// @param uv_plane UV平面
// @param width 宽度
// @param height 高度
// @param y_pitch Y平面的步长
// @param uv_pitch UV平面的步长
// @param rgb_buffer 输出RGB数据的缓冲区
void NV12ToRGB(uint8_t *y_plane, uint8_t *uv_plane, int width,
               int height, int y_pitch, int uv_pitch, uint8_t *rgb_buffer);

// @param frame 必须是NV12格式
// @param texture 必须是YUV420P格式
// @param renderer 必须是YUV420P格式
void SDLDisplayNV12Frame(SDL_Renderer *renderer, SDL_Texture *texture, AVFrame *frame);

// 绘制矩形
// @param renderer 渲染器
// @param texture 纹理
// @param rect 矩形
void SDLDrawRect(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Rect *rect);
// 绘制文本
// @param renderer 渲染器
// @param texture 纹理
// @param text 文本
// @param x 文本的x坐标
// @param y 文本的y坐标
void SDLDrawText(SDL_Renderer *renderer, SDL_Texture *texture, TTF_Font *font, const char *text, int x, int y);

// 绘制标签
// @param renderer 渲染器
// @param texture 纹理
// @param text 文本
// @param x 文本的x坐标
// @param y 文本的y坐标
void SDLDrawLabel(SDL_Renderer *renderer, SDL_Texture *texture, TTF_Font *font, const char *text, int x, int y);

#endif