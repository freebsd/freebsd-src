/*-
 * Copyright (c) 1990 University of Utah.
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1993, 1994 John S. Dyson
 * Copyright (c) 1995, David Greenman
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
 */

/*
 * Page to/from files (vnodes).
 */

/*
 * TODO:
 *	Implement VOP_GETPAGES/PUTPAGES interface for filesystems. Will
 *	greatly re-simplify the vnode_pager.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/vmmeter.h>
#include <sys/limits.h>
#include <sys/conf.h>
#include <sys/rwlock.h>
#include <sys/sf_buf.h>

#include <machine/atomic.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_map.h>
#include <vm/vnode_pager.h>
#include <vm/vm_extern.h>

static int vnode_pager_addr(struct vnode *vp, vm_ooffset_t address,
    daddr_t *rtaddress, int *run);
static int vnode_pager_input_smlfs(vm_object_t object, vm_page_t m);
static int vnode_pager_input_old(vm_object_t object, vm_page_t m);
static void vnode_pager_dealloc(vm_object_t);
static int vnode_pager_getpages(vm_object_t, vm_page_t *, int, int);
static void vnode_pager_putpages(vm_object_t, vm_page_t *, int, boolean_t, int *);
static boolean_t vnode_pager_haspage(vm_object_t, vm_pindex_t, int *, int *);
static vm_object_t vnode_pager_alloc(void *, vm_ooffset_t, vm_prot_t,
    vm_ooffset_t, struct ucred *cred);

struct pagerops vnodepagerops = {
	.pgo_alloc =	vnode_pager_alloc,
	.pgo_dealloc =	vnode_pager_dealloc,
	.pgo_getpages =	vnode_pager_getpages,
	.pgo_putpages =	vnode_pager_putpages,
	.pgo_haspage =	vnode_pager_haspage,
};

int vnode_pbuf_freecnt;

/* Create the VM system backing object for this vnode */
int
vnode_create_vobject(struct vnode *vp, off_t isize, struct thread *td)
{
	vm_object_t object;
	vm_ooffset_t size = isize;
	struct vattr va;

	if (!vn_isdisk(vp, NULL) && vn_canvmio(vp) == FALSE)
		return (0);

	while ((object = vp->v_object) != NULL) {
		VM_OBJECT_WLOCK(object);
		if (!(object->flags & OBJ_DEAD)) {
			VM_OBJECT_WUNLOCK(object);
			return (0);
		}
		VOP_UNLOCK(vp, 0);
		vm_object_set_flag(object, OBJ_DISCONNECTWNT);
		VM_OBJECT_SLEEP(object, object, PDROP | PVM, "vodead", 0);
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
	}

	if (size == 0) {
		if (vn_isdisk(vp, NULL)) {
			size = IDX_TO_OFF(INT_MAX);
		} else {
			if (VOP_GETATTR(vp, &va, td->td_ucred))
				return (0);
			size = va.va_size;
		}
	}

	object = vnode_pager_alloc(vp, size, 0, 0, td->td_ucred);
	/*
	 * Dereference the reference we just created.  This assumes
	 * that the object is associated with the vp.
	 */
	VM_OBJECT_WLOCK(object);
	object->ref_count--;
	VM_OBJECT_WUNLOCK(object);
	vrele(vp);

	KASSERT(vp->v_object != NULL, ("vnode_create_vobject: NULL object"));

	return (0);
}

void
vnode_destroy_vobject(struct vnode *vp)
{
	struct vm_object *obj;

	obj = vp->v_object;
	if (obj == NULL)
		return;
	ASSERT_VOP_ELOCKED(vp, "vnode_destroy_vobject");
	VM_OBJECT_WLOCK(obj);
	if (obj->ref_count == 0) {
		/*
		 * don't double-terminate the object
		 */
		if ((obj->flags & OBJ_DEAD) == 0)
			vm_object_terminate(obj);
		else
			VM_OBJECT_WUNLOCK(obj);
	} else {
		/*
		 * Woe to the process that tries to page now :-).
		 */
		vm_pager_deallocate(obj);
		VM_OBJECT_WUNLOCK(obj);
	}
	vp->v_object = NULL;
}


/*
 * Allocate (or lookup) pager for a vnode.
 * Handle is a vnode pointer.
 *
 * MPSAFE
 */
vm_object_t
vnode_pager_alloc(void *handle, vm_ooffset_t size, vm_prot_t prot,
    vm_ooffset_t offset, struct ucred *cred)
{
	vm_object_t object;
	struct vnode *vp;

	/*
	 * Pageout to vnode, no can do yet.
	 */
	if (handle == NULL)
		return (NULL);

	vp = (struct vnode *) handle;

	/*
	 * If the object is being terminated, wait for it to
	 * go away.
	 */
retry:
	while ((object = vp->v_object) != NULL) {
		VM_OBJECT_WLOCK(object);
		if ((object->flags & OBJ_DEAD) == 0)
			break;
		vm_object_set_flag(object, OBJ_DISCONNECTWNT);
		VM_OBJECT_SLEEP(object, object, PDROP | PVM, "vadead", 0);
	}

	KASSERT(vp->v_usecount != 0, ("vnode_pager_alloc: no vnode reference"));

	if (object == NULL) {
		/*
		 * Add an object of the appropriate size
		 */
		object = vm_object_allocate(OBJT_VNODE, OFF_TO_IDX(round_page(size)));

		object->un_pager.vnp.vnp_size = size;
		object->un_pager.vnp.writemappings = 0;

		object->handle = handle;
		VI_LOCK(vp);
		if (vp->v_object != NULL) {
			/*
			 * Object has been created while we were sleeping
			 */
			VI_UNLOCK(vp);
			vm_object_destroy(object);
			goto retry;
		}
		vp->v_object = object;
		VI_UNLOCK(vp);
	} else {
		object->ref_count++;
		VM_OBJECT_WUNLOCK(object);
	}
	vref(vp);
	return (object);
}

