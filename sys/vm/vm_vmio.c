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
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 * $Id$
 */
/*
 * THIS IS PRELIMINARY, BUGGY AND NON-WORKING CODE
 * WHEN THIS NOTICE IS REMOVED -- IT WILL WORK... :-).
 *
 * THINGS TO DO:
 *	COMMENTS, THE SYSTEM ALMOST RUNS WITH IT.  THIS IS CURRENTLY MEANT
 *	ONLY AS A PLACEHOLDER!!!
 */
#define VMIO
#include "param.h"
#include "proc.h"
#include "malloc.h"
#include "vm_param.h"
#include "vm.h"
#include "lock.h"
#include "queue.h"
#include "vm_prot.h"
#include "vm_object.h"
#include "vm_page.h"
#include "vnode_pager.h"
#include "vm_map.h"
#include "vm_pageout.h"
#include "vnode.h"
#include "uio.h"
#include "mount.h"

/* #include "buf.h" */
#include "miscfs/specfs/specdev.h"
/* #include "vmio.h" */

int vnode_pager_initialized;
extern vm_map_t pager_map;

struct buf * getpbuf();
void relpbuf(struct buf *bp);

/*
 * map an object into kva
 * return 1 if all are in memory, and 0 if any are fake.
 */
int
vmio_alloc_pages( vm_object_t object, vm_offset_t start,
		vm_offset_t size, vm_page_t *ms) {

	int pagecount;
	vm_page_t m;
	int i,j;
	int s;
	vm_offset_t kva;
	struct buf *bp;
	int ioneeded=0;
	
	pagecount = size / PAGE_SIZE;
	
	for(i=0;i<pagecount;i++) {
		vm_page_t m;
		m = vm_page_lookup( object, start + i * PAGE_SIZE);
		if( m) {
			/*
			 * See if something else has already gotten this page
			 */
			if( m->busy || (m->flags & (PG_VMIO|PG_BUSY))) {
				/*
				 * Something has the page, so we have to
				 * release all the pages we've gotten to
				 * this point, wait for the page to unbusy
				 * and then start over.
				 */
				int j;
				for(j = 0; j < i; j++) {
					vm_page_t n;
					n = ms[j];
					if( n) {
						/*
						 * unbusy the page.
						 */
						PAGE_WAKEUP(n);
						if( n->flags & PG_FAKE)
							vm_page_free(n);
					}
				}
				m->flags |= PG_WANTED;
				tsleep((caddr_t)m, PVM, "vngpwt", 0);
				return -1;
			}
			m->flags |= PG_REFERENCED;
		} else {
			m = (vm_page_t) vm_page_alloc( object,
				start + i * PAGE_SIZE);
			if( !m) {
				VM_WAIT;
				for(j=0;j<i;j++) {
					vm_page_t n;
					n = ms[j];
					if( n) {
						PAGE_WAKEUP(n);
						if( n->flags & PG_FAKE)
							vm_page_free(n);
					}
				}
				return -1;
			}
		}
		ms[i] = m;
	}
	/*
	 * hold the pages and assign them to the mapping
	 */
	for(i=0;i<pagecount;i++) {
		m = ms[i];
		vm_page_hold(m);
		if( m->flags & PG_FAKE)
			++ioneeded;

		vm_page_deactivate(m);
		if( (m->flags & PG_ACTIVE) == 0)
			vm_page_activate(m);
		m->flags |= PG_VMIO;
		m->flags &= ~PG_BUSY;
		++m->act_count;
		pmap_page_protect( VM_PAGE_TO_PHYS(m), VM_PROT_READ);
	}
	return ioneeded;

}

void
vmio_rawiodone( struct buf *bp) {
	int s;
	int i;
	vm_object_t object;
	vm_page_t m;

	if( bp->b_bufsize != bp->b_bcount)
		bzero( bp->b_data + bp->b_bcount, bp->b_bufsize - bp->b_bcount);
	printf("rawdone: (blk: %d, count: %d)\n",
		bp->b_blkno, bp->b_bcount);
	s = splbio();
	object = bp->b_pages[0]->object;
	for( i = 0; i < bp->b_npages; i++) {
		m = bp->b_pages[i];
		if( m) {
			--m->busy;
			if( m->busy == 0) {
				m->flags |= PG_CLEAN;
				m->flags &= ~(PG_LAUNDRY|PG_FAKE);
				PAGE_WAKEUP(m);
			}
		} else {
			panic("vmio_rawiodone: page is gone!!!");
		}
	}
	HOLDRELE(bp->b_vp);
	relpbuf(bp);
	--object->paging_in_progress;
	if( object->paging_in_progress == 0)
		wakeup((caddr_t)object);
	splx(s);
	return;
}

