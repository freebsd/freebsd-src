/*
 * Copyright (c) 1988 Mark Nudleman
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static char sccsid[] = "@(#)ch.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*
 * Low level character input from the input file.
 * We use these special purpose routines which optimize moving
 * both forward and backward from the current read pointer.
 */

#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <stdio.h>
#include <less.h>

int file = -1;		/* File descriptor of the input file */

/*
 * Pool of buffers holding the most recently used blocks of the input file.
 */
struct buf {
	struct buf *next, *prev;
	long block;
	int datasize;
	char data[BUFSIZ];
};
int nbufs;

/*
 * The buffer pool is kept as a doubly-linked circular list, in order from
 * most- to least-recently used.  The circular list is anchored by buf_anchor.
 */
#define	END_OF_CHAIN	((struct buf *)&buf_anchor)
#define	buf_head	buf_anchor.next
#define	buf_tail	buf_anchor.prev

static struct {
	struct buf *next, *prev;
} buf_anchor = { END_OF_CHAIN, END_OF_CHAIN };

extern int ispipe, cbufs, sigs;

/*
 * Current position in file.
 * Stored as a block number and an offset into the block.
 */
static long ch_block;
static int ch_offset;

/* Length of file, needed if input is a pipe. */
static off_t ch_fsize;

/* Number of bytes read, if input is standard input (a pipe). */
static off_t last_piped_pos;

/*
 * Get the character pointed to by the read pointer.  ch_get() is a macro
 * which is more efficient to call than fch_get (the function), in the usual
 * case that the block desired is at the head of the chain.
 */
#define	ch_get() \
	((buf_head->block == ch_block && \
	    ch_offset < buf_head->datasize) ? \
	    (unsigned char)buf_head->data[ch_offset] : fch_get())

static
fch_get()
{
	extern int bs_mode;
	register struct buf *bp;
	register int n, ch;
	register char *p, *t;
	off_t pos, lseek();

	/* look for a buffer holding the desired block. */
	for (bp = buf_head;  bp != END_OF_CHAIN;  bp = bp->next)
		if (bp->block == ch_block) {
			if (ch_offset >= bp->datasize)
				/*
				 * Need more data in this buffer.
				 */
				goto read_more;
			/*
			 * On a pipe, we don't sort the buffers LRU
			 * because this can cause gaps in the buffers.
			 * For example, suppose we've got 12 1K buffers,
			 * and a 15K input stream.  If we read the first 12K
			 * sequentially, then jump to line 1, then jump to
			 * the end, the buffers have blocks 0,4,5,6,..,14.
			 * If we then jump to line 1 again and try to
			 * read sequentially, we're out of luck when we
			 * get to block 1 (we'd get the "pipe error" below).
			 * To avoid this, we only sort buffers on a pipe
			 * when we actually READ the data, not when we
			 * find it already buffered.
			 */
			if (ispipe)
				return((unsigned char)bp->data[ch_offset]);
			goto found;
		}
	/*
	 * Block is not in a buffer.  Take the least recently used buffer
	 * and read the desired block into it.  If the LRU buffer has data
	 * in it, and input is a pipe, then try to allocate a new buffer first.
	 */
	if (ispipe && buf_tail->block != (long)(-1))
		(void)ch_addbuf(1);
	bp = buf_tail;
	bp->block = ch_block;
	bp->datasize = 0;

read_more:
	pos = (ch_block * BUFSIZ) + bp->datasize;
	if (ispipe) {
		/*
		 * The data requested should be immediately after
		 * the last data read from the pipe.
		 */
		if (pos != last_piped_pos) {
			error("pipe error");
			quit();
		}
	} else
		(void)lseek(file, pos, L_SET);

	/*
	 * Read the block.
	 * If we read less than a full block, we just return the
	 * partial block and pick up the rest next time.
	 */
	n = iread(file, &bp->data[bp->datasize], BUFSIZ - bp->datasize);
	if (n == READ_INTR)
		return (EOI);
	if (n < 0) {
		error("read error");
		quit();
	}
	if (ispipe)
		last_piped_pos += n;

	p = &bp->data[bp->datasize];
	bp->datasize += n;

	/*
	 * Set an EOI marker in the buffered data itself.  Then ensure the
	 * data is "clean": there are no extra EOI chars in the data and
	 * that the "meta" bit (the 0200 bit) is reset in each char;
	 * also translate \r\n sequences to \n if -u flag not set.
	 */
	if (n == 0) {
		ch_fsize = pos;
		bp->data[bp->datasize++] = EOI;
	}

	if (bs_mode) {
		for (p = &bp->data[bp->datasize]; --n >= 0;) {
			*--p;
			if (*p == EOI)
				*p = 0200;
		}
	}
	else {
		for (t = p; --n >= 0; ++p) {
			ch = *p;
			if (ch == '\r' && n && p[1] == '\n') {
				++p;
				*t++ = '\n';
			}
			else
				*t++ = (ch == EOI) ? 0200 : ch;
		}
		if (p != t) {
			bp->datasize -= p - t;
			if (ispipe)
				last_piped_pos -= p - t;
		}
	}

found:
	if (buf_head != bp) {
		/*
		 * Move the buffer to the head of the buffer chain.
		 * This orders the buffer chain, most- to least-recently used.
		 */
		bp->next->prev = bp->prev;
		bp->prev->next = bp->next;

		bp->next = buf_head;
		bp->prev = END_OF_CHAIN;
		buf_head->prev = bp;
		buf_head = bp;
	}

	if (ch_offset >= bp->datasize)
		/*
		 * After all that, we still don't have enough data.
		 * Go back and try again.
		 */
		goto read_more;

	return((unsigned char)bp->data[ch_offset]);
}

