/*
 * Copyright (c) 1990 University of Utah.
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1993,1994 John S. Dyson
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
 *	from: @(#)vnode_pager.c	7.5 (Berkeley) 4/20/91
 *	$Id: vnode_pager.c,v 1.21 1994/06/22 05:53:12 jkh Exp $
 */

/*
 * Page to/from files (vnodes).
 *
 * TODO:
 *	pageouts
 *	fix credential use (uses current process credentials now)
 */

/*
 * MODIFICATIONS:
 * John S. Dyson  08 Dec 93
 *
 * This file in conjunction with some vm_fault mods, eliminate the performance
 * advantage for using the buffer cache and minimize memory copies.
 *
 * 1) Supports multiple - block reads
 * 2) Bypasses buffer cache for reads
 *
 * TODO:
 *
 * 1) Totally bypass buffer cache for reads
 *    (Currently will still sometimes use buffer cache for reads)
 * 2) Bypass buffer cache for writes
 *    (Code does not support it, but mods are simple)
 */

#include "param.h"
#include "proc.h"
#include "malloc.h"
#include "vnode.h"
#include "uio.h"
#include "mount.h"

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
#include "buf.h"
#include "specdev.h"

int     vnode_pager_putmulti();

struct pagerops vnodepagerops = {
	vnode_pager_init,
	vnode_pager_alloc,
	vnode_pager_dealloc,
	vnode_pager_getpage,
	vnode_pager_getmulti,
	vnode_pager_putpage,
	vnode_pager_putmulti,
	vnode_pager_haspage
};

static int vnode_pager_input(vn_pager_t vnp, vm_page_t * m, int count, int reqpage);
static int vnode_pager_output(vn_pager_t vnp, vm_page_t * m, int count, int *rtvals);
struct buf * getpbuf();
void relpbuf(struct buf * bp);

extern vm_map_t pager_map;

queue_head_t vnode_pager_list;	/* list of managed vnodes */

#define MAXBP (NBPG/DEV_BSIZE);

void
vnode_pager_init()
{
	queue_init(&vnode_pager_list);
}

/*
 * Allocate (or lookup) pager for a vnode.
 * Handle is a vnode pointer.
 */
vm_pager_t
vnode_pager_alloc(handle, size, prot, offset)
	caddr_t handle;
	vm_size_t size;
	vm_prot_t prot;
	vm_offset_t offset;
{
	register vm_pager_t pager;
	register vn_pager_t vnp;
	vm_object_t object;
	struct vattr vattr;
	struct vnode *vp;
	struct proc *p = curproc;	/* XXX */

	/*
	 * Pageout to vnode, no can do yet.
	 */
	if (handle == NULL)
		return (NULL);

	/*
	 * Vnodes keep a pointer to any associated pager so no need to lookup
	 * with vm_pager_lookup.
	 */
	vp = (struct vnode *) handle;
	pager = (vm_pager_t) vp->v_vmdata;
	if (pager == NULL) {

		/*
		 * Allocate pager structures
		 */
		pager = (vm_pager_t) malloc(sizeof *pager, M_VMPAGER, M_WAITOK);
		if (pager == NULL)
			return (NULL);
		vnp = (vn_pager_t) malloc(sizeof *vnp, M_VMPGDATA, M_WAITOK);
		if (vnp == NULL) {
			free((caddr_t) pager, M_VMPAGER);
			return (NULL);
		}

		/*
		 * And an object of the appropriate size
		 */
		if (VOP_GETATTR(vp, &vattr, p->p_ucred, p) == 0) {
			object = vm_object_allocate(round_page(vattr.va_size));
			vm_object_enter(object, pager);
			vm_object_setpager(object, pager, 0, TRUE);
		} else {
			free((caddr_t) vnp, M_VMPGDATA);
			free((caddr_t) pager, M_VMPAGER);
			return (NULL);
		}

		/*
		 * Hold a reference to the vnode and initialize pager data.
		 */
		VREF(vp);
		vnp->vnp_flags = 0;
		vnp->vnp_vp = vp;
		vnp->vnp_size = vattr.va_size;

		queue_enter(&vnode_pager_list, pager, vm_pager_t, pg_list);
		pager->pg_handle = handle;
		pager->pg_type = PG_VNODE;
		pager->pg_ops = &vnodepagerops;
		pager->pg_data = (caddr_t) vnp;
		vp->v_vmdata = (caddr_t) pager;
	} else {

		/*
		 * vm_object_lookup() will remove the object from the cache if
		 * found and also gain a reference to the object.
		 */
		object = vm_object_lookup(pager);
	}
	return (pager);
}

void
vnode_pager_dealloc(pager)
	vm_pager_t pager;
{
	register vn_pager_t vnp = (vn_pager_t) pager->pg_data;
	register struct vnode *vp;
	struct proc *p = curproc;	/* XXX */

	if (vp = vnp->vnp_vp) {
		vp->v_vmdata = NULL;
		vp->v_flag &= ~VTEXT;
#if 0
		/* can hang if done at reboot on NFS FS */
		(void) VOP_FSYNC(vp, p->p_ucred, p);
#endif
		vrele(vp);
	}
	queue_remove(&vnode_pager_list, pager, vm_pager_t, pg_list);
	free((caddr_t) vnp, M_VMPGDATA);
	free((caddr_t) pager, M_VMPAGER);
}

