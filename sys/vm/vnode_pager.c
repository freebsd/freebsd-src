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
 *	from: @(#)vnode_pager.c	7.5 (Berkeley) 4/20/91
 *	$Id: vnode_pager.c,v 1.2 1993/10/16 16:21:02 rgrimes Exp $
 */

/*
 * Page to/from files (vnodes).
 *
 * TODO:
 *	pageouts
 *	fix credential use (uses current process credentials now)
 */
#include "vnodepager.h"
#if NVNODEPAGER > 0

#include "param.h"
#include "proc.h"
#include "malloc.h"
#include "vnode.h"
#include "uio.h"
#include "mount.h"

#include "vm_param.h"
#include "lock.h"
#include "queue.h"
#include "vm_prot.h"
#include "vm_object.h"
#include "vm_page.h"
#include "vnode_pager.h"

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
vnode_pager_alloc(handle, size, prot)
	caddr_t handle;
	vm_size_t size;
	vm_prot_t prot;
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

vnode_pager_getpage(pager, m, sync)
	vm_pager_t pager;
	vm_page_t m;
	boolean_t sync;
{

#ifdef DEBUG
	if (vpagerdebug & VDB_FOLLOW)
		printf("vnode_pager_getpage(%x, %x)\n", pager, m);
#endif
	return(vnode_pager_io((vn_pager_t)pager->pg_data, m, UIO_READ));
}

boolean_t
vnode_pager_putpage(pager, m, sync)
	vm_pager_t pager;
	vm_page_t m;
	boolean_t sync;
{
	int err;

#ifdef DEBUG
	if (vpagerdebug & VDB_FOLLOW)
		printf("vnode_pager_putpage(%x, %x)\n", pager, m);
#endif
	if (pager == NULL)
		return;
	err = vnode_pager_io((vn_pager_t)pager->pg_data, m, UIO_WRITE);
	if (err == VM_PAGER_OK) {
		m->clean = TRUE;			/* XXX - wrong place */
		pmap_clear_modify(VM_PAGE_TO_PHYS(m));	/* XXX - wrong place */
	}
	return(err);
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
	if (nsize < vnp->vnp_size) {
		vm_object_lock(object);
		vm_object_page_remove(object,
				      (vm_offset_t)nsize, vnp->vnp_size);
		vm_object_unlock(object);
	}
	vnp->vnp_size = (vm_offset_t)nsize;
	vm_object_deallocate(object);
}

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

vnode_pager_io(vnp, m, rw)
	register vn_pager_t vnp;
	vm_page_t m;
	enum uio_rw rw;
{
	struct uio auio;
	struct iovec aiov;
	vm_offset_t kva, foff;
	int error, size;
	struct proc *p = curproc;		/* XXX */

#ifdef DEBUG
	if (vpagerdebug & VDB_FOLLOW)
		printf("vnode_pager_io(%x, %x, %c): vnode %x\n",
		       vnp, m, rw == UIO_READ ? 'R' : 'W', vnp->vnp_vp);
#endif
	foff = m->offset + m->object->paging_offset;
	/*
	 * Return failure if beyond current EOF
	 */
	if (foff >= vnp->vnp_size) {
#ifdef DEBUG
		if (vpagerdebug & VDB_SIZE)
			printf("vnode_pager_io: vp %x, off %d size %d\n",
			       vnp->vnp_vp, foff, vnp->vnp_size);
#endif
		return(VM_PAGER_BAD);
	}
	if (foff + PAGE_SIZE > vnp->vnp_size)
		size = vnp->vnp_size - foff;
	else
		size = PAGE_SIZE;
	/*
	 * Allocate a kernel virtual address and initialize so that
	 * we can use VOP_READ/WRITE routines.
	 */
	kva = vm_pager_map_page(m);
	aiov.iov_base = (caddr_t)kva;
	aiov.iov_len = size;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = foff;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = rw;
	auio.uio_resid = size;
	auio.uio_procp = (struct proc *)0;
#ifdef DEBUG
	if (vpagerdebug & VDB_IO)
		printf("vnode_pager_io: vp %x kva %x foff %x size %x",
		       vnp->vnp_vp, kva, foff, size);
#endif
	if (rw == UIO_READ)
		error = VOP_READ(vnp->vnp_vp, &auio, 0, p->p_ucred);
	else
		error = VOP_WRITE(vnp->vnp_vp, &auio, 0, p->p_ucred);
#ifdef DEBUG
	if (vpagerdebug & VDB_IO) {
		if (error || auio.uio_resid)
			printf(" returns error %x, resid %x",
			       error, auio.uio_resid);
		printf("\n");
	}
#endif
	if (!error) {
		register int count = size - auio.uio_resid;

		if (count == 0)
			error = EINVAL;
		else if (count != PAGE_SIZE && rw == UIO_READ)
			bzero(kva + count, PAGE_SIZE - count);
	}
	vm_pager_unmap_page(kva);
	return (error ? VM_PAGER_FAIL : VM_PAGER_OK);
}
#endif
