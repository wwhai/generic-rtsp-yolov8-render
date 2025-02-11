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

// #define SDL_MAIN_HANDLED
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "pull_camera_handler_thread.h"
#include "frame_queue.h"
#include "video_renderer.h"
#include "detection_thread.h"
#include <signal.h>
#include <unistd.h>
#include "background.h"
#include "context.h"
#include "push_stream_thread.h"
#include <curl/curl.h>
// 全局上下文指针数组
Context *contexts[4];

// 信号处理函数
void handle_signal(int sig)
{
    for (int i = 0; i < 4; i++)
    {
        if (contexts[i])
        {
            CancelContext(contexts[i]);
        }
    }
    fprintf(stderr, "Received signal %d, exiting...\n", sig);
    exit(0);
}

// 创建线程的辅助函数
int create_thread(pthread_t *thread, void *(*start_routine)(void *), void *arg)
{
    int ret = pthread_create(thread, NULL, start_routine, arg);
    if (ret != 0)
    {
        perror("Failed to create thread");
        return -1;
    }
    return 0;
}

// 销毁上下文的辅助函数
void destroy_contexts()
{
    for (int i = 0; i < 4; i++)
    {
        if (contexts[i])
        {
            CancelContext(contexts[i]);
            if (i > 0)
            {
                pthread_mutex_destroy(&contexts[i]->mtx);
            }
            else
            {
                pthread_cond_destroy(&contexts[i]->cond);
            }
        }
    }
}

// 销毁帧队列的辅助函数
void destroy_frame_queues(FrameQueue *queues, int num_queues)
{
    for (int i = 0; i < num_queues; i++)
    {
        frame_queue_destroy(&queues[i]);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Usage: %s <camera_URL> <PUSH_URL>\n", argv[0]);
        return 1;
    }

    // 设置信号处理函数
    if (signal(SIGINT, handle_signal) == SIG_ERR)
    {
        perror("Failed to set signal handler for SIGTERM");
        return 1;
    }
    curl_global_init(CURL_GLOBAL_ALL);
    const char *pull_from_camera_url = argv[1];
    const char *push_to_camera_url = argv[2];

    // 创建上下文
    contexts[0] = CreateContext();
    contexts[1] = CreateContext();
    contexts[2] = CreateContext();
    contexts[3] = CreateContext();

    // 检查上下文是否创建成功
    for (int i = 0; i < 4; i++)
    {
        if (!contexts[i])
        {
            fprintf(stderr, "Failed to create context %d\n", i);
            destroy_contexts();
            return 1;
        }
    }

    // 初始化帧队列
    FrameQueue queues[6];
    for (int i = 0; i < 6; i++)
    {
        frame_queue_init(&queues[i], 60);
    }

    // 创建线程参数
    ThreadArgs background_thread_args = {.ctx = contexts[0]};
    ThreadArgs common_args = {pull_from_camera_url, push_to_camera_url, &queues[0], &queues[1], &queues[2],
                              &queues[3], &queues[4], &queues[5], NULL, NULL, contexts[1]};

    // 创建线程
    pthread_t threads[4];
    if (create_thread(&threads[0], background_task_thread, &background_thread_args) != 0 ||
        create_thread(&threads[1], pull_camera_handler_thread, &common_args) != 0 ||
        create_thread(&threads[2], video_renderer_thread, &common_args) != 0 ||
        create_thread(&threads[3], frame_detection_thread, &common_args) != 0)
    {
        destroy_contexts();
        destroy_frame_queues(queues, 6);
        return 1;
    }

    fprintf(stderr, "Main thread waiting for threads to finish...\n");

    // 分离线程
    for (int i = 1; i < 4; i++)
    {
        pthread_detach(threads[i]);
    }

    // 等待后台线程结束
    pthread_join(threads[0], NULL);

    // 清理资源
    destroy_contexts();
    destroy_frame_queues(queues, 6);
    curl_global_cleanup();
    return EXIT_SUCCESS;
}