/*
 * Determine if a specific block is currently in one of the buffers.
 */
static
buffered(block)
	long block;
{
	register struct buf *bp;

	for (bp = buf_head; bp != END_OF_CHAIN; bp = bp->next)
		if (bp->block == block)
			return(1);
	return(0);
}

/*
 * Seek to a specified position in the file.
 * Return 0 if successful, non-zero if can't seek there.
 */
ch_seek(pos)
	register off_t pos;
{
	long new_block;

	new_block = pos / BUFSIZ;
	if (!ispipe || pos == last_piped_pos || buffered(new_block)) {
		/*
		 * Set read pointer.
		 */
		ch_block = new_block;
		ch_offset = pos % BUFSIZ;
		return(0);
	}
	return(1);
}

/*
 * Seek to the end of the file.
 */
ch_end_seek()
{
	off_t ch_length();

	if (!ispipe)
		return(ch_seek(ch_length()));

	/*
	 * Do it the slow way: read till end of data.
	 */
	while (ch_forw_get() != EOI)
		if (sigs)
			return(1);
	return(0);
}

/*
 * Seek to the beginning of the file, or as close to it as we can get.
 * We may not be able to seek there if input is a pipe and the
 * beginning of the pipe is no longer buffered.
 */
ch_beg_seek()
{
	register struct buf *bp, *firstbp;

	/*
	 * Try a plain ch_seek first.
	 */
	if (ch_seek((off_t)0) == 0)
		return(0);

	/*
	 * Can't get to position 0.
	 * Look thru the buffers for the one closest to position 0.
	 */
	firstbp = bp = buf_head;
	if (bp == END_OF_CHAIN)
		return(1);
	while ((bp = bp->next) != END_OF_CHAIN)
		if (bp->block < firstbp->block)
			firstbp = bp;
	ch_block = firstbp->block;
	ch_offset = 0;
	return(0);
}

/*
 * Return the length of the file, if known.
 */
off_t
ch_length()
{
	off_t lseek();

	if (ispipe)
		return(ch_fsize);
	return((off_t)(lseek(file, (off_t)0, L_XTND)));
}

/*
 * Return the current position in the file.
 */
off_t
ch_tell()
{
	return(ch_block * BUFSIZ + ch_offset);
}

/*
 * Get the current char and post-increment the read pointer.
 */
ch_forw_get()
{
	register int c;

	c = ch_get();
	if (c != EOI && ++ch_offset >= BUFSIZ) {
		ch_offset = 0;
		++ch_block;
	}
	return(c);
}

/*
 * Pre-decrement the read pointer and get the new current char.
 */
ch_back_get()
{
	if (--ch_offset < 0) {
		if (ch_block <= 0 || (ispipe && !buffered(ch_block-1))) {
			ch_offset = 0;
			return(EOI);
		}
		ch_offset = BUFSIZ - 1;
		ch_block--;
	}
	return(ch_get());
}

/*
 * Allocate buffers.
 * Caller wants us to have a total of at least want_nbufs buffers.
 * keep==1 means keep the data in the current buffers;
 * otherwise discard the old data.
 */
ch_init(want_nbufs, keep)
	int want_nbufs;
	int keep;
{
	register struct buf *bp;
	char message[80];

	cbufs = nbufs;
	if (nbufs < want_nbufs && ch_addbuf(want_nbufs - nbufs)) {
		/*
		 * Cannot allocate enough buffers.
		 * If we don't have ANY, then quit.
		 * Otherwise, just report the error and return.
		 */
		(void)snprintf(message, sizeof(message),
		    "cannot allocate %d buffers", want_nbufs - nbufs);
		error(message);
		if (nbufs == 0)
			quit();
		return;
	}

	if (keep)
		return;

	/*
	 * We don't want to keep the old data,
	 * so initialize all the buffers now.
	 */
	for (bp = buf_head;  bp != END_OF_CHAIN;  bp = bp->next)
		bp->block = (long)(-1);
	last_piped_pos = (off_t)0;
	ch_fsize = NULL_POSITION;
	(void)ch_seek((off_t)0);
}

/*
 * Allocate some new buffers.
 * The buffers are added to the tail of the buffer chain.
 */
ch_addbuf(nnew)
	int nnew;
{
	register struct buf *bp;
	register struct buf *newbufs;
	char *calloc();

	/*
	 * We don't have enough buffers.
	 * Allocate some new ones.
	 */
	newbufs = (struct buf *)calloc((u_int)nnew, sizeof(struct buf));
	if (newbufs == NULL)
		return(1);

	/*
	 * Initialize the new buffers and link them together.
	 * Link them all onto the tail of the buffer list.
	 */
	nbufs += nnew;
	cbufs = nbufs;
	for (bp = &newbufs[0];  bp < &newbufs[nnew];  bp++) {
		bp->next = bp + 1;
		bp->prev = bp - 1;
		bp->block = (long)(-1);
	}
	newbufs[nnew-1].next = END_OF_CHAIN;
	newbufs[0].prev = buf_tail;
	buf_tail->next = &newbufs[0];
	buf_tail = &newbufs[nnew-1];
	return(0);
}
