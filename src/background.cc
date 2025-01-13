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

#include "background.h"
#include "thread_args.h"

void *background_task_thread(void *arg)
{
    ThreadArgs *args = (ThreadArgs *)arg;
    pthread_mutex_lock(&(args->ctx->mtx));
    while (!args->ctx->is_cancelled)
    {
        pthread_cond_wait(&(args->ctx->cond), &(args->ctx->mtx));
    }
    pthread_mutex_unlock(&(args->ctx->mtx));
    return NULL;
}
