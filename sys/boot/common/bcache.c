/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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
 * $FreeBSD: src/sys/boot/common/bcache.c,v 1.6.2.1 2000/03/16 18:13:36 jhb Exp $
 */

/*
 * Simple LRU block cache
 */

#include <stand.h>
#include <string.h>
#include <bitstring.h>

#include "bootstrap.h"

/* #define BCACHE_DEBUG */

#ifdef BCACHE_DEBUG
#define BCACHE_TIMEOUT	10
# define DEBUG(fmt, args...)	printf("%s: " fmt "\n" , __FUNCTION__ , ## args)
#else
#define BCACHE_TIMEOUT	2
# define DEBUG(fmt, args...)
#endif


struct bcachectl
{
    daddr_t	bc_blkno;
    time_t	bc_stamp;
    int		bc_count;
};

static struct bcachectl	*bcache_ctl;
static caddr_t		bcache_data;
static bitstr_t		*bcache_miss;
static int		bcache_nblks;
static int		bcache_blksize;
static int		bcache_hits, bcache_misses, bcache_ops, bcache_bypasses;
static int		bcache_flushes;
static int		bcache_bcount;

static void	bcache_insert(caddr_t buf, daddr_t blkno);
static int	bcache_lookup(caddr_t buf, daddr_t blkno);

/*
 * Initialise the cache for (nblks) of (bsize).
 */
int
bcache_init(int nblks, size_t bsize)
{
    /* discard any old contents */
    if (bcache_data != NULL) {
	free(bcache_data);
	bcache_data = NULL;
	free(bcache_ctl);
    }

    /* Allocate control structures */
    bcache_nblks = nblks;
    bcache_blksize = bsize;
    bcache_data = malloc(bcache_nblks * bcache_blksize);
    bcache_ctl = (struct bcachectl *)malloc(bcache_nblks * sizeof(struct bcachectl));
    bcache_miss = bit_alloc((bcache_nblks + 1) / 2);
    if ((bcache_data == NULL) || (bcache_ctl == NULL) || (bcache_miss == NULL)) {
	if (bcache_miss)
	    free(bcache_miss);
	if (bcache_ctl)
	    free(bcache_ctl);
	if (bcache_data)
	    free(bcache_data);
	bcache_data = NULL;
	return(ENOMEM);
    }

    return(0);
}

/*
 * Flush the cache
 */
void
bcache_flush()
{
    int		i;

    bcache_flushes++;

    /* Flush the cache */
    for (i = 0; i < bcache_nblks; i++) {
	bcache_ctl[i].bc_count = -1;
	bcache_ctl[i].bc_blkno = -1;
    }
}

/* 
 * Handle a transfer request; fill in parts of the request that can
 * be satisfied by the cache, use the supplied strategy routine to do
 * device I/O and then use the I/O results to populate the cache. 
 *
 * Requests larger than 1/2 the cache size will be bypassed and go
 * directly to the disk.  XXX tune this.
 */