/*
 *	The object must be locked.
 */
static void
vnode_pager_dealloc(object)
	vm_object_t object;
{
	struct vnode *vp;
	int refs;

	vp = object->handle;
	if (vp == NULL)
		panic("vnode_pager_dealloc: pager already dealloced");

	VM_OBJECT_ASSERT_WLOCKED(object);
	vm_object_pip_wait(object, "vnpdea");
	refs = object->ref_count;

	object->handle = NULL;
	object->type = OBJT_DEAD;
	if (object->flags & OBJ_DISCONNECTWNT) {
		vm_object_clear_flag(object, OBJ_DISCONNECTWNT);
		wakeup(object);
	}
	ASSERT_VOP_ELOCKED(vp, "vnode_pager_dealloc");
	if (object->un_pager.vnp.writemappings > 0) {
		object->un_pager.vnp.writemappings = 0;
		VOP_ADD_WRITECOUNT(vp, -1);
		CTR3(KTR_VFS, "%s: vp %p v_writecount decreased to %d",
		    __func__, vp, vp->v_writecount);
	}
	vp->v_object = NULL;
	VOP_UNSET_TEXT(vp);
	VM_OBJECT_WUNLOCK(object);
	while (refs-- > 0)
		vunref(vp);
	VM_OBJECT_WLOCK(object);
}

static boolean_t
vnode_pager_haspage(object, pindex, before, after)
	vm_object_t object;
	vm_pindex_t pindex;
	int *before;
	int *after;
{
	struct vnode *vp = object->handle;
	daddr_t bn;
	int err;
	daddr_t reqblock;
	int poff;
	int bsize;
	int pagesperblock, blocksperpage;

	VM_OBJECT_ASSERT_WLOCKED(object);
	/*
	 * If no vp or vp is doomed or marked transparent to VM, we do not
	 * have the page.
	 */
	if (vp == NULL || vp->v_iflag & VI_DOOMED)
		return FALSE;
	/*
	 * If the offset is beyond end of file we do
	 * not have the page.
	 */
	if (IDX_TO_OFF(pindex) >= object->un_pager.vnp.vnp_size)
		return FALSE;

	bsize = vp->v_mount->mnt_stat.f_iosize;
	pagesperblock = bsize / PAGE_SIZE;
	blocksperpage = 0;
	if (pagesperblock > 0) {
		reqblock = pindex / pagesperblock;
	} else {
		blocksperpage = (PAGE_SIZE / bsize);
		reqblock = pindex * blocksperpage;
	}
	VM_OBJECT_WUNLOCK(object);
	err = VOP_BMAP(vp, reqblock, NULL, &bn, after, before);
	VM_OBJECT_WLOCK(object);
	if (err)
		return TRUE;
	if (bn == -1)
		return FALSE;
	if (pagesperblock > 0) {
		poff = pindex - (reqblock * pagesperblock);
		if (before) {
			*before *= pagesperblock;
			*before += poff;
		}
		if (after) {
			int numafter;
			*after *= pagesperblock;
			numafter = pagesperblock - (poff + 1);
			if (IDX_TO_OFF(pindex + numafter) >
			    object->un_pager.vnp.vnp_size) {
				numafter =
		    		    OFF_TO_IDX(object->un_pager.vnp.vnp_size) -
				    pindex;
			}
			*after += numafter;
		}
	} else {
		if (before) {
			*before /= blocksperpage;
		}

		if (after) {
			*after /= blocksperpage;
		}
	}
	return TRUE;
}

/*
 * Lets the VM system know about a change in size for a file.
 * We adjust our own internal size and flush any cached pages in
 * the associated object that are affected by the size change.
 *
 * Note: this routine may be invoked as a result of a pager put
 * operation (possibly at object termination time), so we must be careful.
 */
