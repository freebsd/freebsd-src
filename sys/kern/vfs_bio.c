/*
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    John S. Dyson.
 * 4. This work was done expressly for inclusion into FreeBSD.  Other use
 *    is allowed if this notation is included.
 * 5. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 * $Id: vfs_bio.c,v 1.131 1997/10/26 20:55:04 phk Exp $
 */

/*
 * this file contains a new buffer I/O scheme implementing a coherent
 * VM object and buffer cache scheme.  Pains have been taken to make
 * sure that the performance degradation associated with schemes such
 * as this is not realized.
 *
 * Author:  John S. Dyson
 * Significant help during the development and debugging phases
 * had been provided by David Greenman, also of the FreeBSD core team.
 */

#include "opt_bounce.h"

#define VMIO
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/vm_kern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/resourcevar.h>

static MALLOC_DEFINE(M_BIOBUF, "BIO buffer", "BIO buffer");

static void vfs_update __P((void));
static struct	proc *updateproc;
static struct kproc_desc up_kp = {
	"update",
	vfs_update,
	&updateproc
};
SYSINIT_KT(update, SI_SUB_KTHREAD_UPDATE, SI_ORDER_FIRST, kproc_start, &up_kp)

struct buf *buf;		/* buffer header pool */
struct swqueue bswlist;

int count_lock_queue __P((void));
static void vm_hold_free_pages(struct buf * bp, vm_offset_t from,
		vm_offset_t to);
static void vm_hold_load_pages(struct buf * bp, vm_offset_t from,
		vm_offset_t to);
static void vfs_buf_set_valid(struct buf *bp, vm_ooffset_t foff,
			      vm_offset_t off, vm_offset_t size,
			      vm_page_t m);
static void vfs_page_set_valid(struct buf *bp, vm_ooffset_t off,
			       int pageno, vm_page_t m);
static void vfs_clean_pages(struct buf * bp);
static void vfs_setdirty(struct buf *bp);
static void vfs_vmio_release(struct buf *bp);
static void flushdirtybuffers(int slpflag, int slptimeo);

int needsbuffer;

/*
 * Internal update daemon, process 3
 *	The variable vfs_update_wakeup allows for internal syncs.
 */
int vfs_update_wakeup;


/*
 * buffers base kva
 */

/*
 * bogus page -- for I/O to/from partially complete buffers
 * this is a temporary solution to the problem, but it is not
 * really that bad.  it would be better to split the buffer
 * for input in the case of buffers partially already in memory,
 * but the code is intricate enough already.
 */
vm_page_t bogus_page;
static vm_offset_t bogus_offset;

static int bufspace, maxbufspace, vmiospace, maxvmiobufspace,
	bufmallocspace, maxbufmallocspace;
int numdirtybuffers, lodirtybuffers, hidirtybuffers;
static int numfreebuffers, lofreebuffers, hifreebuffers;

SYSCTL_INT(_vfs, OID_AUTO, numdirtybuffers, CTLFLAG_RD,
	&numdirtybuffers, 0, "");
SYSCTL_INT(_vfs, OID_AUTO, lodirtybuffers, CTLFLAG_RW,
	&lodirtybuffers, 0, "");
SYSCTL_INT(_vfs, OID_AUTO, hidirtybuffers, CTLFLAG_RW,
	&hidirtybuffers, 0, "");
SYSCTL_INT(_vfs, OID_AUTO, numfreebuffers, CTLFLAG_RD,
	&numfreebuffers, 0, "");
SYSCTL_INT(_vfs, OID_AUTO, lofreebuffers, CTLFLAG_RW,
	&lofreebuffers, 0, "");
SYSCTL_INT(_vfs, OID_AUTO, hifreebuffers, CTLFLAG_RW,
	&hifreebuffers, 0, "");
SYSCTL_INT(_vfs, OID_AUTO, maxbufspace, CTLFLAG_RW,
	&maxbufspace, 0, "");
SYSCTL_INT(_vfs, OID_AUTO, bufspace, CTLFLAG_RD,
	&bufspace, 0, "");
SYSCTL_INT(_vfs, OID_AUTO, maxvmiobufspace, CTLFLAG_RW,
	&maxvmiobufspace, 0, "");
SYSCTL_INT(_vfs, OID_AUTO, vmiospace, CTLFLAG_RD,
	&vmiospace, 0, "");
SYSCTL_INT(_vfs, OID_AUTO, maxmallocbufspace, CTLFLAG_RW,
	&maxbufmallocspace, 0, "");
SYSCTL_INT(_vfs, OID_AUTO, bufmallocspace, CTLFLAG_RD,
	&bufmallocspace, 0, "");

static LIST_HEAD(bufhashhdr, buf) bufhashtbl[BUFHSZ], invalhash;
static TAILQ_HEAD(bqueues, buf) bufqueues[BUFFER_QUEUES];

extern int vm_swap_size;

#define BUF_MAXUSE 24

#define VFS_BIO_NEED_ANY 1
#define VFS_BIO_NEED_LOWLIMIT 2
#define VFS_BIO_NEED_FREE 4

/*
 * Initialize buffer headers and related structures.
 */
void
bufinit()
{
	struct buf *bp;
	int i;

	TAILQ_INIT(&bswlist);
	LIST_INIT(&invalhash);

	/* first, make a null hash table */
	for (i = 0; i < BUFHSZ; i++)
		LIST_INIT(&bufhashtbl[i]);

	/* next, make a null set of free lists */
	for (i = 0; i < BUFFER_QUEUES; i++)
		TAILQ_INIT(&bufqueues[i]);

	/* finally, initialize each buffer header and stick on empty q */
	for (i = 0; i < nbuf; i++) {
		bp = &buf[i];
		bzero(bp, sizeof *bp);
		bp->b_flags = B_INVAL;	/* we're just an empty header */
		bp->b_dev = NODEV;
		bp->b_rcred = NOCRED;
		bp->b_wcred = NOCRED;
		bp->b_qindex = QUEUE_EMPTY;
		bp->b_vnbufs.le_next = NOLIST;
		TAILQ_INSERT_TAIL(&bufqueues[QUEUE_EMPTY], bp, b_freelist);
		LIST_INSERT_HEAD(&invalhash, bp, b_hash);
	}
/*
 * maxbufspace is currently calculated to support all filesystem blocks
 * to be 8K.  If you happen to use a 16K filesystem, the size of the buffer
 * cache is still the same as it would be for 8K filesystems.  This
 * keeps the size of the buffer cache "in check" for big block filesystems.
 */
	maxbufspace = (nbuf + 8) * DFLTBSIZE;
/*
 * reserve 1/3 of the buffers for metadata (VDIR) which might not be VMIO'ed
 */
	maxvmiobufspace = 2 * maxbufspace / 3;
/*
 * Limit the amount of malloc memory since it is wired permanently into
 * the kernel space.  Even though this is accounted for in the buffer
 * allocation, we don't want the malloced region to grow uncontrolled.
 * The malloc scheme improves memory utilization significantly on average
 * (small) directories.
 */
	maxbufmallocspace = maxbufspace / 20;

/*
 * Remove the probability of deadlock conditions by limiting the
 * number of dirty buffers.
 */
	hidirtybuffers = nbuf / 6 + 20;
	lodirtybuffers = nbuf / 12 + 10;
	numdirtybuffers = 0;
	lofreebuffers = nbuf / 18 + 5;
	hifreebuffers = 2 * lofreebuffers;
	numfreebuffers = nbuf;

	bogus_offset = kmem_alloc_pageable(kernel_map, PAGE_SIZE);
	bogus_page = vm_page_alloc(kernel_object,
			((bogus_offset - VM_MIN_KERNEL_ADDRESS) >> PAGE_SHIFT),
			VM_ALLOC_NORMAL);

}

/*
 * Free the kva allocation for a buffer
 * Must be called only at splbio or higher,
 *  as this is the only locking for buffer_map.
 */
static void
bfreekva(struct buf * bp)
{
	if (bp->b_kvasize == 0)
		return;
		
	vm_map_delete(buffer_map,
		(vm_offset_t) bp->b_kvabase,
		(vm_offset_t) bp->b_kvabase + bp->b_kvasize);

	bp->b_kvasize = 0;

}

/*
 * remove the buffer from the appropriate free list
 */
void
bremfree(struct buf * bp)
{
	int s = splbio();

	if (bp->b_qindex != QUEUE_NONE) {
		TAILQ_REMOVE(&bufqueues[bp->b_qindex], bp, b_freelist);
		bp->b_qindex = QUEUE_NONE;
	} else {
#if !defined(MAX_PERF)
		panic("bremfree: removing a buffer when not on a queue");
#endif
	}
	if ((bp->b_flags & B_INVAL) ||
		(bp->b_flags & (B_DELWRI|B_LOCKED)) == 0)
		--numfreebuffers;
	splx(s);
}

/*
 * Get a buffer with the specified data.  Look in the cache first.
 */
int
bread(struct vnode * vp, daddr_t blkno, int size, struct ucred * cred,
    struct buf ** bpp)
{
	struct buf *bp;

	bp = getblk(vp, blkno, size, 0, 0);
	*bpp = bp;

