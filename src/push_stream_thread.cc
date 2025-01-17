#include "push_stream_thread.h"
// 初始化推流上下文
int init_rtmp_stream(RtmpStreamContext *ctx, const char *output_url, int width, int height, int fps)
{
    int ret = 0;

    // 创建输出上下文
    ret = avformat_alloc_output_context2(&ctx->output_ctx, NULL, "flv", output_url);
    if (!ctx->output_ctx)
    {
        fprintf(stderr, "Could not create output context: %s\n", get_av_error_string(ret));
        return -1;
    }

    // 查找编码器

    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        fprintf(stderr, "Codec not found\n");
        return -1;
    }

    // 创建新的视频流
    ctx->video_stream = avformat_new_stream(ctx->output_ctx, NULL);
    if (!ctx->video_stream)
    {
        fprintf(stderr, "Could not create video stream\n");
        return -1;
    }

    // 初始化编码器上下文
    ctx->codec_ctx = avcodec_alloc_context3(codec);
    if (!ctx->codec_ctx)
    {
        fprintf(stderr, "Could not allocate codec context\n");
        return -1;
    }

    ctx->codec_ctx->codec_id = codec->id;
    ctx->codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    ctx->codec_ctx->width = width;
    ctx->codec_ctx->height = height;
    ctx->codec_ctx->time_base = (AVRational){1, fps};
    ctx->codec_ctx->framerate = (AVRational){fps, 1};
    ctx->codec_ctx->gop_size = 10; // 每组图像的大小
    ctx->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    if (ctx->output_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        ctx->codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // 打开编码器
    ret = avcodec_open2(ctx->codec_ctx, codec, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "Could not open codec: %s\n", get_av_error_string(ret));
        return -1;
    }

    // 将编码器参数复制到视频流
    ret = avcodec_parameters_from_context(ctx->video_stream->codecpar, ctx->codec_ctx);
    if (ret < 0)
    {
        fprintf(stderr, "Could not copy codec parameters: %s\n", get_av_error_string(ret));
        return -1;
    }

    // 打开输出文件
    if (!(ctx->output_ctx->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&ctx->output_ctx->pb, output_url, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            fprintf(stderr, "Could not open output URL: %s\n", get_av_error_string(ret));
            return -1;
        }
    }

    // 写入流头信息
    ret = avformat_write_header(ctx->output_ctx, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "Error occurred when writing header: %s\n", get_av_error_string(ret));
        return -1;
    }

    return 0;
}

void push_stream(RtmpStreamContext *ctx, AVFrame *frame)
{
    int ret;

    // 发送帧到编码器
    ret = avcodec_send_frame(ctx->codec_ctx, frame);
    if (ret < 0)
    {
        fprintf(stderr, "Error sending frame: %s\n", get_av_error_string(ret));
        return;
    }

    // 获取编码后的数据包
    AVPacket pkt = {0};
    while (ret >= 0)
    {
        ret = avcodec_receive_packet(ctx->codec_ctx, &pkt);
        if (ret < 0)
        {
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                break;
            }
            fprintf(stderr, "Error encoding frame: %s\n", get_av_error_string(ret));
            break;
        }

        // 设置数据包流索引
        pkt.stream_index = ctx->video_stream->index;
        av_packet_rescale_ts(&pkt, ctx->codec_ctx->time_base, ctx->video_stream->time_base);

        // 写入数据包到输出流
        ret = av_interleaved_write_frame(ctx->output_ctx, &pkt);
        if (ret < 0)
        {
            fprintf(stderr, "Error writing frame: %s\n", get_av_error_string(ret));
            break;
        }

        av_packet_unref(&pkt);
    }
}

// 推流线程处理函数
void *push_rtmp_handler_thread(void *arg)
{
    ThreadArgs *args = (ThreadArgs *)arg;
    const char *output_url = "rtmp://192.168.10.6:1935/live/tlive001";

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
