/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * John Heidemann of the UCLA Ficus project.
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
 *	$Id: ntfs_vnops.c,v 1.9 1999/02/02 01:54:55 semen Exp $
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/proc.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/dirent.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>
#include <vm/vm_extern.h>


/*#define NTFS_DEBUG 1*/
#include <ntfs/ntfs.h>
#include <ntfs/ntfs_inode.h>
#include <ntfs/ntfs_subr.h>
#include <miscfs/specfs/specdev.h>

static int	ntfs_bypass __P((struct vop_generic_args *ap));
static int	ntfs_read __P((struct vop_read_args *));
static int	ntfs_bwrite __P((struct vop_bwrite_args *ap));
static int	ntfs_getattr __P((struct vop_getattr_args *ap));
static int	ntfs_inactive __P((struct vop_inactive_args *ap));
static int	ntfs_print __P((struct vop_print_args *ap));
static int	ntfs_reclaim __P((struct vop_reclaim_args *ap));
static int	ntfs_strategy __P((struct vop_strategy_args *ap));
#if __FreeBSD_version < 300000
static int	ntfs_islocked __P((struct vop_islocked_args *ap));
static int	ntfs_unlock __P((struct vop_unlock_args *ap));
static int	ntfs_lock __P((struct vop_lock_args *ap));
#endif
static int	ntfs_access __P((struct vop_access_args *ap));
static int	ntfs_open __P((struct vop_open_args *ap));
static int	ntfs_close __P((struct vop_close_args *ap));
static int	ntfs_readdir __P((struct vop_readdir_args *ap));
static int	ntfs_lookup __P((struct vop_lookup_args *ap));
static int	ntfs_bmap __P((struct vop_bmap_args *ap));
static int	ntfs_getpages __P((struct vop_getpages_args *ap));
static int	ntfs_fsync __P((struct vop_fsync_args *ap));

int	ntfs_prtactive = 1;	/* 1 => print out reclaim of active vnodes */

/*
 * Vnode op for VM getpages.
 */
int
ntfs_getpages(ap)
	struct vop_getpages_args *ap;
{
	int i, error, nextoff, size, toff, npages, count;
	struct uio uio;
	struct iovec iov;
	vm_offset_t kva;
	struct buf *bp;
	struct vnode *vp;
	struct proc *p;
	struct ucred *cred;
	struct ntfsmount *ntmp;
	vm_page_t *pages;

	vp = ap->a_vp;
	p = curproc;				/* XXX */
	cred = curproc->p_ucred;		/* XXX */
	ntmp = VFSTONTFS(vp->v_mount);
	pages = ap->a_m;
	count = ap->a_count;

	if (vp->v_object == NULL) {
		printf("ntfs_getpages: called with non-merged cache vnode??\n");
		return VM_PAGER_ERROR;
	}

	/*
	 * We use only the kva address for the buffer, but this is extremely
	 * convienient and fast.
	 */
#if __FreeBSD_version >= 400000
	bp = getpbuf(NULL);
#else
	bp = getpbuf();
#endif

	npages = btoc(count);
	kva = (vm_offset_t) bp->b_data;
	pmap_qenter(kva, pages, npages);

	iov.iov_base = (caddr_t) kva;
	iov.iov_len = count;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = IDX_TO_OFF(pages[0]->pindex);
	uio.uio_resid = count;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_procp = p;

	error = VOP_READ(vp, &uio, 0, cred);
	pmap_qremove(kva, npages);

#if __FreeBSD_version >= 400000
	relpbuf(bp,NULL);
#else
	relpbuf(bp);
#endif

	if (error && (uio.uio_resid == count))
		return VM_PAGER_ERROR;

	size = count - uio.uio_resid;

	for (i = 0, toff = 0; i < npages; i++, toff = nextoff) {
		vm_page_t m;
		nextoff = toff + PAGE_SIZE;
		m = pages[i];

		m->flags &= ~PG_ZERO;

		if (nextoff <= size) {
			m->valid = VM_PAGE_BITS_ALL;
			m->dirty = 0;
		} else {
			int nvalid = ((size + DEV_BSIZE - 1) - toff) & ~(DEV_BSIZE - 1);
			vm_page_set_validclean(m, 0, nvalid);
		}
		
		if (i != ap->a_reqpage) {
			/*
			 * Whether or not to leave the page activated is up in
			 * the air, but we should put the page on a page queue
			 * somewhere (it already is in the object).  Result:
			 * It appears that emperical results show that
			 * deactivating pages is best.
			 */

			/*
			 * Just in case someone was asking for this page we
			 * now tell them that it is ok to use.
			 */
			if (!error) {
				if (m->flags & PG_WANTED)
					vm_page_activate(m);
				else
					vm_page_deactivate(m);
#if __FreeBSD_version >= 300000
				vm_page_wakeup(m);
#endif
			} else {
				vnode_pager_freepage(m);
			}
		}
	}
	return 0;
}

