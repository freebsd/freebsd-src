/*
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

/*-
 * buf.h --
 *	Header for users of the buf library.
 */

#ifndef _BUF_H
#define	_BUF_H

#include    "sprite.h"

typedef char Byte;

typedef struct Buffer {
    int	    size; 	/* Current size of the buffer */
    int     left;	/* Space left (== size - (inPtr - buffer)) */
    Byte    *buffer;	/* The buffer itself */
    Byte    *inPtr;	/* Place to write to */
    Byte    *outPtr;	/* Place to read from */
} *Buffer;

/* Buf_AddByte adds a single byte to a buffer. */
#define	Buf_AddByte(bp, byte) \
	(void) (--(bp)->left <= 0 ? Buf_OvAddByte(bp, byte), 1 : \
		(*(bp)->inPtr++ = (byte), *(bp)->inPtr = 0), 1)

#define	BUF_ERROR 256

void Buf_OvAddByte(Buffer, int);
void Buf_AddBytes(Buffer, int, const Byte *);
void Buf_UngetByte(Buffer, int);
void Buf_UngetBytes(Buffer, int, Byte *);
int Buf_GetByte(Buffer);
int Buf_GetBytes(Buffer, int, Byte *);
Byte *Buf_GetAll(Buffer, int *);
void Buf_Discard(Buffer, int);
int Buf_Size(Buffer);
Buffer Buf_Init(int);
void Buf_Destroy(Buffer, Boolean);
void Buf_ReplaceLastByte(Buffer, int);

#endif /* _BUF_H */
