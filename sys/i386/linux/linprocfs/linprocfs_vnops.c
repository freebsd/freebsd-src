/*
 * Copyright (c) 2000 Dag-Erling Coïdan Smørgrav
 * Copyright (c) 1999 Pierre Beyssac
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
 * $FreeBSD$
 */

/*
 * procfs vnode interface
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/fcntl.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/malloc.h>
#include <machine/reg.h>
#include <vm/vm_zone.h>
#include <i386/linux/linprocfs/linprocfs.h>
#include <sys/pioctl.h>

extern struct vnode *procfs_findtextvp __P((struct proc *));
extern int	procfs_kmemaccess __P((struct proc *));

static int	linprocfs_access __P((struct vop_access_args *));
static int	linprocfs_badop __P((void));
static int	linprocfs_bmap __P((struct vop_bmap_args *));
static int	linprocfs_close __P((struct vop_close_args *));
static int	linprocfs_getattr __P((struct vop_getattr_args *));
static int	linprocfs_inactive __P((struct vop_inactive_args *));
static int	linprocfs_ioctl __P((struct vop_ioctl_args *));
static int	linprocfs_lookup __P((struct vop_lookup_args *));
static int	linprocfs_open __P((struct vop_open_args *));
static int	linprocfs_print __P((struct vop_print_args *));
static int	linprocfs_readdir __P((struct vop_readdir_args *));
static int	linprocfs_readlink __P((struct vop_readlink_args *));
static int	linprocfs_reclaim __P((struct vop_reclaim_args *));
static int	linprocfs_setattr __P((struct vop_setattr_args *));

/*
 * This is a list of the valid names in the
 * process-specific sub-directories.  It is
 * used in linprocfs_lookup and linprocfs_readdir
 */
static struct proc_target {
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
	{ DT_REG, N("mem"),	Pmem,		NULL },
	{ DT_LNK, N("exe"),	Pexe,		NULL },
#undef N
};
static const int nproc_targets = sizeof(proc_targets) / sizeof(proc_targets[0]);

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
static int
linprocfs_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct pfsnode *pfs = VTOPFS(ap->a_vp);
	struct proc *p1, *p2;

	p2 = PFIND(pfs->pfs_pid);
	if (p2 == NULL)
		return (ENOENT);
	if (pfs->pfs_pid && p_can(ap->a_p, p2, P_CAN_SEE, NULL))
		return (ENOENT);

	switch (pfs->pfs_type) {
	case Pmem:
		if (((pfs->pfs_flags & FWRITE) && (ap->a_mode & O_EXCL)) ||
		    ((pfs->pfs_flags & O_EXCL) && (ap->a_mode & FWRITE)))
			return (EBUSY);

		p1 = ap->a_p;
		if (p_can(p1, p2, P_CAN_DEBUG, NULL) &&
		    !procfs_kmemaccess(p1))
			return (EPERM);

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
static int
linprocfs_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct pfsnode *pfs = VTOPFS(ap->a_vp);
	struct proc *p;

	switch (pfs->pfs_type) {
	case Pmem:
		if ((ap->a_fflag & FWRITE) && (pfs->pfs_flags & O_EXCL))
			pfs->pfs_flags &= ~(FWRITE|O_EXCL);
		/*
		 * If this is the last close, then it checks to see if
		 * the target process has PF_LINGER set in p_pfsflags,
		 * if this is *not* the case, then the process' stop flags
		 * are cleared, and the process is woken up.  This is
		 * to help prevent the case where a process has been
		 * told to stop on an event, but then the requesting process
		 * has gone away or forgotten about it.
		 */
		if ((ap->a_vp->v_usecount < 2)
		    && (p = pfind(pfs->pfs_pid))
		    && !(p->p_pfsflags & PF_LINGER)) {
			p->p_stops = 0;
			p->p_step = 0;
			wakeup(&p->p_step);
		}
		break;
	default:
		break;
	}

	return (0);
}

/*
 * do an ioctl operation on a pfsnode (vp).
 * (vp) is not locked on entry or exit.
 */
static int
linprocfs_ioctl(ap)
	struct vop_ioctl_args *ap;
{
	struct pfsnode *pfs = VTOPFS(ap->a_vp);
	struct proc *procp, *p;
	int error;
	int signo;
	struct procfs_status *psp;
	unsigned char flags;

	p = ap->a_p;
	procp = pfind(pfs->pfs_pid);
	if (procp == NULL)
		return ENOTTY;

	if ((error = p_can(p, procp, P_CAN_DEBUG, NULL)))
		return (error == ESRCH ? ENOENT : error);

