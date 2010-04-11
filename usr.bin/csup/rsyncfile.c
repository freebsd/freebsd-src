/*-
 * Copyright (c) 2008-2009, Ulf Lilleengen <lulf@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "misc.h"
#include "rsyncfile.h"

#define MINBLOCKSIZE 1024
#define MAXBLOCKSIZE (16 * 1024)
#define RECEIVEBUFFERSIZE (15 * 1024)
#define BLOCKINFOSIZE 26
#define SEARCHREGION 10
#define MAXBLOCKS (RECEIVEBUFFERSIZE / BLOCKINFOSIZE)

#define CHAR_OFFSET 3
#define RSUM_SIZE 9

struct rsyncfile {
	char *start;
	char *buf;
	char *end;
	size_t blocksize;
	size_t fsize;
	int fd;

	char *blockptr;
	int blocknum;
	char blockmd5[MD5_DIGEST_SIZE];
	char rsumstr[RSUM_SIZE];
	uint32_t rsum;
};

static size_t		rsync_chooseblocksize(size_t);
static uint32_t		rsync_rollsum(char *, size_t);

/* Open a file and initialize variable for rsync operation. */
struct rsyncfile *
rsync_open(char *path, size_t blocksize, int rdonly)
{
	struct rsyncfile *rf;
	struct stat st;
	int error;

	rf = xmalloc(sizeof(*rf));
	error = stat(path, &st);
	if (error) {
		free(rf);
		return (NULL);
	}
	rf->fsize = st.st_size;

	rf->fd = open(path, rdonly ? O_RDONLY : O_RDWR);
	if (rf->fd < 0) {
		free(rf);
		return (NULL);
	}
	rf->buf = mmap(0, rf->fsize, PROT_READ, MAP_SHARED, rf->fd, 0);
	if (rf->buf == MAP_FAILED) {
		free(rf);
		return (NULL);
	}
	rf->start = rf->buf;
	rf->end = rf->buf + rf->fsize;
	rf->blocksize = (blocksize == 0 ? rsync_chooseblocksize(rf->fsize) :
	    blocksize);
	rf->blockptr = rf->buf;
	rf->blocknum = 0;
	return (rf);
}

/* Close and free all resources related to an rsync file transfer. */
int
rsync_close(struct rsyncfile *rf)
{
	int error;

	error = munmap(rf->buf, rf->fsize);
	if (error)
		return (error);
	close(rf->fd);
	free(rf);
	return (0);
}

/*
 * Choose the most appropriate block size for an rsync transfer. Modeled
 * algorithm after cvsup.
 */
static size_t
rsync_chooseblocksize(size_t fsize)
{
	size_t bestrem, blocksize, bs, hisearch, losearch, rem;

	blocksize = fsize / MAXBLOCKS;
	losearch = blocksize - SEARCHREGION;
	hisearch = blocksize + SEARCHREGION;

	if (losearch < MINBLOCKSIZE) {
		losearch = MINBLOCKSIZE;
		hisearch = losearch + (2 * SEARCHREGION);
	} else if (hisearch > MAXBLOCKSIZE) {
		hisearch = MAXBLOCKSIZE;
		losearch = hisearch - (2 * SEARCHREGION);
	}

	bestrem = MAXBLOCKSIZE;
	for (bs = losearch; bs <= hisearch; bs++) {
		rem = fsize % bs;
		if (rem < bestrem) {
			bestrem = rem;
			blocksize = bs;
		}
	}
	return (bestrem);
}

/* Get the next rsync block of a file. */
int
rsync_nextblock(struct rsyncfile *rf)
{
	MD5_CTX ctx;
	size_t blocksize;

	if (rf->blockptr >= rf->end)
		return (0);
	blocksize = min((size_t)(rf->end - rf->blockptr), rf->blocksize);
	/* Calculate MD5 of the block. */
	MD5_Init(&ctx);
	MD5_Update(&ctx, rf->blockptr, blocksize);
	MD5_End(rf->blockmd5, &ctx);

	rf->rsum = rsync_rollsum(rf->blockptr, blocksize);
	snprintf(rf->rsumstr, RSUM_SIZE, "%x", rf->rsum);
	rf->blocknum++;
	rf->blockptr += blocksize;
	return (1);
}

/* Get the rolling checksum of a file. */
static uint32_t
rsync_rollsum(char *buf, size_t len)
{
	uint32_t a, b;
	char *ptr, *limit;

	a = b = 0;
	ptr = buf;
	limit = buf + len;

	while (ptr < limit) {
		a += *ptr + CHAR_OFFSET;
		b += a;
		ptr++;
	}
	return ((b << 16) | a);
}

/* Get running sum so far. */
char *
rsync_rsum(struct rsyncfile *rf)
{

	return (rf->rsumstr);
}

/* Get MD5 of current block. */
char *
rsync_blockmd5(struct rsyncfile *rf)
{

	return (rf->blockmd5);
}

/* Accessor for blocksize. */
size_t
rsync_blocksize(struct rsyncfile *rf)
{

	return (rf->blocksize);
}

/* Accessor for filesize. */
size_t
rsync_filesize(struct rsyncfile *rf)
{

	return (rf->fsize);
}
