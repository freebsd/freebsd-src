/*
 * Copyright (c) 1993, 1995 Jan-Simon Pendry
 * Copyright (c) 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Jan-Simon Pendry.
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
 *	@(#)procfs_vnops.c	8.18 (Berkeley) 5/21/95
 *
 * From:
 *	$Id: procfs_vnops.c,v 3.2 1993/12/15 09:40:17 jsp Exp $
 */

/*
 * procfs vnode interface
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/resourcevar.h>
#include <vm/vm.h>	/* for PAGE_SIZE */
#include <machine/reg.h>
#include <miscfs/procfs/procfs.h>

/*
 * Vnode Operations.
 *
 */

/*
 * This is a list of the valid names in the
 * process-specific sub-directories.  It is
 * used in procfs_lookup and procfs_readdir
 */
struct proc_target {
	u_char	pt_type;
	u_char	pt_namlen;
	char	*pt_name;
	pfstype	pt_pfstype;
	int	(*pt_valid) __P((struct proc *p));
} proc_targets[] = {
#define N(s) sizeof(s)-1, s
	/*	  name		type		validp */
	{ DT_DIR, N("."),	Pproc,		NULL },
	{ DT_DIR, N(".."),	Proot,		NULL },
	{ DT_REG, N("file"),	Pfile,		procfs_validfile },
	{ DT_REG, N("mem"),	Pmem,		NULL },
	{ DT_REG, N("regs"),	Pregs,		procfs_validregs },
	{ DT_REG, N("fpregs"),	Pfpregs,	procfs_validfpregs },
	{ DT_REG, N("ctl"),	Pctl,		NULL },
	{ DT_REG, N("status"),	Pstatus,	NULL },
	{ DT_REG, N("note"),	Pnote,		NULL },
	{ DT_REG, N("notepg"),	Pnotepg,	NULL },
#undef N
};
static int nproc_targets = sizeof(proc_targets) / sizeof(proc_targets[0]);

static pid_t atopid __P((const char *, u_int));

/*
 * set things up for doing i/o on
 * the pfsnode (vp).  (vp) is locked
 * on entry, and should be left locked
 * on exit.
 *
 * for procfs we don't need to do anything
 * in particular for i/o.  all that is done
 * is to support exclusive open on process
 * memory images.
 */
procfs_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct pfsnode *pfs = VTOPFS(ap->a_vp);

	switch (pfs->pfs_type) {
	case Pmem:
		if (PFIND(pfs->pfs_pid) == 0)
			return (ENOENT);	/* was ESRCH, jsp */

		if ((pfs->pfs_flags & FWRITE) && (ap->a_mode & O_EXCL) ||
		    (pfs->pfs_flags & O_EXCL) && (ap->a_mode & FWRITE))
			return (EBUSY);

		if (ap->a_mode & FWRITE)
			pfs->pfs_flags = ap->a_mode & (FWRITE|O_EXCL);

		return (0);

	default:
		break;
	}

	return (0);
}

/*
 * close the pfsnode (vp) after doing i/o.
 * (vp) is not locked on entry or exit.
 *
 * nothing to do for procfs other than undo
 * any exclusive open flag (see _open above).
 */
procfs_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct pfsnode *pfs = VTOPFS(ap->a_vp);

	switch (pfs->pfs_type) {
	case Pmem:
		if ((ap->a_fflag & FWRITE) && (pfs->pfs_flags & O_EXCL))
			pfs->pfs_flags &= ~(FWRITE|O_EXCL);
		break;
	}

	return (0);
}

/*
 * do an ioctl operation on pfsnode (vp).
 * (vp) is not locked on entry or exit.
 */
procfs_ioctl(ap)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		int a_command;
		caddr_t a_data;
		int a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{

	return (ENOTTY);
}

/*
 * do block mapping for pfsnode (vp).
 * since we don't use the buffer cache
 * for procfs this function should never
 * be called.  in any case, it's not clear
 * what part of the kernel ever makes use
 * of this function.  for sanity, this is the
 * usual no-op bmap, although returning
 * (EIO) would be a reasonable alternative.
 */
