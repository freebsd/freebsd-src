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
 *	$Id: vnode_pager.c,v 1.51 1995/10/23 02:23:29 dyson Exp $
 */

/*
 * Page to/from files (vnodes).
 */

/*
 * TODO:
 *	Implement VOP_GETPAGES/PUTPAGES interface for filesystems. Will
 *	greatly re-simplify the vnode_pager.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/mount.h>
#include <sys/buf.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>

extern vm_offset_t vnode_pager_addr __P((struct vnode *vp, vm_offset_t address,
					 int *run));
extern void vnode_pager_iodone __P((struct buf *bp));
extern int vnode_pager_input_smlfs __P((vm_object_t object, vm_page_t m));
extern int vnode_pager_input_old __P((vm_object_t object, vm_page_t m));

struct pagerops vnodepagerops = {
	NULL,
	vnode_pager_alloc,
	vnode_pager_dealloc,
	vnode_pager_getpages,
	vnode_pager_putpages,
	vnode_pager_haspage,
	NULL
};

static int vnode_pager_leaf_getpages __P((vm_object_t object, vm_page_t *m,
					  int count, int reqpage));
static int vnode_pager_leaf_putpages __P((vm_object_t object, vm_page_t *m,
					  int count, boolean_t sync,
					  int *rtvals));

/*
 * Allocate (or lookup) pager for a vnode.
 * Handle is a vnode pointer.
 */
vm_object_t
vnode_pager_alloc(handle, size, prot, offset)
	void *handle;
	vm_size_t size;
	vm_prot_t prot;
	vm_offset_t offset;
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
	 * Prevent race condition when allocating the object. This
	 * can happen with NFS vnodes since the nfsnode isn't locked.
	 */
	while (vp->v_flag & VOLOCK) {
		vp->v_flag |= VOWANT;
		tsleep(vp, PVM, "vnpobj", 0);
	}
	vp->v_flag |= VOLOCK;

	/*
	 * If the object is being terminated, wait for it to
	 * go away.
	 */
	while (((object = vp->v_object) != NULL) && (object->flags & OBJ_DEAD)) {
		tsleep(object, PVM, "vadead", 0);
	}

	if (object == NULL) {
		/*
		 * And an object of the appropriate size
		 */
		object = vm_object_allocate(OBJT_VNODE, round_page(size));
		object->flags = OBJ_CANPERSIST;

		/*
		 * Hold a reference to the vnode and initialize object data.
		 */
		VREF(vp);
		object->un_pager.vnp.vnp_size = size;

		object->handle = handle;
		vp->v_object = object;
	} else {
		/*
		 * vm_object_reference() will remove the object from the cache if
		 * found and gain a reference to the object.
		 */
		vm_object_reference(object);
	}

	if (vp->v_type == VREG)
		vp->v_flag |= VVMIO;

	vp->v_flag &= ~VOLOCK;
	if (vp->v_flag & VOWANT) {
		vp->v_flag &= ~VOWANT;
		wakeup(vp);
	}
	return (object);
}

void
vnode_pager_dealloc(object)
	vm_object_t object;
{
	register struct vnode *vp = object->handle;

	if (vp == NULL)
		panic("vnode_pager_dealloc: pager already dealloced");

	if (object->paging_in_progress) {
		int s = splbio();
		while (object->paging_in_progress) {
			object->flags |= OBJ_PIPWNT;
			tsleep(object, PVM, "vnpdea", 0);
		}
		splx(s);
	}

	object->handle = NULL;

	vp->v_object = NULL;
	vp->v_flag &= ~(VTEXT | VVMIO);
	vp->v_flag |= VAGE;
	vrele(vp);
}

