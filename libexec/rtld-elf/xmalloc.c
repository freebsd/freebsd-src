/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 1996-1998 John D. Polstra.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "rtld.h"
#include "rtld_printf.h"
#include "rtld_malloc.h"
#include "rtld_libc.h"

void *
xcalloc(size_t number, size_t size)
{
	void *p;

	p = __crt_calloc(number, size);
	if (p == NULL) {
		rtld_fdputstr(STDERR_FILENO, "Out of memory\n");
		_exit(1);
	}
	return (p);
}

void *
xmalloc(size_t size)
{

	void *p;

	p = __crt_malloc(size);
	if (p == NULL) {
		rtld_fdputstr(STDERR_FILENO, "Out of memory\n");
		_exit(1);
	}
	return (p);
}

char *
xstrdup(const char *str)
{
	char *copy;
	size_t len;

	len = strlen(str) + 1;
	copy = xmalloc(len);
	memcpy(copy, str, len);
	return (copy);
}

void *
xmalloc_aligned(size_t size, size_t align, size_t offset)
{
	void *res;

	offset &= align - 1;
	if (align < sizeof(void *))
		align = sizeof(void *);

	res = __crt_aligned_alloc_offset(align, size, offset);
	if (res == NULL) {
		rtld_fdputstr(STDERR_FILENO, "Out of memory\n");
		_exit(1);
	}
	return (res);
}