int
vnode_pager_getmulti(pager, m, count, reqpage, sync)
	vm_pager_t pager;
	vm_page_t *m;
	int     count;
	int     reqpage;
	boolean_t sync;
{

	return vnode_pager_input((vn_pager_t) pager->pg_data, m, count, reqpage);
}

int
vnode_pager_getpage(pager, m, sync)
	vm_pager_t pager;
	vm_page_t m;
	boolean_t sync;
{

	int     err;
	vm_page_t marray[1];

	if (pager == NULL)
		return FALSE;
	marray[0] = m;

	return vnode_pager_input((vn_pager_t) pager->pg_data, marray, 1, 0);
}

boolean_t
vnode_pager_putpage(pager, m, sync)
	vm_pager_t pager;
	vm_page_t m;
	boolean_t sync;
{
	int     err;
	vm_page_t marray[1];
	int     rtvals[1];

	if (pager == NULL)
		return FALSE;
	marray[0] = m;
	vnode_pager_output((vn_pager_t) pager->pg_data, marray, 1, rtvals);
	return rtvals[0];
}

int
vnode_pager_putmulti(pager, m, c, sync, rtvals)
	vm_pager_t pager;
	vm_page_t *m;
	int     c;
	boolean_t sync;
	int    *rtvals;
{
	return vnode_pager_output((vn_pager_t) pager->pg_data, m, c, rtvals);
}


boolean_t
vnode_pager_haspage(pager, offset)
	vm_pager_t pager;
	vm_offset_t offset;
{
	register vn_pager_t vnp = (vn_pager_t) pager->pg_data;
	daddr_t bn;
	int     err;

	/*
	 * Offset beyond end of file, do not have the page
	 */
	if (offset >= vnp->vnp_size) {
		return (FALSE);
	}

	/*
	 * Read the index to find the disk block to read from.  If there is no
	 * block, report that we don't have this data.
	 * 
	 * Assumes that the vnode has whole page or nothing.
	 */
	err = VOP_BMAP(vnp->vnp_vp,
		       offset / vnp->vnp_vp->v_mount->mnt_stat.f_bsize,
		       (struct vnode **) 0, &bn);
	if (err) {
		return (TRUE);
	}
	return ((long) bn < 0 ? FALSE : TRUE);
}

/*
 * Lets the VM system know about a change in size for a file.
 * If this vnode is mapped into some address space (i.e. we have a pager
 * for it) we adjust our own internal size and flush any cached pages in
 * the associated object that are affected by the size change.
 *
 * Note: this routine may be invoked as a result of a pager put
 * operation (possibly at object termination time), so we must be careful.
 */
void
vnode_pager_setsize(vp, nsize)
	struct vnode *vp;
	u_long  nsize;
{
	register vn_pager_t vnp;
	register vm_object_t object;
	vm_pager_t pager;

	/*
	 * Not a mapped vnode
	 */
	if (vp == NULL || vp->v_type != VREG || vp->v_vmdata == NULL)
		return;

	/*
	 * Hasn't changed size
	 */
	pager = (vm_pager_t) vp->v_vmdata;
	vnp = (vn_pager_t) pager->pg_data;
	if (nsize == vnp->vnp_size)
		return;

	/*
	 * No object. This can happen during object termination since
	 * vm_object_page_clean is called after the object has been removed
	 * from the hash table, and clean may cause vnode write operations
	 * which can wind up back here.
	 */
	object = vm_object_lookup(pager);
	if (object == NULL)
		return;

	/*
	 * File has shrunk. Toss any cached pages beyond the new EOF.
	 */
	if (nsize < vnp->vnp_size) {
		vm_object_lock(object);
		vm_object_page_remove(object,
			     round_page((vm_offset_t) nsize), vnp->vnp_size);
		vm_object_unlock(object);

		/*
		 * this gets rid of garbage at the end of a page that is now
		 * only partially backed by the vnode...
		 */
		if (nsize & PAGE_MASK) {
			vm_offset_t kva;
			vm_page_t m;

			m = vm_page_lookup(object, trunc_page((vm_offset_t) nsize));
			if (m) {
				kva = vm_pager_map_page(m);
				bzero((caddr_t) kva + (nsize & PAGE_MASK),
				      round_page(nsize) - nsize);
				vm_pager_unmap_page(kva);
			}
		}
	} else {

		/*
		 * this allows the filesystem and VM cache to stay in sync if
		 * the VM page hasn't been modified...  After the page is
		 * removed -- it will be faulted back in from the filesystem
		 * cache.
		 */
		if (vnp->vnp_size & PAGE_MASK) {
			vm_page_t m;

			m = vm_page_lookup(object, trunc_page(vnp->vnp_size));
			if (m && (m->flags & PG_CLEAN)) {
				vm_object_lock(object);
				vm_object_page_remove(object,
					       vnp->vnp_size, vnp->vnp_size);
				vm_object_unlock(object);
			}
		}
	}
	vnp->vnp_size = (vm_offset_t) nsize;
	object->size = round_page(nsize);

	vm_object_deallocate(object);
}

