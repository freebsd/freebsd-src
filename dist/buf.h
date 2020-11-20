/*	$NetBSD: buf.h,v 1.36 2020/11/10 00:32:12 rillig Exp $	*/

/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
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
 *
 *	from: @(#)buf.h	8.1 (Berkeley) 6/6/93
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
 *
 *	from: @(#)buf.h	8.1 (Berkeley) 6/6/93
 */

/* Automatically growing null-terminated buffers of characters. */

#ifndef MAKE_BUF_H
#define MAKE_BUF_H

#include <stddef.h>

/* An automatically growing null-terminated buffer of characters. */
typedef struct Buffer {
    size_t cap;		/* Allocated size of the buffer, including the null */
    size_t len;		/* Number of bytes in buffer, excluding the null */
    char *data;		/* The buffer itself (always null-terminated) */
} Buffer;

/* If we aren't on NetBSD, __predict_false() might not be defined. */
#ifndef __predict_false
#define __predict_false(x) (x)
#endif

void Buf_Expand_1(Buffer *);

/* Buf_AddByte adds a single byte to a buffer. */
MAKE_INLINE void
Buf_AddByte(Buffer *buf, char byte)
{
    size_t old_len = buf->len++;
    char *end;
    if (__predict_false(old_len + 1 >= buf->cap))
	Buf_Expand_1(buf);
    end = buf->data + old_len;
    end[0] = byte;
    end[1] = '\0';
}

MAKE_INLINE size_t
Buf_Len(const Buffer *buf)
{
    return buf->len;
}

MAKE_INLINE Boolean
Buf_EndsWith(const Buffer *buf, char ch)
{
    return buf->len > 0 && buf->data[buf->len - 1] == ch;
}

void Buf_AddBytes(Buffer *, const char *, size_t);
void Buf_AddBytesBetween(Buffer *, const char *, const char *);
void Buf_AddStr(Buffer *, const char *);
void Buf_AddInt(Buffer *, int);
char *Buf_GetAll(Buffer *, size_t *);
void Buf_Empty(Buffer *);
void Buf_Init(Buffer *);
void Buf_InitSize(Buffer *, size_t);
char *Buf_Destroy(Buffer *, Boolean);
char *Buf_DestroyCompact(Buffer *);

#endif /* MAKE_BUF_H */
