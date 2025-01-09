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
#include "rtsp_handler.h"
#include "frame_queue.h"
#include "video_renderer.h"
#include "detection_thread.h"
#include <signal.h>
#include <unistd.h>
#include "context.h"

void handle_signal(int sig)
{
    fprintf(stderr, "Received signal: %d\n", sig);
    exit(0);
}

int main(int argc, char *argv[])
{
    if (signal(SIGTERM, handle_signal) == SIG_ERR)
    {
        perror("Failed to set signal handler for SIGTERM");
        return 1;
    }
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <RTSP_URL>\n", argv[0]);
        return EXIT_FAILURE;
    }
    const char *rtsp_url = argv[1];

    // Initialize frame queues
    FrameQueue video_queue, detection_queue, box_queue;
    frame_queue_init(&video_queue, 60);
    frame_queue_init(&detection_queue, 60);
    frame_queue_init(&box_queue, 60);

    // Create threads
    pthread_t rtsp_thread, renderer_thread, detection_thread;
    Context *rtsp_thread_ctx = CreateContext();
    ThreadArgs rtsp_thread_args = {rtsp_url, &video_queue, &detection_queue, &box_queue, rtsp_thread_ctx};
    if (pthread_create(&rtsp_thread, NULL, rtsp_handler_thread, (void *)&rtsp_thread_args) != 0)
    {
        perror("Failed to create RTSP thread");
        return EXIT_FAILURE;
    }
    Context *video_renderer_thread_ctx = CreateContext();
    ThreadArgs video_renderer_thread_args = {rtsp_url, &video_queue, &detection_queue, &box_queue, video_renderer_thread_ctx};
    if (pthread_create(&renderer_thread, NULL, video_renderer_thread, (void *)&video_renderer_thread_args) != 0)
    {
        perror("Failed to create video renderer thread");
        return EXIT_FAILURE;
    }
    Context *detection_thread_ctx = CreateContext();
    ThreadArgs detection_thread_args = {rtsp_url, &video_queue, &detection_queue, &box_queue, detection_thread_ctx};
    if (pthread_create(&detection_thread, NULL, frame_detection_thread, (void *)&detection_thread_args) != 0)
    {
        perror("Failed to create detection thread");
        return EXIT_FAILURE;
    }

    // Wait for threads to finish
    pthread_join(rtsp_thread, NULL);
    pthread_join(renderer_thread, NULL);
    pthread_join(detection_thread, NULL);

    // Cleanup
    CancelContext(rtsp_thread_ctx);
    CancelContext(video_renderer_thread_ctx);
    CancelContext(detection_thread_ctx);
    //
    pthread_cond_destroy(&rtsp_thread_ctx->cond);
    pthread_mutex_destroy(&video_renderer_thread_ctx->mtx);
    pthread_mutex_destroy(&detection_thread_ctx->mtx);
    //
    frame_queue_destroy(&video_queue);
    frame_queue_destroy(&box_queue);
    frame_queue_destroy(&detection_queue);

    return EXIT_SUCCESS;
}
