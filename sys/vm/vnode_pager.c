/*
 * Copyright (c) 1990 University of Utah.
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1993 John S. Dyson
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
 *	$Id: vnode_pager.c,v 1.11.2.3 1994/04/18 04:57:49 rgrimes Exp $
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

struct pagerops vnodepagerops = {
	vnode_pager_init,
	vnode_pager_alloc,
	vnode_pager_dealloc,
	vnode_pager_getpage,
	vnode_pager_getmulti,
	vnode_pager_putpage,
	vnode_pager_haspage
};

static int vnode_pager_io(vn_pager_t vnp, vm_page_t *m, int count, int reqpage, 
	enum uio_rw rw);
struct buf * getpbuf() ;
void relpbuf(struct buf *bp) ;

extern vm_map_t pager_map;

queue_head_t	vnode_pager_list;	/* list of managed vnodes */

#ifdef DEBUG
int	vpagerdebug = 0x00;
#define	VDB_FOLLOW	0x01
#define VDB_INIT	0x02
#define VDB_IO		0x04
#define VDB_FAIL	0x08
#define VDB_ALLOC	0x10
#define VDB_SIZE	0x20
#endif

void
vnode_pager_init()
{
#ifdef DEBUG
	if (vpagerdebug & VDB_FOLLOW)
		printf("vnode_pager_init()\n");
#endif
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

#ifdef DEBUG
	if (vpagerdebug & (VDB_FOLLOW|VDB_ALLOC))
		printf("vnode_pager_alloc(%x, %x, %x)\n", handle, size, prot);
#endif
	/*
	 * Pageout to vnode, no can do yet.
	 */
	if (handle == NULL)
		return(NULL);

	/*
	 * Vnodes keep a pointer to any associated pager so no need to
	 * lookup with vm_pager_lookup.
	 */
	vp = (struct vnode *)handle;
	pager = (vm_pager_t)vp->v_vmdata;
	if (pager == NULL) {
		/*
		 * Allocate pager structures
		 */
		pager = (vm_pager_t)malloc(sizeof *pager, M_VMPAGER, M_WAITOK);
		if (pager == NULL)
			return(NULL);
		vnp = (vn_pager_t)malloc(sizeof *vnp, M_VMPGDATA, M_WAITOK);
		if (vnp == NULL) {
			free((caddr_t)pager, M_VMPAGER);
			return(NULL);
		}
		/*
		 * And an object of the appropriate size
		 */
		if (VOP_GETATTR(vp, &vattr, p->p_ucred, p) == 0) {
			object = vm_object_allocate(round_page(vattr.va_size));
			vm_object_enter(object, pager);
			vm_object_setpager(object, pager, 0, TRUE);
		} else {
			free((caddr_t)vnp, M_VMPGDATA);
			free((caddr_t)pager, M_VMPAGER);
			return(NULL);
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
		pager->pg_data = (caddr_t)vnp;
		vp->v_vmdata = (caddr_t)pager;
	} else {
		/*
		 * vm_object_lookup() will remove the object from the
		 * cache if found and also gain a reference to the object.
		 */
		object = vm_object_lookup(pager);
#ifdef DEBUG
		vnp = (vn_pager_t)pager->pg_data;
#endif
	}
#ifdef DEBUG
	if (vpagerdebug & VDB_ALLOC)
		printf("vnode_pager_setup: vp %x sz %x pager %x object %x\n",
		       vp, vnp->vnp_size, pager, object);
#endif
	return(pager);
}

void
vnode_pager_dealloc(pager)
	vm_pager_t pager;
{
	register vn_pager_t vnp = (vn_pager_t)pager->pg_data;
	register struct vnode *vp;
	struct proc *p = curproc;		/* XXX */

#ifdef DEBUG
	if (vpagerdebug & VDB_FOLLOW)
		printf("vnode_pager_dealloc(%x)\n", pager);
#endif
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
	free((caddr_t)vnp, M_VMPGDATA);
	free((caddr_t)pager, M_VMPAGER);
}

int
vnode_pager_getmulti(pager, m, count, reqpage, sync)
	vm_pager_t pager;
	vm_page_t *m;
	int count;
	int reqpage;
	boolean_t sync;
{
	
	return vnode_pager_io((vn_pager_t) pager->pg_data, m, count, reqpage, UIO_READ);
}


int
vnode_pager_getpage(pager, m, sync)
	vm_pager_t pager;
	vm_page_t m;
	boolean_t sync;
{

	int err;
	vm_page_t marray[1];
#ifdef DEBUG
	if (vpagerdebug & VDB_FOLLOW)
		printf("vnode_pager_getpage(%x, %x)\n", pager, m);
#endif
	if (pager == NULL)
		return FALSE;
	marray[0] = m;

	return vnode_pager_io((vn_pager_t)pager->pg_data, marray, 1, 0, UIO_READ);
}

boolean_t
vnode_pager_putpage(pager, m, sync)
	vm_pager_t pager;
	vm_page_t m;
	boolean_t sync;
{
	int err;
	vm_page_t marray[1];

#ifdef DEBUG
	if (vpagerdebug & VDB_FOLLOW)
		printf("vnode_pager_putpage(%x, %x)\n", pager, m);
#endif
	if (pager == NULL)
		return FALSE;
	marray[0] = m;
	err = vnode_pager_io((vn_pager_t)pager->pg_data, marray, 1, 0, UIO_WRITE);
	return err;
}

boolean_t
vnode_pager_haspage(pager, offset)
	vm_pager_t pager;
	vm_offset_t offset;
{
	register vn_pager_t vnp = (vn_pager_t)pager->pg_data;
	daddr_t bn;
	int err;

#ifdef DEBUG
	if (vpagerdebug & VDB_FOLLOW)
		printf("vnode_pager_haspage(%x, %x)\n", pager, offset);
#endif

	/*
	 * Offset beyond end of file, do not have the page
	 */
	if (offset >= vnp->vnp_size) {
#ifdef DEBUG
		if (vpagerdebug & (VDB_FAIL|VDB_SIZE))
			printf("vnode_pager_haspage: pg %x, off %x, size %x\n",
			       pager, offset, vnp->vnp_size);
#endif
		return(FALSE);
	}

	/*
	 * Read the index to find the disk block to read
	 * from.  If there is no block, report that we don't
	 * have this data.
	 *
	 * Assumes that the vnode has whole page or nothing.
	 */
	err = VOP_BMAP(vnp->vnp_vp,
		       offset / vnp->vnp_vp->v_mount->mnt_stat.f_bsize,
		       (struct vnode **)0, &bn);
	if (err) {
#ifdef DEBUG
		if (vpagerdebug & VDB_FAIL)
			printf("vnode_pager_haspage: BMAP err %d, pg %x, off %x\n",
			       err, pager, offset);
#endif
		return(TRUE);
	}
	return((long)bn < 0 ? FALSE : TRUE);
}

/*
 * (XXX)
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
	u_long nsize;
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
	pager = (vm_pager_t)vp->v_vmdata;
	vnp = (vn_pager_t)pager->pg_data;
	if (nsize == vnp->vnp_size)
		return;
	/*
	 * No object.
	 * This can happen during object termination since
	 * vm_object_page_clean is called after the object
	 * has been removed from the hash table, and clean
	 * may cause vnode write operations which can wind
	 * up back here.
	 */
	object = vm_object_lookup(pager);
	if (object == NULL)
		return;

#ifdef DEBUG
	if (vpagerdebug & (VDB_FOLLOW|VDB_SIZE))
		printf("vnode_pager_setsize: vp %x obj %x osz %d nsz %d\n",
		       vp, object, vnp->vnp_size, nsize);
#endif

	/*
	 * File has shrunk.
	 * Toss any cached pages beyond the new EOF.
	 */
	if (round_page(nsize) < round_page(vnp->vnp_size)) {
		vm_object_lock(object);
		vm_object_page_remove(object,
				      (vm_offset_t)round_page(nsize), round_page(vnp->vnp_size));
		vm_object_unlock(object);
	}
	vnp->vnp_size = (vm_offset_t)nsize;
	vm_object_deallocate(object);
}

