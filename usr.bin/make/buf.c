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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)buf.c	8.1 (Berkeley) 6/6/93";
#else
static const char rcsid[] =
  "$FreeBSD$";
#endif
#endif /* not lint */

/*-
 * buf.c --
 *	Functions for automatically-expanded buffers.
 */

#include    "sprite.h"
#include    "make.h"
#include    "buf.h"

#ifndef max
#define max(a,b)  ((a) > (b) ? (a) : (b))
#endif

/*
 * BufExpand --
 * 	Expand the given buffer to hold the given number of additional
 *	bytes.
 *	Makes sure there's room for an extra NULL byte at the end of the
 *	buffer in case it holds a string.
 */
#define BufExpand(bp,nb) \
 	if (bp->left < (nb)+1) {\
	    int newSize = (bp)->size + max((nb)+1,BUF_ADD_INC); \
	    Byte  *newBuf = (Byte *) erealloc((bp)->buffer, newSize); \
	    \
	    (bp)->inPtr = newBuf + ((bp)->inPtr - (bp)->buffer); \
	    (bp)->outPtr = newBuf + ((bp)->outPtr - (bp)->buffer);\
	    (bp)->buffer = newBuf;\
	    (bp)->size = newSize;\
	    (bp)->left = newSize - ((bp)->inPtr - (bp)->buffer);\
	}

