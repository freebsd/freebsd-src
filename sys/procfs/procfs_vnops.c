/*
 * Copyright (c) 1993 Paul Kranenburg
 * All rights reserved.
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
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: procfs_vnops.c,v 1.6 1994/01/31 04:19:20 davidg Exp $
 */

/*
 * PROCFS vnode interface routines
 */

#include "param.h"
#include "systm.h"
#include "time.h"
#include "kernel.h"
#include "ioctl.h"
#include "file.h"
#include "proc.h"
#include "buf.h"
#include "vnode.h"
#include "namei.h"
#include "resourcevar.h"
#include "vm/vm.h"
#include "kinfo.h"
#include "kinfo_proc.h"

#include "sys/procfs.h"
#include "pfsnode.h"

#include "machine/vmparam.h"

/*
 * procfs vnode operations.
 */
struct vnodeops pfs_vnodeops = {
	pfs_lookup,		/* lookup */
	pfs_create,		/* create */
	pfs_mknod,		/* mknod */
	pfs_open,		/* open */
	pfs_close,		/* close */
	pfs_access,		/* access */
	pfs_getattr,		/* getattr */
	pfs_setattr,		/* setattr */
	pfs_read,		/* read */
	pfs_write,		/* write */
	pfs_ioctl,		/* ioctl */
	pfs_select,		/* select */
	pfs_mmap,		/* mmap */
	pfs_fsync,		/* fsync */
	pfs_seek,		/* seek */
	pfs_remove,		/* remove */
	pfs_link,		/* link */
	pfs_rename,		/* rename */
	pfs_mkdir,		/* mkdir */
	pfs_rmdir,		/* rmdir */
	pfs_symlink,		/* symlink */
	pfs_readdir,		/* readdir */
	pfs_readlink,		/* readlink */
	pfs_abortop,		/* abortop */
	pfs_inactive,		/* inactive */
	pfs_reclaim,		/* reclaim */
	pfs_lock,		/* lock */
	pfs_unlock,		/* unlock */
	pfs_bmap,		/* bmap */
	pfs_strategy,		/* strategy */
	pfs_print,		/* print */
	pfs_islocked,		/* islocked */
	pfs_advlock,		/* advlock */
};

/*
 * Vnode Operations.
 *
 */
/* ARGSUSED */
int
pfs_open(vp, mode, cred, p)
	register struct vnode *vp;
	int mode;
	struct ucred *cred;
	struct proc *p;
{
	struct pfsnode	*pfsp = VTOPFS(vp);
	struct proc *procp;

#ifdef DEBUG
	if (pfs_debug)
		printf("pfs_open: vp 0x%x, proc %d\n", vp, p->p_pid);
#endif

	if ( (procp = (pfsp->pfs_pid?pfind(pfsp->pfs_pid):&proc0)) == NULL)
		return ESRCH;

	if (	(pfsp->pfs_flags & FWRITE) && (mode & O_EXCL) ||
		(pfsp->pfs_flags & O_EXCL) && (mode & FWRITE)	)
		return EBUSY;


	if (mode & FWRITE)
		pfsp->pfs_flags = (mode & (FWRITE|O_EXCL));

	procp->p_vmspace->vm_refcnt++;
	pfsp->pfs_vs = procp->p_vmspace;
	return 0;
}

/*
 * /proc filesystem close routine
 */
/* ARGSUSED */
int
pfs_close(vp, flag, cred, p)
	register struct vnode *vp;
	int flag;
	struct ucred *cred;
	struct proc *p;
{
	struct pfsnode	*pfsp = VTOPFS(vp);

#ifdef DEBUG
	if (pfs_debug)
		printf("pfs_close: vp 0x%x proc %d\n", vp, p->p_pid);
#endif
	if ((flag & FWRITE) && (pfsp->pfs_flags & O_EXCL))
		pfsp->pfs_flags &= ~(FWRITE|O_EXCL);

	vmspace_free(pfsp->pfs_vs);
	return (0);
}

/*
 * Ioctl operation.
 */