/*
 * This is a noop, simply returning what one has been given.
 */
int
ntfs_bmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
		int *a_runb;
	} */ *ap;
{
	if (ap->a_vpp != NULL)
		*ap->a_vpp = ap->a_vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn;
	if (ap->a_runp != NULL)
		*ap->a_runp = 0;
	if (ap->a_runb != NULL)
		*ap->a_runb = 0;
	return (0);
}

static int
ntfs_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct ntnode *ip = VTONT(vp);
	struct uio *uio = ap->a_uio;
	struct ntfsmount *ntmp = ip->i_mp;
	u_int8_t *data;
	u_int64_t toread;
	int error;

	dprintf(("ntfs_read: ino: %d, off: %d resid: %d, segflg: %d\n",ip->i_number,(u_int32_t)uio->uio_offset,uio->uio_resid,uio->uio_segflg));

	ntfs_filesize( ntmp, ip, &toread, NULL );
	dprintf(("ntfs_read: filesize: %d",(u_int32_t)toread));

	toread = min( uio->uio_resid, toread - uio->uio_offset );

	dprintf((", toread: %d\n",(u_int32_t)toread));

	MALLOC(data, u_int8_t *, toread, M_TEMP,M_WAITOK);

	error = ntfs_breadattr( ntmp, ip, ip->i_defattr, ip->i_defattrname, 
		uio->uio_offset, toread, data );
	if(error) {
		printf("ntfs_read: ntfs_breadattr failed: %d\n",error);
		FREE(data, M_TEMP);
		return (error);
	}

	error = uiomove(data, (int) toread, uio);
	if(error) {
		FREE(data, M_TEMP);
		return (error);
	}

	FREE(data, M_TEMP);

	return (0);
}

static int
ntfs_bypass(ap)
	struct vop_generic_args /* {
		struct vnodeop_desc *a_desc;
		<other random data follows, presumably>
	} */ *ap;
{
	int error = ENOTTY;
	dprintf (("ntfs_bypass: %s\n", ap->a_desc->vdesc_name));
	return (error);
}


static int
ntfs_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct ntnode *ip = VTONT(vp);
	register struct vattr *vap = ap->a_vap;
	int error;

	dprintf(("ntfs_getattr: %d, flags: %d\n",ip->i_number,ip->i_flag));

	if ((ip->i_flag & (IN_LOADED | IN_PRELOADED)) == 0) {
		error = ntfs_loadnode(ip->i_mp,ip);
		if (error)
			return error;
	}
	vap->va_fsid = ip->i_dev;
	vap->va_fileid = ip->i_number;
	vap->va_mode = ip->i_mode;
	vap->va_nlink = ip->i_nlink;
	vap->va_uid = ip->i_uid;
	vap->va_gid = ip->i_gid;
	vap->va_rdev = (dev_t)0;
	vap->va_size = ip->i_size;
	vap->va_bytes = ip->i_allocated;
	vap->va_atime = ntfs_nttimetounix(ip->i_times.t_access);
	vap->va_mtime = ntfs_nttimetounix(ip->i_times.t_write);
	vap->va_ctime = ntfs_nttimetounix(ip->i_times.t_create);
	vap->va_flags = ip->i_flag;
	vap->va_gen = 0;
	vap->va_blocksize = ip->i_mp->ntm_spc * ip->i_mp->ntm_bps;
	vap->va_type = ip->i_type;
	vap->va_filerev = 0;
	return (0);
}


/*
 * Last reference to an ntnode.  If necessary, write or delete it.
 */
int
ntfs_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
#if defined(NTFS_DEBUG) || defined(DISGNOSTIC)
	register struct ntnode *ip = VTONT(vp);
#endif
	int error;

	dprintf(("ntfs_inactive: %d (%d locks)\n",
		ip->i_number,ip->i_lockcount));

	if (ntfs_prtactive && vp->v_usecount != 0)
		vprint("ntfs_inactive: pushing active", vp);

	error = 0;

