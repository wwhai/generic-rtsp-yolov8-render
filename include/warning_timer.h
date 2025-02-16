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

// 定义告警结构体
typedef struct
{
    uint32_t warning_count;       // 告警次数
    uint32_t interval_ms;         // 时间间隔（毫秒）
    int latest_warning_type;      // 最新告警类型
    int latest_warning_timestamp; // 最新告警时间戳
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
void warning_timer_record_warning(int type, int timestamp);

// 停止告警计时器
void warning_timer_stop();

// 触发事件的回调函数
void event_triggered(WarningInfo *info);

#endif // WARNING_TIMER_H