void
vnode_pager_setsize(vp, nsize)
	struct vnode *vp;
	vm_ooffset_t nsize;
{
	vm_object_t object;
	vm_page_t m;
	vm_pindex_t nobjsize;

	if ((object = vp->v_object) == NULL)
		return;
/* 	ASSERT_VOP_ELOCKED(vp, "vnode_pager_setsize and not locked vnode"); */
	VM_OBJECT_WLOCK(object);
	if (object->type == OBJT_DEAD) {
		VM_OBJECT_WUNLOCK(object);
		return;
	}
	KASSERT(object->type == OBJT_VNODE,
	    ("not vnode-backed object %p", object));
	if (nsize == object->un_pager.vnp.vnp_size) {
		/*
		 * Hasn't changed size
		 */
		VM_OBJECT_WUNLOCK(object);
		return;
	}
	nobjsize = OFF_TO_IDX(nsize + PAGE_MASK);
	if (nsize < object->un_pager.vnp.vnp_size) {
		/*
		 * File has shrunk. Toss any cached pages beyond the new EOF.
		 */
		if (nobjsize < object->size)
			vm_object_page_remove(object, nobjsize, object->size,
			    0);
		/*
		 * this gets rid of garbage at the end of a page that is now
		 * only partially backed by the vnode.
		 *
		 * XXX for some reason (I don't know yet), if we take a
		 * completely invalid page and mark it partially valid
		 * it can screw up NFS reads, so we don't allow the case.
		 */
		if ((nsize & PAGE_MASK) &&
		    (m = vm_page_lookup(object, OFF_TO_IDX(nsize))) != NULL &&
		    m->valid != 0) {
			int base = (int)nsize & PAGE_MASK;
			int size = PAGE_SIZE - base;

			/*
			 * Clear out partial-page garbage in case
			 * the page has been mapped.
			 */
			pmap_zero_page_area(m, base, size);

			/*
			 * Update the valid bits to reflect the blocks that
			 * have been zeroed.  Some of these valid bits may
			 * have already been set.
			 */
			vm_page_set_valid_range(m, base, size);

			/*
			 * Round "base" to the next block boundary so that the
			 * dirty bit for a partially zeroed block is not
			 * cleared.
			 */
			base = roundup2(base, DEV_BSIZE);

			/*
			 * Clear out partial-page dirty bits.
			 *
			 * note that we do not clear out the valid
			 * bits.  This would prevent bogus_page
			 * replacement from working properly.
			 */
			vm_page_clear_dirty(m, base, PAGE_SIZE - base);
		} else if ((nsize & PAGE_MASK) &&
		    vm_page_is_cached(object, OFF_TO_IDX(nsize))) {
			vm_page_cache_free(object, OFF_TO_IDX(nsize),
			    nobjsize);
		}
	}
	object->un_pager.vnp.vnp_size = nsize;
	object->size = nobjsize;
	VM_OBJECT_WUNLOCK(object);
}

/*
 * calculate the linear (byte) disk address of specified virtual
 * file address
 */
static int
vnode_pager_addr(struct vnode *vp, vm_ooffset_t address, daddr_t *rtaddress,
    int *run)
{
	int bsize;
	int err;
	daddr_t vblock;
	daddr_t voffset;

	if (address < 0)
		return -1;

	if (vp->v_iflag & VI_DOOMED)
		return -1;

	bsize = vp->v_mount->mnt_stat.f_iosize;
	vblock = address / bsize;
	voffset = address % bsize;

	err = VOP_BMAP(vp, vblock, NULL, rtaddress, run, NULL);
	if (err == 0) {
		if (*rtaddress != -1)
			*rtaddress += voffset / DEV_BSIZE;
		if (run) {
			*run += 1;
			*run *= bsize/PAGE_SIZE;
			*run -= voffset/PAGE_SIZE;
		}
	}

	return (err);
}

/*
 * small block filesystem vnode pager input
 */
static int
vnode_pager_input_smlfs(object, m)
	vm_object_t object;
	vm_page_t m;
{
	struct vnode *vp;
	struct bufobj *bo;
	struct buf *bp;
	struct sf_buf *sf;
	daddr_t fileaddr;
	vm_offset_t bsize;
	vm_page_bits_t bits;
	int error, i;

	error = 0;
	vp = object->handle;
	if (vp->v_iflag & VI_DOOMED)
		return VM_PAGER_BAD;

	bsize = vp->v_mount->mnt_stat.f_iosize;

	VOP_BMAP(vp, 0, &bo, 0, NULL, NULL);

	sf = sf_buf_alloc(m, 0);

	for (i = 0; i < PAGE_SIZE / bsize; i++) {
		vm_ooffset_t address;

		bits = vm_page_bits(i * bsize, bsize);
		if (m->valid & bits)
			continue;

		address = IDX_TO_OFF(m->pindex) + i * bsize;
		if (address >= object->un_pager.vnp.vnp_size) {
			fileaddr = -1;
		} else {
			error = vnode_pager_addr(vp, address, &fileaddr, NULL);
			if (error)
				break;
		}
		if (fileaddr != -1) {
			bp = getpbuf(&vnode_pbuf_freecnt);

			/* build a minimal buffer header */
			bp->b_iocmd = BIO_READ;
			bp->b_iodone = bdone;
			KASSERT(bp->b_rcred == NOCRED, ("leaking read ucred"));
			KASSERT(bp->b_wcred == NOCRED, ("leaking write ucred"));
			bp->b_rcred = crhold(curthread->td_ucred);
			bp->b_wcred = crhold(curthread->td_ucred);
			bp->b_data = (caddr_t)sf_buf_kva(sf) + i * bsize;
			bp->b_blkno = fileaddr;
			pbgetbo(bo, bp);
			bp->b_vp = vp;
			bp->b_bcount = bsize;
			bp->b_bufsize = bsize;
			bp->b_runningbufspace = bp->b_bufsize;
			atomic_add_long(&runningbufspace, bp->b_runningbufspace);

			/* do the input */
			bp->b_iooffset = dbtob(bp->b_blkno);
			bstrategy(bp);

			bwait(bp, PVM, "vnsrd");

			if ((bp->b_ioflags & BIO_ERROR) != 0)
				error = EIO;

			/*
			 * free the buffer header back to the swap buffer pool
			 */
			bp->b_vp = NULL;
			pbrelbo(bp);
			relpbuf(bp, &vnode_pbuf_freecnt);
			if (error)
				break;
		} else
			bzero((caddr_t)sf_buf_kva(sf) + i * bsize, bsize);
		KASSERT((m->dirty & bits) == 0,
		    ("vnode_pager_input_smlfs: page %p is dirty", m));
		VM_OBJECT_WLOCK(object);
		m->valid |= bits;
		VM_OBJECT_WUNLOCK(object);
	}
	sf_buf_free(sf);
	if (error) {
		return VM_PAGER_ERROR;
	}
	return VM_PAGER_OK;
}

