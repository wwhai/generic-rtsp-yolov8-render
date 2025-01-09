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
#include "frame_queue.h"
// 初始化队列
void frame_queue_init(FrameQueue *q, int max_size)
{
    q->front = NULL;
    q->rear = NULL;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
    q->size = 0;
    q->max_size = max_size;
}
// 入队操作
// @param q 队列指针
// @param item 入队元素
// @return 1 成功，0 失败

int enqueue(FrameQueue *q, QueueItem item)
{
    pthread_mutex_lock(&q->lock);
    if (q->size >= q->max_size)
    {
        // 队列已满，移除队首元素
        QueueNode *temp = q->front;
        q->front = q->front->next;
        if (q->front == NULL)
        {
            q->rear = NULL;
        }
        q->size--;
        // 释放 AVFrame 资源（如果有的话）
        if (temp->item.data != NULL)
        {
            AVFrame *frame = (AVFrame *)temp->item.data;
            av_frame_free(&frame);
        }
        free(temp);
    }
    QueueNode *newNode = (QueueNode *)malloc(sizeof(QueueNode));
    if (newNode == NULL)
    {
        perror("malloc failed");
        pthread_mutex_unlock(&q->lock);
        exit(EXIT_FAILURE);
    }
    newNode->item = item;
    newNode->next = NULL;
    if (q->rear == NULL)
    {
        q->front = q->rear = newNode;
    }
    else
    {
        q->rear->next = newNode;
        q->rear = newNode;
    }
    q->size++; // 增加元素数量
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->lock);
    return 1;
}

// 出队操作
// @param q 队列指针
// @param item 出队元素
// @return 1 成功，0 失败
int dequeue(FrameQueue *q, QueueItem *item)
{
    pthread_mutex_lock(&q->lock);
    while (q->front == NULL)
    {
        pthread_cond_wait(&q->cond, &q->lock);
    }
    QueueNode *temp = q->front;
    *item = temp->item;
    q->front = q->front->next;
    if (q->front == NULL)
    {
        q->rear = NULL;
    }
    q->size--; // 减少元素数量
    pthread_mutex_unlock(&q->lock);
    free(temp);
    return 1;
}

// 释放队列资源的函数
void frame_queue_destroy(FrameQueue *q)
{
    pthread_mutex_lock(&q->lock);
    QueueNode *current = q->front;
    QueueNode *next;
    while (current != NULL)
    {
        // 释放 AVFrame 资源（如果有的话）
        if (current->item.data != NULL)
        {
            AVFrame *frame = (AVFrame *)current->item.data;
            av_frame_free(&frame);
        }
        next = current->next;
        free(current);
        current = next;
    }
    pthread_mutex_unlock(&q->lock);
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->cond);
    q->front = NULL;
    q->rear = NULL;
    q->size = 0;
}
