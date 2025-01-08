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

// 初始化队列
void frame_queue_init(FrameQueue *q, int max_size);

// 入队操作
int enqueue(FrameQueue *q, QueueItem item);

// 出队操作
int dequeue(FrameQueue *q, QueueItem *item);

// 释放队列资源的函数
void frame_queue_destroy(FrameQueue *q);

#endif // FRAME_QUEUE_H
