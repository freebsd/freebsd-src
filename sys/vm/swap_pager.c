/*
 * Copyright (c) 1990 University of Utah.
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	from: Utah $Hdr: swap_pager.c 1.4 91/04/30$
 *	from: @(#)swap_pager.c	7.4 (Berkeley) 5/7/91
 *	$Id: swap_pager.c,v 1.2 1993/10/16 16:20:19 rgrimes Exp $
 */

/*
 * Quick hack to page to dedicated partition(s).
 * TODO:
 *	Add multiprocessor locks
 *	Deal with async writes in a better fashion
 */

#include "swappager.h"
#if NSWAPPAGER > 0

#include "param.h"
#include "proc.h"
#include "buf.h"
#include "systm.h"
#include "specdev.h"
#include "vnode.h"
#include "malloc.h"
#include "queue.h"
#include "rlist.h"

#include "vm_param.h"
#include "queue.h"
#include "lock.h"
#include "vm_prot.h"
#include "vm_object.h"
#include "vm_page.h"
#include "vm_pageout.h"
#include "swap_pager.h"

#define NSWSIZES	16	/* size of swtab */
#define NPENDINGIO	64	/* max # of pending cleans */
#define MAXDADDRS	64	/* max # of disk addrs for fixed allocations */

#ifdef DEBUG
int	swpagerdebug = 0 /*0x100*/;
#define	SDB_FOLLOW	0x001
#define SDB_INIT	0x002
#define SDB_ALLOC	0x004
#define SDB_IO		0x008
#define SDB_WRITE	0x010
#define SDB_FAIL	0x020
#define SDB_ALLOCBLK	0x040
#define SDB_FULL	0x080
#define SDB_ANOM	0x100
#define SDB_ANOMPANIC	0x200
#endif

struct swpagerclean {
	queue_head_t		spc_list;
	int			spc_flags;
	struct buf		*spc_bp;
	sw_pager_t		spc_swp;
	vm_offset_t		spc_kva;
	vm_page_t		spc_m;
} swcleanlist[NPENDINGIO];
typedef	struct swpagerclean	*swp_clean_t;

/* spc_flags values */
#define SPC_FREE	0x00
#define SPC_BUSY	0x01
#define SPC_DONE	0x02
#define SPC_ERROR	0x04
#define SPC_DIRTY	0x08

struct swtab {
	vm_size_t st_osize;	/* size of object (bytes) */
	int	  st_bsize;	/* vs. size of swap block (DEV_BSIZE units) */
#ifdef DEBUG
	u_long	  st_inuse;	/* number in this range in use */
	u_long	  st_usecnt;	/* total used of this size */
#endif
} swtab[NSWSIZES+1];

#ifdef DEBUG
int		swap_pager_pendingio;	/* max pending async "clean" ops */
int		swap_pager_poip;	/* pageouts in progress */
int		swap_pager_piip;	/* pageins in progress */
#endif

queue_head_t	swap_pager_inuse;	/* list of pending page cleans */
queue_head_t	swap_pager_free;	/* list of free pager clean structs */
queue_head_t	swap_pager_list;	/* list of "named" anon regions */

void
swap_pager_init()
{
	register swp_clean_t spc;
	register int i, bsize;
	extern int dmmin, dmmax;
	int maxbsize;

#ifdef DEBUG
	if (swpagerdebug & (SDB_FOLLOW|SDB_INIT))
		printf("swpg_init()\n");
#endif
	dfltpagerops = &swappagerops;
	queue_init(&swap_pager_list);

	/*
	 * Initialize clean lists
	 */
	queue_init(&swap_pager_inuse);
	queue_init(&swap_pager_free);
	for (i = 0, spc = swcleanlist; i < NPENDINGIO; i++, spc++) {
		queue_enter(&swap_pager_free, spc, swp_clean_t, spc_list);
		spc->spc_flags = SPC_FREE;
	}

	/*
	 * Calculate the swap allocation constants.
	 */
        if (dmmin == 0) {
                dmmin = DMMIN;
		if (dmmin < CLBYTES/DEV_BSIZE)
			dmmin = CLBYTES/DEV_BSIZE;
	}
        if (dmmax == 0)
                dmmax = DMMAX;

	/*
	 * Fill in our table of object size vs. allocation size
	 */
	bsize = btodb(PAGE_SIZE);
	if (bsize < dmmin)
		bsize = dmmin;
	maxbsize = btodb(sizeof(sw_bm_t) * NBBY * PAGE_SIZE);
	if (maxbsize > dmmax)
		maxbsize = dmmax;
	for (i = 0; i < NSWSIZES; i++) {
		swtab[i].st_osize = (vm_size_t) (MAXDADDRS * dbtob(bsize));
		swtab[i].st_bsize = bsize;
#ifdef DEBUG
		if (swpagerdebug & SDB_INIT)
			printf("swpg_init: ix %d, size %x, bsize %x\n",
			       i, swtab[i].st_osize, swtab[i].st_bsize);
#endif
		if (bsize >= maxbsize)
			break;
		bsize *= 2;
	}
	swtab[i].st_osize = 0;
	swtab[i].st_bsize = bsize;
}

