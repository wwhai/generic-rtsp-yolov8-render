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

#include "thread_args.h"
void dump_thread_args(ThreadArgs *args)
{
    fprintf(stderr, "=== dump_thread_args ===\n");
    fprintf(stderr, "input_stream_url=%s\n", args->input_stream_url);
    fprintf(stderr, "output_stream_url=%s\n", args->output_stream_url);
}