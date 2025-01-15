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
#include "background.h"
#include "context.h"
#include "push_rtsp_thread.h"
/// @brief
Context *background_thread_ctx;
Context *pull_rtsp_thread_ctx;
Context *video_renderer_thread_ctx;
Context *detection_thread_ctx;
Context *push_rtsp_thread_ctx;
/// @brief
/// @param sig
void handle_signal(int sig)
{
    CancelContext(pull_rtsp_thread_ctx);
    CancelContext(video_renderer_thread_ctx);
    CancelContext(detection_thread_ctx);
    CancelContext(background_thread_ctx);
    CancelContext(push_rtsp_thread_ctx);
    fprintf(stderr, "Received signal %d, exiting...\n", sig);
    exit(0);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <RTSP_URL>\n", argv[0]);
        return 1;
    }
    if (signal(SIGINT, handle_signal) == SIG_ERR)
    {
        perror("Failed to set signal handler for SIGTERM");
        return 1;
    }
    const char *rtsp_url = argv[1];
    background_thread_ctx = CreateContext();
    pull_rtsp_thread_ctx = CreateContext();
    video_renderer_thread_ctx = CreateContext();
    detection_thread_ctx = CreateContext();
    push_rtsp_thread_ctx = CreateContext();

    // Initialize frame queues
    FrameQueue video_queue, detection_queue, box_queue, push_origin_rtsp_queue, push_infer_rtsp_queue;
    frame_queue_init(&video_queue, 60);
    frame_queue_init(&detection_queue, 60);
    frame_queue_init(&box_queue, 60);
    frame_queue_init(&push_origin_rtsp_queue, 60);
    frame_queue_init(&push_infer_rtsp_queue, 60);

    // Create threads
    pthread_t background_thread, pull_rtsp_thread, renderer_thread, detection_thread, push_rtsp_thread;
    //
    ThreadArgs background_thread_args = {.ctx = background_thread_ctx};

    ThreadArgs pull_rtsp_thread_args = {rtsp_url, &video_queue,
                                        &detection_queue, &box_queue, &push_origin_rtsp_queue,
                                        &push_infer_rtsp_queue, pull_rtsp_thread_ctx};
    ThreadArgs video_renderer_thread_args = {rtsp_url, &video_queue,
                                             &detection_queue, &box_queue, &push_origin_rtsp_queue,
                                             &push_infer_rtsp_queue, pull_rtsp_thread_ctx};
    ThreadArgs detection_thread_args = {rtsp_url, &video_queue,
                                        &detection_queue, &box_queue, &push_origin_rtsp_queue,
                                        &push_infer_rtsp_queue, pull_rtsp_thread_ctx};
    ThreadArgs push_rtsp_thread_args = {rtsp_url, &video_queue,
                                        &detection_queue, &box_queue, &push_origin_rtsp_queue,
                                        &push_infer_rtsp_queue, pull_rtsp_thread_ctx};
    //
    if (pthread_create(&background_thread, NULL, background_task_thread, (void *)&background_thread_args) != 0)
    {
        perror("Failed to create RTSP thread");
        goto END;
    }
    //
    if (pthread_create(&pull_rtsp_thread, NULL, pull_rtsp_handler_thread, (void *)&pull_rtsp_thread_args) != 0)
    {
        perror("Failed to create RTSP thread");
        goto END;
    }
    //
    if (pthread_create(&renderer_thread, NULL, video_renderer_thread, (void *)&video_renderer_thread_args) != 0)
    {
        perror("Failed to create video renderer thread");
        goto END;
    }
    //
    if (pthread_create(&detection_thread, NULL, frame_detection_thread, (void *)&detection_thread_args) != 0)
    {
        perror("Failed to create detection thread");
        goto END;
    }
    //
    if (pthread_create(&push_rtsp_thread, NULL, push_rtsp_handler_thread, (void *)&push_rtsp_thread_args) != 0)
    {
        perror("Failed to create detection thread");
        goto END;
    }
    fprintf(stderr, "Main thread waiting for threads to finish...\n");
    pthread_detach(pull_rtsp_thread);
    pthread_detach(renderer_thread);
    pthread_detach(detection_thread);
    pthread_detach(push_rtsp_thread);
    pthread_join(background_thread, NULL);
END:
    // Cancel contexts
    CancelContext(background_thread_ctx);
    CancelContext(pull_rtsp_thread_ctx);
    CancelContext(video_renderer_thread_ctx);
    CancelContext(detection_thread_ctx);
    CancelContext(push_rtsp_thread_ctx);
    // Destroy threads
    pthread_cond_destroy(&background_thread_ctx->cond);
    pthread_mutex_destroy(&pull_rtsp_thread_ctx->mtx);
    pthread_mutex_destroy(&video_renderer_thread_ctx->mtx);
    pthread_mutex_destroy(&detection_thread_ctx->mtx);
    pthread_mutex_destroy(&push_rtsp_thread_ctx->mtx);
    // Free frame queues
    frame_queue_destroy(&video_queue);
    frame_queue_destroy(&box_queue);
    frame_queue_destroy(&detection_queue);
    frame_queue_destroy(&push_origin_rtsp_queue);
    frame_queue_destroy(&push_infer_rtsp_queue);

    return EXIT_SUCCESS;
}
