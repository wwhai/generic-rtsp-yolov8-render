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
#ifndef COCO_CLASS_H
#define COCO_CLASS_H
#include <stdio.h>

// 初始化数组
void init_coco_names();
// 根据id获取name
const char *get_coco_name(int id);
// 打印出coco类型数组
void print_coco_names();
#endif // COCO_CLASS_H