void
vnode_pager_umount(mp)
	register struct mount *mp;
{
	register vm_pager_t pager, npager;
	struct vnode *vp;

	pager = (vm_pager_t) queue_first(&vnode_pager_list);
	while (!queue_end(&vnode_pager_list, (queue_entry_t) pager)) {

		/*
		 * Save the next pointer now since uncaching may terminate the
		 * object and render pager invalid
		 */
		vp = ((vn_pager_t) pager->pg_data)->vnp_vp;
		npager = (vm_pager_t) queue_next(&pager->pg_list);
		if (mp == (struct mount *) 0 || vp->v_mount == mp)
			(void) vnode_pager_uncache(vp);
		pager = npager;
	}
}

/*
 * Remove vnode associated object from the object cache.
 *
 * Note: this routine may be invoked as a result of a pager put
 * operation (possibly at object termination time), so we must be careful.
 */
boolean_t
vnode_pager_uncache(vp)
	register struct vnode *vp;
{
	register vm_object_t object;
	boolean_t uncached, locked;
	vm_pager_t pager;

	/*
	 * Not a mapped vnode
	 */
	pager = (vm_pager_t) vp->v_vmdata;
	if (pager == NULL)
		return (TRUE);

	/*
	 * Unlock the vnode if it is currently locked. We do this since
	 * uncaching the object may result in its destruction which may
	 * initiate paging activity which may necessitate locking the vnode.
	 */
	locked = VOP_ISLOCKED(vp);
	if (locked)
		VOP_UNLOCK(vp);

	/*
	 * Must use vm_object_lookup() as it actually removes the object from
	 * the cache list.
	 */
	object = vm_object_lookup(pager);
	if (object) {
		uncached = (object->ref_count <= 1);
		pager_cache(object, FALSE);
	} else
		uncached = TRUE;
	if (locked)
		VOP_LOCK(vp);
	return (uncached);
}


void
vnode_pager_freepage(m)
	vm_page_t m;
{
	PAGE_WAKEUP(m);
	vm_page_free(m);
}

/*
 * calculate the linear (byte) disk address of specified virtual
 * file address
 */
vm_offset_t
vnode_pager_addr(vp, address)
	struct vnode *vp;
	vm_offset_t address;
{
	int     rtaddress;
	int     bsize;
	vm_offset_t block;
	struct vnode *rtvp;
	int     err;
	int     vblock, voffset;

	bsize = vp->v_mount->mnt_stat.f_bsize;
	vblock = address / bsize;
	voffset = address % bsize;

	err = VOP_BMAP(vp, vblock, &rtvp, &block);

	if (err)
		rtaddress = -1;
	else
		rtaddress = block * DEV_BSIZE + voffset;

	return rtaddress;
}

/*
 * interrupt routine for I/O completion
 */
void
vnode_pager_iodone(bp)
	struct buf *bp;
{
	bp->b_flags |= B_DONE;
	wakeup((caddr_t) bp);
}

/*
 * small block file system vnode pager input
 */
