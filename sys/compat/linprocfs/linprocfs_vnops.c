/*
 * Copyright (c) 2001 Jonathan Lemon <jlemon@freebsd.org>
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
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/pioctl.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/sx.h>
#include <sys/time.h>
#include <sys/vnode.h>

#include <machine/reg.h>

#include <vm/vm_zone.h>

#include <compat/linprocfs/linprocfs.h>

extern struct vnode *procfs_findtextvp __P((struct proc *));
extern int	procfs_kmemaccess __P((struct proc *));

static int	linprocfs_access __P((struct vop_access_args *));
static int	linprocfs_badop __P((void));
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
static pid_t 	atopid __P((const char *, u_int));

#define N(s) s, sizeof(s)-1
#define A(s) linprocfs_ ## s
#define D(s) (node_action_t *)(s)

struct node_data net_dir[] = {
	{ N("."),	VDIR, 0555, 0,    D(net_dir)		},
	{ N(".."),	VDIR, 0555, 0,    D(root_dir)		},
	{ N("dev"), 	VREG, 0444, 0,    A(donetdev)		},
	{ N(""),	VNON, 0000, 0,    NULL			},
};

struct node_data proc_dir[] = {
	{ N("."),	VDIR, 0555, PDEP, D(proc_dir)		},
	{ N(".."),	VDIR, 0555, 0,    D(root_dir)	 	},
	{ N("cmdline"), VREG, 0444, PDEP, procfs_docmdline 	},
	{ N("exe"),	VLNK, 0444, PDEP, A(doexelink) 		},
	{ N("mem"),	VREG, 0600, PDEP, procfs_domem 		},
	{ N("stat"),	VREG, 0444, PDEP, A(doprocstat) 	},
	{ N("status"),	VREG, 0444, PDEP, A(doprocstatus) 	},
	{ N(""),	VNON, 0000, 0,    NULL 			},
};

struct node_data root_dir[] = {
	{ N("."),	VDIR, 0555, 0,    D(root_dir)		},
	{ N(".."),	VDIR, 0555, 0,    NULL			},
	{ N("cmdline"), VREG, 0444, 0,    A(docmdline)		},
	{ N("cpuinfo"), VREG, 0444, 0,    A(docpuinfo)		},
	{ N("devices"), VREG, 0444, 0,    A(dodevices)		},
	{ N("meminfo"),	VREG, 0444, 0,    A(domeminfo)		},
	{ N("net"),	VDIR, 0555, 0,    D(net_dir) 		},
	{ N("self"),	VLNK, 0444, 0,    A(doselflink)		},
	{ N("stat"),	VREG, 0444, 0,    A(dostat)		},
	{ N("uptime"),	VREG, 0444, 0,    A(douptime)		},
	{ N("version"),	VREG, 0444, 0,    A(doversion)		},
	{ N(""),	VNON, 0000, 0,    NULL			},
};

#undef N
#undef A

static int vn2ft[] = {
	DT_UNKNOWN,	/* VNON */
	DT_REG,		/* VREG */
	DT_DIR,		/* VDIR */
	DT_BLK,		/* VBLK */
	DT_CHR,		/* VCHR */
	DT_LNK,		/* VLNK */
	DT_SOCK,	/* VSOCK */
	DT_FIFO,	/* VFIFO */
	DT_UNKNOWN,	/* VBAD */
};

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
	struct node_data *nd = pfs->pfs_nd;
	struct proc *p1, *p2 = NULL;

	if (nd->nd_flags & PDEP) {
		p2 = PFIND(pfs->pfs_pid);
		if (p2 == NULL)
			return (ENOENT);
		if (pfs->pfs_pid && p_can(ap->a_p, p2, P_CAN_SEE, NULL)) {
			PROC_UNLOCK(p2);
			return (ENOENT);
		}
		PROC_UNLOCK(p2);
	}

	if (nd->nd_action == procfs_domem) {
		if (((pfs->pfs_flags & FWRITE) && (ap->a_mode & O_EXCL)) ||
		    ((pfs->pfs_flags & O_EXCL) && (ap->a_mode & FWRITE)))
			return (EBUSY);

		p1 = ap->a_p;
		if (p_can(p1, p2, P_CAN_DEBUG, NULL) && !procfs_kmemaccess(p1))
			return (EPERM);

		if (ap->a_mode & FWRITE)
			pfs->pfs_flags = ap->a_mode & (FWRITE|O_EXCL);
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

	if (pfs->pfs_nd->nd_action == procfs_domem) {
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
		if ((ap->a_vp->v_usecount < 2) && (p = PFIND(pfs->pfs_pid))) {
			if (!(p->p_pfsflags & PF_LINGER)) {
				p->p_stops = 0;
				p->p_step = 0;
				wakeup(&p->p_step);
			}
			PROC_UNLOCK(p);
		}
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
	procp = PFIND(pfs->pfs_pid);
	if (procp == NULL)
		return ENOTTY;

	if ((error = p_can(p, procp, P_CAN_DEBUG, NULL))) {
		PROC_UNLOCK(procp);
		return (error == ESRCH ? ENOENT : error);
	}

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
	  if (flags & NFLAGS && (error = suser(p))) {
	    PROC_UNLOCK(procp);
	    return error;
	  }
	  procp->p_pfsflags = flags;
	  break;
	case PIOCGFL:
	  *(unsigned int*)ap->a_data = (unsigned int)procp->p_pfsflags;
	  /* FALLTHROUGH */
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
	    error = msleep(&procp->p_stype, &procp->p_mtx, PWAIT | PCATCH,
	      "piocwait", 0);
	    if (error) {
	      PROC_UNLOCK(procp);
	      return error;
	    }
	  }
	  psp->state = 1;	/* It stopped */
	  psp->flags = procp->p_pfsflags;
	  psp->events = procp->p_stops;
	  psp->why = procp->p_stype;	/* why it stopped */
	  psp->val = procp->p_xstat;	/* any extra info */
	  break;
	case PIOCCONT:	/* Restart a proc */
	  if (procp->p_step == 0) {
	    PROC_UNLOCK(procp);
	    return EINVAL;	/* Can only start a stopped process */
	  }
	  if ((signo = *(int*)ap->a_data) != 0) {
	    if (signo >= NSIG || signo <= 0) {
	      PROC_UNLOCK(procp);
	      return EINVAL;
	    }
	    psignal(procp, signo);
	  }
	  procp->p_step = 0;
	  wakeup(&procp->p_step);
	  break;
	default:
	  PROC_UNLOCK(procp);
	  return (ENOTTY);
	}
	PROC_UNLOCK(procp);
	return 0;
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
	struct node_data *nd = pfs->pfs_nd;

	printf("tag VT_PROCFS, name %s, pid %ld, mode %x, flags %lx\n",
	    nd->nd_name, (long)pfs->pfs_pid, nd->nd_mode, pfs->pfs_flags);
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
	struct node_data *nd = pfs->pfs_nd;
	struct vattr *vap = ap->a_vap;
	struct proc *procp;

	/*
	 * First make sure that the process and its credentials 
	 * still exist.
	 */
	if (nd->nd_flags & PDEP) {
		procp = PFIND(pfs->pfs_pid);
		if (procp == NULL)
			return (ENOENT);
		if (procp->p_cred == NULL || procp->p_ucred == NULL) {
			PROC_UNLOCK(procp);
			return (ENOENT);
		}
		if (p_can(ap->a_p, procp, P_CAN_SEE, NULL)) {
			PROC_UNLOCK(procp);
			return (ENOENT);
		}
		PROC_UNLOCK(procp);
	} else {
		procp = NULL;
	}

	/* start by zeroing out the attributes */
	VATTR_NULL(vap);

	/* next do all the common fields */
	vap->va_type = ap->a_vp->v_type;
	vap->va_mode = nd->nd_mode;
	vap->va_fileid = pfs->pfs_fileno;
	vap->va_flags = 0;
	vap->va_blocksize = PAGE_SIZE;
	vap->va_bytes = vap->va_size = 0;
	vap->va_fsid = ap->a_vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_nlink = 1;

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
	 * now do the object specific fields
	 *
	 * The size could be set from struct reg, but it's hardly
	 * worth the trouble, and it puts some (potentially) machine
	 * dependent data into this machine-independent code.  If it
	 * becomes important then this function should break out into
	 * a per-file stat function in the corresponding .c file.
	 */
	if (procp) {
		PROC_LOCK(procp);
		vap->va_uid = procp->p_ucred->cr_uid;
		vap->va_gid = procp->p_ucred->cr_gid;
		PROC_UNLOCK(procp);
	} else {
		vap->va_uid = 0;
		vap->va_gid = 0;
	}

	/*
	 * If the process has exercised some setuid or setgid
	 * privilege, then change the owner to root.
	 */
	if (nd->nd_action == procfs_domem) {
		PROC_LOCK(procp);
		if (procp->p_flag & P_SUGID)
			vap->va_uid = 0;
		PROC_UNLOCK(procp);
	}

	return (0);
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

	if (pfs->pfs_nd->nd_flags & PDEP) {
		procp = PFIND(pfs->pfs_pid);
		if (procp == NULL)
			return (ENOENT);
		if (p_can(ap->a_p, procp, P_CAN_SEE, NULL)) {
			PROC_UNLOCK(procp);
			return (ENOENT);
		}
		PROC_UNLOCK(procp);
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
	struct node_data *nd;
	pid_t pid;
	struct pfsnode *pfs;
	struct proc *p;
	int error;

	*vpp = NULL;

	if (cnp->cn_nameiop != LOOKUP)
		return (EROFS);

	if (cnp->cn_namelen == 1 && *pname == '.') {
		*vpp = dvp;
		VREF(dvp);
		/* vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY, curp); */
		return (0);
	}

	pfs = VTOPFS(dvp);
	nd = pfs->pfs_nd;
	if (nd->nd_type != VDIR)
		return (ENOTDIR);

	if (cnp->cn_flags & ISDOTDOT) {
		if (nd == root_dir)
			return (EIO);
		nd = (struct node_data *)nd[1].nd_action;
		return (linprocfs_allocvp(dvp->v_mount, vpp, pfs->pfs_pid, nd));
	}

	/* generic lookup */
	for (nd = pfs->pfs_nd; nd->nd_type != VNON; nd++) {
		if (cnp->cn_namelen != nd->nd_namlen ||
		    memcmp(pname, nd->nd_name, nd->nd_namlen))
			continue;
		if (nd->nd_type == VDIR)
			nd = (struct node_data *)nd->nd_action;
		return (linprocfs_allocvp(dvp->v_mount, vpp, pfs->pfs_pid, nd));
	}

	error = ENOENT;

	/* directory specific lookups */
	if (pfs->pfs_nd == root_dir) {
		pid = atopid(pname, cnp->cn_namelen);
		if (pid == NO_PID)
			goto done;

		p = PFIND(pid);
		if (p == NULL)
			goto done;

		if (p_can(curp, p, P_CAN_SEE, NULL)) {
			PROC_UNLOCK(p);
			goto done;
		}
		PROC_UNLOCK(p);

		error = linprocfs_allocvp(dvp->v_mount, vpp, pid, proc_dir);
	}
done:
	return (error);
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
	struct node_data *nd;
	struct proc *p;
	int i, start, copied, error, off;
	static u_int delen;

	if (!delen) {
		d.d_namlen = PROCFS_NAMELEN;
		delen = GENERIC_DIRSIZ(&d);
	}

	pfs = VTOPFS(ap->a_vp);
	nd = pfs->pfs_nd;

	if (nd->nd_type != VDIR)
		return (ENOTDIR);

	off = (int)uio->uio_offset;
	if (off != uio->uio_offset || off < 0 || 
	    off % delen != 0 || uio->uio_resid < delen)
		return (EINVAL);

	error = 0;
	copied = 0;
	start = off / delen;

	if (nd->nd_flags & PDEP) {
		p = PFIND(pfs->pfs_pid);
		if (p == NULL)
			goto done;
		if (p_can(curproc, p, P_CAN_SEE, NULL)) {
			PROC_UNLOCK(p);
			goto done;
		}
		PROC_UNLOCK(p);
	}

	/*
	 * copy out static entries
	 */
	for (i = 0; i < start && nd->nd_type != VNON; nd++, i++);
	for (; uio->uio_resid >= delen && nd->nd_type != VNON; nd++, copied++) {

		dp->d_reclen = delen;
		dp->d_fileno = PROCFS_FILENO(nd, pfs->pfs_pid);
		dp->d_namlen = nd->nd_namlen;
		memcpy(dp->d_name, nd->nd_name, nd->nd_namlen + 1);
		dp->d_type = vn2ft[nd->nd_type];

		error = uiomove((caddr_t)dp, delen, uio);
		if (error)
			goto done;
	}

	/*
	 * this is for the root of the procfs filesystem
	 */
	if (pfs->pfs_nd == root_dir) {
		sx_slock(&allproc_lock);
		p = LIST_FIRST(&allproc);
		i = nd - pfs->pfs_nd;

		while (p && i < start) {
			if (p_can(curproc, p, P_CAN_SEE, NULL) == 0)
				i++;
			p = LIST_NEXT(p, p_list);
		}
		for (; p && uio->uio_resid >= delen; p = LIST_NEXT(p, p_list)) {
			if (p_can(curproc, p, P_CAN_SEE, NULL))
				continue;
			dp->d_reclen = delen;
			dp->d_fileno = PROCFS_FILENO(proc_dir, p->p_pid);
			dp->d_namlen = sprintf(dp->d_name, "%ld",
			    (long)p->p_pid);
			dp->d_type = DT_DIR;

			error = uiomove((caddr_t)dp, delen, uio);
			if (error) 
				break;
			copied++;
		}
		sx_sunlock(&allproc_lock);
	}
done:
	return (error);
}

/*
 * readlink reads the link of `self' or `exe'
 */
static int
linprocfs_readlink(ap)
	struct vop_readlink_args *ap;
{
	struct pfsnode *pfs = VTOPFS(ap->a_vp);
	struct node_data *nd = pfs->pfs_nd;

	/* sanity check */
	if (nd->nd_type != VLNK)
		return (EINVAL);

	return (nd->nd_action(NULL, NULL, pfs, ap->a_uio));
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