#if __FreeBSD_version >= 300000
	VOP_UNLOCK(vp,0,ap->a_p);
#else
#ifdef DIAGNOSTIC
	if (VOP_ISLOCKED(vp))
		panic("ntfs_inactive: locked ntnode");
	if (curproc)
		ip->i_lockholder = curproc->p_pid;
	else
		ip->i_lockholder = -1;
#endif
	ip->i_flag |= IN_LOCKED;
	VOP_UNLOCK(vp);
#endif
	/*
	 * If we are done with the ntnode, reclaim it
	 * so that it can be reused immediately.
	 */
	if (vp->v_usecount == 0 /*&& ip->i_mode == 0*/)
#if __FreeBSD_version >= 300000
		vrecycle(vp, (struct simplelock *)0, ap->a_p);
#else
		vgone(vp);
#endif
	return (error);
}

/*
 * Reclaim an inode so that it can be used for other purposes.
 */
int
ntfs_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct ntnode *ip = VTONT(vp);

	dprintf(("ntfs_reclaim: reclaim: %d\n",ip->i_number));

#if __FreeBSD_version >= 300000
	VOP_UNLOCK(vp,0,ap->a_p);
#endif

	if(ip->i_dirblbuf) {
		FREE(ip->i_dirblbuf, M_NTFSDIR);
		ip->i_dirblbuf = NULL;
	}

	/*
	 * Remove the inode from its hash chain.
	 */
	ntfs_ihashrem(ip);

	/*
	 * Purge old data structures associated with the inode.
	 */
	cache_purge(vp);
	if (ip->i_devvp) {
		vrele(ip->i_devvp);
		ip->i_devvp = 0;
	}

	ntfs_ntrele(ip);

	vp->v_data = NULL;

	return (0);
}

static int
ntfs_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
/*	printf("[ntfs_print]");*/
	
	return (0);
}

/*
 * Calculate the logical to physical mapping if not done already,
 * then call the device strategy routine.
 */
int
ntfs_strategy(ap)
	struct vop_strategy_args /* {
		struct buf *a_bp;
	} */ *ap;
{
	register struct buf *bp = ap->a_bp;
	struct ucred *cr;
	struct proc *p;
	int error = 0;

	dprintf(("strategy: data: %p, npages: %d,dirty: %d\n",bp->b_data,bp->b_npages,bp->b_dirtyend));
	if (bp->b_flags & B_PHYS)
		panic("ntfs physio");
	if (bp->b_flags & B_ASYNC)
		p = (struct proc *)0;
	else
		p = curproc;	/* XXX */
	if (bp->b_flags & B_READ)
		cr = bp->b_rcred;
	else
		cr = bp->b_wcred;
	/*
	 * If the op is asynchronous and an i/o daemon is waiting
	 * queue the request, wake it up and wait for completion
	 * otherwise just do it ourselves.
	 */
/*
	if ((bp->b_flags & B_ASYNC) == 0 ||
		nfs_asyncio(bp, NOCRED))
		error = nfs_doio(bp, cr, p);
*/

	return (ENOTTY);
	return (error);
}

static int
ntfs_bwrite(ap)
	struct vop_bwrite_args /* {
		struct buf *a_bp;
	} */ *ap;
{
	int error = ENOTTY;

	printf("ntfs_bwrite: \n");

	return (error);
}

#if __FreeBSD_version < 300000
/*
 * Check for a locked ntnode.
 */
int
ntfs_islocked(ap)
	struct vop_islocked_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct ntnode *ip = VTONT(ap->a_vp);

	dprintf(("ntfs_islocked %d (%d locks)\n",ip->i_number,ip->i_lockcount));

	if (ip->i_flag & IN_LOCKED)
		return (1);
	return (0);
}

/*
 * Unlock an ntnode.  If WANT bit is on, wakeup.
 */
int ntfs_lockcount = 90;
int
ntfs_unlock(ap)
	struct vop_unlock_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct ntnode *ip = VTONT(ap->a_vp);
	struct proc *p = curproc;

	dprintf(("ntfs_unlock %d (%d locks)\n",ip->i_number,ip->i_lockcount));