int
vnode_pager_input_smlfs(vnp, m)
	vn_pager_t vnp;
	vm_page_t m;
{
	int     i;
	int     s;
	vm_offset_t paging_offset;
	struct vnode *dp, *vp;
	struct buf *bp;
	vm_offset_t mapsize;
	vm_offset_t foff;
	vm_offset_t kva;
	int     fileaddr;
	int     block;
	vm_offset_t bsize;
	int     error = 0;

	paging_offset = m->object->paging_offset;
	vp = vnp->vnp_vp;
	bsize = vp->v_mount->mnt_stat.f_bsize;
	foff = m->offset + paging_offset;

	VOP_BMAP(vp, foff, &dp, 0);

	kva = vm_pager_map_page(m);

	for (i = 0; i < NBPG / bsize; i++) {

		/*
		 * calculate logical block and offset
		 */
		block = foff / bsize + i;
		s = splbio();
		while (bp = incore(vp, block)) {
			int     amount;

			/*
			 * wait until the buffer is avail or gone
			 */
			if (bp->b_flags & B_BUSY) {
				bp->b_flags |= B_WANTED;
				tsleep((caddr_t) bp, PVM, "vnwblk", 0);
				continue;
			}
			amount = bsize;
			if ((foff + bsize) > vnp->vnp_size)
				amount = vnp->vnp_size - foff;

			/*
			 * make sure that this page is in the buffer
			 */
			if ((amount > 0) && amount <= bp->b_bcount) {
				bp->b_flags |= B_BUSY;
				splx(s);

				/*
				 * copy the data from the buffer
				 */
				bcopy(bp->b_un.b_addr, (caddr_t) kva + i * bsize, amount);
				if (amount < bsize) {
					bzero((caddr_t) kva + amount, bsize - amount);
				}
				bp->b_flags &= ~B_BUSY;
				wakeup((caddr_t) bp);
				goto nextblock;
			}
			break;
		}
		splx(s);
		fileaddr = vnode_pager_addr(vp, foff + i * bsize);
		if (fileaddr != -1) {
			VHOLD(vp);
			bp = getpbuf();

			/* build a minimal buffer header */
			bp->b_flags = B_BUSY | B_READ | B_CALL;
			bp->b_iodone = vnode_pager_iodone;
			bp->b_proc = curproc;
			bp->b_rcred = bp->b_wcred = bp->b_proc->p_ucred;
			bp->b_un.b_addr = (caddr_t) kva + i * bsize;
			bp->b_blkno = fileaddr / DEV_BSIZE;
			bp->b_vp = dp;

			/*
			 * Should be a BLOCK or character DEVICE if we get
			 * here
			 */
			bp->b_dev = dp->v_rdev;
			bp->b_bcount = bsize;
			bp->b_bufsize = bsize;

			/* do the input */
			VOP_STRATEGY(bp);

			/* we definitely need to be at splbio here */

			s = splbio();
			while ((bp->b_flags & B_DONE) == 0) {
				tsleep((caddr_t) bp, PVM, "vnsrd", 0);
			}
			splx(s);
			if ((bp->b_flags & B_ERROR) != 0)
				error = EIO;

			HOLDRELE(vp);

			/*
			 * free the buffer header back to the swap buffer pool
			 */
			relpbuf(bp);
			if (error)
				break;
		} else {
			bzero((caddr_t) kva + i * bsize, bsize);
		}
nextblock:
	}
	vm_pager_unmap_page(kva);
	if (error) {
		return VM_PAGER_FAIL;
	}
	pmap_clear_modify(VM_PAGE_TO_PHYS(m));
	m->flags |= PG_CLEAN;
	m->flags &= ~PG_LAUNDRY;
	return VM_PAGER_OK;

}


/*
 * old style vnode pager output routine
 */
int
vnode_pager_input_old(vnp, m)
	vn_pager_t vnp;
	vm_page_t m;
{
	int     i;
	struct uio auio;
	struct iovec aiov;
	int     error;
	int     size;
	vm_offset_t foff;
	vm_offset_t kva;

	error = 0;
	foff = m->offset + m->object->paging_offset;

	/*
	 * Return failure if beyond current EOF
	 */
	if (foff >= vnp->vnp_size) {
		return VM_PAGER_BAD;
	} else {
		size = NBPG;
		if (foff + size > vnp->vnp_size)
			size = vnp->vnp_size - foff;
/*
 * Allocate a kernel virtual address and initialize so that
 * we can use VOP_READ/WRITE routines.
 */
		kva = vm_pager_map_page(m);
		aiov.iov_base = (caddr_t) kva;
		aiov.iov_len = size;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = foff;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_rw = UIO_READ;
		auio.uio_resid = size;
		auio.uio_procp = (struct proc *) 0;

		error = VOP_READ(vnp->vnp_vp, &auio, IO_PAGER, curproc->p_ucred);
		if (!error) {
			register int count = size - auio.uio_resid;

			if (count == 0)
				error = EINVAL;
			else if (count != NBPG)
				bzero((caddr_t) kva + count, NBPG - count);
		}
		vm_pager_unmap_page(kva);
	}
	pmap_clear_modify(VM_PAGE_TO_PHYS(m));
	m->flags |= PG_CLEAN;
	m->flags &= ~PG_LAUNDRY;
	return error ? VM_PAGER_FAIL : VM_PAGER_OK;
}

/*
 * generic vnode pager input routine
 */
