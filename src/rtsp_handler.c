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
}
#include "frame_queue.h"
#include "rtsp_handler.h"
#include "libav_utils.h"

void *rtsp_handler_thread(void *arg)
{
    const RTSPThreadArgs *args = (RTSPThreadArgs *)arg;
    AVFormatContext *fmt_ctx = NULL;
    int ret;

    // Open RTSP input stream
    if ((ret = avformat_open_input(&fmt_ctx, args->rtsp_url, NULL, NULL)) < 0)
    {
        fprintf(stderr, "Error: Could not open RTSP stream :(%s).\n", args->rtsp_url);
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
    av_dump_format(fmt_ctx, 0, args->rtsp_url, 0);

    // Allocate AVPacket for reading frames
    AVPacket *packet = av_packet_alloc();
    if (!packet)
    {
        fprintf(stderr, "Error: Could not allocate packet.\n");
        avformat_close_input(&fmt_ctx);
        pthread_exit(NULL);
    }

    // Get the codec parameters of the video stream
    AVCodecParameters *codecpar = fmt_ctx->streams[video_stream_index]->codecpar;
    // Find the decoder for the video stream
    AVCodec *decoder = avcodec_find_decoder(codecpar->codec_id);
    if (!decoder)
    {
        fprintf(stderr, "Error: Failed to find decoder for codec ID %d.\n", codecpar->codec_id);
        avformat_close_input(&fmt_ctx);
        av_packet_free(&packet);
        pthread_exit(NULL);
    }

    // Allocate a codec context for the decoder
    AVCodecContext *codec_ctx = avcodec_alloc_context3(decoder);
    if (!codec_ctx)
    {
        fprintf(stderr, "Error: Failed to allocate codec context.\n");
        avformat_close_input(&fmt_ctx);
        av_packet_free(&packet);
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
        av_packet_free(&packet);
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
        av_packet_free(&packet);
        pthread_exit(NULL);
    }

    printf("RTSP handler thread started. Listening to stream: %s\n", args->rtsp_url);

    // Read frames from the stream
    while (av_read_frame(fmt_ctx, packet) >= 0)
    {
        if (packet->stream_index == video_stream_index)
        {
            // Send the packet to the decoder
            ret = avcodec_send_packet(codec_ctx, packet);
            if (ret < 0)
            {
                char err_str[AV_ERROR_MAX_STRING_SIZE];
                av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, ret);
                fprintf(stderr, "Error: Failed to send packet to decoder (%s).\n", err_str);
                // Free the packet for the next read
                av_packet_unref(packet);
                continue;
            }

            AVFrame *frame = av_frame_alloc();
            if (!frame)
            {
                fprintf(stderr, "Error: Failed to allocate frame.\n");
                // Free the packet for the next read
                av_packet_unref(packet);
                continue;
            }

            // Receive the decoded frame from the decoder
            ret = avcodec_receive_frame(codec_ctx, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                printf("No frame received from decoder.\n");
                av_frame_free(&frame);
                // Free the packet for the next read
                av_packet_unref(packet);
                continue;
            }
            else if (ret < 0)
            {
                char err_str[AV_ERROR_MAX_STRING_SIZE];
                av_make_error_string(err_str, AV_ERROR_MAX_STRING_SIZE, ret);
                fprintf(stderr, "Error: Failed to receive frame from decoder (%s).\n", err_str);
                av_frame_free(&frame);
                // Free the packet for the next read
                av_packet_unref(packet);
                continue;
            }
            else
            {
                printf("Frame received from decoder.\n");
                printf("Frame size1: %d\n", frame->linesize[0]);
                printf("Frame size2: %d\n", frame->linesize[1]);
                printf("Frame size3: %d\n", frame->linesize[2]);
            }

            // Copy一份展示
            AVFrame *frameOutput = CopyAVFrame(frame);
            QueueItem outputItem;
            outputItem.type = ONLY_FRAME;
            outputItem.data = frameOutput;
            memset(outputItem.Boxes, 0, sizeof(outputItem.Boxes));
            if (enqueue(args->video_queue, outputItem) == 0)
            {
                av_frame_free(&frameOutput);
            }
            av_frame_free(&frame);
        }
        av_packet_unref(packet);
    }

    // Cleanup
    printf("RTSP handler thread finished.\n");
    avcodec_free_context(&codec_ctx);
    av_packet_free(&packet);
    avformat_close_input(&fmt_ctx);
    avformat_network_deinit();

    pthread_exit(NULL);
}