	switch (ap->a_command) {
	case PIOCBIS:
	  procp->p_stops |= *(unsigned int*)ap->a_data;
	  break;
	case PIOCBIC:
	  procp->p_stops &= ~*(unsigned int*)ap->a_data;
	  break;
	case PIOCSFL:
	  /*
	   * NFLAGS is "non-suser_xxx flags" -- currently, only
	   * PFS_ISUGID ("ignore set u/g id");
	   */
#define NFLAGS	(PF_ISUGID)
	  flags = (unsigned char)*(unsigned int*)ap->a_data;
	  if (flags & NFLAGS && (error = suser(p)))
	    return error;
	  procp->p_pfsflags = flags;
	  break;
	case PIOCGFL:
	  *(unsigned int*)ap->a_data = (unsigned int)procp->p_pfsflags;
	case PIOCSTATUS:
	  psp = (struct procfs_status *)ap->a_data;
	  psp->state = (procp->p_step == 0);
	  psp->flags = procp->p_pfsflags;
	  psp->events = procp->p_stops;
	  if (procp->p_step) {
	    psp->why = procp->p_stype;
	    psp->val = procp->p_xstat;
	  } else {
	    psp->why = psp->val = 0;	/* Not defined values */
	  }
	  break;
	case PIOCWAIT:
	  psp = (struct procfs_status *)ap->a_data;
	  if (procp->p_step == 0) {
	    error = tsleep(&procp->p_stype, PWAIT | PCATCH, "piocwait", 0);
	    if (error)
	      return error;
	  }
	  psp->state = 1;	/* It stopped */
	  psp->flags = procp->p_pfsflags;
	  psp->events = procp->p_stops;
	  psp->why = procp->p_stype;	/* why it stopped */
	  psp->val = procp->p_xstat;	/* any extra info */
	  break;
	case PIOCCONT:	/* Restart a proc */
	  if (procp->p_step == 0)
	    return EINVAL;	/* Can only start a stopped process */
	  if ((signo = *(int*)ap->a_data) != 0) {
	    if (signo >= NSIG || signo <= 0)
	      return EINVAL;
	    psignal(procp, signo);
	  }
	  procp->p_step = 0;
	  wakeup(&procp->p_step);
	  break;
	default:
	  return (ENOTTY);
	}
	return 0;
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
static int
linprocfs_bmap(ap)
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
 * linprocfs_inactive is called when the pfsnode
 * is vrele'd and the reference count goes
 * to zero.  (vp) will be on the vnode free
 * list, so to get it back vget() must be
 * used.
 *
 * (vp) is locked on entry, but must be unlocked on exit.
 */
static int
linprocfs_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	VOP_UNLOCK(vp, 0, ap->a_p);

	return (0);
}

/*
 * _reclaim is called when getnewvnode()
 * wants to make use of an entry on the vnode
 * free list.  at this time the filesystem needs
 * to free any private data and remove the node
 * from any private lists.
 */
static int
linprocfs_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
	} */ *ap;
{

	return (linprocfs_freevp(ap->a_vp));
}

/*
 * _print is used for debugging.
 * just print a readable description
 * of (vp).
 */
static int
linprocfs_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct pfsnode *pfs = VTOPFS(ap->a_vp);

	printf("tag VT_PROCFS, type %d, pid %ld, mode %x, flags %lx\n",
	    pfs->pfs_type, (long)pfs->pfs_pid, pfs->pfs_mode, pfs->pfs_flags);
	return (0);
}

/*
 * generic entry point for unsupported operations
 */
