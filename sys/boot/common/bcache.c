/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * Copyright 2015 Toomas Soome <tsoome@me.com>
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
 */

#include <sys/cdefs.h>
#include <sys/param.h>
__FBSDID("$FreeBSD$");

/*
 * Simple hashed block cache
 */

#include <sys/stdint.h>

#include <stand.h>
#include <string.h>
#include <strings.h>

#include "bootstrap.h"

/* #define BCACHE_DEBUG */

#ifdef BCACHE_DEBUG
# define DEBUG(fmt, args...)	printf("%s: " fmt "\n" , __func__ , ## args)
#else
# define DEBUG(fmt, args...)
#endif

struct bcachectl
{
    daddr_t	bc_blkno;
    int		bc_count;
};

/*
 * bcache per device node. cache is allocated on device first open and freed
 * on last close, to save memory. The issue there is the size; biosdisk
 * supports up to 31 (0x1f) devices. Classic setup would use single disk
 * to boot from, but this has changed with zfs.
 */
struct bcache {
    struct bcachectl	*bcache_ctl;
    caddr_t		bcache_data;
    u_int		bcache_nblks;
    size_t		ra;
};

static u_int bcache_total_nblks;	/* set by bcache_init */
static u_int bcache_blksize;		/* set by bcache_init */
static u_int bcache_numdev;		/* set by bcache_add_dev */
/* statistics */
static u_int bcache_units;	/* number of devices with cache */
static u_int bcache_unit_nblks;	/* nblocks per unit */
static u_int bcache_hits;
static u_int bcache_misses;
static u_int bcache_ops;
static u_int bcache_bypasses;
static u_int bcache_bcount;
static u_int bcache_rablks;

#define	BHASH(bc, blkno)	((blkno) & ((bc)->bcache_nblks - 1))
#define	BCACHE_LOOKUP(bc, blkno)	\
	((bc)->bcache_ctl[BHASH((bc), (blkno))].bc_blkno != (blkno))
#define	BCACHE_READAHEAD	256
#define	BCACHE_MINREADAHEAD	32

static void	bcache_invalidate(struct bcache *bc, daddr_t blkno);
static void	bcache_insert(struct bcache *bc, daddr_t blkno);
static void	bcache_free_instance(struct bcache *bc);

/*
 * Initialise the cache for (nblks) of (bsize).
 */
void
bcache_init(u_int nblks, size_t bsize)
{
    /* set up control data */
    bcache_total_nblks = nblks;
    bcache_blksize = bsize;
}

/*
 * add number of devices to bcache. we have to divide cache space
 * between the devices, so bcache_add_dev() can be used to set up the
 * number. The issue is, we need to get the number before actual allocations.
 * bcache_add_dev() is supposed to be called from device init() call, so the
 * assumption is, devsw dv_init is called for plain devices first, and
 * for zfs, last.
 */
void
bcache_add_dev(int devices)
{
    bcache_numdev += devices;
}

void *
bcache_allocate(void)
{
    u_int i;
    struct bcache *bc = malloc(sizeof (struct bcache));
    int disks = bcache_numdev;

    if (disks == 0)
	disks = 1;	/* safe guard */

    if (bc == NULL) {
	errno = ENOMEM;
	return (bc);
    }

    /*
     * the bcache block count must be power of 2 for hash function
     */
    i = fls(disks) - 1;		/* highbit - 1 */
    if (disks > (1 << i))	/* next power of 2 */
	i++;

    bc->bcache_nblks = bcache_total_nblks >> i;
    bcache_unit_nblks = bc->bcache_nblks;
    bc->bcache_data = malloc(bc->bcache_nblks * bcache_blksize);
    if (bc->bcache_data == NULL) {
	/* dont error out yet. fall back to 32 blocks and try again */
	bc->bcache_nblks = 32;
	bc->bcache_data = malloc(bc->bcache_nblks * bcache_blksize);
    }

    bc->bcache_ctl = malloc(bc->bcache_nblks * sizeof(struct bcachectl));

    if ((bc->bcache_data == NULL) || (bc->bcache_ctl == NULL)) {
	bcache_free_instance(bc);
	errno = ENOMEM;
	return(NULL);
    }

    /* Flush the cache */
    for (i = 0; i < bc->bcache_nblks; i++) {
	bc->bcache_ctl[i].bc_count = -1;
	bc->bcache_ctl[i].bc_blkno = -1;
    }
    bcache_units++;
    bc->ra = BCACHE_READAHEAD;	/* optimistic read ahead */
    return (bc);
}

