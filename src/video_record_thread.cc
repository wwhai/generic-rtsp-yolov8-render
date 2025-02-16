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
#include "libav_utils.h"
#include "logger.h"
// 初始化 RTMP 流上下文
int init_mp4_stream(Mp4StreamContext *ctx, const char *output_url, int width, int height, int fps)
{
    if (!ctx || !output_url)
    {
        log_info( "Invalid input parameters for init_rtmp_stream");
        return -1;
    }
    // 输出参数
    log_info( "init_rtmp_stream === output_url=%s,width=%d,height=%d,fps=%d",
                output_url, width, height, fps);
    // 创建输出上下文
    int ret = avformat_alloc_output_context2(&ctx->output_ctx, NULL, "flv", output_url);
    if (ret < 0 || !ctx->output_ctx)
    {
        log_info( "Failed to create output context: %s", get_av_error(ret));
        return -1;
    }
    // 查找编码器
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        log_info( "H.264 encoder not found");
        goto cleanup_output_context;
    }
    // 创建编码器上下文
    ctx->codec_ctx = avcodec_alloc_context3(codec);
    if (!ctx->codec_ctx)
    {
        log_info( "Failed to allocate codec context");
        goto cleanup_output_context;
    }
    // 从输入流复制编解码器参数到编码器上下文
    if (ctx->input_stream->codecpar)
    {
        ret = avcodec_parameters_to_context(ctx->codec_ctx, ctx->input_stream->codecpar);
        if (ret < 0)
        {
            log_info( "Failed to copy codec parameters from input stream: %s", get_av_error(ret));
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
        log_info( "Failed to create video stream");
        goto cleanup_codec_context;
    }
    // 关联编码器参数到输出流
    ret = avcodec_parameters_from_context(ctx->video_stream->codecpar, ctx->codec_ctx);
    if (ret < 0)
    {
        log_info( "Failed to copy codec parameters to output stream: %s", get_av_error(ret));
        goto cleanup_codec_context;
    }
    // 打开编码器
    ret = avcodec_open2(ctx->codec_ctx, codec, NULL);
    if (ret < 0)
    {
        log_info( "Failed to open codec: %s", get_av_error(ret));
        goto cleanup_codec_context;
    }
    log_info( "=== av_dump_format output.mp4 === ");
    av_dump_format(ctx->output_ctx, 0, "output.mp4", 1);
    log_info( "================================= ");
    // 打开网络输出
    if (!(ctx->output_ctx->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ctx->output_ctx->pb, output_url, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            log_info( "Failed to open output URL: %s", get_av_error(ret));
            goto cleanup_codec_context;
        }
    }
    // 写入文件头
    ret = avformat_write_header(ctx->output_ctx, NULL);
    if (ret < 0)
    {
        log_info( "Failed to write header: %d, %s", ret, get_av_error(ret));
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
void save_mp4(Mp4StreamContext *ctx, AVFrame *frame)
{
    if (!ctx || !frame)
    {
        log_info( "Invalid input parameters: RtmpStreamContext or AVFrame is NULL");
        return;
    }
    int ret = 0;
    // 发送帧到编码器
    ret = avcodec_send_frame(ctx->codec_ctx, frame);
    if (ret < 0)
    {
        log_info( "Error sending frame: %s", get_av_error(ret));
        return;
    }
    AVPacket *pkt = av_packet_alloc();
    if (!pkt)
    {
        log_info( "Error allocating AVPacket");
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
            log_info( "Error encoding frame: %s", get_av_error(ret));
            break;
        }
        av_packet_rescale_ts(pkt, ctx->input_stream->time_base, ctx->video_stream->time_base);

        ret = av_interleaved_write_frame(ctx->output_ctx, pkt);
        if (ret < 0)
        {
            log_info( "Error writing packet: %d, %s", ret, get_av_error(ret));
        }
        // 释放数据包
        av_packet_unref(pkt);
    }
    av_packet_free(&pkt);
}

void *save_mp4_handler_thread(void *arg)
{
    ThreadArgs *args = (ThreadArgs *)arg;

    Mp4StreamContext ctx;
    memset(&ctx, 0, sizeof(Mp4StreamContext));
    ctx.input_stream = args->input_stream;

    // 初始化输出流
    log_info( "Start save mp4 record thread");
    // 获取当前时间戳作为文件名
    time_t current_time = time(NULL);
    char file_name[100];
    strftime(file_name, sizeof(file_name), "./local_%Y%m%d_%H%M%S.mp4", localtime(&current_time));
    if (init_mp4_stream(&ctx, file_name, 1920, 1080, 25) < 0)
    {
        log_info( "Failed to initialize RTMP stream");
        return NULL;
    }

    // 记录开始时间
    time_t start_time = time(NULL);
    int file_index = 0;

    // Main processing loop
    while (!args->ctx->is_cancelled)
    {
        QueueItem item;
        memset(&item, 0, sizeof(QueueItem));

        if (dequeue(args->record_frame_queue, &item))
        {
            if (item.type == ONLY_FRAME && item.data)
            {
                AVFrame *frame = (AVFrame *)item.data;
                // 检查是否超过 30 分钟
                current_time = time(NULL);
                double elapsed_time = difftime(current_time, start_time);
                if (elapsed_time >= 30 * 60)
                {
                    // 关闭当前文件
                    av_write_trailer(ctx.output_ctx);
                    avio_closep(&ctx.output_ctx->pb);
                    avcodec_free_context(&ctx.codec_ctx);
                    avformat_free_context(ctx.output_ctx);

                    // 新建文件
                    file_index++;
                    // 获取新的时间戳作为文件名
                    current_time = time(NULL);
                    strftime(file_name, sizeof(file_name), "./local_%Y%m%d_%H%M%S.mp4", localtime(&current_time));
                    memset(&ctx, 0, sizeof(Mp4StreamContext));
                    ctx.input_stream = args->input_stream;
                    if (init_mp4_stream(&ctx, file_name, 1920, 1080, 25) < 0)
                    {
                        log_info( "Failed to initialize new MP4 stream");
                        return NULL;
                    }
                    start_time = time(NULL);
                }
                save_mp4(&ctx, frame);
                av_frame_free(&frame);
            }
        }
    }

    log_info( "Stop save mp4 record thread");
    av_write_trailer(ctx.output_ctx);
    avio_closep(&ctx.output_ctx->pb);
    avcodec_free_context(&ctx.codec_ctx);
    avformat_free_context(ctx.output_ctx);
    return NULL;
}