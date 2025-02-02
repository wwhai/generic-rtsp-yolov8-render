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

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>

int main()
{
    AVFormatContext *input_format_ctx = NULL, *output_format_ctx = NULL;
    AVCodecContext *decoder_ctx = NULL, *encoder_ctx = NULL;
    AVStream *video_stream = NULL, *out_stream = NULL;
    AVPacket packet;
    AVFrame *frame = NULL;
    int ret;

    // 打开输入RTSP
    ret = avformat_open_input(&input_format_ctx, "rtsp://192.168.10.4:554/av0_0", NULL, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "无法打开输入流\n");
        goto end;
    }

    // 查找流信息
    ret = avformat_find_stream_info(input_format_ctx, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "找不到流信息\n");
        goto end;
    }

    // 查找视频流
    int video_stream_index = -1;
    for (int i = 0; i < input_format_ctx->nb_streams; i++)
    {
        if (input_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video_stream_index = i;
            break;
        }
    }
    if (video_stream_index == -1)
    {
        fprintf(stderr, "找不到视频流\n");
        goto end;
    }

    // 获取解码器
    const AVCodec *decoder = avcodec_find_decoder(input_format_ctx->streams[video_stream_index]->codecpar->codec_id);
    if (!decoder)
    {
        fprintf(stderr, "找不到解码器\n");
        goto end;
    }

    // 创建解码器上下文
    decoder_ctx = avcodec_alloc_context3(decoder);
    if (!decoder_ctx)
    {
        fprintf(stderr, "无法分配解码器上下文\n");
        goto end;
    }
    avcodec_parameters_to_context(decoder_ctx, input_format_ctx->streams[video_stream_index]->codecpar);

    // 打开解码器
    ret = avcodec_open2(decoder_ctx, decoder, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "无法打开解码器\n");
        goto end;
    }

    // 打开输出RTMP
    ret = avformat_alloc_output_context2(&output_format_ctx, NULL, "flv", "rtmp://192.168.10.5:1935/live/tlive001");
    if (ret < 0)
    {
        fprintf(stderr, "无法创建输出上下文\n");
        goto end;
    }

    // 创建输出流
    out_stream = avformat_new_stream(output_format_ctx, NULL);
    if (!out_stream)
    {
        fprintf(stderr, "无法创建输出流\n");
        goto end;
    }

    // 获取编码器
    const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_FLV1);
    if (!encoder)
    {
        fprintf(stderr, "找不到编码器\n");
        goto end;
    }

    // 创建编码器上下文
    encoder_ctx = avcodec_alloc_context3(encoder);
    if (!encoder_ctx)
    {
        fprintf(stderr, "无法分配编码器上下文\n");
        goto end;
    }

    // 设置编码器参数
    encoder_ctx->width = 1920;
    encoder_ctx->height = 1080;
    encoder_ctx->bit_rate = 400000;
    encoder_ctx->time_base = (AVRational){1, 25};
    encoder_ctx->framerate = (AVRational){25, 1};
    encoder_ctx->gop_size = 10;
    encoder_ctx->max_b_frames = 1;
    encoder_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    // 打开编码器
    ret = avcodec_open2(encoder_ctx, encoder, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "无法打开编码器\n");
        goto end;
    }

    // 复制编码器参数到输出流
    ret = avcodec_parameters_from_context(out_stream->codecpar, encoder_ctx);
    if (ret < 0)
    {
        fprintf(stderr, "无法复制编码器参数到输出流\n");
        goto end;
    }
    // 打开输出URL并写入头信息
    ret = avio_open(&output_format_ctx->pb, output_format_ctx->url, AVIO_FLAG_WRITE);
    if (ret < 0)
    {
        fprintf(stderr, "无法打开输出URL '%s'\n", output_format_ctx->url);
        goto end;
    }

    // 写入头信息
    ret = avformat_write_header(output_format_ctx, NULL);
    if (ret < 0)
    {
        fprintf(stderr, "无法写入头信息\n");
        goto end;
    }

    // 分配帧
    frame = av_frame_alloc();
    if (!frame)
    {
        fprintf(stderr, "无法分配帧\n");
        goto end;
    }

    // 读取输入流
    while (av_read_frame(input_format_ctx, &packet) >= 0)
    {
        if (packet.stream_index == video_stream_index)
        {
            // 解码
            ret = avcodec_send_packet(decoder_ctx, &packet);
            if (ret < 0)
            {
                fprintf(stderr, "解码发送数据包失败\n");
                goto end;
            }

            while (ret >= 0)
            {
                ret = avcodec_receive_frame(decoder_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                {
                    break;
                }
                else if (ret < 0)
                {
                    fprintf(stderr, "解码接收帧失败\n");
                    goto end;
                }

                // 编码
                ret = avcodec_send_frame(encoder_ctx, frame);
                if (ret < 0)
                {
                    fprintf(stderr, "编码发送帧失败\n");
                    goto end;
                }

                while (ret >= 0)
                {
                    ret = avcodec_receive_packet(encoder_ctx, &packet);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    {
                        break;
                    }
                    else if (ret < 0)
                    {
                        fprintf(stderr, "编码接收数据包失败\n");
                        goto end;
                    }

                    // 将数据包时间戳转换为输出流的时间基准
                    packet.stream_index = out_stream->index;
                    av_packet_rescale_ts(&packet, encoder_ctx->time_base, out_stream->time_base);

                    // 写入输出流
                    ret = av_interleaved_write_frame(output_format_ctx, &packet);
                    if (ret < 0)
                    {
                        fprintf(stderr, "写入输出流失败\n");
                        goto end;
                    }
                }
            }
        }

        // 释放数据包
        av_packet_unref(&packet);
    }

    // 写入输出流的尾信息
    av_write_trailer(output_format_ctx);

end:
    // 释放资源
    avformat_close_input(&input_format_ctx);
    if (output_format_ctx && !(output_format_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&output_format_ctx->pb);
    avformat_free_context(output_format_ctx);
    avcodec_free_context(&decoder_ctx);
    avcodec_free_context(&encoder_ctx);
    av_frame_free(&frame);

    return ret;
}