/* ARGSUSED */
int
pfs_ioctl(vp, com, data, fflag, cred, p)
	struct vnode *vp;
	int com;
	caddr_t data;
	int fflag;
	struct ucred *cred;
	struct proc *p;
{
	int		error = 0;
	struct proc	*procp;
	struct pfsnode	*pfsp = VTOPFS(vp);

	procp = pfsp->pfs_pid?pfind(pfsp->pfs_pid):&proc0;
	if (!procp)
		return ESRCH;

	switch (com) {

	case PIOCGPINFO: {
		int copysize = sizeof(struct kinfo_proc), needed;
		kinfo_doproc(KINFO_PROC_PID, data, &copysize,
						pfsp->pfs_pid, &needed);
		break;
		}

#ifdef notyet /* Changes to proc.h needed */
	case PIOCGSIGSET:
		procp->p_psigset = *(sigset_t *)data;
		break;

	case PIOCSSIGSET:
		*(sigset_t *)data = procp->p_psigset;
		break;

	case PIOCGFLTSET:
		procp->p_pfltset = *(sigflt_t *)data;
		break;

	case PIOCSFLTSET:
		*(fltset_t *)data = procp->p_pfltset;
		break;
#endif

	case PIOCGMAPFD:
		error = pfs_vmfd(procp, pfsp, (struct vmfd *)data, p);
		break;

	case PIOCGNMAP:
		*(int *)data = pfs_vm_nentries(procp, pfsp);
		break;

	case PIOCGMAP:
		error = pfs_vmmap(procp, pfsp, *(struct procmap *)data);
		break;

	default:
		error = EIO;
		break;
	}
	return error;
}

/*
 * Pass I/O requests to the memory filesystem process.
 */
int
pfs_strategy(bp)
	register struct buf *bp;
{
	struct vnode *vp;
	struct proc *p = curproc;		/* XXX */

	return (0);
}

/*
 * This is a noop, simply returning what one has been given.
 */
int
pfs_bmap(vp, bn, vpp, bnp)
	struct vnode *vp;
	daddr_t bn;
	struct vnode **vpp;
	daddr_t *bnp;
{

	if (vpp != NULL)
		*vpp = vp;
	if (bnp != NULL)
		*bnp = bn;
	return (0);
}

/*
 * /proc filesystem inactive routine
 */
/* ARGSUSED */
int
pfs_inactive(vp, p)
	struct vnode *vp;
	struct proc *p;
{
	struct pfsnode	*pfsp = VTOPFS(vp);

	if ((pfsp->pfs_pid?pfind(pfsp->pfs_pid):&proc0) == NULL
	    && vp->v_usecount == 0)
		vgone(vp);

	return 0;
}

/*
 * /proc filesystem reclaim routine
 */
/* ARGSUSED */
int
pfs_reclaim(vp)
	struct vnode *vp;
{
	struct pfsnode	**pp, *pfsp = VTOPFS(vp);

	for (pp = &pfshead; *pp; pp = &(*pp)->pfs_next) {
		if (*pp == pfsp) {
			*pp = pfsp->pfs_next;
			break;
		}
	}
	return 0;
}

/*
 * Print out the contents of an pfsnode.
 */
void
pfs_print(vp)
	struct vnode *vp;
{
	struct pfsnode	*pfsp = VTOPFS(vp);

	printf("tag VT_PROCFS, pid %d, uid %d, gid %d, mode %x, flags %x\n",
		pfsp->pfs_pid,
		pfsp->pfs_uid, pfsp->pfs_gid,
		pfsp->pfs_mode, pfsp->pfs_flags);

	return;
}

/*
 * /proc bad operation
 */
int
pfs_badop()
{
	printf("pfs_badop called\n");
	return EIO;
}

/*
 * Make up some attributes for a process file
 */
int
pfs_getattr (vp, vap, cred, p)
	struct vnode *vp;
	struct vattr *vap;
	struct ucred *cred;
	struct proc *p;
{
	struct pfsnode *pfsp = VTOPFS(vp);
	struct proc *procp;

	VATTR_NULL(vap);
	vap->va_type = vp->v_type;
	vap->va_flags = pfsp->pfs_vflags;

