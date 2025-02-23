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
#include "pull_stream_handler_thread.h"
#include "frame_queue.h"
#include "video_renderer.h"
#include "detection_thread.h"
#include <signal.h>
#include <unistd.h>
#include "background.h"
#include "context.h"
#include "push_stream_thread.h"
#include "warning_timer.h"
#include <curl/curl.h>
#include "logger.h"
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
    log_info("Received signal %d, exiting...", sig);
    exit(0);
}

// 创建线程的辅助函数
int create_thread(pthread_t *thread, void *(*start_routine)(void *), void *arg)
{
    int ret = pthread_create(thread, NULL, start_routine, arg);
    if (ret != 0)
    {
        log_error( "Failed to create thread");
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
    set_log_level(LOG_DEBUG);
    // 检查命令行参数数量
    if (argc < 3)
    {
        log_info("Usage: %s <camera_URL> <PUSH_URL>", argv[0]);
        return EXIT_FAILURE;
    }

    // 设置信号处理函数
    if (signal(SIGINT, handle_signal) == SIG_ERR)
    {
        log_error( "Failed to set signal handler for SIGINT");
        return EXIT_FAILURE;
    }

    // 初始化告警计时器
    if (warning_timer_init(10000, 10, event_triggered) != 0)
    {
        log_info("Failed to initialize warning timer");
        return EXIT_FAILURE;
    }

    // 初始化cURL库
    if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK)
    {
        log_info("Failed to initialize cURL library");
        warning_timer_stop();
        return EXIT_FAILURE;
    }

    const char *pull_from_camera_url = argv[1];
    const char *push_to_camera_url = argv[2];

    // 创建上下文
    for (int i = 0; i < 4; i++)
    {
        contexts[i] = CreateContext();
        if (!contexts[i])
        {
            log_info("Failed to create context %d", i);
            destroy_contexts();
            curl_global_cleanup();
            warning_timer_stop();
            return EXIT_FAILURE;
        }
    }

    // 初始化帧队列
    const int num_queues = 6;
    FrameQueue queues[num_queues];
    for (int i = 0; i < num_queues; i++)
    {
        frame_queue_init(&queues[i], 60);
    }

    // 创建线程参数
    ThreadArgs background_thread_args = {.ctx = contexts[0]};
    ThreadArgs common_args = {pull_from_camera_url, push_to_camera_url, &queues[0], &queues[1], &queues[2],
                              &queues[3], &queues[4], &queues[5], NULL, contexts[1]};

    // 创建线程
    pthread_t threads[4];
    if (create_thread(&threads[0], background_task_thread, &background_thread_args) != 0 ||
        create_thread(&threads[1], pull_stream_handler_thread, &common_args) != 0 ||
        create_thread(&threads[2], video_renderer_thread, &common_args) != 0 ||
        create_thread(&threads[3], frame_detection_thread, &common_args) != 0)
    {
        destroy_contexts();
        destroy_frame_queues(queues, num_queues);
        curl_global_cleanup();
        warning_timer_stop();
        return EXIT_FAILURE;
    }

    log_info("Main thread waiting for threads to finish...");

    // 等待所有线程结束
    for (int i = 0; i < 4; i++)
    {
        if (pthread_join(threads[i], NULL) != 0)
        {
            log_error( "Failed to join thread");
        }
    }

    // 清理资源
    destroy_contexts();
    destroy_frame_queues(queues, num_queues);
    curl_global_cleanup();
    // 清理计时器
    warning_timer_stop();

    return EXIT_SUCCESS;
}