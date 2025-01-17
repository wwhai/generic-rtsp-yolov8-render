#include "push_stream_thread.h"

// 初始化 AV 上下文相关的函数
int init_av_contexts(const char *input_url, const char *output_url,
                     AVFormatContext **input_format_ctx, AVFormatContext **output_format_ctx,
                     AVCodecContext **input_codec_ctx, AVStream **input_stream, AVStream **output_stream)
{
    // 打开输入 RTSP 流
    if (avformat_open_input(input_format_ctx, input_url, NULL, NULL) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not open input stream\n");
        return -1;
    }

    // 获取输入流信息
    if (avformat_find_stream_info(*input_format_ctx, NULL) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not find stream information\n");
        avformat_close_input(input_format_ctx);
        return -1;
    }

    // 查找视频流
    int video_stream_index = -1;
    for (unsigned int i = 0; i < (*input_format_ctx)->nb_streams; i++)
    {
        if ((*input_format_ctx)->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_stream_index = i;
            break;
        }
    }

    if (video_stream_index == -1)
    {
        av_log(NULL, AV_LOG_ERROR, "No video stream found\n");
        avformat_close_input(input_format_ctx);
        return -1;
    }

    *input_stream = (*input_format_ctx)->streams[video_stream_index];

    // 查找解码器
    const AVCodec *input_codec = avcodec_find_decoder((*input_stream)->codecpar->codec_id);
    if (!input_codec)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not find decoder\n");
        avformat_close_input(input_format_ctx);
        return -1;
    }

    *input_codec_ctx = avcodec_alloc_context3(input_codec);
    if (!*input_codec_ctx)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not allocate codec context\n");
        avformat_close_input(input_format_ctx);
        return -1;
    }

    // 复制解码器参数
    if (avcodec_parameters_to_context(*input_codec_ctx, (*input_stream)->codecpar) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not copy codec parameters to context\n");
        avcodec_free_context(input_codec_ctx);
        avformat_close_input(input_format_ctx);
        return -1;
    }

    // 打开解码器
    if (avcodec_open2(*input_codec_ctx, input_codec, NULL) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not open codec\n");
        avcodec_free_context(input_codec_ctx);
        avformat_close_input(input_format_ctx);
        return -1;
    }

    // 创建输出格式上下文
    if (avformat_alloc_output_context2(output_format_ctx, NULL, "flv", output_url) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not create output context\n");
        avcodec_free_context(input_codec_ctx);
        avformat_close_input(input_format_ctx);
        return -1;
    }

    // 创建输出流
    *output_stream = avformat_new_stream(*output_format_ctx, NULL);
    if (!*output_stream)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not create output stream\n");
        avcodec_free_context(input_codec_ctx);
        avformat_close_input(input_format_ctx);
        avformat_free_context(*output_format_ctx);
        return -1;
    }

    // 复制输入流的编码参数到输出流
    if (avcodec_parameters_copy((*output_stream)->codecpar, (*input_stream)->codecpar) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not copy codec parameters\n");
        avcodec_free_context(input_codec_ctx);
        avformat_close_input(input_format_ctx);
        avformat_free_context(*output_format_ctx);
        return -1;
    }

    // 打开输出流
    if (avio_open(&(*output_format_ctx)->pb, output_url, AVIO_FLAG_WRITE) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not open output stream\n");
        avcodec_free_context(input_codec_ctx);
        avformat_close_input(input_format_ctx);
        avformat_free_context(*output_format_ctx);
        return -1;
    }

    // 写入头部信息
    if (avformat_write_header(*output_format_ctx, NULL) < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Could not write output header\n");
        avcodec_free_context(input_codec_ctx);
        avformat_close_input(input_format_ctx);
        avformat_free_context(*output_format_ctx);
        return -1;
    }

    return 0;
}

