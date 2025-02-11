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

#include "http_api.h"

// 回调函数，用于处理响应数据
size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    fprintf(stdout, "=== http write callback === %.*s\n", (int)realsize, (char *)contents);
    return realsize;
}
void post_recognized_type(const char *url, int type, const char *device_uuid)
{
    char json_body[256];
    time_t now = time(NULL);
    struct tm *tm_info;
    char ts[20];
    // 将时间戳转换为本地时间
    tm_info = localtime(&now);
    // 使用 strftime 函数将时间格式化为 YYYY-MM-DD HH:MM:SS
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
    // 构建 JSON 数据
    snprintf(json_body, sizeof(json_body), "{\"type\": %d, \"ts\": \"%s\", \"device_uuid\": \"%s\"}", type, ts, device_uuid);
    // 输出JSON
    fprintf(stdout, "====== post_recognized_type json_body ======\n");
    fprintf(stdout, "POST: %s, Body: %s\n", url, json_body);
    fprintf(stdout, "============================================\n");
}
