/*
 * *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2021 Gavin D. Howard and contributors.
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
 * Definitions for implementing buffered I/O on my own terms.
 *
 */

#ifndef BC_FILE_H
#define BC_FILE_H

#include <stdarg.h>

#include <vector.h>

#define BC_FILE_ULL_LENGTH (21)

typedef struct BcFile {

	int fd;
	char *buf;
	size_t len;
	size_t cap;

} BcFile;

void bc_file_init(BcFile *f, int fd, char *buf, size_t cap);
void bc_file_free(BcFile *f);

void bc_file_putchar(BcFile *restrict f, uchar c);
BcStatus bc_file_flushErr(BcFile *restrict f);
void bc_file_flush(BcFile *restrict f);
void bc_file_write(BcFile *restrict f, const char *buf, size_t n);
void bc_file_printf(BcFile *restrict f, const char *fmt, ...);
void bc_file_vprintf(BcFile *restrict f, const char *fmt, va_list args);
void bc_file_puts(BcFile *restrict f, const char *str);

#endif // BC_FILE_H