procfs_bmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap;
{

	if (ap->a_vpp != NULL)
		*ap->a_vpp = ap->a_vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn;
	if (ap->a_runp != NULL)
		*ap->a_runp = 0;
	return (0);
}

/*
 * procfs_inactive is called when the pfsnode
 * is vrele'd and the reference count goes
 * to zero.  (vp) will be on the vnode free
 * list, so to get it back vget() must be
 * used.
 *
 * for procfs, check if the process is still
 * alive and if it isn't then just throw away
 * the vnode by calling vgone().  this may
 * be overkill and a waste of time since the
 * chances are that the process will still be
 * there and PFIND is not free.
 *
 * (vp) is locked on entry, but must be unlocked on exit.
 */
procfs_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct pfsnode *pfs = VTOPFS(vp);

	VOP_UNLOCK(vp, 0, ap->a_p);
	if (PFIND(pfs->pfs_pid) == 0)
		vgone(vp);

	return (0);
}

/*
 * _reclaim is called when getnewvnode()
 * wants to make use of an entry on the vnode
 * free list.  at this time the filesystem needs
 * to free any private data and remove the node
 * from any private lists.
 */
procfs_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	return (procfs_freevp(ap->a_vp));
}

/*
 * Return POSIX pathconf information applicable to special devices.
 */
procfs_pathconf(ap)
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		int *a_retval;
	} */ *ap;
{

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = LINK_MAX;
		return (0);
	case _PC_MAX_CANON:
		*ap->a_retval = MAX_CANON;
		return (0);
	case _PC_MAX_INPUT:
		*ap->a_retval = MAX_INPUT;
		return (0);
	case _PC_PIPE_BUF:
		*ap->a_retval = PIPE_BUF;
		return (0);
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		return (0);
	case _PC_VDISABLE:
		*ap->a_retval = _POSIX_VDISABLE;
		return (0);
	default:
		return (EINVAL);
	}
	/* NOTREACHED */
}

/*
 * _print is used for debugging.
 * just print a readable description
 * of (vp).
 */
procfs_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct pfsnode *pfs = VTOPFS(ap->a_vp);

	printf("tag VT_PROCFS, type %s, pid %d, mode %x, flags %x\n",
	    pfs->pfs_type, pfs->pfs_pid, pfs->pfs_mode, pfs->pfs_flags);
}

/*
 * _abortop is called when operations such as
 * rename and create fail.  this entry is responsible
 * for undoing any side-effects caused by the lookup.
 * this will always include freeing the pathname buffer.
 */
procfs_abortop(ap)
	struct vop_abortop_args /* {
		struct vnode *a_dvp;
		struct componentname *a_cnp;
	} */ *ap;
{

	if ((ap->a_cnp->cn_flags & (HASBUF | SAVESTART)) == HASBUF)
		FREE(ap->a_cnp->cn_pnbuf, M_NAMEI);
	return (0);
}

/*
 * generic entry point for unsupported operations
 */
procfs_badop()
{

	return (EIO);
}

/*
 * Invent attributes for pfsnode (vp) and store
 * them in (vap).
 * Directories lengths are returned as zero since
 * any real length would require the genuine size
 * to be computed, and nothing cares anyway.
 *
 * this is relatively minimal for procfs.
 */
