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

#ifndef CONTEXT_H
#define CONTEXT_H

#include <pthread.h>

typedef struct
{
    pthread_mutex_t mtx;
    pthread_cond_t cond;
    int is_cancelled;
} Context;

// 创建 Context 结构体
Context *CreateContext();

// 取消 Context
void CancelContext(Context *ctx);

// 检查是否已取消
int IsCancelled(Context *ctx);

#endif