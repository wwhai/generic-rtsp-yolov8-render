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
#include <libavutil/log.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
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
    for (int i = 0; i < (*input_format_ctx)->nb_streams; i++)
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

int main()
{
    const char *input_url = "rtsp://192.168.10.5:554/av0_0";
    const char *output_url = "rtmp://192.168.10.7:1935/live/tlive001";

    AVFormatContext *input_format_ctx = NULL;
    AVFormatContext *output_format_ctx = NULL;
    AVCodecContext *input_codec_ctx = NULL;
    AVStream *input_stream = NULL, *output_stream = NULL;
    AVPacket packet;

    // 调用初始化函数
    if (init_av_contexts(input_url, output_url, &input_format_ctx, &output_format_ctx,
                         &input_codec_ctx, &input_stream, &output_stream) < 0)
    {
        return -1;
    }

    // 从输入流读取数据并写入输出流
    while (1)
    {
        if (av_read_frame(input_format_ctx, &packet) < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "Error reading frame\n");
            break;
        }

        if (packet.stream_index == input_stream->index)
        {
            // 转换 packet 的流索引
            packet.stream_index = output_stream->index;
            // 转换时间戳
            packet.pts = av_rescale_q_rnd(packet.pts, input_stream->time_base, output_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
            packet.dts = av_rescale_q_rnd(packet.dts, input_stream->time_base, output_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
            packet.duration = av_rescale_q(packet.duration, input_stream->time_base, output_stream->time_base);
            packet.pos = -1;
            if (av_write_frame(output_format_ctx, &packet) < 0)
            {
                av_log(NULL, AV_LOG_ERROR, "Error writing packet\n");
                break;
            }
        }

        av_packet_unref(&packet);
    }

    // 写入结尾数据
    av_write_trailer(output_format_ctx);

    // 清理资源
    avcodec_free_context(&input_codec_ctx);
    avformat_close_input(&input_format_ctx);
    avformat_free_context(output_format_ctx);

    return 0;
}