procfs_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct pfsnode *pfs = VTOPFS(ap->a_vp);
	struct vattr *vap = ap->a_vap;
	struct proc *procp;
	struct timeval tv;
	int error;

	/* first check the process still exists */
	switch (pfs->pfs_type) {
	case Proot:
	case Pcurproc:
		procp = 0;
		break;

	default:
		procp = PFIND(pfs->pfs_pid);
		if (procp == 0)
			return (ENOENT);
	}

	error = 0;

	/* start by zeroing out the attributes */
	VATTR_NULL(vap);

	/* next do all the common fields */
	vap->va_type = ap->a_vp->v_type;
	vap->va_mode = pfs->pfs_mode;
	vap->va_fileid = pfs->pfs_fileno;
	vap->va_flags = 0;
	vap->va_blocksize = PAGE_SIZE;
	vap->va_bytes = vap->va_size = 0;

	/*
	 * Make all times be current TOD.
	 * It would be possible to get the process start
	 * time from the p_stat structure, but there's
	 * no "file creation" time stamp anyway, and the
	 * p_stat structure is not addressible if u. gets
	 * swapped out for that process.
	 */
	microtime(&tv);
	TIMEVAL_TO_TIMESPEC(&tv, &vap->va_ctime);
	vap->va_atime = vap->va_mtime = vap->va_ctime;

	/*
	 * If the process has exercised some setuid or setgid
	 * privilege, then rip away read/write permission so
	 * that only root can gain access.
	 */
	switch (pfs->pfs_type) {
	case Pmem:
	case Pregs:
	case Pfpregs:
		if (procp->p_flag & P_SUGID)
			vap->va_mode &= ~((VREAD|VWRITE)|
					  ((VREAD|VWRITE)>>3)|
					  ((VREAD|VWRITE)>>6));
	case Pctl:
	case Pstatus:
	case Pnote:
	case Pnotepg:
		vap->va_nlink = 1;
		vap->va_uid = procp->p_ucred->cr_uid;
		vap->va_gid = procp->p_ucred->cr_gid;
		break;
	}

	/*
	 * now do the object specific fields
	 *
	 * The size could be set from struct reg, but it's hardly
	 * worth the trouble, and it puts some (potentially) machine
	 * dependent data into this machine-independent code.  If it
	 * becomes important then this function should break out into
	 * a per-file stat function in the corresponding .c file.
	 */

	switch (pfs->pfs_type) {
	case Proot:
		/*
		 * Set nlink to 1 to tell fts(3) we don't actually know.
		 */
		vap->va_nlink = 1;
		vap->va_uid = 0;
		vap->va_gid = 0;
		vap->va_size = vap->va_bytes = DEV_BSIZE;
		break;

	case Pcurproc: {
		char buf[16];		/* should be enough */
		vap->va_nlink = 1;
		vap->va_uid = 0;
		vap->va_gid = 0;
		vap->va_size = vap->va_bytes =
		    sprintf(buf, "%ld", (long)curproc->p_pid);
		break;
	}

	case Pproc:
		vap->va_nlink = 2;
		vap->va_uid = procp->p_ucred->cr_uid;
		vap->va_gid = procp->p_ucred->cr_gid;
		vap->va_size = vap->va_bytes = DEV_BSIZE;
		break;

	case Pfile:
		error = EOPNOTSUPP;
		break;

	case Pmem:
		vap->va_bytes = vap->va_size =
			ctob(procp->p_vmspace->vm_tsize +
				    procp->p_vmspace->vm_dsize +
				    procp->p_vmspace->vm_ssize);
		break;

	case Pregs:
		vap->va_bytes = vap->va_size = sizeof(struct reg);
		break;

	case Pfpregs:
		vap->va_bytes = vap->va_size = sizeof(struct fpreg);
		break;

	case Pctl:
	case Pstatus:
	case Pnote:
	case Pnotepg:
		break;

	default:
		panic("procfs_getattr");
	}

	return (error);
}

procfs_setattr(ap)
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	/*
	 * just fake out attribute setting
	 * it's not good to generate an error
	 * return, otherwise things like creat()
	 * will fail when they try to set the
	 * file length to 0.  worse, this means
	 * that echo $note > /proc/$pid/note will fail.
	 */

	return (0);
}

/*
 * implement access checking.
 *
 * something very similar to this code is duplicated
 * throughout the 4bsd kernel and should be moved
 * into kern/vfs_subr.c sometime.
 *
 * actually, the check for super-user is slightly
 * broken since it will allow read access to write-only
 * objects.  this doesn't cause any particular trouble
 * but does mean that the i/o entry points need to check
 * that the operation really does make sense.
 */
