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

#include "push_stream_thread.h"
#include "libav_utils.h"
// 初始化 RTMP 流上下文
int init_rtmp_stream(RtmpStreamContext *ctx, const char *output_url, int width, int height, int fps)
{
    // 输出参数
    fprintf(stderr, "init_rtmp_stream === output_url=%s,width=%d,height=%d,fps=%d\n",
            output_url, width, height, fps);
    // 创建输出上下文
    int ret = avformat_alloc_output_context2(&ctx->output_ctx, NULL, "flv", output_url);
    if (ret < 0 || !ctx->output_ctx)
    {
        fprintf(stderr, "Failed to create output context: %s\n", get_av_error(ret));
        return -1;
    }

    // 查找编码器
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        fprintf(stderr, "H.264 encoder not found\n");
        return -1;
    }

    // 创建编码器上下文
    ctx->codec_ctx = avcodec_alloc_context3(codec);
    if (!ctx->codec_ctx)
    {
        fprintf(stderr, "Failed to allocate codec context\n");
        return -1;
    }

    // 配置编码参数
    ctx->codec_ctx->width = width;
    ctx->codec_ctx->height = height;
    ctx->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    ctx->codec_ctx->time_base = (AVRational){1, fps};
    ctx->codec_ctx->framerate = (AVRational){fps, 1};
    ctx->codec_ctx->bit_rate = 4000000;
    ctx->codec_ctx->gop_size = 12;

    // H.264高级配置
    // av_opt_set(ctx->codec_ctx->priv_data, "preset", "fast", 0);
    // av_opt_set(ctx->codec_ctx->priv_data, "tune", "zerolatency", 0);

    // 创建输出流
    ctx->video_stream = avformat_new_stream(ctx->output_ctx, NULL);
    if (!ctx->video_stream)
    {
        fprintf(stderr, "Failed to create video stream\n");
        return -1;
    }

    // 关联编码器参数到流
    ret = avcodec_parameters_from_context(ctx->video_stream->codecpar, ctx->codec_ctx);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to copy codec parameters: %s\n", get_av_error(ret));
        return -1;
    }

    // 打开编码器
    ret = avcodec_open2(ctx->codec_ctx, codec, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to open codec: %s\n", get_av_error(ret));
        return -1;
    }

    // 打开网络输出
    if (!(ctx->output_ctx->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ctx->output_ctx->pb, output_url, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            fprintf(stderr, "Failed to open output URL: %s\n", get_av_error(ret));
            return -1;
        }
    }

    // 写入文件头
    ret = avformat_write_header(ctx->output_ctx, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to write header:%d, %s\n", ret, get_av_error(ret));
        return -1;
    }

    return 0;
}

// 优化后的 push_stream 函数
void push_stream(RtmpStreamContext *ctx, AVFrame *frame)
{
    if (!ctx || !frame)
    {
        fprintf(stderr, "Invalid input parameters: RtmpStreamContext or AVFrame is NULL\n");
        return;
    }
    int ret = 0;
    // 发送帧到编码器
    ret = avcodec_send_frame(ctx->codec_ctx, frame);
    if (ret < 0)
    {
        fprintf(stderr, "Error sending frame: %s\n", get_av_error(ret));
        return;
    }
    AVPacket *pkt = av_packet_alloc();
    if (!pkt)
    {
        fprintf(stderr, "Error allocating AVPacket\n");
        return;
    }
    static int64_t last_dts = -1; // 用于记录上一个 DTS
    static int64_t last_pts = -1; // 用于记录上一个 PTS
    // 接收编码后的数据包
    while (1)
    {
        ret = avcodec_receive_packet(ctx->codec_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            // 没有更多数据或需要更多输入帧
            break;
        }
        else if (ret < 0)
        {
            fprintf(stderr, "Error encoding frame: %s\n", get_av_error(ret));
            break;
        }
        // 输出 frame index
        fprintf(stderr, "Packet index: %d\n", pkt->stream_index);
        fprintf(stderr, "Codec time base: %d/%d\n", ctx->codec_ctx->time_base.num, ctx->codec_ctx->time_base.den);
        fprintf(stderr, "Video stream time base: %d/%d\n", ctx->video_stream->time_base.num, ctx->video_stream->time_base.den);
        fprintf(stderr, "BEFORE ====== PTS: %ld, DTS: %ld, Duration: %ld\n", pkt->pts, pkt->dts, pkt->duration);
        // 调整时间戳
        if (pkt->pts != AV_NOPTS_VALUE)
        {
            pkt->pts = av_rescale_q_rnd(pkt->pts, ctx->codec_ctx->time_base, ctx->video_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        }
        if (pkt->dts != AV_NOPTS_VALUE)
        {
            pkt->dts = av_rescale_q_rnd(pkt->dts, ctx->codec_ctx->time_base, ctx->video_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        }
        int fps = 25;
        AVRational codec_time_base = ctx->codec_ctx->time_base;
        pkt->duration = av_rescale_q(1, (AVRational){1, fps}, codec_time_base);
        pkt->pos = -1;
        pkt->stream_index = ctx->video_stream->index;
        // 确保 DTS 和 PTS 单调递增
        if (last_dts != -1 && (pkt->dts == AV_NOPTS_VALUE || pkt->dts <= last_dts))
        {
            pkt->dts = last_dts + pkt->duration;
        }
        if (last_pts != -1 && (pkt->pts == AV_NOPTS_VALUE || pkt->pts <= last_pts))
        {
            pkt->pts = last_pts + pkt->duration;
        }
        last_dts = pkt->dts;
        last_pts = pkt->pts;
        fprintf(stderr, "AFTER ====== PTS: %ld, DTS: %ld, Duration: %ld\n", pkt->pts, pkt->dts, pkt->duration);
        // 检查数据包大小
        if (pkt->size <= 0)
        {
            fprintf(stderr, "Packet size is invalid: %d\n", pkt->size);
            av_packet_free(&pkt);
            return;
        }
        ret = av_interleaved_write_frame(ctx->output_ctx, pkt);
        if (ret < 0)
        {
            fprintf(stderr, "Error writing packet: %d, %s\n", ret, get_av_error(ret));
        }
        // 释放数据包
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
}

// 推流线程处理函数
void *push_rtmp_handler_thread(void *arg)
{
    ThreadArgs *args = (ThreadArgs *)arg;

    RtmpStreamContext ctx;
    memset(&ctx, 0, sizeof(RtmpStreamContext));

    // 初始化输出流
    if (init_rtmp_stream(&ctx, args->output_stream_url, 1920, 1080, 25) < 0)
    {
        fprintf(stderr, "Failed to initialize RTMP stream\n");
        return NULL;
    }

    // Main processing loop
    while (1)
    {
        QueueItem item;
        memset(&item, 0, sizeof(QueueItem));

        if (dequeue(args->origin_frame_queue, &item))
        {
            if (item.type == ONLY_FRAME && item.data)
            {
                AVFrame *frame = (AVFrame *)item.data;
                push_stream(&ctx, frame);
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