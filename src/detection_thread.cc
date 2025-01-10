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
#include "detection_thread.h"
#include <stdio.h>
#include "thread_args.h"
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "opencv_dnn_module.h"
#include "opencv_utils.h"

void *frame_detection_thread(void *arg)
{
    const ThreadArgs *args = (ThreadArgs *)arg;
    const char *modelPath = "./yolov8n.onnx";
    cv::dnn::Net net;
    if (Init_CV_ONNX_DNN_Yolov8(modelPath, &net) != 0)
    {
        printf("Error: Failed to initialize the YOLOv8 ONNX DNN model.\n");
        pthread_exit(NULL);
        return NULL;
    }

    while (1)
    {
        if (args->ctx->is_cancelled)
        {
            goto END;
        }

        QueueItem detection_item;
        memset(&detection_item, 0, sizeof(QueueItem));
        if (dequeue(args->detection_queue, &detection_item))
        {
            if (detection_item.type == ONLY_FRAME)
            {
                AVFrame *detection_frame = (AVFrame *)detection_item.data;
                cv::Mat detection_mat = AVFrameToCVMat(detection_frame);
                if (!detection_mat.empty())
                {
                    std::vector<Box> outputs;
                    Infer_CV_ONNX_DNN_Yolov8(&net, detection_mat, outputs);
                    QueueItem boxes_item;
                    memset(&boxes_item, 0, sizeof(QueueItem));
                    boxes_item.box_count = (outputs.size() > 20) ? 20 : outputs.size();
                    boxes_item.type = ONLY_BOXES;
                    boxes_item.data = NULL;
                    for (int i = 0; i < boxes_item.box_count; ++i)
                    {
                        // 存储检测框信息
                        boxes_item.Boxes[i].x = outputs[i].x;
                        boxes_item.Boxes[i].y = outputs[i].y;
                        boxes_item.Boxes[i].w = outputs[i].w;
                        boxes_item.Boxes[i].h = outputs[i].h;
                        boxes_item.Boxes[i].prop = outputs[i].prop;
                        strcpy(boxes_item.Boxes[i].label, outputs[i].label);
                    }
                    enqueue(args->box_queue, boxes_item);
                }
                av_frame_free(&detection_frame);
            }
        }
    }

END:
    Release_CV_ONNX_DNN_Yolov8(&net);
    pthread_exit(NULL);
    return NULL;
}