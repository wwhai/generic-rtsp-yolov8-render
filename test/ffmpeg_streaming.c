#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>

#define RTSP_URL "rtsp://192.168.10.6:554/av0_0"
#define RTMP_URL "rtmp://192.168.10.9:1935/live/tlive001"
#define QUEUE_SIZE 10
#define WIDTH 1920
#define HEIGHT 1080
#define FPS 25
#define PIX_FMT AV_PIX_FMT_YUV420P

// 全局编码和格式上下文
AVFormatContext *ofmt_ctx = NULL;
AVCodecContext *enc_ctx = NULL;
AVCodec *encoder = NULL;

// 简单的队列实现
typedef struct
{
    AVFrame *frames[QUEUE_SIZE];
    int front, rear, size;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} FrameQueue;

FrameQueue frame_queue = {.front = 0, .rear = 0, .size = 0, .lock = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};

void enqueue(AVFrame *frame)
{
    pthread_mutex_lock(&frame_queue.lock);
    while (frame_queue.size >= QUEUE_SIZE)
        pthread_cond_wait(&frame_queue.cond, &frame_queue.lock);

    frame_queue.frames[frame_queue.rear] = frame;
    frame_queue.rear = (frame_queue.rear + 1) % QUEUE_SIZE;
    frame_queue.size++;
    pthread_cond_signal(&frame_queue.cond);
    pthread_mutex_unlock(&frame_queue.lock);
}

AVFrame *dequeue()
{
    pthread_mutex_lock(&frame_queue.lock);
    while (frame_queue.size == 0)
        pthread_cond_wait(&frame_queue.cond, &frame_queue.lock);

    AVFrame *frame = frame_queue.frames[frame_queue.front];
    frame_queue.front = (frame_queue.front + 1) % QUEUE_SIZE;
    frame_queue.size--;
    pthread_cond_signal(&frame_queue.cond);
    pthread_mutex_unlock(&frame_queue.lock);
    return frame;
}

void draw_text(AVFrame *frame)
{
    int x = 50, y = 50;
    uint8_t *data = frame->data[0];
    int linesize = frame->linesize[0];
    for (int i = 0; i < 20; i++)
    {
        for (int j = 0; j < 100; j++)
        {
            data[(y + i) * linesize + (x + j)] = 255;
        }
    }
}

void *pull_stream(void *arg)
{
    AVFormatContext *ifmt_ctx = NULL;
    avformat_open_input(&ifmt_ctx, RTSP_URL, NULL, NULL);
    avformat_find_stream_info(ifmt_ctx, NULL);
    int video_stream_index = -1;
    for (int i = 0; i < ifmt_ctx->nb_streams; i++)
    {
        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_stream_index = i;
            break;
        }
    }

    AVCodec *decoder = avcodec_find_decoder(ifmt_ctx->streams[video_stream_index]->codecpar->codec_id);
    AVCodecContext *dec_ctx = avcodec_alloc_context3(decoder);
    avcodec_parameters_to_context(dec_ctx, ifmt_ctx->streams[video_stream_index]->codecpar);
    avcodec_open2(dec_ctx, decoder, NULL);

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    while (av_read_frame(ifmt_ctx, packet) >= 0)
    {
        if (packet->stream_index == video_stream_index)
        {
            avcodec_send_packet(dec_ctx, packet);
            if (avcodec_receive_frame(dec_ctx, frame) == 0)
            {
                AVFrame *cloned_frame = av_frame_alloc();
                av_frame_ref(cloned_frame, frame);
                enqueue(cloned_frame);
            }
        }
        av_packet_unref(packet);
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
    avcodec_free_context(&dec_ctx);
    avformat_close_input(&ifmt_ctx);
    return NULL;
}

void *push_stream(void *arg)
{
    encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!encoder)
    {
        printf("无法找到编码器\n");
        return NULL;
    }

    enc_ctx = avcodec_alloc_context3(encoder);
    enc_ctx->width = WIDTH;
    enc_ctx->height = HEIGHT;
    enc_ctx->pix_fmt = PIX_FMT;
    enc_ctx->time_base = (AVRational){1, FPS};
    enc_ctx->framerate = (AVRational){FPS, 1};
    avcodec_open2(enc_ctx, encoder, NULL);

    avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", RTMP_URL);
    if (!ofmt_ctx)
    {
        printf("无法创建输出上下文\n");
        return NULL;
    }

    while (1)
    {
        AVFrame *frame = dequeue();
        draw_text(frame);

        AVPacket *pkt = av_packet_alloc();
        avcodec_send_frame(enc_ctx, frame);
        if (avcodec_receive_packet(enc_ctx, pkt) == 0)
        {
            pkt->stream_index = 0;
            av_interleaved_write_frame(ofmt_ctx, pkt);
            av_packet_unref(pkt);
        }
        av_frame_free(&frame);
        av_packet_free(&pkt);
    }

    avcodec_free_context(&enc_ctx);
    avformat_free_context(ofmt_ctx);
    return NULL;
}

int main()
{
    pthread_t thread_pull, thread_push;
    pthread_create(&thread_pull, NULL, pull_stream, NULL);
    pthread_create(&thread_push, NULL, push_stream, NULL);
    pthread_join(thread_pull, NULL);
    pthread_join(thread_push, NULL);
    return 0;
}