/*
 * old style vnode pager input routine
 */
static int
vnode_pager_input_old(object, m)
	vm_object_t object;
	vm_page_t m;
{
	struct uio auio;
	struct iovec aiov;
	int error;
	int size;
	struct sf_buf *sf;
	struct vnode *vp;

	VM_OBJECT_ASSERT_WLOCKED(object);
	error = 0;

	/*
	 * Return failure if beyond current EOF
	 */
	if (IDX_TO_OFF(m->pindex) >= object->un_pager.vnp.vnp_size) {
		return VM_PAGER_BAD;
	} else {
		size = PAGE_SIZE;
		if (IDX_TO_OFF(m->pindex) + size > object->un_pager.vnp.vnp_size)
			size = object->un_pager.vnp.vnp_size - IDX_TO_OFF(m->pindex);
		vp = object->handle;
		VM_OBJECT_WUNLOCK(object);

		/*
		 * Allocate a kernel virtual address and initialize so that
		 * we can use VOP_READ/WRITE routines.
		 */
		sf = sf_buf_alloc(m, 0);

		aiov.iov_base = (caddr_t)sf_buf_kva(sf);
		aiov.iov_len = size;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = IDX_TO_OFF(m->pindex);
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_rw = UIO_READ;
		auio.uio_resid = size;
		auio.uio_td = curthread;

		error = VOP_READ(vp, &auio, 0, curthread->td_ucred);
		if (!error) {
			int count = size - auio.uio_resid;

			if (count == 0)
				error = EINVAL;
			else if (count != PAGE_SIZE)
				bzero((caddr_t)sf_buf_kva(sf) + count,
				    PAGE_SIZE - count);
		}
		sf_buf_free(sf);

		VM_OBJECT_WLOCK(object);
	}
	KASSERT(m->dirty == 0, ("vnode_pager_input_old: page %p is dirty", m));
	if (!error)
		m->valid = VM_PAGE_BITS_ALL;
	return error ? VM_PAGER_ERROR : VM_PAGER_OK;
}

/*
 * generic vnode pager input routine
 */

/*
 * Local media VFS's that do not implement their own VOP_GETPAGES
 * should have their VOP_GETPAGES call to vnode_pager_generic_getpages()
 * to implement the previous behaviour.
 *
 * All other FS's should use the bypass to get to the local media
 * backing vp's VOP_GETPAGES.
 */
static int
vnode_pager_getpages(object, m, count, reqpage)
	vm_object_t object;
	vm_page_t *m;
	int count;
	int reqpage;
{
	int rtval;
	struct vnode *vp;
	int bytes = count * PAGE_SIZE;

	vp = object->handle;
	VM_OBJECT_WUNLOCK(object);
	rtval = VOP_GETPAGES(vp, m, bytes, reqpage, 0);
	KASSERT(rtval != EOPNOTSUPP,
	    ("vnode_pager: FS getpages not implemented\n"));
	VM_OBJECT_WLOCK(object);
	return rtval;
}

/*
 * This is now called from local media FS's to operate against their
 * own vnodes if they fail to implement VOP_GETPAGES.
 */
