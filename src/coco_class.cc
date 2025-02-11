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

#include "coco_class.h"

// 定义存储名称的数组
const char *coco_names[80];
void init_coco_names()
{
    coco_names[0] = "person";
    coco_names[1] = "bicycle";
    coco_names[2] = "car";
    coco_names[3] = "motorcycle";
    coco_names[4] = "airplane";
    coco_names[5] = "bus";
    coco_names[6] = "train";
    coco_names[7] = "truck";
    coco_names[8] = "boat";
    coco_names[9] = "traffic light";
    coco_names[10] = "fire hydrant";
    coco_names[11] = "stop sign";
    coco_names[12] = "parking meter";
    coco_names[13] = "bench";
    coco_names[14] = "bird";
    coco_names[15] = "cat";
    coco_names[16] = "dog";
    coco_names[17] = "horse";
    coco_names[18] = "sheep";
    coco_names[19] = "cow";
    coco_names[20] = "elephant";
    coco_names[21] = "bear";
    coco_names[22] = "zebra";
    coco_names[23] = "giraffe";
    coco_names[24] = "backpack";
    coco_names[25] = "umbrella";
    coco_names[26] = "handbag";
    coco_names[27] = "tie";
    coco_names[28] = "suitcase";
    coco_names[29] = "frisbee";
    coco_names[30] = "skis";
    coco_names[31] = "snowboard";
    coco_names[32] = "sports ball";
    coco_names[33] = "kite";
    coco_names[34] = "baseball bat";
    coco_names[35] = "baseball glove";
    coco_names[36] = "skateboard";
    coco_names[37] = "surfboard";
    coco_names[38] = "tennis racket";
    coco_names[39] = "bottle";
    coco_names[40] = "wine glass";
    coco_names[41] = "cup";
    coco_names[42] = "fork";
    coco_names[43] = "knife";
    coco_names[44] = "spoon";
    coco_names[45] = "bowl";
    coco_names[46] = "banana";
    coco_names[47] = "apple";
    coco_names[48] = "sandwich";
    coco_names[49] = "orange";
    coco_names[50] = "broccoli";
    coco_names[51] = "carrot";
    coco_names[52] = "hot dog";
    coco_names[53] = "pizza";
    coco_names[54] = "donut";
    coco_names[55] = "cake";
    coco_names[56] = "chair";
    coco_names[57] = "couch";
    coco_names[58] = "potted plant";
    coco_names[59] = "bed";
    coco_names[60] = "dining table";
    coco_names[61] = "toilet";
    coco_names[62] = "tv";
    coco_names[63] = "laptop";
    coco_names[64] = "mouse";
    coco_names[65] = "remote";
    coco_names[66] = "keyboard";
    coco_names[67] = "cell phone";
    coco_names[68] = "microwave";
    coco_names[69] = "oven";
    coco_names[70] = "toaster";
    coco_names[71] = "sink";
    coco_names[72] = "refrigerator";
    coco_names[73] = "book";
    coco_names[74] = "clock";
    coco_names[75] = "vase";
    coco_names[76] = "scissors";
    coco_names[77] = "teddy bear";
    coco_names[78] = "hair drier";
    coco_names[79] = "toothbrush";
}
void print_coco_names()
{
    fprintf(stdout, "===========coco names============ \n");
    for (int i = 0; i < 80; i++)
    {
        printf("  %d => %s\n", i, coco_names[i]);
    }
    fprintf(stdout, "================================= \n");
}

const char *get_coco_name(int id)
{
    if (id < 0 || id > 80)
    {
        return (const char *)"unknown";
    }
    return coco_names[id];
}