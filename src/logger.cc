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
#include "logger.h"
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

// 全局日志等级
static LogLevel current_log_level = LOG_INFO;

// 颜色代码
const char *color_codes[] = {
    "\x1B[90m", // 灰色 (TRACE)
    "\x1B[36m", // 青色 (DEBUG)
    "\x1B[32m", // 绿色 (INFO)
    "\x1B[33m", // 黄色 (WARN)
    "\x1B[31m", // 红色 (ERROR)
    "\x1B[41m"  // 红底白字 (FATAL)
};

const char *reset_color = "\x1B[0m";

// 日志等级名称
const char *log_level_names[] = {
    "TRACE",
    "DEBUG",
    "INFO",
    "WARN",
    "ERROR",
    "FATAL"};

// 设置日志等级
void set_log_level(LogLevel level)
{
    current_log_level = level;
}

// 获取当前时间字符串
char *get_current_time()
{
    time_t rawtime;
    struct tm *timeinfo;
    char *time_str = (char *)malloc(26);
    if (time_str == NULL)
    {
        return NULL;
    }

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    if (timeinfo == NULL || strftime(time_str, 26, "%Y-%m-%d %H:%M:%S", timeinfo) == 0)
    {
        free(time_str);
        return NULL;
    }

    return time_str;
}

// 日志记录函数，增加文件和行号参数
void log_message(LogLevel level, const char *file, int line, const char *format, ...)
{
    if (level < current_log_level)
    {
        return;
    }

    char *time_str = get_current_time();
    va_list args;

    // 打印时间、文件、行号和日志等级
    if (time_str != NULL)
    {
        fprintf(stdout, "%s%s [%s] %s:%d - ", color_codes[level], time_str, log_level_names[level], file, line);
        free(time_str);
    }
    else
    {
        fprintf(stdout, "%sUnknownTime [%s] %s:%d - ", color_codes[level], log_level_names[level], file, line);
    }

    // 打印日志消息
    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);

    // 重置颜色
    fprintf(stdout, "%s\n", reset_color);
}