procfs_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vattr *vap;
	struct vattr vattr;
	int error;

	/*
	 * If you're the super-user,
	 * you always get access.
	 */
	if (ap->a_cred->cr_uid == 0)
		return (0);

	vap = &vattr;
	if (error = VOP_GETATTR(ap->a_vp, vap, ap->a_cred, ap->a_p))
		return (error);

	/*
	 * Access check is based on only one of owner, group, public.
	 * If not owner, then check group. If not a member of the
	 * group, then check public access.
	 */
	if (ap->a_cred->cr_uid != vap->va_uid) {
		gid_t *gp;
		int i;

		ap->a_mode >>= 3;
		gp = ap->a_cred->cr_groups;
		for (i = 0; i < ap->a_cred->cr_ngroups; i++, gp++)
			if (vap->va_gid == *gp)
				goto found;
		ap->a_mode >>= 3;
found:
		;
	}

	if ((vap->va_mode & ap->a_mode) == ap->a_mode)
		return (0);

	return (EACCES);
}

/*
 * lookup.  this is incredibly complicated in the
 * general case, however for most pseudo-filesystems
 * very little needs to be done.
 *
 * unless you want to get a migraine, just make sure your
 * filesystem doesn't do any locking of its own.  otherwise
 * read and inwardly digest ufs_lookup().
 */
procfs_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	struct vnode **vpp = ap->a_vpp;
	struct vnode *dvp = ap->a_dvp;
	char *pname = cnp->cn_nameptr;
	struct proc *curp = cnp->cn_proc;
	int error = 0;
	struct proc_target *pt;
	struct vnode *fvp;
	pid_t pid;
	struct pfsnode *pfs;
	struct proc *p;
	int i;

	*vpp = NULL;

	if (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)
		return (EROFS);

	if (cnp->cn_namelen == 1 && *pname == '.') {
		*vpp = dvp;
		VREF(dvp);
		/* vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY, curp); */
		return (0);
	}

	pfs = VTOPFS(dvp);
	switch (pfs->pfs_type) {
	case Proot:
		if (cnp->cn_flags & ISDOTDOT)
			return (EIO);

		if (CNEQ(cnp, "curproc", 7))
			return (procfs_allocvp(dvp->v_mount, vpp, 0, Pcurproc));

		pid = atopid(pname, cnp->cn_namelen);
		if (pid == NO_PID)
			break;

		p = PFIND(pid);
		if (p == 0)
			break;

		return (procfs_allocvp(dvp->v_mount, vpp, pid, Pproc));

	case Pproc:
		if (cnp->cn_flags & ISDOTDOT)
			return (procfs_root(dvp->v_mount, vpp));

		p = PFIND(pfs->pfs_pid);
		if (p == 0)
			break;

		for (pt = proc_targets, i = 0; i < nproc_targets; pt++, i++) {
			if (cnp->cn_namelen == pt->pt_namlen &&
			    bcmp(pt->pt_name, pname, cnp->cn_namelen) == 0 &&
			    (pt->pt_valid == NULL || (*pt->pt_valid)(p)))
				goto found;
		}
		break;

	found:
		if (pt->pt_pfstype == Pfile) {
			fvp = procfs_findtextvp(p);
			/* We already checked that it exists. */
			VREF(fvp);
			vn_lock(fvp, LK_EXCLUSIVE | LK_RETRY, curp);
			*vpp = fvp;
			return (0);
		}

		return (procfs_allocvp(dvp->v_mount, vpp, pfs->pfs_pid,
		    pt->pt_pfstype));

	default:
		return (ENOTDIR);
	}

	return (cnp->cn_nameiop == LOOKUP ? ENOENT : EROFS);
}

int
procfs_validfile(p)
	struct proc *p;
{

	return (procfs_findtextvp(p) != NULLVP);
}

/*
 * readdir returns directory entries from pfsnode (vp).
 *
 * the strategy here with procfs is to generate a single
 * directory entry at a time (struct pfsdent) and then
 * copy that out to userland using uiomove.  a more efficent
 * though more complex implementation, would try to minimize
 * the number of calls to uiomove().  for procfs, this is
 * hardly worth the added code complexity.
 *
 * this should just be done through read()
 */