#ifdef DIAGNOSTIC

	if ((ip->i_flag & IN_LOCKED) == 0) {
		vprint("ntfs_unlock: unlocked ntnode", ap->a_vp);
		panic("ntfs_unlock NOT LOCKED");
	}
	if (p && p->p_pid != ip->i_lockholder && p->p_pid > -1 &&
	    ip->i_lockholder > -1 && ntfs_lockcount++ < 100)
		panic("unlocker (%d) != lock holder (%d)",
		    p->p_pid, ip->i_lockholder);
#endif

	if (--ip->i_lockcount > 0) {
		if ((ip->i_flag & IN_RECURSE) == 0)
			panic("ntfs_unlock: recursive lock prematurely released, pid=%d\n", ip->i_lockholder);
		return (0);
	}
	ip->i_lockholder = 0;
	ip->i_flag &= ~(IN_LOCKED|IN_RECURSE);
	if (ip->i_flag & IN_WANTED) {
		ip->i_flag &= ~IN_WANTED;
		wakeup((caddr_t)ip);
	}
	return (0);
}

/*
 * Lock an ntnode. If its already locked, set the WANT bit and sleep.
 */
int
ntfs_lock(ap)
	struct vop_lock_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct proc *p = curproc;
	register struct vnode *vp = ap->a_vp;
	register struct ntnode *ip = VTONT(vp);

	dprintf(("ntfs_lock %d (%d locks)\n",ip->i_number,ip->i_lockcount));

start:
	while (vp->v_flag & VXLOCK) {
		vp->v_flag |= VXWANT;
		(void) tsleep((caddr_t)vp, PINOD, "ntflk1", 0);
	}
	if (vp->v_tag == VT_NON)
		return (ENOENT);
	ip = VTONT(vp);
	if (ip->i_flag & IN_LOCKED) {
		if (p->p_pid == ip->i_lockholder) {
			if( (ip->i_flag & IN_RECURSE) == 0)
				panic("ntfs_lock: recursive lock not expected, pid: %d\n",
					ip->i_lockholder);
		} else {
			ip->i_flag |= IN_WANTED;
#ifdef DIAGNOSTIC
			if (p)
				ip->i_lockwaiter = p->p_pid;
			else
				ip->i_lockwaiter = -1;
#endif
			(void) tsleep((caddr_t)ip, PINOD, "ntflk2", 0);
			goto start;
		}
	}
#ifdef DIAGNOSTIC
	ip->i_lockwaiter = 0;
	if (((ip->i_flag & IN_RECURSE) == 0) && (ip->i_lockholder != 0))
		panic("lockholder (%d) != 0", ip->i_lockholder);
	if (p && p->p_pid == 0)
		printf("locking by process 0\n");
#endif

	if ((ip->i_flag & IN_RECURSE) == 0)
		ip->i_lockcount = 1;
	else
		++ip->i_lockcount;

	if (p)
		ip->i_lockholder = p->p_pid;
	else
		ip->i_lockholder = -1;
	ip->i_flag |= IN_LOCKED;
	return (0);
}
#endif

int
ntfs_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct ntnode *ip = VTONT(vp);
	struct ucred *cred = ap->a_cred;
	mode_t mask, mode = ap->a_mode;
	register gid_t *gp;
	int i;
#ifdef QUOTA
	int error;
#endif

	dprintf(("ntfs_access: %d\n",ip->i_number));

	/*
	 * Disallow write attempts on read-only file systems;
	 * unless the file is a socket, fifo, or a block or
	 * character device resident on the file system.
	 */
	if (mode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
#ifdef QUOTA
			if (error = getinoquota(ip))
				return (error);
#endif
			break;
		}
	}

	/* If immutable bit set, nobody gets to write it. */
/*
	if ((mode & VWRITE) && (ip->i_flags & IMMUTABLE))
		return (EPERM);
*/

	/* Otherwise, user id 0 always gets access. */
	if (cred->cr_uid == 0)
		return (0);

	mask = 0;

	/* Otherwise, check the owner. */
	if (cred->cr_uid == ip->i_uid) {
		if (mode & VEXEC)
			mask |= S_IXUSR;
		if (mode & VREAD)
			mask |= S_IRUSR;
		if (mode & VWRITE)
			mask |= S_IWUSR;
		return ((ip->i_mode & mask) == mask ? 0 : EACCES);
	}

	/* Otherwise, check the groups. */
	for (i = 0, gp = cred->cr_groups; i < cred->cr_ngroups; i++, gp++)
		if (ip->i_gid == *gp) {
			if (mode & VEXEC)
				mask |= S_IXGRP;
			if (mode & VREAD)
				mask |= S_IRGRP;
			if (mode & VWRITE)
				mask |= S_IWGRP;
			return ((ip->i_mode & mask) == mask ? 0 : EACCES);
		}

	/* Otherwise, check everyone else. */
	if (mode & VEXEC)
		mask |= S_IXOTH;
	if (mode & VREAD)
		mask |= S_IROTH;
	if (mode & VWRITE)
		mask |= S_IWOTH;
	return ((ip->i_mode & mask) == mask ? 0 : EACCES);
}