int
vnode_pager_input(vnp, m, count, reqpage)
	register vn_pager_t vnp;
	vm_page_t *m;
	int     count, reqpage;
{
	int     i, j;
	vm_offset_t kva, foff;
	int     size;
	struct proc *p = curproc;	/* XXX */
	vm_object_t object;
	vm_offset_t paging_offset;
	struct vnode *dp, *vp;
	vm_offset_t mapsize;
	int     bsize;

	int     first, last;
	int     reqaddr, firstaddr;
	int     block, offset;

	int     nbp;
	struct buf *bp;
	int     s;
	int     failflag;

	int     errtype = 0;	/* 0 is file type otherwise vm type */
	int     error = 0;

	object = m[reqpage]->object;	/* all vm_page_t items are in same
					 * object */
	paging_offset = object->paging_offset;

	vp = vnp->vnp_vp;
	bsize = vp->v_mount->mnt_stat.f_bsize;

	/* get the UNDERLYING device for the file with VOP_BMAP() */

	/*
	 * originally, we did not check for an error return value -- assuming
	 * an fs always has a bmap entry point -- that assumption is wrong!!!
	 */
	kva = 0;
	mapsize = 0;
	foff = m[reqpage]->offset + paging_offset;
	if (!VOP_BMAP(vp, foff, &dp, 0)) {

		/*
		 * we do not block for a kva, notice we default to a kva
		 * conservative behavior
		 */
		kva = kmem_alloc_pageable(pager_map, (mapsize = count * NBPG));
		if (!kva) {
			for (i = 0; i < count; i++) {
				if (i != reqpage) {
					vnode_pager_freepage(m[i]);
				}
			}
			m[0] = m[reqpage];
			kva = kmem_alloc_wait(pager_map, mapsize = NBPG);
			reqpage = 0;
			count = 1;
		}
	}

	/*
	 * if we can't get a kva or we can't bmap, use old VOP code
	 */
	if (!kva) {
		for (i = 0; i < count; i++) {
			if (i != reqpage) {
				vnode_pager_freepage(m[i]);
			}
		}
		return vnode_pager_input_old(vnp, m[reqpage]);

		/*
		 * if the blocksize is smaller than a page size, then use
		 * special small filesystem code.  NFS sometimes has a small
		 * blocksize, but it can handle large reads itself.
		 */
	} else if ((NBPG / bsize) > 1 &&
		   (vp->v_mount->mnt_stat.f_type != MOUNT_NFS)) {

		kmem_free_wakeup(pager_map, kva, mapsize);

		for (i = 0; i < count; i++) {
			if (i != reqpage) {
				vnode_pager_freepage(m[i]);
			}
		}
		return vnode_pager_input_smlfs(vnp, m[reqpage]);
	}
/*
 * here on direct device I/O
 */


	/*
	 * This pathetic hack gets data from the buffer cache, if it's there.
	 * I believe that this is not really necessary, and the ends can be
	 * gotten by defaulting to the normal vfs read behavior, but this
	 * might be more efficient, because the will NOT invoke read-aheads
	 * and one of the purposes of this code is to bypass the buffer cache
	 * and keep from flushing it by reading in a program.
	 */

	/*
	 * calculate logical block and offset
	 */
	block = foff / bsize;
	offset = foff % bsize;
	s = splbio();

	/*
	 * if we have a buffer in core, then try to use it
	 */
	while (bp = incore(vp, block)) {
		int     amount;

		/*
		 * wait until the buffer is avail or gone
		 */
		if (bp->b_flags & B_BUSY) {
			bp->b_flags |= B_WANTED;
			tsleep((caddr_t) bp, PVM, "vnwblk", 0);
			continue;
		}
		amount = NBPG;
		if ((foff + amount) > vnp->vnp_size)
			amount = vnp->vnp_size - foff;

		/*
		 * make sure that this page is in the buffer
		 */
		if ((amount > 0) && (offset + amount) <= bp->b_bcount) {
			bp->b_flags |= B_BUSY;
			splx(s);

			/*
			 * map the requested page
			 */
			pmap_kenter(kva, VM_PAGE_TO_PHYS(m[reqpage]));
			pmap_update();

			/*
			 * copy the data from the buffer
			 */
			bcopy(bp->b_un.b_addr + offset, (caddr_t) kva, amount);
			if (amount < NBPG) {
				bzero((caddr_t) kva + amount, NBPG - amount);
			}

			/*
			 * unmap the page and free the kva
			 */
			pmap_remove(vm_map_pmap(pager_map), kva, kva + NBPG);
			kmem_free_wakeup(pager_map, kva, mapsize);

			/*
			 * release the buffer back to the block subsystem
			 */
			bp->b_flags &= ~B_BUSY;
			wakeup((caddr_t) bp);

			/*
			 * we did not have to do any work to get the requested
			 * page, the read behind/ahead does not justify a read
			 */
			for (i = 0; i < count; i++) {
				if (i != reqpage) {
					vnode_pager_freepage(m[i]);
				}
			}
			count = 1;
			reqpage = 0;
			m[0] = m[reqpage];

			/*
			 * sorry for the goto
			 */
			goto finishup;
		}

		/*
		 * buffer is nowhere to be found, read from the disk
		 */
		break;
	}
	splx(s);

	reqaddr = vnode_pager_addr(vp, foff);

	/*
	 * Make sure that our I/O request is contiguous. Scan backward and
	 * stop for the first discontiguous entry or stop for a page being in
	 * buffer cache.
	 */
	failflag = 0;
	first = reqpage;
	for (i = reqpage - 1; i >= 0; --i) {
		if (failflag ||
		    incore(vp, (foff + (i - reqpage) * NBPG) / bsize) ||
		    (vnode_pager_addr(vp, m[i]->offset + paging_offset))
		    != reqaddr + (i - reqpage) * NBPG) {
			vnode_pager_freepage(m[i]);
			failflag = 1;
		} else {
			first = i;
		}
	}

	/*
	 * Scan forward and stop for the first non-contiguous entry or stop
	 * for a page being in buffer cache.
	 */
	failflag = 0;
	last = reqpage + 1;
	for (i = reqpage + 1; i < count; i++) {
		if (failflag ||
		    incore(vp, (foff + (i - reqpage) * NBPG) / bsize) ||
		    (vnode_pager_addr(vp, m[i]->offset + paging_offset))
		    != reqaddr + (i - reqpage) * NBPG) {
			vnode_pager_freepage(m[i]);
			failflag = 1;
		} else {
			last = i + 1;
		}
	}

	/*
	 * the first and last page have been calculated now, move input pages
	 * to be zero based...
	 */
	count = last;
	if (first != 0) {
		for (i = first; i < count; i++) {
			m[i - first] = m[i];
		}
		count -= first;
		reqpage -= first;
	}

	/*
	 * calculate the file virtual address for the transfer
	 */
	foff = m[0]->offset + paging_offset;

	/*
	 * and get the disk physical address (in bytes)
	 */
	firstaddr = vnode_pager_addr(vp, foff);

	/*
	 * calculate the size of the transfer
	 */
	size = count * NBPG;
	if ((foff + size) > vnp->vnp_size)
		size = vnp->vnp_size - foff;

	/*
	 * round up physical size for real devices
	 */
	if (dp->v_type == VBLK || dp->v_type == VCHR)
		size = (size + DEV_BSIZE - 1) & ~(DEV_BSIZE - 1);

	/*
	 * and map the pages to be read into the kva
	 */
	for (i = 0; i < count; i++)
		pmap_kenter(kva + NBPG * i, VM_PAGE_TO_PHYS(m[i]));

	pmap_update();
	VHOLD(vp);
	bp = getpbuf();

	/* build a minimal buffer header */
	bp->b_flags = B_BUSY | B_READ | B_CALL;
	bp->b_iodone = vnode_pager_iodone;
	/* B_PHYS is not set, but it is nice to fill this in */
	bp->b_proc = curproc;
	bp->b_rcred = bp->b_wcred = bp->b_proc->p_ucred;
	bp->b_un.b_addr = (caddr_t) kva;
	bp->b_blkno = firstaddr / DEV_BSIZE;
	bp->b_vp = dp;

	/* Should be a BLOCK or character DEVICE if we get here */
	bp->b_dev = dp->v_rdev;
	bp->b_bcount = size;
	bp->b_bufsize = size;

	/* do the input */
	VOP_STRATEGY(bp);

	s = splbio();
	/* we definitely need to be at splbio here */

	while ((bp->b_flags & B_DONE) == 0) {
		tsleep((caddr_t) bp, PVM, "vnread", 0);
	}
	splx(s);
	if ((bp->b_flags & B_ERROR) != 0)
		error = EIO;

	if (!error) {
		if (size != count * NBPG)
			bzero((caddr_t) kva + size, NBPG * count - size);
	}
	HOLDRELE(vp);

	pmap_remove(vm_map_pmap(pager_map), kva, kva + NBPG * count);
	kmem_free_wakeup(pager_map, kva, mapsize);

	/*
	 * free the buffer header back to the swap buffer pool
	 */
	relpbuf(bp);

finishup:
	for (i = 0; i < count; i++) {
		pmap_clear_modify(VM_PAGE_TO_PHYS(m[i]));
		m[i]->flags |= PG_CLEAN;
		m[i]->flags &= ~PG_LAUNDRY;
		if (i != reqpage) {

			/*
			 * whether or not to leave the page activated is up in
			 * the air, but we should put the page on a page queue
			 * somewhere. (it already is in the object). Result:
			 * It appears that emperical results show that
			 * deactivating pages is best.
			 */

			/*
			 * just in case someone was asking for this page we
			 * now tell them that it is ok to use
			 */
			if (!error) {
				vm_page_deactivate(m[i]);
				PAGE_WAKEUP(m[i]);
				m[i]->flags &= ~PG_FAKE;
			} else {
				vnode_pager_freepage(m[i]);
			}
		}
	}
	if (error) {
		printf("vnode pager read error: %d\n", error);
	}
	if (errtype)
		return error;
	return (error ? VM_PAGER_FAIL : VM_PAGER_OK);
}