int
vnode_pager_generic_getpages(vp, m, bytecount, reqpage)
	struct vnode *vp;
	vm_page_t *m;
	int bytecount;
	int reqpage;
{
	vm_object_t object;
	vm_offset_t kva;
	off_t foff, tfoff, nextoff;
	int i, j, size, bsize, first;
	daddr_t firstaddr, reqblock;
	struct bufobj *bo;
	int runpg;
	int runend;
	struct buf *bp;
	struct mount *mp;
	int count;
	int error;

	object = vp->v_object;
	count = bytecount / PAGE_SIZE;

	KASSERT(vp->v_type != VCHR && vp->v_type != VBLK,
	    ("vnode_pager_generic_getpages does not support devices"));
	if (vp->v_iflag & VI_DOOMED)
		return VM_PAGER_BAD;

	bsize = vp->v_mount->mnt_stat.f_iosize;

	/* get the UNDERLYING device for the file with VOP_BMAP() */

	/*
	 * originally, we did not check for an error return value -- assuming
	 * an fs always has a bmap entry point -- that assumption is wrong!!!
	 */
	foff = IDX_TO_OFF(m[reqpage]->pindex);

	/*
	 * if we can't bmap, use old VOP code
	 */
	error = VOP_BMAP(vp, foff / bsize, &bo, &reqblock, NULL, NULL);
	if (error == EOPNOTSUPP) {
		VM_OBJECT_WLOCK(object);
		
		for (i = 0; i < count; i++)
			if (i != reqpage) {
				vm_page_lock(m[i]);
				vm_page_free(m[i]);
				vm_page_unlock(m[i]);
			}
		PCPU_INC(cnt.v_vnodein);
		PCPU_INC(cnt.v_vnodepgsin);
		error = vnode_pager_input_old(object, m[reqpage]);
		VM_OBJECT_WUNLOCK(object);
		return (error);
	} else if (error != 0) {
		VM_OBJECT_WLOCK(object);
		for (i = 0; i < count; i++)
			if (i != reqpage) {
				vm_page_lock(m[i]);
				vm_page_free(m[i]);
				vm_page_unlock(m[i]);
			}
		VM_OBJECT_WUNLOCK(object);
		return (VM_PAGER_ERROR);

		/*
		 * if the blocksize is smaller than a page size, then use
		 * special small filesystem code.  NFS sometimes has a small
		 * blocksize, but it can handle large reads itself.
		 */
	} else if ((PAGE_SIZE / bsize) > 1 &&
	    (vp->v_mount->mnt_stat.f_type != nfs_mount_type)) {
		VM_OBJECT_WLOCK(object);
		for (i = 0; i < count; i++)
			if (i != reqpage) {
				vm_page_lock(m[i]);
				vm_page_free(m[i]);
				vm_page_unlock(m[i]);
			}
		VM_OBJECT_WUNLOCK(object);
		PCPU_INC(cnt.v_vnodein);
		PCPU_INC(cnt.v_vnodepgsin);
		return vnode_pager_input_smlfs(object, m[reqpage]);
	}

	/*
	 * If we have a completely valid page available to us, we can
	 * clean up and return.  Otherwise we have to re-read the
	 * media.
	 */
	VM_OBJECT_WLOCK(object);
	if (m[reqpage]->valid == VM_PAGE_BITS_ALL) {
		for (i = 0; i < count; i++)
			if (i != reqpage) {
				vm_page_lock(m[i]);
				vm_page_free(m[i]);
				vm_page_unlock(m[i]);
			}
		VM_OBJECT_WUNLOCK(object);
		return VM_PAGER_OK;
	} else if (reqblock == -1) {
		pmap_zero_page(m[reqpage]);
		KASSERT(m[reqpage]->dirty == 0,
		    ("vnode_pager_generic_getpages: page %p is dirty", m));
		m[reqpage]->valid = VM_PAGE_BITS_ALL;
		for (i = 0; i < count; i++)
			if (i != reqpage) {
				vm_page_lock(m[i]);
				vm_page_free(m[i]);
				vm_page_unlock(m[i]);
			}
		VM_OBJECT_WUNLOCK(object);
		return (VM_PAGER_OK);
	}
	m[reqpage]->valid = 0;
	VM_OBJECT_WUNLOCK(object);

	/*
	 * here on direct device I/O
	 */
	firstaddr = -1;

	/*
	 * calculate the run that includes the required page
	 */
	for (first = 0, i = 0; i < count; i = runend) {
		if (vnode_pager_addr(vp, IDX_TO_OFF(m[i]->pindex), &firstaddr,
		    &runpg) != 0) {
			VM_OBJECT_WLOCK(object);
			for (; i < count; i++)
				if (i != reqpage) {
					vm_page_lock(m[i]);
					vm_page_free(m[i]);
					vm_page_unlock(m[i]);
				}
			VM_OBJECT_WUNLOCK(object);
			return (VM_PAGER_ERROR);
		}
		if (firstaddr == -1) {
			VM_OBJECT_WLOCK(object);
			if (i == reqpage && foff < object->un_pager.vnp.vnp_size) {
				panic("vnode_pager_getpages: unexpected missing page: firstaddr: %jd, foff: 0x%jx%08jx, vnp_size: 0x%jx%08jx",
				    (intmax_t)firstaddr, (uintmax_t)(foff >> 32),
				    (uintmax_t)foff,
				    (uintmax_t)
				    (object->un_pager.vnp.vnp_size >> 32),
				    (uintmax_t)object->un_pager.vnp.vnp_size);
			}
			vm_page_lock(m[i]);
			vm_page_free(m[i]);
			vm_page_unlock(m[i]);
			VM_OBJECT_WUNLOCK(object);
			runend = i + 1;
			first = runend;
			continue;
		}
		runend = i + runpg;
		if (runend <= reqpage) {
			VM_OBJECT_WLOCK(object);
			for (j = i; j < runend; j++) {
				vm_page_lock(m[j]);
				vm_page_free(m[j]);
				vm_page_unlock(m[j]);
			}
			VM_OBJECT_WUNLOCK(object);
		} else {
			if (runpg < (count - first)) {
				VM_OBJECT_WLOCK(object);
				for (i = first + runpg; i < count; i++) {
					vm_page_lock(m[i]);
					vm_page_free(m[i]);
					vm_page_unlock(m[i]);
				}
				VM_OBJECT_WUNLOCK(object);
				count = first + runpg;
			}
			break;
		}
		first = runend;
	}

	/*
	 * the first and last page have been calculated now, move input pages
	 * to be zero based...
	 */
	if (first != 0) {
		m += first;
		count -= first;
		reqpage -= first;
	}

	/*
	 * calculate the file virtual address for the transfer
	 */
	foff = IDX_TO_OFF(m[0]->pindex);

	/*
	 * calculate the size of the transfer
	 */
	size = count * PAGE_SIZE;
	KASSERT(count > 0, ("zero count"));
	if ((foff + size) > object->un_pager.vnp.vnp_size)
		size = object->un_pager.vnp.vnp_size - foff;
	KASSERT(size > 0, ("zero size"));

	/*
	 * round up physical size for real devices.
	 */
	if (1) {
		int secmask = bo->bo_bsize - 1;
		KASSERT(secmask < PAGE_SIZE && secmask > 0,
		    ("vnode_pager_generic_getpages: sector size %d too large",
		    secmask + 1));
		size = (size + secmask) & ~secmask;
	}

	bp = getpbuf(&vnode_pbuf_freecnt);
	kva = (vm_offset_t)bp->b_data;

	/*
	 * and map the pages to be read into the kva, if the filesystem
	 * requires mapped buffers.
	 */
	mp = vp->v_mount;
	if (mp != NULL && (mp->mnt_kern_flag & MNTK_UNMAPPED_BUFS) != 0 &&
	    unmapped_buf_allowed) {
		bp->b_data = unmapped_buf;
		bp->b_kvabase = unmapped_buf;
		bp->b_offset = 0;
		bp->b_flags |= B_UNMAPPED;
		bp->b_npages = count;
		for (i = 0; i < count; i++)
			bp->b_pages[i] = m[i];
	} else
		pmap_qenter(kva, m, count);

	/* build a minimal buffer header */
	bp->b_iocmd = BIO_READ;
	bp->b_iodone = bdone;
	KASSERT(bp->b_rcred == NOCRED, ("leaking read ucred"));
	KASSERT(bp->b_wcred == NOCRED, ("leaking write ucred"));
	bp->b_rcred = crhold(curthread->td_ucred);
	bp->b_wcred = crhold(curthread->td_ucred);
	bp->b_blkno = firstaddr;
	pbgetbo(bo, bp);
	bp->b_vp = vp;
	bp->b_bcount = size;
	bp->b_bufsize = size;
	bp->b_runningbufspace = bp->b_bufsize;
	atomic_add_long(&runningbufspace, bp->b_runningbufspace);

	PCPU_INC(cnt.v_vnodein);
	PCPU_ADD(cnt.v_vnodepgsin, count);

	/* do the input */
	bp->b_iooffset = dbtob(bp->b_blkno);
	bstrategy(bp);

	bwait(bp, PVM, "vnread");

	if ((bp->b_ioflags & BIO_ERROR) != 0)
		error = EIO;

	if (error == 0 && size != count * PAGE_SIZE) {
		if ((bp->b_flags & B_UNMAPPED) != 0) {
			bp->b_flags &= ~B_UNMAPPED;
			pmap_qenter(kva, m, count);
		}
		bzero((caddr_t)kva + size, PAGE_SIZE * count - size);
	}
	if ((bp->b_flags & B_UNMAPPED) == 0)
		pmap_qremove(kva, count);
	if (mp != NULL && (mp->mnt_kern_flag & MNTK_UNMAPPED_BUFS) != 0) {
		bp->b_data = (caddr_t)kva;
		bp->b_kvabase = (caddr_t)kva;
		bp->b_flags &= ~B_UNMAPPED;
		for (i = 0; i < count; i++)
			bp->b_pages[i] = NULL;
	}

	/*
	 * free the buffer header back to the swap buffer pool
	 */
	bp->b_vp = NULL;
	pbrelbo(bp);
	relpbuf(bp, &vnode_pbuf_freecnt);

	VM_OBJECT_WLOCK(object);
	for (i = 0, tfoff = foff; i < count; i++, tfoff = nextoff) {
		vm_page_t mt;

		nextoff = tfoff + PAGE_SIZE;
		mt = m[i];

		if (nextoff <= object->un_pager.vnp.vnp_size) {
			/*
			 * Read filled up entire page.
			 */
			mt->valid = VM_PAGE_BITS_ALL;
			KASSERT(mt->dirty == 0,
			    ("vnode_pager_generic_getpages: page %p is dirty",
			    mt));
			KASSERT(!pmap_page_is_mapped(mt),
			    ("vnode_pager_generic_getpages: page %p is mapped",
			    mt));
		} else {
			/*
			 * Read did not fill up entire page.
			 *
			 * Currently we do not set the entire page valid,
			 * we just try to clear the piece that we couldn't
			 * read.
			 */
			vm_page_set_valid_range(mt, 0,
			    object->un_pager.vnp.vnp_size - tfoff);
			KASSERT((mt->dirty & vm_page_bits(0,
			    object->un_pager.vnp.vnp_size - tfoff)) == 0,
			    ("vnode_pager_generic_getpages: page %p is dirty",
			    mt));
		}
		
		if (i != reqpage)
			vm_page_readahead_finish(mt);
	}
	VM_OBJECT_WUNLOCK(object);
	if (error) {
		printf("vnode_pager_getpages: I/O read error\n");
	}
	return (error ? VM_PAGER_ERROR : VM_PAGER_OK);
}

