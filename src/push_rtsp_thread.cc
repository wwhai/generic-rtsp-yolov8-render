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
#include "push_rtsp_thread.h"

// 错误日志记录
void log_error(const char *msg, int ret)
{
    char buffer[64] = {0};
    char *error = av_make_error_string(buffer, sizeof(buffer), ret);
    av_log(NULL, AV_LOG_ERROR, "%s: %s\n", msg, error);
}

// 推送帧到 RTSP 服务器的函数
int PushFrameToRTSPServer(AVFormatContext *fmt_ctx, AVCodecContext *codec_ctx, AVFrame *frame)
{
    AVPacket pkt;
    int ret;
    pkt.data = NULL;
    pkt.size = 0;

    ret = avcodec_send_frame(codec_ctx, frame);
    if (ret < 0)
    {
        log_error("Error sending frame", ret);
        return ret;
    }

    // 接收并推送帧
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(codec_ctx, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            log_error("Error receiving packet", ret);
            return ret;
        }

        pkt.stream_index = 0;
        ret = av_interleaved_write_frame(fmt_ctx, &pkt);
        if (ret < 0)
        {
            log_error("Error writing frame", ret);
            av_packet_unref(&pkt);
            return ret;
        }

        av_packet_unref(&pkt);
    }

    return 0;
}

// 打开并配置 AVFormatContext
int OpenOutputAVFormatContext(AVFormatContext **fmt_ctx, const char *rtsp_url)
{
    int ret;
    ret = avformat_alloc_output_context2(fmt_ctx, NULL, "rtmp", rtsp_url);
    if (ret < 0)
    {
        log_error("Could not allocate output context", ret);
        return ret;
    }

    if (!((*fmt_ctx)->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&(*fmt_ctx)->pb, rtsp_url, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            log_error("Could not open output URL", ret);
            avformat_free_context(*fmt_ctx);
            return ret;
        }
    }

    AVStream *out_stream = avformat_new_stream(*fmt_ctx, NULL);
    if (!out_stream)
    {
        fprintf(stderr, "Failed to allocate output stream\n");
        avformat_free_context(*fmt_ctx);
        return AVERROR(ENOMEM);
    }

    out_stream->codecpar->codec_id = AV_CODEC_ID_H264;
    out_stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;

    ret = avformat_write_header(*fmt_ctx, NULL);
    if (ret < 0)
    {
        log_error("Failed to write header", ret);
        avformat_free_context(*fmt_ctx);
        return ret;
    }

    return 0;
}

// 打开并配置 AVCodecContext
int OpenAVCodecContext(AVFormatContext *fmt_ctx, AVCodecContext **codec_ctx)
{
    int ret;
    AVStream *out_stream = fmt_ctx->streams[0];
    *codec_ctx = avcodec_alloc_context3(avcodec_find_encoder(out_stream->codecpar->codec_id));
    if (!(*codec_ctx))
    {
        fprintf(stderr, "Failed to allocate codec context\n");
        avformat_free_context(fmt_ctx);
        return AVERROR(ENOMEM);
    }

    ret = avcodec_parameters_to_context(*codec_ctx, out_stream->codecpar);
    if (ret < 0)
    {
        log_error("Failed to copy codec parameters", ret);
        avcodec_free_context(codec_ctx);
        avformat_free_context(fmt_ctx);
        return ret;
    }

    ret = avcodec_open2(*codec_ctx, avcodec_find_encoder((*codec_ctx)->codec_id), NULL);
    if (ret < 0)
    {
        log_error("Failed to open codec", ret);
        avcodec_free_context(codec_ctx);
        avformat_free_context(fmt_ctx);
        return ret;
    }

    return 0;
}

// 帧检测线程函数
void *push_rtsp_handler_thread(void *arg)
{
    const ThreadArgs *args = (ThreadArgs *)arg;
    const char *rtsp_url = "rtmp://127.0.0.1:1935/live/tlive001";
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    int ret;

    ret = OpenOutputAVFormatContext(&fmt_ctx, rtsp_url);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to open output AVFormatContext\n");
        pthread_exit(NULL);
    }

    ret = OpenAVCodecContext(fmt_ctx, &codec_ctx);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to open AVCodecContext\n");
        pthread_exit(NULL);
    }

    while (!args->ctx->is_cancelled) // 优化退出条件
    {
        QueueItem item;
        memset(&item, 0, sizeof(QueueItem));

        if (dequeue(args->push_origin_rtsp_queue, &item))
        {
            if (item.type == ONLY_FRAME && item.data != NULL)
            {
                AVFrame *newFrame = (AVFrame *)item.data;
                ret = PushFrameToRTSPServer(fmt_ctx, codec_ctx, newFrame);
                if (ret < 0)
                {
                    log_error("Failed to push frame to RTSP server", ret);
                }
                av_frame_free(&newFrame);
            }
        }
    }

    ret = av_write_trailer(fmt_ctx);
    if (ret < 0)
    {
        log_error("Failed to write trailer", ret);
    }

    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        avio_close(fmt_ctx->pb);
    }

    avformat_free_context(fmt_ctx);
    pthread_exit(NULL);
    return NULL;
}