	/* if not found in cache, do some I/O */
	if ((bp->b_flags & B_CACHE) == 0) {
		if (curproc != NULL)
			curproc->p_stats->p_ru.ru_inblock++;
		bp->b_flags |= B_READ;
		bp->b_flags &= ~(B_DONE | B_ERROR | B_INVAL);
		if (bp->b_rcred == NOCRED) {
			if (cred != NOCRED)
				crhold(cred);
			bp->b_rcred = cred;
		}
		vfs_busy_pages(bp, 0);
		VOP_STRATEGY(bp);
		return (biowait(bp));
	}
	return (0);
}

/*
 * Operates like bread, but also starts asynchronous I/O on
 * read-ahead blocks.
 */
int
breadn(struct vnode * vp, daddr_t blkno, int size,
    daddr_t * rablkno, int *rabsize,
    int cnt, struct ucred * cred, struct buf ** bpp)
{
	struct buf *bp, *rabp;
	int i;
	int rv = 0, readwait = 0;

	*bpp = bp = getblk(vp, blkno, size, 0, 0);

	/* if not found in cache, do some I/O */
	if ((bp->b_flags & B_CACHE) == 0) {
		if (curproc != NULL)
			curproc->p_stats->p_ru.ru_inblock++;
		bp->b_flags |= B_READ;
		bp->b_flags &= ~(B_DONE | B_ERROR | B_INVAL);
		if (bp->b_rcred == NOCRED) {
			if (cred != NOCRED)
				crhold(cred);
			bp->b_rcred = cred;
		}
		vfs_busy_pages(bp, 0);
		VOP_STRATEGY(bp);
		++readwait;
	}
	for (i = 0; i < cnt; i++, rablkno++, rabsize++) {
		if (inmem(vp, *rablkno))
			continue;
		rabp = getblk(vp, *rablkno, *rabsize, 0, 0);

		if ((rabp->b_flags & B_CACHE) == 0) {
			if (curproc != NULL)
				curproc->p_stats->p_ru.ru_inblock++;
			rabp->b_flags |= B_READ | B_ASYNC;
			rabp->b_flags &= ~(B_DONE | B_ERROR | B_INVAL);
			if (rabp->b_rcred == NOCRED) {
				if (cred != NOCRED)
					crhold(cred);
				rabp->b_rcred = cred;
			}
			vfs_busy_pages(rabp, 0);
			VOP_STRATEGY(rabp);
		} else {
			brelse(rabp);
		}
	}

	if (readwait) {
		rv = biowait(bp);
	}
	return (rv);
}

/*
 * Write, release buffer on completion.  (Done by iodone
 * if async.)
 */
int
bwrite(struct buf * bp)
{
	int oldflags = bp->b_flags;

	if (bp->b_flags & B_INVAL) {
		brelse(bp);
		return (0);
	}
#if !defined(MAX_PERF)
	if (!(bp->b_flags & B_BUSY))
		panic("bwrite: buffer is not busy???");
#endif

	bp->b_flags &= ~(B_READ | B_DONE | B_ERROR | B_DELWRI);
	bp->b_flags |= B_WRITEINPROG;

	if ((oldflags & B_DELWRI) == B_DELWRI) {
		--numdirtybuffers;
		reassignbuf(bp, bp->b_vp);
	}

	bp->b_vp->v_numoutput++;
	vfs_busy_pages(bp, 1);
	if (curproc != NULL)
		curproc->p_stats->p_ru.ru_oublock++;
	VOP_STRATEGY(bp);

	if ((oldflags & B_ASYNC) == 0) {
		int rtval = biowait(bp);

		if (oldflags & B_DELWRI) {
			reassignbuf(bp, bp->b_vp);
		}
		brelse(bp);
		return (rtval);
	}
	return (0);
}

void
vfs_bio_need_satisfy(void) {
	++numfreebuffers;
	if (!needsbuffer)
		return;
	if (numdirtybuffers < lodirtybuffers) {
		needsbuffer &= ~(VFS_BIO_NEED_ANY | VFS_BIO_NEED_LOWLIMIT);
	} else {
		needsbuffer &= ~VFS_BIO_NEED_ANY;
	}
	if (numfreebuffers >= hifreebuffers) {
		needsbuffer &= ~VFS_BIO_NEED_FREE;
	}
	wakeup(&needsbuffer);
}

/*
 * Delayed write. (Buffer is marked dirty).
 */
void
bdwrite(struct buf * bp)
{

#if !defined(MAX_PERF)
	if ((bp->b_flags & B_BUSY) == 0) {
		panic("bdwrite: buffer is not busy");
	}
#endif

	if (bp->b_flags & B_INVAL) {
		brelse(bp);
		return;
	}
	if (bp->b_flags & B_TAPE) {
		bawrite(bp);
		return;
	}
	bp->b_flags &= ~(B_READ|B_RELBUF);
	if ((bp->b_flags & B_DELWRI) == 0) {
		bp->b_flags |= B_DONE | B_DELWRI;
		reassignbuf(bp, bp->b_vp);
		++numdirtybuffers;
	}

	/*
	 * This bmap keeps the system from needing to do the bmap later,
	 * perhaps when the system is attempting to do a sync.  Since it
	 * is likely that the indirect block -- or whatever other datastructure
	 * that the filesystem needs is still in memory now, it is a good
	 * thing to do this.  Note also, that if the pageout daemon is
	 * requesting a sync -- there might not be enough memory to do
	 * the bmap then...  So, this is important to do.
	 */
	if (bp->b_lblkno == bp->b_blkno) {
		VOP_BMAP(bp->b_vp, bp->b_lblkno, NULL, &bp->b_blkno, NULL, NULL);
	}

	/*
	 * Set the *dirty* buffer range based upon the VM system dirty pages.
	 */
	vfs_setdirty(bp);

	/*
	 * We need to do this here to satisfy the vnode_pager and the
	 * pageout daemon, so that it thinks that the pages have been
	 * "cleaned".  Note that since the pages are in a delayed write
	 * buffer -- the VFS layer "will" see that the pages get written
	 * out on the next sync, or perhaps the cluster will be completed.
	 */
	vfs_clean_pages(bp);
	bqrelse(bp);

	if (numdirtybuffers >= hidirtybuffers)
		flushdirtybuffers(0, 0);

	return;
}

/*
 * Asynchronous write.
 * Start output on a buffer, but do not wait for it to complete.
 * The buffer is released when the output completes.
 */
void
bawrite(struct buf * bp)
{
	bp->b_flags |= B_ASYNC;
	(void) VOP_BWRITE(bp);
}

/*
 * Ordered write.
 * Start output on a buffer, but only wait for it to complete if the
 * output device cannot guarantee ordering in some other way.  Devices
 * that can perform asynchronous ordered writes will set the B_ASYNC
 * flag in their strategy routine.
 * The buffer is released when the output completes.
 */
int
bowrite(struct buf * bp)
{
	/*
	 * XXX Add in B_ASYNC once the SCSI
	 *     layer can deal with ordered
	 *     writes properly.
	 */
	bp->b_flags |= B_ORDERED;
	return (VOP_BWRITE(bp));
}

/*
 * Release a buffer.
 */
