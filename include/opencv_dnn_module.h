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

#ifndef OPENCV_DNN_MODULE_H
#define OPENCV_DNN_MODULE_H

#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>
#include <vector>
#include "frame_queue.h"

// 初始化YOLOv8模型
int Init_CV_ONNX_DNN_Yolov8(const char *model_path, cv::dnn::Net *net);

// 执行YOLOv8推理
int Infer_CV_ONNX_DNN_Yolov8(cv::dnn::Net *net, cv::Mat frame, std::vector<Box> &boxes);

// 释放模型资源
int Release_CV_ONNX_DNN_Yolov8(cv::dnn::Net *net);

#endif // OPENCV_DNN_MODULE_H
