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

const char *get_av_error(int errnum)
{
    static char str[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, str, sizeof(str));
    return str;
}

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
    av_opt_set(ctx->codec_ctx->priv_data, "preset", "fast", 0);
    av_opt_set(ctx->codec_ctx->priv_data, "tune", "zerolatency", 0);

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
        fprintf(stderr, "Failed to write header: %s\n", get_av_error(ret));
        return -1;
    }

    return 0;
}

// 优化后的push_stream函数
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

    // 接收编码后的数据包
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

        // 确保时间戳被正确设置
        if (pkt->pts == AV_NOPTS_VALUE)
        {
            // 如果编码器没有产生正确的 PTS，我们可以手动设置
            pkt->pts = frame->pts;
        }
        if (pkt->dts == AV_NOPTS_VALUE)
        {
            pkt->dts = frame->pts;
        }

        // 手动计算并设置数据包的持续时间
        if (pkt->duration == 0)
        {
            // 根据帧率计算持续时间
            AVRational time_base = ctx->codec_ctx->time_base;
            int fps = (int)(1 / av_q2d(time_base));
            pkt->duration = av_rescale_q(1, (AVRational){1, fps}, time_base);
        }

        // 设置数据包属性
        pkt->stream_index = ctx->video_stream->index;

        // 重新缩放时间戳
        av_packet_rescale_ts(pkt, ctx->codec_ctx->time_base, ctx->video_stream->time_base);

        // 打印数据包信息
        // fprintf(stdout, "Encoded packet information:\n");
        // fprintf(stdout, "  size: %d\n", pkt->size);
        // fprintf(stdout, "  pts: %" PRId64 "\n", pkt->pts);
        // fprintf(stdout, "  dts: %" PRId64 "\n", pkt->dts);
        // fprintf(stdout, "  duration: %" PRId64 "\n", pkt->duration);

        // 发送数据包
        ret = av_interleaved_write_frame(ctx->output_ctx, pkt);
        if (ret < 0)
        {
            fprintf(stderr, "Error writing packet: %s\n", get_av_error(ret));
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
    const char *output_url = "rtmp://192.168.10.5:1935/live/tlive001";

    RtmpStreamContext ctx;
    memset(&ctx, 0, sizeof(RtmpStreamContext));

    // 初始化输出流
    if (init_rtmp_stream(&ctx, output_url, 1920, 1080, 25) < 0)
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