void
brelse(struct buf * bp)
{
	int s;

	if (bp->b_flags & B_CLUSTER) {
		relpbuf(bp);
		return;
	}
	/* anyone need a "free" block? */
	s = splbio();

	/* anyone need this block? */
	if (bp->b_flags & B_WANTED) {
		bp->b_flags &= ~(B_WANTED | B_AGE);
		wakeup(bp);
	} 

	if (bp->b_flags & B_LOCKED)
		bp->b_flags &= ~B_ERROR;

	if ((bp->b_flags & (B_NOCACHE | B_INVAL | B_ERROR)) ||
	    (bp->b_bufsize <= 0)) {
		bp->b_flags |= B_INVAL;
		if (bp->b_flags & B_DELWRI)
			--numdirtybuffers;
		bp->b_flags &= ~(B_DELWRI | B_CACHE);
		if (((bp->b_flags & B_VMIO) == 0) && bp->b_vp) {
			if (bp->b_bufsize)
				allocbuf(bp, 0);
			brelvp(bp);
		}
	}

	/*
	 * VMIO buffer rundown.  It is not very necessary to keep a VMIO buffer
	 * constituted, so the B_INVAL flag is used to *invalidate* the buffer,
	 * but the VM object is kept around.  The B_NOCACHE flag is used to
	 * invalidate the pages in the VM object.
	 *
	 * If the buffer is a partially filled NFS buffer, keep it
	 * since invalidating it now will lose informatio.  The valid
	 * flags in the vm_pages have only DEV_BSIZE resolution but
	 * the b_validoff, b_validend fields have byte resolution.
	 * This can avoid unnecessary re-reads of the buffer.
	 * XXX this seems to cause performance problems.
	 */
	if ((bp->b_flags & B_VMIO)
	    && !(bp->b_vp->v_tag == VT_NFS &&
		 bp->b_vp->v_type != VBLK &&
		 (bp->b_flags & B_DELWRI) != 0)
#ifdef notdef
	    && (bp->b_vp->v_tag != VT_NFS
		|| bp->b_vp->v_type == VBLK
		|| (bp->b_flags & (B_NOCACHE | B_INVAL | B_ERROR))
		|| bp->b_validend == 0
		|| (bp->b_validoff == 0
		    && bp->b_validend == bp->b_bufsize))
#endif
	    ) {
		vm_ooffset_t foff;
		vm_object_t obj;
		int i, resid;
		vm_page_t m;
		struct vnode *vp;
		int iototal = bp->b_bufsize;

		vp = bp->b_vp;

#if !defined(MAX_PERF)
		if (!vp) 
			panic("brelse: missing vp");
#endif

		if (bp->b_npages) {
			vm_pindex_t poff;
			obj = (vm_object_t) vp->v_object;
			if (vp->v_type == VBLK)
				foff = ((vm_ooffset_t) bp->b_lblkno) << DEV_BSHIFT;
			else
				foff = (vm_ooffset_t) vp->v_mount->mnt_stat.f_iosize * bp->b_lblkno;
			poff = OFF_TO_IDX(foff);
			for (i = 0; i < bp->b_npages; i++) {
				m = bp->b_pages[i];
				if (m == bogus_page) {
					m = vm_page_lookup(obj, poff + i);
#if !defined(MAX_PERF)
					if (!m) {
						panic("brelse: page missing\n");
					}
#endif
					bp->b_pages[i] = m;
					pmap_qenter(trunc_page(bp->b_data),
						bp->b_pages, bp->b_npages);
				}
				resid = IDX_TO_OFF(m->pindex+1) - foff;
				if (resid > iototal)
					resid = iototal;
				if (resid > 0) {
					/*
					 * Don't invalidate the page if the local machine has already
					 * modified it.  This is the lesser of two evils, and should
					 * be fixed.
					 */
					if (bp->b_flags & (B_NOCACHE | B_ERROR)) {
						vm_page_test_dirty(m);
						if (m->dirty == 0) {
							vm_page_set_invalid(m, (vm_offset_t) foff, resid);
							if (m->valid == 0)
								vm_page_protect(m, VM_PROT_NONE);
						}
					}
					if (resid >= PAGE_SIZE) {
						if ((m->valid & VM_PAGE_BITS_ALL) != VM_PAGE_BITS_ALL) {
							bp->b_flags |= B_INVAL;
						}
					} else {
						if (!vm_page_is_valid(m,
							(((vm_offset_t) bp->b_data) & PAGE_MASK), resid)) {
							bp->b_flags |= B_INVAL;
						}
					}
				}
				foff += resid;
				iototal -= resid;
			}
		}
		if (bp->b_flags & (B_INVAL | B_RELBUF))
			vfs_vmio_release(bp);
	}
#if !defined(MAX_PERF)
	if (bp->b_qindex != QUEUE_NONE)
		panic("brelse: free buffer onto another queue???");
#endif

	/* enqueue */
	/* buffers with no memory */
	if (bp->b_bufsize == 0) {
		bp->b_flags |= B_INVAL;
		bp->b_qindex = QUEUE_EMPTY;
		TAILQ_INSERT_HEAD(&bufqueues[QUEUE_EMPTY], bp, b_freelist);
		LIST_REMOVE(bp, b_hash);
		LIST_INSERT_HEAD(&invalhash, bp, b_hash);
		bp->b_dev = NODEV;
		/*
		 * Get rid of the kva allocation *now*
		 */
		bfreekva(bp);

	/* buffers with junk contents */
	} else if (bp->b_flags & (B_ERROR | B_INVAL | B_NOCACHE | B_RELBUF)) {
		bp->b_flags |= B_INVAL;
		bp->b_qindex = QUEUE_AGE;
		TAILQ_INSERT_HEAD(&bufqueues[QUEUE_AGE], bp, b_freelist);
		LIST_REMOVE(bp, b_hash);
		LIST_INSERT_HEAD(&invalhash, bp, b_hash);
		bp->b_dev = NODEV;

	/* buffers that are locked */
	} else if (bp->b_flags & B_LOCKED) {
		bp->b_qindex = QUEUE_LOCKED;
		TAILQ_INSERT_TAIL(&bufqueues[QUEUE_LOCKED], bp, b_freelist);

	/* buffers with stale but valid contents */
	} else if (bp->b_flags & B_AGE) {
		bp->b_qindex = QUEUE_AGE;
		TAILQ_INSERT_TAIL(&bufqueues[QUEUE_AGE], bp, b_freelist);

	/* buffers with valid and quite potentially reuseable contents */
	} else {
		bp->b_qindex = QUEUE_LRU;
		TAILQ_INSERT_TAIL(&bufqueues[QUEUE_LRU], bp, b_freelist);
	}

	if ((bp->b_flags & B_INVAL) ||
		(bp->b_flags & (B_LOCKED|B_DELWRI)) == 0) {
		if (bp->b_flags & B_DELWRI) {
			--numdirtybuffers;
			bp->b_flags &= ~B_DELWRI;
		}
		vfs_bio_need_satisfy();
	}

	/* unlock */
	bp->b_flags &= ~(B_ORDERED | B_WANTED | B_BUSY |
				B_ASYNC | B_NOCACHE | B_AGE | B_RELBUF);
	splx(s);
}

/*
 * Release a buffer.
 */
void
bqrelse(struct buf * bp)
{
	int s;

	s = splbio();

	/* anyone need this block? */
	if (bp->b_flags & B_WANTED) {
		bp->b_flags &= ~(B_WANTED | B_AGE);
		wakeup(bp);
	} 

#if !defined(MAX_PERF)
	if (bp->b_qindex != QUEUE_NONE)
		panic("bqrelse: free buffer onto another queue???");
#endif

	if (bp->b_flags & B_LOCKED) {
		bp->b_flags &= ~B_ERROR;
		bp->b_qindex = QUEUE_LOCKED;
		TAILQ_INSERT_TAIL(&bufqueues[QUEUE_LOCKED], bp, b_freelist);
		/* buffers with stale but valid contents */
	} else {
		bp->b_qindex = QUEUE_LRU;
		TAILQ_INSERT_TAIL(&bufqueues[QUEUE_LRU], bp, b_freelist);
	}

	if ((bp->b_flags & (B_LOCKED|B_DELWRI)) == 0) {
		vfs_bio_need_satisfy();
	}

	/* unlock */
	bp->b_flags &= ~(B_ORDERED | B_WANTED | B_BUSY |
		B_ASYNC | B_NOCACHE | B_AGE | B_RELBUF);
	splx(s);
}

static void
vfs_vmio_release(bp)
	struct buf *bp;
{
	int i;
	vm_page_t m;

	for (i = 0; i < bp->b_npages; i++) {
		m = bp->b_pages[i];
		bp->b_pages[i] = NULL;
		vm_page_unwire(m);
		/*
		 * We don't mess with busy pages, it is
		 * the responsibility of the process that
		 * busied the pages to deal with them.
		 */
		if ((m->flags & PG_BUSY) || (m->busy != 0))
			continue;
			
		if (m->wire_count == 0) {

			if (m->flags & PG_WANTED) {
				m->flags &= ~PG_WANTED;
				wakeup(m);
			}

			/*
			 * If this is an async free -- we cannot place
			 * pages onto the cache queue.  If it is an
			 * async free, then we don't modify any queues.
			 * This is probably in error (for perf reasons),
			 * and we will eventually need to build
			 * a more complete infrastructure to support I/O
			 * rundown.
			 */
			if ((bp->b_flags & B_ASYNC) == 0) {

			/*
			 * In the case of sync buffer frees, we can do pretty much
			 * anything to any of the memory queues.  Specifically,
			 * the cache queue is okay to be modified.
			 */
				if (m->valid) {
					if(m->dirty == 0)
						vm_page_test_dirty(m);
					/*
					 * this keeps pressure off of the process memory
					 */
					if (m->dirty == 0 && m->hold_count == 0)
						vm_page_cache(m);
					else
						vm_page_deactivate(m);
				} else if (m->hold_count == 0) {
					vm_page_protect(m, VM_PROT_NONE);
					vm_page_free(m);
				}
			} else {
				/*
				 * If async, then at least we clear the
				 * act_count.
				 */
				m->act_count = 0;
			}
		}
	}
	bufspace -= bp->b_bufsize;
	vmiospace -= bp->b_bufsize;
	pmap_qremove(trunc_page((vm_offset_t) bp->b_data), bp->b_npages);
	bp->b_npages = 0;
	bp->b_bufsize = 0;
	bp->b_flags &= ~B_VMIO;
	if (bp->b_vp)
		brelvp(bp);
}

/*
 * Check to see if a block is currently memory resident.
 */
struct buf *
gbincore(struct vnode * vp, daddr_t blkno)
{
	struct buf *bp;
	struct bufhashhdr *bh;

	bh = BUFHASH(vp, blkno);
	bp = bh->lh_first;

	/* Search hash chain */
	while (bp != NULL) {
		/* hit */
		if (bp->b_vp == vp && bp->b_lblkno == blkno &&
		    (bp->b_flags & B_INVAL) == 0) {
			break;
		}
		bp = bp->b_hash.le_next;
	}
	return (bp);
}

/*
 * this routine implements clustered async writes for
 * clearing out B_DELWRI buffers...  This is much better
 * than the old way of writing only one buffer at a time.
 */