void
vnode_pager_umount(mp)
	register struct mount *mp;
{
	register vm_pager_t pager, npager;
	struct vnode *vp;

	pager = (vm_pager_t) queue_first(&vnode_pager_list);
	while (!queue_end(&vnode_pager_list, (queue_entry_t)pager)) {
		/*
		 * Save the next pointer now since uncaching may
		 * terminate the object and render pager invalid
		 */
		vp = ((vn_pager_t)pager->pg_data)->vnp_vp;
		npager = (vm_pager_t) queue_next(&pager->pg_list);
		if (mp == (struct mount *)0 || vp->v_mount == mp)
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
	pager = (vm_pager_t)vp->v_vmdata;
	if (pager == NULL)
		return (TRUE);
	/*
	 * Unlock the vnode if it is currently locked.
	 * We do this since uncaching the object may result
	 * in its destruction which may initiate paging
	 * activity which may necessitate locking the vnode.
	 */
	locked = VOP_ISLOCKED(vp);
	if (locked)
		VOP_UNLOCK(vp);
	/*
	 * Must use vm_object_lookup() as it actually removes
	 * the object from the cache list.
	 */
	object = vm_object_lookup(pager);
	if (object) {
		uncached = (object->ref_count <= 1);
		pager_cache(object, FALSE);
	} else
		uncached = TRUE;
	if (locked)
		VOP_LOCK(vp);
	return(uncached);
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
	int rtaddress;
	int bsize;
	vm_offset_t block;
	struct vnode *rtvp;
	int err;
	int vblock, voffset;

	bsize = vp->v_mount->mnt_stat.f_bsize;
	vblock = address / bsize;
	voffset = address % bsize;

	err = VOP_BMAP(vp,vblock,&rtvp,&block);

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
	wakeup((caddr_t)bp);
}

/*
 * vnode_pager_io:
 * 	Perform read or write operation for vnode_paging
 *
 *	args:
 *               vnp -- pointer to vnode pager data structure
 *                      containing size and vnode pointer, etc 
 *
 *               m   -- pointer to array of vm_page_t entries to
 *                      do I/O to.  It is not necessary to fill any
 *                      pages except for the reqpage entry.  If a
 *                      page is not filled, it needs to be removed
 *                      from its object...
 *
 *               count -- number of pages for I/O
 *
 *               reqpage -- fault requested page for I/O
 *                          (index into vm_page_t entries above)
 *
 *               rw -- UIO_READ or UIO_WRITE
 *
 * NOTICE!!!! direct writes look like that they are close to being
 *            implemented.  They are not really,  several things need
 *            to be done to make it work (subtile things.)  Hack at
 *            your own risk (direct writes are scarey).
 *
 * ANOTHER NOTICE!!!!
 *	      we currently only support direct I/O to filesystems whose
 *	      contiguously allocated blocksize is at least a vm page.
 *	      changes will be made in the future to support more flexibility.
 */

int
vnode_pager_io(vnp, m, count, reqpage, rw)
	register vn_pager_t vnp;
	vm_page_t *m;
	int count, reqpage;
	enum uio_rw rw;
{
	int i,j;
	struct uio auio;
	struct iovec aiov;
	vm_offset_t kva, foff;
	int size;
	struct proc *p = curproc;		/* XXX */
	vm_object_t object;
	vm_offset_t paging_offset;
	struct vnode *dp, *vp;
	vm_offset_t mapsize;
	int bsize;
	int errtype=0; /* 0 is file type otherwise vm type */
	int error = 0;
	int trimmed;

	object = m[reqpage]->object;	/* all vm_page_t items are in same object */
	paging_offset = object->paging_offset;

	vp = vnp->vnp_vp;
	bsize = vp->v_mount->mnt_stat.f_bsize;

	/* get the UNDERLYING device for the file with VOP_BMAP() */
	/*
	 * originally, we did not check for an error return
	 * value -- assuming an fs always has a bmap entry point
	 * -- that assumption is wrong!!!
	 */
	/*
	 * we only do direct I/O if the file is on a local
	 * BLOCK device and currently if it is a read operation only.
	 */
	kva = 0;
	mapsize = 0;
	if (!VOP_BMAP(vp, m[reqpage]->offset+paging_offset, &dp, 0) &&
		rw == UIO_READ && ((dp->v_type == VBLK &&
		(vp->v_mount->mnt_stat.f_type == MOUNT_UFS)) ||
		 (vp->v_mount->mnt_stat.f_type == MOUNT_NFS))) {
		/*
		 * we do not block for a kva, notice we default to a kva
		 * conservative behavior
		 */
		kva = kmem_alloc_pageable(pager_map,
			(mapsize = count*NBPG));
		if( !kva) {
			for (i = 0; i < count; i++) {
				if (i != reqpage) {
					vnode_pager_freepage(m[i]);
					m[i] = 0;
				}
			}
			m[0] = m[reqpage];
			kva = vm_pager_map_page(m[0]);
			reqpage = 0;
			count = 1;
			mapsize = count*NBPG;
		}
	}

	if (!kva) {
		/*
		 * here on I/O through VFS
		 */
		for (i = 0; i < count; i++) {
			if (i != reqpage) {
				vnode_pager_freepage(m[i]);
				m[i] = 0;
			}
		}
		m[0] = m[reqpage];
		foff = m[0]->offset + paging_offset;
		reqpage = 0;
		count = 1;
	/*
	 * Return failure if beyond current EOF
	 */
		if (foff >= vnp->vnp_size) {
			errtype = 1;
			error = VM_PAGER_BAD;
		} else {
			if (foff + NBPG > vnp->vnp_size)
				size = vnp->vnp_size - foff;
			else
				size = NBPG;
/*
 * Allocate a kernel virtual address and initialize so that
 * we can use VOP_READ/WRITE routines.
 */
			kva = vm_pager_map_page(m[0]);
			aiov.iov_base = (caddr_t)kva;
			aiov.iov_len = size;
			auio.uio_iov = &aiov;
			auio.uio_iovcnt = 1;
			auio.uio_offset = foff;
			auio.uio_segflg = UIO_SYSSPACE;
			auio.uio_rw = rw;
			auio.uio_resid = size;
			auio.uio_procp = (struct proc *)0;
			if (rw == UIO_READ) {
				error = VOP_READ(vp, &auio, IO_PAGER, p->p_ucred);
			} else {
				error = VOP_WRITE(vp, &auio, IO_PAGER, p->p_ucred);
			}
			if (!error) {
				register int count = size - auio.uio_resid;

				if (count == 0)
					error = EINVAL;
				else if (count != NBPG && rw == UIO_READ)
					bzero((caddr_t)kva + count, NBPG - count);
			}
			vm_pager_unmap_page(kva);
		}
	} else {

		/*
		 * here on direct device I/O
		 */
		int first=0, last=count;
		int reqaddr, firstaddr;
		int block, offset;
		
		struct buf *bp;
		int s;
		int failflag;

		foff = m[reqpage]->offset + paging_offset;

		/*
		 * This pathetic hack gets data from the buffer cache, if it's there.
		 * I believe that this is not really necessary, and the ends can
		 * be gotten by defaulting to the normal vfs read behavior, but this
		 * might be more efficient, because the will NOT invoke read-aheads
		 * and one of the purposes of this code is to bypass the buffer
		 * cache and keep from flushing it by reading in a program.
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
			int amount;
					
			/*
			 * wait until the buffer is avail or gone
			 */
			if (bp->b_flags & B_BUSY) {
				bp->b_flags |= B_WANTED;
				tsleep ((caddr_t)bp, PVM, "vnwblk", 0);
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
				pmap_enter(vm_map_pmap(pager_map),
					kva, VM_PAGE_TO_PHYS(m[reqpage]),
					VM_PROT_DEFAULT, TRUE);
				/*
				 * copy the data from the buffer
				 */
				bcopy(bp->b_un.b_addr + offset, (caddr_t)kva, amount);
				if (amount < NBPG) {
					bzero((caddr_t)kva + amount, NBPG - amount);
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
				wakeup((caddr_t)bp);
				/*
				 * we did not have to do any work to get the requested
				 * page, the read behind/ahead does not justify a read
				 */
				for (i = 0; i < count; i++) {
					if (i != reqpage) {
						vnode_pager_freepage(m[i]);
						m[i] = 0;
					}
				}
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

		foff = m[reqpage]->offset + paging_offset;
		reqaddr = vnode_pager_addr(vp, foff);
		/*
		 * Make sure that our I/O request is contiguous.
		 * Scan backward and stop for the first discontiguous
		 *	entry or stop for a page being in buffer cache.
		 */
		failflag = 0;
		for (i = reqpage - 1; i >= 0; --i) {
			int myaddr;
			if (failflag ||
			   incore(vp, (foff + (i - reqpage) * NBPG) / bsize) ||
			   (myaddr = vnode_pager_addr(vp, m[i]->offset + paging_offset))
					!= reqaddr + (i - reqpage) * NBPG) {
				vnode_pager_freepage(m[i]);
				m[i] = 0;
				if (first == 0)
					first = i + 1;
				failflag = 1;
			}
		}

		/*
		 * Scan forward and stop for the first non-contiguous
		 * entry or stop for a page being in buffer cache.
		 */
		failflag = 0;
		for (i = reqpage + 1; i < count; i++) {
			int myaddr;
			if (failflag ||
			   incore(vp, (foff + (i - reqpage) * NBPG) / bsize) ||
			   (myaddr = vnode_pager_addr(vp, m[i]->offset + paging_offset))
					!= reqaddr + (i - reqpage) * NBPG) {
				vnode_pager_freepage(m[i]);
				m[i] = 0;
				if (last == count)
					last = i;
				failflag = 1;
			}
		}

		/*
		 * the first and last page have been calculated now, move input
		 * pages to be zero based...
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
		if ((m[count - 1]->offset + paging_offset) + NBPG > vnp->vnp_size)
			size = vnp->vnp_size - foff;
		else
			size = count * NBPG;


		/*
		 * and map the pages to be read into the kva
		 */
		for (i = 0; i < count; i++)
			pmap_enter(vm_map_pmap(pager_map),
				kva + NBPG * i, VM_PAGE_TO_PHYS(m[i]),
				VM_PROT_DEFAULT, TRUE);
		VHOLD(vp);
		bp = getpbuf();

		/* build a minimal buffer header */
		bzero((caddr_t)bp, sizeof(struct buf));
		bp->b_flags = B_BUSY | B_READ | B_CALL;
		bp->b_iodone = vnode_pager_iodone;
		/* B_PHYS is not set, but it is nice to fill this in */
		/* bp->b_proc = &proc0; */
		bp->b_proc = curproc;
		bp->b_rcred = bp->b_wcred = bp->b_proc->p_ucred;
		bp->b_un.b_addr = (caddr_t) kva;
		bp->b_blkno = firstaddr / DEV_BSIZE;
		bp->b_vp = dp;
	
		/* Should be a BLOCK or character DEVICE if we get here */
		bp->b_dev = dp->v_rdev;
		bp->b_bcount = NBPG * count;

		/* do the input */
		VOP_STRATEGY(bp);

		/* we definitely need to be at splbio here */

		while ((bp->b_flags & B_DONE) == 0) {
			tsleep((caddr_t)bp, PVM, "vnread", 0);
		}
		splx(s);
		if ((bp->b_flags & B_ERROR) != 0)
			error = EIO;

		if (!error) {
			if (size != count * NBPG)
				bzero((caddr_t)kva + size, NBPG * count - size);
		}
		HOLDRELE(vp);

		pmap_remove(vm_map_pmap(pager_map), kva, kva + NBPG * count);
		kmem_free_wakeup(pager_map, kva, mapsize);

		/*
		 * free the buffer header back to the swap buffer pool
		 */
		relpbuf(bp);

	}

finishup:
	if (rw == UIO_READ)
	for (i = 0; i < count; i++) {
		/*
		 * we dont mess with pages that have been already
		 * deallocated....
		 */
		if (!m[i]) 
			continue;
		pmap_clear_modify(VM_PAGE_TO_PHYS(m[i]));
		m[i]->flags |= PG_CLEAN;
		m[i]->flags &= ~PG_LAUNDRY;
		if (i != reqpage) {
			/*
			 * whether or not to leave the page activated
			 * is up in the air, but we should put the page
			 * on a page queue somewhere. (it already is in
			 * the object).
			 * Result: It appears that emperical results show
			 * that deactivating pages is best.
			 */
			/*
			 * just in case someone was asking for this
			 * page we now tell them that it is ok to use
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
	if (!error && rw == UIO_WRITE) {
		pmap_clear_modify(VM_PAGE_TO_PHYS(m[reqpage]));
		m[reqpage]->flags |= PG_CLEAN;
		m[reqpage]->flags &= ~PG_LAUNDRY;
	}
	if (error) {
		printf("vnode pager error: %d\n", error);
	}
	if (errtype)
		return error;
	return (error ? VM_PAGER_FAIL : VM_PAGER_OK);
}