/*
 * Allocate a pager structure and associated resources.
 * Note that if we are called from the pageout daemon (handle == NULL)
 * we should not wait for memory as it could resulting in deadlock.
 */
vm_pager_t
swap_pager_alloc(handle, size, prot)
	caddr_t handle;
	register vm_size_t size;
	vm_prot_t prot;
{
	register vm_pager_t pager;
	register sw_pager_t swp;
	struct swtab *swt;
	int waitok;

#ifdef DEBUG
	if (swpagerdebug & (SDB_FOLLOW|SDB_ALLOC))
		printf("swpg_alloc(%x, %x, %x)\n", handle, size, prot);
#endif
	/*
	 * If this is a "named" anonymous region, look it up and
	 * return the appropriate pager if it exists.
	 */
	if (handle) {
		pager = vm_pager_lookup(&swap_pager_list, handle);
		if (pager != NULL) {
			/*
			 * Use vm_object_lookup to gain a reference
			 * to the object and also to remove from the
			 * object cache.
			 */
			if (vm_object_lookup(pager) == NULL)
				panic("swap_pager_alloc: bad object");
			return(pager);
		}
	}
	/*
	 * Pager doesn't exist, allocate swap management resources
	 * and initialize.
	 */
	waitok = handle ? M_WAITOK : M_NOWAIT;
	pager = (vm_pager_t)malloc(sizeof *pager, M_VMPAGER, waitok);
	if (pager == NULL)
		return(NULL);
	swp = (sw_pager_t)malloc(sizeof *swp, M_VMPGDATA, waitok);
	if (swp == NULL) {
#ifdef DEBUG
		if (swpagerdebug & SDB_FAIL)
			printf("swpg_alloc: swpager malloc failed\n");
#endif
		free((caddr_t)pager, M_VMPAGER);
		return(NULL);
	}
	size = round_page(size);
	for (swt = swtab; swt->st_osize; swt++)
		if (size <= swt->st_osize)
			break;
#ifdef DEBUG
	swt->st_inuse++;
	swt->st_usecnt++;
#endif
	swp->sw_osize = size;
	swp->sw_bsize = swt->st_bsize;
	swp->sw_nblocks = (btodb(size) + swp->sw_bsize - 1) / swp->sw_bsize;
	swp->sw_blocks = (sw_blk_t)
		malloc(swp->sw_nblocks*sizeof(*swp->sw_blocks),
		       M_VMPGDATA, M_NOWAIT);
	if (swp->sw_blocks == NULL) {
		free((caddr_t)swp, M_VMPGDATA);
		free((caddr_t)pager, M_VMPAGER);
#ifdef DEBUG
		if (swpagerdebug & SDB_FAIL)
			printf("swpg_alloc: sw_blocks malloc failed\n");
		swt->st_inuse--;
		swt->st_usecnt--;
#endif
		return(FALSE);
	}
	bzero((caddr_t)swp->sw_blocks,
	      swp->sw_nblocks * sizeof(*swp->sw_blocks));
	swp->sw_poip = 0;
	if (handle) {
		vm_object_t object;

		swp->sw_flags = SW_NAMED;
		queue_enter(&swap_pager_list, pager, vm_pager_t, pg_list);
		/*
		 * Consistant with other pagers: return with object
		 * referenced.  Can't do this with handle == NULL
		 * since it might be the pageout daemon calling.
		 */
		object = vm_object_allocate(size);
		vm_object_enter(object, pager);
		vm_object_setpager(object, pager, 0, FALSE);
	} else {
		swp->sw_flags = 0;
		queue_init(&pager->pg_list);
	}
	pager->pg_handle = handle;
	pager->pg_ops = &swappagerops;
	pager->pg_type = PG_SWAP;
	pager->pg_data = (caddr_t)swp;

#ifdef DEBUG
	if (swpagerdebug & SDB_ALLOC)
		printf("swpg_alloc: pg_data %x, %x of %x at %x\n",
		       swp, swp->sw_nblocks, swp->sw_bsize, swp->sw_blocks);
#endif
	return(pager);
}