int
vfs_bio_awrite(struct buf * bp)
{
	int i;
	daddr_t lblkno = bp->b_lblkno;
	struct vnode *vp = bp->b_vp;
	int s;
	int ncl;
	struct buf *bpa;
	int nwritten;

	s = splbio();
	/*
	 * right now we support clustered writing only to regular files
	 */
	if ((vp->v_type == VREG) && 
	    (vp->v_mount != 0) && /* Only on nodes that have the size info */
	    (bp->b_flags & (B_CLUSTEROK | B_INVAL)) == B_CLUSTEROK) {
		int size;
		int maxcl;

		size = vp->v_mount->mnt_stat.f_iosize;
		maxcl = MAXPHYS / size;

		for (i = 1; i < maxcl; i++) {
			if ((bpa = gbincore(vp, lblkno + i)) &&
			    ((bpa->b_flags & (B_BUSY | B_DELWRI | B_CLUSTEROK | B_INVAL)) ==
			    (B_DELWRI | B_CLUSTEROK)) &&
			    (bpa->b_bufsize == size)) {
				if ((bpa->b_blkno == bpa->b_lblkno) ||
				    (bpa->b_blkno != bp->b_blkno + ((i * size) >> DEV_BSHIFT)))
					break;
			} else {
				break;
			}
		}
		ncl = i;
		/*
		 * this is a possible cluster write
		 */
		if (ncl != 1) {
			nwritten = cluster_wbuild(vp, size, lblkno, ncl);
			splx(s);
			return nwritten;
		}
	}
	bremfree(bp);
	splx(s);
	/*
	 * default (old) behavior, writing out only one block
	 */
	bp->b_flags |= B_BUSY | B_ASYNC;
	nwritten = bp->b_bufsize;
	(void) VOP_BWRITE(bp);
	return nwritten;
}


/*
 * Find a buffer header which is available for use.
 */
static struct buf *
getnewbuf(struct vnode *vp, int slpflag, int slptimeo, int size, int maxsize)
{
	struct buf *bp;
	int nbyteswritten = 0;
	vm_offset_t addr;
	static int writerecursion = 0;

start:
	if (bufspace >= maxbufspace)
		goto trytofreespace;

	/* can we constitute a new buffer? */
	if ((bp = TAILQ_FIRST(&bufqueues[QUEUE_EMPTY]))) {
#if !defined(MAX_PERF)
		if (bp->b_qindex != QUEUE_EMPTY)
			panic("getnewbuf: inconsistent EMPTY queue, qindex=%d",
			    bp->b_qindex);
#endif
		bp->b_flags |= B_BUSY;
		bremfree(bp);
		goto fillbuf;
	}
trytofreespace:
	/*
	 * We keep the file I/O from hogging metadata I/O
	 * This is desirable because file data is cached in the
	 * VM/Buffer cache even if a buffer is freed.
	 */
	if ((bp = TAILQ_FIRST(&bufqueues[QUEUE_AGE]))) {
#if !defined(MAX_PERF)
		if (bp->b_qindex != QUEUE_AGE)
			panic("getnewbuf: inconsistent AGE queue, qindex=%d",
			    bp->b_qindex);
#endif
	} else if ((bp = TAILQ_FIRST(&bufqueues[QUEUE_LRU]))) {
#if !defined(MAX_PERF)
		if (bp->b_qindex != QUEUE_LRU)
			panic("getnewbuf: inconsistent LRU queue, qindex=%d",
			    bp->b_qindex);
#endif
	}
	if (!bp) {
		/* wait for a free buffer of any kind */
		needsbuffer |= VFS_BIO_NEED_ANY;
		do
			tsleep(&needsbuffer, (PRIBIO + 1) | slpflag, "newbuf",
			    slptimeo);
		while (needsbuffer & VFS_BIO_NEED_ANY);
		return (0);
	}

#if defined(DIAGNOSTIC)
	if (bp->b_flags & B_BUSY) {
		panic("getnewbuf: busy buffer on free list\n");
	}
#endif

	/*
	 * We are fairly aggressive about freeing VMIO buffers, but since
	 * the buffering is intact without buffer headers, there is not
	 * much loss.  We gain by maintaining non-VMIOed metadata in buffers.
	 */
	if ((bp->b_qindex == QUEUE_LRU) && (bp->b_usecount > 0)) {
		if ((bp->b_flags & B_VMIO) == 0 ||
			(vmiospace < maxvmiobufspace)) {
			--bp->b_usecount;
			TAILQ_REMOVE(&bufqueues[QUEUE_LRU], bp, b_freelist);
			if (TAILQ_FIRST(&bufqueues[QUEUE_LRU]) != NULL) {
				TAILQ_INSERT_TAIL(&bufqueues[QUEUE_LRU], bp, b_freelist);
				goto start;
			}
			TAILQ_INSERT_TAIL(&bufqueues[QUEUE_LRU], bp, b_freelist);
		}
	}


	/* if we are a delayed write, convert to an async write */
	if ((bp->b_flags & (B_DELWRI | B_INVAL)) == B_DELWRI) {

		if (writerecursion > 0) {
			bp = TAILQ_FIRST(&bufqueues[QUEUE_AGE]);
			while (bp) {
				if ((bp->b_flags & B_DELWRI) == 0)
					break;
				bp = TAILQ_NEXT(bp, b_freelist);
			}
			if (bp == NULL) {
				bp = TAILQ_FIRST(&bufqueues[QUEUE_LRU]);
				while (bp) {
					if ((bp->b_flags & B_DELWRI) == 0)
						break;
					bp = TAILQ_NEXT(bp, b_freelist);
				}
			}
			if (bp == NULL)
				panic("getnewbuf: cannot get buffer, infinite recursion failure");
		} else {
			++writerecursion;
			nbyteswritten += vfs_bio_awrite(bp);
			--writerecursion;
			if (!slpflag && !slptimeo) {
				return (0);
			}
			goto start;
		}
	}

	if (bp->b_flags & B_WANTED) {
		bp->b_flags &= ~B_WANTED;
		wakeup(bp);
	}
	bremfree(bp);
	bp->b_flags |= B_BUSY;

	if (bp->b_flags & B_VMIO) {
		bp->b_flags &= ~B_ASYNC;
		vfs_vmio_release(bp);
	}

	if (bp->b_vp)
		brelvp(bp);

fillbuf:
	/* we are not free, nor do we contain interesting data */
	if (bp->b_rcred != NOCRED) {
		crfree(bp->b_rcred);
		bp->b_rcred = NOCRED;
	}
	if (bp->b_wcred != NOCRED) {
		crfree(bp->b_wcred);
		bp->b_wcred = NOCRED;
	}

	LIST_REMOVE(bp, b_hash);
	LIST_INSERT_HEAD(&invalhash, bp, b_hash);
	if (bp->b_bufsize) {
		allocbuf(bp, 0);
	}
	bp->b_flags = B_BUSY;
	bp->b_dev = NODEV;
	bp->b_vp = NULL;
	bp->b_blkno = bp->b_lblkno = 0;
	bp->b_iodone = 0;
	bp->b_error = 0;
	bp->b_resid = 0;
	bp->b_bcount = 0;
	bp->b_npages = 0;
	bp->b_dirtyoff = bp->b_dirtyend = 0;
	bp->b_validoff = bp->b_validend = 0;
	bp->b_usecount = 4;

	maxsize = (maxsize + PAGE_MASK) & ~PAGE_MASK;

	/*
	 * we assume that buffer_map is not at address 0
	 */
	addr = 0;
	if (maxsize != bp->b_kvasize) {
		bfreekva(bp);
		
		/*
		 * See if we have buffer kva space
		 */
		if (vm_map_findspace(buffer_map,
			vm_map_min(buffer_map), maxsize, &addr)) {
			bp->b_flags |= B_INVAL;
			brelse(bp);
			goto trytofreespace;
		}
	}

	/*
	 * See if we are below are allocated minimum
	 */
	if (bufspace >= (maxbufspace + nbyteswritten)) {
		bp->b_flags |= B_INVAL;
		brelse(bp);
		goto trytofreespace;
	}

	/*
	 * create a map entry for the buffer -- in essence
	 * reserving the kva space.
	 */
	if (addr) {
		vm_map_insert(buffer_map, NULL, 0,
			addr, addr + maxsize,
			VM_PROT_ALL, VM_PROT_ALL, MAP_NOFAULT);

		bp->b_kvabase = (caddr_t) addr;
		bp->b_kvasize = maxsize;
	}
	bp->b_data = bp->b_kvabase;
	
	return (bp);
}

static void
waitfreebuffers(int slpflag, int slptimeo) {
	while (numfreebuffers < hifreebuffers) {
		flushdirtybuffers(slpflag, slptimeo);
		if (numfreebuffers < hifreebuffers)
			break;
		needsbuffer |= VFS_BIO_NEED_FREE;
		if (tsleep(&needsbuffer, PRIBIO|slpflag, "biofre", slptimeo))
			break;
	}
}

