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

int PushFrameToRTMPServer(AVFormatContext *fmt_ctx, AVCodecContext *codec_ctx, AVFrame *frame)
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

    // 释放 frame 资源
    av_frame_free(&frame);

    return 0;
}
int OpenOutputAVFormatContext(AVFormatContext **fmt_ctx, const char *rtsp_url)
{
    int ret;

    // Step 1: Allocate output context
    ret = avformat_alloc_output_context2(fmt_ctx, NULL, "flv", rtsp_url);
    if (ret < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, sizeof(buffer), ret);
        fprintf(stderr, "Could not allocate output context: %s\n", error);
        return ret;
    }
    // Step 2: Check if we need to open a file (if it's a network stream like RTMP/RTSP, no file is involved)
    if (!((*fmt_ctx)->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&(*fmt_ctx)->pb, rtsp_url, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            char buffer[64] = {0};
            char *error = av_make_error_string(buffer, sizeof(buffer), ret);
            fprintf(stderr, "Could not open output URL: %s\n", error);
            avformat_free_context(*fmt_ctx);
            return ret;
        }
    }
    // Step 3: Create a video stream (H.264 or others)
    AVStream *out_stream = avformat_new_stream(*fmt_ctx, NULL);
    if (!out_stream)
    {
        fprintf(stderr, "Failed to allocate output stream\n");
        avformat_free_context(*fmt_ctx);
        return AVERROR(ENOMEM);
    }
    // Step 4: Set codec parameters for the stream
    // You can configure this based on the codec you're using (e.g., H.264)
    out_stream->codecpar->codec_id = AV_CODEC_ID_H264; // H.264 codec for video
    out_stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    out_stream->codecpar->width = 1920;                // Set video width
    out_stream->codecpar->height = 1080;               // Set video height
    out_stream->codecpar->format = AV_PIX_FMT_YUV420P; // Pixel format (YUV420P is common)

    // Step 5: Write output file header (important for starting the stream)
    ret = avformat_write_header(*fmt_ctx, NULL);
    if (ret < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, sizeof(buffer), ret);
        fprintf(stderr, "Failed to write header: %s\n", error);
        avformat_free_context(*fmt_ctx);
        return ret;
    }
    return 0;
}

int OpenAVCodecContext(AVFormatContext *fmt_ctx, AVCodecContext **codec_ctx)
{
    int ret;
    AVStream *out_stream = fmt_ctx->streams[0];

    // 查找编码器
    const AVCodec *codec = avcodec_find_encoder(out_stream->codecpar->codec_id);
    if (!codec)
    {
        fprintf(stderr, "Codec not found\n");
        avformat_free_context(fmt_ctx);
        return AVERROR_ENCODER_NOT_FOUND;
    }
    // 分配编码器上下文
    *codec_ctx = avcodec_alloc_context3(codec);
    if (!(*codec_ctx))
    {
        fprintf(stderr, "Failed to allocate codec context\n");
        avformat_free_context(fmt_ctx);
        return AVERROR(ENOMEM);
    }

    // 将流的参数复制到编码器上下文
    ret = avcodec_parameters_to_context(*codec_ctx, out_stream->codecpar);
    if (ret < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, sizeof(buffer), ret);
        fprintf(stderr, "Failed to copy codec parameters: %s\n", error);
        avcodec_free_context(codec_ctx);
        avformat_free_context(fmt_ctx);
        return ret;
    }
    // 设置视频分辨率和其他参数
    (*codec_ctx)->bit_rate = 5000000;              // 设置比特率为 5 Mbps
    (*codec_ctx)->width = 1920;                    // 设置视频宽度为 1920
    (*codec_ctx)->height = 1080;                   // 设置视频高度为 1080
    (*codec_ctx)->time_base = (AVRational){1, 25}; // 设置时间基为 30 FPS
    (*codec_ctx)->gop_size = 12;                   // 设置 GOP 大小
    (*codec_ctx)->max_b_frames = 3;                // 设置最大 B 帧数

    // 设置像素格式为 YUV420P，FLV 容器支持此像素格式
    (*codec_ctx)->pix_fmt = AV_PIX_FMT_YUV420P;

    // 打开编码器
    ret = avcodec_open2(*codec_ctx, codec, NULL);
    if (ret < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, sizeof(buffer), ret);
        fprintf(stderr, "Failed to open codec: %s\n", error);
        avcodec_free_context(codec_ctx);
        avformat_free_context(fmt_ctx);
        return ret;
    }
    return 0;
}

// 帧检测线程函数
void *push_rtmp_handler_thread(void *arg)
{
    const ThreadArgs *args = (ThreadArgs *)arg;
    const char *rtmp_url = "rtmp://192.168.10.7:1935/live/tlive001"; // RTMP 流地址
    AVFormatContext *fmt_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    int ret;
    // Step 1: 打开并配置 AVFormatContext
    ret = OpenOutputAVFormatContext(&fmt_ctx, rtmp_url);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to open output AVFormatContext\n");
        pthread_exit(NULL);
    }
    // 打印出 AVFormatContext 的信息
    av_dump_format(fmt_ctx, 0, rtmp_url, 1);
    // Step 2: 打开并配置 AVCodecContext
    ret = OpenAVCodecContext(fmt_ctx, &codec_ctx);
    if (ret < 0)
    {
        fprintf(stderr, "Failed to open AVCodecContext\n");
        pthread_exit(NULL);
    }

    // Step 3: 循环推送视频帧
    while (1)
    {
        if (args->ctx->is_cancelled) // 检查是否需要取消线程
        {
            fprintf(stderr, "RTSP handler thread cancelled.\n");
            break;
        }
        // fprintf(stderr, "RTSP handler thread started. Push stream: %s\n", args->rtsp_url);
        QueueItem item;
        memset(&item, 0, sizeof(QueueItem));
        // Step 4: 从队列中获取视频帧
        if (dequeue(args->push_origin_rtsp_queue, &item))
        {
            if (item.type == ONLY_FRAME && item.data != NULL)
            {
                AVFrame *newFrame = (AVFrame *)item.data;
                // Step 5: 推送帧到 RTMP 服务器
                ret = PushFrameToRTMPServer(fmt_ctx, codec_ctx, newFrame);
                if (ret < 0)
                {
                    char buffer[64] = {0};
                    char *error = av_make_error_string(buffer, sizeof(buffer), ret);
                    fprintf(stderr, "Failed to push frame to RTMP server: %s\n", error);
                }
                // Step 6: 释放帧内存
                av_frame_free(&newFrame);
            }
        }
    }
    fprintf(stderr, "RTSP handler thread finished. Push stream: %s\n", args->rtsp_url);
    // Step 7: 写尾信息（如果需要）
    ret = av_write_trailer(fmt_ctx);
    if (ret < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, sizeof(buffer), ret);
        fprintf(stderr, "Failed to write trailer: %s\n", error);
    }

    // Step 8: 清理资源
    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
    {
        avio_close(fmt_ctx->pb);
    }
    fprintf(stderr, "RTSP handler thread started. Push stream: %s\n", args->rtsp_url);
    avformat_free_context(fmt_ctx);
    pthread_exit(NULL);
    return NULL;
}
