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
 * $Id: vfs_bio.c,v 1.17 1995/01/09 16:04:52 davidg Exp $
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

#define VMIO
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <vm/vm.h>
#include <vm/vm_pageout.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/resourcevar.h>
#include <sys/proc.h>

#include <miscfs/specfs/specdev.h>

struct buf *buf;		/* buffer header pool */
int nbuf;			/* number of buffer headers calculated
				 * elsewhere */
struct swqueue bswlist;
int nvmio, nlru;

extern vm_map_t buffer_map, io_map, kernel_map, pager_map;

void vm_hold_free_pages(struct buf * bp, vm_offset_t from, vm_offset_t to);
void vm_hold_load_pages(struct buf * bp, vm_offset_t from, vm_offset_t to);
void vfs_dirty_pages(struct buf * bp);
void vfs_busy_pages(struct buf *, int clear_modify);

int needsbuffer;

/*
 * Internal update daemon, process 3
 *	The variable vfs_update_wakeup allows for internal syncs.
 */
int vfs_update_wakeup;


/*
 * buffers base kva
 */
caddr_t buffers_kva;

/*
 * bogus page -- for I/O to/from partially complete buffers
 */
vm_page_t bogus_page;
vm_offset_t bogus_offset;

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

	buffers_kva = (caddr_t) kmem_alloc_pageable(buffer_map, MAXBSIZE * nbuf);
	/* finally, initialize each buffer header and stick on empty q */
	for (i = 0; i < nbuf; i++) {
		bp = &buf[i];
		bzero(bp, sizeof *bp);
		bp->b_flags = B_INVAL;	/* we're just an empty header */
		bp->b_dev = NODEV;
		bp->b_vp = NULL;
		bp->b_rcred = NOCRED;
		bp->b_wcred = NOCRED;
		bp->b_qindex = QUEUE_EMPTY;
		bp->b_vnbufs.le_next = NOLIST;
		bp->b_data = buffers_kva + i * MAXBSIZE;
		TAILQ_INSERT_TAIL(&bufqueues[QUEUE_EMPTY], bp, b_freelist);
		LIST_INSERT_HEAD(&invalhash, bp, b_hash);
	}

	bogus_offset = kmem_alloc_pageable(kernel_map, PAGE_SIZE);
	bogus_page = vm_page_alloc(kernel_object, bogus_offset - VM_MIN_KERNEL_ADDRESS, 0);

}

/*
 * remove the buffer from the appropriate free list
 */
