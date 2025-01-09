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
// 函数用于复制AVFrame
AVFrame *CopyAVFrame(AVFrame *srcFrame)
{
    AVFrame *dstFrame = av_frame_alloc();
    if (!dstFrame)
    {
        fprintf(stderr, "Could not allocate frame\n");
        return NULL;
    }
    dstFrame->format = srcFrame->format;
    dstFrame->width = srcFrame->width;
    dstFrame->height = srcFrame->height;
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
    return dstFrame;
}

int CaptureImage(AVFrame *srcFrame, const char *file_path)
{
    if (!srcFrame || !file_path)
    {
        fprintf(stderr, "Invalid input to CaptureImage\n");
        return -1;
    }

    // Initialize variables
    const AVCodec *jpegCodec = NULL;
    AVCodecContext *codecCtx = NULL;
    AVFrame *rgbFrame = NULL;
    struct SwsContext *swsCtx = NULL;
    FILE *file = NULL;
    int ret = 0;
    int rgbBufferSize = 0;
    uint8_t *rgbBuffer;
    // Allocate packet for JPEG encoding
    AVPacket pkt;
    pkt.data = NULL;
    pkt.size = 0;

    // Find the JPEG codec
    jpegCodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!jpegCodec)
    {
        fprintf(stderr, "JPEG codec not found\n");
        return -1;
    }

    // Allocate codec context
    codecCtx = avcodec_alloc_context3(jpegCodec);
    if (!codecCtx)
    {
        fprintf(stderr, "Could not allocate codec context\n");
        return -1;
    }

    // Set codec parameters
    codecCtx->pix_fmt = AV_PIX_FMT_YUVJ420P;
    codecCtx->width = srcFrame->width;
    codecCtx->height = srcFrame->height;
    codecCtx->time_base = (AVRational){1, 25};

    // Open codec
    if ((ret = avcodec_open2(codecCtx, jpegCodec, NULL)) < 0)
    {
        // fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
        goto cleanup;
    }

    // Allocate frame for RGB conversion
    rgbFrame = av_frame_alloc();
    if (!rgbFrame)
    {
        fprintf(stderr, "Could not allocate RGB frame\n");
        ret = -1;
        goto cleanup;
    }

    // Set up RGB frame
    rgbBufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, srcFrame->width, srcFrame->height, 1);
    rgbBuffer = (uint8_t *)av_malloc(rgbBufferSize);
    if (!rgbBuffer)
    {
        fprintf(stderr, "Could not allocate RGB buffer\n");
        ret = -1;
        goto cleanup;
    }
    av_image_fill_arrays(rgbFrame->data, rgbFrame->linesize, rgbBuffer, AV_PIX_FMT_RGB24, srcFrame->width, srcFrame->height, 1);

    // Set up SwsContext for RGB conversion
    swsCtx = sws_getContext(srcFrame->width, srcFrame->height, (enum AVPixelFormat)srcFrame->format,
                            srcFrame->width, srcFrame->height, AV_PIX_FMT_RGB24,
                            SWS_BICUBIC, NULL, NULL, NULL);
    if (!swsCtx)
    {
        fprintf(stderr, "Could not create SwsContext\n");
        ret = -1;
        goto cleanup;
    }

    // Convert frame to RGB
    sws_scale(swsCtx, (const uint8_t *const *)srcFrame->data, srcFrame->linesize, 0, srcFrame->height,
              rgbFrame->data, rgbFrame->linesize);

    // Send frame to encoder
    if ((ret = avcodec_send_frame(codecCtx, rgbFrame)) < 0)
    {
        // fprintf(stderr, "Error sending frame to encoder: %s\n", av_err2str(ret));
        goto cleanup;
    }

    // Receive encoded packet
    ret = avcodec_receive_packet(codecCtx, &pkt);
    if (ret < 0)
    {
        // fprintf(stderr, "Error receiving packet from encoder: %s\n", av_err2str(ret));
        goto cleanup;
    }

    // Write packet to file
    file = fopen(file_path, "wb");
    if (!file)
    {
        fprintf(stderr, "Could not open file for writing: %s\n", file_path);
        ret = -1;
        goto cleanup;
    }
    fwrite(pkt.data, 1, pkt.size, file);

    // Success
    ret = 0;

cleanup:
    if (file)
        fclose(file);
    if (rgbFrame)
        av_frame_free(&rgbFrame);
    if (codecCtx)
        avcodec_free_context(&codecCtx);
    if (swsCtx)
        sws_freeContext(swsCtx);
    av_packet_unref(&pkt);

    return ret;
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
        // fprintf(stderr, "Could not allocate format context: %s\n", av_err2str(ret));
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
        // fprintf(stderr, "Could not open codec: %s\n", av_err2str(ret));
        goto cleanup;
    }

    // Copy codec parameters to the stream
    ret = avcodec_parameters_from_context(video_stream->codecpar, codec_ctx);
    if (ret < 0)
    {
        // fprintf(stderr, "Could not copy codec parameters: %s\n", av_err2str(ret));
        goto cleanup;
    }

    // Open output file
    if (!(format_ctx->oformat->flags & AVFMT_NOFILE))
    {
        if ((ret = avio_open(&format_ctx->pb, output_file, AVIO_FLAG_WRITE)) < 0)
        {
            // fprintf(stderr, "Could not open output file: %s\n", av_err2str(ret));
            goto cleanup;
        }
    }

    // Write the header
    if ((ret = avformat_write_header(format_ctx, NULL)) < 0)
    {
        // fprintf(stderr, "Error writing header: %s\n", av_err2str(ret));
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
            // fprintf(stderr, "Error sending frame to encoder: %s\n", av_err2str(ret));
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
                // fprintf(stderr, "Error receiving packet from encoder: %s\n", av_err2str(ret));
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
                // fprintf(stderr, "Error writing packet: %s\n", av_err2str(ret));
                goto cleanup;
            }
        }
    }

    // Write trailer
    if ((ret = av_write_trailer(format_ctx)) < 0)
    {
        // fprintf(stderr, "Error writing trailer: %s\n", av_err2str(ret));
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
