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

#include "video_renderer.h"
#define TARGET_FPS 30                  // 目标帧率
#define FRAME_TIME (1000 / TARGET_FPS) // 每帧目标时间 (毫秒)
void *video_renderer_thread(void *arg)
{
    ThreadArgs *args = (ThreadArgs *)arg;

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return NULL;
    }
    if (TTF_Init() == -1)
    {
        fprintf(stderr, "SDL_ttf could not initialize! TTF_Error: %s\n", TTF_GetError());
        SDL_Quit();
        return NULL;
    }

    SDL_Window *window = SDL_CreateWindow("RTSP-VIDEO-PLAYER",
                                          SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          1920, 1080, SDL_WINDOW_SHOWN);
    if (!window)
    {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return NULL;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return NULL;
    }

    SDL_Texture *texture = SDL_CreateTexture(renderer,
                                             SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING,
                                             1920, 1080);
    if (!texture)
    {
        fprintf(stderr, "SDL_CreateTexture Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return NULL;
    }
    // 加载字体
    TTF_Font *font = TTF_OpenFont("SourceHanMonoSC-Light.ttf", 24);
    if (font == NULL)
    {
        fprintf(stderr, "Failed to load font! TTF_Error: %s\n", TTF_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return NULL;
    }

    int frameCount = 0;
    float fps = 0;
    char fpsLabel[20] = "FPS:00";
    Uint64 currentFrameTime = 0;
    Uint64 lastFrameTime = SDL_GetPerformanceCounter();
    Uint64 performanceFrequency = SDL_GetPerformanceFrequency();
    SDL_Event event;
    // loop
    while (1)
    {
        Uint32 frameStart = SDL_GetTicks();

        if (args->ctx->is_cancelled)
        {
            goto END;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // 处理视频队列中的帧
        QueueItem frame_item;
        memset(&frame_item, 0, sizeof(QueueItem));

        if (dequeue(args->video_queue, &frame_item))
        {
            if (frame_item.type == ONLY_FRAME && frame_item.data != NULL)
            {
                AVFrame *newFrame = (AVFrame *)frame_item.data;
                int ret = SDL_UpdateYUVTexture(texture, NULL,
                                               newFrame->data[0], newFrame->linesize[0],
                                               newFrame->data[1], newFrame->linesize[1],
                                               newFrame->data[2], newFrame->linesize[2]);
                if (ret < 0)
                {
                    fprintf(stderr, "SDL_UpdateYUVTexture failed: %s\n", SDL_GetError());
                }
                av_frame_free(&newFrame);
            }
        }
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        // 处理检测结果队列
        QueueItem boxes_item;
        if (async_dequeue(args->box_queue, &boxes_item))
        {
            if (boxes_item.type == ONLY_BOXES)
            {
                for (int i = 0; i < boxes_item.box_count; ++i)
                {
                    RenderBox(renderer, &boxes_item.Boxes[i]);
                    SDLDrawText(renderer, texture, font, boxes_item.Boxes[i].label, boxes_item.Boxes[i].x, boxes_item.Boxes[i].y);
                }
            }
        }
        // 计算FPS并显示
        frameCount++;
        currentFrameTime = SDL_GetPerformanceCounter();
        float elapsedTime = (currentFrameTime - lastFrameTime) / (float)performanceFrequency;
        if (elapsedTime >= 1.0f)
        {
            fps = frameCount / elapsedTime;
            frameCount = 0;
            lastFrameTime = currentFrameTime;
            sprintf(fpsLabel, "FPS: %.2f", fps);
        }

        SDL_Color textColor = {255, 0, 0, 255};       // 红色文本
        SDL_Color backgroundColor = {0, 255, 0, 255}; // 绿色背景
        SDLDrawLabel(renderer, font, (const char *)fpsLabel, 2, 2, textColor, backgroundColor);

        // 更新屏幕
        SDL_RenderPresent(renderer);

        Uint32 frameTime = SDL_GetTicks() - frameStart; // 当前帧消耗的时间
        if (frameTime < FRAME_TIME)
        {
            SDL_Delay(FRAME_TIME - frameTime); // 如果当前帧耗时少于目标时间，延迟剩余时间
        }

        // 事件处理
        while (SDL_PollEvent(&event) != 0)
        {
            if (event.type == SDL_QUIT)
            {
                goto END;
            }
            else if (event.type == SDL_KEYDOWN)
            {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                {
                    goto END;
                }
            }
        }
    }

END:
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    pthread_exit(NULL);
    return NULL;
}
