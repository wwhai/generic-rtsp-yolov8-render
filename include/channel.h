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

#ifndef CHANNEL_H
#define CHANNEL_H

#include <pthread.h>

// 定义 Channel 结构体
typedef struct
{
    void **queue;             // 存储数据的队列
    int capacity;             // 队列的容量
    int head;                 // 队列头部索引
    int tail;                 // 队列尾部索引
    int count;                // 当前队列中的元素数量
    int closed;               // 标记 channel 是否关闭
    pthread_mutex_t mutex;    // 互斥锁，用于线程同步
    pthread_cond_t not_full;  // 条件变量，用于通知队列不满
    pthread_cond_t not_empty; // 条件变量，用于通知队列不为空
} Channel;

// 创建一个新的 channel
Channel *channel_create(int capacity);

// 关闭 channel
void channel_close(Channel *ch);

// 销毁 channel
void channel_destroy(Channel *ch);

// 阻塞式发送数据到 channel
int channel_send_blocking(Channel *ch, void *data);

// 非阻塞式发送数据到 channel
int channel_send_nonblocking(Channel *ch, void *data);

// 阻塞式从 channel 接收数据
int channel_recv_blocking(Channel *ch, void **data);

// 非阻塞式从 channel 接收数据
int channel_recv_nonblocking(Channel *ch, void **data);

#endif // CHANNEL_H