static void
flushdirtybuffers(int slpflag, int slptimeo) {
	int s;
	static pid_t flushing = 0;

	s = splbio();

	if (flushing) {
		if (flushing == curproc->p_pid) {
			splx(s);
			return;
		}
		while (flushing) {
			if (tsleep(&flushing, PRIBIO|slpflag, "biofls", slptimeo)) {
				splx(s);
				return;
			}
		}
	}
	flushing = curproc->p_pid;

	while (numdirtybuffers > lodirtybuffers) {
		struct buf *bp;
		needsbuffer |= VFS_BIO_NEED_LOWLIMIT;
		bp = TAILQ_FIRST(&bufqueues[QUEUE_AGE]);
		if (bp == NULL)
			bp = TAILQ_FIRST(&bufqueues[QUEUE_LRU]);

		while (bp && ((bp->b_flags & B_DELWRI) == 0)) {
			bp = TAILQ_NEXT(bp, b_freelist);
		}

		if (bp) {
			splx(s);
			vfs_bio_awrite(bp);
			s = splbio();
			continue;
		}
		break;
	}

	flushing = 0;
	wakeup(&flushing);
	splx(s);
}

/*
 * Check to see if a block is currently memory resident.
 */
struct buf *
incore(struct vnode * vp, daddr_t blkno)
{
	struct buf *bp;

	int s = splbio();
	bp = gbincore(vp, blkno);
	splx(s);
	return (bp);
}

/*
 * Returns true if no I/O is needed to access the
 * associated VM object.  This is like incore except
 * it also hunts around in the VM system for the data.
 */

int
inmem(struct vnode * vp, daddr_t blkno)
{
	vm_object_t obj;
	vm_offset_t toff, tinc;
	vm_page_t m;
	vm_ooffset_t off;

	if (incore(vp, blkno))
		return 1;
	if (vp->v_mount == NULL)
		return 0;
	if ((vp->v_object == NULL) || (vp->v_flag & VVMIO) == 0)
		return 0;

	obj = vp->v_object;
	tinc = PAGE_SIZE;
	if (tinc > vp->v_mount->mnt_stat.f_iosize)
		tinc = vp->v_mount->mnt_stat.f_iosize;
	off = blkno * vp->v_mount->mnt_stat.f_iosize;

	for (toff = 0; toff < vp->v_mount->mnt_stat.f_iosize; toff += tinc) {

		m = vm_page_lookup(obj, OFF_TO_IDX(off + toff));
		if (!m)
			return 0;
		if (vm_page_is_valid(m, (vm_offset_t) (toff + off), tinc) == 0)
			return 0;
	}
	return 1;
}

/*
 * now we set the dirty range for the buffer --
 * for NFS -- if the file is mapped and pages have
 * been written to, let it know.  We want the
 * entire range of the buffer to be marked dirty if
 * any of the pages have been written to for consistancy
 * with the b_validoff, b_validend set in the nfs write
 * code, and used by the nfs read code.
 */
static void
vfs_setdirty(struct buf *bp) {
	int i;
	vm_object_t object;
	vm_offset_t boffset, offset;
	/*
	 * We qualify the scan for modified pages on whether the
	 * object has been flushed yet.  The OBJ_WRITEABLE flag
	 * is not cleared simply by protecting pages off.
	 */
	if ((bp->b_flags & B_VMIO) &&
		((object = bp->b_pages[0]->object)->flags & (OBJ_WRITEABLE|OBJ_CLEANING))) {
		/*
		 * test the pages to see if they have been modified directly
		 * by users through the VM system.
		 */
		for (i = 0; i < bp->b_npages; i++)
			vm_page_test_dirty(bp->b_pages[i]);

		/*
		 * scan forwards for the first page modified
		 */
		for (i = 0; i < bp->b_npages; i++) {
			if (bp->b_pages[i]->dirty) {
				break;
			}
		}
		boffset = (i << PAGE_SHIFT);
		if (boffset < bp->b_dirtyoff) {
			bp->b_dirtyoff = boffset;
		}

		/*
		 * scan backwards for the last page modified
		 */
		for (i = bp->b_npages - 1; i >= 0; --i) {
			if (bp->b_pages[i]->dirty) {
				break;
			}
		}
		boffset = (i + 1);
		offset = boffset + bp->b_pages[0]->pindex;
		if (offset >= object->size)
			boffset = object->size - bp->b_pages[0]->pindex;
		if (bp->b_dirtyend < (boffset << PAGE_SHIFT))
			bp->b_dirtyend = (boffset << PAGE_SHIFT);
	}
}

/*
 * Get a block given a specified block and offset into a file/device.
 */
struct buf *
getblk(struct vnode * vp, daddr_t blkno, int size, int slpflag, int slptimeo)
{
	struct buf *bp;
	int s;
	struct bufhashhdr *bh;
	int maxsize;
	static pid_t flushing = 0;

	if (vp->v_mount) {
		maxsize = vp->v_mount->mnt_stat.f_iosize;
		/*
		 * This happens on mount points.
		 */
		if (maxsize < size)
			maxsize = size;
	} else {
		maxsize = size;
	}

#if !defined(MAX_PERF)
	if (size > MAXBSIZE)
		panic("getblk: size(%d) > MAXBSIZE(%d)\n", size, MAXBSIZE);
#endif

	s = splbio();
loop:
	if (numfreebuffers < lofreebuffers) {
		waitfreebuffers(slpflag, slptimeo);
	}
		
	if ((bp = gbincore(vp, blkno))) {
		if (bp->b_flags & B_BUSY) {
			bp->b_flags |= B_WANTED;
			if (bp->b_usecount < BUF_MAXUSE)
				++bp->b_usecount;
			if (!tsleep(bp,
				(PRIBIO + 1) | slpflag, "getblk", slptimeo))
				goto loop;

			splx(s);
			return (struct buf *) NULL;
		}
		bp->b_flags |= B_BUSY | B_CACHE;
		bremfree(bp);

		/*
		 * check for size inconsistancies (note that they shouldn't
		 * happen but do when filesystems don't handle the size changes
		 * correctly.) We are conservative on metadata and don't just
		 * extend the buffer but write and re-constitute it.
		 */

		if (bp->b_bcount != size) {
			if ((bp->b_flags & B_VMIO) && (size <= bp->b_kvasize)) {
				allocbuf(bp, size);
			} else {
				bp->b_flags |= B_NOCACHE;
				VOP_BWRITE(bp);
				goto loop;
			}
		}

		if (bp->b_usecount < BUF_MAXUSE)
			++bp->b_usecount;
		splx(s);
		return (bp);
	} else {
		vm_object_t obj;

		if ((bp = getnewbuf(vp, slpflag, slptimeo, size, maxsize)) == 0) {
			if (slpflag || slptimeo) {
				splx(s);
				return NULL;
			}
			goto loop;
		}

		/*
		 * This code is used to make sure that a buffer is not
		 * created while the getnewbuf routine is blocked.
		 * Normally the vnode is locked so this isn't a problem.
		 * VBLK type I/O requests, however, don't lock the vnode.
		 */
		if (!VOP_ISLOCKED(vp) && gbincore(vp, blkno)) {
			bp->b_flags |= B_INVAL;
			brelse(bp);
			goto loop;
		}

		/*
		 * Insert the buffer into the hash, so that it can
		 * be found by incore.
		 */
		bp->b_blkno = bp->b_lblkno = blkno;
		bgetvp(vp, bp);
		LIST_REMOVE(bp, b_hash);
		bh = BUFHASH(vp, blkno);
		LIST_INSERT_HEAD(bh, bp, b_hash);

		if ((obj = vp->v_object) && (vp->v_flag & VVMIO)) {
			bp->b_flags |= (B_VMIO | B_CACHE);
#if defined(VFS_BIO_DEBUG)
			if (vp->v_type != VREG && vp->v_type != VBLK)
				printf("getblk: vmioing file type %d???\n", vp->v_type);
#endif
		} else {
			bp->b_flags &= ~B_VMIO;
		}
		splx(s);

		allocbuf(bp, size);
#ifdef	PC98
		/*
		 * 1024byte/sector support
		 */
#define B_XXX2 0x8000000
		if (vp->v_flag & 0x10000) bp->b_flags |= B_XXX2;
#endif
		return (bp);
	}
}

/*
 * Get an empty, disassociated buffer of given size.
 */
struct buf *
geteblk(int size)
{
	struct buf *bp;
	int s;

	s = splbio();
	while ((bp = getnewbuf(0, 0, 0, size, MAXBSIZE)) == 0);
	splx(s);
	allocbuf(bp, size);
	bp->b_flags |= B_INVAL;
	return (bp);
}


/*
 * This code constitutes the buffer memory from either anonymous system
 * memory (in the case of non-VMIO operations) or from an associated
 * VM object (in the case of VMIO operations).
 *
 * Note that this code is tricky, and has many complications to resolve
 * deadlock or inconsistant data situations.  Tread lightly!!!
 *
 * Modify the length of a buffer's underlying buffer storage without
 * destroying information (unless, of course the buffer is shrinking).
 */
