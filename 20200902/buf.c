/*	$NetBSD: buf.c,v 1.37 2020/08/23 08:21:50 rillig Exp $	*/

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

#ifndef MAKE_NATIVE
static char rcsid[] = "$NetBSD: buf.c,v 1.37 2020/08/23 08:21:50 rillig Exp $";
#else
#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)buf.c	8.1 (Berkeley) 6/6/93";
#else
__RCSID("$NetBSD: buf.c,v 1.37 2020/08/23 08:21:50 rillig Exp $");
#endif
#endif /* not lint */
#endif

/* Functions for automatically-expanded null-terminated buffers. */

#include <limits.h>
#include "make.h"

/* Extend the buffer for adding a single byte. */
void
Buf_Expand_1(Buffer *bp)
{
    bp->size += MAX(bp->size, 16);
    bp->buffer = bmake_realloc(bp->buffer, bp->size);
}

/* Add the given bytes to the buffer. */
void
Buf_AddBytes(Buffer *bp, const char *bytesPtr, size_t numBytes)
{
    size_t count = bp->count;
    char *ptr;

    if (__predict_false(count + numBytes >= bp->size)) {
	bp->size += MAX(bp->size, numBytes + 16);
	bp->buffer = bmake_realloc(bp->buffer, bp->size);
    }

    ptr = bp->buffer + count;
    bp->count = count + numBytes;
    memcpy(ptr, bytesPtr, numBytes);
    ptr[numBytes] = '\0';
}

/* Add the bytes between start and end to the buffer. */
void
Buf_AddBytesBetween(Buffer *bp, const char *start, const char *end)
{
    Buf_AddBytes(bp, start, (size_t)(end - start));
}

/* Add the given string to the buffer. */
void
Buf_AddStr(Buffer *bp, const char *str)
{
    Buf_AddBytes(bp, str, strlen(str));
}

/* Add the given number to the buffer. */
void
Buf_AddInt(Buffer *bp, int n)
{
    enum {
	bits = sizeof(int) * CHAR_BIT,
	max_octal_digits = (bits + 2) / 3,
	max_decimal_digits = /* at most */ max_octal_digits,
	max_sign_chars = 1,
	buf_size = max_sign_chars + max_decimal_digits + 1
    };
    char buf[buf_size];

    size_t len = (size_t)snprintf(buf, sizeof buf, "%d", n);
    Buf_AddBytes(bp, buf, len);
}

/* Get the data (usually a string) from the buffer.
 * The returned data is valid until the next modifying operation
 * on the buffer.
 *
 * Returns the pointer to the data and optionally the length of the
 * data in the buffer. */
char *
Buf_GetAll(Buffer *bp, size_t *numBytesPtr)
{
    if (numBytesPtr != NULL)
	*numBytesPtr = bp->count;
    return bp->buffer;
}

/* Mark the buffer as empty, so it can be filled with data again. */
void
Buf_Empty(Buffer *bp)
{
    bp->count = 0;
    bp->buffer[0] = '\0';
}

/* Initialize a buffer.
 * If the given initial size is 0, a reasonable default is used. */
void
Buf_Init(Buffer *bp, size_t size)
{
    if (size <= 0) {
	size = 256;
    }
    bp->size = size;
    bp->count = 0;
    bp->buffer = bmake_malloc(size);
    bp->buffer[0] = '\0';
}

/* Reset the buffer.
 * If freeData is TRUE, the data from the buffer is freed as well.
 * Otherwise it is kept and returned. */
char *
Buf_Destroy(Buffer *buf, Boolean freeData)
{
    char *data = buf->buffer;
    if (freeData) {
	free(data);
	data = NULL;
    }

    buf->size = 0;
    buf->count = 0;
    buf->buffer = NULL;

    return data;
}

#ifndef BUF_COMPACT_LIMIT
# define BUF_COMPACT_LIMIT 128		/* worthwhile saving */
#endif

/* Reset the buffer and return its data.
 *
 * If the buffer size is much greater than its content,
 * a new buffer will be allocated and the old one freed. */
char *
Buf_DestroyCompact(Buffer *buf)
{
#if BUF_COMPACT_LIMIT > 0
    if (buf->size - buf->count >= BUF_COMPACT_LIMIT) {
	/* We trust realloc to be smart */
	char *data = bmake_realloc(buf->buffer, buf->count + 1);
	data[buf->count] = '\0';
	Buf_Destroy(buf, FALSE);
	return data;
    }
#endif
    return Buf_Destroy(buf, FALSE);
}
