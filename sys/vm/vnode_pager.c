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
 *	$Id: vnode_pager.c,v 1.30 1995/03/16 18:17:34 bde Exp $
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/vnode.h>
#include <sys/uio.h>
#include <sys/mount.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vnode_pager.h>

#include <sys/buf.h>
#include <miscfs/specfs/specdev.h>

int vnode_pager_putmulti();

void vnode_pager_init();
void vnode_pager_dealloc();
int vnode_pager_getpage();
int vnode_pager_getmulti();
int vnode_pager_putpage();
boolean_t vnode_pager_haspage();

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

extern vm_map_t pager_map;

struct pagerlst vnode_pager_list;	/* list of managed vnodes */

#define MAXBP (PAGE_SIZE/DEV_BSIZE);

void
vnode_pager_init()
{
	TAILQ_INIT(&vnode_pager_list);
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
	vm_object_t object, tobject;
	struct vattr vattr;
	struct vnode *vp;
	struct proc *p = curproc;	/* XXX */
	int rtval;

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
	while ((object = (vm_object_t) vp->v_vmdata) && (object->flags & OBJ_DEAD))
		tsleep((caddr_t) object, PVM, "vadead", 0);

	pager = NULL;
	if (object != NULL)
		pager = object->pager;
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
		if ((rtval = VOP_GETATTR(vp, &vattr, p->p_ucred, p)) == 0) {
			object = vm_object_allocate(round_page(vattr.va_size));
			object->flags = OBJ_CANPERSIST;
			vm_object_enter(object, pager);
			object->pager = pager;
		} else {
			printf("Error in getattr: %d\n", rtval);
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

		TAILQ_INSERT_TAIL(&vnode_pager_list, pager, pg_list);
		pager->pg_handle = handle;
		pager->pg_type = PG_VNODE;
		pager->pg_ops = &vnodepagerops;
		pager->pg_data = (caddr_t) vnp;
		vp->v_vmdata = (caddr_t) object;
	} else {

		/*
		 * vm_object_lookup() will remove the object from the cache if
		 * found and also gain a reference to the object.
		 */
		(void) vm_object_lookup(pager);
	}
	return (pager);
}

void
vnode_pager_dealloc(pager)
	vm_pager_t pager;
{
	register vn_pager_t vnp = (vn_pager_t) pager->pg_data;
	register struct vnode *vp;
	vm_object_t object;

	vp = vnp->vnp_vp;
	if (vp) {
		int s = splbio();

		object = (vm_object_t) vp->v_vmdata;
		if (object) {
			while (object->paging_in_progress) {
				object->flags |= OBJ_PIPWNT;
				tsleep(object, PVM, "vnpdea", 0);
			}
		}
		splx(s);

		vp->v_vmdata = NULL;
		vp->v_flag &= ~(VTEXT | VVMIO);
		vp->v_flag |= VAGE;
		vrele(vp);
	}
	TAILQ_REMOVE(&vnode_pager_list, pager, pg_list);
	free((caddr_t) vnp, M_VMPGDATA);
	free((caddr_t) pager, M_VMPAGER);
}

int
vnode_pager_getmulti(pager, m, count, reqpage, sync)
	vm_pager_t pager;
	vm_page_t *m;
	int count;
	int reqpage;
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
	vm_page_t marray[1];
	int rtvals[1];

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
	int c;
	boolean_t sync;
	int *rtvals;
{
	return vnode_pager_output((vn_pager_t) pager->pg_data, m, c, rtvals);
}


