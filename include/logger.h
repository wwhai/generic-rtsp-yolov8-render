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
#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>

// 日志等级枚举
typedef enum
{
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
} LogLevel;

// 设置日志等级
void set_log_level(LogLevel level);

// 日志记录函数，增加文件和行号参数
void log_message(LogLevel level, const char *file, int line, const char *format, ...);

// 定义宏，方便调用时自动传入文件和行号
#define log_trace(format, ...) log_message(LOG_TRACE, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define log_debug(format, ...) log_message(LOG_DEBUG, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define log_info(format, ...) log_message(LOG_INFO, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define log_warn(format, ...) log_message(LOG_WARN, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define log_error(format, ...) log_message(LOG_ERROR, __FILE__, __LINE__, format, ##__VA_ARGS__)
#define log_fatal(format, ...) log_message(LOG_FATAL, __FILE__, __LINE__, format, ##__VA_ARGS__)

#endif // LOGGER_H