procfs_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		u_long *a_cookies;
		int a_ncookies;
	} */ *ap;
{
	struct uio *uio = ap->a_uio;
	struct pfsdent d;
	struct pfsdent *dp = &d;
	struct pfsnode *pfs;
	int error;
	int count;
	int i;

	/*
	 * We don't allow exporting procfs mounts, and currently local
	 * requests do not need cookies.
	 */
	if (ap->a_ncookies)
		panic("procfs_readdir: not hungry");

	pfs = VTOPFS(ap->a_vp);

	if (uio->uio_resid < UIO_MX)
		return (EINVAL);
	if (uio->uio_offset & (UIO_MX-1))
		return (EINVAL);
	if (uio->uio_offset < 0)
		return (EINVAL);

	error = 0;
	count = 0;
	i = uio->uio_offset / UIO_MX;

	switch (pfs->pfs_type) {
	/*
	 * this is for the process-specific sub-directories.
	 * all that is needed to is copy out all the entries
	 * from the procent[] table (top of this file).
	 */
	case Pproc: {
		struct proc *p;
		struct proc_target *pt;

		p = PFIND(pfs->pfs_pid);
		if (p == NULL)
			break;

		for (pt = &proc_targets[i];
		     uio->uio_resid >= UIO_MX && i < nproc_targets; pt++, i++) {
			if (pt->pt_valid && (*pt->pt_valid)(p) == 0)
				continue;
			
			dp->d_reclen = UIO_MX;
			dp->d_fileno = PROCFS_FILENO(pfs->pfs_pid, pt->pt_pfstype);
			dp->d_namlen = pt->pt_namlen;
			bcopy(pt->pt_name, dp->d_name, pt->pt_namlen + 1);
			dp->d_type = pt->pt_type;

			if (error = uiomove((caddr_t)dp, UIO_MX, uio))
				break;
		}

	    	break;
	    }

	/*
	 * this is for the root of the procfs filesystem
	 * what is needed is a special entry for "curproc"
	 * followed by an entry for each process on allproc
#ifdef PROCFS_ZOMBIE
	 * and zombproc.
#endif
	 */

	case Proot: {
#ifdef PROCFS_ZOMBIE
		int doingzomb = 0;
#endif
		int pcnt = 0;
		volatile struct proc *p = allproc.lh_first;

	again:
		for (; p && uio->uio_resid >= UIO_MX; i++, pcnt++) {
			bzero((char *) dp, UIO_MX);
			dp->d_reclen = UIO_MX;

			switch (i) {
			case 0:		/* `.' */
			case 1:		/* `..' */
				dp->d_fileno = PROCFS_FILENO(0, Proot);
				dp->d_namlen = i + 1;
				bcopy("..", dp->d_name, dp->d_namlen);
				dp->d_name[i + 1] = '\0';
				dp->d_type = DT_DIR;
				break;

			case 2:
				dp->d_fileno = PROCFS_FILENO(0, Pcurproc);
				dp->d_namlen = 7;
				bcopy("curproc", dp->d_name, 8);
				dp->d_type = DT_LNK;
				break;

			default:
				while (pcnt < i) {
					pcnt++;
					p = p->p_list.le_next;
					if (!p)
						goto done;
				}
				dp->d_fileno = PROCFS_FILENO(p->p_pid, Pproc);
				dp->d_namlen = sprintf(dp->d_name, "%ld",
				    (long)p->p_pid);
				dp->d_type = DT_REG;
				p = p->p_list.le_next;
				break;
			}

			if (error = uiomove((caddr_t)dp, UIO_MX, uio))
				break;
		}
	done:

#ifdef PROCFS_ZOMBIE
		if (p == 0 && doingzomb == 0) {
			doingzomb = 1;
			p = zombproc.lh_first;
			goto again;
		}
#endif

		break;

	    }

	default:
		error = ENOTDIR;
		break;
	}

	uio->uio_offset = i * UIO_MX;

	return (error);
}

/*
 * readlink reads the link of `curproc'
 */
