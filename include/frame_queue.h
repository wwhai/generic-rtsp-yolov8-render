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

#ifndef FRAME_QUEUE_H
#define FRAME_QUEUE_H

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
extern "C"
{
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/frame.h>
}
typedef enum
{
    ONLY_FRAME,
    ONLY_BOXES,
    FRAME_WITH_BOXES,
} ItemType;

// 定义结构体
typedef struct Box
{
    int x, y, w, h;
    float prop;
    char label[40];
} Box;

typedef struct QueueItem
{
    ItemType type;
    void *data;
    int box_count;
    Box Boxes[20];
} QueueItem;
// 释放队列节点
void free_queue_node(QueueItem *item);
// 队列节点结构体
typedef struct QueueNode
{
    QueueItem item;
    struct QueueNode *next;
} QueueNode;

typedef struct FrameQueue
{
    QueueNode *front;
    QueueNode *rear;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int size;
    int max_size;
} FrameQueue;

/// @brief 初始化队列
/// @param q 队列指针
/// @param max_size 队列最大容量
void frame_queue_init(FrameQueue *q, int max_size);

/// @brief  入队操作
/// @param q
/// @param item
/// @return
int enqueue(FrameQueue *q, QueueItem item);

// 出队操作
int dequeue(FrameQueue *q, QueueItem *item);
// 出队操作
// @param q 队列指针
// @param item 出队元素
// @return 1 成功，-1 队列为空，0 失败
int async_dequeue(FrameQueue *q, QueueItem *item);
/// 销毁队列
/// @param q 队列指针
/// @return 0 成功，-1 失败
void frame_queue_destroy(FrameQueue *q);

#endif // FRAME_QUEUE_H
