/*
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
#include <sys/conf.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_map.h>
#include <vm/vnode_pager.h>
#include <vm/vm_extern.h>

static void vnode_pager_init(void);
static vm_offset_t vnode_pager_addr(struct vnode *vp, vm_ooffset_t address,
					 int *run);
static int vnode_pager_input_smlfs(vm_object_t object, vm_page_t m);
static int vnode_pager_input_old(vm_object_t object, vm_page_t m);
static void vnode_pager_dealloc(vm_object_t);
static int vnode_pager_getpages(vm_object_t, vm_page_t *, int, int);
static void vnode_pager_putpages(vm_object_t, vm_page_t *, int, boolean_t, int *);
static boolean_t vnode_pager_haspage(vm_object_t, vm_pindex_t, int *, int *);

struct pagerops vnodepagerops = {
	.pgo_init =	vnode_pager_init,
	.pgo_alloc =	vnode_pager_alloc,
	.pgo_dealloc =	vnode_pager_dealloc,
	.pgo_getpages =	vnode_pager_getpages,
	.pgo_putpages =	vnode_pager_putpages,
	.pgo_haspage =	vnode_pager_haspage,
};

int vnode_pbuf_freecnt;

static void
vnode_pager_init(void)
{

	vnode_pbuf_freecnt = nswbuf / 2 + 1;
}

/*
 * Allocate (or lookup) pager for a vnode.
 * Handle is a vnode pointer.
 *
 * MPSAFE
 */
vm_object_t
vnode_pager_alloc(void *handle, vm_ooffset_t size, vm_prot_t prot,
		  vm_ooffset_t offset)
{
	vm_object_t object;
	struct vnode *vp;

	/*
	 * Pageout to vnode, no can do yet.
	 */
	if (handle == NULL)
		return (NULL);

	vp = (struct vnode *) handle;

	ASSERT_VOP_LOCKED(vp, "vnode_pager_alloc");

	mtx_lock(&Giant);
	/*
	 * Prevent race condition when allocating the object. This
	 * can happen with NFS vnodes since the nfsnode isn't locked.
	 */
	VI_LOCK(vp);
	while (vp->v_iflag & VI_OLOCK) {
		vp->v_iflag |= VI_OWANT;
		msleep(vp, VI_MTX(vp), PVM, "vnpobj", 0);
	}
	vp->v_iflag |= VI_OLOCK;
	VI_UNLOCK(vp);

	/*
	 * If the object is being terminated, wait for it to
	 * go away.
	 */
	while ((object = vp->v_object) != NULL) {
		VM_OBJECT_LOCK(object);
		if ((object->flags & OBJ_DEAD) == 0)
			break;
		msleep(object, VM_OBJECT_MTX(object), PDROP | PVM, "vadead", 0);
	}

	if (vp->v_usecount == 0)
		panic("vnode_pager_alloc: no vnode reference");

	if (object == NULL) {
		/*
		 * And an object of the appropriate size
		 */
		object = vm_object_allocate(OBJT_VNODE, OFF_TO_IDX(round_page(size)));

		object->un_pager.vnp.vnp_size = size;

		object->handle = handle;
		vp->v_object = object;
	} else {
		object->ref_count++;
		VM_OBJECT_UNLOCK(object);
	}
	VI_LOCK(vp);
	vp->v_usecount++;
	vp->v_iflag &= ~VI_OLOCK;
	if (vp->v_iflag & VI_OWANT) {
		vp->v_iflag &= ~VI_OWANT;
		wakeup(vp);
	}
	VI_UNLOCK(vp);
	mtx_unlock(&Giant);
	return (object);
}

/*
 *	The object must be locked.
 */