/*
 * old-style vnode pager output routine
 */
int
vnode_pager_output_old(vnp, m)
	register vn_pager_t vnp;
	vm_page_t m;
{
	vm_offset_t foff;
	vm_offset_t kva;
	vm_offset_t size;
	struct iovec aiov;
	struct uio auio;
	struct vnode *vp;
	int     error;

	vp = vnp->vnp_vp;
	foff = m->offset + m->object->paging_offset;

	/*
	 * Return failure if beyond current EOF
	 */
	if (foff >= vnp->vnp_size) {
		return VM_PAGER_BAD;
	} else {
		size = NBPG;
		if (foff + size > vnp->vnp_size)
			size = vnp->vnp_size - foff;
/*
 * Allocate a kernel virtual address and initialize so that
 * we can use VOP_WRITE routines.
 */
		kva = vm_pager_map_page(m);
		aiov.iov_base = (caddr_t) kva;
		aiov.iov_len = size;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = foff;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_rw = UIO_WRITE;
		auio.uio_resid = size;
		auio.uio_procp = (struct proc *) 0;

		error = VOP_WRITE(vp, &auio, IO_PAGER, curproc->p_ucred);

		if (!error) {
			if ((size - auio.uio_resid) == 0) {
				error = EINVAL;
			}
		}
		vm_pager_unmap_page(kva);
		return error ? VM_PAGER_FAIL : VM_PAGER_OK;
	}
}

