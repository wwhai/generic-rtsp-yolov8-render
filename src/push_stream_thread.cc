#include "push_stream_thread.h"

// 初始化 AVFormatContext 和 AVCodecContext
void init_av_contexts(AVFormatContext **output_ctx, AVCodecContext **codec_ctx, const char *rtmp_url)
{
    int ret;
    // 分配输出上下文
    if ((ret = avformat_alloc_output_context2(output_ctx, NULL, "flv", rtmp_url)) < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, 64, ret);
        fprintf(stderr, "Error avformat_alloc_output_context2: %s\n", error);
        exit(1);
    }

    // 查找编码器
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, 64, ret);
        fprintf(stderr, "Error avcodec_find_encoder: %s\n", error);
        exit(1);
    }

    // 分配编解码器上下文
    *codec_ctx = avcodec_alloc_context3(codec);
    if (!*codec_ctx)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, 64, ret);
        fprintf(stderr, "Error avcodec_alloc_context3: %s\n", error);
        exit(1);
    }

    // 设置编解码器参数
    (*codec_ctx)->bit_rate = 400000;
    (*codec_ctx)->width = 1920;
    (*codec_ctx)->height = 1080;
    (*codec_ctx)->time_base = (AVRational){1, 25};
    (*codec_ctx)->framerate = (AVRational){25, 1};
    (*codec_ctx)->gop_size = 10;
    (*codec_ctx)->max_b_frames = 1;
    (*codec_ctx)->pix_fmt = AV_PIX_FMT_YUV420P;

    if ((ret = avcodec_open2(*codec_ctx, codec, NULL)) < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, 64, ret);
        fprintf(stderr, "Error avcodec_open2: %s\n", error);
        exit(1);
    }

    // 为输出上下文添加流信息
    AVStream *out_stream = avformat_new_stream(*output_ctx, codec);
    if (!out_stream)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, 64, ret);
        fprintf(stderr, "Error avformat_new_stream: %s\n", error);
        exit(1);
    }

    if ((ret = avcodec_parameters_from_context(out_stream->codecpar, *codec_ctx)) < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, 64, ret);
        fprintf(stderr, "Error avcodec_parameters_from_context: %s\n", error);
        exit(1);
    }

    // 打开输出 URL
    if (!((*output_ctx)->oformat->flags & AVFMT_NOFILE))
    {
        if ((ret = avio_open(&(*output_ctx)->pb, rtmp_url, AVIO_FLAG_WRITE)) < 0)
        {
            char buffer[64] = {0};
            char *error = av_make_error_string(buffer, 64, ret);
            fprintf(stderr, "Error avio_open: %s\n", error);
            exit(1);
        }
    }

    // 写文件头
    if ((ret = avformat_write_header(*output_ctx, NULL)) < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, 64, ret);
        fprintf(stderr, "Error avformat_write_header: %s\n", error);
        exit(1);
    }
}

// 在函数外定义和初始化 previous_pts
static int64_t previous_pts = AV_NOPTS_VALUE; // 初始时设置为无效值

// 推送数据包到 RTMP 服务器
int push_rtmp_frame(AVCodecContext *codec_ctx, AVFrame *frame, AVPacket *pkt, AVFormatContext *output_ctx)
{
    int ret;

    // 如果帧没有设置 PTS，手动设置一个递增的 PTS
    if (frame->pts == AV_NOPTS_VALUE)
    {
        // 如果 previous_pts 是无效值（第一次推流时），设置为一个默认的值
        if (previous_pts == AV_NOPTS_VALUE)
        {
            frame->pts = 0;
        }
        else
        {
            // 否则，递增 PTS
            frame->pts = previous_pts + 1; // PTS 增加 1，这里可以根据帧率等进行调整
        }
    }

    // 更新 previous_pts，用于下一帧的计算
    previous_pts = frame->pts;

    // 发送帧到编码器
    if ((ret = avcodec_send_frame(codec_ctx, frame)) < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, 64, ret);
        fprintf(stderr, "Error sending frame to encoder: %s\n", error);
        return ret;
    }

    // 接收编码后的数据包
    ret = avcodec_receive_packet(codec_ctx, pkt);
    while (ret >= 0)
    {
        // 设置数据包的时间戳和时间基准
        pkt->pts = frame->pts;
        pkt->dts = frame->pkt_dts;
        // pkt->duration = frame->pkt_duration;
        // pkt->time_base = codec_ctx->time_base;

        // 推送数据包到 RTMP 服务器
        ret = av_interleaved_write_frame(output_ctx, pkt);
        if (ret < 0)
        {
            char buffer[64] = {0};
            char *error = av_make_error_string(buffer, 64, ret);
            fprintf(stderr, "Error writing packet to output file: %s\n", error);
        }

        av_packet_unref(pkt); // 释放数据包资源
        ret = avcodec_receive_packet(codec_ctx, pkt);
    }

    return 0;
}

// 推流线程处理函数
void *push_rtmp_handler_thread(void *arg)
{
    ThreadArgs *args = (ThreadArgs *)arg;
    const char *rtmp_url = RTMP_URL;
    AVFormatContext *output_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVPacket pkt;
    pkt.data = NULL;
    pkt.size = 0;

    init_av_contexts(&output_ctx, &codec_ctx, rtmp_url);

    while (1)
    {
        QueueItem item;
        memset(&item, 0, sizeof(QueueItem));

        if (dequeue(args->origin_frame_queue, &item))
        {
            if (item.type == ONLY_FRAME && item.data != NULL)
            {
                AVFrame *newFrame = (AVFrame *)item.data;
                int ret = push_rtmp_frame(codec_ctx, newFrame, &pkt, output_ctx);
                if (ret < 0)
                {
                    fprintf(stderr, "Failed to encode frame\n");
                }
                av_frame_free(&newFrame);
            }
        }
    }

    // 写文件尾
    av_write_trailer(output_ctx);

    // 清理资源
    if (output_ctx && !(output_ctx->oformat->flags & AVFMT_NOFILE))
    {
        avio_close(output_ctx->pb);
    }
    avcodec_free_context(&codec_ctx);
    avformat_free_context(output_ctx);

    return NULL;
}
