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
#include <unistd.h>
#include "context.h"

// 创建 Context 结构体
Context *CreateContext()
{
    Context *ctx = (Context *)malloc(sizeof(Context));
    if (ctx == NULL)
    {
        perror("malloc");
        return NULL;
    }
    if (pthread_mutex_init(&ctx->mtx, NULL) != 0)
    {
        perror("pthread_mutex_init");
        free(ctx);
        return NULL;
    }
    if (pthread_cond_init(&ctx->cond, NULL) != 0)
    {
        perror("pthread_cond_init");
        pthread_mutex_destroy(&ctx->mtx);
        free(ctx);
        return NULL;
    }
    ctx->is_cancelled = 0;
    return ctx;
}

// 取消 Context
void CancelContext(Context *ctx)
{
    pthread_mutex_lock(&ctx->mtx);
    ctx->is_cancelled = 1;
    pthread_cond_broadcast(&ctx->cond);
    pthread_mutex_unlock(&ctx->mtx);
}

// 检查是否已取消
int IsCancelled(Context *ctx)
{
    pthread_mutex_lock(&ctx->mtx);
    int result = ctx->is_cancelled;
    pthread_mutex_unlock(&ctx->mtx);
    return result;
}

// 工作线程函数
void *workerFunction(void *arg)
{
    Context *ctx = (Context *)arg;
    while (1)
    {
        if (IsCancelled(ctx))
        {
            printf("Worker is stopping...\n");
            break;
        }
        printf("Worker is running...\n");
        sleep(1);
    }
    pthread_exit(NULL);
    return (void *)NULL;
}
