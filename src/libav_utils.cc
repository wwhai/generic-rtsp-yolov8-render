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
#include <libavutil/imgutils.h>
#include "logger.h"
// 保存AVFrame图像到文件，格式为png
const char *get_av_error(int errnum)
{
    static char str[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, str, sizeof(str));
    return str;
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

// 保存 AVFrame 为 BMP 文件
void save_frame_as_bmp(AVFrame *frame, const char *filename)
{
    if (!frame || !filename)
    {
        log_info( "Invalid input parameters");
        return;
    }

    int width = frame->width;
    int height = frame->height;
    if (width <= 0 || height <= 0)
    {
        log_info( "Invalid frame dimensions: %d*%d", width, height);
        return;
    }
    AVPixelFormat input_format = (AVPixelFormat)frame->format;
    AVPixelFormat output_format = AV_PIX_FMT_YUV420P;

    // 检查像素格式是否支持
    const AVPixFmtDescriptor *input_desc = av_pix_fmt_desc_get(input_format);
    const AVPixFmtDescriptor *output_desc = av_pix_fmt_desc_get(output_format);
    if (!input_desc || !output_desc)
    {
        log_info( "Unsupported pixel format");
        return;
    }

    // 打开文件
    FILE *file = fopen(filename, "wb");
    if (!file)
    {
        log_info( "Failed to open file: %s", filename);
        return;
    }

    // BMP 文件头
    unsigned char bmp_file_header[14] = {
        'B', 'M',   // 文件类型
        0, 0, 0, 0, // 文件大小
        0, 0,       // 保留
        0, 0,       // 保留
        54          // 数据偏移量
    };

    // BMP 信息头
    unsigned char bmp_info_header[40] = {
        40, 0, 0, 0, // 信息头大小
        0, 0, 0, 0,  // 图像宽度
        0, 0, 0, 0,  // 图像高度
        1, 0,        // 平面数
        24, 0,       // 位深度
        0, 0, 0, 0,  // 压缩方式
        0, 0, 0, 0,  // 图像数据大小
        0, 0, 0, 0,  // 水平分辨率
        0, 0, 0, 0,  // 垂直分辨率
        0, 0, 0, 0,  // 调色板颜色数
        0, 0, 0, 0   // 重要颜色数
    };

    int row_size = (width * 3 + 3) & ~3; // 每行字节数按 4 字节对齐
    int file_size = 54 + row_size * height;

    // 设置文件大小
    bmp_file_header[2] = (unsigned char)(file_size);
    bmp_file_header[3] = (unsigned char)(file_size >> 8);
    bmp_file_header[4] = (unsigned char)(file_size >> 16);
    bmp_file_header[5] = (unsigned char)(file_size >> 24);

    // 设置图像宽度和高度
    bmp_info_header[4] = (unsigned char)(width);
    bmp_info_header[5] = (unsigned char)(width >> 8);
    bmp_info_header[6] = (unsigned char)(width >> 16);
    bmp_info_header[7] = (unsigned char)(width >> 24);
    bmp_info_header[8] = (unsigned char)(height);
    bmp_info_header[9] = (unsigned char)(height >> 8);
    bmp_info_header[10] = (unsigned char)(height >> 16);
    bmp_info_header[11] = (unsigned char)(height >> 24);

    // 写入文件头和信息头
    fwrite(bmp_file_header, 1, 14, file);
    fwrite(bmp_info_header, 1, 40, file);

    // 创建图像转换上下文
    struct SwsContext *sws_ctx = sws_getContext(width, height, input_format,
                                                width, height, output_format,
                                                SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx)
    {
        log_info( "Failed to create SwsContext");
        fclose(file);
        return;
    }

    // 分配输出帧
    AVFrame *bgr_frame = av_frame_alloc();
    if (!bgr_frame)
    {
        log_info( "Failed to allocate AVFrame");
        sws_freeContext(sws_ctx);
        fclose(file);
        return;
    }

    // 分配输出缓冲区
    int num_bytes = av_image_get_buffer_size(output_format, width, height, 1);
    uint8_t *buffer = (uint8_t *)av_malloc(num_bytes * sizeof(uint8_t));
    if (!buffer)
    {
        log_info( "Failed to allocate buffer");
        av_frame_free(&bgr_frame);
        sws_freeContext(sws_ctx);
        fclose(file);
        return;
    }

    // 填充输出帧数据
    av_image_fill_arrays(bgr_frame->data, bgr_frame->linesize, buffer, output_format, width, height, 1);

    // 进行图像转换
    sws_scale(sws_ctx, (const uint8_t *const *)frame->data, frame->linesize, 0, height,
              bgr_frame->data, bgr_frame->linesize);

    // 写入图像数据（BMP 图像是从下到上存储的）
    for (int y = height - 1; y >= 0; y--)
    {
        fwrite(bgr_frame->data[0] + y * bgr_frame->linesize[0], 1, row_size, file);
    }

    // 释放资源
    av_free(buffer);
    av_frame_free(&bgr_frame);
    sws_freeContext(sws_ctx);
    fclose(file);
}