static void
vnode_pager_dealloc(object)
	vm_object_t object;
{
	struct vnode *vp = object->handle;

	if (vp == NULL)
		panic("vnode_pager_dealloc: pager already dealloced");

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	vm_object_pip_wait(object, "vnpdea");

	object->handle = NULL;
	object->type = OBJT_DEAD;
	ASSERT_VOP_LOCKED(vp, "vnode_pager_dealloc");
	vp->v_object = NULL;
	vp->v_vflag &= ~(VV_TEXT | VV_OBJBUF);
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

	VM_OBJECT_LOCK_ASSERT(object, MA_OWNED);
	/*
	 * If no vp or vp is doomed or marked transparent to VM, we do not
	 * have the page.
	 */
	if (vp == NULL)
		return FALSE;

	VI_LOCK(vp);
	if (vp->v_iflag & VI_DOOMED) {
		VI_UNLOCK(vp);
		return FALSE;
	}
	VI_UNLOCK(vp);
	/*
	 * If filesystem no longer mounted or offset beyond end of file we do
	 * not have the page.
	 */
	if ((vp->v_mount == NULL) ||
	    (IDX_TO_OFF(pindex) >= object->un_pager.vnp.vnp_size))
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
	VM_OBJECT_UNLOCK(object);
	err = VOP_BMAP(vp, reqblock, NULL, &bn, after, before);
	VM_OBJECT_LOCK(object);
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
	VM_OBJECT_LOCK(object);
	if (nsize == object->un_pager.vnp.vnp_size) {
		/*
		 * Hasn't changed size
		 */
		VM_OBJECT_UNLOCK(object);
		return;
	}
	nobjsize = OFF_TO_IDX(nsize + PAGE_MASK);
	if (nsize < object->un_pager.vnp.vnp_size) {
		/*
		 * File has shrunk. Toss any cached pages beyond the new EOF.
		 */
		if (nobjsize < object->size)
			vm_object_page_remove(object, nobjsize, object->size,
			    FALSE);
		/*
		 * this gets rid of garbage at the end of a page that is now
		 * only partially backed by the vnode.
		 *
		 * XXX for some reason (I don't know yet), if we take a
		 * completely invalid page and mark it partially valid
		 * it can screw up NFS reads, so we don't allow the case.
		 */
		if ((nsize & PAGE_MASK) &&
		    (m = vm_page_lookup(object, OFF_TO_IDX(nsize))) != NULL) {
			vm_page_lock_queues();
			if (m->valid) {
				int base = (int)nsize & PAGE_MASK;
				int size = PAGE_SIZE - base;

				/*
				 * Clear out partial-page garbage in case
				 * the page has been mapped.
				 */
				pmap_zero_page_area(m, base, size);

				/*
				 * XXX work around SMP data integrity race
				 * by unmapping the page from user processes.
				 * The garbage we just cleared may be mapped
				 * to a user process running on another cpu
				 * and this code is not running through normal
				 * I/O channels which handle SMP issues for
				 * us, so unmap page to synchronize all cpus.
				 *
				 * XXX should vm_pager_unmap_page() have
				 * dealt with this?
				 */
				pmap_remove_all(m);

				/*
				 * Clear out partial-page dirty bits.  This
				 * has the side effect of setting the valid
				 * bits, but that is ok.  There are a bunch
				 * of places in the VM system where we expected
				 * m->dirty == VM_PAGE_BITS_ALL.  The file EOF
				 * case is one of them.  If the page is still
				 * partially dirty, make it fully dirty.
				 *
				 * note that we do not clear out the valid
				 * bits.  This would prevent bogus_page
				 * replacement from working properly.
				 */
				vm_page_set_validclean(m, base, size);
				if (m->dirty != 0)
					m->dirty = VM_PAGE_BITS_ALL;
			}
			vm_page_unlock_queues();
		}
	}
	object->un_pager.vnp.vnp_size = nsize;
	object->size = nobjsize;
	VM_OBJECT_UNLOCK(object);
}

/*
 * calculate the linear (byte) disk address of specified virtual
 * file address
 */
static vm_offset_t
vnode_pager_addr(vp, address, run)
	struct vnode *vp;
	vm_ooffset_t address;
	int *run;
{
	int rtaddress;
	int bsize;
	daddr_t block;
	int err;
	daddr_t vblock;
	int voffset;

	GIANT_REQUIRED;
	if ((int) address < 0)
		return -1;

	if (vp->v_mount == NULL)
		return -1;

	bsize = vp->v_mount->mnt_stat.f_iosize;
	vblock = address / bsize;
	voffset = address % bsize;

	err = VOP_BMAP(vp, vblock, NULL, &block, run, NULL);

	if (err || (block == -1))
		rtaddress = -1;
	else {
		rtaddress = block + voffset / DEV_BSIZE;
		if (run) {
			*run += 1;
			*run *= bsize/PAGE_SIZE;
			*run -= voffset/PAGE_SIZE;
		}
	}

	return rtaddress;
}