/*
 * EOPNOTSUPP is no longer legal.  For local media VFS's that do not
 * implement their own VOP_PUTPAGES, their VOP_PUTPAGES should call to
 * vnode_pager_generic_putpages() to implement the previous behaviour.
 *
 * All other FS's should use the bypass to get to the local media
 * backing vp's VOP_PUTPAGES.
 */
static void
vnode_pager_putpages(object, m, count, sync, rtvals)
	vm_object_t object;
	vm_page_t *m;
	int count;
	boolean_t sync;
	int *rtvals;
{
	int rtval;
	struct vnode *vp;
	int bytes = count * PAGE_SIZE;

	/*
	 * Force synchronous operation if we are extremely low on memory
	 * to prevent a low-memory deadlock.  VOP operations often need to
	 * allocate more memory to initiate the I/O ( i.e. do a BMAP 
	 * operation ).  The swapper handles the case by limiting the amount
	 * of asynchronous I/O, but that sort of solution doesn't scale well
	 * for the vnode pager without a lot of work.
	 *
	 * Also, the backing vnode's iodone routine may not wake the pageout
	 * daemon up.  This should be probably be addressed XXX.
	 */

	if ((cnt.v_free_count + cnt.v_cache_count) < cnt.v_pageout_free_min)
		sync |= OBJPC_SYNC;

	/*
	 * Call device-specific putpages function
	 */
	vp = object->handle;
	VM_OBJECT_WUNLOCK(object);
	rtval = VOP_PUTPAGES(vp, m, bytes, sync, rtvals, 0);
	KASSERT(rtval != EOPNOTSUPP, 
	    ("vnode_pager: stale FS putpages\n"));
	VM_OBJECT_WLOCK(object);
}


