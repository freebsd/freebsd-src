/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)buf.h	8.2 (Berkeley) 4/28/95
 * $FreeBSD$
 */

#ifndef buf_h_a61a6812
#define	buf_h_a61a6812

/*-
 * buf.h --
 *	Header for users of the buf library.
 */

#include <sys/types.h>

#include "sprite.h"

/*
 * There are several places where expandable buffers are used (parse.c and
 * var.c). This constant is merely the starting point for those buffers. If
 * lines tend to be much shorter than this, it would be best to reduce BSIZE.
 * If longer, it should be increased. Reducing it will cause more copying to
 * be done for longer lines, but will save space for shorter ones. In any
 * case, it ought to be a power of two simply because most storage allocation
 * schemes allocate in powers of two.
 */
#define	MAKE_BSIZE	256	/* starting size for expandable buffers */

#define	BUF_ERROR 256

typedef char Byte;

typedef struct Buffer {
	size_t	size; 		/* Current size of the buffer */
	size_t	left;		/* Space left (== size - (inPtr - buffer)) */
	Byte	*buffer;	/* The buffer itself */
	Byte	*inPtr;		/* Place to write to */
	Byte	*outPtr;	/* Place to read from */
} *Buffer;

/* Buf_AddByte adds a single byte to a buffer. */
#define	Buf_AddByte(bp, byte) \
	(void)(--(bp)->left <= 0 ? Buf_OvAddByte((bp), (byte)), 1 : \
		(*(bp)->inPtr++ = (byte), *(bp)->inPtr = 0), 1)

void Buf_OvAddByte(Buffer, Byte);
void Buf_AddBytes(Buffer, size_t, const Byte *);
void Buf_UngetByte(Buffer, Byte);
void Buf_UngetBytes(Buffer, size_t, Byte *);
int Buf_GetByte(Buffer);
int Buf_GetBytes(Buffer, size_t, Byte *);
Byte *Buf_GetAll(Buffer, size_t *);
void Buf_Discard(Buffer, size_t);
size_t Buf_Size(Buffer);
Buffer Buf_Init(size_t);
void Buf_Destroy(Buffer, Boolean);
void Buf_ReplaceLastByte(Buffer, Byte);

#endif /* buf_h_a61a6812 */