	if (vp->v_flag & VROOT) {
		vap->va_mode = 0750; /* /proc = rwxr-x--- */
		vap->va_nlink = 2;
		vap->va_size =
			roundup((2+nprocs)*sizeof(struct pfsdent), DIRBLKSIZ);
		vap->va_size_rsv = 0;
		vap->va_uid = 0;
		vap->va_gid = 2; /* XXX group kmem */
		vap->va_bytes = 0;
		vap->va_atime = vap->va_mtime = vap->va_ctime = time; /*XXX*/
		vap->va_rdev = makedev(255, 255);
		return 0;
	}


	vap->va_rdev = makedev(255, pfsp->pfs_pid);
	vap->va_mode = 0644;
	procp = pfsp->pfs_pid?pfind(pfsp->pfs_pid):&proc0;
	if (!procp)
		return ESRCH;

	vap->va_nlink = 1;
	vap->va_size = ctob(	procp->p_vmspace->vm_tsize +
				procp->p_vmspace->vm_dsize +
				procp->p_vmspace->vm_ssize);
	vap->va_bytes = 0;
	vap->va_size_rsv = 0;
	vap->va_blocksize = PAGE_SIZE;
	vap->va_uid = procp->p_ucred->cr_uid;
	vap->va_gid = procp->p_ucred->cr_gid;
	if (vap->va_uid != procp->p_cred->p_ruid)
		vap->va_mode |= VSUID;
	if (vap->va_gid != procp->p_cred->p_rgid)
		vap->va_mode |= VSGID;
	if (procp->p_flag & SLOAD) {
		vap->va_atime = vap->va_mtime = vap->va_ctime =
			procp->p_stats->p_start;
	}

	return 0;
}

/*
 * Set some attributes for a process file
 */
int
pfs_setattr (vp, vap, cred, p)
	struct vnode *vp;
	struct vattr *vap;
	struct ucred *cred;
	struct proc *p;
{
	struct pfsnode *pfsp = VTOPFS(vp);
	struct proc *procp;
	int error = 0;

	procp = pfsp->pfs_pid?pfind(pfsp->pfs_pid):&proc0;
		if (!procp)
			return ESRCH;

	/*
	 * Check for unsetable attributes.
	 */
	if ((vap->va_type != VNON) || (vap->va_nlink != (short)VNOVAL) ||
	    (vap->va_fsid != (long)VNOVAL) ||
	    (vap->va_fileid != (long)VNOVAL) ||
	    (vap->va_blocksize != (long)VNOVAL) ||
	    (vap->va_rdev != (dev_t)VNOVAL) ||
	    ((int)vap->va_bytes != (u_long)VNOVAL) ||
	    ((int)vap->va_bytes_rsv != (u_long)VNOVAL) ||
	    ((int)vap->va_size != (u_long)VNOVAL) ||
	    ((int)vap->va_size_rsv != (u_long)VNOVAL) ||
	    (vap->va_gen != (long)VNOVAL) ||
	    ((int)vap->va_atime.tv_sec != (u_long)VNOVAL) ||
	    ((int)vap->va_mtime.tv_sec != (u_long)VNOVAL) ||
	    ((int)vap->va_ctime.tv_sec != (u_long)VNOVAL) ||
	    ((	(vap->va_uid != (uid_t)VNOVAL) ||
		(vap->va_gid != (gid_t)VNOVAL)) && !(vp->v_flag & VROOT)) ) {
		return (EINVAL);
	}

	/* set mode bits, only rwx bits are modified */
	if (vap->va_mode != (u_short)VNOVAL) {
		if (cred->cr_uid != pfsp->pfs_uid &&
			(error = suser(cred, &p->p_acflag)))
				return (error);
		pfsp->pfs_mode = vap->va_mode & 0777;
	}

	/* For now, only allow to change ownership of "/proc" itself */
	if ((vp->v_flag & VROOT) && vap->va_uid != (uid_t)VNOVAL) {
		if ((error = suser(cred, &p->p_acflag)))
			return (error);
		pfsp->pfs_uid = vap->va_uid;
	}