// 推流线程处理函数
void *push_rtmp_handler_thread(void *arg)
{
    ThreadArgs *args = (ThreadArgs *)arg;
    const char *input_url = "rtsp://192.168.10.5:554/av0_0";
    const char *output_url = "rtmp://192.168.10.7:1935/live/tlive001";

    AVFormatContext *input_format_ctx = NULL;
    AVFormatContext *output_format_ctx = NULL;
    AVCodecContext *input_codec_ctx = NULL;
    AVStream *input_stream = NULL;
    AVStream *output_stream = NULL;
    const AVCodec *encoder;
    AVCodecContext *encoder_ctx;
    AVPacket packet;
    packet.data = NULL;
    packet.size = 0;
    int ret = 0;

    // 调用初始化函数
    if (init_av_contexts(input_url, output_url, &input_format_ctx, &output_format_ctx,
                         &input_codec_ctx, &input_stream, &output_stream) < 0)
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "Error init_av_contexts: %s\n", errbuf);
        return NULL;
    }

    // 查找编码器
    encoder = avcodec_find_encoder(output_stream->codecpar->codec_id);
    if (!encoder)
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "Error avcodec_find_encoder: %s\n", errbuf);
        goto cleanup;
    }

    encoder_ctx = avcodec_alloc_context3(encoder);
    if (!encoder_ctx)
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "Error avcodec_alloc_context3: %s\n", errbuf);
        goto cleanup;
    }

    // 复制编码器参数
    ret = avcodec_parameters_to_context(encoder_ctx, output_stream->codecpar);
    if (ret < 0)
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "Error avcodec_parameters_to_context: %s\n", errbuf);
        avcodec_free_context(&encoder_ctx);
        goto cleanup;
    }

    // 打开编码器
    ret = avcodec_open2(encoder_ctx, encoder, NULL);
    if (ret < 0)
    {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        fprintf(stderr, "Error avcodec_open2: %s\n", errbuf);
        avcodec_free_context(&encoder_ctx);
        goto cleanup;
    }

    while (1)
    {
        QueueItem item;
        memset(&item, 0, sizeof(QueueItem));

        if (dequeue(args->origin_frame_queue, &item))
        {
            if (item.type == ONLY_FRAME && item.data != NULL)
            {
                AVFrame *newFrame = (AVFrame *)item.data;
                packet.data = NULL;
                packet.size = 0;
                if (avcodec_send_frame(encoder_ctx, newFrame) < 0)
                {
                    char errbuf[AV_ERROR_MAX_STRING_SIZE];
                    av_strerror(ret, errbuf, sizeof(errbuf));
                    fprintf(stderr, "Error avcodec_send_frame: %s\n", errbuf);
                    av_frame_free(&newFrame);
                    continue;
                }
                av_frame_free(&newFrame);
                //
                while (avcodec_receive_packet(encoder_ctx, &packet) == 0)
                {
                    packet.stream_index = output_stream->index;
                    packet.pts = av_rescale_q_rnd(packet.pts, input_stream->time_base, output_stream->time_base,
                                                  (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                    packet.dts = av_rescale_q_rnd(packet.dts, input_stream->time_base, output_stream->time_base,
                                                  (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                    packet.duration = av_rescale_q(packet.duration, input_stream->time_base, output_stream->time_base);
                    packet.pos = -1;
                    if (av_write_frame(output_format_ctx, &packet) < 0)
                    {
                        char errbuf[AV_ERROR_MAX_STRING_SIZE];
                        av_strerror(ret, errbuf, sizeof(errbuf));
                        fprintf(stderr, "Error av_write_frame: %s\n", errbuf);
                        break;
                    }
                    av_packet_unref(&packet);
                }
            }
        }
    }
    av_write_trailer(output_format_ctx);

cleanup:
    // 清理资源
    if (output_format_ctx && !(output_format_ctx->oformat->flags & AVFMT_NOFILE))
    {
        avio_close(output_format_ctx->pb);
    }
    if (encoder_ctx)
    {
        avcodec_free_context(&encoder_ctx);
    }
    avcodec_free_context(&input_codec_ctx);
    avformat_free_context(output_format_ctx);

    return NULL;
}