void
swap_pager_dealloc(pager)
	vm_pager_t pager;
{
	register int i;
	register sw_blk_t bp;
	register sw_pager_t swp;
	struct swtab *swt;
	int s;

#ifdef DEBUG
	/* save panic time state */
	if ((swpagerdebug & SDB_ANOMPANIC) && panicstr)
		return;
	if (swpagerdebug & (SDB_FOLLOW|SDB_ALLOC))
		printf("swpg_dealloc(%x)\n", pager);
#endif
	/*
	 * Remove from list right away so lookups will fail if we
	 * block for pageout completion.
	 */
	swp = (sw_pager_t) pager->pg_data;
	if (swp->sw_flags & SW_NAMED) {
		queue_remove(&swap_pager_list, pager, vm_pager_t, pg_list);
		swp->sw_flags &= ~SW_NAMED;
	}
#ifdef DEBUG
	for (swt = swtab; swt->st_osize; swt++)
		if (swp->sw_osize <= swt->st_osize)
			break;
	swt->st_inuse--;
#endif

	/*
	 * Wait for all pageouts to finish and remove
	 * all entries from cleaning list.
	 */
	s = splbio();
	while (swp->sw_poip) {
		swp->sw_flags |= SW_WANTED;
		assert_wait((int)swp);
		thread_block();
	}
	splx(s);
	(void) swap_pager_clean(NULL, B_WRITE);

	/*
	 * Free left over swap blocks
	 */
	s = splbio();
	for (i = 0, bp = swp->sw_blocks; i < swp->sw_nblocks; i++, bp++)
		if (bp->swb_block) {
#ifdef DEBUG
			if (swpagerdebug & (SDB_ALLOCBLK|SDB_FULL))
				printf("swpg_dealloc: blk %x\n",
				       bp->swb_block);
#endif
			rlist_free(&swapmap, (unsigned)bp->swb_block,
				(unsigned)bp->swb_block + swp->sw_bsize - 1);
		}
	splx(s);
	/*
	 * Free swap management resources
	 */
	free((caddr_t)swp->sw_blocks, M_VMPGDATA);
	free((caddr_t)swp, M_VMPGDATA);
	free((caddr_t)pager, M_VMPAGER);
}

swap_pager_getpage(pager, m, sync)
	vm_pager_t pager;
	vm_page_t m;
	boolean_t sync;
{
#ifdef DEBUG
	if (swpagerdebug & SDB_FOLLOW)
		printf("swpg_getpage(%x, %x, %d)\n", pager, m, sync);
#endif
	return(swap_pager_io((sw_pager_t)pager->pg_data, m, B_READ));
}

swap_pager_putpage(pager, m, sync)
	vm_pager_t pager;
	vm_page_t m;
	boolean_t sync;
{
	int flags;

#ifdef DEBUG
	if (swpagerdebug & SDB_FOLLOW)
		printf("swpg_putpage(%x, %x, %d)\n", pager, m, sync);
#endif
	if (pager == NULL) {
		(void) swap_pager_clean(NULL, B_WRITE);
		return;
	}
	flags = B_WRITE;
	if (!sync)
		flags |= B_ASYNC;
	return(swap_pager_io((sw_pager_t)pager->pg_data, m, flags));
}