void
bcache_free(void *cache)
{
    struct bcache *bc = cache;

    if (bc == NULL)
	return;

    bcache_free_instance(bc);
    bcache_units--;
}

/*
 * Handle a write request; write directly to the disk, and populate the
 * cache with the new values.
 */
static int
write_strategy(void *devdata, int rw, daddr_t blk, size_t offset,
    size_t size, char *buf, size_t *rsize)
{
    struct bcache_devdata	*dd = (struct bcache_devdata *)devdata;
    struct bcache		*bc = dd->dv_cache;
    daddr_t			i, nblk;

    nblk = size / bcache_blksize;

    /* Invalidate the blocks being written */
    for (i = 0; i < nblk; i++) {
	bcache_invalidate(bc, blk + i);
    }

    /* Write the blocks */
    return (dd->dv_strategy(dd->dv_devdata, rw, blk, offset, size, buf, rsize));
}

/*
 * Handle a read request; fill in parts of the request that can
 * be satisfied by the cache, use the supplied strategy routine to do
 * device I/O and then use the I/O results to populate the cache. 
 */
static int
read_strategy(void *devdata, int rw, daddr_t blk, size_t offset,
    size_t size, char *buf, size_t *rsize)
{
    struct bcache_devdata	*dd = (struct bcache_devdata *)devdata;
    struct bcache		*bc = dd->dv_cache;
    size_t			i, nblk, p_size, r_size, complete, ra;
    int				result;
    daddr_t			p_blk;
    caddr_t			p_buf;

    if (bc == NULL) {
	errno = ENODEV;
	return (-1);
    }

    if (rsize != NULL)
	*rsize = 0;

    nblk = size / bcache_blksize;
    if ((nblk == 0 && size != 0) || offset != 0)
	nblk++;
    result = 0;
    complete = 1;

    /* Satisfy any cache hits up front, break on first miss */
    for (i = 0; i < nblk; i++) {
	if (BCACHE_LOOKUP(bc, (daddr_t)(blk + i))) {
	    bcache_misses += (nblk - i);
	    complete = 0;
	    if (nblk - i > BCACHE_MINREADAHEAD && bc->ra > BCACHE_MINREADAHEAD)
		bc->ra >>= 1;	/* reduce read ahead */
	    break;
	} else {
	    bcache_hits++;
	}
    }

   if (complete) {	/* whole set was in cache, return it */
	if (bc->ra < BCACHE_READAHEAD)
		bc->ra <<= 1;	/* increase read ahead */
	bcopy(bc->bcache_data + (bcache_blksize * BHASH(bc, blk)) + offset,
	    buf, size);
	goto done;
   }

    /*
     * Fill in any misses. From check we have i pointing to first missing
     * block, read in all remaining blocks + readahead.
     * We have space at least for nblk - i before bcache wraps.
     */
    p_blk = blk + i;
    p_buf = bc->bcache_data + (bcache_blksize * BHASH(bc, p_blk));
    r_size = bc->bcache_nblks - BHASH(bc, p_blk); /* remaining blocks */

    p_size = MIN(r_size, nblk - i);	/* read at least those blocks */

    ra = bc->bcache_nblks - BHASH(bc, p_blk + p_size);
    if (ra != bc->bcache_nblks) { /* do we have RA space? */
	ra = MIN(bc->ra, ra);
	p_size += ra;
    }

    /* invalidate bcache */
    for (i = 0; i < p_size; i++) {
	bcache_invalidate(bc, p_blk + i);
    }

    r_size = 0;
    /*
     * with read-ahead, it may happen we are attempting to read past
     * disk end, as bcache has no information about disk size.
     * in such case we should get partial read if some blocks can be
     * read or error, if no blocks can be read.
     * in either case we should return the data in bcache and only
     * return error if there is no data.
     */
    result = dd->dv_strategy(dd->dv_devdata, rw, p_blk, 0,
	p_size * bcache_blksize, p_buf, &r_size);

    r_size /= bcache_blksize;
    for (i = 0; i < r_size; i++)
	bcache_insert(bc, p_blk + i);

    /* update ra statistics */
    if (r_size != 0) {
	if (r_size < p_size)
	    bcache_rablks += (p_size - r_size);
	else
	    bcache_rablks += ra;
    }

    /* check how much data can we copy */
    for (i = 0; i < nblk; i++) {
	if (BCACHE_LOOKUP(bc, (daddr_t)(blk + i)))
	    break;
    }

    if (size > i * bcache_blksize)
	size = i * bcache_blksize;

    if (size != 0) {
	bcopy(bc->bcache_data + (bcache_blksize * BHASH(bc, blk)) + offset,
	    buf, size);
	result = 0;
    }

 done:
    if ((result == 0) && (rsize != NULL))
	*rsize = size;
    return(result);
}

