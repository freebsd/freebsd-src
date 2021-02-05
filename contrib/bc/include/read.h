/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2020 Gavin D. Howard and contributors.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * *****************************************************************************
 *
 * Code to handle special I/O for bc.
 *
 */

#ifndef BC_READ_H
#define BC_READ_H

#include <stdlib.h>

#include <status.h>
#include <vector.h>

#ifndef BC_ENABLE_PROMPT
#define BC_ENABLE_PROMPT (1)
#endif // BC_ENABLE_PROMPT

#if !BC_ENABLE_PROMPT
#define bc_read_line(vec, prompt) bc_read_line(vec)
#define bc_read_chars(vec, prompt) bc_read_chars(vec)
#endif // BC_ENABLE_PROMPT

#define BC_READ_BIN_CHAR(c) (((c) < ' ' && !isspace((c))) || ((uchar) c) > '~')

BcStatus bc_read_line(BcVec *vec, const char *prompt);
void bc_read_file(const char *path, char **buf);
BcStatus bc_read_chars(BcVec *vec, const char *prompt);
bool bc_read_buf(BcVec *vec, char *buf, size_t *buf_len);

#endif // BC_READ_H