int
allocbuf(struct buf * bp, int size)
{

	int s;
	int newbsize, mbsize;
	int i;

#if !defined(MAX_PERF)
	if (!(bp->b_flags & B_BUSY))
		panic("allocbuf: buffer not busy");

	if (bp->b_kvasize < size)
		panic("allocbuf: buffer too small");
#endif

	if ((bp->b_flags & B_VMIO) == 0) {
		caddr_t origbuf;
		int origbufsize;
		/*
		 * Just get anonymous memory from the kernel
		 */
		mbsize = (size + DEV_BSIZE - 1) & ~(DEV_BSIZE - 1);
#if !defined(NO_B_MALLOC)
		if (bp->b_flags & B_MALLOC)
			newbsize = mbsize;
		else
#endif
			newbsize = round_page(size);

		if (newbsize < bp->b_bufsize) {
#if !defined(NO_B_MALLOC)
			/*
			 * malloced buffers are not shrunk
			 */
			if (bp->b_flags & B_MALLOC) {
				if (newbsize) {
					bp->b_bcount = size;
				} else {
					free(bp->b_data, M_BIOBUF);
					bufspace -= bp->b_bufsize;
					bufmallocspace -= bp->b_bufsize;
					bp->b_data = bp->b_kvabase;
					bp->b_bufsize = 0;
					bp->b_bcount = 0;
					bp->b_flags &= ~B_MALLOC;
				}
				return 1;
			}		
#endif
			vm_hold_free_pages(
			    bp,
			    (vm_offset_t) bp->b_data + newbsize,
			    (vm_offset_t) bp->b_data + bp->b_bufsize);
		} else if (newbsize > bp->b_bufsize) {
#if !defined(NO_B_MALLOC)
			/*
			 * We only use malloced memory on the first allocation.
			 * and revert to page-allocated memory when the buffer grows.
			 */
			if ( (bufmallocspace < maxbufmallocspace) &&
				(bp->b_bufsize == 0) &&
				(mbsize <= PAGE_SIZE/2)) {

				bp->b_data = malloc(mbsize, M_BIOBUF, M_WAITOK);
				bp->b_bufsize = mbsize;
				bp->b_bcount = size;
				bp->b_flags |= B_MALLOC;
				bufspace += mbsize;
				bufmallocspace += mbsize;
				return 1;
			}
#endif
			origbuf = NULL;
			origbufsize = 0;
#if !defined(NO_B_MALLOC)
			/*
			 * If the buffer is growing on it's other-than-first allocation,
			 * then we revert to the page-allocation scheme.
			 */
			if (bp->b_flags & B_MALLOC) {
				origbuf = bp->b_data;
				origbufsize = bp->b_bufsize;
				bp->b_data = bp->b_kvabase;
				bufspace -= bp->b_bufsize;
				bufmallocspace -= bp->b_bufsize;
				bp->b_bufsize = 0;
				bp->b_flags &= ~B_MALLOC;
				newbsize = round_page(newbsize);
			}
#endif
			vm_hold_load_pages(
			    bp,
			    (vm_offset_t) bp->b_data + bp->b_bufsize,
			    (vm_offset_t) bp->b_data + newbsize);
#if !defined(NO_B_MALLOC)
			if (origbuf) {
				bcopy(origbuf, bp->b_data, origbufsize);
				free(origbuf, M_BIOBUF);
			}
#endif
		}
	} else {
		vm_page_t m;
		int desiredpages;

		newbsize = (size + DEV_BSIZE - 1) & ~(DEV_BSIZE - 1);
		desiredpages = (round_page(newbsize) >> PAGE_SHIFT);

#if !defined(NO_B_MALLOC)
		if (bp->b_flags & B_MALLOC)
			panic("allocbuf: VMIO buffer can't be malloced");
#endif

		if (newbsize < bp->b_bufsize) {
			if (desiredpages < bp->b_npages) {
				for (i = desiredpages; i < bp->b_npages; i++) {
					/*
					 * the page is not freed here -- it
					 * is the responsibility of vnode_pager_setsize
					 */
					m = bp->b_pages[i];
#if defined(DIAGNOSTIC)
					if (m == bogus_page)
						panic("allocbuf: bogus page found");
#endif
					s = splvm();
					while ((m->flags & PG_BUSY) || (m->busy != 0)) {
						m->flags |= PG_WANTED;
						tsleep(m, PVM, "biodep", 0);
					}
					splx(s);

					bp->b_pages[i] = NULL;
					vm_page_unwire(m);
				}
				pmap_qremove((vm_offset_t) trunc_page(bp->b_data) +
				    (desiredpages << PAGE_SHIFT), (bp->b_npages - desiredpages));
				bp->b_npages = desiredpages;
			}
		} else if (newbsize > bp->b_bufsize) {
			vm_object_t obj;
			vm_offset_t tinc, toff;
			vm_ooffset_t off;
			vm_pindex_t objoff;
			int pageindex, curbpnpages;
			struct vnode *vp;
			int bsize;

			vp = bp->b_vp;

			if (vp->v_type == VBLK)
				bsize = DEV_BSIZE;
			else
				bsize = vp->v_mount->mnt_stat.f_iosize;

			if (bp->b_npages < desiredpages) {
				obj = vp->v_object;
				tinc = PAGE_SIZE;
				if (tinc > bsize)
					tinc = bsize;
				off = (vm_ooffset_t) bp->b_lblkno * bsize;
				curbpnpages = bp->b_npages;
		doretry:
				bp->b_flags |= B_CACHE;
				bp->b_validoff = bp->b_validend = 0;
				for (toff = 0; toff < newbsize; toff += tinc) {
					int bytesinpage;

					pageindex = toff >> PAGE_SHIFT;
					objoff = OFF_TO_IDX(off + toff);
					if (pageindex < curbpnpages) {

						m = bp->b_pages[pageindex];
#ifdef VFS_BIO_DIAG
						if (m->pindex != objoff)
							panic("allocbuf: page changed offset??!!!?");
#endif
						bytesinpage = tinc;
						if (tinc > (newbsize - toff))
							bytesinpage = newbsize - toff;
						if (bp->b_flags & B_CACHE)
							vfs_buf_set_valid(bp, off, toff, bytesinpage, m);
						continue;
					}
					m = vm_page_lookup(obj, objoff);
					if (!m) {
						m = vm_page_alloc(obj, objoff, VM_ALLOC_NORMAL);
						if (!m) {
							VM_WAIT;
							goto doretry;
						}
						/*
						 * Normally it is unwise to clear PG_BUSY without
						 * PAGE_WAKEUP -- but it is okay here, as there is
						 * no chance for blocking between here and vm_page_alloc
						 */
						m->flags &= ~PG_BUSY;
						vm_page_wire(m);
						bp->b_flags &= ~B_CACHE;
					} else if (m->flags & PG_BUSY) {
						s = splvm();
						if (m->flags & PG_BUSY) {
							m->flags |= PG_WANTED;
							tsleep(m, PVM, "pgtblk", 0);
						}
						splx(s);
						goto doretry;
					} else {
						if ((curproc != pageproc) &&
							((m->queue - m->pc) == PQ_CACHE) &&
						    ((cnt.v_free_count + cnt.v_cache_count) <
								(cnt.v_free_min + cnt.v_cache_min))) {
							pagedaemon_wakeup();
						}
						bytesinpage = tinc;
						if (tinc > (newbsize - toff))
							bytesinpage = newbsize - toff;
						if (bp->b_flags & B_CACHE)
							vfs_buf_set_valid(bp, off, toff, bytesinpage, m);
						vm_page_wire(m);
					}
					bp->b_pages[pageindex] = m;
					curbpnpages = pageindex + 1;
				}
				if (vp->v_tag == VT_NFS && 
				    vp->v_type != VBLK) {
					if (bp->b_dirtyend > 0) {
						bp->b_validoff = min(bp->b_validoff, bp->b_dirtyoff);
						bp->b_validend = max(bp->b_validend, bp->b_dirtyend);
					}
					if (bp->b_validend == 0)
						bp->b_flags &= ~B_CACHE;
				}
				bp->b_data = (caddr_t) trunc_page(bp->b_data);
				bp->b_npages = curbpnpages;
				pmap_qenter((vm_offset_t) bp->b_data,
					bp->b_pages, bp->b_npages);
				((vm_offset_t) bp->b_data) |= off & PAGE_MASK;
			}
		}
	}
	if (bp->b_flags & B_VMIO)
		vmiospace += (newbsize - bp->b_bufsize);
	bufspace += (newbsize - bp->b_bufsize);
	bp->b_bufsize = newbsize;
	bp->b_bcount = size;
	return 1;
}

/*
 * Wait for buffer I/O completion, returning error status.
 */
int
biowait(register struct buf * bp)
{
	int s;

	s = splbio();
	while ((bp->b_flags & B_DONE) == 0)
#if defined(NO_SCHEDULE_MODS)
		tsleep(bp, PRIBIO, "biowait", 0);
#else
		tsleep(bp, curproc->p_usrpri, "biowait", 0);
#endif
	splx(s);
	if (bp->b_flags & B_EINTR) {
		bp->b_flags &= ~B_EINTR;
		return (EINTR);
	}
	if (bp->b_flags & B_ERROR) {
		return (bp->b_error ? bp->b_error : EIO);
	} else {
		return (0);
	}
}

/*
 * Finish I/O on a buffer, calling an optional function.
 * This is usually called from interrupt level, so process blocking
 * is not *a good idea*.
 */
