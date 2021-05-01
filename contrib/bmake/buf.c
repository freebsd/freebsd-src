/*	$NetBSD: buf.c,v 1.51 2021/01/30 21:18:14 rillig Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1988, 1989 by Adam de Boor
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Automatically-expanding null-terminated character buffers. */

#include <limits.h>
#include "make.h"

/*	"@(#)buf.c	8.1 (Berkeley) 6/6/93"	*/
MAKE_RCSID("$NetBSD: buf.c,v 1.51 2021/01/30 21:18:14 rillig Exp $");

/* Make space in the buffer for adding at least 16 more bytes. */
void
Buf_Expand(Buffer *buf)
{
	buf->cap += buf->cap > 16 ? buf->cap : 16;
	buf->data = bmake_realloc(buf->data, buf->cap);
}

/* Add the bytes to the buffer. */
void
Buf_AddBytes(Buffer *buf, const char *bytes, size_t bytes_len)
{
	size_t old_len = buf->len;
	char *end;

	if (old_len + bytes_len >= buf->cap) {
		size_t minIncr = bytes_len + 16;
		buf->cap += buf->cap > minIncr ? buf->cap : minIncr;
		buf->data = bmake_realloc(buf->data, buf->cap);
	}

	end = buf->data + old_len;
	buf->len = old_len + bytes_len;
	memcpy(end, bytes, bytes_len);
	end[bytes_len] = '\0';
}

/* Add the bytes between start and end to the buffer. */
void
Buf_AddBytesBetween(Buffer *buf, const char *start, const char *end)
{
	Buf_AddBytes(buf, start, (size_t)(end - start));
}

/* Add the string to the buffer. */
void
Buf_AddStr(Buffer *buf, const char *str)
{
	Buf_AddBytes(buf, str, strlen(str));
}

/* Add the number to the buffer. */
void
Buf_AddInt(Buffer *buf, int n)
{
	enum {
		bits = sizeof(int) * CHAR_BIT,
		max_octal_digits = (bits + 2) / 3,
		max_decimal_digits = /* at most */ max_octal_digits,
		max_sign_chars = 1,
		str_size = max_sign_chars + max_decimal_digits + 1
	};
	char str[str_size];

	size_t len = (size_t)snprintf(str, sizeof str, "%d", n);
	Buf_AddBytes(buf, str, len);
}

/* Mark the buffer as empty, so it can be filled with data again. */
void
Buf_Empty(Buffer *buf)
{
	buf->len = 0;
	buf->data[0] = '\0';
}

/* Initialize a buffer. */
void
Buf_InitSize(Buffer *buf, size_t cap)
{
	buf->cap = cap;
	buf->len = 0;
	buf->data = bmake_malloc(cap);
	buf->data[0] = '\0';
}

void
Buf_Init(Buffer *buf)
{
	Buf_InitSize(buf, 256);
}

/*
 * Free the data from the buffer.
 * Leave the buffer itself in an indeterminate state.
 */
void
Buf_Done(Buffer *buf)
{
	free(buf->data);

#ifdef CLEANUP
	buf->cap = 0;
	buf->len = 0;
	buf->data = NULL;
#endif
}

/*
 * Return the data from the buffer.
 * Leave the buffer itself in an indeterminate state.
 */
char *
Buf_DoneData(Buffer *buf)
{
	char *data = buf->data;

#ifdef CLEANUP
	buf->cap = 0;
	buf->len = 0;
	buf->data = NULL;
#endif

	return data;
}

#ifndef BUF_COMPACT_LIMIT
# define BUF_COMPACT_LIMIT 128	/* worthwhile saving */
#endif

/*
 * Return the data from the buffer.
 * Leave the buffer itself in an indeterminate state.
 *
 * If the buffer size is much greater than its content,
 * a new buffer will be allocated and the old one freed.
 */
char *
Buf_DoneDataCompact(Buffer *buf)
{
#if BUF_COMPACT_LIMIT > 0
	if (buf->cap - buf->len >= BUF_COMPACT_LIMIT) {
		/* We trust realloc to be smart */
		char *data = bmake_realloc(buf->data, buf->len + 1);
		data[buf->len] = '\0';	/* XXX: unnecessary */
		Buf_DoneData(buf);
		return data;
	}
#endif
	return Buf_DoneData(buf);
}