/*
 * This is now called from local media FS's to operate against their
 * own vnodes if they fail to implement VOP_PUTPAGES.
 *
 * This is typically called indirectly via the pageout daemon and
 * clustering has already typically occured, so in general we ask the
 * underlying filesystem to write the data out asynchronously rather
 * then delayed.
 */
int
vnode_pager_generic_putpages(struct vnode *vp, vm_page_t *ma, int bytecount,
    int flags, int *rtvals)
{
	int i;
	vm_object_t object;
	vm_page_t m;
	int count;

	int maxsize, ncount;
	vm_ooffset_t poffset;
	struct uio auio;
	struct iovec aiov;
	int error;
	int ioflags;
	int ppscheck = 0;
	static struct timeval lastfail;
	static int curfail;

	object = vp->v_object;
	count = bytecount / PAGE_SIZE;

	for (i = 0; i < count; i++)
		rtvals[i] = VM_PAGER_ERROR;

	if ((int64_t)ma[0]->pindex < 0) {
		printf("vnode_pager_putpages: attempt to write meta-data!!! -- 0x%lx(%lx)\n",
		    (long)ma[0]->pindex, (u_long)ma[0]->dirty);
		rtvals[0] = VM_PAGER_BAD;
		return VM_PAGER_BAD;
	}

	maxsize = count * PAGE_SIZE;
	ncount = count;

	poffset = IDX_TO_OFF(ma[0]->pindex);

	/*
	 * If the page-aligned write is larger then the actual file we
	 * have to invalidate pages occuring beyond the file EOF.  However,
	 * there is an edge case where a file may not be page-aligned where
	 * the last page is partially invalid.  In this case the filesystem
	 * may not properly clear the dirty bits for the entire page (which
	 * could be VM_PAGE_BITS_ALL due to the page having been mmap()d).
	 * With the page locked we are free to fix-up the dirty bits here.
	 *
	 * We do not under any circumstances truncate the valid bits, as
	 * this will screw up bogus page replacement.
	 */
	VM_OBJECT_WLOCK(object);
	if (maxsize + poffset > object->un_pager.vnp.vnp_size) {
		if (object->un_pager.vnp.vnp_size > poffset) {
			int pgoff;

			maxsize = object->un_pager.vnp.vnp_size - poffset;
			ncount = btoc(maxsize);
			if ((pgoff = (int)maxsize & PAGE_MASK) != 0) {
				/*
				 * If the object is locked and the following
				 * conditions hold, then the page's dirty
				 * field cannot be concurrently changed by a
				 * pmap operation.
				 */
				m = ma[ncount - 1];
				vm_page_assert_sbusied(m);
				KASSERT(!pmap_page_is_write_mapped(m),
		("vnode_pager_generic_putpages: page %p is not read-only", m));
				vm_page_clear_dirty(m, pgoff, PAGE_SIZE -
				    pgoff);
			}
		} else {
			maxsize = 0;
			ncount = 0;
		}
		if (ncount < count) {
			for (i = ncount; i < count; i++) {
				rtvals[i] = VM_PAGER_BAD;
			}
		}
	}
	VM_OBJECT_WUNLOCK(object);

	/*
	 * pageouts are already clustered, use IO_ASYNC to force a bawrite()
	 * rather then a bdwrite() to prevent paging I/O from saturating 
	 * the buffer cache.  Dummy-up the sequential heuristic to cause
	 * large ranges to cluster.  If neither IO_SYNC or IO_ASYNC is set,
	 * the system decides how to cluster.
	 */
	ioflags = IO_VMIO;
	if (flags & (VM_PAGER_PUT_SYNC | VM_PAGER_PUT_INVAL))
		ioflags |= IO_SYNC;
	else if ((flags & VM_PAGER_CLUSTER_OK) == 0)
		ioflags |= IO_ASYNC;
	ioflags |= (flags & VM_PAGER_PUT_INVAL) ? IO_INVAL: 0;
	ioflags |= IO_SEQMAX << IO_SEQSHIFT;

	aiov.iov_base = (caddr_t) 0;
	aiov.iov_len = maxsize;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = poffset;
	auio.uio_segflg = UIO_NOCOPY;
	auio.uio_rw = UIO_WRITE;
	auio.uio_resid = maxsize;
	auio.uio_td = (struct thread *) 0;
	error = VOP_WRITE(vp, &auio, ioflags, curthread->td_ucred);
	PCPU_INC(cnt.v_vnodeout);
	PCPU_ADD(cnt.v_vnodepgsout, ncount);

	if (error) {
		if ((ppscheck = ppsratecheck(&lastfail, &curfail, 1)))
			printf("vnode_pager_putpages: I/O error %d\n", error);
	}
	if (auio.uio_resid) {
		if (ppscheck || ppsratecheck(&lastfail, &curfail, 1))
			printf("vnode_pager_putpages: residual I/O %zd at %lu\n",
			    auio.uio_resid, (u_long)ma[0]->pindex);
	}
	for (i = 0; i < ncount; i++) {
		rtvals[i] = VM_PAGER_OK;
	}
	return rtvals[0];
}

