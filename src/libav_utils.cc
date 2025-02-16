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
#include <stdio.h>
#include <stdlib.h>
#include <png.h>
#include <libavutil/imgutils.h>
// 保存AVFrame图像到文件，格式为png
const char *get_av_error(int errnum)
{
    static char str[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, str, sizeof(str));
    return str;
}

int capture_image(AVFrame *frame, const char *filename)
{
    if (!frame || !filename)
    {
        fprintf(stderr, "Invalid input: frame or filename is NULL\n");
        return -1;
    }

    int width = frame->width;
    int height = frame->height;

    // 确保图像尺寸有效
    if (width <= 0 || height <= 0)
    {
        fprintf(stderr, "Invalid frame dimensions: %dx%d\n", width, height);
        return -1;
    }

    // 创建一个新的 AVFrame 用于存储 RGB 图像
    AVFrame *rgbFrame = av_frame_alloc();
    if (!rgbFrame)
    {
        fprintf(stderr, "Failed to allocate memory for RGB frame\n");
        return -1;
    }

    // 设置目标图像的格式为 RGB24
    rgbFrame->format = AV_PIX_FMT_RGB24;
    rgbFrame->width = width;
    rgbFrame->height = height;

    // 分配图像数据内存
    if (av_image_alloc(rgbFrame->data, rgbFrame->linesize, width, height, AV_PIX_FMT_RGB24, 1) < 0)
    {
        fprintf(stderr, "Failed to allocate memory for RGB image\n");
        av_frame_free(&rgbFrame);
        return -1;
    }

    // 创建转换上下文
    struct SwsContext *sws_ctx = sws_getContext(
        width, height, AV_PIX_FMT_YUV420P, // 输入格式为 YUV420P
        width, height, AV_PIX_FMT_RGB24,   // 输出格式为 RGB24
        SWS_BILINEAR, NULL, NULL, NULL     // 使用双线性插值进行转换
    );
    if (!sws_ctx)
    {
        fprintf(stderr, "Failed to create sws context\n");
        av_frame_free(&rgbFrame);
        return -1;
    }

    // 将 YUV420 图像转换为 RGB
    sws_scale(sws_ctx, (const uint8_t *const *)frame->data, frame->linesize, 0, height, rgbFrame->data, rgbFrame->linesize);

    // 释放转换上下文
    sws_freeContext(sws_ctx);

    // 打开 PNG 文件进行写入
    FILE *png_file = fopen(filename, "wb");
    if (!png_file)
    {
        perror("Failed to open file for writing");
        av_frame_free(&rgbFrame);
        return -1;
    }

    // 创建 libpng 写入结构
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
    {
        fprintf(stderr, "Failed to create PNG write struct\n");
        fclose(png_file);
        av_frame_free(&rgbFrame);
        return -1;
    }

    png_infop info = png_create_info_struct(png);
    if (!info)
    {
        fprintf(stderr, "Failed to create PNG info struct\n");
        png_destroy_write_struct(&png, NULL);
        fclose(png_file);
        av_frame_free(&rgbFrame);
        return -1;
    }

    // 设置错误处理
    if (setjmp(png_jmpbuf(png)))
    {
        png_destroy_write_struct(&png, &info);
        fclose(png_file);
        av_frame_free(&rgbFrame);
        return -1;
    }

    // 初始化 PNG 写入
    png_init_io(png, png_file);

    // 设置 PNG 图像头部信息
    png_set_IHDR(
        png, info, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_NONE);
    png_write_info(png, info);

    // 将每一行 RGB 数据写入 PNG 文件
    for (int y = 0; y < height; y++)
    {
        png_bytep row = (png_bytep)(rgbFrame->data[0] + y * rgbFrame->linesize[0]);
        png_write_row(png, row);
    }

    // 结束 PNG 写入
    png_write_end(png, info);

    // 清理
    png_destroy_write_struct(&png, &info);
    fclose(png_file);
    av_frame_free(&rgbFrame);

    return 0;
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
//
void copy_codec_context_properties(AVCodecContext *src_ctx, AVCodecContext *dst_ctx)
{
    AVCodecParameters *params = avcodec_parameters_alloc();
    if (!params)
    {
        // 处理内存分配失败的情况
        return;
    }
    // 将源编解码器上下文的参数复制到 AVCodecParameters
    int ret = avcodec_parameters_from_context(params, src_ctx);
    if (ret < 0)
    {
        // 处理复制失败的情况
        avcodec_parameters_free(&params);
        return;
    }
    // 将 AVCodecParameters 的参数复制到目标编解码器上下文
    ret = avcodec_parameters_to_context(dst_ctx, params);
    if (ret < 0)
    {
        // 处理复制失败的情况
    }
    // 释放 AVCodecParameters
    avcodec_parameters_free(&params);
}