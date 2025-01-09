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
extern "C"
{
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
}
#include "frame_queue.h"
#include "libav_utils.h"
#include "sdl_utils.h"
void *video_renderer_thread(void *arg)
{
    FrameQueue *video_queue = (FrameQueue *)arg;

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

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer)
    {
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return NULL;
    }

    SDL_Texture *texture = SDL_CreateTexture(renderer,
                                             SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
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
    char fpsLabel[20] = {0};
    Uint64 currentFrameTime = 0;
    Uint64 lastFrameTime = SDL_GetPerformanceCounter();
    Uint64 performanceFrequency = SDL_GetPerformanceFrequency();
    SDL_Rect rect = {0, 0, 20, 20};
    SDL_Event event;
    while (1)
    {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);

        QueueItem item;
        memset(&item, 0, sizeof(QueueItem));
        if (dequeue(video_queue, &item))
        {
            if (item.type == ONLY_FRAME && item.data != NULL)
            {
                AVFrame *newFrame = (AVFrame *)item.data;
                SDL_UpdateYUVTexture(texture, NULL,
                                     newFrame->data[0], newFrame->linesize[0],
                                     newFrame->data[1], newFrame->linesize[1],
                                     newFrame->data[2], newFrame->linesize[2]);
                av_frame_free(&newFrame);
            }
        }
        else
        {
            SDL_Delay(10);
        }
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        if (SDL_RenderDrawRect(renderer, &rect) < 0)
        {
            fprintf(stderr, "Could not draw rectangle! SDL_Error: %s\n", SDL_GetError());
        }

        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        frameCount++;
        currentFrameTime = SDL_GetPerformanceCounter();
        float elapsedTime = (currentFrameTime - lastFrameTime) / (float)performanceFrequency;
        sprintf(fpsLabel, "FPS:%.2f", fps);
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
            else if (event.type == SDL_MOUSEMOTION)
            {
                rect.x = event.motion.x;
                rect.y = event.motion.y;
            }
            else if (event.type == SDL_KEYDOWN)
            {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                {
                    goto END;
                }
            }
            else if (event.type == SDL_FIRSTEVENT)
            {
                fprintf(stderr, "SDL_PollEvent Error: %s\n", SDL_GetError());
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
    return NULL;
}