static int
linprocfs_badop()
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
static int
linprocfs_getattr(ap)
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
	int error;

	/*
	 * First make sure that the process and its credentials 
	 * still exist.
	 */
	switch (pfs->pfs_type) {
	case Proot:
	case Pself:
		procp = 0;
		break;

	default:
		procp = PFIND(pfs->pfs_pid);
		if (procp == 0 || procp->p_cred == NULL ||
		    procp->p_ucred == NULL)
			return (ENOENT);

		if (p_can(ap->a_p, procp, P_CAN_SEE, NULL))
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
	vap->va_fsid = ap->a_vp->v_mount->mnt_stat.f_fsid.val[0];

	/*
	 * Make all times be current TOD.
	 * It would be possible to get the process start
	 * time from the p_stat structure, but there's
	 * no "file creation" time stamp anyway, and the
	 * p_stat structure is not addressible if u. gets
	 * swapped out for that process.
	 */
	nanotime(&vap->va_ctime);
	vap->va_atime = vap->va_mtime = vap->va_ctime;

	/*
	 * If the process has exercised some setuid or setgid
	 * privilege, then rip away read/write permission so
	 * that only root can gain access.
	 */
	switch (pfs->pfs_type) {
	case Pmem:
		/* Retain group kmem readablity. */
		if (procp->p_flag & P_SUGID)
			vap->va_mode &= ~(VREAD|VWRITE);
		break;
	default:
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

	vap->va_nlink = 1;
	if (procp) {
		vap->va_uid = procp->p_ucred->cr_uid;
		vap->va_gid = procp->p_ucred->cr_gid;
	}

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

	case Pself: {
		char buf[16];		/* should be enough */
		vap->va_uid = 0;
		vap->va_gid = 0;
		vap->va_size = vap->va_bytes =
		    snprintf(buf, sizeof(buf), "%ld", (long)curproc->p_pid);
		break;
	}

	case Pproc:
		vap->va_nlink = nproc_targets;
		vap->va_size = vap->va_bytes = DEV_BSIZE;
		break;

	case Pexe: {
		char *fullpath, *freepath;
		error = textvp_fullpath(procp, &fullpath, &freepath);
		if (error == 0) {
			vap->va_size = strlen(fullpath);
			free(freepath, M_TEMP);
		} else {
			vap->va_size = sizeof("unknown") - 1;
			error = 0;
		}
		vap->va_bytes = vap->va_size;
		break;
	}

	case Pmeminfo:
	case Pcpuinfo:
	case Pstat:
	case Puptime:
	case Pversion:
		vap->va_bytes = vap->va_size = 0;
		vap->va_uid = 0;
		vap->va_gid = 0;
		break;
		
	case Pmem:
		/*
		 * If we denied owner access earlier, then we have to
		 * change the owner to root - otherwise 'ps' and friends
		 * will break even though they are setgid kmem. *SIGH*
		 */
		if (procp->p_flag & P_SUGID)
			vap->va_uid = 0;
		else
			vap->va_uid = procp->p_ucred->cr_uid;
		vap->va_gid = KMEM_GROUP;
		break;

	default:
		panic("linprocfs_getattr");
	}

	return (error);
}

