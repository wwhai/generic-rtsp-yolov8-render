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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "channel.h"

// 创建一个新的 channel
Channel *channel_create(int capacity)
{
    Channel *ch = (Channel *)malloc(sizeof(Channel));
    if (ch == NULL)
    {
        return NULL;
    }
    ch->queue = (void **)malloc(sizeof(void *) * capacity);
    if (ch->queue == NULL)
    {
        free(ch);
        return NULL;
    }
    ch->capacity = capacity;
    ch->head = 0;
    ch->tail = 0;
    ch->count = 0;
    ch->closed = 0;
    pthread_mutex_init(&ch->mutex, NULL);
    pthread_cond_init(&ch->not_full, NULL);
    pthread_cond_init(&ch->not_empty, NULL);
    return ch;
}

// 关闭 channel
void channel_close(Channel *ch)
{
    pthread_mutex_lock(&ch->mutex);
    if (!ch->closed)
    {
        ch->closed = 1;
        // 广播通知所有等待的线程
        pthread_cond_broadcast(&ch->not_full);
        pthread_cond_broadcast(&ch->not_empty);
    }
    pthread_mutex_unlock(&ch->mutex);
}

// 销毁 channel
void channel_destroy(Channel *ch)
{
    pthread_mutex_destroy(&ch->mutex);
    pthread_cond_destroy(&ch->not_full);
    pthread_cond_destroy(&ch->not_empty);
    free(ch->queue);
    free(ch);
}

// 阻塞式发送数据到 channel
int channel_send_blocking(Channel *ch, void *data)
{
    pthread_mutex_lock(&ch->mutex);
    // 如果 channel 已关闭，不能再发送数据
    if (ch->closed)
    {
        pthread_mutex_unlock(&ch->mutex);
        return -1;
    }
    // 等待队列有空间
    while (ch->count == ch->capacity)
    {
        pthread_cond_wait(&ch->not_full, &ch->mutex);
        if (ch->closed)
        {
            pthread_mutex_unlock(&ch->mutex);
            return -1;
        }
    }
    ch->queue[ch->tail] = data;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    // 通知有新数据进入，队列不再为空
    pthread_cond_signal(&ch->not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return 0;
}

// 非阻塞式发送数据到 channel
int channel_send_nonblocking(Channel *ch, void *data)
{
    pthread_mutex_lock(&ch->mutex);
    // 如果 channel 已关闭，不能再发送数据
    if (ch->closed)
    {
        pthread_mutex_unlock(&ch->mutex);
        return -1;
    }
    // 如果队列已满，直接返回
    if (ch->count == ch->capacity)
    {
        pthread_mutex_unlock(&ch->mutex);
        return -2;
    }
    ch->queue[ch->tail] = data;
    ch->tail = (ch->tail + 1) % ch->capacity;
    ch->count++;
    // 通知有新数据进入，队列不再为空
    pthread_cond_signal(&ch->not_empty);
    pthread_mutex_unlock(&ch->mutex);
    return 0;
}

// 阻塞式从 channel 接收数据
int channel_recv_blocking(Channel *ch, void **data)
{
    pthread_mutex_lock(&ch->mutex);
    // 等待队列有数据
    while (ch->count == 0 && !ch->closed)
    {
        pthread_cond_wait(&ch->not_empty, &ch->mutex);
    }
    if (ch->count == 0 && ch->closed)
    {
        pthread_mutex_unlock(&ch->mutex);
        return -1;
    }
    *data = ch->queue[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    // 通知队列有空间了
    pthread_cond_signal(&ch->not_full);
    pthread_mutex_unlock(&ch->mutex);
    return 0;
}

// 非阻塞式从 channel 接收数据
int channel_recv_nonblocking(Channel *ch, void **data)
{
    pthread_mutex_lock(&ch->mutex);
    // 如果队列是空且 channel 已关闭，返回 -1
    if (ch->count == 0 && ch->closed)
    {
        pthread_mutex_unlock(&ch->mutex);
        return -1;
    }
    // 如果队列为空，直接返回
    if (ch->count == 0)
    {
        pthread_mutex_unlock(&ch->mutex);
        return -2;
    }
    *data = ch->queue[ch->head];
    ch->head = (ch->head + 1) % ch->capacity;
    ch->count--;
    // 通知队列有空间了
    pthread_cond_signal(&ch->not_full);
    pthread_mutex_unlock(&ch->mutex);
    return 0;
}
