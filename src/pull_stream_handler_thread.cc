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
#include "pull_stream_handler_thread.h"
#include "libav_utils.h"
#include "push_stream_thread.h"
#include "video_record_thread.h"
// 克隆帧并加入队列
void clone_and_enqueue(AVFrame *src_frame, FrameQueue *queue)
{
    AVFrame *output_frame = av_frame_clone(src_frame);
    QueueItem outputItem;
    outputItem.type = ONLY_FRAME;
    outputItem.data = output_frame;
    memset(outputItem.Boxes, 0, sizeof(outputItem.Boxes));
    if (!enqueue(queue, outputItem))
    {
        av_frame_free(&output_frame);
    }
}
// 自定义错误处理和资源释放函数

void handle_error(const char *message, int ret, AVFormatContext **fmt_ctx, AVPacket **origin_packet, AVCodecContext **codec_ctx)
{
    if (message == nullptr)
    {
        message = "Unknown error";
    }
    fprintf(stderr, "%s (%s).\n", message, get_av_error(ret));
    if (codec_ctx != nullptr && *codec_ctx != nullptr)
    {
        avcodec_free_context(codec_ctx);
        *codec_ctx = nullptr;
    }
    if (origin_packet != nullptr && *origin_packet != nullptr)
    {
        av_packet_free(origin_packet);
        *origin_packet = nullptr;
    }
    if (fmt_ctx != nullptr && *fmt_ctx != nullptr)
    {
        avformat_close_input(fmt_ctx);
        *fmt_ctx = nullptr;
    }
    pthread_exit(NULL);
}
void *pull_stream_handler_thread(void *arg)
{
    const ThreadArgs *args = (ThreadArgs *)arg;
    AVFormatContext *fmt_ctx = NULL;
    AVPacket *origin_packet = NULL;
    AVCodecContext *codec_ctx = NULL;
    int ret;
    // Open Stream input stream
    if ((ret = avformat_open_input(&fmt_ctx, args->input_stream_url, NULL, NULL)) < 0)
    {
        handle_error("Error: Could not open Stream stream", ret, &fmt_ctx, &origin_packet, &codec_ctx);
    }
    // Find stream info
    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0)
    {
        handle_error("Error: Could not find stream info", ret, &fmt_ctx, &origin_packet, &codec_ctx);
    }
    // Find the first video and audio streams
    int video_stream_index = -1;
    int audio_stream_index = -1;
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++)
    {
        if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_stream_index = i;
        }
        else if (fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audio_stream_index = i;
        }
    }
    if (audio_stream_index == -1)
    {
        handle_error("Error: No audio stream found", AVERROR_STREAM_NOT_FOUND, &fmt_ctx, &origin_packet, &codec_ctx);
    }
    if (video_stream_index == -1)
    {
        handle_error("Error: No video stream found", AVERROR_STREAM_NOT_FOUND, &fmt_ctx, &origin_packet, &codec_ctx);
    }
    // Print stream information
    fprintf(stdout, "=========input_stream_url======== \n");
    av_dump_format(fmt_ctx, 0, args->input_stream_url, 0);
    fprintf(stdout, "================================= \n");
    // Allocate AVPacket for reading frames
    origin_packet = av_packet_alloc();
    if (!origin_packet)
    {
        handle_error("Error: Could not allocate origin_packet", AVERROR(ENOMEM), &fmt_ctx, &origin_packet, &codec_ctx);
    }
    // Get the codec parameters of the video stream
    AVCodecParameters *codecpar = fmt_ctx->streams[video_stream_index]->codecpar;
    // Find the decoder for the video stream
    const AVCodec *decoder = avcodec_find_decoder(codecpar->codec_id);
    if (!decoder)
    {
        handle_error("Error: Failed to find decoder for codec ID", codecpar->codec_id, &fmt_ctx, &origin_packet, &codec_ctx);
    }
    // Allocate a codec context for the decoder
    codec_ctx = avcodec_alloc_context3(decoder);
    if (!codec_ctx)
    {
        handle_error("Error: Failed to allocate codec context", AVERROR(ENOMEM), &fmt_ctx, &origin_packet, &codec_ctx);
    }
    // Copy codec parameters to the codec context
    if ((ret = avcodec_parameters_to_context(codec_ctx, codecpar)) < 0)
    {
        handle_error("Error: Failed to copy codec parameters to codec context", ret, &fmt_ctx, &origin_packet, &codec_ctx);
    }
    // Open the codec
    if ((ret = avcodec_open2(codec_ctx, decoder, NULL)) < 0)
    {
        handle_error("Error: Failed to open codec", ret, &fmt_ctx, &origin_packet, &codec_ctx);
    }
    // 输出详细信息
    for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++)
    {
        AVStream *stream = fmt_ctx->streams[i];
        char pix_fmt_str[16];
        av_get_pix_fmt_string(pix_fmt_str, sizeof(pix_fmt_str), (AVPixelFormat)stream->codecpar->format);
        fprintf(stdout, "======= Stream Info =====\n");
        printf("Stream %d:\n", i);
        printf("  pixel_format: %s\n", pix_fmt_str);
        printf("  codec_type: %s\n", av_get_media_type_string(stream->codecpar->codec_type));
        printf("  codec_name: %s\n", avcodec_get_name(stream->codecpar->codec_id));
        fprintf(stdout, "=========================\n");
    }
    fprintf(stdout, "Stream handler thread started. Pull stream: %s\n", args->input_stream_url);
    Context *record_mp4_thread_ctx = CreateContext();
    Context *push_stream_thread_ctx = CreateContext();
    if (!record_mp4_thread_ctx || !push_stream_thread_ctx)
    {
        handle_error("Error: Failed to create context", AVERROR(ENOMEM), &fmt_ctx, &origin_packet, &codec_ctx);
    }
    // 启动保存 MP4 的线程
    {
        ThreadArgs record_mp4_thread_args = *args;
        record_mp4_thread_args.ctx = record_mp4_thread_ctx;
        record_mp4_thread_args.input_stream = fmt_ctx->streams[video_stream_index];
        pthread_t record_mp4_thread;
        AVCodecParameters *params = avcodec_parameters_alloc();
        if (!params)
        {
            handle_error("Error: Failed to allocate codec parameters", AVERROR(ENOMEM), &fmt_ctx, &origin_packet, &codec_ctx);
        }
        ret = avcodec_parameters_from_context(params, codec_ctx);
        if (ret < 0)
        {
            avcodec_parameters_free(&params);
            handle_error("Error: Failed to copy codec parameters", ret, &fmt_ctx, &origin_packet, &codec_ctx);
        }
        if (pthread_create(&record_mp4_thread, NULL, save_mp4_handler_thread, (void *)&record_mp4_thread_args) != 0)
        {
            avcodec_parameters_free(&params);
            handle_error("Failed to create save_mp4_handler thread", AVERROR(EAGAIN), &fmt_ctx, &origin_packet, &codec_ctx);
        }
        pthread_detach(record_mp4_thread);
    }
    // 启动推流的线程
    {
        ThreadArgs push_stream_thread_args = *args;
        push_stream_thread_args.ctx = push_stream_thread_ctx;
        push_stream_thread_args.input_stream = fmt_ctx->streams[video_stream_index];
        pthread_t push_stream_thread;
        AVCodecParameters *params = avcodec_parameters_alloc();
        if (!params)
        {
            handle_error("Error: Failed to allocate codec parameters", AVERROR(ENOMEM), &fmt_ctx, &origin_packet, &codec_ctx);
        }
        ret = avcodec_parameters_from_context(params, codec_ctx);
        if (ret < 0)
        {
            avcodec_parameters_free(&params);
            handle_error("Error: Failed to copy codec parameters", ret, &fmt_ctx, &origin_packet, &codec_ctx);
        }
        if (pthread_create(&push_stream_thread, NULL, push_rtmp_handler_thread, (void *)&push_stream_thread_args) != 0)
        {
            avcodec_parameters_free(&params);
            handle_error("Failed to create push_rtmp_handler thread", AVERROR(EAGAIN), &fmt_ctx, &origin_packet, &codec_ctx);
        }
        pthread_detach(push_stream_thread);
    }
    // Read frames from the stream
    while (av_read_frame(fmt_ctx, origin_packet) >= 0)
    {
        if (args->ctx->is_cancelled)
        {
            break;
        }
        if (origin_packet->stream_index == video_stream_index)
        {
            // Send the packet to the decoder
            ret = avcodec_send_packet(codec_ctx, origin_packet);
            if (ret < 0)
            {
                fprintf(stdout, "Error: Failed to send packet to decoder (%s).\n", get_av_error(ret));
                av_packet_unref(origin_packet);
                continue;
            }
            AVFrame *origin_frame = av_frame_alloc();
            if (!origin_frame)
            {
                fprintf(stdout, "Error: Failed to allocate frame(%s).\n", get_av_error(AVERROR(ENOMEM)));
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
                fprintf(stdout, "Error: Failed to receive frame from decoder (%s).\n", get_av_error(ret));
                av_frame_free(&origin_frame);
                av_packet_unref(origin_packet);
                continue;
            }
            else
            {

                clone_and_enqueue(origin_frame, args->video_queue);
                clone_and_enqueue(origin_frame, args->origin_frame_queue);
                clone_and_enqueue(origin_frame, args->record_frame_queue);
                clone_and_enqueue(origin_frame, args->detection_queue);
            }
            av_frame_free(&origin_frame);
        }
        av_packet_unref(origin_packet);
    }
    fprintf(stdout, "Stream handler thread stopped\n");
    // 取消上下文并销毁互斥锁
    if (push_stream_thread_ctx)
    {
        CancelContext(push_stream_thread_ctx);
        pthread_mutex_destroy(&push_stream_thread_ctx->mtx);
    }
    if (record_mp4_thread_ctx)
    {
        CancelContext(record_mp4_thread_ctx);
        pthread_mutex_destroy(&record_mp4_thread_ctx->mtx);
    }
    // 释放资源
    avcodec_free_context(&codec_ctx);
    av_packet_free(&origin_packet);
    avformat_close_input(&fmt_ctx);
    avformat_network_deinit();
    pthread_exit(NULL);
    return (void *)NULL;
}