static int
linprocfs_setattr(ap)
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{

	if (ap->a_vap->va_flags != VNOVAL)
		return (EOPNOTSUPP);

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
 * actually, the check for super-user is slightly
 * broken since it will allow read access to write-only
 * objects.  this doesn't cause any particular trouble
 * but does mean that the i/o entry points need to check
 * that the operation really does make sense.
 */
static int
linprocfs_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct pfsnode *pfs = VTOPFS(ap->a_vp);
	struct vnode *vp = ap->a_vp;
	struct proc *procp;
	struct vattr *vap;
	struct vattr vattr;
	int error;

	switch (pfs->pfs_type) {
	case Proot:
	case Pself:
	case Pmeminfo:
	case Pcpuinfo:
		break;
	default:
		procp = PFIND(pfs->pfs_pid);
		if (procp == NULL)
			return (ENOENT);
		if (p_can(ap->a_p, procp, P_CAN_SEE, NULL))
			return (ENOENT);
	}

	vap = &vattr;
	error = VOP_GETATTR(vp, vap, ap->a_cred, ap->a_p);
	if (error)
		return (error);

	return (vaccess(vp->v_type, vap->va_mode, vap->va_uid, vap->va_gid,
	    ap->a_mode, ap->a_cred, NULL));
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
static int
linprocfs_lookup(ap)
	struct vop_lookup_args /* {
		struct vnode * a_dvp;
		struct vnode ** a_vpp;
		struct componentname * a_cnp;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	struct proc *curp = cnp->cn_proc;
	struct vnode **vpp = ap->a_vpp;
	struct vnode *dvp = ap->a_dvp;
	char *pname = cnp->cn_nameptr;
	struct proc_target *pt;
	pid_t pid;
	struct pfsnode *pfs;
	struct proc *p;
	int i;

	*vpp = NULL;

	if (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME ||
	    cnp->cn_nameiop == CREATE)
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

		if (CNEQ(cnp, "self", 4))
			return (linprocfs_allocvp(dvp->v_mount, vpp, 0, Pself));
		if (CNEQ(cnp, "meminfo", 7))
			return (linprocfs_allocvp(dvp->v_mount, vpp, 0, Pmeminfo));
		if (CNEQ(cnp, "cpuinfo", 7))
			return (linprocfs_allocvp(dvp->v_mount, vpp, 0, Pcpuinfo));
		if (CNEQ(cnp, "stat", 4))
			return (linprocfs_allocvp(dvp->v_mount, vpp, 0, Pstat));
		if (CNEQ(cnp, "uptime", 6))
			return (linprocfs_allocvp(dvp->v_mount, vpp, 0, Puptime));
		if (CNEQ(cnp, "version", 7))
			return (linprocfs_allocvp(dvp->v_mount, vpp, 0, Pversion));

		pid = atopid(pname, cnp->cn_namelen);
		if (pid == NO_PID)
			break;

		p = PFIND(pid);
		if (p == 0)
			break;

		if (p_can(curp, p, P_CAN_SEE, NULL))
			break;

		return (linprocfs_allocvp(dvp->v_mount, vpp, pid, Pproc));

	case Pproc:
		if (cnp->cn_flags & ISDOTDOT)
			return (linprocfs_root(dvp->v_mount, vpp));

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
		return (linprocfs_allocvp(dvp->v_mount, vpp, pfs->pfs_pid,
		    pt->pt_pfstype));

	default:
		return (ENOTDIR);
	}

	return (cnp->cn_nameiop == LOOKUP ? ENOENT : EROFS);
}

/*
 * Does this process have a text file?
 */
int
linprocfs_validfile(p)
	struct proc *p;
{

	return (procfs_findtextvp(p) != NULLVP);
}

/*
 * readdir() returns directory entries from pfsnode (vp).
 *
 * We generate just one directory entry at a time, as it would probably
 * not pay off to buffer several entries locally to save uiomove calls.
 */
static int
linprocfs_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		int *a_ncookies;
		u_long **a_cookies;
	} */ *ap;
{
	struct uio *uio = ap->a_uio;
	struct dirent d;
	struct dirent *dp = &d;
	struct pfsnode *pfs;
	int count, error, i, off;
	static u_int delen;

	if (!delen) {

		d.d_namlen = PROCFS_NAMELEN;
		delen = GENERIC_DIRSIZ(&d);
	}

	pfs = VTOPFS(ap->a_vp);

	off = (int)uio->uio_offset;
	if (off != uio->uio_offset || off < 0 || 
	    off % delen != 0 || uio->uio_resid < delen)
		return (EINVAL);

	error = 0;
	count = 0;
	i = off / delen;

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
		if (p_can(curproc, p, P_CAN_SEE, NULL))
			break;

		for (pt = &proc_targets[i];
		     uio->uio_resid >= delen && i < nproc_targets; pt++, i++) {
			if (pt->pt_valid && (*pt->pt_valid)(p) == 0)
				continue;

			dp->d_reclen = delen;
			dp->d_fileno = PROCFS_FILENO(pfs->pfs_pid, pt->pt_pfstype);
			dp->d_namlen = pt->pt_namlen;
			bcopy(pt->pt_name, dp->d_name, pt->pt_namlen + 1);
			dp->d_type = pt->pt_type;

			if ((error = uiomove((caddr_t)dp, delen, uio)) != 0)
				break;
		}

	    	break;
	    }

	/*
	 * this is for the root of the procfs filesystem
	 * what is needed is a special entry for "self"
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
		struct proc *p = allproc.lh_first;

		for (; p && uio->uio_resid >= delen; i++, pcnt++) {
			bzero((char *) dp, delen);
			dp->d_reclen = delen;

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
				dp->d_fileno = PROCFS_FILENO(0, Pself);
				dp->d_namlen = 4;
				bcopy("self", dp->d_name, 5);
				dp->d_type = DT_LNK;
				break;

			case 3:
				dp->d_fileno = PROCFS_FILENO(0, Pmeminfo);
				dp->d_namlen = 7;
				bcopy("meminfo", dp->d_name, 8);
				dp->d_type = DT_REG;
				break;

			case 4:
				dp->d_fileno = PROCFS_FILENO(0, Pcpuinfo);
				dp->d_namlen = 7;
				bcopy("cpuinfo", dp->d_name, 8);
				dp->d_type = DT_REG;
				break;

			case 5:
				dp->d_fileno = PROCFS_FILENO(0, Puptime);
				dp->d_namlen = 4;
				bcopy("stat", dp->d_name, 5);
				dp->d_type = DT_REG;
				break;
			    
			case 6:
				dp->d_fileno = PROCFS_FILENO(0, Puptime);
				dp->d_namlen = 6;
				bcopy("uptime", dp->d_name, 7);
				dp->d_type = DT_REG;
				break;

			case 7:
				dp->d_fileno = PROCFS_FILENO(0, Pversion);
				dp->d_namlen = 7;
				bcopy("version", dp->d_name, 8);
				dp->d_type = DT_REG;
				break;

			default:
				while (pcnt < i) {
					p = p->p_list.le_next;
					if (!p)
						goto done;
					if (p_can(curproc, p, P_CAN_SEE, NULL))
						continue;
					pcnt++;
				}
				while (p_can(curproc, p, P_CAN_SEE, NULL)) {
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

			if ((error = uiomove((caddr_t)dp, delen, uio)) != 0)
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

	uio->uio_offset = i * delen;

	return (error);
}

/*
 * readlink reads the link of `self' or `exe'
 */