/* 
 * Requests larger than 1/2 cache size will be bypassed and go
 * directly to the disk.  XXX tune this.
 */
int
bcache_strategy(void *devdata, int rw, daddr_t blk, size_t offset,
    size_t size, char *buf, size_t *rsize)
{
    struct bcache_devdata	*dd = (struct bcache_devdata *)devdata;
    struct bcache		*bc = dd->dv_cache;
    u_int bcache_nblks = 0;
    int nblk, cblk, ret;
    size_t csize, isize, total;

    bcache_ops++;

    if (bc != NULL)
	bcache_nblks = bc->bcache_nblks;

    /* bypass large requests, or when the cache is inactive */
    if (bc == NULL ||
	(offset == 0 && ((size * 2 / bcache_blksize) > bcache_nblks))) {
	DEBUG("bypass %d from %d", size / bcache_blksize, blk);
	bcache_bypasses++;
	return (dd->dv_strategy(dd->dv_devdata, rw, blk, offset, size, buf,
	    rsize));
    }

    /* normalize offset */
    while (offset >= bcache_blksize) {
	blk++;
	offset -= bcache_blksize;
    }

    switch (rw) {
    case F_READ:
	nblk = size / bcache_blksize;
	if (offset || (size != 0 && nblk == 0))
	    nblk++;	/* read at least one block */

	ret = 0;
	total = 0;
	while(size) {
	    cblk = bcache_nblks - BHASH(bc, blk); /* # of blocks left */
	    cblk = MIN(cblk, nblk);

	    if (size <= bcache_blksize)
		csize = size;
	    else {
		csize = cblk * bcache_blksize;
		if (offset)
		    csize -= (bcache_blksize - offset);
	    }

	    ret = read_strategy(devdata, rw, blk, offset,
		csize, buf+total, &isize);

	    /*
	     * we may have error from read ahead, if we have read some data
	     * return partial read.
	     */
	    if (ret != 0 || isize == 0) {
		if (total != 0)
		    ret = 0;
		break;
	    }
	    blk += (offset+isize) / bcache_blksize;
	    offset = 0;
	    total += isize;
	    size -= isize;
	    nblk = size / bcache_blksize;
	}

	if (rsize)
	    *rsize = total;

	return (ret);
    case F_WRITE:
	return write_strategy(devdata, rw, blk, offset, size, buf, rsize);
    }
    return -1;
}

/*
 * Free allocated bcache instance
 */
static void
bcache_free_instance(struct bcache *bc)
{
    if (bc != NULL) {
	if (bc->bcache_ctl)
	    free(bc->bcache_ctl);
	if (bc->bcache_data)
	    free(bc->bcache_data);
	free(bc);
    }
}

/*
 * Insert a block into the cache.
 */
static void
bcache_insert(struct bcache *bc, daddr_t blkno)
{
    u_int	cand;
    
    cand = BHASH(bc, blkno);

    DEBUG("insert blk %llu -> %u # %d", blkno, cand, bcache_bcount);
    bc->bcache_ctl[cand].bc_blkno = blkno;
    bc->bcache_ctl[cand].bc_count = bcache_bcount++;
}

/*
 * Invalidate a block from the cache.
 */
static void
bcache_invalidate(struct bcache *bc, daddr_t blkno)
{
    u_int	i;
    
    i = BHASH(bc, blkno);
    if (bc->bcache_ctl[i].bc_blkno == blkno) {
	bc->bcache_ctl[i].bc_count = -1;
	bc->bcache_ctl[i].bc_blkno = -1;
	DEBUG("invalidate blk %llu", blkno);
    }
}

#ifndef BOOT2
COMMAND_SET(bcachestat, "bcachestat", "get disk block cache stats", command_bcache);

static int
command_bcache(int argc, char *argv[])
{
    if (argc != 1) {
	command_errmsg = "wrong number of arguments";
	return(CMD_ERROR);
    }

    printf("\ncache blocks: %d\n", bcache_total_nblks);
    printf("cache blocksz: %d\n", bcache_blksize);
    printf("cache readahead: %d\n", bcache_rablks);
    printf("unit cache blocks: %d\n", bcache_unit_nblks);
    printf("cached units: %d\n", bcache_units);
    printf("%d ops  %d bypasses  %d hits  %d misses\n", bcache_ops,
	bcache_bypasses, bcache_hits, bcache_misses);
    return(CMD_OK);
}
#endif
