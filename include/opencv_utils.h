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
#ifndef OPENCV_UTILS_H
#define OPENCV_UTILS_H
#include <opencv2/opencv.hpp>
extern "C"
{
#include <libavutil/frame.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}
struct BestResult
{
    int bestId;
    float bestScore;
};
typedef struct
{
    int x;
    int y;
    int w;
    int h;
    float score;
    int class_id;
} DnnResult;
/// @brief 将AVFrame转换为cv::Mat
/// @param frame AVFrame指针
/// @return
cv::Mat AVFrameToCVMat(AVFrame *frame);
/// @brief 将cv::Mat转换为AVFrame
/// @param confidenceValues
/// @param size
/// @return
BestResult getBestFromConfidenceValue(float confidenceValues[], size_t size);

/// @brief postprocess
/// @param frame
/// @param outs
/// @param confThreshold
/// @param nmsThreshold
/// @return
std::vector<DnnResult> postprocess(cv::Mat &frame, const std::vector<cv::Mat> &outs, float confThreshold, float nmsThreshold);
// 函数：计算宽度和高度的缩放比例
// @param original_width 原始图像的宽度
// @param original_height 原始图像的高度
// @param scaled_width 缩放后图像的宽度
// @param scaled_height 缩放后图像的高度
// @return 宽度和高度的缩放比例
void calculate_scale_factors(float original_width, float original_height, float scaled_width,
                             float scaled_height, float *width_scale, float *height_scale);

// 函数：将缩放后的标记框尺寸还原为原始尺寸
// @param x 缩放后标记框的 x 坐标
// @param y 缩放后标记框的 y 坐标
// @param w 缩放后标记框的宽度
// @param h 缩放后标记框的高度
// @param width_scale 宽度的缩放比例
// @param height_scale 高度的缩放比例
// @return 原始尺寸下的 x, y, w, h
void rescale_box(float x, float y, float w, float h, float width_scale, float height_scale,
                 float *x_original, float *y_original, float *w_original, float *h_original);

// 实现 letterbox 功能的 C 风格函数
// @param src 输入的源图像指针
// @param dst 输出的 letterbox 处理后的图像指针
// @param new_width 目标宽度
// @param new_height 目标高度
// @param color 填充颜色，默认为黑色 (0, 0, 0)
void letterbox(const cv::Mat *src, cv::Mat *dst, int new_width, int new_height, cv::Scalar color);
// 将缩放后的矩形映射回原始图像的函数
// @param box 缩放后图像上的矩形（x, y, width, height）
// @param original_size 原始图像的尺寸
// @param letterboxed_size 经过 letterbox 处理后的图像尺寸
// @return 原始图像上的矩形
cv::Rect map_box_to_original(cv::Rect box, cv::Size original_size, cv::Size letterboxed_size);
#endif