	if ((vp->v_flag & VROOT) && vap->va_gid != (gid_t)VNOVAL) {
		if ((cred->cr_uid != pfsp->pfs_uid ||
			!groupmember(vap->va_gid, cred)) &&
				(error = suser(cred, &p->p_acflag)))
			return error;

		pfsp->pfs_gid = vap->va_gid;
	}

	/* chflags() */
	if (vap->va_flags != (u_long)VNOVAL) {
		if (cred->cr_uid != pfsp->pfs_uid &&
			(error = suser(cred, &p->p_acflag)))
				return (error);
		if (cred->cr_uid == 0) {
			pfsp->pfs_vflags = vap->va_flags;
		} else {
			pfsp->pfs_vflags &= 0xffff0000ul;
			pfsp->pfs_vflags |= (vap->va_flags & 0xffff);
		}
	}
	return 0;
}

int
pfs_access (vp, mode, cred, p)
	struct vnode *vp;
	int mode;
	struct ucred *cred;
	struct proc *p;
{
	register struct vattr *vap;
	register gid_t *gp;
	struct vattr vattr;
	register int i;
	int error;

	/*
	 * If you're the super-user,
	 * you always get access.
	 */
	if (cred->cr_uid == (uid_t)0)
		return (0);
	vap = &vattr;
	if (error = pfs_getattr(vp, vap, cred, p))
		return (error);
	/*
	 * Access check is based on only one of owner, group, public.
	 * If not owner, then check group. If not a member of the
	 * group, then check public access.
	 */
	if (cred->cr_uid != vap->va_uid) {
		mode >>= 3;
		gp = cred->cr_groups;
		for (i = 0; i < cred->cr_ngroups; i++, gp++)
			if (vap->va_gid == *gp)
				goto found;
		mode >>= 3;
found:
		;
	}
	if ((vap->va_mode & mode) != 0)
		return (0);
	return (EACCES);
}

/*
 * /proc lookup
 */
int
pfs_lookup(vp, ndp, p)
	register struct vnode *vp;
	register struct nameidata *ndp;
	struct proc *p;
{
	int lockparent, wantparent, flag, error = 0;
	pid_t pid;
	struct vnode *nvp;
	struct pfsnode *pfsp;
	struct proc *procp;

#ifdef DEBUG
	if (pfs_debug)
		printf("pfs_lookup: vp 0x%x name %s proc %d\n",
			vp, ndp->ni_ptr, p->p_pid);
#endif

	ndp->ni_dvp = vp;
	ndp->ni_vp = NULL;
	if (vp->v_type != VDIR)
		return (ENOTDIR);

	lockparent = ndp->ni_nameiop & LOCKPARENT;
	flag = ndp->ni_nameiop & OPMASK;
	wantparent = ndp->ni_nameiop & (LOCKPARENT|WANTPARENT);
	if (flag != LOOKUP)
		return EACCES;
	if (ndp->ni_isdotdot) {
		/* Should not happen */
		printf("pfs_lookup: vp 0x%x: dotdot\n", vp);
		return EIO;
	}
	if (ndp->ni_namelen == 1 && *ndp->ni_ptr == '.') {
		VREF(vp);
		ndp->ni_vp = vp;
		return 0;
	}

	pid = (pid_t)atoi(ndp->ni_ptr, ndp->ni_namelen);
	if (pid == (pid_t)-1)
		return ENOENT;

	if ((procp = pid?pfind(pid):&proc0) == NULL)
		return ENOENT;

loop:
	/* Search pfs node list first */
	for (pfsp = pfshead; pfsp != NULL; pfsp = pfsp->pfs_next) {
		if (pfsp->pfs_pid == pid)
			break;
	}

	if (pfsp == NULL) {
		struct pfsnode	**pp;
		error = getnewvnode(VT_PROCFS, vp->v_mount, &pfs_vnodeops, &nvp);
		if (error)
			return error;

		nvp->v_type = VPROC;
		pfsp = VTOPFS(nvp);
		pfsp->pfs_next = NULL;
		pfsp->pfs_pid = pid;
		pfsp->pfs_vnode = nvp;
		pfsp->pfs_flags = 0;
		pfsp->pfs_vflags = 0;
		pfsp->pfs_uid = procp->p_ucred->cr_uid;
		pfsp->pfs_gid = procp->p_ucred->cr_gid;
		pfsp->pfs_mode = 0700;	/* Initial access bits */

		/* Append to pfs node list */
		pfsp->pfs_next = pfshead;
		pfshead = pfsp;

	} else {
		if (vget(pfsp->pfs_vnode))
			goto loop;
		VOP_UNLOCK(pfsp->pfs_vnode);
	}
	ndp->ni_vp = pfsp->pfs_vnode;