/*
 * Open called.
 *
 * Nothing to do.
 */
/* ARGSUSED */
static int
ntfs_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
#if NTFS_DEBUG
	register struct vnode *vp = ap->a_vp;
	register struct ntnode *ip = VTONT(vp);

	printf("ntfs_open: %d (%d locks)\n",ip->i_number,ip->i_lockcount);
#endif

	/*
	 * Files marked append-only must be opened for appending.
	 */

	return (0);
}

/*
 * Close called.
 *
 * Update the times on the inode.
 */
/* ARGSUSED */
static int
ntfs_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
#if NTFS_DEBUG
	register struct vnode *vp = ap->a_vp;
	register struct ntnode *ip = VTONT(vp);

	printf("ntfs_close: %d (%d locks)\n",ip->i_number,ip->i_lockcount);
#endif

	return (0);
}

/*
#undef dprintf
#define dprintf(a) printf a
*/
int
ntfs_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_ncookies;
		u_int **cookies;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct ntnode *ip = VTONT(vp);
	struct uio *uio = ap->a_uio;
	struct ntfsmount *ntmp = ip->i_mp;
	int i, error = 0;
	u_int32_t faked = 0, num;
	int ncookies = 0;
	struct dirent cde;
	off_t off;

	dprintf(("ntfs_readdir %d off: %d resid: %d\n",ip->i_number,(u_int32_t)uio->uio_offset,uio->uio_resid));

	off = uio->uio_offset;

	/* Simulate . in every dir except ROOT */
	if( ip->i_number != NTFS_ROOTINO ) {
		struct dirent dot = { NTFS_ROOTINO,
				sizeof(struct dirent), DT_DIR, 1, "." };

		if( uio->uio_offset < sizeof(struct dirent) ) {
			dot.d_fileno = ip->i_number;
			error = uiomove((char *)&dot,sizeof(struct dirent),uio);
			if(error)
				return (error);

			ncookies ++;
		}
	}

	/* Simulate .. in every dir including ROOT */
	if( uio->uio_offset < 2 * sizeof(struct dirent) ) {
		struct dirent dotdot = { NTFS_ROOTINO,
				sizeof(struct dirent), DT_DIR, 2, ".." };

		error = uiomove((char *)&dotdot,sizeof(struct dirent),uio);
		if(error)
			return (error);

		ncookies ++;
	}

	faked = (ip->i_number == NTFS_ROOTINO) ? 1 : 2;
	num = uio->uio_offset / sizeof(struct dirent) - faked;

	while( uio->uio_resid >= sizeof(struct dirent) ) {
		struct attr_indexentry *iep;

		error = ntfs_ntreaddir(ntmp, ip, num, &iep);

		if(error)
			return (error);

		if( NULL == iep )
			break;

		while( !(iep->ie_flag & NTFS_IEFLAG_LAST) && (uio->uio_resid >= sizeof(struct dirent)) ) {

			if( ntfs_isnamepermitted(ntmp,iep) ) {
				dprintf(("ntfs_readdir: elem: %d, fname:[",num));
				for(i=0;i<iep->ie_fnamelen;i++) {
					cde.d_name[i] = (char)iep->ie_fname[i];
					dprintf(("%c", cde.d_name[i]));
				}
				dprintf(("] type: %d, flag: %d, ",iep->ie_fnametype, iep->ie_flag));
				cde.d_name[i] = '\0';
				cde.d_namlen = iep->ie_fnamelen;
				cde.d_fileno = iep->ie_number;
				cde.d_type = (iep->ie_fflag & NTFS_FFLAG_DIR) ? DT_DIR : DT_REG;
				cde.d_reclen = sizeof(struct dirent);
				dprintf(("%s\n", (cde.d_type == DT_DIR) ? "dir":"reg"));

				error = uiomove((char *)&cde, sizeof(struct dirent), uio);
				if(error)
					return (error);

				ncookies++;
				num++;
			}

			iep = NTFS_NEXTREC(iep,struct attr_indexentry *);
		}
	}

	dprintf(("ntfs_readdir: %d entries (%d bytes) read\n",
		ncookies,(u_int)(uio->uio_offset - off)));
	dprintf(("ntfs_readdir: off: %d resid: %d\n",
		(u_int32_t)uio->uio_offset,uio->uio_resid));

	if (!error && ap->a_ncookies != NULL) {
		struct dirent* dpStart;
		struct dirent* dp;
#if __FreeBSD_version >= 300000
		u_long *cookies;
		u_long *cookiep;
#else
		u_int *cookies;
		u_int *cookiep;
#endif

		printf("ntfs_readdir: %d cookies\n",ncookies);
		if (uio->uio_segflg != UIO_SYSSPACE || uio->uio_iovcnt != 1)
			panic("ntfs_readdir: unexpected uio from NFS server");
		dpStart = (struct dirent *)
		     (uio->uio_iov->iov_base - (uio->uio_offset - off));
#if __FreeBSD_version >= 300000
		MALLOC(cookies, u_long *, ncookies * sizeof(u_long),
		       M_TEMP, M_WAITOK);
#else
		MALLOC(cookies, u_int *, ncookies * sizeof(u_int),
		       M_TEMP, M_WAITOK);
#endif
		for (dp = dpStart, cookiep = cookies, i=0;
		     i < ncookies;
		     dp = (struct dirent *)((caddr_t) dp + dp->d_reclen), i++) {
			off += dp->d_reclen;
			*cookiep++ = (u_int) off;
		}
		*ap->a_ncookies = ncookies;
		*ap->a_cookies = cookies;
	}