/*
 * small block filesystem vnode pager input
 */
static int
vnode_pager_input_smlfs(object, m)
	vm_object_t object;
	vm_page_t m;
{
	int i;
	struct vnode *dp, *vp;
	struct buf *bp;
	vm_offset_t kva;
	int fileaddr;
	vm_offset_t bsize;
	int error = 0;

	GIANT_REQUIRED;

	vp = object->handle;
	if (vp->v_mount == NULL)
		return VM_PAGER_BAD;

	bsize = vp->v_mount->mnt_stat.f_iosize;

	VOP_BMAP(vp, 0, &dp, 0, NULL, NULL);

	kva = vm_pager_map_page(m);

	for (i = 0; i < PAGE_SIZE / bsize; i++) {
		vm_ooffset_t address;

		if (vm_page_bits(i * bsize, bsize) & m->valid)
			continue;

		address = IDX_TO_OFF(m->pindex) + i * bsize;
		if (address >= object->un_pager.vnp.vnp_size) {
			fileaddr = -1;
		} else {
			fileaddr = vnode_pager_addr(vp, address, NULL);
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
			bp->b_data = (caddr_t) kva + i * bsize;
			bp->b_blkno = fileaddr;
			pbgetvp(dp, bp);
			bp->b_bcount = bsize;
			bp->b_bufsize = bsize;
			bp->b_runningbufspace = bp->b_bufsize;
			runningbufspace += bp->b_runningbufspace;

			/* do the input */
			bp->b_iooffset = dbtob(bp->b_blkno);
			VOP_SPECSTRATEGY(bp->b_vp, bp);

			/* we definitely need to be at splvm here */

			bwait(bp, PVM, "vnsrd");

			if ((bp->b_ioflags & BIO_ERROR) != 0)
				error = EIO;

			/*
			 * free the buffer header back to the swap buffer pool
			 */
			relpbuf(bp, &vnode_pbuf_freecnt);
			if (error)
				break;

			vm_page_lock_queues();
			vm_page_set_validclean(m, (i * bsize) & PAGE_MASK, bsize);
			vm_page_unlock_queues();
		} else {
			vm_page_lock_queues();
			vm_page_set_validclean(m, (i * bsize) & PAGE_MASK, bsize);
			vm_page_unlock_queues();
			bzero((caddr_t) kva + i * bsize, bsize);
		}
	}
	vm_pager_unmap_page(kva);
	vm_page_lock_queues();
	pmap_clear_modify(m);
	vm_page_flag_clear(m, PG_ZERO);
	vm_page_unlock_queues();
	if (error) {
		return VM_PAGER_ERROR;
	}
	return VM_PAGER_OK;

}