boolean_t
swap_pager_haspage(pager, offset)
	vm_pager_t pager;
	vm_offset_t offset;
{
	register sw_pager_t swp;
	register sw_blk_t swb;
	int ix;

#ifdef DEBUG
	if (swpagerdebug & (SDB_FOLLOW|SDB_ALLOCBLK))
		printf("swpg_haspage(%x, %x) ", pager, offset);
#endif
	swp = (sw_pager_t) pager->pg_data;
	ix = offset / dbtob(swp->sw_bsize);
	if (swp->sw_blocks == NULL || ix >= swp->sw_nblocks) {
#ifdef DEBUG
		if (swpagerdebug & (SDB_FAIL|SDB_FOLLOW|SDB_ALLOCBLK))
			printf("swpg_haspage: %x bad offset %x, ix %x\n",
			       swp->sw_blocks, offset, ix);
#endif
		return(FALSE);
	}
	swb = &swp->sw_blocks[ix];
	if (swb->swb_block)
		ix = atop(offset % dbtob(swp->sw_bsize));
#ifdef DEBUG
	if (swpagerdebug & SDB_ALLOCBLK)
		printf("%x blk %x+%x ", swp->sw_blocks, swb->swb_block, ix);
	if (swpagerdebug & (SDB_FOLLOW|SDB_ALLOCBLK))
		printf("-> %c\n",
		       "FT"[swb->swb_block && (swb->swb_mask & (1 << ix))]);
#endif
	if (swb->swb_block && (swb->swb_mask & (1 << ix)))
		return(TRUE);
	return(FALSE);
}

/*
 * Scaled down version of swap().
 * Assumes that PAGE_SIZE < MAXPHYS; i.e. only one operation needed.
 * BOGUS:  lower level IO routines expect a KVA so we have to map our
 * provided physical page into the KVA to keep them happy.
 */
