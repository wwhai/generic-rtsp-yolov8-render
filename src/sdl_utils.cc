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

#include "sdl_utils.h"
#include "frame_queue.h"
void NV12ToRGB(uint8_t *y_plane, uint8_t *uv_plane, int width,
               int height, int y_pitch, int uv_pitch, uint8_t *rgb_buffer)
{
    for (int j = 0; j < height; ++j)
    {
        for (int i = 0; i < width; ++i)
        {
            // 获取 YUV 值
            int y = y_plane[j * y_pitch + i];
            int uv_index = (j / 2) * uv_pitch + (i & ~1);
            int u = uv_plane[uv_index] - 128;
            int v = uv_plane[uv_index + 1] - 128;

            // YUV 转 RGB
            int c = y - 16;
            int d = u;
            int e = v;

            int r = (298 * c + 409 * e + 128) >> 8;
            int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
            int b = (298 * c + 516 * d + 128) >> 8;

            // 裁剪到 [0, 255]
            r = r < 0 ? 0 : (r > 255 ? 255 : r);
            g = g < 0 ? 0 : (g > 255 ? 255 : g);
            b = b < 0 ? 0 : (b > 255 ? 255 : b);

            // 写入 RGB 缓冲区
            int rgb_index = (j * width + i) * 3;
            rgb_buffer[rgb_index] = r;
            rgb_buffer[rgb_index + 1] = g;
            rgb_buffer[rgb_index + 2] = b;
        }
    }
}
// 显示视频帧和绘制检测框的函数
// / 用 SDL_UpdateTexture 显示 NV12 帧
void SDLDisplayNV12Frame(SDL_Renderer *renderer, SDL_Texture *texture, AVFrame *frame)
{
    if (!frame->data[0] ||
        !frame->data[1] ||
        frame->width <= 0 ||
        frame->height <= 0)
    {
        fprintf(stderr, "frame is null or invalid\n");
        return;
    }

    int width = frame->width;
    int height = frame->height;
    int y_pitch = frame->linesize[0];
    int uv_pitch = frame->linesize[1];

    // 创建 RGB 缓冲区
    uint8_t *rgb_buffer = (uint8_t *)malloc(width * height * 3);
    if (!rgb_buffer)
    {
        fprintf(stderr, "Failed to allocate RGB buffer\n");
        return;
    }
    // 转换 NV12 为 RGB
    NV12ToRGB(frame->data[0], frame->data[1], width, height, y_pitch, uv_pitch, rgb_buffer);
    // 更新纹理
    if (SDL_UpdateTexture(texture, NULL, rgb_buffer, width * 3) < 0)
    {
        fprintf(stderr, "Failed to update texture: %s\n", SDL_GetError());
        free(rgb_buffer);
        return;
    }
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    free(rgb_buffer);
}
// 绘制矩形
// @param renderer 渲染器
// @param texture 纹理
// @param rect 矩形
void SDLDrawRect(SDL_Renderer *renderer, SDL_Texture *texture, SDL_Rect *rect)
{
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderDrawRect(renderer, rect);
    SDL_RenderPresent(renderer);
}
// 绘制文本
// @param renderer 渲染器
// @param texture 纹理
// @param text 文本
// @param x 文本的x坐标
// @param y 文本的y坐标
void SDLDrawText(SDL_Renderer *renderer, SDL_Texture *texture, TTF_Font *font, const char *text, int x, int y)
{
    // 设置文本颜色
    SDL_Color textColor = {255, 0, 0, 255};
    // 创建文本表面
    SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, textColor);
    if (textSurface == NULL)
    {
        fprintf(stderr, "Failed to render text: %s\n", TTF_GetError());
        return;
    }
    // 创建纹理
    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (textTexture == NULL)
    {
        fprintf(stderr, "Failed to create texture from surface: %s\n", SDL_GetError());
        SDL_FreeSurface(textSurface);
        return;
    }
    // 绘制文本
    SDL_Rect textRect = {x, y, textSurface->w, textSurface->h};
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
    // 释放资源
    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);
}
// 绘制标签
void SDLDrawLabel(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y, SDL_Color textColor, SDL_Color backgroundColor)
{
    // 创建文本表面
    SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, textColor);
    if (!textSurface)
    {
        printf("Unable to render text surface! SDL_ttf Error: %s\n", TTF_GetError());
        return;
    }
    // 创建一个矩形作为背景
    SDL_Rect backgroundRect = {x, y, textSurface->w, textSurface->h};

    // 绘制背景色
    SDL_SetRenderDrawColor(renderer, backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a);
    SDL_RenderFillRect(renderer, &backgroundRect);
    // 创建一个纹理用于渲染文本
    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_FreeSurface(textSurface);

    if (!textTexture)
    {
        printf("Unable to create texture from rendered text! SDL_Error: %s\n", SDL_GetError());
        return;
    }

    // 绘制文本
    SDL_Rect textRect = {x, y, textSurface->w, textSurface->h};
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
    SDL_DestroyTexture(textTexture);
}

void SDLDrawBox(SDL_Renderer *renderer, TTF_Font *font, const char *text,
                int x, int y, int w, int h, int thickness)
{
    // Draw the rectangle with the specified thickness
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

    // Draw top border
    SDL_Rect top = {x, y, w, thickness};
    SDL_RenderFillRect(renderer, &top);

    // Draw bottom border
    SDL_Rect bottom = {x, y + h - thickness, w, thickness};
    SDL_RenderFillRect(renderer, &bottom);

    // Draw left border
    SDL_Rect left = {x, y, thickness, h};
    SDL_RenderFillRect(renderer, &left);

    // Draw right border
    SDL_Rect right = {x + w - thickness, y, thickness, h};
    SDL_RenderFillRect(renderer, &right);

    // Render text above the rectangle
    if (text != NULL)
    {
        SDL_Color textColor = {255, 0, 0, 0}; // White color
        SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, textColor);
        SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);

        SDL_Rect textRect;
        textRect.x = x;
        textRect.y = y - textSurface->h; // Position text above the rectangle
        textRect.w = textSurface->w;
        textRect.h = textSurface->h;

        SDL_RenderCopy(renderer, textTexture, NULL, &textRect);

        SDL_FreeSurface(textSurface);
        SDL_DestroyTexture(textTexture);
    }
}
void RenderBox(SDL_Renderer *renderer, Box *box)
{
    SDL_Rect rect = {box->x, box->y, box->w, box->h};
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // 红色
    SDL_RenderDrawRect(renderer, &rect);
}
