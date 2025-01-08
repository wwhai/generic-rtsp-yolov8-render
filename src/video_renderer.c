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
#include "frame_queue.h"
#include <SDL2/SDL.h>

void *video_renderer_thread(void *arg)
{
    FrameQueue *video_queue = (FrameQueue *)arg;

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return NULL;
    }

    SDL_Window *win = SDL_CreateWindow("RTSP Player", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN);
    if (!win)
    {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return NULL;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return NULL;
    }

    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, 640, 480);
    if (!texture)
    {
        fprintf(stderr, "SDL_CreateTexture Error: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return NULL;
    }

    int frameCount = 0;
    float fps = 0;
    Uint64 currentFrameTime = 0;
    Uint64 lastFrameTime = SDL_GetPerformanceCounter();
    Uint64 performanceFrequency = SDL_GetPerformanceFrequency();

    QueueItem item;
    memset(&item, 0, sizeof(QueueItem));
    SDL_Event event;
    AVFrame *lastFrame = NULL; // 缓存最后一帧

    while (1)
    {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        // 尝试从队列获取新帧
        if (dequeue(video_queue, &item))
        {
            printf("dequeue\n");
            if (item.type == ONLY_FRAME && item.data != NULL)
            {
                if (lastFrame != NULL)
                {
                    av_frame_free(&lastFrame);
                    lastFrame = NULL;
                }
                AVFrame *newFrame = (AVFrame *)item.data;
                int ret = SDL_UpdateYUVTexture(texture, NULL,
                                               newFrame->data[0], newFrame->linesize[0], newFrame->data[1],
                                               newFrame->linesize[1], newFrame->data[2], newFrame->linesize[2]);
                if (ret != 0)
                {
                    fprintf(stderr, "SDL_UpdateYUVTexture Error: %s\n", SDL_GetError());
                }

                av_frame_free(&newFrame);
            }
        }
        SDL_RenderPresent(renderer);
        frameCount++;
        currentFrameTime = SDL_GetPerformanceCounter();
        float elapsedTime = (currentFrameTime - lastFrameTime) / (float)performanceFrequency;
        char fpsLabel[20] = {0};
        sprintf(fpsLabel, "FPS:%.2f", fps);
        fprintf(stdout, "%s\n", fpsLabel);
        if (elapsedTime >= 1.0f)
        {
            fps = frameCount / elapsedTime;
            frameCount = 0;
            lastFrameTime = currentFrameTime;
        }
        while (SDL_PollEvent(&event) != 0)
        {
            if (event.type == SDL_QUIT)
            {
                goto END;
            }
        }
    }
END:
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return NULL;
}
