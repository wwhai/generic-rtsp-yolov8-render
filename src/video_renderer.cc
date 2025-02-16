#include "video_renderer.h"
#include "logger.h"
#define TARGET_FPS 25                  // 目标帧率
#define FRAME_TIME (1000 / TARGET_FPS) // 每帧目标时间 (毫秒)

void *video_renderer_thread(void *arg)
{
    // 检查输入参数是否为空
    if (arg == NULL)
    {
        log_info( "Error: Input argument is NULL.");
        return NULL;
    }
    ThreadArgs *args = (ThreadArgs *)arg;

    // 初始化 SDL 和 TTF
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    {
        log_info( "SDL_Init Error: %s", SDL_GetError());
        return NULL;
    }
    if (TTF_Init() == -1)
    {
        log_info( "SDL_ttf could not initialize! TTF_Error: %s", TTF_GetError());
        SDL_Quit();
        return NULL;
    }

    // 创建窗口
    SDL_Window *window = SDL_CreateWindow("VIDEO-PLAYER",
                                          SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          1920, 1080, SDL_WINDOW_SHOWN);
    if (!window)
    {
        log_info( "SDL_CreateWindow Error: %s", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return NULL;
    }

    // 创建渲染器
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        log_info( "SDL_CreateRenderer Error: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return NULL;
    }

    // 创建纹理
    SDL_Texture *texture = SDL_CreateTexture(renderer,
                                             SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING,
                                             1920, 1080);
    if (!texture)
    {
        log_info( "SDL_CreateTexture Error: %s", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return NULL;
    }

    // 加载字体
    TTF_Font *font = TTF_OpenFont("mono.ttf", 18);
    if (font == NULL)
    {
        log_info( "Failed to load font! TTF_Error: %s", TTF_GetError());
        SDL_DestroyTexture(texture);
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

    // 主循环
    while (1)
    {
        Uint32 frameStart = SDL_GetTicks();

        // 检查上下文是否被取消
        if (args->ctx->is_cancelled)
        {
            break;
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
                    log_info( "SDL_UpdateYUVTexture failed: %s", SDL_GetError());
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
                    // 检查边界框数据是否有效
                    if (boxes_item.Boxes != NULL)
                    {
                        SDLDrawBox(renderer, font, boxes_item.Boxes[i].label,
                                   boxes_item.Boxes[i].x, boxes_item.Boxes[i].y,
                                   boxes_item.Boxes[i].w, boxes_item.Boxes[i].h, 1);
                    }
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
                break;
            }
            else if (event.type == SDL_KEYDOWN)
            {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                {
                    break;
                }
            }
        }
        if (event.type == SDL_QUIT || event.key.keysym.sym == SDLK_ESCAPE)
        {
            break;
        }
    }

    // 资源释放
    if (texture)
    {
        SDL_DestroyTexture(texture);
    }
    if (renderer)
    {
        SDL_DestroyRenderer(renderer);
    }
    if (window)
    {
        SDL_DestroyWindow(window);
    }
    if (font)
    {
        TTF_CloseFont(font);
    }
    TTF_Quit();
    SDL_Quit();

    log_info( "video_renderer_thread exit");
    return NULL;
}