void
biodone(register struct buf * bp)
{
	int s;

	s = splbio();

#if !defined(MAX_PERF)
	if (!(bp->b_flags & B_BUSY))
		panic("biodone: buffer not busy");
#endif

	if (bp->b_flags & B_DONE) {
		splx(s);
#if !defined(MAX_PERF)
		printf("biodone: buffer already done\n");
#endif
		return;
	}
	bp->b_flags |= B_DONE;

	if ((bp->b_flags & B_READ) == 0) {
		vwakeup(bp);
	}
#ifdef BOUNCE_BUFFERS
	if (bp->b_flags & B_BOUNCE)
		vm_bounce_free(bp);
#endif

	/* call optional completion function if requested */
	if (bp->b_flags & B_CALL) {
		bp->b_flags &= ~B_CALL;
		(*bp->b_iodone) (bp);
		splx(s);
		return;
	}
	if (bp->b_flags & B_VMIO) {
		int i, resid;
		vm_ooffset_t foff;
		vm_page_t m;
		vm_object_t obj;
		int iosize;
		struct vnode *vp = bp->b_vp;

		obj = vp->v_object;

#if defined(VFS_BIO_DEBUG)
		if (vp->v_usecount == 0) {
			panic("biodone: zero vnode ref count");
		}

		if (vp->v_object == NULL) {
			panic("biodone: missing VM object");
		}

		if ((vp->v_flag & VVMIO) == 0) {
			panic("biodone: vnode is not setup for merged cache");
		}
#endif

		if (vp->v_type == VBLK)
			foff = (vm_ooffset_t) DEV_BSIZE * bp->b_lblkno;
		else
			foff = (vm_ooffset_t) vp->v_mount->mnt_stat.f_iosize * bp->b_lblkno;
#if !defined(MAX_PERF)
		if (!obj) {
			panic("biodone: no object");
		}
#endif
#if defined(VFS_BIO_DEBUG)
		if (obj->paging_in_progress < bp->b_npages) {
			printf("biodone: paging in progress(%d) < bp->b_npages(%d)\n",
			    obj->paging_in_progress, bp->b_npages);
		}
#endif
		iosize = bp->b_bufsize;
		for (i = 0; i < bp->b_npages; i++) {
			int bogusflag = 0;
			m = bp->b_pages[i];
			if (m == bogus_page) {
				bogusflag = 1;
				m = vm_page_lookup(obj, OFF_TO_IDX(foff));
				if (!m) {
#if defined(VFS_BIO_DEBUG)
					printf("biodone: page disappeared\n");
#endif
					--obj->paging_in_progress;
					continue;
				}
				bp->b_pages[i] = m;
				pmap_qenter(trunc_page(bp->b_data), bp->b_pages, bp->b_npages);
			}
#if defined(VFS_BIO_DEBUG)
			if (OFF_TO_IDX(foff) != m->pindex) {
				printf("biodone: foff(%d)/m->pindex(%d) mismatch\n", foff, m->pindex);
			}
#endif
			resid = IDX_TO_OFF(m->pindex + 1) - foff;
			if (resid > iosize)
				resid = iosize;
			/*
			 * In the write case, the valid and clean bits are
			 * already changed correctly, so we only need to do this
			 * here in the read case.
			 */
			if ((bp->b_flags & B_READ) && !bogusflag && resid > 0) {
				vfs_page_set_valid(bp, foff, i, m);
			}

			/*
			 * when debugging new filesystems or buffer I/O methods, this
			 * is the most common error that pops up.  if you see this, you
			 * have not set the page busy flag correctly!!!
			 */
			if (m->busy == 0) {
#if !defined(MAX_PERF)
				printf("biodone: page busy < 0, "
				    "pindex: %d, foff: 0x(%x,%x), "
				    "resid: %d, index: %d\n",
				    (int) m->pindex, (int)(foff >> 32),
						(int) foff & 0xffffffff, resid, i);
#endif
				if (vp->v_type != VBLK)
#if !defined(MAX_PERF)
					printf(" iosize: %ld, lblkno: %d, flags: 0x%lx, npages: %d\n",
					    bp->b_vp->v_mount->mnt_stat.f_iosize,
					    (int) bp->b_lblkno,
					    bp->b_flags, bp->b_npages);
				else
					printf(" VDEV, lblkno: %d, flags: 0x%lx, npages: %d\n",
					    (int) bp->b_lblkno,
					    bp->b_flags, bp->b_npages);
				printf(" valid: 0x%x, dirty: 0x%x, wired: %d\n",
				    m->valid, m->dirty, m->wire_count);
#endif
				panic("biodone: page busy < 0\n");
			}
			--m->busy;
			if ((m->busy == 0) && (m->flags & PG_WANTED)) {
				m->flags &= ~PG_WANTED;
				wakeup(m);
			}
			--obj->paging_in_progress;
			foff += resid;
			iosize -= resid;
		}
		if (obj && obj->paging_in_progress == 0 &&
		    (obj->flags & OBJ_PIPWNT)) {
			obj->flags &= ~OBJ_PIPWNT;
			wakeup(obj);
		}
	}
	/*
	 * For asynchronous completions, release the buffer now. The brelse
	 * checks for B_WANTED and will do the wakeup there if necessary - so
	 * no need to do a wakeup here in the async case.
	 */

	if (bp->b_flags & B_ASYNC) {
		if ((bp->b_flags & (B_NOCACHE | B_INVAL | B_ERROR | B_RELBUF)) != 0)
			brelse(bp);
		else
			bqrelse(bp);
	} else {
		bp->b_flags &= ~B_WANTED;
		wakeup(bp);
	}
	splx(s);
}

int
count_lock_queue()
{
	int count;
	struct buf *bp;

	count = 0;
	for (bp = TAILQ_FIRST(&bufqueues[QUEUE_LOCKED]);
	    bp != NULL;
	    bp = TAILQ_NEXT(bp, b_freelist))
		count++;
	return (count);
}

int vfs_update_interval = 30;

static void
vfs_update()
{
	while (1) {
		tsleep(&vfs_update_wakeup, PUSER, "update",
		    hz * vfs_update_interval);
		vfs_update_wakeup = 0;
		sync(curproc, NULL, NULL);
	}
}

static int
sysctl_kern_updateinterval SYSCTL_HANDLER_ARGS
{
	int error = sysctl_handle_int(oidp,
		oidp->oid_arg1, oidp->oid_arg2, req);
	if (!error)
		wakeup(&vfs_update_wakeup);
	return error;
}

SYSCTL_PROC(_kern, KERN_UPDATEINTERVAL, update, CTLTYPE_INT|CTLFLAG_RW,
	&vfs_update_interval, 0, sysctl_kern_updateinterval, "I", "");


/*
 * This routine is called in lieu of iodone in the case of
 * incomplete I/O.  This keeps the busy status for pages
 * consistant.
 */
void
vfs_unbusy_pages(struct buf * bp)
{
	int i;

	if (bp->b_flags & B_VMIO) {
		struct vnode *vp = bp->b_vp;
		vm_object_t obj = vp->v_object;
		vm_ooffset_t foff;

		foff = (vm_ooffset_t) vp->v_mount->mnt_stat.f_iosize * bp->b_lblkno;

		for (i = 0; i < bp->b_npages; i++) {
			vm_page_t m = bp->b_pages[i];

			if (m == bogus_page) {
				m = vm_page_lookup(obj, OFF_TO_IDX(foff) + i);
#if !defined(MAX_PERF)
				if (!m) {
					panic("vfs_unbusy_pages: page missing\n");
				}
#endif
				bp->b_pages[i] = m;
				pmap_qenter(trunc_page(bp->b_data), bp->b_pages, bp->b_npages);
			}
			--obj->paging_in_progress;
			--m->busy;
			if ((m->busy == 0) && (m->flags & PG_WANTED)) {
				m->flags &= ~PG_WANTED;
				wakeup(m);
			}
		}
		if (obj->paging_in_progress == 0 &&
		    (obj->flags & OBJ_PIPWNT)) {
			obj->flags &= ~OBJ_PIPWNT;
			wakeup(obj);
		}
	}
}

/*
 * Set NFS' b_validoff and b_validend fields from the valid bits
 * of a page.  If the consumer is not NFS, and the page is not
 * valid for the entire range, clear the B_CACHE flag to force
 * the consumer to re-read the page.
 */
static void
vfs_buf_set_valid(struct buf *bp,
		  vm_ooffset_t foff, vm_offset_t off, vm_offset_t size,
		  vm_page_t m)
{
	if (bp->b_vp->v_tag == VT_NFS && bp->b_vp->v_type != VBLK) {
		vm_offset_t svalid, evalid;
		int validbits = m->valid;

		/*
		 * This only bothers with the first valid range in the
		 * page.
		 */
		svalid = off;
		while (validbits && !(validbits & 1)) {
			svalid += DEV_BSIZE;
			validbits >>= 1;
		}
		evalid = svalid;
		while (validbits & 1) {
			evalid += DEV_BSIZE;
			validbits >>= 1;
		}
		/*
		 * Make sure this range is contiguous with the range
		 * built up from previous pages.  If not, then we will
		 * just use the range from the previous pages.
		 */
		if (svalid == bp->b_validend) {
			bp->b_validoff = min(bp->b_validoff, svalid);
			bp->b_validend = max(bp->b_validend, evalid);
		}
	} else if (!vm_page_is_valid(m,
				     (vm_offset_t) ((foff + off) & PAGE_MASK),
				     size)) {
		bp->b_flags &= ~B_CACHE;
	}
}