void
vmio_get_pager(struct vnode *vp) {
	if( vp->v_type == VREG) {
		vm_object_t object;
		vm_pager_t pager;
		if((vp->v_vmdata == NULL) || (vp->v_flag & VVMIO) == 0) {
			pager = (vm_pager_t) vnode_pager_alloc(vp, 0, 0, 0);
			object = (vm_object_t) vp->v_vmdata;
			if( object->pager != pager)
				panic("vmio_get_pager: pager/object mismatch");
			(void) vm_object_lookup( pager);
			pager_cache( object, TRUE);
			vp->v_flag |= VVMIO;
		} else {
			object = (vm_object_t) vp->v_vmdata;
			pager = object->pager;
			if( pager == NULL) {
				panic("vmio_get_pager: pager missing");
			}
			(void) vm_object_lookup( pager);
		}
	}
}

void
vmio_free_pager( struct vnode *vp) {
	if( vp->v_vmdata == NULL)
		panic("vmio_free_pager: object missing");
	vm_object_deallocate( (vm_object_t) vp->v_vmdata);
}

void
vmio_aread( struct buf *lbp) {
	struct vnode *vp, *dp;
	vm_object_t object;
	vm_offset_t offset;
	vm_offset_t size;
	vm_offset_t off;
	int forcemore;
	int runp;
	int s;

	s = splbio();
	vp = lbp->b_vp;
	object = (vm_object_t) vp->v_vmdata;
	offset = trunc_page(lbp->b_lblkno * vp->v_mount->mnt_stat.f_iosize);
	size = round_page(lbp->b_bcount);
	forcemore = 0;
	printf("queueing read: iosize: %d, b_lblkno: %d, offset: %d, size: %d:",
		vp->v_mount->mnt_stat.f_iosize,
		lbp->b_lblkno, offset, size);

	for(off = 0; off < size; ) {
		vm_offset_t curoff;
		int pgidx, pgcnt, i;
		struct buf *bp;
		pgidx = off / PAGE_SIZE;
		if( !forcemore) {
			while( (off < size) &&
				(lbp->b_pages[pgidx]->flags & PG_FAKE) == 0) {
				off = trunc_page(off) + PAGE_SIZE;
				pgidx += 1;
			}
		}
		if( off >= size)
			break;
		bp = getpbuf();

		++object->paging_in_progress;

		curoff = offset + off;
		VOP_BMAP( vp, curoff / vp->v_mount->mnt_stat.f_iosize,
			&dp, &bp->b_blkno, &runp);
		
		bp->b_bcount = (runp + 1) * vp->v_mount->mnt_stat.f_iosize;
		if( off + bp->b_bcount > size)
			bp->b_bcount = size - off;
		bp->b_bufsize = round_page(bp->b_bcount);
			
		pgcnt = bp->b_bufsize / PAGE_SIZE;
		for(i=0;i<pgcnt;i++) {
			++lbp->b_pages[pgidx+i]->busy;
			bp->b_pages[i] = lbp->b_pages[pgidx+i];
			bp->b_pages[i]->flags |= PG_BUSY;
		}
		bp->b_npages = pgcnt;
		pmap_qenter((vm_offset_t) bp->b_data, &lbp->b_pages[pgidx], pgcnt);
		bp->b_data += curoff - trunc_page(curoff);
		off += bp->b_bcount;
		if( off & (PAGE_SIZE - 1))
			forcemore = 1;
		else
			forcemore = 0;
		/*
		 * round up physical size for real devices
		 */
		if( dp->v_type == VBLK || dp->v_type == VCHR) {
			bp->b_bcount = (bp->b_bcount + DEV_BSIZE - 1) & ~(DEV_BSIZE - 1);
		}
		/*
		 * and map the pages to be read into the kva
		 */
		VHOLD(vp);
		/* build a minimal buffer header */
		bp->b_flags = B_BUSY | B_READ | B_CALL | B_ASYNC;
		bp->b_iodone = vmio_rawiodone;
		/* B_PHYS is not set, but it is nice to fill this in */
		bp->b_proc = curproc;
		bp->b_rcred = bp->b_wcred = bp->b_proc->p_ucred;
		if( bp->b_rcred != NOCRED)
			crhold(bp->b_rcred);
		if( bp->b_wcred != NOCRED)
			crhold(bp->b_wcred);
		bp->b_lblkno = bp->b_blkno;
/*		bp->b_vp = dp; */
		bgetvp( dp, bp);
		printf(" p: (blk: %d, coff: %d, off: %d, count: %d)",
			bp->b_blkno, curoff, off, bp->b_bcount);

		/* Should be a BLOCK or character DEVICE if we get here */
		bp->b_dev = dp->v_rdev;

		/* do the input */
		VOP_STRATEGY(bp);
	}
	printf("\n");
	splx(s);
}