/*
 * vnode pager output on a small-block file system
 */
int
vnode_pager_output_smlfs(vnp, m)
	vn_pager_t vnp;
	vm_page_t m;
{
	int     i;
	int     s;
	vm_offset_t paging_offset;
	struct vnode *dp, *vp;
	struct buf *bp;
	vm_offset_t mapsize;
	vm_offset_t foff;
	vm_offset_t kva;
	int     fileaddr;
	int     block;
	vm_offset_t bsize;
	int     error = 0;

	paging_offset = m->object->paging_offset;
	vp = vnp->vnp_vp;
	bsize = vp->v_mount->mnt_stat.f_bsize;
	foff = m->offset + paging_offset;

	VOP_BMAP(vp, foff, &dp, 0);
	kva = vm_pager_map_page(m);
	for (i = 0; !error && i < (NBPG / bsize); i++) {

		/*
		 * calculate logical block and offset
		 */
		fileaddr = vnode_pager_addr(vp, foff + i * bsize);
		if (fileaddr != -1) {
			s = splbio();
			if (bp = incore(vp, (foff / bsize) + i)) {
				bp = getblk(vp, (foff / bsize) + i, bp->b_bufsize);
				bp->b_flags |= B_INVAL;
				brelse(bp);
			}
			splx(s);

			VHOLD(vp);
			bp = getpbuf();

			/* build a minimal buffer header */
			bp->b_flags = B_BUSY | B_CALL | B_WRITE;
			bp->b_iodone = vnode_pager_iodone;
			bp->b_proc = curproc;
			bp->b_rcred = bp->b_wcred = bp->b_proc->p_ucred;
			bp->b_un.b_addr = (caddr_t) kva + i * bsize;
			bp->b_blkno = fileaddr / DEV_BSIZE;
			bp->b_vp = dp;
			++dp->v_numoutput;
			/* for NFS */
			bp->b_dirtyoff = 0;
			bp->b_dirtyend = bsize;

			/*
			 * Should be a BLOCK or character DEVICE if we get
			 * here
			 */
			bp->b_dev = dp->v_rdev;
			bp->b_bcount = bsize;
			bp->b_bufsize = bsize;

			/* do the input */
			VOP_STRATEGY(bp);

			/* we definitely need to be at splbio here */

			s = splbio();
			while ((bp->b_flags & B_DONE) == 0) {
				tsleep((caddr_t) bp, PVM, "vnswrt", 0);
			}
			splx(s);
			if ((bp->b_flags & B_ERROR) != 0)
				error = EIO;

			HOLDRELE(vp);

			/*
			 * free the buffer header back to the swap buffer pool
			 */
			relpbuf(bp);
		}
	}
	vm_pager_unmap_page(kva);
	if (error)
		return VM_PAGER_FAIL;
	else
		return VM_PAGER_OK;
}

/*
 * generic vnode pager output routine
 */
