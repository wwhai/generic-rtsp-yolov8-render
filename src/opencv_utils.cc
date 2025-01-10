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

#include "opencv_utils.h"
// 将 AVFrame 转换为 OpenCV 的 cv::Mat

cv::Mat AVFrameToCVMat(AVFrame *frame)
{
    // 获取帧的格式、宽度和高度
    int width = frame->width;
    int height = frame->height;
    cv::Mat cvFrame;

    // 检查帧的格式是否为支持的格式
    AVPixelFormat pix_fmt = (AVPixelFormat)frame->format;
    if (pix_fmt == AV_PIX_FMT_YUV420P || pix_fmt == AV_PIX_FMT_YUVJ420P)
    {
        cv::Mat yuvFrame(height + height / 2, width, CV_8UC1);
        // 复制 YUV 平面
        av_image_copy_to_buffer(yuvFrame.data, yuvFrame.total() * yuvFrame.elemSize(),
                                (const uint8_t **)frame->data, frame->linesize,
                                pix_fmt, width, height, 1);
        cv::Mat rgbFrame(height, width, CV_8UC3);
        // 将 YUV 转换为 RGB
        cv::cvtColor(yuvFrame, rgbFrame, cv::COLOR_YUV2RGB_I420);
        cvFrame = rgbFrame;
    }
    else if (pix_fmt == AV_PIX_FMT_RGB24)
    {
        cvFrame = cv::Mat(height, width, CV_8UC3);
        // 复制 RGB 平面
        av_image_copy_to_buffer(cvFrame.data, cvFrame.total() * cvFrame.elemSize(),
                                (const uint8_t **)frame->data, frame->linesize,
                                pix_fmt, width, height, 1);
    }
    else
    {
        fprintf(stderr, "Unsupported pixel format: %s\n", av_get_pix_fmt_name(pix_fmt));
        return cv::Mat();
    }

    return cvFrame;
}

BestResult getBestFromConfidenceValue(float confidenceValues[], size_t size)
{
    BestResult result;
    result.bestId = -1; // 初始化为无效值
    result.bestScore = 0.0f;
    for (size_t i = 0; i < size; ++i)
    {
        if (confidenceValues[i] > result.bestScore)
        {
            result.bestId = static_cast<int>(i);
            result.bestScore = confidenceValues[i];
        }
    }
    return result;
}

std::vector<DnnResult> postprocess(cv::Mat &frame, const std::vector<cv::Mat> &outs, float confThreshold, float nmsThreshold)
{

    std::vector<int> classIds;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;
    // 网络输出的后处理
    int columns = 84;
    int rows = 8400;
    for (const auto &out : outs)
    {
        float *data_ptr = (float *)out.data;
        // std::cout << "out.rows: " << out.size << std::endl;
        for (int i = 0; i < rows; ++i)
        {
            auto x = (data_ptr[i + rows * 0]);
            auto y = (data_ptr[i + rows * 1]);
            auto w = (data_ptr[i + rows * 2]);
            auto h = (data_ptr[i + rows * 3]);
            float confidenceValues[80] = {};
            for (int j = 4; j < columns; ++j)
            {
                confidenceValues[j - 4] = data_ptr[i + rows * j];
            }
            BestResult result = getBestFromConfidenceValue(confidenceValues, 80);
            classIds.push_back(result.bestId);
            confidences.push_back(result.bestScore);
            boxes.push_back(cv::Rect(int(x - w / 2), int(y - h / 2), w, h));
        }
    }

    // 非极大值抑制
    std::vector<DnnResult> boxes_result;
    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, confidences, confThreshold, nmsThreshold, indices);
    for (int idx : indices)
    {
        cv::Rect box = boxes[idx];
        // printf("box: %d, %d, %d, %d, %f, %d\n", box.x, box.y, box.width, box.height, confidences[idx], classIds[idx]);
        boxes_result.push_back({box.x, box.y, box.width, box.height, confidences[idx], classIds[idx]});
    }
    return boxes_result;
}

void calculate_scale_factors(float original_width, float original_height, float scaled_width, float scaled_height, float *width_scale, float *height_scale)
{
    *width_scale = original_width / scaled_width;
    *height_scale = original_height / scaled_height;
}

void rescale_box(float x, float y, float w, float h, float width_scale, float height_scale, float *x_original, float *y_original, float *w_original, float *h_original)
{
    *x_original = x * width_scale;
    *y_original = y * height_scale;
    *w_original = w * width_scale;
    *h_original = h * height_scale;
}

void letterbox(const cv::Mat *src, cv::Mat *dst, int new_width, int new_height, cv::Scalar color)
{
    float scale = std::min((float)new_width / src->cols, (float)new_height / src->rows);
    int unpad_w = scale * src->cols;
    int unpad_h = scale * src->rows;
    cv::resize(*src, *dst, cv::Size(unpad_w, unpad_h));
    int pad_w = new_width - unpad_w;
    int pad_h = new_height - unpad_h;
    int top = pad_h / 2;
    int bottom = pad_h - top;
    int left = pad_w / 2;
    int right = pad_w - left;
    cv::copyMakeBorder(*dst, *dst, top, bottom, left, right, cv::BORDER_CONSTANT, color);
}

cv::Rect map_box_to_original(cv::Rect box, cv::Size original_size, cv::Size letterboxed_size)
{
    float scale = std::min((float)letterboxed_size.width / original_size.width, (float)letterboxed_size.height / original_size.height);
    int unpad_w = scale * original_size.width;
    int unpad_h = scale * original_size.height;
    int pad_w = letterboxed_size.width - unpad_w;
    int pad_h = letterboxed_size.height - unpad_h;
    int top = pad_h / 2;
    int left = pad_w / 2;
    cv::Rect mapped_box;
    mapped_box.x = (box.x - left) / scale;
    mapped_box.y = (box.y - top) / scale;
    mapped_box.width = box.width / scale;
    mapped_box.height = box.height / scale;
    return mapped_box;
}