void
vnode_pager_undirty_pages(vm_page_t *ma, int *rtvals, int written)
{
	vm_object_t obj;
	int i, pos;

	if (written == 0)
		return;
	obj = ma[0]->object;
	VM_OBJECT_WLOCK(obj);
	for (i = 0, pos = 0; pos < written; i++, pos += PAGE_SIZE) {
		if (pos < trunc_page(written)) {
			rtvals[i] = VM_PAGER_OK;
			vm_page_undirty(ma[i]);
		} else {
			/* Partially written page. */
			rtvals[i] = VM_PAGER_AGAIN;
			vm_page_clear_dirty(ma[i], 0, written & PAGE_MASK);
		}
	}
	VM_OBJECT_WUNLOCK(obj);
}

void
vnode_pager_update_writecount(vm_object_t object, vm_offset_t start,
    vm_offset_t end)
{
	struct vnode *vp;
	vm_ooffset_t old_wm;

	VM_OBJECT_WLOCK(object);
	if (object->type != OBJT_VNODE) {
		VM_OBJECT_WUNLOCK(object);
		return;
	}
	old_wm = object->un_pager.vnp.writemappings;
	object->un_pager.vnp.writemappings += (vm_ooffset_t)end - start;
	vp = object->handle;
	if (old_wm == 0 && object->un_pager.vnp.writemappings != 0) {
		ASSERT_VOP_ELOCKED(vp, "v_writecount inc");
		VOP_ADD_WRITECOUNT(vp, 1);
		CTR3(KTR_VFS, "%s: vp %p v_writecount increased to %d",
		    __func__, vp, vp->v_writecount);
	} else if (old_wm != 0 && object->un_pager.vnp.writemappings == 0) {
		ASSERT_VOP_ELOCKED(vp, "v_writecount dec");
		VOP_ADD_WRITECOUNT(vp, -1);
		CTR3(KTR_VFS, "%s: vp %p v_writecount decreased to %d",
		    __func__, vp, vp->v_writecount);
	}
	VM_OBJECT_WUNLOCK(object);
}

void
vnode_pager_release_writecount(vm_object_t object, vm_offset_t start,
    vm_offset_t end)
{
	struct vnode *vp;
	struct mount *mp;
	vm_offset_t inc;

	VM_OBJECT_WLOCK(object);

	/*
	 * First, recheck the object type to account for the race when
	 * the vnode is reclaimed.
	 */
	if (object->type != OBJT_VNODE) {
		VM_OBJECT_WUNLOCK(object);
		return;
	}

	/*
	 * Optimize for the case when writemappings is not going to
	 * zero.
	 */
	inc = end - start;
	if (object->un_pager.vnp.writemappings != inc) {
		object->un_pager.vnp.writemappings -= inc;
		VM_OBJECT_WUNLOCK(object);
		return;
	}

	vp = object->handle;
	vhold(vp);
	VM_OBJECT_WUNLOCK(object);
	mp = NULL;
	vn_start_write(vp, &mp, V_WAIT);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);

	/*
	 * Decrement the object's writemappings, by swapping the start
	 * and end arguments for vnode_pager_update_writecount().  If
	 * there was not a race with vnode reclaimation, then the
	 * vnode's v_writecount is decremented.
	 */
	vnode_pager_update_writecount(object, end, start);
	VOP_UNLOCK(vp, 0);
	vdrop(vp);
	if (mp != NULL)
		vn_finished_write(mp);
}