procfs_readlink(ap)
	struct vop_readlink_args *ap;
{
	struct uio *uio = ap->a_uio;
	char buf[16];		/* should be enough */
	int len;

	if (VTOPFS(ap->a_vp)->pfs_fileno != PROCFS_FILENO(0, Pcurproc))
		return (EINVAL);

	len = sprintf(buf, "%ld", (long)curproc->p_pid);

	return (uiomove((caddr_t)buf, len, ap->a_uio));
}

/*
 * convert decimal ascii to pid_t
 */
static pid_t
atopid(b, len)
	const char *b;
	u_int len;
{
	pid_t p = 0;

	while (len--) {
		char c = *b++;
		if (c < '0' || c > '9')
			return (NO_PID);
		p = 10 * p + (c - '0');
		if (p > PID_MAX)
			return (NO_PID);
	}

	return (p);
}

/*
 * procfs vnode operations.
 */
int (**procfs_vnodeop_p)();
struct vnodeopv_entry_desc procfs_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, procfs_lookup },		/* lookup */
	{ &vop_create_desc, procfs_create },		/* create */
	{ &vop_mknod_desc, procfs_mknod },		/* mknod */
	{ &vop_open_desc, procfs_open },		/* open */
	{ &vop_close_desc, procfs_close },		/* close */
	{ &vop_access_desc, procfs_access },		/* access */
	{ &vop_getattr_desc, procfs_getattr },		/* getattr */
	{ &vop_setattr_desc, procfs_setattr },		/* setattr */
	{ &vop_read_desc, procfs_read },		/* read */
	{ &vop_write_desc, procfs_write },		/* write */
	{ &vop_ioctl_desc, procfs_ioctl },		/* ioctl */
	{ &vop_select_desc, procfs_select },		/* select */
	{ &vop_mmap_desc, procfs_mmap },		/* mmap */
	{ &vop_revoke_desc, procfs_revoke },		/* revoke */
	{ &vop_fsync_desc, procfs_fsync },		/* fsync */
	{ &vop_seek_desc, procfs_seek },		/* seek */
	{ &vop_remove_desc, procfs_remove },		/* remove */
	{ &vop_link_desc, procfs_link },		/* link */
	{ &vop_rename_desc, procfs_rename },		/* rename */
	{ &vop_mkdir_desc, procfs_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, procfs_rmdir },		/* rmdir */
	{ &vop_symlink_desc, procfs_symlink },		/* symlink */
	{ &vop_readdir_desc, procfs_readdir },		/* readdir */
	{ &vop_readlink_desc, procfs_readlink },	/* readlink */
	{ &vop_abortop_desc, procfs_abortop },		/* abortop */
	{ &vop_inactive_desc, procfs_inactive },	/* inactive */
	{ &vop_reclaim_desc, procfs_reclaim },		/* reclaim */
	{ &vop_lock_desc, procfs_lock },		/* lock */
	{ &vop_unlock_desc, procfs_unlock },		/* unlock */
	{ &vop_bmap_desc, procfs_bmap },		/* bmap */
	{ &vop_strategy_desc, procfs_strategy },	/* strategy */
	{ &vop_print_desc, procfs_print },		/* print */
	{ &vop_islocked_desc, procfs_islocked },	/* islocked */
	{ &vop_pathconf_desc, procfs_pathconf },	/* pathconf */
	{ &vop_advlock_desc, procfs_advlock },		/* advlock */
	{ &vop_blkatoff_desc, procfs_blkatoff },	/* blkatoff */
	{ &vop_valloc_desc, procfs_valloc },		/* valloc */
	{ &vop_vfree_desc, procfs_vfree },		/* vfree */
	{ &vop_truncate_desc, procfs_truncate },	/* truncate */
	{ &vop_update_desc, procfs_update },		/* update */
	{ (struct vnodeop_desc*)NULL, (int(*)())NULL }
};
struct vnodeopv_desc procfs_vnodeop_opv_desc =
	{ &procfs_vnodeop_p, procfs_vnodeop_entries };