int
vnode_pager_output(vnp, m, count, rtvals)
	vn_pager_t vnp;
	vm_page_t *m;
	int     count;
	int    *rtvals;
{
	int     i, j;
	vm_offset_t kva, foff;
	int     size;
	struct proc *p = curproc;	/* XXX */
	vm_object_t object;
	vm_offset_t paging_offset;
	struct vnode *dp, *vp;
	struct buf *bp;
	vm_offset_t mapsize;
	vm_offset_t reqaddr;
	int     bsize;
	int     s;

	int     error = 0;

retryoutput:
	object = m[0]->object;	/* all vm_page_t items are in same object */
	paging_offset = object->paging_offset;

	vp = vnp->vnp_vp;
	bsize = vp->v_mount->mnt_stat.f_bsize;

	for (i = 0; i < count; i++)
		rtvals[i] = VM_PAGER_TRYAGAIN;

	/*
	 * if the filesystem does not have a bmap, then use the old code
	 */
	if (VOP_BMAP(vp, m[0]->offset + paging_offset, &dp, 0)) {

		rtvals[0] = vnode_pager_output_old(vnp, m[0]);

		pmap_clear_modify(VM_PAGE_TO_PHYS(m[0]));
		m[0]->flags |= PG_CLEAN;
		m[0]->flags &= ~PG_LAUNDRY;
		return rtvals[0];
	}

	/*
	 * if the filesystem has a small blocksize, then use the small block
	 * filesystem output code
	 */
	if ((bsize < NBPG) &&
	    (vp->v_mount->mnt_stat.f_type != MOUNT_NFS)) {

		for (i = 0; i < count; i++) {
			rtvals[i] = vnode_pager_output_smlfs(vnp, m[i]);
			if (rtvals[i] == VM_PAGER_OK) {
				pmap_clear_modify(VM_PAGE_TO_PHYS(m[i]));
				m[i]->flags |= PG_CLEAN;
				m[i]->flags &= ~PG_LAUNDRY;
			}
		}
		return rtvals[0];
	}

	/*
	 * get some kva for the output
	 */
	kva = kmem_alloc_pageable(pager_map, (mapsize = count * NBPG));
	if (!kva) {
		kva = kmem_alloc_pageable(pager_map, (mapsize = NBPG));
		count = 1;
		if (!kva)
			return rtvals[0];
	}
	for (i = 0; i < count; i++) {
		foff = m[i]->offset + paging_offset;
		if (foff >= vnp->vnp_size) {
			for (j = i; j < count; j++)
				rtvals[j] = VM_PAGER_BAD;
			count = i;
			break;
		}
	}
	if (count == 0) {
		return rtvals[0];
	}
	foff = m[0]->offset + paging_offset;
	reqaddr = vnode_pager_addr(vp, foff);

	/*
	 * Scan forward and stop for the first non-contiguous entry or stop
	 * for a page being in buffer cache.
	 */
	for (i = 1; i < count; i++) {
		if (vnode_pager_addr(vp, m[i]->offset + paging_offset)
		    != reqaddr + i * NBPG) {
			count = i;
			break;
		}
	}

	/*
	 * calculate the size of the transfer
	 */
	size = count * NBPG;
	if ((foff + size) > vnp->vnp_size)
		size = vnp->vnp_size - foff;

	/*
	 * round up physical size for real devices
	 */
	if (dp->v_type == VBLK || dp->v_type == VCHR)
		size = (size + DEV_BSIZE - 1) & ~(DEV_BSIZE - 1);

	/*
	 * and map the pages to be read into the kva
	 */
	for (i = 0; i < count; i++)
		pmap_kenter(kva + NBPG * i, VM_PAGE_TO_PHYS(m[i]));
	pmap_update();
/*
	printf("vnode: writing foff: %d, devoff: %d, size: %d\n",
		foff, reqaddr, size);
*/

	/*
	 * next invalidate the incore vfs_bio data
	 */
	for (i = 0; i < count; i++) {
		int     filblock = (foff + i * NBPG) / bsize;
		struct buf *fbp;

		s = splbio();
		if (fbp = incore(vp, filblock)) {
			fbp = getblk(vp, filblock, fbp->b_bufsize);
			if (fbp->b_flags & B_DELWRI) {
				if (fbp->b_bufsize <= NBPG)
					fbp->b_flags &= ~B_DELWRI;
				else {
					bwrite(fbp);
					fbp = getblk(vp, filblock,
						     fbp->b_bufsize);
				}
			}
			fbp->b_flags |= B_INVAL;
			brelse(fbp);
		}
		splx(s);
	}


	VHOLD(vp);
	bp = getpbuf();

	/* build a minimal buffer header */
	bp->b_flags = B_BUSY | B_WRITE | B_CALL;
	bp->b_iodone = vnode_pager_iodone;
	/* B_PHYS is not set, but it is nice to fill this in */
	/* bp->b_proc = &proc0; */
	bp->b_proc = curproc;
	bp->b_rcred = bp->b_wcred = bp->b_proc->p_ucred;
	bp->b_un.b_addr = (caddr_t) kva;
	bp->b_blkno = reqaddr / DEV_BSIZE;
	bp->b_vp = dp;
	++dp->v_numoutput;

	/* Should be a BLOCK or character DEVICE if we get here */
	bp->b_dev = dp->v_rdev;
	/* for NFS */
	bp->b_dirtyoff = 0;
	bp->b_dirtyend = size;

	bp->b_bcount = size;
	bp->b_bufsize = size;

	/* do the output */
	VOP_STRATEGY(bp);

	s = splbio();

	/* we definitely need to be at splbio here */

	while ((bp->b_flags & B_DONE) == 0) {
		tsleep((caddr_t) bp, PVM, "vnwrite", 0);
	}
	splx(s);

	if ((bp->b_flags & B_ERROR) != 0)
		error = EIO;

	HOLDRELE(vp);

	pmap_remove(vm_map_pmap(pager_map), kva, kva + NBPG * count);
	kmem_free_wakeup(pager_map, kva, mapsize);

	/*
	 * free the buffer header back to the swap buffer pool
	 */
	relpbuf(bp);

	if (!error) {
		for (i = 0; i < count; i++) {
			pmap_clear_modify(VM_PAGE_TO_PHYS(m[i]));
			m[i]->flags |= PG_CLEAN;
			m[i]->flags &= ~PG_LAUNDRY;
			rtvals[i] = VM_PAGER_OK;
		}
	} else if (count != 1) {
		error = 0;
		count = 1;
		goto retryoutput;
	}
	if (error) {
		printf("vnode pager write error: %d\n", error);
	}
	return (error ? VM_PAGER_FAIL : VM_PAGER_OK);
}
