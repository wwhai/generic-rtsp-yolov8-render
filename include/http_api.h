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

#ifndef __INTEGRATED_HTTP_API_H__
#define __INTEGRATED_HTTP_API_H__
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "curl/curl.h"
// 回调函数，用于处理响应数据
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp);
// 打印表示识别类型的 JSON 字符串的函数，增加时间戳和上传者 UUID
void post_recognized_type(const char *url, const char *type, const char *device_uuid);
#endif