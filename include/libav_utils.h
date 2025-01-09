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

#ifndef LIBAV_UTILS_H
#define LIBAV_UTILS_H
extern "C"
{
#include <libavutil/frame.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

#include <SDL2/SDL.h>
}
// 函数用于复制AVFrame
// 注意：使用完毕后需要释放
// @param srcFrame 源帧
// @return 复制后的帧
AVFrame *CopyAVFrame(AVFrame *srcFrame);

// 截图
int CaptureImage(AVFrame *srcFrame, const char *file_path);
// 录制
int RecordAVFrameToMP4(const char *output_file, AVFrame *frames[], int num_frames, int width, int height, int fps);
#endif