#define BUF_DEF_SIZE	256 	/* Default buffer size */
#define BUF_ADD_INC	256 	/* Expansion increment when Adding */
#define BUF_UNGET_INC	16  	/* Expansion increment when Ungetting */

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
Buf_OvAddByte (bp, byte)
    register Buffer bp;
    int    byte;
{
    int nbytes = 1;
    bp->left = 0;
    BufExpand (bp, nbytes);

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
Buf_AddBytes (bp, numBytes, bytesPtr)
    register Buffer bp;
    int	    numBytes;
    const Byte *bytesPtr;
{

    BufExpand (bp, numBytes);

    memcpy (bp->inPtr, bytesPtr, numBytes);
    bp->inPtr += numBytes;
    bp->left -= numBytes;

    /*
     * Null-terminate
     */
    *bp->inPtr = 0;
}

/*-
 *-----------------------------------------------------------------------
 * Buf_UngetByte --
 *	Place the byte back at the beginning of the buffer.
 *
 * Results:
 *	SUCCESS if the byte was added ok. FAILURE if not.
 *
 * Side Effects:
 *	The byte is stuffed in the buffer and outPtr is decremented.
 *
 *-----------------------------------------------------------------------
 */
void
Buf_UngetByte (bp, byte)
    register Buffer bp;
    int    byte;
{

    if (bp->outPtr != bp->buffer) {
	bp->outPtr--;
	*bp->outPtr = byte;
    } else if (bp->outPtr == bp->inPtr) {
	*bp->inPtr = byte;
	bp->inPtr++;
	bp->left--;
	*bp->inPtr = 0;
    } else {
	/*
	 * Yech. have to expand the buffer to stuff this thing in.
	 * We use a different expansion constant because people don't
	 * usually push back many bytes when they're doing it a byte at
	 * a time...
	 */
	int 	  numBytes = bp->inPtr - bp->outPtr;
	Byte	  *newBuf;

	newBuf = (Byte *)emalloc(bp->size + BUF_UNGET_INC);
	memcpy ((char *)(newBuf+BUF_UNGET_INC), (char *)bp->outPtr, numBytes+1);
	bp->outPtr = newBuf + BUF_UNGET_INC;
	bp->inPtr = bp->outPtr + numBytes;
	free ((char *)bp->buffer);
	bp->buffer = newBuf;
	bp->size += BUF_UNGET_INC;
	bp->left = bp->size - (bp->inPtr - bp->buffer);
	bp->outPtr -= 1;
	*bp->outPtr = byte;
    }
}

/*-
 *-----------------------------------------------------------------------
 * Buf_UngetBytes --
 *	Push back a series of bytes at the beginning of the buffer.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	outPtr is decremented and the bytes copied into the buffer.
 *
 *-----------------------------------------------------------------------
 */
void
Buf_UngetBytes (bp, numBytes, bytesPtr)
    register Buffer bp;
    int	    numBytes;
    Byte    *bytesPtr;
{

    if (bp->outPtr - bp->buffer >= numBytes) {
	bp->outPtr -= numBytes;
	memcpy (bp->outPtr, bytesPtr, numBytes);
    } else if (bp->outPtr == bp->inPtr) {
	Buf_AddBytes (bp, numBytes, bytesPtr);
    } else {
	int 	  curNumBytes = bp->inPtr - bp->outPtr;
	Byte	  *newBuf;
	int 	  newBytes = max(numBytes,BUF_UNGET_INC);

	newBuf = (Byte *)emalloc (bp->size + newBytes);
	memcpy((char *)(newBuf+newBytes), (char *)bp->outPtr, curNumBytes+1);
	bp->outPtr = newBuf + newBytes;
	bp->inPtr = bp->outPtr + curNumBytes;
	free ((char *)bp->buffer);
	bp->buffer = newBuf;
	bp->size += newBytes;
	bp->left = bp->size - (bp->inPtr - bp->buffer);
	bp->outPtr -= numBytes;
	memcpy ((char *)bp->outPtr, (char *)bytesPtr, numBytes);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Buf_GetByte --
 *	Return the next byte from the buffer. Actually returns an integer.
 *
 * Results:
 *	Returns BUF_ERROR if there's no byte in the buffer, or the byte
 *	itself if there is one.
 *
 * Side Effects:
 *	outPtr is incremented and both outPtr and inPtr will be reset if
 *	the buffer is emptied.
 *
 *-----------------------------------------------------------------------
 */
int
Buf_GetByte (bp)
    register Buffer bp;
{
    int	    res;

    if (bp->inPtr == bp->outPtr) {
	return (BUF_ERROR);
    } else {
	res = (int) *bp->outPtr;
	bp->outPtr += 1;
	if (bp->outPtr == bp->inPtr) {
	    bp->outPtr = bp->inPtr = bp->buffer;
	    bp->left = bp->size;
	    *bp->inPtr = 0;
	}
	return (res);
    }
}

/*-
 *-----------------------------------------------------------------------
 * Buf_GetBytes --
 *	Extract a number of bytes from the buffer.
 *
 * Results:
 *	The number of bytes gotten.
 *
 * Side Effects:
 *	The passed array is overwritten.
 *
 *-----------------------------------------------------------------------
 */
int
Buf_GetBytes (bp, numBytes, bytesPtr)
    register Buffer bp;
    int	    numBytes;
    Byte    *bytesPtr;
{

    if (bp->inPtr - bp->outPtr < numBytes) {
	numBytes = bp->inPtr - bp->outPtr;
    }
    memcpy (bytesPtr, bp->outPtr, numBytes);
    bp->outPtr += numBytes;

    if (bp->outPtr == bp->inPtr) {
	bp->outPtr = bp->inPtr = bp->buffer;
	bp->left = bp->size;
	*bp->inPtr = 0;
    }
    return (numBytes);
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
Buf_GetAll (bp, numBytesPtr)
    register Buffer bp;
    int	    *numBytesPtr;
{

    if (numBytesPtr != (int *)NULL) {
	*numBytesPtr = bp->inPtr - bp->outPtr;
    }

    return (bp->outPtr);
}

/*-
 *-----------------------------------------------------------------------
 * Buf_Discard --
 *	Throw away bytes in a buffer.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The bytes are discarded.
 *
 *-----------------------------------------------------------------------
 */
void
Buf_Discard (bp, numBytes)
    register Buffer bp;
    int	    numBytes;
{

    if (bp->inPtr - bp->outPtr <= numBytes) {
	bp->inPtr = bp->outPtr = bp->buffer;
	bp->left = bp->size;
	*bp->inPtr = 0;
    } else {
	bp->outPtr += numBytes;
    }
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
int
Buf_Size (buf)
    Buffer  buf;
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
Buffer
Buf_Init (size)
    int	    size; 	/* Initial size for the buffer */
{
    Buffer bp;	  	/* New Buffer */

    bp = (Buffer)emalloc(sizeof(*bp));

    if (size <= 0) {
	size = BUF_DEF_SIZE;
    }
    bp->left = bp->size = size;
    bp->buffer = (Byte *)emalloc(size);
    bp->inPtr = bp->outPtr = bp->buffer;
    *bp->inPtr = 0;

    return (bp);
}

/*-
 *-----------------------------------------------------------------------
 * Buf_Destroy --
 *	Nuke a buffer and all its resources.
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
Buf_Destroy (buf, freeData)
    Buffer  buf;  	/* Buffer to destroy */
    Boolean freeData;	/* TRUE if the data should be destroyed as well */
{

    if (freeData) {
	free ((char *)buf->buffer);
    }
    free ((char *)buf);
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
Buf_ReplaceLastByte (buf, byte)
    Buffer buf;	/* buffer to augment */
    int byte;	/* byte to be written */
{
    if (buf->inPtr == buf->outPtr)
        Buf_AddByte(buf, byte);
    else
        *(buf->inPtr - 1) = byte;
}
