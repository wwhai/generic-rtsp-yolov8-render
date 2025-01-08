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
#include "libav_utils.h"
// 函数用于复制AVFrame
AVFrame *CopyAVFrame(AVFrame *srcFrame)
{
    AVFrame *dstFrame = av_frame_alloc();
    if (!dstFrame)
    {
        fprintf(stderr, "Could not allocate frame\n");
        return NULL;
    }
    dstFrame->format = srcFrame->format;
    dstFrame->width = srcFrame->width;
    dstFrame->height = srcFrame->height;
    int ret = av_frame_get_buffer(dstFrame, 32);
    if (ret < 0)
    {
        fprintf(stderr, "Could not allocate new frame buffer\n");
        av_frame_free(&dstFrame);
        return NULL;
    }
    ret = av_frame_copy(dstFrame, srcFrame);
    if (ret < 0)
    {
        fprintf(stderr, "Could not copy frame\n");
        av_frame_free(&dstFrame);
        return NULL;
    }
    return dstFrame;
}