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
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "rtsp_handler.h"
#include "frame_queue.h"
#include "video_renderer.h"
#include "detection_thread.h"
#include <signal.h>
#include <unistd.h>

void handle_signal(int sig)
{
    fprintf(stderr, "Received signal: %d\n", sig);
    exit(0);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <RTSP_URL>\n", argv[0]);
        return EXIT_FAILURE;
    }
    // 设置信号处理
    if (signal(SIGTERM, handle_signal) == SIG_ERR)
    {
        perror("Failed to set signal handler for SIGTERM");
        return 1;
    }
    const char *rtsp_url = argv[1];

    // Initialize frame queues
    FrameQueue video_queue, detection_queue;
    frame_queue_init(&video_queue, 60);
    frame_queue_init(&detection_queue, 60);

    // Create threads
    pthread_t rtsp_thread, renderer_thread, detection_thread;
    RTSPThreadArgs rtsp_args = {rtsp_url, &video_queue, &detection_queue};
    if (pthread_create(&rtsp_thread, NULL, rtsp_handler_thread, (void *)&rtsp_args) != 0)
    {
        perror("Failed to create RTSP thread");
        return EXIT_FAILURE;
    }

    if (pthread_create(&renderer_thread, NULL, video_renderer_thread, (void *)&video_queue) != 0)
    {
        perror("Failed to create video renderer thread");
        return EXIT_FAILURE;
    }

    if (pthread_create(&detection_thread, NULL, detection_thread_func, (void *)&detection_queue) != 0)
    {
        perror("Failed to create detection thread");
        return EXIT_FAILURE;
    }

    // Wait for threads to finish
    pthread_join(rtsp_thread, NULL);
    pthread_join(renderer_thread, NULL);
    pthread_join(detection_thread, NULL);

    // Cleanup
    frame_queue_destroy(&video_queue);
    frame_queue_destroy(&detection_queue);

    return EXIT_SUCCESS;
}
