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
    if (!ctx || !output_url)
    {
        fprintf(stdout, "Invalid input parameters for init_rtmp_stream\n");
        return -1;
    }
    // 输出参数
    fprintf(stdout, "init_rtmp_stream === output_url=%s,width=%d,height=%d,fps=%d\n",
            output_url, width, height, fps);
    // 创建输出上下文
    int ret = avformat_alloc_output_context2(&ctx->output_ctx, NULL, "flv", output_url);
    if (ret < 0 || !ctx->output_ctx)
    {
        fprintf(stdout, "Failed to create output context: %s\n", get_av_error(ret));
        return -1;
    }
    // 查找编码器
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        fprintf(stdout, "H.264 encoder not found\n");
        goto cleanup_output_context;
    }
    // 创建编码器上下文
    ctx->codec_ctx = avcodec_alloc_context3(codec);
    if (!ctx->codec_ctx)
    {
        fprintf(stdout, "Failed to allocate codec context\n");
        goto cleanup_output_context;
    }
    // 从输入流复制编解码器参数到编码器上下文
    if (ctx->input_stream_codecpar)
    {
        ret = avcodec_parameters_to_context(ctx->codec_ctx, ctx->input_stream_codecpar);
        if (ret < 0)
        {
            fprintf(stdout, "Failed to copy codec parameters from input stream: %s\n", get_av_error(ret));
            goto cleanup_codec_context;
        }
    }
    // 配置编码参数
    ctx->codec_ctx->width = width;
    ctx->codec_ctx->height = height;
    ctx->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    ctx->codec_ctx->time_base = (AVRational){1, fps};
    ctx->codec_ctx->framerate = (AVRational){fps, 1};
    // H.264 高级配置
    av_opt_set(ctx->codec_ctx->priv_data, "preset", "fast", 0);
    av_opt_set(ctx->codec_ctx->priv_data, "tune", "zerolatency", 0);
    // 创建输出流
    ctx->video_stream = avformat_new_stream(ctx->output_ctx, NULL);
    if (!ctx->video_stream)
    {
        fprintf(stdout, "Failed to create video stream\n");
        goto cleanup_codec_context;
    }
    // 关联编码器参数到输出流
    ret = avcodec_parameters_from_context(ctx->video_stream->codecpar, ctx->codec_ctx);
    if (ret < 0)
    {
        fprintf(stdout, "Failed to copy codec parameters to output stream: %s\n", get_av_error(ret));
        goto cleanup_codec_context;
    }
    // 打开编码器
    ret = avcodec_open2(ctx->codec_ctx, codec, NULL);
    if (ret < 0)
    {
        fprintf(stdout, "Failed to open codec: %s\n", get_av_error(ret));
        goto cleanup_codec_context;
    }
    // 打开网络输出
    if (!(ctx->output_ctx->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ctx->output_ctx->pb, output_url, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            fprintf(stdout, "Failed to open output URL: %s\n", get_av_error(ret));
            goto cleanup_codec_context;
        }
    }
    // 写入文件头
    ret = avformat_write_header(ctx->output_ctx, NULL);
    if (ret < 0)
    {
        fprintf(stdout, "Failed to write header: %d, %s\n", ret, get_av_error(ret));
        goto cleanup_io;
    }
    return 0;
cleanup_io:
    if (!(ctx->output_ctx->oformat->flags & AVFMT_NOFILE))
    {
        avio_closep(&ctx->output_ctx->pb);
    }
cleanup_codec_context:
    avcodec_free_context(&ctx->codec_ctx);
cleanup_output_context:
    avformat_free_context(ctx->output_ctx);
    return -1;
}
// 优化后的 push_stream 函数
void push_stream(RtmpStreamContext *ctx, AVFrame *frame)
{
    if (!ctx || !frame)
    {
        fprintf(stdout, "Invalid input parameters: RtmpStreamContext or AVFrame is NULL\n");
        return;
    }
    int ret = 0;
    // 发送帧到编码器
    ret = avcodec_send_frame(ctx->codec_ctx, frame);
    if (ret < 0)
    {
        fprintf(stdout, "Error sending frame: %s\n", get_av_error(ret));
        return;
    }
    AVPacket *pkt = av_packet_alloc();
    if (!pkt)
    {
        fprintf(stdout, "Error allocating AVPacket\n");
        return;
    }
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
            fprintf(stdout, "Error encoding frame: %s\n", get_av_error(ret));
            break;
        }

        av_packet_rescale_ts(pkt, ctx->input_stream->time_base, ctx->video_stream->time_base);
        ret = av_interleaved_write_frame(ctx->output_ctx, pkt);
        if (ret < 0)
        {
            fprintf(stdout, "Error writing packet: %d, %s\n", ret, get_av_error(ret));
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
    ctx.input_stream_codecpar = args->input_stream_codecpar;
    ctx.input_stream = args->input_stream;
    // 初始化输出流
    if (init_rtmp_stream(&ctx, args->output_stream_url, 1920, 1080, 25) < 0)
    {
        fprintf(stdout, "Failed to initialize RTMP stream\n");
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