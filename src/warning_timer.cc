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

#include "warning_timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include "http_api.h"
// 全局变量
static uint32_t interval_ms;
static uint32_t threshold;
static void (*event_callback)(WarningInfo *);
static uint32_t warning_count = 0;
static pthread_t timer_thread;
static volatile int running = 0;
static int latest_warning_type;
static int latest_warning_timestamp;
//
void print_warning_info(WarningInfo *info)
{
    printf("WarningInfo: warning_count=%u, interval_ms=%u, latest_warning_type=%d, latest_warning_timestamp=%d\n",
           info->warning_count, info->interval_ms, info->latest_warning_type, info->latest_warning_timestamp);
}
// 计时器线程函数
static void *timer_thread_func(void *arg)
{
    struct timespec start, current;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (running)
    {
        clock_gettime(CLOCK_MONOTONIC, &current);
        uint64_t elapsed_ms = (current.tv_sec - start.tv_sec) * 1000 + (current.tv_nsec - start.tv_nsec) / 1000000;

        if (elapsed_ms >= interval_ms)
        {
            if (warning_count >= threshold)
            {
                if (event_callback != NULL)
                {
                    WarningInfo info;
                    info.warning_count = warning_count;
                    info.interval_ms = interval_ms;
                    info.latest_warning_type = latest_warning_type;
                    info.latest_warning_timestamp = latest_warning_timestamp;
                    event_callback(&info);
                }
            }
            warning_count = 0;
            clock_gettime(CLOCK_MONOTONIC, &start);
        }

        usleep(1000000 * 5); // 每 10 毫秒检查一次
    }

    return NULL;
}

// 初始化告警计时器
int warning_timer_init(uint32_t interval_ms_param, uint32_t threshold_param, void (*callback)(WarningInfo *))
{
    interval_ms = interval_ms_param;
    threshold = threshold_param;
    event_callback = callback;
    warning_count = 0;
    running = 1;

    if (pthread_create(&timer_thread, NULL, timer_thread_func, NULL) != 0)
    {
        perror("Failed to create timer thread");
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "Warning timer initialized!\n");
    return 0;
}

// 记录一次告警
void warning_timer_record_warning(int type, int timestamp)
{
    warning_count++;
    latest_warning_type = type;
    latest_warning_timestamp = timestamp;
}

// 停止告警计时器
void warning_timer_stop()
{
    running = 0;
    pthread_join(timer_thread, NULL);
    fprintf(stdout, "Warning timer stopped!\n");
}
// 触发事件的回调函数
void event_triggered(WarningInfo *info)
{
    post_recognized_type("http://127.0.0.1:3345", info->latest_warning_type, (const char *)"1234567890abcdef");
    // print_warning_info(info);
}