/*
 * old style vnode pager output routine
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
	vm_offset_t kva;
	struct vnode *vp;

	GIANT_REQUIRED;
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

		/*
		 * Allocate a kernel virtual address and initialize so that
		 * we can use VOP_READ/WRITE routines.
		 */
		kva = vm_pager_map_page(m);

		vp = object->handle;
		aiov.iov_base = (caddr_t) kva;
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
				bzero((caddr_t) kva + count, PAGE_SIZE - count);
		}
		vm_pager_unmap_page(kva);
	}
	vm_page_lock_queues();
	pmap_clear_modify(m);
	vm_page_undirty(m);
	vm_page_flag_clear(m, PG_ZERO);
	if (!error)
		m->valid = VM_PAGE_BITS_ALL;
	vm_page_unlock_queues();
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
	VM_OBJECT_UNLOCK(object);
	rtval = VOP_GETPAGES(vp, m, bytes, reqpage, 0);
	KASSERT(rtval != EOPNOTSUPP,
	    ("vnode_pager: FS getpages not implemented\n"));
	VM_OBJECT_LOCK(object);
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
	int i, j, size, bsize, first, firstaddr;
	struct vnode *dp;
	int runpg;
	int runend;
	struct buf *bp;
	int count;
	int error = 0;

	GIANT_REQUIRED;
	object = vp->v_object;
	count = bytecount / PAGE_SIZE;

	if (vp->v_mount == NULL)
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
	if (VOP_BMAP(vp, 0, &dp, 0, NULL, NULL)) {
		VM_OBJECT_LOCK(object);
		vm_page_lock_queues();
		for (i = 0; i < count; i++)
			if (i != reqpage)
				vm_page_free(m[i]);
		vm_page_unlock_queues();
		VM_OBJECT_UNLOCK(object);
		cnt.v_vnodein++;
		cnt.v_vnodepgsin++;
		return vnode_pager_input_old(object, m[reqpage]);

		/*
		 * if the blocksize is smaller than a page size, then use
		 * special small filesystem code.  NFS sometimes has a small
		 * blocksize, but it can handle large reads itself.
		 */
	} else if ((PAGE_SIZE / bsize) > 1 &&
	    (vp->v_mount->mnt_stat.f_type != nfs_mount_type)) {
		VM_OBJECT_LOCK(object);
		vm_page_lock_queues();
		for (i = 0; i < count; i++)
			if (i != reqpage)
				vm_page_free(m[i]);
		vm_page_unlock_queues();
		VM_OBJECT_UNLOCK(object);
		cnt.v_vnodein++;
		cnt.v_vnodepgsin++;
		return vnode_pager_input_smlfs(object, m[reqpage]);
	}

	/*
	 * If we have a completely valid page available to us, we can
	 * clean up and return.  Otherwise we have to re-read the
	 * media.
	 */
	if (m[reqpage]->valid == VM_PAGE_BITS_ALL) {
		VM_OBJECT_LOCK(object);
		vm_page_lock_queues();
		for (i = 0; i < count; i++)
			if (i != reqpage)
				vm_page_free(m[i]);
		vm_page_unlock_queues();
		VM_OBJECT_UNLOCK(object);
		return VM_PAGER_OK;
	}
	m[reqpage]->valid = 0;

	/*
	 * here on direct device I/O
	 */
	firstaddr = -1;

	/*
	 * calculate the run that includes the required page
	 */
	for (first = 0, i = 0; i < count; i = runend) {
		firstaddr = vnode_pager_addr(vp,
			IDX_TO_OFF(m[i]->pindex), &runpg);
		if (firstaddr == -1) {
			VM_OBJECT_LOCK(object);
			if (i == reqpage && foff < object->un_pager.vnp.vnp_size) {
				panic("vnode_pager_getpages: unexpected missing page: firstaddr: %d, foff: 0x%jx%08jx, vnp_size: 0x%jx%08jx",
				    firstaddr, (uintmax_t)(foff >> 32),
				    (uintmax_t)foff,
				    (uintmax_t)
				    (object->un_pager.vnp.vnp_size >> 32),
				    (uintmax_t)object->un_pager.vnp.vnp_size);
			}
			vm_page_lock_queues();
			vm_page_free(m[i]);
			vm_page_unlock_queues();
			VM_OBJECT_UNLOCK(object);
			runend = i + 1;
			first = runend;
			continue;
		}
		runend = i + runpg;
		if (runend <= reqpage) {
			VM_OBJECT_LOCK(object);
			vm_page_lock_queues();
			for (j = i; j < runend; j++)
				vm_page_free(m[j]);
			vm_page_unlock_queues();
			VM_OBJECT_UNLOCK(object);
		} else {
			if (runpg < (count - first)) {
				VM_OBJECT_LOCK(object);
				vm_page_lock_queues();
				for (i = first + runpg; i < count; i++)
					vm_page_free(m[i]);
				vm_page_unlock_queues();
				VM_OBJECT_UNLOCK(object);
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
		for (i = first; i < count; i++) {
			m[i - first] = m[i];
		}
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
	if ((foff + size) > object->un_pager.vnp.vnp_size)
		size = object->un_pager.vnp.vnp_size - foff;

	/*
	 * round up physical size for real devices.
	 */
	if (dp->v_type == VBLK || dp->v_type == VCHR) {
		int secmask = dp->v_rdev->si_bsize_phys - 1;
		KASSERT(secmask < PAGE_SIZE, ("vnode_pager_generic_getpages: sector size %d too large\n", secmask + 1));
		size = (size + secmask) & ~secmask;
	}

	bp = getpbuf(&vnode_pbuf_freecnt);
	kva = (vm_offset_t) bp->b_data;

	/*
	 * and map the pages to be read into the kva
	 */
	pmap_qenter(kva, m, count);

	/* build a minimal buffer header */
	bp->b_iocmd = BIO_READ;
	bp->b_iodone = bdone;
	KASSERT(bp->b_rcred == NOCRED, ("leaking read ucred"));
	KASSERT(bp->b_wcred == NOCRED, ("leaking write ucred"));
	bp->b_rcred = crhold(curthread->td_ucred);
	bp->b_wcred = crhold(curthread->td_ucred);
	bp->b_blkno = firstaddr;
	pbgetvp(dp, bp);
	bp->b_bcount = size;
	bp->b_bufsize = size;
	bp->b_runningbufspace = bp->b_bufsize;
	runningbufspace += bp->b_runningbufspace;

	cnt.v_vnodein++;
	cnt.v_vnodepgsin += count;

	/* do the input */
	bp->b_iooffset = dbtob(bp->b_blkno);
	if (dp->v_type == VCHR)
		VOP_SPECSTRATEGY(bp->b_vp, bp);
	else
		VOP_STRATEGY(bp->b_vp, bp);

	bwait(bp, PVM, "vnread");

	if ((bp->b_ioflags & BIO_ERROR) != 0)
		error = EIO;

	if (!error) {
		if (size != count * PAGE_SIZE)
			bzero((caddr_t) kva + size, PAGE_SIZE * count - size);
	}
	pmap_qremove(kva, count);

	/*
	 * free the buffer header back to the swap buffer pool
	 */
	relpbuf(bp, &vnode_pbuf_freecnt);

	VM_OBJECT_LOCK(object);
	vm_page_lock_queues();
	for (i = 0, tfoff = foff; i < count; i++, tfoff = nextoff) {
		vm_page_t mt;

		nextoff = tfoff + PAGE_SIZE;
		mt = m[i];

		if (nextoff <= object->un_pager.vnp.vnp_size) {
			/*
			 * Read filled up entire page.
			 */
			mt->valid = VM_PAGE_BITS_ALL;
			vm_page_undirty(mt);	/* should be an assert? XXX */
			pmap_clear_modify(mt);
		} else {
			/*
			 * Read did not fill up entire page.  Since this
			 * is getpages, the page may be mapped, so we have
			 * to zero the invalid portions of the page even
			 * though we aren't setting them valid.
			 *
			 * Currently we do not set the entire page valid,
			 * we just try to clear the piece that we couldn't
			 * read.
			 */
			vm_page_set_validclean(mt, 0,
			    object->un_pager.vnp.vnp_size - tfoff);
			/* handled by vm_fault now */
			/* vm_page_zero_invalid(mt, FALSE); */
		}
		
		vm_page_flag_clear(mt, PG_ZERO);
		if (i != reqpage) {

			/*
			 * whether or not to leave the page activated is up in
			 * the air, but we should put the page on a page queue
			 * somewhere. (it already is in the object). Result:
			 * It appears that empirical results show that
			 * deactivating pages is best.
			 */

			/*
			 * just in case someone was asking for this page we
			 * now tell them that it is ok to use
			 */
			if (!error) {
				if (mt->flags & PG_WANTED)
					vm_page_activate(mt);
				else
					vm_page_deactivate(mt);
				vm_page_wakeup(mt);
			} else {
				vm_page_free(mt);
			}
		}
	}
	vm_page_unlock_queues();
	VM_OBJECT_UNLOCK(object);
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
	struct mount *mp;
	int bytes = count * PAGE_SIZE;

	GIANT_REQUIRED;
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
	if (vp->v_type != VREG)
		mp = NULL;
	(void)vn_start_write(vp, &mp, V_WAIT);
	rtval = VOP_PUTPAGES(vp, m, bytes, sync, rtvals, 0);
	KASSERT(rtval != EOPNOTSUPP, 
	    ("vnode_pager: stale FS putpages\n"));
	vn_finished_write(mp);
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
vnode_pager_generic_putpages(vp, m, bytecount, flags, rtvals)
	struct vnode *vp;
	vm_page_t *m;
	int bytecount;
	int flags;
	int *rtvals;
{
	int i;
	vm_object_t object;
	int count;

	int maxsize, ncount;
	vm_ooffset_t poffset;
	struct uio auio;
	struct iovec aiov;
	int error;
	int ioflags;

	GIANT_REQUIRED;
	object = vp->v_object;
	count = bytecount / PAGE_SIZE;

	for (i = 0; i < count; i++)
		rtvals[i] = VM_PAGER_AGAIN;

	if ((int) m[0]->pindex < 0) {
		printf("vnode_pager_putpages: attempt to write meta-data!!! -- 0x%lx(%lx)\n",
			(long)m[0]->pindex, (u_long)m[0]->dirty);
		rtvals[0] = VM_PAGER_BAD;
		return VM_PAGER_BAD;
	}

	maxsize = count * PAGE_SIZE;
	ncount = count;

	poffset = IDX_TO_OFF(m[0]->pindex);

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
	if (maxsize + poffset > object->un_pager.vnp.vnp_size) {
		if (object->un_pager.vnp.vnp_size > poffset) {
			int pgoff;

			maxsize = object->un_pager.vnp.vnp_size - poffset;
			ncount = btoc(maxsize);
			if ((pgoff = (int)maxsize & PAGE_MASK) != 0) {
				vm_page_lock_queues();
				vm_page_clear_dirty(m[ncount - 1], pgoff,
					PAGE_SIZE - pgoff);
				vm_page_unlock_queues();
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

	/*
	 * pageouts are already clustered, use IO_ASYNC t o force a bawrite()
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
	cnt.v_vnodeout++;
	cnt.v_vnodepgsout += ncount;

	if (error) {
		printf("vnode_pager_putpages: I/O error %d\n", error);
	}
	if (auio.uio_resid) {
		printf("vnode_pager_putpages: residual I/O %d at %lu\n",
		    auio.uio_resid, (u_long)m[0]->pindex);
	}
	for (i = 0; i < ncount; i++) {
		rtvals[i] = VM_PAGER_OK;
	}
	return rtvals[0];
}

struct vnode *
vnode_pager_lock(vm_object_t first_object)
{
	struct vnode *vp;
	vm_object_t backing_object, object;

	VM_OBJECT_LOCK_ASSERT(first_object, MA_OWNED);
	for (object = first_object; object != NULL; object = backing_object) {
		if (object->type != OBJT_VNODE) {
			if ((backing_object = object->backing_object) != NULL)
				VM_OBJECT_LOCK(backing_object);
			if (object != first_object)
				VM_OBJECT_UNLOCK(object);
			continue;
		}
	retry:
		if (object->flags & OBJ_DEAD) {
			if (object != first_object)
				VM_OBJECT_UNLOCK(object);
			return NULL;
		}
		vp = object->handle;
		VI_LOCK(vp);
		VM_OBJECT_UNLOCK(object);
		if (first_object != object)
			VM_OBJECT_UNLOCK(first_object);
		if (vget(vp, LK_CANRECURSE | LK_INTERLOCK | LK_NOPAUSE |
		    LK_RETRY | LK_SHARED, curthread)) {
			VM_OBJECT_LOCK(first_object);
			if (object != first_object)
				VM_OBJECT_LOCK(object);
			if (object->type != OBJT_VNODE) {
				if (object != first_object)
					VM_OBJECT_UNLOCK(object);
				return NULL;
			}
			printf("vnode_pager_lock: retrying\n");
			goto retry;
		}
		VM_OBJECT_LOCK(first_object);
		return (vp);
	}
	return NULL;
}