/*
	if (ap->a_eofflag)
	    *ap->a_eofflag = VTONT(ap->a_vp)->i_size <= uio->uio_offset;
*/
	return (error);
}
/*
#undef dprintf
#define dprintf(a) 
*/

int
ntfs_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	register struct vnode *dvp = ap->a_dvp;
	register struct ntnode *dip = VTONT(dvp);
	struct ntfsmount *ntmp = dip->i_mp;
	struct componentname *cnp = ap->a_cnp;
	struct ucred *cred = cnp->cn_cred;
	int error;
	int lockparent = cnp->cn_flags & LOCKPARENT;
#if NTFS_DEBUG
	int wantparent = cnp->cn_flags & (LOCKPARENT|WANTPARENT);
#endif
	dprintf(("ntfs_lookup: %s (%ld bytes) in %d, lp: %d, wp: %d \n",
		cnp->cn_nameptr, cnp->cn_namelen,
		dip->i_number,lockparent, wantparent));

	error = VOP_ACCESS(dvp,VEXEC, cred, cnp->cn_proc);
	if(error)
		return (error);

	if( (cnp->cn_namelen == 1) &&
	    !strncmp(cnp->cn_nameptr,".",1) ) {
		dprintf(("ntfs_lookup: faking . directory in %d\n",
			dip->i_number));

		VREF(dvp);
		*ap->a_vpp = dvp;
		return (0);
	} else if( (cnp->cn_namelen == 2) &&
	    !strncmp(cnp->cn_nameptr,"..",2) &&
	    (cnp->cn_flags & ISDOTDOT) ) {
		struct ntvattr *vap;

		dprintf(("ntfs_lookup: faking .. directory in %d\n",
			 dip->i_number));

		error = ntfs_ntvattrget(ntmp, dip, NTFS_A_NAME, NULL, 0, &vap);
		if(error)
			return (error);

#if __FreeBSD_version >= 300000
		VOP_UNLOCK(dvp,0,cnp->cn_proc);
#else
		VOP_UNLOCK(dvp);
#endif

		dprintf(("ntfs_lookup: parentdir: %d\n",
			 vap->va_a_name->n_pnumber));
		error = VFS_VGET(ntmp->ntm_mountp,
				 vap->va_a_name->n_pnumber,ap->a_vpp); 
		ntfs_ntvattrrele(vap);
		if(error) {
#if __FreeBSD_version >= 300000
			VOP_LOCK(dvp, 0, cnp->cn_proc);
#else
			VOP_LOCK(dvp);
#endif
			return(error);
		}

		if( lockparent && (cnp->cn_flags & ISLASTCN) && 
#if __FreeBSD_version >= 300000
		    (error = VOP_LOCK(dvp, 0, cnp->cn_proc)) ) {
#else
		    (error = VOP_LOCK(dvp)) ) {
#endif
			vput( *(ap->a_vpp) );
			return (error);
		}
		return (error);
	} else {
		struct ntnode * nip;

		error = ntfs_ntlookup(ntmp, dip, cnp, &nip);
		if(error)
			return (error);

		dprintf(("ntfs_lookup: found ino: %d\n", nip->i_number));
		if( nip->i_number == dip->i_number ) {
			ntfs_ntrele(nip);
			VREF(dvp);
			*ap->a_vpp = dvp;
			return (0);
		}

		*ap->a_vpp = ntfs_ihashget(ntmp->ntm_dev, nip->i_number);
		if(*ap->a_vpp == NULL) {
			error = getnewvnode(VT_NTFS, ntmp->ntm_mountp,
					    ntfs_vnodeop_p, ap->a_vpp);
			if(error) {
				ntfs_ntrele(nip);
				return (error);
			}
			nip->i_vnode = *(ap->a_vpp);
			(*ap->a_vpp)->v_data = nip;
			(*ap->a_vpp)->v_type = nip->i_type;	

			ntfs_ihashins(nip);

			VREF(nip->i_devvp);
		} else {
			printf("found in cache\n");
			ntfs_ntrele(nip);
		}

		if(!lockparent || !(cnp->cn_flags & ISLASTCN))
#if __FreeBSD_version >= 300000
			VOP_UNLOCK(dvp, 0, cnp->cn_proc);
#else
			VOP_UNLOCK(dvp);
#endif

	}
	return (error);
}

