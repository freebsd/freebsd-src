/*
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
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
 *	@(#)buf.h	5.4 (Berkeley) 12/28/90
 */

/*-
 * buf.h --
 *	Header for users of the buf library.
 */

#ifndef _BUF_H
#define _BUF_H

#include    "sprite.h"

typedef unsigned char Byte;

typedef struct Buffer {
    int	    size; 	/* Current size of the buffer */
    int     left;	/* Space left (== size - (inPtr - buffer)) */
    Byte    *buffer;	/* The buffer itself */
    Byte    *inPtr;	/* Place to write to */
    Byte    *outPtr;	/* Place to read from */
} *Buffer;

Buffer	    	  Buf_Init();	    /* Initialize a buffer */
void	    	  Buf_Destroy();    /* Destroy a buffer */
void	    	  Buf_AddBytes();   /* Add a range of bytes to a buffer */
int	    	  Buf_GetByte();    /* Get a byte from a buffer */
int	    	  Buf_GetBytes();   /* Get multiple bytes */
void		  Buf_UngetByte();  /* Push a byte back into the buffer */
void		  Buf_UngetBytes(); /* Push many bytes back into the buf */
Byte	    	  *Buf_GetAll();    /* Get them all */
void	    	  Buf_Discard();    /* Throw away some of the bytes */
int	    	  Buf_Size();	    /* See how many are there */

/* Buf_AddByte adds a single byte to a buffer. */
#define	Buf_AddByte(bp, byte) \
	(--(bp)->left <= 0 ? Buf_OvAddByte(bp, byte) : \
		(void)(*(bp)->inPtr++ = (byte), *(bp)->inPtr = 0))

void	Buf_OvAddByte();		/* adds a byte when buffer overflows */

#define BUF_ERROR 256

#endif _BUF_H