	return (error);
}

int
pfs_readdir(vp, uio, cred, eofflagp)
        struct vnode *vp;
        register struct uio *uio;
        struct ucred *cred;
        int *eofflagp;
{
	int	error = 0;
	int	count, lost, pcnt, skipcnt, doingzomb = 0;
	struct proc *p;
	struct pfsdent dent;

#ifdef DEBUG
	if (pfs_debug)
		printf("pfs_readdir: vp 0x%x proc %d\n",
				vp, uio->uio_procp->p_pid);
#endif
	count = uio->uio_resid;
	count &= ~(DIRBLKSIZ - 1);
	lost = uio->uio_resid - count;
	if (count < DIRBLKSIZ || (uio->uio_offset & (DIRBLKSIZ -1)))
		return (EINVAL);
	uio->uio_resid = count;
	uio->uio_iov->iov_len = count;
	*eofflagp = 1;
	skipcnt = uio->uio_offset / sizeof(struct pfsdent);

	count = 0;
	if (skipcnt == 0) {
		/* Fake "." and ".." entries? */
#if 0
		dent.d_fileno = 2;		/* XXX - Filesystem root */
		dent.d_reclen = sizeof(struct pfsdent);

		dent.d_namlen = 1;
		dent.d_nam[0] = '.';
		dent.d_nam[1] = '\0';
		error = uiomove((char *)&dent, sizeof(struct pfsdent) , uio);
		if (error)
			return error;
		
		dent.d_fileno = 2;
		dent.d_namlen = 2;
		dent.d_nam[1] = '.';
		dent.d_nam[2] = '\0';
		error = uiomove((char *)&dent, sizeof(struct pfsdent) , uio);
		if (error)
			return error;
#endif
		count += 2*dent.d_reclen;
	}

	p = (struct proc *)allproc;
	for (pcnt = 0; p && uio->uio_resid; pcnt++) {
		if (pcnt < skipcnt) {
			p = p->p_nxt;
			if (p == NULL && doingzomb == 0) {
				doingzomb = 1;
				p = zombproc;
			}
			continue;
		}
		*eofflagp = 0;

		/* "inode" is process slot (actually position on list) */
		dent.d_fileno = (unsigned long)(pcnt+1);
		dent.d_namlen = itos((unsigned int)p->p_pid, dent.d_nam);
		dent.d_nam[dent.d_namlen] = '\0';

		p = p->p_nxt;
		if (p == NULL && doingzomb == 0) {
			doingzomb = 1;
			p = zombproc;
		}
		if (p == NULL) {
			/* Extend 'reclen' to end of block */;
			dent.d_reclen = DIRBLKSIZ - (count & (DIRBLKSIZ - 1));
		} else
			dent.d_reclen = sizeof(struct pfsdent);
		count += dent.d_reclen;
		error = uiomove((char *)&dent, dent.d_reclen, uio);
		if (error)
			break;
	}
	if (count == 0)
		*eofflagp = 1;

	uio->uio_resid += lost;
	return error;
}

/*
 * convert n to decimal representation in character array b
 * return number of decimal digits produced.
 */
int
itos(n, b)
unsigned int n;
char *b;
{
#define BASE	10
	int m = (n<BASE)?0:itos(n/BASE, b);
 
	*(b+m) = "0123456789abcdef"[n%BASE];
	return m+1;
}

/*
 * convert decimal ascii representation in b of length len to integer
 */
int
atoi(b, len)
char *b;
unsigned int len;
{
	int n = 0;

	while (len--) {
		register char c = *b++;
		if (c < '0' || c > '9')
			return -1;
		n = 10 * n + (c - '0');
	}
	return n;
}