boolean_t
vnode_pager_haspage(pager, offset)
	vm_pager_t pager;
	vm_offset_t offset;
{
	register vn_pager_t vnp = (vn_pager_t) pager->pg_data;
	register struct vnode *vp = vnp->vnp_vp;
	daddr_t bn;
	int err;
	daddr_t block;

	/*
	 * If filesystem no longer mounted or offset beyond end of file we do
	 * not have the page.
	 */
	if ((vp->v_mount == NULL) || (offset >= vnp->vnp_size))
		return FALSE;

	block = offset / vp->v_mount->mnt_stat.f_iosize;
	if (incore(vp, block))
		return TRUE;
	/*
	 * Read the index to find the disk block to read from.  If there is no
	 * block, report that we don't have this data.
	 * 
	 * Assumes that the vnode has whole page or nothing.
	 */
	err = VOP_BMAP(vp, block, (struct vnode **) 0, &bn, 0);
	if (err)
		return (TRUE);
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
	object = (vm_object_t) vp->v_vmdata;
	if (object == NULL)
		return;
	if ((pager = object->pager) == NULL)
		return;
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
		if (round_page((vm_offset_t) nsize) < vnp->vnp_size) {
			vm_object_lock(object);
			vm_object_page_remove(object,
			    round_page((vm_offset_t) nsize), vnp->vnp_size);
			vm_object_unlock(object);
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

	for (pager = vnode_pager_list.tqh_first; pager != NULL; pager = npager) {
		/*
		 * Save the next pointer now since uncaching may terminate the
		 * object and render pager invalid
		 */
		npager = pager->pg_list.tqe_next;
		vp = ((vn_pager_t) pager->pg_data)->vnp_vp;
		if (mp == (struct mount *) 0 || vp->v_mount == mp) {
			VOP_LOCK(vp);
			(void) vnode_pager_uncache(vp);
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
boolean_t
vnode_pager_uncache(vp)
	register struct vnode *vp;
{
	register vm_object_t object;
	boolean_t uncached;
	vm_pager_t pager;

	/*
	 * Not a mapped vnode
	 */
	object = (vm_object_t) vp->v_vmdata;
	if (object == NULL)
		return (TRUE);

	pager = object->pager;
	if (pager == NULL)
		return (TRUE);

#ifdef DEBUG
	if (!VOP_ISLOCKED(vp)) {
		extern int (**nfsv2_vnodeop_p)();

		if (vp->v_op != nfsv2_vnodeop_p)
			panic("vnode_pager_uncache: vnode not locked!");
	}
#endif
	/*
	 * Must use vm_object_lookup() as it actually removes the object from
	 * the cache list.
	 */
	object = vm_object_lookup(pager);
	if (object) {
		uncached = (object->ref_count <= 1);
		VOP_UNLOCK(vp);
		pager_cache(object, FALSE);
		VOP_LOCK(vp);
	} else
		uncached = TRUE;
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

	bsize = vp->v_mount->mnt_stat.f_iosize;
	vblock = address / bsize;
	voffset = address % bsize;

	err = VOP_BMAP(vp, vblock, &rtvp, &block, run);

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
	wakeup((caddr_t) bp);
	if (bp->b_flags & B_ASYNC) {
		vm_offset_t paddr;
		vm_page_t m;
		vm_object_t obj = 0;
		int i;
		int npages;

		paddr = (vm_offset_t) bp->b_data;
		if (bp->b_bufsize != bp->b_bcount)
			bzero(bp->b_data + bp->b_bcount,
			    bp->b_bufsize - bp->b_bcount);

		npages = (bp->b_bufsize + PAGE_SIZE - 1) / PAGE_SIZE;
		for (i = 0; i < npages; i++) {
			m = PHYS_TO_VM_PAGE(pmap_kextract(paddr + i * PAGE_SIZE));
			obj = m->object;
			if (m) {
				m->dirty = 0;
				m->valid = VM_PAGE_BITS_ALL;
				if (m->flags & PG_WANTED)
					m->flags |= PG_REFERENCED;
				PAGE_WAKEUP(m);
			} else {
				panic("vnode_pager_iodone: page is gone!!!");
			}
		}
		pmap_qremove(paddr, npages);
		if (obj) {
			vm_object_pip_wakeup(obj);
		} else {
			panic("vnode_pager_iodone: object is gone???");
		}
		relpbuf(bp);
	}
}

/*
 * small block file system vnode pager input
 */
int
vnode_pager_input_smlfs(vnp, m)
	vn_pager_t vnp;
	vm_page_t m;
{
	int i;
	int s;
	struct vnode *dp, *vp;
	struct buf *bp;
	vm_offset_t kva;
	int fileaddr;
	int block;
	vm_offset_t bsize;
	int error = 0;

	vp = vnp->vnp_vp;
	bsize = vp->v_mount->mnt_stat.f_iosize;

	VOP_BMAP(vp, 0, &dp, 0, 0);

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
				tsleep((caddr_t) bp, PVM, "vnsrd", 0);
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

			vm_page_set_clean(m, i * bsize, bsize);
			vm_page_set_valid(m, i * bsize, bsize);
		} else {
			vm_page_set_clean(m, i * bsize, bsize);
			bzero((caddr_t) kva + i * bsize, bsize);
		}
nextblock:
	}
	vm_pager_unmap_page(kva);
	pmap_clear_modify(VM_PAGE_TO_PHYS(m));
	if (error) {
		return VM_PAGER_ERROR;
	}
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
	struct uio auio;
	struct iovec aiov;
	int error;
	int size;
	vm_offset_t kva;

	error = 0;

	/*
	 * Return failure if beyond current EOF
	 */
	if (m->offset >= vnp->vnp_size) {
		return VM_PAGER_BAD;
	} else {
		size = PAGE_SIZE;
		if (m->offset + size > vnp->vnp_size)
			size = vnp->vnp_size - m->offset;
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

		error = VOP_READ(vnp->vnp_vp, &auio, 0, curproc->p_ucred);
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
	return error ? VM_PAGER_ERROR : VM_PAGER_OK;
}

/*
 * generic vnode pager input routine
 */
int
vnode_pager_input(vnp, m, count, reqpage)
	register vn_pager_t vnp;
	vm_page_t *m;
	int count, reqpage;
{
	int i;
	vm_offset_t kva, foff;
	int size, sizea;
	vm_object_t object;
	struct vnode *dp, *vp;
	int bsize;

	int first, last;
	int firstaddr;
	int block, offset;
	int runpg;
	int runend;

	struct buf *bp, *bpa;
	int counta;
	int s;
	int failflag;

	int error = 0;

	object = m[reqpage]->object;	/* all vm_page_t items are in same
					 * object */

	vp = vnp->vnp_vp;
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
	if (VOP_BMAP(vp, 0, &dp, 0, 0)) {
		for (i = 0; i < count; i++) {
			if (i != reqpage) {
				vnode_pager_freepage(m[i]);
			}
		}
		cnt.v_vnodein++;
		cnt.v_vnodepgsin++;
		return vnode_pager_input_old(vnp, m[reqpage]);

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
		return vnode_pager_input_smlfs(vnp, m[reqpage]);
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
			if( i == reqpage && foff < vnp->vnp_size) {
				printf("vnode_pager_input: unexpected missing page: firstaddr: %d, foff: %d, vnp_size: %d\n",
			   	 firstaddr, foff, vnp->vnp_size);
				panic("vnode_pager_input:...");
			}
			vnode_pager_freepage(m[i]);
			runend = i + 1;
			first = runend;
			continue;
		}
		runend = i + runpg;
		if( runend <= reqpage) {
			int j;
			for(j = i; j < runend; j++) {
				vnode_pager_freepage(m[j]);
			}
		} else {
			if( runpg < (count - first)) {
				for(i=first + runpg; i < count; i++)
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
#if 0
	printf("foff: 0x%lx, firstaddr: 0x%lx\n",
		foff, firstaddr);
	DELAY(6000000);
#endif

	/*
	 * calculate the size of the transfer
	 */
	size = count * PAGE_SIZE;
	if ((foff + size) > vnp->vnp_size)
		size = vnp->vnp_size - foff;

	/*
	 * round up physical size for real devices
	 */
	if (dp->v_type == VBLK || dp->v_type == VCHR)
		size = (size + DEV_BSIZE - 1) & ~(DEV_BSIZE - 1);

	counta = 0;
	if (count * PAGE_SIZE > bsize)
		counta = (count - reqpage) - 1;
	bpa = 0;
	sizea = 0;
	bp = getpbuf();
	if (counta) {
		bpa = (struct buf *) trypbuf();
		if (bpa) {
			count -= counta;
			sizea = size - count * PAGE_SIZE;
			size = count * PAGE_SIZE;
		}
	}
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

	if (counta) {
		for (i = 0; i < counta; i++) {
			vm_page_deactivate(m[count + i]);
		}
		pmap_qenter((vm_offset_t) bpa->b_data, &m[count], counta);
		++m[count]->object->paging_in_progress;
		bpa->b_flags = B_BUSY | B_READ | B_CALL | B_ASYNC;
		bpa->b_iodone = vnode_pager_iodone;
		/* B_PHYS is not set, but it is nice to fill this in */
		bpa->b_proc = curproc;
		bpa->b_rcred = bpa->b_wcred = bpa->b_proc->p_ucred;
		if (bpa->b_rcred != NOCRED)
			crhold(bpa->b_rcred);
		if (bpa->b_wcred != NOCRED)
			crhold(bpa->b_wcred);
		bpa->b_blkno = firstaddr + count * (PAGE_SIZE / DEV_BSIZE);
		pbgetvp(dp, bpa);
		bpa->b_bcount = sizea;
		bpa->b_bufsize = counta * PAGE_SIZE;

		cnt.v_vnodepgsin += counta;
		VOP_STRATEGY(bpa);
	}
	s = splbio();
	/* we definitely need to be at splbio here */

	while ((bp->b_flags & B_DONE) == 0) {
		tsleep((caddr_t) bp, PVM, "vnread", 0);
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

finishup:
	for (i = 0; i < count; i++) {
		pmap_clear_modify(VM_PAGE_TO_PHYS(m[i]));
		m[i]->dirty = 0;
		m[i]->valid = VM_PAGE_BITS_ALL;
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
		printf("vnode_pager_input: I/O read error\n");
	}
	return (error ? VM_PAGER_ERROR : VM_PAGER_OK);
}

/*
 * old-style vnode pager output routine
 */
int
vnode_pager_output_old(vnp, m)
	register vn_pager_t vnp;
	vm_page_t m;
{
	vm_offset_t kva, kva2;
	vm_offset_t size;
	struct iovec aiov;
	struct uio auio;
	struct vnode *vp;
	int error;

	vp = vnp->vnp_vp;

	/*
	 * Dont return failure if beyond current EOF placate the VM system.
	 */
	if (m->offset >= vnp->vnp_size) {
		return VM_PAGER_OK;
	} else {
		size = PAGE_SIZE;
		if (m->offset + size > vnp->vnp_size)
			size = vnp->vnp_size - m->offset;

		kva2 = kmem_alloc(pager_map, PAGE_SIZE);
		/*
		 * Allocate a kernel virtual address and initialize so that
		 * we can use VOP_WRITE routines.
		 */
		kva = vm_pager_map_page(m);
		bcopy((caddr_t) kva, (caddr_t) kva2, size);
		vm_pager_unmap_page(kva);
		pmap_clear_modify(VM_PAGE_TO_PHYS(m));
		PAGE_WAKEUP(m);

		aiov.iov_base = (caddr_t) kva2;
		aiov.iov_len = size;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = m->offset;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_rw = UIO_WRITE;
		auio.uio_resid = size;
		auio.uio_procp = (struct proc *) 0;

		error = VOP_WRITE(vp, &auio, 0, curproc->p_ucred);

		kmem_free_wakeup(pager_map, kva2, PAGE_SIZE);
		if (!error) {
			if ((size - auio.uio_resid) == 0) {
				error = EINVAL;
			}
		}
		return error ? VM_PAGER_ERROR : VM_PAGER_OK;
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
	int i;
	int s;
	struct vnode *dp, *vp;
	struct buf *bp;
	vm_offset_t kva;
	int fileaddr;
	vm_offset_t bsize;
	int error = 0;

	vp = vnp->vnp_vp;
	bsize = vp->v_mount->mnt_stat.f_iosize;

	VOP_BMAP(vp, 0, &dp, 0, 0);
	kva = vm_pager_map_page(m);
	for (i = 0; !error && i < (PAGE_SIZE / bsize); i++) {

		if ((vm_page_bits(m->offset + i * bsize, bsize) & m->valid & m->dirty) == 0)
			continue;
		/*
		 * calculate logical block and offset
		 */
		fileaddr = vnode_pager_addr(vp, m->offset + i * bsize, (int *)0);
		if (fileaddr != -1) {

			bp = getpbuf();

			/* build a minimal buffer header */
			bp->b_flags = B_BUSY | B_CALL | B_WRITE;
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
			++dp->v_numoutput;
			/* for NFS */
			bp->b_dirtyoff = 0;
			bp->b_dirtyend = bsize;
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

			vm_page_set_clean(m, i * bsize, bsize);
			/*
			 * free the buffer header back to the swap buffer pool
			 */
			relpbuf(bp);
		}
	}
	vm_pager_unmap_page(kva);
	if (error)
		return VM_PAGER_ERROR;
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
	int count;
	int *rtvals;
{
	int i, j;
	vm_offset_t kva, foff;
	int size;
	vm_object_t object;
	struct vnode *dp, *vp;
	struct buf *bp;
	vm_offset_t reqaddr;
	int bsize;
	int s;
	daddr_t block;
	struct timeval tv;
	int runpg;

	int error = 0;

retryoutput:
	object = m[0]->object;	/* all vm_page_t items are in same object */

	vp = vnp->vnp_vp;

	/*
	 * Make sure underlying filesystem is still mounted.
	 */
	if (vp->v_mount == NULL)
		return VM_PAGER_FAIL;

	bsize = vp->v_mount->mnt_stat.f_iosize;

	for (i = 0; i < count; i++)
		rtvals[i] = VM_PAGER_AGAIN;

	if ((int) m[0]->offset < 0) {
		printf("vnode_pager_output: attempt to write meta-data!!! -- 0x%x\n", m[0]->offset);
		m[0]->dirty = 0;
		rtvals[0] = VM_PAGER_OK;
		return VM_PAGER_OK;
	}
	/*
	 * if the filesystem does not have a bmap, then use the old code
	 */
	if (VOP_BMAP(vp, (m[0]->offset / bsize), &dp, &block, 0) ||
	    (block == -1)) {

		rtvals[0] = vnode_pager_output_old(vnp, m[0]);

		m[0]->dirty = 0;
		cnt.v_vnodeout++;
		cnt.v_vnodepgsout++;
		return rtvals[0];
	}
	tv = time;
	VOP_UPDATE(vp, &tv, &tv, 0);

	/*
	 * if the filesystem has a small blocksize, then use the small block
	 * filesystem output code
	 */
	if ((bsize < PAGE_SIZE) &&
	    (vp->v_mount->mnt_stat.f_type != MOUNT_NFS)) {

		for (i = 0; i < count; i++) {
			rtvals[i] = vnode_pager_output_smlfs(vnp, m[i]);
			if (rtvals[i] == VM_PAGER_OK) {
				pmap_clear_modify(VM_PAGE_TO_PHYS(m[i]));
			}
		}
		cnt.v_vnodeout++;
		cnt.v_vnodepgsout += count;
		return rtvals[0];
	}
	for (i = 0; i < count; i++) {
		foff = m[i]->offset;
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
	foff = m[0]->offset;
	reqaddr = vnode_pager_addr(vp, foff, &runpg);
	if( runpg < count)
		count = runpg;

	/*
	 * calculate the size of the transfer
	 */
	size = count * PAGE_SIZE;
	if ((foff + size) > vnp->vnp_size)
		size = vnp->vnp_size - foff;

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
	bp->b_flags = B_BUSY | B_WRITE | B_CALL;
	bp->b_iodone = vnode_pager_iodone;
	/* B_PHYS is not set, but it is nice to fill this in */
	bp->b_proc = curproc;
	bp->b_rcred = bp->b_wcred = bp->b_proc->p_ucred;

	if (bp->b_rcred != NOCRED)
		crhold(bp->b_rcred);
	if (bp->b_wcred != NOCRED)
		crhold(bp->b_wcred);
	bp->b_blkno = reqaddr;
	pbgetvp(dp, bp);
	++dp->v_numoutput;

	/* for NFS */
	bp->b_dirtyoff = 0;
	bp->b_dirtyend = size;

	bp->b_bcount = size;
	bp->b_bufsize = size;

	cnt.v_vnodeout++;
	cnt.v_vnodepgsout += count;

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

	pmap_qremove(kva, count);

	/*
	 * free the buffer header back to the swap buffer pool
	 */
	relpbuf(bp);

	if (!error) {
		for (i = 0; i < count; i++) {
			pmap_clear_modify(VM_PAGE_TO_PHYS(m[i]));
			m[i]->dirty = 0;
			rtvals[i] = VM_PAGER_OK;
		}
	} else if (count != 1) {
		error = 0;
		count = 1;
		goto retryoutput;
	}
	if (error) {
		printf("vnode_pager_output: I/O write error\n");
	}
	return (error ? VM_PAGER_ERROR : VM_PAGER_OK);
}