int
vmio_getblk( struct buf *bp) {
	struct vnode *vp;
	vm_object_t object;
	vm_offset_t offset;
	vm_offset_t size;
	vm_page_t ms[(MAXBSIZE + PAGE_SIZE - 1)/PAGE_SIZE];
	int rtval;

	vp = bp->b_vp;
	if( (object = (vm_object_t) vp->v_vmdata) == NULL)
		panic("vmio_build_bp: object missing");

	offset = trunc_page(bp->b_lblkno * vp->v_mount->mnt_stat.f_iosize);
	size = round_page(bp->b_bcount);
	printf("vmio_getblk: off:%d(0x%x) blk: %d, size: %d",
		offset, offset, bp->b_lblkno, size);
		
	bp->b_flags &= ~B_CACHE;
	if( (rtval = vmio_alloc_pages( object, offset, size, ms)) == 0) {
		bp->b_flags |= B_CACHE;
		printf("(cached)\n");
	} else {
		printf("(ioneeded:%d)\n", rtval);
	}

	if( rtval < 0) {
		printf("vmio_getblk: vmio_alloc_pages busy\n");
		return 0;
	}

	bp->b_npages = size / PAGE_SIZE;
	pmap_qenter((vm_offset_t) bp->b_data, ms, bp->b_npages);
	bp->b_flags |= B_VMIO;
	bp->b_bufsize = size;
	bcopy( ms, bp->b_pages, bp->b_npages * sizeof(vm_page_t));
	return 1;
}

int
vmio_biowait( struct buf *lbp) {
	int i;
	int s;
	printf("vmio_biowait -->");
restart:
	s = splbio();
	for(i=lbp->b_npages-1;i >= 0; --i) {
		if( lbp->b_pages[i]->busy || (lbp->b_pages[i]->flags & PG_BUSY)) {
			lbp->b_pages[i]->flags |= PG_WANTED;
			printf(" waiting on page %d of xfer at %d, flags: 0x%x\n",
				i, lbp->b_blkno, lbp->b_pages[i]->flags);
			tsleep( (caddr_t) lbp->b_pages[i], PVM, "vbiowt", 0);
			splx(s);
			goto restart;
		}
	}
	splx(s);
	printf("vmio_biowait: finished\n");
	return 1;
}

void
vmio_brelse( struct buf *lbp) {
	int i;
	int nfullpages = lbp->b_bcount / PAGE_SIZE;
	pmap_qremove((vm_offset_t) lbp->b_data, lbp->b_npages);
	for(i=0;i<lbp->b_npages;i++) {
		if( (i < nfullpages) && ((lbp->b_flags & B_DELWRI) == 0))
			lbp->b_pages[i]->flags |= PG_CLEAN;
		if( lbp->b_flags & B_DELWRI)
			lbp->b_pages[i]->flags &= ~PG_CLEAN;
		lbp->b_pages[i]->flags &= ~PG_VMIO;
		vm_page_unhold(lbp->b_pages[i]);
		PAGE_WAKEUP(lbp->b_pages[i]);
	}
	lbp->b_flags &= ~B_VMIO;
	lbp->b_flags |= B_INVAL;
	lbp->b_bufsize = 0;
}