static int
linprocfs_readlink(ap)
	struct vop_readlink_args *ap;
{
	char buf[16];		/* should be enough */
	struct proc *procp;
	struct vnode *vp = ap->a_vp;
	struct pfsnode *pfs = VTOPFS(vp);
	char *fullpath, *freepath;
	int error, len;

	switch (pfs->pfs_type) {
	case Pself:
		if (pfs->pfs_fileno != PROCFS_FILENO(0, Pself))
			return (EINVAL);

		len = snprintf(buf, sizeof(buf), "%ld", (long)curproc->p_pid);

		return (uiomove(buf, len, ap->a_uio));
	/*
	 * There _should_ be no way for an entire process to disappear
	 * from under us...
	 */
	case Pexe:
		procp = PFIND(pfs->pfs_pid);
		if (procp == NULL || procp->p_cred == NULL ||
		    procp->p_ucred == NULL) {
			printf("linprocfs_readlink: pid %d disappeared\n",
			    pfs->pfs_pid);
			return (uiomove("unknown", sizeof("unknown") - 1,
			    ap->a_uio));
		}
		error = textvp_fullpath(procp, &fullpath, &freepath);
		if (error != 0)
			return (uiomove("unknown", sizeof("unknown") - 1,
			    ap->a_uio));
		error = uiomove(fullpath, strlen(fullpath), ap->a_uio);
		free(freepath, M_TEMP);
		return (error);
	default:
		return (EINVAL);
	}
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
vop_t **linprocfs_vnodeop_p;
static struct vnodeopv_entry_desc linprocfs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_access_desc,		(vop_t *) linprocfs_access },
	{ &vop_advlock_desc,		(vop_t *) linprocfs_badop },
	{ &vop_bmap_desc,		(vop_t *) linprocfs_bmap },
	{ &vop_close_desc,		(vop_t *) linprocfs_close },
	{ &vop_create_desc,		(vop_t *) linprocfs_badop },
	{ &vop_getattr_desc,		(vop_t *) linprocfs_getattr },
	{ &vop_inactive_desc,		(vop_t *) linprocfs_inactive },
	{ &vop_link_desc,		(vop_t *) linprocfs_badop },
	{ &vop_lookup_desc,		(vop_t *) linprocfs_lookup },
	{ &vop_mkdir_desc,		(vop_t *) linprocfs_badop },
	{ &vop_mknod_desc,		(vop_t *) linprocfs_badop },
	{ &vop_open_desc,		(vop_t *) linprocfs_open },
	{ &vop_pathconf_desc,		(vop_t *) vop_stdpathconf },
	{ &vop_print_desc,		(vop_t *) linprocfs_print },
	{ &vop_read_desc,		(vop_t *) linprocfs_rw },
	{ &vop_readdir_desc,		(vop_t *) linprocfs_readdir },
	{ &vop_readlink_desc,		(vop_t *) linprocfs_readlink },
	{ &vop_reclaim_desc,		(vop_t *) linprocfs_reclaim },
	{ &vop_remove_desc,		(vop_t *) linprocfs_badop },
	{ &vop_rename_desc,		(vop_t *) linprocfs_badop },
	{ &vop_rmdir_desc,		(vop_t *) linprocfs_badop },
	{ &vop_setattr_desc,		(vop_t *) linprocfs_setattr },
	{ &vop_symlink_desc,		(vop_t *) linprocfs_badop },
	{ &vop_write_desc,		(vop_t *) linprocfs_rw },
	{ &vop_ioctl_desc,		(vop_t *) linprocfs_ioctl },
	{ NULL, NULL }
};
static struct vnodeopv_desc linprocfs_vnodeop_opv_desc =
	{ &linprocfs_vnodeop_p, linprocfs_vnodeop_entries };

VNODEOP_SET(linprocfs_vnodeop_opv_desc);
