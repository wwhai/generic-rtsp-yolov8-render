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
#include "video_record_thread.h"
#include <unistd.h>
#include "libav_utils.h"
// 初始化 MP4 流上下文
int init_mp4_stream(Mp4StreamContext *ctx, const char *output_url, int width, int height, int fps)
{
    int ret;

    // 分配输出格式上下文
    avformat_alloc_output_context2(&ctx->output_ctx, NULL, "mp4", output_url);
    if (!ctx->output_ctx)
    {
        fprintf(stderr, "Could not create output context\n");
        return -1;
    }

    // 查找视频编码器
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        fprintf(stderr, "Codec not found\n");
        return -1;
    }

    // 分配编码器上下文
    ctx->codec_ctx = avcodec_alloc_context3(codec);
    if (!ctx->codec_ctx)
    {
        fprintf(stderr, "Could not allocate codec context\n");
        return -1;
    }

    // 设置编码器参数
    ctx->codec_ctx->codec_id = AV_CODEC_ID_H264;
    ctx->codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    ctx->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    ctx->codec_ctx->width = width;
    ctx->codec_ctx->height = height;
    ctx->codec_ctx->time_base = (AVRational){1, fps};
    ctx->codec_ctx->framerate = (AVRational){fps, 1};

    // 打开编码器
    ret = avcodec_open2(ctx->codec_ctx, codec, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "Could not open codec: %s\n", get_av_error(ret));
        return ret;
    }

    // 创建视频流
    ctx->video_stream = avformat_new_stream(ctx->output_ctx, NULL);
    if (!ctx->video_stream)
    {
        fprintf(stderr, "Could not create new stream\n");
        return -1;
    }
    ctx->video_stream->time_base = ctx->codec_ctx->time_base;
    ret = avcodec_parameters_from_context(ctx->video_stream->codecpar, ctx->codec_ctx);
    if (ret < 0)
    {
        fprintf(stderr, "Could not copy codec parameters: %s\n", get_av_error(ret));
        return ret;
    }

    // 打开输出文件
    if (!(ctx->output_ctx->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ctx->output_ctx->pb, output_url, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            fprintf(stderr, "Could not open output file: %s\n", get_av_error(ret));
            return ret;
        }
    }

    // 写入文件头
    ret = avformat_write_header(ctx->output_ctx, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "Error occurred when opening output file: %s\n", get_av_error(ret));
        return ret;
    }

    return 0;
}

// 保存一帧视频到 MP4 文件
void save_mp4(Mp4StreamContext *ctx, AVFrame *frame)
{
    int ret;
    AVPacket *pkt = av_packet_alloc();
    if (!pkt)
    {
        fprintf(stderr, "Could not allocate packet\n");
        return;
    }

    // 发送帧到编码器
    ret = avcodec_send_frame(ctx->codec_ctx, frame);
    if (ret < 0)
    {
        fprintf(stderr, "Error sending a frame for encoding: %s\n", get_av_error(ret));
        av_packet_free(&pkt);
        return;
    }

    // 从编码器接收数据包
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(ctx->codec_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        else if (ret < 0)
        {
            fprintf(stderr, "Error during encoding: %s\n", get_av_error(ret));
            break;
        }

        // 调整时间戳
        av_packet_rescale_ts(pkt, ctx->codec_ctx->time_base, ctx->video_stream->time_base);
        pkt->stream_index = ctx->video_stream->index;

        // 写入数据包到文件
        ret = av_interleaved_write_frame(ctx->output_ctx, pkt);
        if (ret < 0)
        {
            fprintf(stderr, "Error muxing packet: %s\n", get_av_error(ret));
            break;
        }

        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);
}

// 推送 MP4 处理线程函数
void *push_mp4_handler_thread(void *arg)
{
    ThreadArgs *thread_args = (ThreadArgs *)arg;
    Mp4StreamContext ctx;
    memset(&ctx, 0, sizeof(Mp4StreamContext));
    // 初始化输出流
    if (init_mp4_stream(&ctx, "./1.mp4", 1920, 1080, 25) < 0)
    {
        fprintf(stderr, "Failed to initialize Mp4 stream\n");
        return NULL;
    }

    // Main processing loop
    while (1)
    {
        QueueItem item;
        memset(&item, 0, sizeof(QueueItem));

        if (dequeue(thread_args->origin_frame_queue, &item))
        {
            if (item.type == ONLY_FRAME && item.data)
            {
                AVFrame *frame = (AVFrame *)item.data;
                save_mp4(&ctx, frame);
                av_frame_free(&frame);
            }
        }
    }

    av_write_trailer(ctx.output_ctx);
    avcodec_free_context(&ctx.codec_ctx);
    avio_closep(&ctx.output_ctx->pb);
    avformat_free_context(ctx.output_ctx);
    return NULL;
}
