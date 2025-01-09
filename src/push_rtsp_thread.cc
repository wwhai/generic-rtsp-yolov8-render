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
// 推送帧到 RTSP 服务器的函数
int PushFrameToRTSPServer(AVFormatContext *fmt_ctx, AVCodecContext *codec_ctx, AVFrame *frame)
{
    AVPacket pkt;
    int ret;
    pkt.data = NULL;
    pkt.size = 0;

    // 将帧数据转换为 AVPacket
    ret = avcodec_send_frame(codec_ctx, frame);
    if (ret < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, 64, ret);
        av_log(NULL, AV_LOG_ERROR, "Error sending frame: %s\n", error);
        return ret;
    }

    // 接收 AVPacket
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(codec_ctx, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            break;
        }
        else if (ret < 0)
        {
            char buffer[64] = {0};
            char *error = av_make_error_string(buffer, 64, ret);
            av_log(NULL, AV_LOG_ERROR, "Error receiving packet: %s\n", error);
            return ret;
        }

        // 设置 packet 的流索引
        pkt.stream_index = 0;

        // 推送帧
        ret = av_interleaved_write_frame(fmt_ctx, &pkt);
        if (ret < 0)
        {
            char buffer[64] = {0};
            char *error = av_make_error_string(buffer, 64, ret);
            av_log(NULL, AV_LOG_ERROR, "Error writing frame: %s\n", error);
            av_packet_unref(&pkt);
            return ret;
        }

        // 释放 packet 资源
        av_packet_unref(&pkt);
    }

    return 0;
}
// 打开并配置 AVFormatContext
int OpenOutputAVFormatContext(AVFormatContext **fmt_ctx, const char *rtsp_url)
{
    int ret;
    // 分配 AVFormatContext
    ret = avformat_alloc_output_context2(fmt_ctx, NULL, "rtsp", rtsp_url);
    if (ret < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, 64, ret);
        av_log(NULL, AV_LOG_ERROR, "Could not allocate output context: %s\n", error);
        return ret;
    }

    // 打开输出 URL
    if (!((*fmt_ctx)->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&(*fmt_ctx)->pb, rtsp_url, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            char buffer[64] = {0};
            char *error = av_make_error_string(buffer, 64, ret);
            av_log(NULL, AV_LOG_ERROR, "Could not open output URL '%s': %s\n", rtsp_url, error);
            avformat_free_context(*fmt_ctx);
            return ret;
        }
    }

    // 配置流媒体参数
    AVStream *out_stream = avformat_new_stream(*fmt_ctx, NULL);
    if (!out_stream)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not create new stream\n");
        avformat_free_context(*fmt_ctx);
        return AVERROR(ENOMEM);
    }
    // 假设使用 H.264 编码，你需要根据实际情况调整
    out_stream->codecpar->codec_id = AV_CODEC_ID_H264;
    out_stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    // 更多参数设置，例如比特率、时间基等，可以根据实际情况添加

    // 写头信息
    ret = avformat_write_header(*fmt_ctx, NULL);
    if (ret < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, 64, ret);
        av_log(NULL, AV_LOG_ERROR, "Error occurred when writing header: %s\n", error);
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

    // 为输出流创建编解码器上下文
    *codec_ctx = avcodec_alloc_context3(avcodec_find_encoder(out_stream->codecpar->codec_id));
    if (!(*codec_ctx))
    {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate codec context\n");
        avformat_free_context(fmt_ctx);
        return AVERROR(ENOMEM);
    }

    // 将编解码器参数复制到编解码器上下文
    ret = avcodec_parameters_to_context(*codec_ctx, out_stream->codecpar);
    if (ret < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, 64, ret);
        av_log(NULL, AV_LOG_ERROR, "Could not copy codec parameters to context: %s\n", error);
        avcodec_free_context(codec_ctx);
        avformat_free_context(fmt_ctx);
        return ret;
    }

    // 打开编解码器
    ret = avcodec_open2(*codec_ctx, avcodec_find_encoder((*codec_ctx)->codec_id), NULL);
    if (ret < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, 64, ret);
        av_log(NULL, AV_LOG_ERROR, "Could not open codec: %s\n", error);
        avcodec_free_context(codec_ctx);
        avformat_free_context(fmt_ctx);
        return ret;
    }

    return 0;
}
// 帧检测线程函数
void *push_rtsp_thread(void *arg)
{
    const ThreadArgs *args = (ThreadArgs *)arg;
    const char *rtsp_url = "rtsp://your_rtsp_server_address";
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    int ret;

    // 打开并配置 AVFormatContext
    ret = OpenOutputAVFormatContext(&fmt_ctx, rtsp_url);
    if (ret < 0)
    {
        pthread_exit(NULL);
    }

    // 打开并配置 AVCodecContext
    ret = OpenAVCodecContext(fmt_ctx, &codec_ctx);
    if (ret < 0)
    {
        pthread_exit(NULL);
    }
    while (1)
    {
        if (args->ctx->is_cancelled)
        {
            break;
        }
        QueueItem item;
        memset(&item, 0, sizeof(QueueItem));
        if (dequeue(args->detection_queue, &item))
        {
            if (item.type == ONLY_FRAME && item.data != NULL)
            {
                AVFrame *newFrame = (AVFrame *)item.data;
                // 调用 PushFrameToRTSPServer 函数
                ret = PushFrameToRTSPServer(fmt_ctx, codec_ctx, newFrame);
                if (ret < 0)
                {
                    char buffer[64] = {0};
                    char *error = av_make_error_string(buffer, 64, ret);
                    fprintf(stderr, "Failed to push frame to RTSP server: %s\n", error);
                }
                av_frame_free(&newFrame);
            }
        }
    }

    // 写尾信息
    ret = av_write_trailer(fmt_ctx);
    if (ret < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, 64, ret);
        av_log(NULL, AV_LOG_ERROR, "Error writing trailer: %s\n", error);
    }

    // 清理资源
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        avio_close(fmt_ctx->pb);
    }
    avformat_free_context(fmt_ctx);
    pthread_exit(NULL);
    return NULL;
}