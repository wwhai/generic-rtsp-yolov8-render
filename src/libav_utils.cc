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
#include "libav_utils.h"
#include "frame_queue.h"
#include "stdio.h"
const char *get_av_error(int errnum)
{
    static char str[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, str, sizeof(str));
    return str;
}
// 函数用于复制AVFrame
AVFrame *CopyAVFrame(AVFrame *srcFrame)
{
    AVFrame *dstFrame = av_frame_alloc();
    if (!dstFrame)
    {
        fprintf(stderr, "Could not allocate frame\n");
        return NULL;
    }

    int ret = av_frame_get_buffer(dstFrame, 32);
    if (ret < 0)
    {
        fprintf(stderr, "Could not allocate new frame buffer\n");
        av_frame_free(&dstFrame);
        return NULL;
    }
    ret = av_frame_copy(dstFrame, srcFrame);
    if (ret < 0)
    {
        fprintf(stderr, "Could not copy frame\n");
        av_frame_free(&dstFrame);
        return NULL;
    }
    if (av_frame_copy_props(dstFrame, srcFrame) < 0)
    {
        fprintf(stderr, "Could not copy frame properties\n");
        av_frame_free(&dstFrame);
        return NULL;
    }
    return dstFrame;
}
// 保存图像到文件,格式为png，不要用Libav的库，直接提取Avframe的Data
int CaptureImage(AVFrame *srcFrame, const char *file_path)
{
    if (!srcFrame || !file_path)
    {
        fprintf(stderr, "Invalid input parameters\n");
        return -1;
    }
    FILE *file = fopen(file_path, "wb");
    if (!file)
    {
        fprintf(stderr, "Could not open file: %s\n", file_path);
        return -1;
    }
    // 写入文件头
    fwrite(srcFrame->data[0], 1, srcFrame->linesize[0] * srcFrame->height, file);
    fclose(file);
    return 0;
}

/**
 * @brief Record an array of AVFrames to an MP4 file.
 *
 * @param output_file The output MP4 file path.
 * @param frames Array of AVFrame pointers to encode.
 * @param num_frames Number of frames in the array.
 * @param width Width of the video.
 * @param height Height of the video.
 * @param fps Frames per second of the video.
 * @return 0 on success, -1 on failure.
 */
int RecordAVFrameToMP4(const char *output_file, AVFrame *frames[], int num_frames, int width, int height, int fps)
{
    if (!output_file || !frames || num_frames <= 0 || width <= 0 || height <= 0 || fps <= 0)
    {
        fprintf(stderr, "Invalid input parameters\n");
        return -1;
    }

    AVFormatContext *format_ctx = NULL;
    const AVCodec *codec = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVStream *video_stream = NULL;
    AVPacket pkt;
    int ret = 0;

    // Allocate the format context
    ret = avformat_alloc_output_context2(&format_ctx, NULL, NULL, output_file);
    if (!format_ctx || ret < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, 64, ret);
        fprintf(stderr, "Could not allocate format context: %s\n", error);
        return -1;
    }

    // Find the H.264 codec
    codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec)
    {
        fprintf(stderr, "H.264 codec not found\n");
        ret = -1;
        goto cleanup;
    }

    // Add a video stream to the format context
    video_stream = avformat_new_stream(format_ctx, codec);
    if (!video_stream)
    {
        fprintf(stderr, "Could not create video stream\n");
        ret = -1;
        goto cleanup;
    }
    video_stream->time_base = (AVRational){1, fps};

    // Allocate codec context
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx)
    {
        fprintf(stderr, "Could not allocate codec context\n");
        ret = -1;
        goto cleanup;
    }

    // Set codec parameters
    codec_ctx->codec_id = codec->id;
    codec_ctx->bit_rate = 400000;
    codec_ctx->width = width;
    codec_ctx->height = height;
    codec_ctx->time_base = video_stream->time_base;
    codec_ctx->framerate = (AVRational){fps, 1};
    codec_ctx->gop_size = 12; // Group of pictures size
    codec_ctx->max_b_frames = 2;
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    // Set global headers for some formats
    if (format_ctx->oformat->flags & AVFMT_GLOBALHEADER)
    {
        codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // Open the codec
    if ((ret = avcodec_open2(codec_ctx, codec, NULL)) < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, 64, ret);
        fprintf(stderr, "Could not open codec: %s\n", error);
        goto cleanup;
    }

    // Copy codec parameters to the stream
    ret = avcodec_parameters_from_context(video_stream->codecpar, codec_ctx);
    if (ret < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, 64, ret);
        fprintf(stderr, "Could not copy codec parameters: %s\n", error);
        goto cleanup;
    }

    // Open output file
    if (!(format_ctx->oformat->flags & AVFMT_NOFILE))
    {
        if ((ret = avio_open(&format_ctx->pb, output_file, AVIO_FLAG_WRITE)) < 0)
        {
            char buffer[64] = {0};
            char *error = av_make_error_string(buffer, 64, ret);
            fprintf(stderr, "Could not open output file: %s\n", error);
            goto cleanup;
        }
    }

    // Write the header
    if ((ret = avformat_write_header(format_ctx, NULL)) < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, 64, ret);
        fprintf(stderr, "Error writing header: %s\n", error);
        goto cleanup;
    }

    // Encode and write frames
    for (int i = 0; i < num_frames; i++)
    {
        AVFrame *frame = frames[i];

        // Send frame to encoder
        ret = avcodec_send_frame(codec_ctx, frame);
        if (ret < 0)
        {
            char buffer[64] = {0};
            char *error = av_make_error_string(buffer, 64, ret);
            fprintf(stderr, "Error sending frame to encoder: %s\n", error);
            goto cleanup;
        }

        // Receive encoded packet
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
                fprintf(stderr, "Error receiving packet from encoder: %s\n", error);
                goto cleanup;
            }

            // Rescale timestamp to match stream time base
            av_packet_rescale_ts(&pkt, codec_ctx->time_base, video_stream->time_base);
            pkt.stream_index = video_stream->index;

            // Write packet to output file
            ret = av_interleaved_write_frame(format_ctx, &pkt);
            av_packet_unref(&pkt);
            if (ret < 0)
            {
                char buffer[64] = {0};
                char *error = av_make_error_string(buffer, 64, ret);
                fprintf(stderr, "Error writing packet: %s\n", error);
                goto cleanup;
            }
        }
    }

    // Write trailer
    if ((ret = av_write_trailer(format_ctx)) < 0)
    {
        char buffer[64] = {0};
        char *error = av_make_error_string(buffer, 64, ret);
        fprintf(stderr, "Error writing trailer: %s\n", error);
    }

cleanup:
    if (codec_ctx)
    {
        avcodec_free_context(&codec_ctx);
    }
    if (format_ctx)
    {
        if (!(format_ctx->oformat->flags & AVFMT_NOFILE))
        {
            avio_closep(&format_ctx->pb);
        }
        avformat_free_context(format_ctx);
    }

    return ret == 0 ? 0 : -1;
}
// 简单的线性插值
Box InterpolateBox(Box prevBox, Box currentBox, float t)
{
    Box result;
    result.x = (1 - t) * prevBox.x + t * currentBox.x;
    result.y = (1 - t) * prevBox.y + t * currentBox.y;
    result.w = (1 - t) * prevBox.w + t * currentBox.w;
    result.h = (1 - t) * prevBox.h + t * currentBox.h;
    result.prop = (1 - t) * prevBox.prop + t * currentBox.prop;
    strcpy(result.label, currentBox.label);
    return result;
}