boolean_t
vnode_pager_haspage(object, offset, before, after)
	vm_object_t object;
	vm_offset_t offset;
	int *before;
	int *after;
{
	struct vnode *vp = object->handle;
	daddr_t bn;
	int err, run;
	daddr_t reqblock;
	int poff;
	int bsize;
	int pagesperblock;

	/*
	 * If filesystem no longer mounted or offset beyond end of file we do
	 * not have the page.
	 */
	if ((vp->v_mount == NULL) || (offset >= object->un_pager.vnp.vnp_size))
		return FALSE;

	bsize = vp->v_mount->mnt_stat.f_iosize;
	pagesperblock = bsize / PAGE_SIZE;
	reqblock = offset / bsize;
	err = VOP_BMAP(vp, reqblock, (struct vnode **) 0, &bn,
		after, before);
	if (err)
		return TRUE;
	if ( bn == -1)
		return FALSE;
	poff = (offset - (reqblock * bsize)) / PAGE_SIZE;
	if (before) {
		*before *= pagesperblock;
		*before += poff;
	}
	if (after) {
		int numafter;
		*after *= pagesperblock;
		numafter = pagesperblock - (poff + 1);
		if (offset + numafter * PAGE_SIZE > object->un_pager.vnp.vnp_size) {
			numafter = (object->un_pager.vnp.vnp_size - offset)/PAGE_SIZE;
		}
		*after += numafter;
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
	u_long nsize;
{
	vm_object_t object = vp->v_object;

	if (object == NULL)
		return;

	/*
	 * Hasn't changed size
	 */
	if (nsize == object->un_pager.vnp.vnp_size)
		return;

	/*
	 * File has shrunk. Toss any cached pages beyond the new EOF.
	 */
	if (nsize < object->un_pager.vnp.vnp_size) {
		if (round_page((vm_offset_t) nsize) < object->un_pager.vnp.vnp_size) {
			vm_object_page_remove(object,
			    round_page((vm_offset_t) nsize), object->un_pager.vnp.vnp_size, FALSE);
		}
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
	}
	object->un_pager.vnp.vnp_size = (vm_offset_t) nsize;
	object->size = round_page(nsize);
}

void
vnode_pager_umount(mp)
	register struct mount *mp;
{
	struct vnode *vp, *nvp;

loop:
	for (vp = mp->mnt_vnodelist.lh_first; vp != NULL; vp = nvp) {
		/*
		 * Vnode can be reclaimed by getnewvnode() while we
		 * traverse the list.
		 */
		if (vp->v_mount != mp)
			goto loop;

		/*
		 * Save the next pointer now since uncaching may terminate the
		 * object and render vnode invalid
		 */
		nvp = vp->v_mntvnodes.le_next;

		if (vp->v_object != NULL) {
			VOP_LOCK(vp);
			vnode_pager_uncache(vp);
			VOP_UNLOCK(vp);
		}
	}
}

/*
 * Remove vnode associated object from the object cache.
 * This routine must be called with the vnode locked.
 *
 * XXX unlock the vnode.
 * We must do this since uncaching the object may result in its
 * destruction which may initiate paging activity which may necessitate
 * re-locking the vnode.
 */
void
vnode_pager_uncache(vp)
	struct vnode *vp;
{
	vm_object_t object;

	/*
	 * Not a mapped vnode
	 */
	object = vp->v_object;
	if (object == NULL)
		return;

	vm_object_reference(object);
	VOP_UNLOCK(vp);
	pager_cache(object, FALSE);
	VOP_LOCK(vp);
	return;
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
vnode_pager_addr(vp, address, run)
	struct vnode *vp;
	vm_offset_t address;
	int *run;
{
	int rtaddress;
	int bsize;
	vm_offset_t block;
	struct vnode *rtvp;
	int err;
	int vblock, voffset;

	if ((int) address < 0)
		return -1;

	if (vp->v_mount == NULL)
		return -1;

	bsize = vp->v_mount->mnt_stat.f_iosize;
	vblock = address / bsize;
	voffset = address % bsize;

	err = VOP_BMAP(vp, vblock, &rtvp, &block, run, NULL);

	if (err || (block == -1))
		rtaddress = -1;
	else {
		rtaddress = block + voffset / DEV_BSIZE;
		if( run) {
			*run += 1;
			*run *= bsize/PAGE_SIZE;
			*run -= voffset/PAGE_SIZE;
		}
	}

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
	wakeup(bp);
}

/*
 * small block file system vnode pager input
 */
int
vnode_pager_input_smlfs(object, m)
	vm_object_t object;
	vm_page_t m;
{
	int i;
	int s;
	struct vnode *dp, *vp;
	struct buf *bp;
	vm_offset_t kva;
	int fileaddr;
	vm_offset_t bsize;
	int error = 0;

	vp = object->handle;
	if (vp->v_mount == NULL)
		return VM_PAGER_BAD;

	bsize = vp->v_mount->mnt_stat.f_iosize;


	VOP_BMAP(vp, 0, &dp, 0, NULL, NULL);

	kva = vm_pager_map_page(m);

	for (i = 0; i < PAGE_SIZE / bsize; i++) {

		if ((vm_page_bits(m->offset + i * bsize, bsize) & m->valid))
			continue;

		fileaddr = vnode_pager_addr(vp, m->offset + i * bsize, (int *)0);
		if (fileaddr != -1) {
			bp = getpbuf();

			/* build a minimal buffer header */
			bp->b_flags = B_BUSY | B_READ | B_CALL;
			bp->b_iodone = vnode_pager_iodone;
			bp->b_proc = curproc;
			bp->b_rcred = bp->b_wcred = bp->b_proc->p_ucred;
			if (bp->b_rcred != NOCRED)
				crhold(bp->b_rcred);
			if (bp->b_wcred != NOCRED)
				crhold(bp->b_wcred);
			bp->b_un.b_addr = (caddr_t) kva + i * bsize;
			bp->b_blkno = fileaddr;
			pbgetvp(dp, bp);
			bp->b_bcount = bsize;
			bp->b_bufsize = bsize;

			/* do the input */
			VOP_STRATEGY(bp);

			/* we definitely need to be at splbio here */

			s = splbio();
			while ((bp->b_flags & B_DONE) == 0) {
				tsleep(bp, PVM, "vnsrd", 0);
			}
			splx(s);
			if ((bp->b_flags & B_ERROR) != 0)
				error = EIO;

			/*
			 * free the buffer header back to the swap buffer pool
			 */
			relpbuf(bp);
			if (error)
				break;

			vm_page_set_validclean(m, (i * bsize) & (PAGE_SIZE-1), bsize);
		} else {
			vm_page_set_validclean(m, (i * bsize) & (PAGE_SIZE-1), bsize);
			bzero((caddr_t) kva + i * bsize, bsize);
		}
	}
	vm_pager_unmap_page(kva);
	pmap_clear_modify(VM_PAGE_TO_PHYS(m));
	m->flags &= ~PG_ZERO;
	if (error) {
		return VM_PAGER_ERROR;
	}
	return VM_PAGER_OK;

}


/*
 * old style vnode pager output routine
 */
int
vnode_pager_input_old(object, m)
	vm_object_t object;
	vm_page_t m;
{
	struct uio auio;
	struct iovec aiov;
	int error;
	int size;
	vm_offset_t kva;

	error = 0;

	/*
	 * Return failure if beyond current EOF
	 */
	if (m->offset >= object->un_pager.vnp.vnp_size) {
		return VM_PAGER_BAD;
	} else {
		size = PAGE_SIZE;
		if (m->offset + size > object->un_pager.vnp.vnp_size)
			size = object->un_pager.vnp.vnp_size - m->offset;

		/*
		 * Allocate a kernel virtual address and initialize so that
		 * we can use VOP_READ/WRITE routines.
		 */
		kva = vm_pager_map_page(m);

		aiov.iov_base = (caddr_t) kva;
		aiov.iov_len = size;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = m->offset;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_rw = UIO_READ;
		auio.uio_resid = size;
		auio.uio_procp = (struct proc *) 0;

		error = VOP_READ(object->handle, &auio, 0, curproc->p_ucred);
		if (!error) {
			register int count = size - auio.uio_resid;

			if (count == 0)
				error = EINVAL;
			else if (count != PAGE_SIZE)
				bzero((caddr_t) kva + count, PAGE_SIZE - count);
		}
		vm_pager_unmap_page(kva);
	}
	pmap_clear_modify(VM_PAGE_TO_PHYS(m));
	m->dirty = 0;
	m->flags &= ~PG_ZERO;
	return error ? VM_PAGER_ERROR : VM_PAGER_OK;
}

/*
 * generic vnode pager input routine
 */

int
vnode_pager_getpages(object, m, count, reqpage)
	vm_object_t object;
	vm_page_t *m;
	int count;
	int reqpage;
{
	int rtval;
	struct vnode *vp;
	vp = object->handle;
	rtval = VOP_GETPAGES(vp, m, count*PAGE_SIZE, reqpage, 0);
	if (rtval == EOPNOTSUPP)
		return vnode_pager_leaf_getpages(object, m, count, reqpage);
	else
		return rtval;
}

static int
vnode_pager_leaf_getpages(object, m, count, reqpage)
	vm_object_t object;
	vm_page_t *m;
	int count;
	int reqpage;
{
	vm_offset_t kva, foff;
	int i, size, bsize, first, firstaddr;
	struct vnode *dp, *vp;
	int runpg;
	int runend;
	struct buf *bp;
	int s;
	int error = 0;

	vp = object->handle;
	if (vp->v_mount == NULL)
		return VM_PAGER_BAD;

	bsize = vp->v_mount->mnt_stat.f_iosize;

	/* get the UNDERLYING device for the file with VOP_BMAP() */

	/*
	 * originally, we did not check for an error return value -- assuming
	 * an fs always has a bmap entry point -- that assumption is wrong!!!
	 */
	foff = m[reqpage]->offset;

	/*
	 * if we can't bmap, use old VOP code
	 */
	if (VOP_BMAP(vp, 0, &dp, 0, NULL, NULL)) {
		for (i = 0; i < count; i++) {
			if (i != reqpage) {
				vnode_pager_freepage(m[i]);
			}
		}
		cnt.v_vnodein++;
		cnt.v_vnodepgsin++;
		return vnode_pager_input_old(object, m[reqpage]);

		/*
		 * if the blocksize is smaller than a page size, then use
		 * special small filesystem code.  NFS sometimes has a small
		 * blocksize, but it can handle large reads itself.
		 */
	} else if ((PAGE_SIZE / bsize) > 1 &&
	    (vp->v_mount->mnt_stat.f_type != MOUNT_NFS)) {

		for (i = 0; i < count; i++) {
			if (i != reqpage) {
				vnode_pager_freepage(m[i]);
			}
		}
		cnt.v_vnodein++;
		cnt.v_vnodepgsin++;
		return vnode_pager_input_smlfs(object, m[reqpage]);
	}
	/*
	 * if ANY DEV_BSIZE blocks are valid on a large filesystem block
	 * then, the entire page is valid --
	 */
	if (m[reqpage]->valid) {
		m[reqpage]->valid = VM_PAGE_BITS_ALL;
		for (i = 0; i < count; i++) {
			if (i != reqpage)
				vnode_pager_freepage(m[i]);
		}
		return VM_PAGER_OK;
	}

	/*
	 * here on direct device I/O
	 */

	firstaddr = -1;
	/*
	 * calculate the run that includes the required page
	 */
	for(first = 0, i = 0; i < count; i = runend) {
		firstaddr = vnode_pager_addr(vp, m[i]->offset, &runpg);
		if (firstaddr == -1) {
			if (i == reqpage && foff < object->un_pager.vnp.vnp_size) {
				panic("vnode_pager_putpages: unexpected missing page: firstaddr: %d, foff: %ld, vnp_size: %d",
			   	 firstaddr, foff, object->un_pager.vnp.vnp_size);
			}
			vnode_pager_freepage(m[i]);
			runend = i + 1;
			first = runend;
			continue;
		}
		runend = i + runpg;
		if (runend <= reqpage) {
			int j;
			for (j = i; j < runend; j++) {
				vnode_pager_freepage(m[j]);
			}
		} else {
			if (runpg < (count - first)) {
				for (i = first + runpg; i < count; i++)
					vnode_pager_freepage(m[i]);
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
	foff = m[0]->offset;

	/*
	 * calculate the size of the transfer
	 */
	size = count * PAGE_SIZE;
	if ((foff + size) > object->un_pager.vnp.vnp_size)
		size = object->un_pager.vnp.vnp_size - foff;

	/*
	 * round up physical size for real devices
	 */
	if (dp->v_type == VBLK || dp->v_type == VCHR)
		size = (size + DEV_BSIZE - 1) & ~(DEV_BSIZE - 1);

	bp = getpbuf();
	kva = (vm_offset_t) bp->b_data;

	/*
	 * and map the pages to be read into the kva
	 */
	pmap_qenter(kva, m, count);

	/* build a minimal buffer header */
	bp->b_flags = B_BUSY | B_READ | B_CALL;
	bp->b_iodone = vnode_pager_iodone;
	/* B_PHYS is not set, but it is nice to fill this in */
	bp->b_proc = curproc;
	bp->b_rcred = bp->b_wcred = bp->b_proc->p_ucred;
	if (bp->b_rcred != NOCRED)
		crhold(bp->b_rcred);
	if (bp->b_wcred != NOCRED)
		crhold(bp->b_wcred);
	bp->b_blkno = firstaddr;
	pbgetvp(dp, bp);
	bp->b_bcount = size;
	bp->b_bufsize = size;

	cnt.v_vnodein++;
	cnt.v_vnodepgsin += count;

	/* do the input */
	VOP_STRATEGY(bp);

	s = splbio();
	/* we definitely need to be at splbio here */

	while ((bp->b_flags & B_DONE) == 0) {
		tsleep(bp, PVM, "vnread", 0);
	}
	splx(s);
	if ((bp->b_flags & B_ERROR) != 0)
		error = EIO;

	if (!error) {
		if (size != count * PAGE_SIZE)
			bzero((caddr_t) kva + size, PAGE_SIZE * count - size);
	}
	pmap_qremove(kva, count);

	/*
	 * free the buffer header back to the swap buffer pool
	 */
	relpbuf(bp);

	for (i = 0; i < count; i++) {
		pmap_clear_modify(VM_PAGE_TO_PHYS(m[i]));
		m[i]->dirty = 0;
		m[i]->valid = VM_PAGE_BITS_ALL;
		m[i]->flags &= ~PG_ZERO;
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
			} else {
				vnode_pager_freepage(m[i]);
			}
		}
	}
	if (error) {
		printf("vnode_pager_getpages: I/O read error\n");
	}
	return (error ? VM_PAGER_ERROR : VM_PAGER_OK);
}

int
vnode_pager_putpages(object, m, count, sync, rtvals)
	vm_object_t object;
	vm_page_t *m;
	int count;
	boolean_t sync;
	int *rtvals;
{
	int rtval;
	struct vnode *vp;
	vp = object->handle;
	rtval = VOP_PUTPAGES(vp, m, count*PAGE_SIZE, sync, rtvals, 0);
	if (rtval == EOPNOTSUPP)
		return vnode_pager_leaf_putpages(object, m, count, sync, rtvals);
	else
		return rtval;
}

/*
 * generic vnode pager output routine
 */
static int
vnode_pager_leaf_putpages(object, m, count, sync, rtvals)
	vm_object_t object;
	vm_page_t *m;
	int count;
	boolean_t sync;
	int *rtvals;
{
	int i;

	struct vnode *vp;
	int maxsize, ncount;
	struct uio auio;
	struct iovec aiov;
	int error;

	vp = object->handle;;
	for (i = 0; i < count; i++)
		rtvals[i] = VM_PAGER_AGAIN;

	if ((int) m[0]->offset < 0) {
		printf("vnode_pager_putpages: attempt to write meta-data!!! -- 0x%x(%x)\n", m[0]->offset, m[0]->dirty);
		rtvals[0] = VM_PAGER_BAD;
		return VM_PAGER_BAD;
	}

	maxsize = count * PAGE_SIZE;
	ncount = count;

	if (maxsize + m[0]->offset > object->un_pager.vnp.vnp_size) {
		if (object->un_pager.vnp.vnp_size > m[0]->offset)
			maxsize = object->un_pager.vnp.vnp_size - m[0]->offset;
		else
			maxsize = 0;
		ncount = (maxsize + PAGE_SIZE - 1) / PAGE_SIZE;
		if (ncount < count) {
			for (i = ncount; i < count; i++) {
				rtvals[i] = VM_PAGER_BAD;
			}
			if (ncount == 0) {
				printf("vnode_pager_putpages: write past end of file: %d, %d\n",
					m[0]->offset, object->un_pager.vnp.vnp_size);
				return rtvals[0];
			}
		}
	}

	for (i = 0; i < count; i++) {
		m[i]->busy++;
		m[i]->flags &= ~PG_BUSY;
	}

	aiov.iov_base = (caddr_t) 0;
	aiov.iov_len = maxsize;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = m[0]->offset;
	auio.uio_segflg = UIO_NOCOPY;
	auio.uio_rw = UIO_WRITE;
	auio.uio_resid = maxsize;
	auio.uio_procp = (struct proc *) 0;
	error = VOP_WRITE(vp, &auio, IO_VMIO, curproc->p_ucred);
	cnt.v_vnodeout++;
	cnt.v_vnodepgsout += ncount;

	if (error) {
		printf("vnode_pager_putpages: I/O error %d\n", error);
	}
	if (auio.uio_resid) {
		printf("vnode_pager_putpages: residual I/O %d at %d\n", auio.uio_resid, m[0]->offset);
	}
	for (i = 0; i < count; i++) {
		m[i]->busy--;
		if (i < ncount) {
			rtvals[i] = VM_PAGER_OK;
		}
		if ((m[i]->busy == 0) && (m[i]->flags & PG_WANTED))
			wakeup(m[i]);
	}
	return rtvals[0];
}

struct vnode *
vnode_pager_lock(object)
	vm_object_t object;
{
	for (; object != NULL; object = object->backing_object) {
		if (object->type != OBJT_VNODE)
			continue;

		VOP_LOCK(object->handle);
		return object->handle;
	}
	return NULL;
}
