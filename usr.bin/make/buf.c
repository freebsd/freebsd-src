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
 * @(#)buf.c	8.1 (Berkeley) 6/6/93
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * buf.c --
 *	Functions for automatically-expanded buffers.
 */

#include <string.h>
#include <stdlib.h>

#include "buf.h"
#include "sprite.h"
#include "util.h"

#ifndef max
#define	max(a,b)  ((a) > (b) ? (a) : (b))
#endif

/*
 * BufExpand --
 * 	Expand the given buffer to hold the given number of additional
 *	bytes.
 *	Makes sure there's room for an extra NULL byte at the end of the
 *	buffer in case it holds a string.
 */
#define	BufExpand(bp, nb) do {						\
 	if ((bp)->left < (nb) + 1) {					\
		int newSize = (bp)->size + max((nb) + 1, BUF_ADD_INC);	\
		Byte *newBuf = erealloc((bp)->buffer, newSize);		\
									\
		(bp)->inPtr = newBuf + ((bp)->inPtr - (bp)->buffer);	\
		(bp)->outPtr = newBuf + ((bp)->outPtr - (bp)->buffer);	\
		(bp)->buffer = newBuf;					\
		(bp)->size = newSize;					\
		(bp)->left = newSize - ((bp)->inPtr - (bp)->buffer);	\
	}								\
    } while (0)

#define	BUF_DEF_SIZE	256 	/* Default buffer size */
#define	BUF_ADD_INC	256 	/* Expansion increment when Adding */
#define	BUF_UNGET_INC	16  	/* Expansion increment when Ungetting */

/*-
 *-----------------------------------------------------------------------
 * Buf_OvAddByte --
 *	Add a single byte to the buffer.  left is zero or negative.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The buffer may be expanded.
 *
 *-----------------------------------------------------------------------
 */
void
Buf_OvAddByte(Buffer *bp, Byte byte)
{

	bp->left = 0;
	BufExpand(bp, 1);

	*bp->inPtr++ = byte;
	bp->left--;

	/*
	 * Null-terminate
	 */
	*bp->inPtr = 0;
}

/*-
 *-----------------------------------------------------------------------
 * Buf_AddBytes --
 *	Add a number of bytes to the buffer.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	Guess what?
 *
 *-----------------------------------------------------------------------
 */
void
Buf_AddBytes(Buffer *bp, size_t numBytes, const Byte *bytesPtr)
{

	BufExpand(bp, numBytes);

	memcpy(bp->inPtr, bytesPtr, numBytes);
	bp->inPtr += numBytes;
	bp->left -= numBytes;

	/*
	 * Null-terminate
	 */
	*bp->inPtr = 0;
}

/*-
 *-----------------------------------------------------------------------
 * Buf_GetAll --
 *	Get all the available data at once.
 *
 * Results:
 *	A pointer to the data and the number of bytes available.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
Byte *
Buf_GetAll(Buffer *bp, size_t *numBytesPtr)
{

	if (numBytesPtr != NULL)
		*numBytesPtr = bp->inPtr - bp->outPtr;

	return (bp->outPtr);
}

/*-
 *-----------------------------------------------------------------------
 * Buf_Size --
 *	Returns the number of bytes in the given buffer. Doesn't include
 *	the null-terminating byte.
 *
 * Results:
 *	The number of bytes.
 *
 * Side Effects:
 *	None.
 *
 *-----------------------------------------------------------------------
 */
size_t
Buf_Size(Buffer *buf)
{

	return (buf->inPtr - buf->outPtr);
}

/*-
 *-----------------------------------------------------------------------
 * Buf_Init --
 *	Initialize a buffer. If no initial size is given, a reasonable
 *	default is used.
 *
 * Results:
 *	A buffer to be given to other functions in this library.
 *
 * Side Effects:
 *	The buffer is created, the space allocated and pointers
 *	initialized.
 *
 *-----------------------------------------------------------------------
 */
Buffer *
Buf_Init(size_t size)
{
	Buffer *bp;	  	/* New Buffer */

	bp = emalloc(sizeof(*bp));

	if (size <= 0)
		size = BUF_DEF_SIZE;

	bp->left = bp->size = size;
	bp->buffer = emalloc(size);
	bp->inPtr = bp->outPtr = bp->buffer;
	*bp->inPtr = 0;

	return (bp);
}

/*-
 *-----------------------------------------------------------------------
 * Buf_Destroy --
 *	Destroy a buffer, and optionally free its data, too.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The buffer is freed.
 *
 *-----------------------------------------------------------------------
 */
void
Buf_Destroy(Buffer *buf, Boolean freeData)
{

	if (freeData)
		free(buf->buffer);
	free(buf);
}

/*-
 *-----------------------------------------------------------------------
 * Buf_ReplaceLastByte --
 *     Replace the last byte in a buffer.
 *
 * Results:
 *     None.
 *
 * Side Effects:
 *     If the buffer was empty intially, then a new byte will be added.
 *     Otherwise, the last byte is overwritten.
 *
 *-----------------------------------------------------------------------
 */
void
Buf_ReplaceLastByte(Buffer *buf, Byte byte)
{
	if (buf->inPtr == buf->outPtr)
		Buf_AddByte(buf, byte);
	else
		*(buf->inPtr - 1) = byte;
}

void
Buf_Clear(Buffer *bp)
{
	bp->inPtr	= bp->buffer;
	bp->outPtr	= bp->buffer;
	bp->left	= bp->size;
	bp->inPtr[0]	= '\0';
}