int
bcache_strategy(void *devdata, int unit, int rw, daddr_t blk, size_t size, void *buf, size_t *rsize)
{
    static int			bcache_unit = -1;
    struct bcache_devdata	*dd = (struct bcache_devdata *)devdata;
    int				nblk, p_size;
    daddr_t			p_blk;
    caddr_t			p_buf;
    int				i, j, result;

    bcache_ops++;

    if(bcache_unit != unit) {
	bcache_flush();
	bcache_unit = unit;
    }

    /* bypass large requests, or when the cache is inactive */
    if ((bcache_data == NULL) || ((size * 2 / bcache_blksize) > bcache_nblks)) {
	DEBUG("bypass %d from %d", size / bcache_blksize, blk);
	bcache_bypasses++;
	return(dd->dv_strategy(dd->dv_devdata, rw, blk, size, buf, rsize));
    }

    nblk = size / bcache_blksize;
    result = 0;

    /* Satisfy any cache hits up front */
    for (i = 0; i < nblk; i++) {
	if (bcache_lookup(buf + (bcache_blksize * i), blk + i)) {
	    bit_set(bcache_miss, i);	/* cache miss */
	    bcache_misses++;
	} else {
	    bit_clear(bcache_miss, i);	/* cache hit */
	    bcache_hits++;
	}
    }

    /* Go back and fill in any misses  XXX optimise */
    p_blk = -1;
    p_buf = NULL;
    p_size = 0;
    for (i = 0; i < nblk; i++) {
	if (bit_test(bcache_miss, i)) {
	    /* miss, add to pending transfer */
	    if (p_blk == -1) {
		p_blk = blk + i;
		p_buf = buf + (bcache_blksize * i);
		p_size = 1;
	    } else {
		p_size++;
	    }
	} else if (p_blk != -1) {
	    /* hit, complete pending transfer */
	    result = dd->dv_strategy(dd->dv_devdata, rw, p_blk, p_size * bcache_blksize, p_buf, NULL);
	    if (result != 0)
		goto done;
	    for (j = 0; j < p_size; j++)
		bcache_insert(p_buf + (j * bcache_blksize), p_blk + j);
	    p_blk = -1;
	}
    }
    if (p_blk != -1) {
	/* pending transfer left */
	result = dd->dv_strategy(dd->dv_devdata, rw, p_blk, p_size * bcache_blksize, p_buf, NULL);
	if (result != 0)
	    goto done;
	for (j = 0; j < p_size; j++)
	    bcache_insert(p_buf + (j * bcache_blksize), p_blk + j);
    }
    
 done:
    if ((result == 0) && (rsize != NULL))
	*rsize = size;
    return(result);
}


/*
 * Insert a block into the cache.  Retire the oldest block to do so, if required.
 *
 * XXX the LRU algorithm will fail after 2^31 blocks have been transferred.
 */
static void
bcache_insert(caddr_t buf, daddr_t blkno) 
{
    time_t	now;
    int		i, cand, ocount;
    
    time(&now);
    cand = 0;				/* assume the first block */
    ocount = bcache_ctl[0].bc_count;

    /* find the oldest block */
    for (i = 1; i < bcache_nblks; i++) {
	if (bcache_ctl[i].bc_blkno == blkno) {
	    /* reuse old entry */
	    cand = i;
	    break;
	}
	if (bcache_ctl[i].bc_count < ocount) {
	    ocount = bcache_ctl[i].bc_count;
	    cand = i;
	}
    }
    
    DEBUG("insert blk %d -> %d @ %d # %d", blkno, cand, now, bcache_bcount);
    bcopy(buf, bcache_data + (bcache_blksize * cand), bcache_blksize);
    bcache_ctl[cand].bc_blkno = blkno;
    bcache_ctl[cand].bc_stamp = now;
    bcache_ctl[cand].bc_count = bcache_bcount++;
}

/*
 * Look for a block in the cache.  Blocks more than BCACHE_TIMEOUT seconds old
 * may be stale (removable media) and thus are discarded.  Copy the block out 
 * if successful and return zero, or return nonzero on failure.
 */
static int
bcache_lookup(caddr_t buf, daddr_t blkno)
{
    time_t	now;
    int		i;
    
    time(&now);

    for (i = 0; i < bcache_nblks; i++)
	/* cache hit? */
	if ((bcache_ctl[i].bc_blkno == blkno) && ((bcache_ctl[i].bc_stamp + BCACHE_TIMEOUT) >= now)) {
	    bcopy(bcache_data + (bcache_blksize * i), buf, bcache_blksize);
	    DEBUG("hit blk %d <- %d (now %d then %d)", blkno, i, now, bcache_ctl[i].bc_stamp);
	    return(0);
	}
    return(ENOENT);
}

COMMAND_SET(bcachestat, "bcachestat", "get disk block cache stats", command_bcache);

static int
command_bcache(int argc, char *argv[])
{
    int		i;
    
    for (i = 0; i < bcache_nblks; i++) {
	printf("%08x %04x %04x|", bcache_ctl[i].bc_blkno, (unsigned int)bcache_ctl[i].bc_stamp & 0xffff, bcache_ctl[i].bc_count & 0xffff);
	if (((i + 1) % 4) == 0)
	    printf("\n");
    }
    printf("\n%d ops  %d bypasses  %d hits  %d misses  %d flushes\n", bcache_ops, bcache_bypasses, bcache_hits, bcache_misses, bcache_flushes);
    return(CMD_OK);
}