swap_pager_io(swp, m, flags)
	register sw_pager_t swp;
	vm_page_t m;
	int flags;
{
	register struct buf *bp;
	register sw_blk_t swb;
	register int s;
	int ix;
	boolean_t rv;
	vm_offset_t kva, off;
	swp_clean_t spc;

#ifdef DEBUG
	/* save panic time state */
	if ((swpagerdebug & SDB_ANOMPANIC) && panicstr)
		return;
	if (swpagerdebug & (SDB_FOLLOW|SDB_IO))
		printf("swpg_io(%x, %x, %x)\n", swp, m, flags);
#endif

	/*
	 * For reads (pageins) and synchronous writes, we clean up
	 * all completed async pageouts.
	 */
	if ((flags & B_ASYNC) == 0) {
		s = splbio();
#ifdef DEBUG
		/*
		 * Check to see if this page is currently being cleaned.
		 * If it is, we just wait til the operation is done before
		 * continuing.
		 */
		while (swap_pager_clean(m, flags&B_READ)) {
			if (swpagerdebug & SDB_ANOM)
				printf("swap_pager_io: page %x cleaning\n", m);

			swp->sw_flags |= SW_WANTED;
			assert_wait((int)swp);
			thread_block();
		}
#else
		(void) swap_pager_clean(m, flags&B_READ);
#endif
		splx(s);
	}
	/*
	 * For async writes (pageouts), we cleanup completed pageouts so
	 * that all available resources are freed.  Also tells us if this
	 * page is already being cleaned.  If it is, or no resources
	 * are available, we try again later.
	 */
	else if (swap_pager_clean(m, B_WRITE) ||
		 queue_empty(&swap_pager_free)) {
#ifdef DEBUG
		if ((swpagerdebug & SDB_ANOM) &&
		    !queue_empty(&swap_pager_free))
			printf("swap_pager_io: page %x already cleaning\n", m);
#endif
		return(VM_PAGER_FAIL);
	}

	/*
	 * Determine swap block and allocate as necessary.
	 */
	off = m->offset + m->object->paging_offset;
	ix = off / dbtob(swp->sw_bsize);
	if (swp->sw_blocks == NULL || ix >= swp->sw_nblocks) {
#ifdef DEBUG
		if (swpagerdebug & SDB_FAIL)
			printf("swpg_io: bad offset %x+%x(%d) in %x\n",
			       m->offset, m->object->paging_offset,
			       ix, swp->sw_blocks);
#endif
		return(VM_PAGER_FAIL);
	}
	s = splbio();
	swb = &swp->sw_blocks[ix];
	off = off % dbtob(swp->sw_bsize);
	if (flags & B_READ) {
		if (swb->swb_block == 0 ||
		    (swb->swb_mask & (1 << atop(off))) == 0) {
#ifdef DEBUG
			if (swpagerdebug & (SDB_ALLOCBLK|SDB_FAIL))
				printf("swpg_io: %x bad read: blk %x+%x, mask %x, off %x+%x\n",
				       swp->sw_blocks,
				       swb->swb_block, atop(off),
				       swb->swb_mask,
				       m->offset, m->object->paging_offset);
#endif
			/* XXX: should we zero page here?? */
	splx(s);
			return(VM_PAGER_FAIL);
		}
	} else if (swb->swb_block == 0) {
#ifdef old
		swb->swb_block = rmalloc(swapmap, swp->sw_bsize);
		if (swb->swb_block == 0) {
#else
		if (!rlist_alloc(&swapmap, (unsigned)swp->sw_bsize,
			(unsigned *)&swb->swb_block)) {
#endif
#ifdef DEBUG
			if (swpagerdebug & SDB_FAIL)
				printf("swpg_io: rmalloc of %x failed\n",
				       swp->sw_bsize);
#endif
	splx(s);
			return(VM_PAGER_FAIL);
		}
#ifdef DEBUG
		if (swpagerdebug & (SDB_FULL|SDB_ALLOCBLK))
			printf("swpg_io: %x alloc blk %x at ix %x\n",
			       swp->sw_blocks, swb->swb_block, ix);
#endif
	}
	splx(s);

	/*
	 * Allocate a kernel virtual address and initialize so that PTE
	 * is available for lower level IO drivers.
	 */
	kva = vm_pager_map_page(m);

	/*
	 * Get a swap buffer header and perform the IO
	 */
	s = splbio();
	while (bswlist.av_forw == NULL) {
#ifdef DEBUG
		if (swpagerdebug & SDB_ANOM)
			printf("swap_pager_io: wait on swbuf for %x (%d)\n",
			       m, flags);
#endif
		bswlist.b_flags |= B_WANTED;
		sleep((caddr_t)&bswlist, PSWP+1);
	}
	bp = bswlist.av_forw;
	bswlist.av_forw = bp->av_forw;
	splx(s);
	bp->b_flags = B_BUSY | (flags & B_READ);
	bp->b_proc = &proc0;	/* XXX (but without B_PHYS set this is ok) */
	bp->b_un.b_addr = (caddr_t)kva;
	bp->b_blkno = swb->swb_block + btodb(off);
	VHOLD(swapdev_vp);
	bp->b_vp = swapdev_vp;
	if (swapdev_vp->v_type == VBLK)
		bp->b_dev = swapdev_vp->v_rdev;
	bp->b_bcount = PAGE_SIZE;
	if ((bp->b_flags & B_READ) == 0)
		swapdev_vp->v_numoutput++;

	/*
	 * If this is an async write we set up additional buffer fields
	 * and place a "cleaning" entry on the inuse queue.
	 */
	if ((flags & (B_READ|B_ASYNC)) == B_ASYNC) {
#ifdef DEBUG
		if (queue_empty(&swap_pager_free))
			panic("swpg_io: lost spc");
#endif
		queue_remove_first(&swap_pager_free,
				   spc, swp_clean_t, spc_list);
#ifdef DEBUG
		if (spc->spc_flags != SPC_FREE)
			panic("swpg_io: bad free spc");
#endif
		spc->spc_flags = SPC_BUSY;
		spc->spc_bp = bp;
		spc->spc_swp = swp;
		spc->spc_kva = kva;
		spc->spc_m = m;
		bp->b_flags |= B_CALL;
		bp->b_iodone = swap_pager_iodone;
		s = splbio();
		swp->sw_poip++;
		queue_enter(&swap_pager_inuse, spc, swp_clean_t, spc_list);

#ifdef DEBUG
		swap_pager_poip++;
		if (swpagerdebug & SDB_WRITE)
			printf("swpg_io: write: bp=%x swp=%x spc=%x poip=%d\n",
			       bp, swp, spc, swp->sw_poip);
		if ((swpagerdebug & SDB_ALLOCBLK) &&
		    (swb->swb_mask & (1 << atop(off))) == 0)
			printf("swpg_io: %x write blk %x+%x\n",
			       swp->sw_blocks, swb->swb_block, atop(off));
#endif
		swb->swb_mask |= (1 << atop(off));
		splx(s);
	}
#ifdef DEBUG
	if (swpagerdebug & SDB_IO)
		printf("swpg_io: IO start: bp %x, db %x, va %x, pa %x\n",
		       bp, swb->swb_block+btodb(off), kva, VM_PAGE_TO_PHYS(m));
#endif
	VOP_STRATEGY(bp);
	if ((flags & (B_READ|B_ASYNC)) == B_ASYNC) {
#ifdef DEBUG
		if (swpagerdebug & SDB_IO)
			printf("swpg_io:  IO started: bp %x\n", bp);
#endif
		return(VM_PAGER_PEND);
	}
	s = splbio();
#ifdef DEBUG
	if (flags & B_READ)
		swap_pager_piip++;
	else
		swap_pager_poip++;
#endif
	while ((bp->b_flags & B_DONE) == 0) {
		assert_wait((int)bp);
		thread_block();
	}
#ifdef DEBUG
	if (flags & B_READ)
		--swap_pager_piip;
	else
		--swap_pager_poip;
#endif
	rv = (bp->b_flags & B_ERROR) ? VM_PAGER_FAIL : VM_PAGER_OK;
	bp->b_flags &= ~(B_BUSY|B_WANTED|B_PHYS|B_DIRTY);
	bp->av_forw = bswlist.av_forw;
	bswlist.av_forw = bp;
	if (bp->b_vp)
		brelvp(bp);
	if (bswlist.b_flags & B_WANTED) {
		bswlist.b_flags &= ~B_WANTED;
		thread_wakeup((int)&bswlist);
	}
	if ((flags & B_READ) == 0 && rv == VM_PAGER_OK) {
		m->clean = TRUE;
		pmap_clear_modify(VM_PAGE_TO_PHYS(m));
	}
	splx(s);
#ifdef DEBUG
	if (swpagerdebug & SDB_IO)
		printf("swpg_io:  IO done: bp %x, rv %d\n", bp, rv);
	if ((swpagerdebug & SDB_FAIL) && rv == VM_PAGER_FAIL)
		printf("swpg_io: IO error\n");
#endif
	vm_pager_unmap_page(kva);
	return(rv);
}

boolean_t
swap_pager_clean(m, rw)
	vm_page_t m;
	int rw;
{
	register swp_clean_t spc, tspc;
	register int s;

#ifdef DEBUG
	/* save panic time state */
	if ((swpagerdebug & SDB_ANOMPANIC) && panicstr)
		return;
	if (swpagerdebug & SDB_FOLLOW)
		printf("swpg_clean(%x, %d)\n", m, rw);
#endif
	tspc = NULL;
	for (;;) {
		/*
		 * Look up and removal from inuse list must be done
		 * at splbio() to avoid conflicts with swap_pager_iodone.
		 */
		s = splbio();
		spc = (swp_clean_t) queue_first(&swap_pager_inuse);
		while (!queue_end(&swap_pager_inuse, (queue_entry_t)spc)) {
			if ((spc->spc_flags & SPC_DONE) &&
			    swap_pager_finish(spc)) {
				queue_remove(&swap_pager_inuse, spc,
					     swp_clean_t, spc_list);
				break;
			}
			if (m && m == spc->spc_m) {
#ifdef DEBUG
				if (swpagerdebug & SDB_ANOM)
					printf("swap_pager_clean: page %x on list, flags %x\n",
					       m, spc->spc_flags);
#endif
				tspc = spc;
			}
			spc = (swp_clean_t) queue_next(&spc->spc_list);
		}

		/*
		 * No operations done, thats all we can do for now.
		 */
		if (queue_end(&swap_pager_inuse, (queue_entry_t)spc))
			break;
		splx(s);

		/*
		 * The desired page was found to be busy earlier in
		 * the scan but has since completed.
		 */
		if (tspc && tspc == spc) {
#ifdef DEBUG
			if (swpagerdebug & SDB_ANOM)
				printf("swap_pager_clean: page %x done while looking\n",
				       m);
#endif
			tspc = NULL;
		}
		spc->spc_flags = SPC_FREE;
		vm_pager_unmap_page(spc->spc_kva);
		queue_enter(&swap_pager_free, spc, swp_clean_t, spc_list);
#ifdef DEBUG
		if (swpagerdebug & SDB_WRITE)
			printf("swpg_clean: free spc %x\n", spc);
#endif
	}
#ifdef DEBUG
	/*
	 * If we found that the desired page is already being cleaned
	 * mark it so that swap_pager_iodone() will not set the clean
	 * flag before the pageout daemon has another chance to clean it.
	 */
	if (tspc && rw == B_WRITE) {
		if (swpagerdebug & SDB_ANOM)
			printf("swap_pager_clean: page %x on clean list\n",
			       tspc);
		tspc->spc_flags |= SPC_DIRTY;
	}
#endif
	splx(s);

#ifdef DEBUG
	if (swpagerdebug & SDB_WRITE)
		printf("swpg_clean: return %d\n", tspc ? TRUE : FALSE);
	if ((swpagerdebug & SDB_ANOM) && tspc)
		printf("swpg_clean: %s of cleaning page %x\n",
		       rw == B_READ ? "get" : "put", m);
#endif
	return(tspc ? TRUE : FALSE);
}

swap_pager_finish(spc)
	register swp_clean_t spc;
{
	vm_object_t object = spc->spc_m->object;

	/*
	 * Mark the paging operation as done.
	 * (XXX) If we cannot get the lock, leave it til later.
	 * (XXX) Also we are assuming that an async write is a
	 *       pageout operation that has incremented the counter.
	 */
	if (!vm_object_lock_try(object))
		return(0);

	if (--object->paging_in_progress == 0)
		thread_wakeup((int) object);

#ifdef DEBUG
	/*
	 * XXX: this isn't even close to the right thing to do,
	 * introduces a variety of race conditions.
	 *
	 * If dirty, vm_pageout() has attempted to clean the page
	 * again.  In this case we do not do anything as we will
	 * see the page again shortly.
	 */
	if (spc->spc_flags & SPC_DIRTY) {
		if (swpagerdebug & SDB_ANOM)
			printf("swap_pager_finish: page %x dirty again\n",
			       spc->spc_m);
		spc->spc_m->busy = FALSE;
		PAGE_WAKEUP(spc->spc_m);
		vm_object_unlock(object);
		return(1);
	}
#endif
	/*
	 * If no error mark as clean and inform the pmap system.
	 * If error, mark as dirty so we will try again.
	 * (XXX could get stuck doing this, should give up after awhile)
	 */
	if (spc->spc_flags & SPC_ERROR) {
		printf("swap_pager_finish: clean of page %x failed\n",
		       VM_PAGE_TO_PHYS(spc->spc_m));
		spc->spc_m->laundry = TRUE;
	} else {
		spc->spc_m->clean = TRUE;
		pmap_clear_modify(VM_PAGE_TO_PHYS(spc->spc_m));
	}
	spc->spc_m->busy = FALSE;
	PAGE_WAKEUP(spc->spc_m);

	vm_object_unlock(object);
	return(1);
}

swap_pager_iodone(bp)
	register struct buf *bp;
{
	register swp_clean_t spc;
	daddr_t blk;
	int s;

#ifdef DEBUG
	/* save panic time state */
	if ((swpagerdebug & SDB_ANOMPANIC) && panicstr)
		return;
	if (swpagerdebug & SDB_FOLLOW)
		printf("swpg_iodone(%x)\n", bp);
#endif
	s = splbio();
	spc = (swp_clean_t) queue_first(&swap_pager_inuse);
	while (!queue_end(&swap_pager_inuse, (queue_entry_t)spc)) {
		if (spc->spc_bp == bp)
			break;
		spc = (swp_clean_t) queue_next(&spc->spc_list);
	}
#ifdef DEBUG
	if (queue_end(&swap_pager_inuse, (queue_entry_t)spc))
		panic("swap_pager_iodone: bp not found");
#endif

	spc->spc_flags &= ~SPC_BUSY;
	spc->spc_flags |= SPC_DONE;
	if (bp->b_flags & B_ERROR) {
		spc->spc_flags |= SPC_ERROR;
printf("error %d blkno %d sz %d ", bp->b_error, bp->b_blkno, bp->b_bcount);
	}
	spc->spc_bp = NULL;
	blk = bp->b_blkno;

#ifdef DEBUG
	--swap_pager_poip;
	if (swpagerdebug & SDB_WRITE)
		printf("swpg_iodone: bp=%x swp=%x flags=%x spc=%x poip=%x\n",
		       bp, spc->spc_swp, spc->spc_swp->sw_flags,
		       spc, spc->spc_swp->sw_poip);
#endif

	spc->spc_swp->sw_poip--;
	if (spc->spc_swp->sw_flags & SW_WANTED) {
		spc->spc_swp->sw_flags &= ~SW_WANTED;
		thread_wakeup((int)spc->spc_swp);
	}
		
	bp->b_flags &= ~(B_BUSY|B_WANTED|B_PHYS|B_DIRTY);
	bp->av_forw = bswlist.av_forw;
	bswlist.av_forw = bp;
	if (bp->b_vp)
		brelvp(bp);
	if (bswlist.b_flags & B_WANTED) {
		bswlist.b_flags &= ~B_WANTED;
		thread_wakeup((int)&bswlist);
	}
	thread_wakeup((int) &vm_pages_needed);
	splx(s);
}
#endif
