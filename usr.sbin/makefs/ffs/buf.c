/*	$NetBSD: buf.c,v 1.12 2004/06/20 22:20:18 jmc Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Luke Mewburn for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/time.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "makefs.h"

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include "ffs/buf.h"
#include "ffs/ufs_inode.h"

extern int sectorsize;		/* XXX: from ffs.c & mkfs.c */

TAILQ_HEAD(buftailhead,buf) buftail;

int
bread(int fd, struct fs *fs, daddr_t blkno, int size, struct buf **bpp)
{
	off_t	offset;
	ssize_t	rv;

	assert (fs != NULL);
	assert (bpp != NULL);

	if (debug & DEBUG_BUF_BREAD)
		printf("bread: fs %p blkno %lld size %d\n",
		    fs, (long long)blkno, size);
	*bpp = getblk(fd, fs, blkno, size);
	offset = (*bpp)->b_blkno * sectorsize;	/* XXX */
	if (debug & DEBUG_BUF_BREAD)
		printf("bread: bp %p blkno %lld offset %lld bcount %ld\n",
		    (*bpp), (long long)(*bpp)->b_blkno, (long long) offset,
		    (*bpp)->b_bcount);
	if (lseek((*bpp)->b_fd, offset, SEEK_SET) == -1)
		err(1, "bread: lseek %lld (%lld)",
		    (long long)(*bpp)->b_blkno, (long long)offset);
	rv = read((*bpp)->b_fd, (*bpp)->b_data, (*bpp)->b_bcount);
	if (debug & DEBUG_BUF_BREAD)
		printf("bread: read %ld (%lld) returned %d\n",
		    (*bpp)->b_bcount, (long long)offset, (int)rv);
	if (rv == -1)				/* read error */
		err(1, "bread: read %ld (%lld) returned %d",
		    (*bpp)->b_bcount, (long long)offset, (int)rv);
	else if (rv != (*bpp)->b_bcount)	/* short read */
		err(1, "bread: read %ld (%lld) returned %d",
		    (*bpp)->b_bcount, (long long)offset, (int)rv);
	else
		return (0);
}

void
brelse(struct buf *bp)
{

	assert (bp != NULL);
	assert (bp->b_data != NULL);

	if (bp->b_lblkno < 0) {
		/*
		 * XXX	don't remove any buffers with negative logical block
		 *	numbers (lblkno), so that we retain the mapping
		 *	of negative lblkno -> real blkno that ffs_balloc()
		 *	sets up.
		 *
		 *	if we instead released these buffers, and implemented
		 *	ufs_strategy() (and ufs_bmaparray()) and called those
		 *	from bread() and bwrite() to convert the lblkno to
		 *	a real blkno, we'd add a lot more code & complexity
		 *	and reading off disk, for little gain, because this
		 *	simple hack works for our purpose.
		 */
		bp->b_bcount = 0;
		return;
	}

	TAILQ_REMOVE(&buftail, bp, b_tailq);
	free(bp->b_data);
	free(bp);
}

int
bwrite(struct buf *bp)
{
	off_t	offset;
	ssize_t	rv;

	assert (bp != NULL);
	offset = bp->b_blkno * sectorsize;	/* XXX */
	if (debug & DEBUG_BUF_BWRITE)
		printf("bwrite: bp %p blkno %lld offset %lld bcount %ld\n",
		    bp, (long long)bp->b_blkno, (long long) offset,
		    bp->b_bcount);
	if (lseek(bp->b_fd, offset, SEEK_SET) == -1)
		return (errno);
	rv = write(bp->b_fd, bp->b_data, bp->b_bcount);
	if (debug & DEBUG_BUF_BWRITE)
		printf("bwrite: write %ld (offset %lld) returned %lld\n",
		    bp->b_bcount, (long long)offset, (long long)rv);
	if (rv == bp->b_bcount)
		return (0);
	else if (rv == -1)		/* write error */
		return (errno);
	else				/* short write ? */
		return (EAGAIN);
}

void
bcleanup(void)
{
	struct buf *bp;

	/*
	 * XXX	this really shouldn't be necessary, but i'm curious to
	 *	know why there's still some buffers lying around that
	 *	aren't brelse()d
	 */

	if (TAILQ_EMPTY(&buftail))
		return;

	printf("bcleanup: unflushed buffers:\n");
	TAILQ_FOREACH(bp, &buftail, b_tailq) {
		printf("\tlblkno %10lld  blkno %10lld  count %6ld  bufsize %6ld\n",
		    (long long)bp->b_lblkno, (long long)bp->b_blkno,
		    bp->b_bcount, bp->b_bufsize);
	}
	printf("bcleanup: done\n");
}

struct buf *
getblk(int fd, struct fs *fs, daddr_t blkno, int size)
{
	static int buftailinitted;
	struct buf *bp;
	void *n;

	assert (fs != NULL);
	if (debug & DEBUG_BUF_GETBLK)
		printf("getblk: fs %p blkno %lld size %d\n", fs,
		    (long long)blkno, size);

	bp = NULL;
	if (!buftailinitted) {
		if (debug & DEBUG_BUF_GETBLK)
			printf("getblk: initialising tailq\n");
		TAILQ_INIT(&buftail);
		buftailinitted = 1;
	} else {
		TAILQ_FOREACH(bp, &buftail, b_tailq) {
			if (bp->b_lblkno != blkno)
				continue;
			break;
		}
	}
	if (bp == NULL) {
		if ((bp = calloc(1, sizeof(struct buf))) == NULL)
			err(1, "getblk: calloc");

		bp->b_bufsize = 0;
		bp->b_blkno = bp->b_lblkno = blkno;
		bp->b_fd = fd;
		bp->b_fs = fs;
		bp->b_data = NULL;
		TAILQ_INSERT_HEAD(&buftail, bp, b_tailq);
	}
	bp->b_bcount = size;
	if (bp->b_data == NULL || bp->b_bcount > bp->b_bufsize) {
		n = realloc(bp->b_data, size);
		if (n == NULL)
			err(1, "getblk: realloc b_data %ld", bp->b_bcount);
		bp->b_data = n;
		bp->b_bufsize = size;
	}

	return (bp);
}
