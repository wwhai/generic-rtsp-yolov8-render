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
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/time.h>
#include <libavutil/pixdesc.h>
}
#include "frame_queue.h"
#include "rtsp_handler.h"
#include "libav_utils.h"
#include "push_stream_thread.h"
void *pull_rtsp_handler_thread(void *arg)
{
    const ThreadArgs *args = (ThreadArgs *)arg;
    AVFormatContext *fmt_ctx = NULL;
    int ret;

    // Open RTSP input stream
    fprintf(stderr, "pull_rtsp_handler_thread: %s\n", args->input_stream_url);
    if ((ret = avformat_open_input(&fmt_ctx, args->input_stream_url, NULL, NULL)) < 0)
    {
        fprintf(stderr, "Error: Could not open RTSP stream :(%s).\n", args->input_stream_url);
        pthread_exit(NULL);
    }

    // Find stream info
    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0)
    {
        fprintf(stderr, "Error: Could not find stream info.\n");
        avformat_close_input(&fmt_ctx);
        pthread_exit(NULL);
    }

    // Find the first video stream
    int video_stream_index = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++)
    {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_stream_index = i;
            break;
        }
    }
    if (video_stream_index == -1)
    {
        fprintf(stderr, "Error: No video stream found.\n");
        avformat_close_input(&fmt_ctx);
        pthread_exit(NULL);
    }

    // Print stream information
    av_dump_format(fmt_ctx, 0, args->input_stream_url, 0);

    // Allocate AVPacket for reading frames
    AVPacket *origin_packet = av_packet_alloc();
    if (!origin_packet)
    {
        fprintf(stderr, "Error: Could not allocate origin_packet.\n");
        avformat_close_input(&fmt_ctx);
        pthread_exit(NULL);
    }

    // Get the codec parameters of the video stream
    AVCodecParameters *codecpar = fmt_ctx->streams[video_stream_index]->codecpar;
    // Find the decoder for the video stream
    const AVCodec *decoder = avcodec_find_decoder(codecpar->codec_id);
    if (!decoder)
    {
        fprintf(stderr, "Error: Failed to find decoder for codec ID %d.\n", codecpar->codec_id);
        avformat_close_input(&fmt_ctx);
        av_packet_free(&origin_packet);
        pthread_exit(NULL);
    }

    // Allocate a codec context for the decoder
    AVCodecContext *codec_ctx = avcodec_alloc_context3(decoder);
    if (!codec_ctx)
    {
        fprintf(stderr, "Error: Failed to allocate codec context.\n");
        avformat_close_input(&fmt_ctx);
        av_packet_free(&origin_packet);
        pthread_exit(NULL);
    }

    // Copy codec parameters to the codec context
    if ((ret = avcodec_parameters_to_context(codec_ctx, codecpar)) < 0)
    {
        char err_str[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, ret);
        fprintf(stderr, "Error: Failed to copy codec parameters to codec context (%s).\n", err_str);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        av_packet_free(&origin_packet);
        pthread_exit(NULL);
    }

    // Open the codec
    if ((ret = avcodec_open2(codec_ctx, decoder, NULL)) < 0)
    {
        char err_str[AV_ERROR_MAX_STRING_SIZE];
        av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, ret);
        fprintf(stderr, "Error: Failed to open codec (%s).\n", err_str);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        av_packet_free(&origin_packet);
        pthread_exit(NULL);
    }
    // 输出详细信息
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++)
    {
        AVStream *stream = fmt_ctx->streams[i];
        char pix_fmt_str[16];
        av_get_pix_fmt_string(pix_fmt_str, sizeof(pix_fmt_str), (AVPixelFormat)stream->codecpar->format);
        printf("Stream %d:\n", i);
        printf("  pixel_format: %s\n", pix_fmt_str);
        printf("  codec_type: %s\n", av_get_media_type_string(stream->codecpar->codec_type));
        printf("  codec_name: %s\n", avcodec_get_name(stream->codecpar->codec_id));
    }
    // Create a thread for pushing frames to the output stream
    pthread_t push_rtsp_thread;
    Context *push_rtmp_handler_thread_ctx = CreateContext();
    ThreadArgs push_rtmp_handler_thread_args;
    // Copy the necessary data to the push_rtmp_handler_thread_args struct
    push_rtmp_handler_thread_args.ctx = push_rtmp_handler_thread_ctx;
    push_rtmp_handler_thread_args.origin_frame_queue = args->origin_frame_queue;
    push_rtmp_handler_thread_args.output_stream_url = args->output_stream_url;
    push_rtmp_handler_thread_args.input_stream = fmt_ctx->streams[video_stream_index];
    fprintf(stderr, "push_rtmp_handler_thread: %s\n", args->output_stream_url);
    if (pthread_create(&push_rtsp_thread, NULL, push_rtmp_handler_thread, (void *)&push_rtmp_handler_thread_args) != 0)
    {
        perror("Failed to create push_rtmp_handler_thread thread");
        exit(1);
    }
    pthread_detach(push_rtsp_thread);
    // Read frames from the stream
    while (av_read_frame(fmt_ctx, origin_packet) >= 0)
    {
        if (args->ctx->is_cancelled)
        {
            goto END;
        }
        if (origin_packet->stream_index == video_stream_index)
        {
            // Send the packet to the decoder
            ret = avcodec_send_packet(codec_ctx, origin_packet);
            if (ret < 0)
            {
                char err_str[AV_ERROR_MAX_STRING_SIZE];
                av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, ret);
                fprintf(stderr, "Error: Failed to send packet to decoder (%s).\n", err_str);
                // Free the packet for the next read
                av_packet_unref(origin_packet);
                continue;
            }

            AVFrame *origin_frame = av_frame_alloc();
            if (!origin_frame)
            {
                fprintf(stderr, "Error: Failed to allocate frame.\n");
                // Free the packet for the next read
                av_packet_unref(origin_packet);
                continue;
            }

            // Receive the decoded frame from the decoder
            ret = avcodec_receive_frame(codec_ctx, origin_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                printf("No frame received from decoder.\n");
                av_frame_free(&origin_frame);
                av_packet_unref(origin_packet);
                continue;
            }
            else if (ret < 0)
            {
                char err_str[AV_ERROR_MAX_STRING_SIZE];
                av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, ret);
                fprintf(stderr, "Error: Failed to receive frame from decoder (%s).\n", err_str);
                av_frame_free(&origin_frame);
                av_packet_unref(origin_packet);
                continue;
            }
            else
            {
                {
                    AVFrame *display_frame = av_frame_clone(origin_frame);
                    QueueItem outputItem;
                    outputItem.type = ONLY_FRAME;
                    outputItem.data = display_frame;
                    memset(outputItem.Boxes, 0, sizeof(outputItem.Boxes));
                    if (!enqueue(args->video_queue, outputItem))
                    {
                        av_frame_free(&display_frame);
                    }
                }
                {
                    AVFrame *output_frame = av_frame_clone(origin_frame);
                    QueueItem outputItem;
                    outputItem.type = ONLY_FRAME;
                    outputItem.data = output_frame;
                    memset(outputItem.Boxes, 0, sizeof(outputItem.Boxes));
                    if (!enqueue(args->origin_frame_queue, outputItem))
                    {
                        av_frame_free(&output_frame);
                    }
                }
                {

                    AVFrame *detection_frame = av_frame_clone(origin_frame);
                    QueueItem outputItem;
                    outputItem.type = ONLY_FRAME;
                    outputItem.data = detection_frame;
                    memset(outputItem.Boxes, 0, sizeof(outputItem.Boxes));
                    if (!enqueue(args->detection_queue, outputItem))
                    {
                        av_frame_free(&detection_frame);
                    }
                }
            }
            av_frame_free(&origin_frame);
        }
        av_packet_unref(origin_packet);
    }

END:
    avcodec_free_context(&codec_ctx);
    av_packet_free(&origin_packet);
    avformat_close_input(&fmt_ctx);
    avformat_network_deinit();
    pthread_exit(NULL);
    return (void *)NULL;
}