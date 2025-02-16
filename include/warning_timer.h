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

#ifndef WARNING_TIMER_H
#define WARNING_TIMER_H

#include <stdint.h>
extern "C"
{
#include <libavutil/frame.h>
#include <libavformat/avformat.h>
}
// 定义告警结构体
typedef struct
{
    uint32_t warning_count;
    uint32_t interval_ms;
    char coco_types[40];
    int latest_warning_timestamp;
    AVFrame *frame;
} WarningInfo;
// 打印WarningInfo
void print_warning_info(WarningInfo *info);
// 初始化告警计时器
// interval_ms: 时间间隔 T（毫秒）
// threshold: 告警次数阈值 N
// callback: 触发事件的回调函数
int warning_timer_init(uint32_t interval_ms, uint32_t threshold, void (*callback)(WarningInfo *));

// 记录一次告警
// type: 告警类型
// timestamp: 告警时间戳
void warning_timer_record_warning(char label[40], int timestamp, AVFrame *frame);
// 停止告警计时器
void warning_timer_stop();

// 触发事件的回调函数
void event_triggered(WarningInfo *info);

#endif // WARNING_TIMER_H