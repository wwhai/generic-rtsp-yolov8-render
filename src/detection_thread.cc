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

void *frame_detection_thread(void *arg)
{
    const ThreadArgs *args = (ThreadArgs *)arg;

    srand(time(NULL)); // 用于生成随机数的种子

    while (1)
    {
        if (args->ctx->is_cancelled)
        {
            goto END;
        }

        // 每隔 100 毫秒插入一些矩形框
        usleep(100000); // 100 毫秒

        // 随机生成矩形框
        QueueItem item;
        memset(&item, 0, sizeof(QueueItem));

        // 设置框的数量，可以根据需要调整
        item.box_count = rand() % 5 + 1; // 随机生成 1 到 5 个矩形框

        for (int i = 0; i < item.box_count; ++i)
        {
            Box *box = &item.Boxes[i];

            // 随机生成矩形框的坐标和大小，范围在 1920x1080 分辨率内
            box->x = rand() % 1920;                     // 随机横坐标
            box->y = rand() % 1080;                     // 随机纵坐标
            box->w = rand() % (1920 - box->x);          // 随机宽度
            box->h = rand() % (1080 - box->y);          // 随机高度
            box->prop = (float)(rand() % 100) / 100.0f; // 随机比例

            // 随机生成标签
            sprintf(box->label, "Box%d", i + 1);
        }
        item.type = ONLY_BOXES;
        item.data = NULL;
        enqueue(args->box_queue, item);
        // printf(">>> enqueue(args->detection_queue, item)\n");
    }

END:
    pthread_exit(NULL);
    return NULL;
}