void
bremfree(struct buf * bp)
{
	int s = splbio();

	if (bp->b_qindex != QUEUE_NONE) {
		if (bp->b_qindex == QUEUE_LRU)
			--nlru;
		TAILQ_REMOVE(&bufqueues[bp->b_qindex], bp, b_freelist);
		bp->b_qindex = QUEUE_NONE;
	} else {
		panic("bremfree: removing a buffer when not on a queue");
	}
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
		if (curproc && curproc->p_stats)	/* count block I/O */
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
	} else if (bp->b_lblkno == bp->b_blkno) {
		VOP_BMAP(vp, bp->b_lblkno, (struct vnode **) 0,
		    &bp->b_blkno, (int *) 0);
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
		if (curproc && curproc->p_stats)	/* count block I/O */
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
	} else if (bp->b_lblkno == bp->b_blkno) {
		VOP_BMAP(vp, bp->b_lblkno, (struct vnode **) 0,
		    &bp->b_blkno, (int *) 0);
	}
	for (i = 0; i < cnt; i++, rablkno++, rabsize++) {
		if (inmem(vp, *rablkno))
			continue;
		rabp = getblk(vp, *rablkno, *rabsize, 0, 0);

		if ((rabp->b_flags & B_CACHE) == 0) {
			if (curproc && curproc->p_stats)
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
 * this routine is used by filesystems to get at pages in the PG_CACHE
 * queue.  also, it is used to read pages that are currently being
 * written out by the file i/o routines.
 */
int
vfs_read_bypass(struct vnode * vp, struct uio * uio, int maxread, daddr_t lbn)
{
	vm_page_t m;
	vm_offset_t kv;
	int nread;
	int error;
	struct buf *bp, *bpa;
	vm_object_t obj;
	int off;
	int nrest;
	int flags;
	int s;

	return 0;
	/*
	 * don't use the bypass mechanism for non-vmio vnodes
	 */
	if ((vp->v_flag & VVMIO) == 0)
		return 0;
	/*
	 * get the VM object (it has the pages)
	 */
	obj = (vm_object_t) vp->v_vmdata;
	if (obj == NULL)
		return 0;

	/*
	 * if there is a buffer that is not busy, it is faster to use it.
	 * This like read-ahead, etc work better
	 */

	s = splbio();
	if ((bp = incore(vp, lbn)) &&
	    (((bp->b_flags & B_READ) && (bp->b_flags & B_BUSY))
		|| (bp->b_flags & B_BUSY) == 0)) {
		splx(s);
		return 0;
	}
	splx(s);

	/*
	 * get a pbuf --> we just use the kva
	 */
	kv = kmem_alloc_wait(pager_map, PAGE_SIZE);
	nread = 0;
	error = 0;

	while (!error && uio->uio_resid && maxread > 0) {
		int po;
		int count;
		int s;

relookup:
		/*
		 * lookup the page
		 */
		m = vm_page_lookup(obj, trunc_page(uio->uio_offset));
		if (!m)
			break;
		/*
		 * get the offset into the page, and the amount to read in the
		 * page
		 */
		nrest = round_page(uio->uio_offset) - uio->uio_offset;
		if (nrest > uio->uio_resid)
			nrest = uio->uio_resid;

		/*
		 * check the valid bits for the page (DEV_BSIZE chunks)
		 */
		if (!vm_page_is_valid(m, uio->uio_offset, nrest))
			break;

		/*
		 * if the page is busy, wait for it
		 */
		s = splhigh();
		if (!m->valid || (m->flags & PG_BUSY)) {
			m->flags |= PG_WANTED;
			tsleep((caddr_t) m, PVM, "vnibyp", 0);
			splx(s);
			goto relookup;
		}
		/*
		 * if the page is on the cache queue, remove it -- cache queue
		 * pages should be freeable by vm_page_alloc anytime.
		 */
		if (m->flags & PG_CACHE) {
			if (cnt.v_free_count + cnt.v_cache_count < cnt.v_free_reserved) {
				VM_WAIT;
				goto relookup;
			}
			vm_page_unqueue(m);
		}
		/*
		 * add a buffer mapping (essentially wires the page too).
		 */
		m->bmapped++;
		splx(s);

		/*
		 * enter it into the kva
		 */
		pmap_qenter(kv, &m, 1);

		/*
		 * do the copy
		 */
		po = uio->uio_offset & (PAGE_SIZE - 1);
		count = PAGE_SIZE - po;
		if (count > maxread)
			count = maxread;
		if (count > uio->uio_resid)
			count = uio->uio_resid;

		error = uiomove((caddr_t) kv + po, count, uio);
		if (!error) {
			nread += count;
			maxread -= count;
		}
		/*
		 * remove from kva
		 */
		pmap_qremove(kv, 1);
		PAGE_WAKEUP(m);	/* XXX probably unnecessary */
		/*
		 * If the page was on the cache queue, then by definition
		 * bmapped was 0. Thus the following case will also take care
		 * of the page being removed from the cache queue above.
		 * Also, it is possible that the page was already entered onto
		 * another queue (or was already there), so we don't put it
		 * onto the cache queue...
		 */
		m->bmapped--;
		if (m->bmapped == 0 &&
		    (m->flags & (PG_CACHE | PG_ACTIVE | PG_INACTIVE)) == 0 &&
		    m->wire_count == 0) {
			vm_page_test_dirty(m);

			/*
			 * make sure that the darned page is on a queue
			 * somewhere...
			 */
			if ((m->dirty & m->valid) == 0) {
				vm_page_cache(m);
			} else if (m->hold_count == 0) {
				vm_page_deactivate(m);
			} else {
				vm_page_activate(m);
			}
		}
	}
	/*
	 * release our buffer(kva).
	 */
	kmem_free_wakeup(pager_map, kv, PAGE_SIZE);
	return nread;
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
	if (!(bp->b_flags & B_BUSY))
		panic("bwrite: buffer is not busy???");

	bp->b_flags &= ~(B_READ | B_DONE | B_ERROR | B_DELWRI);
	bp->b_flags |= B_WRITEINPROG;

	if (oldflags & B_ASYNC) {
		if (oldflags & B_DELWRI) {
			reassignbuf(bp, bp->b_vp);
		} else if (curproc) {
			++curproc->p_stats->p_ru.ru_oublock;
		}
	}
	bp->b_vp->v_numoutput++;
	vfs_busy_pages(bp, 1);
	VOP_STRATEGY(bp);

	if ((oldflags & B_ASYNC) == 0) {
		int rtval = biowait(bp);

		if (oldflags & B_DELWRI) {
			reassignbuf(bp, bp->b_vp);
		} else if (curproc) {
			++curproc->p_stats->p_ru.ru_oublock;
		}
		brelse(bp);
		return (rtval);
	}
	return (0);
}

int
vn_bwrite(ap)
	struct vop_bwrite_args *ap;
{
	return (bwrite(ap->a_bp));
}

/*
 * Delayed write. (Buffer is marked dirty).
 */
void
bdwrite(struct buf * bp)
{

	if ((bp->b_flags & B_BUSY) == 0) {
		panic("bdwrite: buffer is not busy");
	}
	if (bp->b_flags & B_INVAL) {
		brelse(bp);
		return;
	}
	if (bp->b_flags & B_TAPE) {
		bawrite(bp);
		return;
	}
	bp->b_flags &= ~B_READ;
	vfs_dirty_pages(bp);
	if ((bp->b_flags & B_DELWRI) == 0) {
		if (curproc)
			++curproc->p_stats->p_ru.ru_oublock;
		bp->b_flags |= B_DONE | B_DELWRI;
		reassignbuf(bp, bp->b_vp);
	}
	brelse(bp);
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
	if (((bp->b_flags & B_DELWRI) == 0) && (bp->b_vp->v_numoutput > 24)) {
		int s = splbio();

		while (bp->b_vp->v_numoutput > 16) {
			bp->b_vp->v_flag |= VBWAIT;
			tsleep((caddr_t) &bp->b_vp->v_numoutput, PRIBIO, "bawnmo", 0);
		}
		splx(s);
	}
	bp->b_flags |= B_ASYNC;
	(void) bwrite(bp);
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

	if (needsbuffer) {
		needsbuffer = 0;
		wakeup((caddr_t) &needsbuffer);
	}
	/* anyone need this block? */
	if (bp->b_flags & B_WANTED) {
		bp->b_flags &= ~(B_PDWANTED | B_WANTED | B_AGE);
		wakeup((caddr_t) bp);
	} else if (bp->b_flags & B_VMIO) {
		bp->b_flags &= ~(B_WANTED | B_PDWANTED);
		wakeup((caddr_t) bp);
	}
	if (bp->b_flags & B_LOCKED)
		bp->b_flags &= ~B_ERROR;

	if ((bp->b_flags & (B_NOCACHE | B_INVAL | B_ERROR)) ||
	    (bp->b_bufsize <= 0)) {
		bp->b_flags |= B_INVAL;
		bp->b_flags &= ~(B_DELWRI | B_CACHE);
		if (((bp->b_flags & B_VMIO) == 0) && bp->b_vp)
			brelvp(bp);
	}
	if (bp->b_flags & B_VMIO) {
		vm_offset_t foff;
		vm_object_t obj;
		int i, resid;
		vm_page_t m;
		int iototal = bp->b_bufsize;

		foff = 0;
		obj = 0;
		if (bp->b_npages) {
			if (bp->b_vp && bp->b_vp->v_mount) {
				foff = bp->b_vp->v_mount->mnt_stat.f_iosize * bp->b_lblkno;
			} else {
				/*
				 * vnode pointer has been ripped away --
				 * probably file gone...
				 */
				foff = bp->b_pages[0]->offset;
			}
		}
		for (i = 0; i < bp->b_npages; i++) {
			m = bp->b_pages[i];
			if (m == bogus_page) {
				panic("brelse: bogus page found");
			}
			resid = (m->offset + PAGE_SIZE) - foff;
			if (resid > iototal)
				resid = iototal;
			if (resid > 0) {
				if (bp->b_flags & (B_ERROR | B_NOCACHE)) {
					vm_page_set_invalid(m, foff, resid);
				} else if ((bp->b_flags & B_DELWRI) == 0) {
					vm_page_set_clean(m, foff, resid);
					vm_page_set_valid(m, foff, resid);
				}
			} else {
				vm_page_test_dirty(m);
			}
			if (bp->b_flags & B_INVAL) {
				if (m->bmapped == 0) {
					panic("brelse: bmapped is zero for page\n");
				}
				--m->bmapped;
				if (m->bmapped == 0) {
					PAGE_WAKEUP(m);
					if ((m->dirty & m->valid) == 0)
						vm_page_cache(m);
				}
			}
			foff += resid;
			iototal -= resid;
		}

		if (bp->b_flags & B_INVAL) {
			pmap_qremove(trunc_page((vm_offset_t) bp->b_data), bp->b_npages);
			bp->b_npages = 0;
			bp->b_bufsize = 0;
			bp->b_flags &= ~B_VMIO;
			if (bp->b_vp)
				brelvp(bp);
			--nvmio;
		}
	}
	if (bp->b_qindex != QUEUE_NONE)
		panic("brelse: free buffer onto another queue???");

	/* enqueue */
	/* buffers with no memory */
	if (bp->b_bufsize == 0) {
		bp->b_qindex = QUEUE_EMPTY;
		TAILQ_INSERT_TAIL(&bufqueues[QUEUE_EMPTY], bp, b_freelist);
		LIST_REMOVE(bp, b_hash);
		LIST_INSERT_HEAD(&invalhash, bp, b_hash);
		bp->b_dev = NODEV;
		/* buffers with junk contents */
	} else if (bp->b_flags & (B_ERROR | B_INVAL | B_NOCACHE)) {
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
		if (bp->b_flags & B_VMIO)
			bp->b_qindex = QUEUE_VMIO;
		else {
			bp->b_qindex = QUEUE_LRU;
			++nlru;
		}
		TAILQ_INSERT_TAIL(&bufqueues[bp->b_qindex], bp, b_freelist);
	}

	/* unlock */
	bp->b_flags &= ~(B_PDWANTED | B_WANTED | B_BUSY | B_ASYNC | B_NOCACHE | B_AGE);
	splx(s);
}

/*
 * this routine implements clustered async writes for
 * clearing out B_DELWRI buffers...
 */
void
vfs_bio_awrite(struct buf * bp)
{
	int i;
	daddr_t lblkno = bp->b_lblkno;
	struct vnode *vp = bp->b_vp;
	int size = vp->v_mount->mnt_stat.f_iosize;
	int s;
	int ncl;
	struct buf *bpa;

	s = splbio();
	ncl = 1;
	if (vp->v_flag & VVMIO) {
		for (i = 1; i < MAXPHYS / size; i++) {
			if ((bpa = incore(vp, lblkno + i)) &&
			    ((bpa->b_flags & (B_BUSY | B_DELWRI | B_BUSY | B_CLUSTEROK | B_INVAL)) == B_DELWRI | B_CLUSTEROK) &&
			    (bpa->b_bufsize == size)) {
				if ((bpa->b_blkno == bpa->b_lblkno) ||
				    (bpa->b_blkno != bp->b_blkno + (i * size) / DEV_BSIZE))
					break;
			} else {
				break;
			}
		}
		ncl = i;
	}
	/*
	 * we don't attempt to cluster meta-data or INVALID??? buffers
	 */
	if ((ncl != 1) &&
	    (bp->b_flags & (B_INVAL | B_CLUSTEROK)) == B_CLUSTEROK) {
		cluster_wbuild(vp, NULL, size, lblkno, ncl, -1);
	} else {
		bremfree(bp);
		bp->b_flags |= B_BUSY | B_ASYNC;
		bwrite(bp);
	}
	splx(s);
}

int freebufspace;
int allocbufspace;

/*
 * Find a buffer header which is available for use.
 */
struct buf *
getnewbuf(int slpflag, int slptimeo, int doingvmio)
{
	struct buf *bp;
	int s;
	int firstbp = 1;

	s = splbio();
start:
	/* can we constitute a new buffer? */
	if ((bp = bufqueues[QUEUE_EMPTY].tqh_first)) {
		if (bp->b_qindex != QUEUE_EMPTY)
			panic("getnewbuf: inconsistent EMPTY queue");
		bremfree(bp);
		goto fillbuf;
	}
	/*
	 * we keep the file I/O from hogging metadata I/O
	 */
	if (bp = bufqueues[QUEUE_AGE].tqh_first) {
		if (bp->b_qindex != QUEUE_AGE)
			panic("getnewbuf: inconsistent AGE queue");
	} else if ((nvmio > (2 * nbuf / 3))
	    && (bp = bufqueues[QUEUE_VMIO].tqh_first)) {
		if (bp->b_qindex != QUEUE_VMIO)
			panic("getnewbuf: inconsistent VMIO queue");
	} else if ((!doingvmio || (nlru > (2 * nbuf / 3))) &&
	    (bp = bufqueues[QUEUE_LRU].tqh_first)) {
		if (bp->b_qindex != QUEUE_LRU)
			panic("getnewbuf: inconsistent LRU queue");
	}
	if (!bp) {
		if (doingvmio) {
			if (bp = bufqueues[QUEUE_VMIO].tqh_first) {
				if (bp->b_qindex != QUEUE_VMIO)
					panic("getnewbuf: inconsistent VMIO queue");
			} else if (bp = bufqueues[QUEUE_LRU].tqh_first) {
				if (bp->b_qindex != QUEUE_LRU)
					panic("getnewbuf: inconsistent LRU queue");
			}
		} else {
			if (bp = bufqueues[QUEUE_LRU].tqh_first) {
				if (bp->b_qindex != QUEUE_LRU)
					panic("getnewbuf: inconsistent LRU queue");
			} else if (bp = bufqueues[QUEUE_VMIO].tqh_first) {
				if (bp->b_qindex != QUEUE_VMIO)
					panic("getnewbuf: inconsistent VMIO queue");
			}
		}
	}
	if (!bp) {
		/* wait for a free buffer of any kind */
		needsbuffer = 1;
		tsleep((caddr_t) &needsbuffer, PRIBIO | slpflag, "newbuf", slptimeo);
		splx(s);
		return (0);
	}
	/* if we are a delayed write, convert to an async write */
	if ((bp->b_flags & (B_DELWRI | B_INVAL)) == B_DELWRI) {
		vfs_bio_awrite(bp);
		if (!slpflag && !slptimeo) {
			splx(s);
			return (0);
		}
		goto start;
	}
	bremfree(bp);

	if (bp->b_flags & B_VMIO) {
		bp->b_flags |= B_INVAL | B_BUSY;
		brelse(bp);
		bremfree(bp);
	}
	if (bp->b_vp)
		brelvp(bp);

	/* we are not free, nor do we contain interesting data */
	if (bp->b_rcred != NOCRED)
		crfree(bp->b_rcred);
	if (bp->b_wcred != NOCRED)
		crfree(bp->b_wcred);
fillbuf:
	bp->b_flags = B_BUSY;
	LIST_REMOVE(bp, b_hash);
	LIST_INSERT_HEAD(&invalhash, bp, b_hash);
	splx(s);
	if (bp->b_bufsize) {
		allocbuf(bp, 0, 0);
	}
	bp->b_dev = NODEV;
	bp->b_vp = NULL;
	bp->b_blkno = bp->b_lblkno = 0;
	bp->b_iodone = 0;
	bp->b_error = 0;
	bp->b_resid = 0;
	bp->b_bcount = 0;
	bp->b_npages = 0;
	bp->b_wcred = bp->b_rcred = NOCRED;
	bp->b_data = buffers_kva + (bp - buf) * MAXBSIZE;
	bp->b_dirtyoff = bp->b_dirtyend = 0;
	bp->b_validoff = bp->b_validend = 0;
	return (bp);
}

/*
 * Check to see if a block is currently memory resident.
 */
struct buf *
incore(struct vnode * vp, daddr_t blkno)
{
	struct buf *bp;
	struct bufhashhdr *bh;

	int s = splbio();

	bh = BUFHASH(vp, blkno);
	bp = bh->lh_first;

	/* Search hash chain */
	while (bp) {
		/* hit */
		if (bp->b_lblkno == blkno && bp->b_vp == vp
		    && (bp->b_flags & B_INVAL) == 0) {
			splx(s);
			return (bp);
		}
		bp = bp->b_hash.le_next;
	}
	splx(s);

	return (0);
}

/*
 * returns true if no I/O is needed to access the
 * associated VM object.
 */

int
inmem(struct vnode * vp, daddr_t blkno)
{
	vm_object_t obj;
	vm_offset_t off, toff, tinc;
	vm_page_t m;

	if (incore(vp, blkno))
		return 1;
	if (vp->v_mount == 0)
		return 0;
	if (vp->v_vmdata == 0)
		return 0;

	obj = (vm_object_t) vp->v_vmdata;
	tinc = PAGE_SIZE;
	if (tinc > vp->v_mount->mnt_stat.f_iosize)
		tinc = vp->v_mount->mnt_stat.f_iosize;
	off = blkno * vp->v_mount->mnt_stat.f_iosize;

	for (toff = 0; toff < vp->v_mount->mnt_stat.f_iosize; toff += tinc) {
		int mask;

		m = vm_page_lookup(obj, trunc_page(toff + off));
		if (!m)
			return 0;
		if (vm_page_is_valid(m, toff + off, tinc) == 0)
			return 0;
	}
	return 1;
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
	vm_offset_t off;
	int bsize;
	int nleft;

	bsize = DEV_BSIZE;
	if (vp->v_mount) {
		bsize = vp->v_mount->mnt_stat.f_iosize;
	}
	s = splbio();
loop:
	if ((cnt.v_free_count + cnt.v_cache_count) <
	    cnt.v_free_reserved + MAXBSIZE / PAGE_SIZE)
		wakeup((caddr_t) &vm_pages_needed);
	if (bp = incore(vp, blkno)) {
		if (bp->b_flags & B_BUSY) {
			bp->b_flags |= B_WANTED;
			if (curproc == pageproc) {
				bp->b_flags |= B_PDWANTED;
				wakeup((caddr_t) &cnt.v_free_count);
			}
			if (!tsleep((caddr_t) bp, PRIBIO | slpflag, "getblk", slptimeo))
				goto loop;
			splx(s);
			return (struct buf *) NULL;
		}
		bp->b_flags |= B_BUSY | B_CACHE;
		bremfree(bp);
		/*
		 * check for size inconsistancies
		 */
		if (bp->b_bcount != size) {
#if defined(VFS_BIO_DEBUG)
			printf("getblk: invalid buffer size: %ld\n", bp->b_bcount);
#endif
			bp->b_flags |= B_INVAL;
			bwrite(bp);
			goto loop;
		}
		splx(s);
		return (bp);
	} else {
		vm_object_t obj;
		int doingvmio;

		if ((obj = (vm_object_t) vp->v_vmdata) &&
		    (vp->v_flag & VVMIO) /* && (blkno >= 0) */ ) {
			doingvmio = 1;
		} else {
			doingvmio = 0;
		}
		if ((bp = getnewbuf(slpflag, slptimeo, doingvmio)) == 0) {
			if (slpflag || slptimeo)
				return NULL;
			goto loop;
		}
		if (incore(vp, blkno)) {
			bp->b_flags |= B_INVAL;
			brelse(bp);
			goto loop;
		}
		bp->b_blkno = bp->b_lblkno = blkno;
		bgetvp(vp, bp);
		LIST_REMOVE(bp, b_hash);
		bh = BUFHASH(vp, blkno);
		LIST_INSERT_HEAD(bh, bp, b_hash);
		if (doingvmio) {
			bp->b_flags |= (B_VMIO | B_CACHE);
#if defined(VFS_BIO_DEBUG)
			if (vp->v_type != VREG)
				printf("getblk: vmioing file type %d???\n", vp->v_type);
#endif
			++nvmio;
		} else {
			if (bp->b_flags & B_VMIO)
				--nvmio;
			bp->b_flags &= ~B_VMIO;
		}
		splx(s);
		if (!allocbuf(bp, size, 1)) {
			s = splbio();
			goto loop;
		}
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

	while ((bp = getnewbuf(0, 0, 0)) == 0);
	allocbuf(bp, size, 0);
	bp->b_flags |= B_INVAL;
	return (bp);
}

/*
 * Modify the length of a buffer's underlying buffer storage without
 * destroying information (unless, of course the buffer is shrinking).
 */
int
allocbuf(struct buf * bp, int size, int vmio)
{

	int s;
	int newbsize;
	int i;

	if ((bp->b_flags & B_VMIO) == 0) {
		newbsize = round_page(size);
		if (newbsize == bp->b_bufsize) {
			bp->b_bcount = size;
			return 1;
		} else if (newbsize < bp->b_bufsize) {
			if (bp->b_flags & B_MALLOC) {
				bp->b_bcount = size;
				return 1;
			}
			vm_hold_free_pages(
			    bp,
			    (vm_offset_t) bp->b_data + newbsize,
			    (vm_offset_t) bp->b_data + bp->b_bufsize);
		} else if (newbsize > bp->b_bufsize) {
			if (bp->b_flags & B_MALLOC) {
				vm_offset_t bufaddr;

				bufaddr = (vm_offset_t) bp->b_data;
				bp->b_data = buffers_kva + (bp - buf) * MAXBSIZE;
				vm_hold_load_pages(
				    bp,
				    (vm_offset_t) bp->b_data,
				    (vm_offset_t) bp->b_data + newbsize);
				bcopy((caddr_t) bufaddr, bp->b_data, bp->b_bcount);
				free((caddr_t) bufaddr, M_TEMP);
			} else if ((newbsize <= PAGE_SIZE / 2) && (bp->b_bufsize == 0)) {
				bp->b_flags |= B_MALLOC;
				bp->b_data = malloc(newbsize, M_TEMP, M_WAITOK);
				bp->b_npages = 0;
			} else {
				vm_hold_load_pages(
				    bp,
				    (vm_offset_t) bp->b_data + bp->b_bufsize,
				    (vm_offset_t) bp->b_data + newbsize);
			}
		}
		/*
		 * adjust buffer cache's idea of memory allocated to buffer
		 * contents
		 */
		freebufspace -= newbsize - bp->b_bufsize;
		allocbufspace += newbsize - bp->b_bufsize;
	} else {
		vm_page_t m;
		int desiredpages;

		newbsize = ((size + DEV_BSIZE - 1) / DEV_BSIZE) * DEV_BSIZE;
		desiredpages = round_page(newbsize) / PAGE_SIZE;

		if (newbsize == bp->b_bufsize) {
			bp->b_bcount = size;
			return 1;
		} else if (newbsize < bp->b_bufsize) {
			if (desiredpages < bp->b_npages) {
				pmap_qremove((vm_offset_t) trunc_page(bp->b_data) +
				    desiredpages * PAGE_SIZE, (bp->b_npages - desiredpages));
				for (i = desiredpages; i < bp->b_npages; i++) {
					m = bp->b_pages[i];
					s = splhigh();
					if ((m->flags & PG_BUSY) || (m->busy != 0)) {
						m->flags |= PG_WANTED;
						tsleep(m, PVM, "biodep", 0);
					}
					splx(s);

					if (m->bmapped == 0) {
						printf("allocbuf: bmapped is zero for page %d\n", i);
						panic("allocbuf: error");
					}
					--m->bmapped;
					if (m->bmapped == 0) {
						PAGE_WAKEUP(m);
						pmap_page_protect(VM_PAGE_TO_PHYS(m), VM_PROT_NONE);
						vm_page_free(m);
					}
					bp->b_pages[i] = NULL;
				}
				bp->b_npages = desiredpages;
			}
		} else {
			vm_object_t obj;
			vm_offset_t tinc, off, toff, objoff;
			int pageindex, curbpnpages;
			struct vnode *vp;
			int bsize;

			vp = bp->b_vp;
			bsize = vp->v_mount->mnt_stat.f_iosize;

			if (bp->b_npages < desiredpages) {
				obj = (vm_object_t) vp->v_vmdata;
				tinc = PAGE_SIZE;
				if (tinc > bsize)
					tinc = bsize;
				off = bp->b_lblkno * bsize;
				curbpnpages = bp->b_npages;
		doretry:
				for (toff = 0; toff < newbsize; toff += tinc) {
					int mask;
					int bytesinpage;

					pageindex = toff / PAGE_SIZE;
					objoff = trunc_page(toff + off);
					if (pageindex < curbpnpages) {
						int pb;

						m = bp->b_pages[pageindex];
						if (m->offset != objoff)
							panic("allocbuf: page changed offset??!!!?");
						bytesinpage = tinc;
						if (tinc > (newbsize - toff))
							bytesinpage = newbsize - toff;
						if (!vm_page_is_valid(m, toff + off, bytesinpage)) {
							bp->b_flags &= ~B_CACHE;
						}
						if ((m->flags & PG_ACTIVE) == 0)
							vm_page_activate(m);
						continue;
					}
					m = vm_page_lookup(obj, objoff);
					if (!m) {
						m = vm_page_alloc(obj, objoff, 0);
						if (!m) {
							int j;

							for (j = bp->b_npages; j < pageindex; j++) {
								vm_page_t mt = bp->b_pages[j];

								PAGE_WAKEUP(mt);
								if (!mt->valid) {
									vm_page_free(mt);
								}
							}
							VM_WAIT;
							if (vmio && (bp->b_flags & B_PDWANTED)) {
								--nvmio;
								bp->b_flags &= ~B_VMIO;
								bp->b_flags |= B_INVAL;
								brelse(bp);
								return 0;
							}
							curbpnpages = bp->b_npages;
							goto doretry;
						}
						m->valid = 0;
						vm_page_activate(m);
					} else if ((m->valid == 0) || (m->flags & PG_BUSY)) {
						int j;
						int bufferdestroyed = 0;

						for (j = bp->b_npages; j < pageindex; j++) {
							vm_page_t mt = bp->b_pages[j];

							PAGE_WAKEUP(mt);
							if (mt->valid == 0) {
								vm_page_free(mt);
							}
						}
						if (vmio && (bp->b_flags & B_PDWANTED)) {
							--nvmio;
							bp->b_flags &= ~B_VMIO;
							bp->b_flags |= B_INVAL;
							brelse(bp);
							VM_WAIT;
							bufferdestroyed = 1;
						}
						s = splbio();
						if (m) {
							m->flags |= PG_WANTED;
							tsleep(m, PRIBIO, "pgtblk", 0);
						}
						splx(s);
						if (bufferdestroyed)
							return 0;
						curbpnpages = bp->b_npages;
						goto doretry;
					} else {
						int pb;

						if ((m->flags & PG_CACHE) &&
						    (cnt.v_free_count + cnt.v_cache_count) < cnt.v_free_reserved) {
							int j;

							for (j = bp->b_npages; j < pageindex; j++) {
								vm_page_t mt = bp->b_pages[j];

								PAGE_WAKEUP(mt);
								if (mt->valid == 0) {
									vm_page_free(mt);
								}
							}
							VM_WAIT;
							if (vmio && (bp->b_flags & B_PDWANTED)) {
								--nvmio;
								bp->b_flags &= ~B_VMIO;
								bp->b_flags |= B_INVAL;
								brelse(bp);
								return 0;
							}
							curbpnpages = bp->b_npages;
							goto doretry;
						}
						bytesinpage = tinc;
						if (tinc > (newbsize - toff))
							bytesinpage = newbsize - toff;
						if (!vm_page_is_valid(m, toff + off, bytesinpage)) {
							bp->b_flags &= ~B_CACHE;
						}
						if ((m->flags & PG_ACTIVE) == 0)
							vm_page_activate(m);
						m->flags |= PG_BUSY;
					}
					bp->b_pages[pageindex] = m;
					curbpnpages = pageindex + 1;
				}
				if (bsize >= PAGE_SIZE) {
					for (i = bp->b_npages; i < curbpnpages; i++) {
						m = bp->b_pages[i];
						if (m->valid == 0) {
							bp->b_flags &= ~B_CACHE;
						}
						m->bmapped++;
						PAGE_WAKEUP(m);
					}
				} else {
					if (!vm_page_is_valid(bp->b_pages[0], off, bsize))
						bp->b_flags &= ~B_CACHE;
					bp->b_pages[0]->bmapped++;
					PAGE_WAKEUP(bp->b_pages[0]);
				}
				bp->b_npages = curbpnpages;
				bp->b_data = buffers_kva + (bp - buf) * MAXBSIZE;
				pmap_qenter((vm_offset_t) bp->b_data, bp->b_pages, bp->b_npages);
				bp->b_data += off % PAGE_SIZE;
			}
		}
	}
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
		tsleep((caddr_t) bp, PRIBIO, "biowait", 0);
	if ((bp->b_flags & B_ERROR) || bp->b_error) {
		if ((bp->b_flags & B_INVAL) == 0) {
			bp->b_flags |= B_INVAL;
			bp->b_dev = NODEV;
			LIST_REMOVE(bp, b_hash);
			LIST_INSERT_HEAD(&invalhash, bp, b_hash);
			wakeup((caddr_t) bp);
		}
		if (!bp->b_error)
			bp->b_error = EIO;
		else
			bp->b_flags |= B_ERROR;
		splx(s);
		return (bp->b_error);
	} else {
		splx(s);
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
	if (bp->b_flags & B_DONE)
		printf("biodone: buffer already done\n");
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
		vm_offset_t foff;
		vm_page_t m;
		vm_object_t obj;
		int iosize;
		struct vnode *vp = bp->b_vp;

		foff = vp->v_mount->mnt_stat.f_iosize * bp->b_lblkno;
		obj = (vm_object_t) vp->v_vmdata;
		if (!obj) {
			return;
		}
#if defined(VFS_BIO_DEBUG)
		if (obj->paging_in_progress < bp->b_npages) {
			printf("biodone: paging in progress(%d) < bp->b_npages(%d)\n",
			    obj->paging_in_progress, bp->b_npages);
		}
#endif
		iosize = bp->b_bufsize;
		for (i = 0; i < bp->b_npages; i++) {
			m = bp->b_pages[i];
			if (m == bogus_page) {
				m = vm_page_lookup(obj, foff);
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
			if (trunc_page(foff) != m->offset) {
				printf("biodone: foff(%d)/m->offset(%d) mismatch\n", foff, m->offset);
			}
#endif
			resid = (m->offset + PAGE_SIZE) - foff;
			if (resid > iosize)
				resid = iosize;
			if (resid > 0) {
				vm_page_set_valid(m, foff, resid);
				vm_page_set_clean(m, foff, resid);
			}
			if (m->busy == 0) {
				printf("biodone: page busy < 0, off: %d, foff: %d, resid: %d, index: %d\n",
				    m->offset, foff, resid, i);
				printf(" iosize: %d, lblkno: %d\n",
				    bp->b_vp->v_mount->mnt_stat.f_iosize, bp->b_lblkno);
				printf(" valid: 0x%x, dirty: 0x%x, mapped: %d\n",
				    m->valid, m->dirty, m->bmapped);
				panic("biodone: page busy < 0\n");
			}
			m->flags &= ~PG_FAKE;
			--m->busy;
			PAGE_WAKEUP(m);
			--obj->paging_in_progress;
			foff += resid;
			iosize -= resid;
		}
		if (obj && obj->paging_in_progress == 0)
			wakeup((caddr_t) obj);
	}
	/*
	 * For asynchronous completions, release the buffer now. The brelse
	 * checks for B_WANTED and will do the wakeup there if necessary - so
	 * no need to do a wakeup here in the async case.
	 */

	if (bp->b_flags & B_ASYNC) {
		brelse(bp);
	} else {
		bp->b_flags &= ~(B_WANTED | B_PDWANTED);
		wakeup((caddr_t) bp);
	}
	splx(s);
}

int
count_lock_queue()
{
	int count;
	struct buf *bp;

	count = 0;
	for (bp = bufqueues[QUEUE_LOCKED].tqh_first;
	    bp != NULL;
	    bp = bp->b_freelist.tqe_next)
		count++;
	return (count);
}

int vfs_update_interval = 30;

void
vfs_update()
{
	(void) spl0();
	while (1) {
		tsleep((caddr_t) &vfs_update_wakeup, PRIBIO, "update",
		    hz * vfs_update_interval);
		vfs_update_wakeup = 0;
		sync(curproc, NULL, NULL);
	}
}

void
vfs_unbusy_pages(struct buf * bp)
{
	int i;

	if (bp->b_flags & B_VMIO) {
		struct vnode *vp = bp->b_vp;
		vm_object_t obj = (vm_object_t) vp->v_vmdata;
		vm_offset_t foff;

		foff = vp->v_mount->mnt_stat.f_iosize * bp->b_lblkno;

		for (i = 0; i < bp->b_npages; i++) {
			vm_page_t m = bp->b_pages[i];

			if (m == bogus_page) {
				m = vm_page_lookup(obj, foff);
				if (!m) {
					panic("vfs_unbusy_pages: page missing\n");
				}
				bp->b_pages[i] = m;
				pmap_qenter(trunc_page(bp->b_data), bp->b_pages, bp->b_npages);
			}
			--obj->paging_in_progress;
			--m->busy;
			PAGE_WAKEUP(m);
		}
		if (obj->paging_in_progress == 0)
			wakeup((caddr_t) obj);
	}
}

void
vfs_busy_pages(struct buf * bp, int clear_modify)
{
	int i;

	if (bp->b_flags & B_VMIO) {
		vm_object_t obj = (vm_object_t) bp->b_vp->v_vmdata;
		vm_offset_t foff = bp->b_vp->v_mount->mnt_stat.f_iosize * bp->b_lblkno;
		int iocount = bp->b_bufsize;

		for (i = 0; i < bp->b_npages; i++) {
			vm_page_t m = bp->b_pages[i];
			int resid = (m->offset + PAGE_SIZE) - foff;

			if (resid > iocount)
				resid = iocount;
			obj->paging_in_progress++;
			m->busy++;
			if (clear_modify) {
				vm_page_test_dirty(m);
				pmap_page_protect(VM_PAGE_TO_PHYS(m), VM_PROT_READ);
			} else if (bp->b_bcount >= PAGE_SIZE) {
				if (m->valid && (bp->b_flags & B_CACHE) == 0) {
					bp->b_pages[i] = bogus_page;
					pmap_qenter(trunc_page(bp->b_data), bp->b_pages, bp->b_npages);
				}
			}
			foff += resid;
			iocount -= resid;
		}
	}
}

void
vfs_dirty_pages(struct buf * bp)
{
	int i;

	if (bp->b_flags & B_VMIO) {
		vm_offset_t foff = bp->b_vp->v_mount->mnt_stat.f_iosize * bp->b_lblkno;
		int iocount = bp->b_bufsize;

		for (i = 0; i < bp->b_npages; i++) {
			vm_page_t m = bp->b_pages[i];
			int resid = (m->offset + PAGE_SIZE) - foff;

			if (resid > iocount)
				resid = iocount;
			if (resid > 0) {
				vm_page_set_valid(m, foff, resid);
				vm_page_set_dirty(m, foff, resid);
			}
			PAGE_WAKEUP(m);
			foff += resid;
			iocount -= resid;
		}
	}
}
/*
 * these routines are not in the correct place (yet)
 * also they work *ONLY* for kernel_pmap!!!
 */
void
vm_hold_load_pages(struct buf * bp, vm_offset_t froma, vm_offset_t toa)
{
	vm_offset_t pg;
	vm_page_t p;
	vm_offset_t from = round_page(froma);
	vm_offset_t to = round_page(toa);

tryagain0:
	if ((curproc != pageproc) && ((cnt.v_free_count + cnt.v_cache_count) <=
		cnt.v_free_reserved + (toa - froma) / PAGE_SIZE)) {
		VM_WAIT;
		goto tryagain0;
	}
	for (pg = from; pg < to; pg += PAGE_SIZE) {

tryagain:

		p = vm_page_alloc(kernel_object, pg - VM_MIN_KERNEL_ADDRESS, 0);
		if (!p) {
			VM_WAIT;
			goto tryagain;
		}
		vm_page_wire(p);
		pmap_kenter(pg, VM_PAGE_TO_PHYS(p));
		bp->b_pages[((caddr_t) pg - bp->b_data) / PAGE_SIZE] = p;
		PAGE_WAKEUP(p);
		bp->b_npages++;
	}
}

void
vm_hold_free_pages(struct buf * bp, vm_offset_t froma, vm_offset_t toa)
{
	vm_offset_t pg;
	vm_page_t p;
	vm_offset_t from = round_page(froma);
	vm_offset_t to = round_page(toa);

	for (pg = from; pg < to; pg += PAGE_SIZE) {
		p = bp->b_pages[((caddr_t) pg - bp->b_data) / PAGE_SIZE];
		bp->b_pages[((caddr_t) pg - bp->b_data) / PAGE_SIZE] = 0;
		pmap_kremove(pg);
		vm_page_free(p);
		--bp->b_npages;
	}
}

void
bufstats()
{
}
