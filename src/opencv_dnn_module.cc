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
#include "opencv_dnn_module.h"
#include "opencv_utils.h"
#include <iostream>
#include "logger.h"
#include "coco_class.h"
// 初始化YOLOv8模型
int Init_CV_ONNX_DNN_Yolov8(const char *model_path, cv::dnn::Net *net)
{
    init_coco_names();
    print_coco_names();
    try
    {
        *net = cv::dnn::readNetFromONNX(model_path);
        if (net->empty())
        {
            std::cerr << "Error: Failed to load YOLOv8 model from " << model_path << std::endl;
            return -1;
        }

        net->setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        net->setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

        return 0; // 成功
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception during model initialization: " << e.what() << std::endl;
        return -1;
    }
}

// 执行YOLOv8推理
int Infer_CV_ONNX_DNN_Yolov8(cv::dnn::Net *net, cv::Mat frame, std::vector<Box> &boxes)
{
    if (!net)
    {
        log_info( "Error: Net pointer is null.");
        return -1;
    }
    // 准备输入; YOLOV8图片尺寸需要压缩为640*640
    cv::Mat blob;
    cv::Mat letterboxed_frame;
    letterbox(&frame, &letterboxed_frame, 640, 640, cv::Scalar(0, 0, 0));
    cv::dnn::blobFromImage(letterboxed_frame, blob, 1.0 / 255.0, cv::Size(640, 640), cv::Scalar(), true, false);
    net->setInput(blob);
    // 推理
    std::vector<cv::Mat> outs;
    std::string outputName = "output0";
    int layerId = net->getLayerId("output0");
    net->registerOutput(outputName, layerId, 0);
    net->forward(outs, outputName);
    // 检查推理结果
    if (outs.empty())
    {
        log_info( "Error: No output from the network.");
        return -1;
    }
    std::vector<DnnResult> results = postprocess(frame, outs, 0.25, 0.5);
    for (auto &&result : results)
    {
        const char *coco_name = get_coco_name(result.class_id);

        cv::Rect box_in_letterbox(result.x, result.y, result.w, result.h);
        cv::Rect box_in_original = map_box_to_original(box_in_letterbox, frame.size(), letterboxed_frame.size());
        Box box = {
            .x = box_in_original.x,
            .y = box_in_original.y,
            .w = box_in_original.width,
            .h = box_in_original.height,
            .prop = result.score,
        };

        strcpy(box.label, coco_name);
        boxes.push_back(box);
    }
    return 0;
}

// 释放模型资源
int Release_CV_ONNX_DNN_Yolov8(cv::dnn::Net *net)
{
    log_info( "Releasing YOLOv8 model...");
    return 0;
}