/*
 * Set the valid bits in a page, taking care of the b_validoff,
 * b_validend fields which NFS uses to optimise small reads.  Off is
 * the offset within the file and pageno is the page index within the buf.
 */
static void
vfs_page_set_valid(struct buf *bp, vm_ooffset_t off, int pageno, vm_page_t m)
{
	struct vnode *vp = bp->b_vp;
	vm_ooffset_t soff, eoff;

	soff = off;
	eoff = off + min(PAGE_SIZE, bp->b_bufsize);
	vm_page_set_invalid(m,
			    (vm_offset_t) (soff & PAGE_MASK),
			    (vm_offset_t) (eoff - soff));
	if (vp->v_tag == VT_NFS && vp->v_type != VBLK) {
		vm_ooffset_t sv, ev;
		off = off - pageno * PAGE_SIZE;
		sv = off + ((bp->b_validoff + DEV_BSIZE - 1) & ~(DEV_BSIZE - 1));
		ev = off + (bp->b_validend & ~(DEV_BSIZE - 1));
		soff = max(sv, soff);
		eoff = min(ev, eoff);
	}
	if (eoff > soff)
		vm_page_set_validclean(m,
				       (vm_offset_t) (soff & PAGE_MASK),
				       (vm_offset_t) (eoff - soff));
}

/*
 * This routine is called before a device strategy routine.
 * It is used to tell the VM system that paging I/O is in
 * progress, and treat the pages associated with the buffer
 * almost as being PG_BUSY.  Also the object paging_in_progress
 * flag is handled to make sure that the object doesn't become
 * inconsistant.
 */
void
vfs_busy_pages(struct buf * bp, int clear_modify)
{
	int i;

	if (bp->b_flags & B_VMIO) {
		struct vnode *vp = bp->b_vp;
		vm_object_t obj = vp->v_object;
		vm_ooffset_t foff;

		if (vp->v_type == VBLK)
			foff = (vm_ooffset_t) DEV_BSIZE * bp->b_lblkno;
		else
			foff = (vm_ooffset_t) vp->v_mount->mnt_stat.f_iosize * bp->b_lblkno;
		vfs_setdirty(bp);
		for (i = 0; i < bp->b_npages; i++, foff += PAGE_SIZE) {
			vm_page_t m = bp->b_pages[i];

			if ((bp->b_flags & B_CLUSTER) == 0) {
				obj->paging_in_progress++;
				m->busy++;
			}
			vm_page_protect(m, VM_PROT_NONE);
			if (clear_modify)
				vfs_page_set_valid(bp, foff, i, m);
			else if (bp->b_bcount >= PAGE_SIZE) {
				if (m->valid && (bp->b_flags & B_CACHE) == 0) {
					bp->b_pages[i] = bogus_page;
					pmap_qenter(trunc_page(bp->b_data), bp->b_pages, bp->b_npages);
				}
			}
		}
	}
}

/*
 * Tell the VM system that the pages associated with this buffer
 * are clean.  This is used for delayed writes where the data is
 * going to go to disk eventually without additional VM intevention.
 */
void
vfs_clean_pages(struct buf * bp)
{
	int i;

	if (bp->b_flags & B_VMIO) {
		struct vnode *vp = bp->b_vp;
		vm_object_t obj = vp->v_object;
		vm_ooffset_t foff;

		if (vp->v_type == VBLK)
			foff = (vm_ooffset_t) DEV_BSIZE * bp->b_lblkno;
		else
			foff = (vm_ooffset_t) vp->v_mount->mnt_stat.f_iosize * bp->b_lblkno;
		for (i = 0; i < bp->b_npages; i++, foff += PAGE_SIZE) {
			vm_page_t m = bp->b_pages[i];

			vfs_page_set_valid(bp, foff, i, m);
		}
	}
}

void
vfs_bio_clrbuf(struct buf *bp) {
	int i;
	if( bp->b_flags & B_VMIO) {
		if( (bp->b_npages == 1) && (bp->b_bufsize < PAGE_SIZE)) {
			int mask;
			mask = 0;
			for(i=0;i<bp->b_bufsize;i+=DEV_BSIZE)
				mask |= (1 << (i/DEV_BSIZE));
			if( bp->b_pages[0]->valid != mask) {
				bzero(bp->b_data, bp->b_bufsize);
			}
			bp->b_pages[0]->valid = mask;
			bp->b_resid = 0;
			return;
		}
		for(i=0;i<bp->b_npages;i++) {
			if( bp->b_pages[i]->valid == VM_PAGE_BITS_ALL)
				continue;
			if( bp->b_pages[i]->valid == 0) {
				if ((bp->b_pages[i]->flags & PG_ZERO) == 0) {
					bzero(bp->b_data + (i << PAGE_SHIFT), PAGE_SIZE);
				}
			} else {
				int j;
				for(j=0;j<PAGE_SIZE/DEV_BSIZE;j++) {
					if( (bp->b_pages[i]->valid & (1<<j)) == 0)
						bzero(bp->b_data + (i << PAGE_SHIFT) + j * DEV_BSIZE, DEV_BSIZE);
				}
			}
			/* bp->b_pages[i]->valid = VM_PAGE_BITS_ALL; */
		}
		bp->b_resid = 0;
	} else {
		clrbuf(bp);
	}
}

/*
 * vm_hold_load_pages and vm_hold_unload pages get pages into
 * a buffers address space.  The pages are anonymous and are
 * not associated with a file object.
 */
void
vm_hold_load_pages(struct buf * bp, vm_offset_t from, vm_offset_t to)
{
	vm_offset_t pg;
	vm_page_t p;
	int index;

	to = round_page(to);
	from = round_page(from);
	index = (from - trunc_page(bp->b_data)) >> PAGE_SHIFT;

	for (pg = from; pg < to; pg += PAGE_SIZE, index++) {

tryagain:

		p = vm_page_alloc(kernel_object,
			((pg - VM_MIN_KERNEL_ADDRESS) >> PAGE_SHIFT),
		    VM_ALLOC_NORMAL);
		if (!p) {
			VM_WAIT;
			goto tryagain;
		}
		vm_page_wire(p);
		pmap_kenter(pg, VM_PAGE_TO_PHYS(p));
		bp->b_pages[index] = p;
		PAGE_WAKEUP(p);
	}
	bp->b_npages = index;
}

void
vm_hold_free_pages(struct buf * bp, vm_offset_t from, vm_offset_t to)
{
	vm_offset_t pg;
	vm_page_t p;
	int index, newnpages;

	from = round_page(from);
	to = round_page(to);
	newnpages = index = (from - trunc_page(bp->b_data)) >> PAGE_SHIFT;

	for (pg = from; pg < to; pg += PAGE_SIZE, index++) {
		p = bp->b_pages[index];
		if (p && (index < bp->b_npages)) {
#if !defined(MAX_PERF)
			if (p->busy) {
				printf("vm_hold_free_pages: blkno: %d, lblkno: %d\n",
					bp->b_blkno, bp->b_lblkno);
			}
#endif
			bp->b_pages[index] = NULL;
			pmap_kremove(pg);
			vm_page_unwire(p);
			vm_page_free(p);
		}
	}
	bp->b_npages = newnpages;
}


#include "opt_ddb.h"
#ifdef DDB
#include <ddb/ddb.h>

DB_SHOW_COMMAND(buffer, db_show_buffer)
{
	/* get args */
	struct buf *bp = (struct buf *)addr;

	if (!have_addr) {
		db_printf("usage: show buffer <addr>\n");
		return;
	}

	db_printf("b_proc = %p,\nb_flags = 0x%b\n", (void *)bp->b_proc,
		  bp->b_flags, "\20\40bounce\37cluster\36vmio\35ram\34ordered"
		  "\33paging\32xxx\31writeinprog\30wanted\27relbuf\26tape"
		  "\25read\24raw\23phys\22clusterok\21malloc\20nocache"
		  "\17locked\16inval\15gathered\14error\13eintr\12done\11dirty"
		  "\10delwri\7call\6cache\5busy\4bad\3async\2needcommit\1age");
	db_printf("b_error = %d, b_bufsize = %ld, b_bcount = %ld, "
		  "b_resid = %ld\nb_dev = 0x%x, b_un.b_addr = %p, "
		  "b_blkno = %d, b_pblkno = %d\n",
		  bp->b_error, bp->b_bufsize, bp->b_bcount, bp->b_resid,
		  bp->b_dev, bp->b_un.b_addr, bp->b_blkno, bp->b_pblkno);
	if (bp->b_npages) {
		int i;
		db_printf("b_npages = %d, pages(OBJ, IDX, PA): ", bp->b_npages);
		for (i = 0; i < bp->b_npages; i++) {
			vm_page_t m;
			m = bp->b_pages[i];
			db_printf("(0x%x, 0x%x, 0x%x)", m->object, m->pindex,
				VM_PAGE_TO_PHYS(m));
			if ((i + 1) < bp->b_npages)
				db_printf(",");
		}
		db_printf("\n");
	}
}
#endif /* DDB */
