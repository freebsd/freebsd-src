/*-
 * Copyright (c) 2005 Max Okumoto
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

/*
 * buf.c
 *	Functions for automatically-expanded buffers.
 */

#include <string.h>
#include <stdlib.h>

#include "buf.h"
#include "util.h"

/**
 * Returns the number of bytes in the buffer.  Doesn't include the
 * null-terminating byte.
 */
inline size_t
Buf_Size(const Buffer *buf)
{

	return (buf->end - buf->buf);
}

/**
 * Returns a reference to the data contained in the buffer.
 *  
 * @note Adding data to the Buffer object may invalidate the reference.
 */
inline char *
Buf_Data(const Buffer *bp)
{

	return (bp->buf);
}

/**
 * Expand the buffer to hold the number of additional bytes, plus
 * space to store a terminating NULL byte.
 */
static inline void
BufExpand(Buffer *bp, size_t nb)
{
	size_t	len = Buf_Size(bp);
	size_t	size;

	if (bp->size < len + nb + 1) {
		size = bp->size + MAX(nb + 1, BUF_ADD_INC);
		bp->size = size;
		bp->buf = erealloc(bp->buf, size);
		bp->end = bp->buf + len;
	}
}

/**
 * Add a single byte to the buffer.
 */
inline void
Buf_AddByte(Buffer *bp, Byte byte)
{

	BufExpand(bp, 1);

	*bp->end = byte;
	bp->end++;
	*bp->end = '\0';
}

/**
 * Add bytes to the buffer.
 */
void
Buf_AddBytes(Buffer *bp, size_t len, const Byte *bytes)
{

	BufExpand(bp, len);

	memcpy(bp->end, bytes, len);
	bp->end += len;
	*bp->end = '\0';
}

/**
 * Get a reference to the internal buffer.
 *
 * len:
 *	Pointer to where we return the number of bytes in the internal buffer.
 *
 * Returns:
 *	return A pointer to the data.
 */
Byte *
Buf_GetAll(Buffer *bp, size_t *len)
{

	if (len != NULL)
		*len = Buf_Size(bp);

	return (bp->buf);
}

/**
 * Get the contents of a buffer and destroy the buffer. If the buffer
 * is NULL, return NULL.
 *
 * Returns:
 *	the pointer to the data.
 */
char *
Buf_Peel(Buffer *bp)
{
	char *ret;

	if (bp == NULL)
		return (NULL);
	ret = bp->buf;
	free(bp);
	return (ret);
}

/**
 * Initialize a buffer. If no initial size is given, a reasonable
 * default is used.
 *
 * Returns:
 *	A buffer object to be given to other functions in this library.
 *
 * Side Effects:
 *	Space is allocated for the Buffer object and a internal buffer.
 */
Buffer *
Buf_Init(size_t size)
{
	Buffer *bp;	/* New Buffer */

	if (size <= 0)
		size = BUF_DEF_SIZE;

	bp = emalloc(sizeof(*bp));
	bp->size = size;
	bp->buf = emalloc(size);
	bp->end = bp->buf;
	*bp->end = '\0';

	return (bp);
}

/**
 * Destroy a buffer, and optionally free its data, too.
 *
 * Side Effects:
 *	Space for the Buffer object and possibly the internal buffer
 *	is de-allocated.
 */
void
Buf_Destroy(Buffer *buf, Boolean freeData)
{

	if (freeData)
		free(buf->buf);
	free(buf);
}

/**
 * Replace the last byte in a buffer.  If the buffer was empty
 * intially, then a new byte will be added.
 */
void
Buf_ReplaceLastByte(Buffer *bp, Byte byte)
{

	if (bp->end == bp->buf) {
		Buf_AddByte(bp, byte);
	} else {
		*(bp->end - 1) = byte;
	}
}

/**
 * Append characters in str to Buffer object
 */
void
Buf_Append(Buffer *bp, const char str[])
{

	Buf_AddBytes(bp, strlen(str), str);
}

/**
 * Append characters in buf to Buffer object
 */
void
Buf_AppendBuf(Buffer *bp, const Buffer *buf)
{

	Buf_AddBytes(bp, Buf_Size(buf), buf->buf);
}

/**
 * Append characters between str and end to Buffer object.
 */
void
Buf_AppendRange(Buffer *bp, const char str[], const char *end)
{

	Buf_AddBytes(bp, end - str, str);
}

/**
 * Convert newlines in buffer to spaces.  The trailing newline is
 * removed.
 */
void
Buf_StripNewlines(Buffer *bp)
{
	char *ptr = bp->end;

	/*
	 * If there is anything in the buffer, remove the last
	 * newline character.
	 */
	if (ptr != bp->buf) {
		if (*(ptr - 1) == '\n') {
			/* shorten buffer */
			*(ptr - 1) = '\0';
			--bp->end;
		}
		--ptr;
	}

	/* Convert newline characters to a space characters.  */
	while (ptr != bp->buf) {
		if (*ptr == '\n') {
			*ptr = ' ';
		}
		--ptr;
	}
}
/**
 * Clear the contents of the buffer.
 */
void
Buf_Clear(Buffer *bp)
{

	bp->end = bp->buf;
	*bp->end = '\0';
}