/*
 * Flush the blocks of a file to disk.
 *
 * This function is worthless for vnodes that represent directories. Maybe we
 * could just do a sync if they try an fsync on a directory file.
 */
static int
ntfs_fsync(ap)
	struct vop_fsync_args /* {
		struct vnode *a_vp;
		struct ucred *a_cred;
		int a_waitfor;
		struct proc *a_p;
	} */ *ap;
{
	return (0);
}

/*
 * Global vfs data structures
 */
vop_t **ntfs_vnodeop_p;
static struct vnodeopv_entry_desc ntfs_vnodeop_entries[] = {
	{ &vop_default_desc, (vop_t *)ntfs_bypass },

	{ &vop_getattr_desc, (vop_t *)ntfs_getattr },
	{ &vop_inactive_desc, (vop_t *)ntfs_inactive },
	{ &vop_reclaim_desc, (vop_t *)ntfs_reclaim },
	{ &vop_print_desc, (vop_t *)ntfs_print },

#if __FreeBSD_version >= 30000
	{ &vop_islocked_desc, (vop_t *)vop_stdislocked },
	{ &vop_unlock_desc, (vop_t *)vop_stdunlock },
	{ &vop_lock_desc, (vop_t *)vop_stdlock },
	{ &vop_cachedlookup_desc, (vop_t *)ntfs_lookup },
	{ &vop_lookup_desc, (vop_t *)vfs_cache_lookup },
#else
	{ &vop_islocked_desc, (vop_t *)ntfs_islocked },
	{ &vop_unlock_desc, (vop_t *)ntfs_unlock },
	{ &vop_lock_desc, (vop_t *)ntfs_lock },
	{ &vop_lookup_desc, (vop_t *)ntfs_lookup },
#endif

	{ &vop_access_desc, (vop_t *)ntfs_access },
	{ &vop_close_desc, (vop_t *)ntfs_close },
	{ &vop_open_desc, (vop_t *)ntfs_open },
	{ &vop_readdir_desc, (vop_t *)ntfs_readdir },
	{ &vop_fsync_desc, (vop_t *)ntfs_fsync },

	{ &vop_bmap_desc, (vop_t *)ntfs_bmap },
	{ &vop_getpages_desc, (vop_t *)ntfs_getpages },

	{ &vop_strategy_desc, (vop_t *)ntfs_strategy },
	{ &vop_bwrite_desc, (vop_t *)ntfs_bwrite },
	{ &vop_read_desc, (vop_t *)ntfs_read },

	{ NULL, NULL }
};

static struct vnodeopv_desc ntfs_vnodeop_opv_desc =
	{ &ntfs_vnodeop_p, ntfs_vnodeop_entries };

VNODEOP_SET(ntfs_vnodeop_opv_desc);
