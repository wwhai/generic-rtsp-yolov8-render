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
    int ret;

    // 1. 打开输出流上下文
    ret = avformat_alloc_output_context2(&ctx->output_ctx, NULL, "flv", output_url);
    if (ret < 0 || !ctx->output_ctx)
    {
        fprintf(stderr, "Failed to allocate output context: %s\n", get_av_error(ret));
        return -1;
    }

    // 2. 配置视频编码器
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        fprintf(stderr, "H264 encoder not found\n");
        return -1;
    }

    ctx->output_stream = avformat_new_stream(ctx->output_ctx, codec);
    if (!ctx->output_stream)
    {
        fprintf(stderr, "Failed to create video stream\n");
        return -1;
    }

    ctx->output_codec_ctx = avcodec_alloc_context3(codec);
    if (!ctx->output_codec_ctx)
    {
        fprintf(stderr, "Failed to allocate codec context\n");
        return -1;
    }
    if (avcodec_parameters_copy(ctx->output_stream->codecpar, ctx->input_stream->codecpar) < 0)
    {
        fprintf(stderr, "Failed to copy codec parameters: %s\n", get_av_error(ret));
        return -1;
    }
    if (ctx->output_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        ctx->output_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    // 4. 打开编码器
    ret = avcodec_open2(ctx->output_codec_ctx, codec, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to open codec: %s\n", get_av_error(ret));
        return -1;
    }

    // 5. 设置流编码器参数
    ret = avcodec_parameters_from_context(ctx->output_stream->codecpar, ctx->output_codec_ctx);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to copy codec parameters: %s\n", get_av_error(ret));
        return -1;
    }

    // 6. 打开输出流
    if (!(ctx->output_ctx->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ctx->output_ctx->pb, output_url, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            fprintf(stderr, "Failed to open output URL: %s\n", get_av_error(ret));
            return -1;
        }
    }

    // 7. 写文件头
    ret = avformat_write_header(ctx->output_ctx, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to write header: %s\n", get_av_error(ret));
        return -1;
    }

    return 0;
}

// 在推送帧之前，确保 PTS 和 DTS 是递增的
static int64_t last_pts = 0;
static int64_t last_dts = 0;

void push_stream(RtmpStreamContext *ctx, AVFrame *frame)
{
    int ret;

    // 设置 PTS 和 DTS
    if (frame->pts == AV_NOPTS_VALUE)
    {
        // 如果 PTS 无效，根据上一帧的 PTS 递增
        frame->pts = last_pts + 1;
    }
    if (frame->pkt_dts == AV_NOPTS_VALUE)
    {
        // 如果 DTS 无效，根据上一帧的 DTS 递增
        frame->pkt_dts = last_dts + 1;
    }

    // 更新 last_pts 和 last_dts
    last_pts = frame->pts;
    last_dts = frame->pkt_dts;

    // 1. 编码帧
    ret = avcodec_send_frame(ctx->output_codec_ctx, frame);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to send frame for encoding: %s\n", get_av_error(ret));
        return;
    }

    // 2. 创建一个新的 AVPacket
    AVPacket *pkt = av_packet_alloc();
    if (!pkt)
    {
        fprintf(stderr, "Failed to allocate AVPacket\n");
        return;
    }

    // 3. 获取编码后的数据包
    ret = avcodec_receive_packet(ctx->output_codec_ctx, pkt);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
    {
        av_packet_free(&pkt);
        return;
    }
    else if (ret < 0)
    {
        fprintf(stderr, "Failed to receive packet: %s\n", get_av_error(ret));
        av_packet_free(&pkt);
        return;
    }

    // 4. 设置时间戳
    pkt->stream_index = ctx->output_stream->index;
    av_packet_rescale_ts(pkt, ctx->output_codec_ctx->time_base, ctx->output_stream->time_base);

    // 5. 推送数据包
    ret = av_write_frame(ctx->output_ctx, pkt);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to write frame: %s\n", get_av_error(ret));
    }

    // 6. 释放 AVPacket
    av_packet_free(&pkt);
}
// 推流线程处理函数
void *push_rtmp_handler_thread(void *arg)
{
    ThreadArgs *args = (ThreadArgs *)arg;
    RtmpStreamContext ctx;
    memset(&ctx, 0, sizeof(RtmpStreamContext));
    fprintf(stderr, "push_rtmp_handler_thread\n");
    ctx.input_stream = args->input_stream;
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
    CancelContext(args->ctx);
    av_write_trailer(ctx.output_ctx);
    avcodec_free_context(&ctx.output_codec_ctx);
    avio_closep(&ctx.output_ctx->pb);
    avformat_free_context(ctx.output_ctx);
    return NULL;
}