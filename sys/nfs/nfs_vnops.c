/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
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
 *	@(#)nfs_vnops.c	8.5 (Berkeley) 2/13/94
 * $Id: nfs_vnops.c,v 1.15.4.2 1995/10/26 09:17:36 davidg Exp $
 */

/*
 * vnode op calls for sun nfs version 2
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/conf.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/lockf.h>

#include <vm/vm.h>

#include <miscfs/specfs/specdev.h>
#include <miscfs/fifofs/fifo.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsv2.h>
#include <nfs/nfs.h>
#include <nfs/nfsnode.h>
#include <nfs/nfsmount.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfsm_subs.h>
#include <nfs/nqnfs.h>

/*
 * Prototypes for NFS vnode operations
 */
static int	nfs_lookup __P((struct vop_lookup_args *));
static int	nfs_create __P((struct vop_create_args *));
static int	nfs_mknod __P((struct vop_mknod_args *));
static int	nfs_open __P((struct vop_open_args *));
static int	nfs_close __P((struct vop_close_args *));
static int	nfsspec_close __P((struct vop_close_args *));
static int	nfsfifo_close __P((struct vop_close_args *));
static int	nfs_access __P((struct vop_access_args *));
static int	nfsspec_access __P((struct vop_access_args *));
static int	nfs_getattr __P((struct vop_getattr_args *));
static int	nfs_setattr __P((struct vop_setattr_args *));
static int	nfs_read __P((struct vop_read_args *));
static int	nfsspec_read __P((struct vop_read_args *));
static int	nfsspec_write __P((struct vop_write_args *));
static int	nfsfifo_read __P((struct vop_read_args *));
static int	nfsfifo_write __P((struct vop_write_args *));
#define nfs_ioctl ((int (*) __P((struct  vop_ioctl_args *)))enoioctl)
#define nfs_select ((int (*) __P((struct  vop_select_args *)))seltrue)
static int	nfs_mmap __P((struct vop_mmap_args *));
static int	nfs_fsync __P((struct vop_fsync_args *));
#define nfs_seek ((int (*) __P((struct  vop_seek_args *)))nullop)
static int	nfs_remove __P((struct vop_remove_args *));
static int	nfs_link __P((struct vop_link_args *));
static int	nfs_rename __P((struct vop_rename_args *));
static int	nfs_mkdir __P((struct vop_mkdir_args *));
static int	nfs_rmdir __P((struct vop_rmdir_args *));
static int	nfs_symlink __P((struct vop_symlink_args *));
static int	nfs_readdir __P((struct vop_readdir_args *));
static int	nfs_readlink __P((struct vop_readlink_args *));
static int	nfs_bmap __P((struct vop_bmap_args *));
static int	nfs_strategy __P((struct vop_strategy_args *));
static int	nfs_print __P((struct vop_print_args *));
static int	nfs_pathconf __P((struct vop_pathconf_args *));
static int	nfs_advlock __P((struct vop_advlock_args *));
static int	nfs_blkatoff __P((struct vop_blkatoff_args *));
static int	nfs_valloc __P((struct vop_valloc_args *));
#define nfs_reallocblks \
	((int (*) __P((struct  vop_reallocblks_args *)))eopnotsupp)
static int	nfs_vfree __P((struct vop_vfree_args *));
static int	nfs_truncate __P((struct vop_truncate_args *));
static int	nfs_update __P((struct vop_update_args *));
static int	nfs_bwrite __P((struct vop_bwrite_args *));


/* Defs */
#define	TRUE	1
#define	FALSE	0

/*
 * Global vfs data structures for nfs
 */
int (**nfsv2_vnodeop_p)();
static struct vnodeopv_entry_desc nfsv2_vnodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, nfs_lookup },	/* lookup */
	{ &vop_create_desc, nfs_create },	/* create */
	{ &vop_mknod_desc, nfs_mknod },		/* mknod */
	{ &vop_open_desc, nfs_open },		/* open */
	{ &vop_close_desc, nfs_close },		/* close */
	{ &vop_access_desc, nfs_access },	/* access */
	{ &vop_getattr_desc, nfs_getattr },	/* getattr */
	{ &vop_setattr_desc, nfs_setattr },	/* setattr */
	{ &vop_read_desc, nfs_read },		/* read */
	{ &vop_write_desc, nfs_write },		/* write */
	{ &vop_ioctl_desc, nfs_ioctl },		/* ioctl */
	{ &vop_select_desc, nfs_select },	/* select */
	{ &vop_mmap_desc, nfs_mmap },		/* mmap */
	{ &vop_fsync_desc, nfs_fsync },		/* fsync */
	{ &vop_seek_desc, nfs_seek },		/* seek */
	{ &vop_remove_desc, nfs_remove },	/* remove */
	{ &vop_link_desc, nfs_link },		/* link */
	{ &vop_rename_desc, nfs_rename },	/* rename */
	{ &vop_mkdir_desc, nfs_mkdir },		/* mkdir */
	{ &vop_rmdir_desc, nfs_rmdir },		/* rmdir */
	{ &vop_symlink_desc, nfs_symlink },	/* symlink */
	{ &vop_readdir_desc, nfs_readdir },	/* readdir */
	{ &vop_readlink_desc, nfs_readlink },	/* readlink */
	{ &vop_abortop_desc, nfs_abortop },	/* abortop */
	{ &vop_inactive_desc, nfs_inactive },	/* inactive */
	{ &vop_reclaim_desc, nfs_reclaim },	/* reclaim */
	{ &vop_lock_desc, nfs_lock },		/* lock */
	{ &vop_unlock_desc, nfs_unlock },	/* unlock */
	{ &vop_bmap_desc, nfs_bmap },		/* bmap */
	{ &vop_strategy_desc, nfs_strategy },	/* strategy */
	{ &vop_print_desc, nfs_print },		/* print */
	{ &vop_islocked_desc, nfs_islocked },	/* islocked */
	{ &vop_pathconf_desc, nfs_pathconf },	/* pathconf */
	{ &vop_advlock_desc, nfs_advlock },	/* advlock */
	{ &vop_blkatoff_desc, nfs_blkatoff },	/* blkatoff */
	{ &vop_valloc_desc, nfs_valloc },	/* valloc */
	{ &vop_reallocblks_desc, nfs_reallocblks },	/* reallocblks */
	{ &vop_vfree_desc, nfs_vfree },		/* vfree */
	{ &vop_truncate_desc, nfs_truncate },	/* truncate */
	{ &vop_update_desc, nfs_update },	/* update */
	{ &vop_bwrite_desc, vn_bwrite },
	{ (struct vnodeop_desc*)NULL, (int(*)())NULL }
};
static struct vnodeopv_desc nfsv2_vnodeop_opv_desc =
	{ &nfsv2_vnodeop_p, nfsv2_vnodeop_entries };
VNODEOP_SET(nfsv2_vnodeop_opv_desc);

/*
 * Special device vnode ops
 */
int (**spec_nfsv2nodeop_p)();
static struct vnodeopv_entry_desc spec_nfsv2nodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, spec_lookup },	/* lookup */
	{ &vop_create_desc, spec_create },	/* create */
	{ &vop_mknod_desc, spec_mknod },	/* mknod */
	{ &vop_open_desc, spec_open },		/* open */
	{ &vop_close_desc, nfsspec_close },	/* close */
	{ &vop_access_desc, nfsspec_access },	/* access */
	{ &vop_getattr_desc, nfs_getattr },	/* getattr */
	{ &vop_setattr_desc, nfs_setattr },	/* setattr */
	{ &vop_read_desc, nfsspec_read },	/* read */
	{ &vop_write_desc, nfsspec_write },	/* write */
	{ &vop_ioctl_desc, spec_ioctl },	/* ioctl */
	{ &vop_select_desc, spec_select },	/* select */
	{ &vop_mmap_desc, spec_mmap },		/* mmap */
	{ &vop_fsync_desc, nfs_fsync },		/* fsync */
	{ &vop_seek_desc, spec_seek },		/* seek */
	{ &vop_remove_desc, spec_remove },	/* remove */
	{ &vop_link_desc, spec_link },		/* link */
	{ &vop_rename_desc, spec_rename },	/* rename */
	{ &vop_mkdir_desc, spec_mkdir },	/* mkdir */
	{ &vop_rmdir_desc, spec_rmdir },	/* rmdir */
	{ &vop_symlink_desc, spec_symlink },	/* symlink */
	{ &vop_readdir_desc, spec_readdir },	/* readdir */
	{ &vop_readlink_desc, spec_readlink },	/* readlink */
	{ &vop_abortop_desc, spec_abortop },	/* abortop */
	{ &vop_inactive_desc, nfs_inactive },	/* inactive */
	{ &vop_reclaim_desc, nfs_reclaim },	/* reclaim */
	{ &vop_lock_desc, nfs_lock },		/* lock */
	{ &vop_unlock_desc, nfs_unlock },	/* unlock */
	{ &vop_bmap_desc, spec_bmap },		/* bmap */
	{ &vop_strategy_desc, spec_strategy },	/* strategy */
	{ &vop_print_desc, nfs_print },		/* print */
	{ &vop_islocked_desc, nfs_islocked },	/* islocked */
	{ &vop_pathconf_desc, spec_pathconf },	/* pathconf */
	{ &vop_advlock_desc, spec_advlock },	/* advlock */
	{ &vop_blkatoff_desc, spec_blkatoff },	/* blkatoff */
	{ &vop_valloc_desc, spec_valloc },	/* valloc */
	{ &vop_reallocblks_desc, spec_reallocblks },	/* reallocblks */
	{ &vop_vfree_desc, spec_vfree },	/* vfree */
	{ &vop_truncate_desc, spec_truncate },	/* truncate */
	{ &vop_update_desc, nfs_update },	/* update */
	{ &vop_bwrite_desc, vn_bwrite },
	{ (struct vnodeop_desc*)NULL, (int(*)())NULL }
};
static struct vnodeopv_desc spec_nfsv2nodeop_opv_desc =
	{ &spec_nfsv2nodeop_p, spec_nfsv2nodeop_entries };
VNODEOP_SET(spec_nfsv2nodeop_opv_desc);

int (**fifo_nfsv2nodeop_p)();
static struct vnodeopv_entry_desc fifo_nfsv2nodeop_entries[] = {
	{ &vop_default_desc, vn_default_error },
	{ &vop_lookup_desc, fifo_lookup },	/* lookup */
	{ &vop_create_desc, fifo_create },	/* create */
	{ &vop_mknod_desc, fifo_mknod },	/* mknod */
	{ &vop_open_desc, fifo_open },		/* open */
	{ &vop_close_desc, nfsfifo_close },	/* close */
	{ &vop_access_desc, nfsspec_access },	/* access */
	{ &vop_getattr_desc, nfs_getattr },	/* getattr */
	{ &vop_setattr_desc, nfs_setattr },	/* setattr */
	{ &vop_read_desc, nfsfifo_read },	/* read */
	{ &vop_write_desc, nfsfifo_write },	/* write */
	{ &vop_ioctl_desc, fifo_ioctl },	/* ioctl */
	{ &vop_select_desc, fifo_select },	/* select */
	{ &vop_mmap_desc, fifo_mmap },		/* mmap */
	{ &vop_fsync_desc, nfs_fsync },		/* fsync */
	{ &vop_seek_desc, fifo_seek },		/* seek */
	{ &vop_remove_desc, fifo_remove },	/* remove */
	{ &vop_link_desc, fifo_link },		/* link */
	{ &vop_rename_desc, fifo_rename },	/* rename */
	{ &vop_mkdir_desc, fifo_mkdir },	/* mkdir */
	{ &vop_rmdir_desc, fifo_rmdir },	/* rmdir */
	{ &vop_symlink_desc, fifo_symlink },	/* symlink */
	{ &vop_readdir_desc, fifo_readdir },	/* readdir */
	{ &vop_readlink_desc, fifo_readlink },	/* readlink */
	{ &vop_abortop_desc, fifo_abortop },	/* abortop */
	{ &vop_inactive_desc, nfs_inactive },	/* inactive */
	{ &vop_reclaim_desc, nfs_reclaim },	/* reclaim */
	{ &vop_lock_desc, nfs_lock },		/* lock */
	{ &vop_unlock_desc, nfs_unlock },	/* unlock */
	{ &vop_bmap_desc, fifo_bmap },		/* bmap */
	{ &vop_strategy_desc, fifo_badop },	/* strategy */
	{ &vop_print_desc, nfs_print },		/* print */
	{ &vop_islocked_desc, nfs_islocked },	/* islocked */
	{ &vop_pathconf_desc, fifo_pathconf },	/* pathconf */
	{ &vop_advlock_desc, fifo_advlock },	/* advlock */
	{ &vop_blkatoff_desc, fifo_blkatoff },	/* blkatoff */
	{ &vop_valloc_desc, fifo_valloc },	/* valloc */
	{ &vop_reallocblks_desc, fifo_reallocblks },	/* reallocblks */
	{ &vop_vfree_desc, fifo_vfree },	/* vfree */
	{ &vop_truncate_desc, fifo_truncate },	/* truncate */
	{ &vop_update_desc, nfs_update },	/* update */
	{ &vop_bwrite_desc, vn_bwrite },
	{ (struct vnodeop_desc*)NULL, (int(*)())NULL }
};
static struct vnodeopv_desc fifo_nfsv2nodeop_opv_desc =
	{ &fifo_nfsv2nodeop_p, fifo_nfsv2nodeop_entries };
VNODEOP_SET(fifo_nfsv2nodeop_opv_desc);

void nqnfs_clientlease();

/*
 * Global variables
 */
extern u_long nfs_procids[NFS_NPROCS];
extern u_long nfs_prog, nfs_vers, nfs_true, nfs_false;
struct proc *nfs_iodwant[NFS_MAXASYNCDAEMON];
int nfs_numasync = 0;
#define	DIRHDSIZ	(sizeof (struct dirent) - (MAXNAMLEN + 1))

/*
 * nfs null call from vfs.
 */
static int
nfs_null(vp, cred, procp)
	struct vnode *vp;
	struct ucred *cred;
	struct proc *procp;
{
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb;

	nfsm_reqhead(vp, NFSPROC_NULL, 0);
	nfsm_request(vp, NFSPROC_NULL, procp, cred);
	nfsm_reqdone;
	return (error);
}

/*
 * nfs access vnode op.
 * For nfs, just return ok. File accesses may fail later.
 * For nqnfs, use the access rpc to check accessibility. If file modes are
 * changed on the server, accesses might still fail later.
 */
static int
nfs_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register u_long *tl;
	register caddr_t cp;
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;

	/*
	 * Disallow write attempts on filesystems mounted read-only;
	 * unless the file is a socket, fifo, or a block or character
	 * device resident on the filesystem.
	 */
	if ((ap->a_mode & VWRITE) && (vp->v_mount->mnt_flag & MNT_RDONLY)) {
		switch (vp->v_type) {
		case VREG: case VDIR: case VLNK:
			return (EROFS);
		}
	}
	/*
	 * For nqnfs, do an access rpc, otherwise you are stuck emulating
	 * ufs_access() locally using the vattr. This may not be correct,
	 * since the server may apply other access criteria such as
	 * client uid-->server uid mapping that we do not know about, but
	 * this is better than just returning anything that is lying about
	 * in the cache.
	 */
	if (VFSTONFS(vp->v_mount)->nm_flag & NFSMNT_NQNFS) {
		nfsstats.rpccnt[NQNFSPROC_ACCESS]++;
		nfsm_reqhead(vp, NQNFSPROC_ACCESS, NFSX_FH + 3 * NFSX_UNSIGNED);
		nfsm_fhtom(vp);
		nfsm_build(tl, u_long *, 3 * NFSX_UNSIGNED);
		if (ap->a_mode & VREAD)
			*tl++ = nfs_true;
		else
			*tl++ = nfs_false;
		if (ap->a_mode & VWRITE)
			*tl++ = nfs_true;
		else
			*tl++ = nfs_false;
		if (ap->a_mode & VEXEC)
			*tl = nfs_true;
		else
			*tl = nfs_false;
		nfsm_request(vp, NQNFSPROC_ACCESS, ap->a_p, ap->a_cred);
		nfsm_reqdone;
		return (error);
	} else
		return (nfsspec_access(ap));
}

/*
 * nfs open vnode op
 * Check to see if the type is ok
 * and that deletion is not in progress.
 * For paged in text files, you will need to flush the page cache
 * if consistency is lost.
 */
/* ARGSUSED */
static int
nfs_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	struct nfsnode *np = VTONFS(vp);
	struct nfsmount *nmp = VFSTONFS(vp->v_mount);
	struct vattr vattr;
	int error;

	if (vp->v_type != VREG && vp->v_type != VDIR && vp->v_type != VLNK)
		return (EACCES);
	/*
	 * Get a valid lease. If cached data is stale, flush it.
	 */
	if (nmp->nm_flag & NFSMNT_NQNFS) {
		if (NQNFS_CKINVALID(vp, np, NQL_READ)) {
		    do {
			error = nqnfs_getlease(vp, NQL_READ, ap->a_cred, ap->a_p);
		    } while (error == NQNFS_EXPIRED);
		    if (error)
			return (error);
		    if (np->n_lrev != np->n_brev ||
			(np->n_flag & NQNFSNONCACHE)) {
			if ((error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred,
				ap->a_p, 1)) == EINTR)
				return (error);
			np->n_brev = np->n_lrev;
		    }
		}
	} else {
		if (np->n_flag & NMODIFIED) {
			if ((error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred,
				ap->a_p, 1)) == EINTR)
				return (error);
			np->n_attrstamp = 0;
			np->n_direofoffset = 0;
			error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
			if (error)
				return (error);
			np->n_mtime = vattr.va_mtime.ts_sec;
		} else {
			error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
			if (error)
				return (error);
			if (np->n_mtime != vattr.va_mtime.ts_sec) {
				np->n_direofoffset = 0;
				if ((error = nfs_vinvalbuf(vp, V_SAVE,
					ap->a_cred, ap->a_p, 1)) == EINTR)
					return (error);
				np->n_mtime = vattr.va_mtime.ts_sec;
			}
		}
	}
	if ((nmp->nm_flag & NFSMNT_NQNFS) == 0)
		np->n_attrstamp = 0; /* For Open/Close consistency */
	return (0);
}

/*
 * nfs close vnode op
 * For reg files, invalidate any buffer cache entries.
 */
/* ARGSUSED */
static int
nfs_close(ap)
	struct vop_close_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct nfsnode *np = VTONFS(vp);
	int error = 0;

	if (vp->v_type == VREG) {
	    if ((VFSTONFS(vp->v_mount)->nm_flag & NFSMNT_NQNFS) == 0 &&
		(np->n_flag & NMODIFIED)) {
		error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p, 1);
		np->n_attrstamp = 0;
	    }
	    if (np->n_flag & NWRITEERR) {
		np->n_flag &= ~NWRITEERR;
		error = np->n_error;
	    }
	}
	return (error);
}

/*
 * nfs getattr call from vfs.
 */
static int
nfs_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct nfsnode *np = VTONFS(vp);
	register caddr_t cp;
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;

	/*
	 * Update local times for special files.
	 */
	if (np->n_flag & (NACC | NUPD))
		np->n_flag |= NCHG;
	/*
	 * First look in the cache.
	 */
	if (nfs_getattrcache(vp, ap->a_vap) == 0)
		return (0);
	nfsstats.rpccnt[NFSPROC_GETATTR]++;
	nfsm_reqhead(vp, NFSPROC_GETATTR, NFSX_FH);
	nfsm_fhtom(vp);
	nfsm_request(vp, NFSPROC_GETATTR, ap->a_p, ap->a_cred);
	nfsm_loadattr(vp, ap->a_vap);
	nfsm_reqdone;
	return (error);
}

/*
 * nfs setattr call.
 */
static int
nfs_setattr(ap)
	struct vop_setattr_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	register struct nfsv2_sattr *sp;
	register caddr_t cp;
	register long t1;
	caddr_t bpos, dpos, cp2;
	u_long *tl;
	int error = 0, isnq;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	register struct vnode *vp = ap->a_vp;
	register struct nfsnode *np = VTONFS(vp);
	register struct vattr *vap = ap->a_vap;
	u_quad_t frev, tsize = 0;

	/*
	 * Disallow write attempts if the filesystem is mounted read-only.
	 */
	if ((vap->va_flags != VNOVAL || vap->va_uid != (uid_t)VNOVAL ||
	    vap->va_gid != (gid_t)VNOVAL || vap->va_atime.ts_sec != VNOVAL ||
	    vap->va_mtime.ts_sec != VNOVAL || vap->va_mode != (mode_t)VNOVAL) &&
	    (vp->v_mount->mnt_flag & MNT_RDONLY))
		return (EROFS);
	if (vap->va_size != VNOVAL) {
		switch (vp->v_type) {
		case VDIR:
			return (EISDIR);
		case VCHR:
		case VBLK:
			if (vap->va_mtime.ts_sec == VNOVAL &&
			    vap->va_atime.ts_sec == VNOVAL &&
			    vap->va_mode == (u_short)VNOVAL &&
			    vap->va_uid == VNOVAL &&
			    vap->va_gid == VNOVAL)
				return (0);
			vap->va_size = VNOVAL;
			break;
		default:
			/*
			 * Disallow write attempts if the filesystem is
			 * mounted read-only.
			 */
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
			if (np->n_flag & NMODIFIED) {
				error = nfs_vinvalbuf(vp,
					vap->va_size ? V_SAVE : 0,
					ap->a_cred, ap->a_p, 1);
				if (error)
					return (error);
			}
			tsize = np->n_size;
			np->n_size = np->n_vattr.va_size = vap->va_size;
			vnode_pager_setsize(vp, (u_long)np->n_size);
		}
	} else if ((vap->va_mtime.ts_sec != VNOVAL ||
	    vap->va_atime.ts_sec != VNOVAL) && (np->n_flag & NMODIFIED)) {
		error = nfs_vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p, 1);
		if (error == EINTR)
			return (error);
	}
	nfsstats.rpccnt[NFSPROC_SETATTR]++;
	isnq = (VFSTONFS(vp->v_mount)->nm_flag & NFSMNT_NQNFS);
	nfsm_reqhead(vp, NFSPROC_SETATTR, NFSX_FH+NFSX_SATTR(isnq));
	nfsm_fhtom(vp);
	nfsm_build(sp, struct nfsv2_sattr *, NFSX_SATTR(isnq));
	if (vap->va_mode == (u_short)-1)
		sp->sa_mode = VNOVAL;
	else
		sp->sa_mode = vtonfs_mode(vp->v_type, vap->va_mode);
	if (vap->va_uid == (uid_t)-1)
		sp->sa_uid = VNOVAL;
	else
		sp->sa_uid = txdr_unsigned(vap->va_uid);
	if (vap->va_gid == (gid_t)-1)
		sp->sa_gid = VNOVAL;
	else
		sp->sa_gid = txdr_unsigned(vap->va_gid);
	if (isnq) {
		txdr_hyper(&vap->va_size, &sp->sa_nqsize);
		txdr_nqtime(&vap->va_atime, &sp->sa_nqatime);
		txdr_nqtime(&vap->va_mtime, &sp->sa_nqmtime);
		sp->sa_nqflags = txdr_unsigned(vap->va_flags);
		sp->sa_nqrdev = VNOVAL;
	} else {
		sp->sa_nfssize = txdr_unsigned(vap->va_size);
		txdr_nfstime(&vap->va_atime, &sp->sa_nfsatime);
		txdr_nfstime(&vap->va_mtime, &sp->sa_nfsmtime);
	}
	nfsm_request(vp, NFSPROC_SETATTR, ap->a_p, ap->a_cred);
	nfsm_loadattr(vp, (struct vattr *)0);
	if ((VFSTONFS(vp->v_mount)->nm_flag & NFSMNT_NQNFS) &&
	    NQNFS_CKCACHABLE(vp, NQL_WRITE)) {
		nfsm_dissect(tl, u_long *, 2*NFSX_UNSIGNED);
		fxdr_hyper(tl, &frev);
		if (frev > np->n_brev)
			np->n_brev = frev;
	}
	nfsm_reqdone;
	if (error && vap->va_size != VNOVAL) {
		np->n_size = np->n_vattr.va_size = tsize;
		vnode_pager_setsize(vp, (u_long)np->n_size);
	}
	return (error);
}

/*
 * nfs lookup call, one step at a time...
 * First look in cache
 * If not found, unlock the directory nfsnode and do the rpc
 */
static int
nfs_lookup(ap)
	struct vop_lookup_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	register struct componentname *cnp = ap->a_cnp;
	register struct vnode *dvp = ap->a_dvp;
	register struct vnode **vpp = ap->a_vpp;
	register int flags = cnp->cn_flags;
	register struct vnode *vdp;
	register u_long *tl;
	register caddr_t cp;
	register long t1, t2;
	struct nfsmount *nmp;
	caddr_t bpos, dpos, cp2;
	time_t reqtime = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct vnode *newvp;
	long len;
	nfsv2fh_t *fhp;
	struct nfsnode *np;
	int lockparent, wantparent, error = 0;
	int nqlflag = 0, cachable = 0;
	u_quad_t frev;

	if ((flags & ISLASTCN) && (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);
	*vpp = NULL;
	if (dvp->v_type != VDIR)
		return (ENOTDIR);
	lockparent = flags & LOCKPARENT;
	wantparent = flags & (LOCKPARENT|WANTPARENT);
	nmp = VFSTONFS(dvp->v_mount);
	np = VTONFS(dvp);
	if ((error = cache_lookup(dvp, vpp, cnp)) && error != ENOENT) {
		struct vattr vattr;
		int vpid;

		vdp = *vpp;
		vpid = vdp->v_id;
		/*
		 * See the comment starting `Step through' in ufs/ufs_lookup.c
		 * for an explanation of the locking protocol
		 */
		if (dvp == vdp) {
			VREF(vdp);
			error = 0;
		} else
			error = vget(vdp, 1);
		if (!error) {
			if (vpid == vdp->v_id) {
			   if (nmp->nm_flag & NFSMNT_NQNFS) {
				if ((nmp->nm_flag & NFSMNT_NQLOOKLEASE) == 0) {
					nfsstats.lookupcache_hits++;
					if (cnp->cn_nameiop != LOOKUP &&
					    (flags & ISLASTCN))
					    cnp->cn_flags |= SAVENAME;
					return (0);
			        } else if (NQNFS_CKCACHABLE(dvp, NQL_READ)) {
					if (np->n_lrev != np->n_brev ||
					    (np->n_flag & NMODIFIED)) {
						np->n_direofoffset = 0;
						cache_purge(dvp);
						error = nfs_vinvalbuf(dvp, 0,
						    cnp->cn_cred, cnp->cn_proc,
						    1);
						if (error == EINTR)
							return (error);
						np->n_brev = np->n_lrev;
					} else {
						nfsstats.lookupcache_hits++;
						if (cnp->cn_nameiop != LOOKUP &&
						    (flags & ISLASTCN))
						    cnp->cn_flags |= SAVENAME;
						return (0);
					}
				}
			   } else if (!VOP_GETATTR(vdp, &vattr, cnp->cn_cred, cnp->cn_proc) &&
			       vattr.va_ctime.ts_sec == VTONFS(vdp)->n_ctime) {
				nfsstats.lookupcache_hits++;
				if (cnp->cn_nameiop != LOOKUP &&
				    (flags & ISLASTCN))
					cnp->cn_flags |= SAVENAME;
				return (0);
			   }
			   cache_purge(vdp);
			}
			vrele(vdp);
		}
		*vpp = NULLVP;
	}
	error = 0;
	nfsstats.lookupcache_misses++;
	nfsstats.rpccnt[NFSPROC_LOOKUP]++;
	len = cnp->cn_namelen;
	nfsm_reqhead(dvp, NFSPROC_LOOKUP, NFSX_FH+NFSX_UNSIGNED+nfsm_rndup(len));

	/*
	 * For nqnfs optionally piggyback a getlease request for the name
	 * being looked up.
	 */
	if (nmp->nm_flag & NFSMNT_NQNFS) {
		nfsm_build(tl, u_long *, NFSX_UNSIGNED);
		if ((nmp->nm_flag & NFSMNT_NQLOOKLEASE) &&
		    ((cnp->cn_flags & MAKEENTRY) &&
		    (cnp->cn_nameiop != DELETE || !(flags & ISLASTCN))))
			*tl = txdr_unsigned(nmp->nm_leaseterm);
		else
			*tl = 0;
	}
	nfsm_fhtom(dvp);
	nfsm_strtom(cnp->cn_nameptr, len, NFS_MAXNAMLEN);
	reqtime = time.tv_sec;
	nfsm_request(dvp, NFSPROC_LOOKUP, cnp->cn_proc, cnp->cn_cred);
nfsmout:
	if (error) {
		if ((cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME) &&
		    (flags & ISLASTCN) && error == ENOENT) {
			if (dvp->v_mount->mnt_flag & MNT_RDONLY)
				error = EROFS;
			else
				error = EJUSTRETURN;
		}
		if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
			cnp->cn_flags |= SAVENAME;
		return (error);
	}
	if (nmp->nm_flag & NFSMNT_NQNFS) {
		nfsm_dissect(tl, u_long *, NFSX_UNSIGNED);
		if (*tl) {
			nqlflag = fxdr_unsigned(int, *tl);
			nfsm_dissect(tl, u_long *, 4*NFSX_UNSIGNED);
			cachable = fxdr_unsigned(int, *tl++);
			reqtime += fxdr_unsigned(int, *tl++);
			fxdr_hyper(tl, &frev);
		} else
			nqlflag = 0;
	}
	nfsm_dissect(fhp, nfsv2fh_t *, NFSX_FH);

	/*
	 * Handle RENAME case...
	 */
	if (cnp->cn_nameiop == RENAME && wantparent && (flags & ISLASTCN)) {
		if (!bcmp(np->n_fh.fh_bytes, (caddr_t)fhp, NFSX_FH)) {
			m_freem(mrep);
			return (EISDIR);
		}
		error = nfs_nget(dvp->v_mount, fhp, &np);
		if (error) {
			m_freem(mrep);
			return (error);
		}
		newvp = NFSTOV(np);
		error = nfs_loadattrcache(&newvp, &md, &dpos, (struct vattr*)0);
		if (error) {
			vrele(newvp);
			m_freem(mrep);
			return (error);
		}
		*vpp = newvp;
		m_freem(mrep);
		cnp->cn_flags |= SAVENAME;
		return (0);
	}

	if (!bcmp(np->n_fh.fh_bytes, (caddr_t)fhp, NFSX_FH)) {
		VREF(dvp);
		newvp = dvp;
	} else {
		error = nfs_nget(dvp->v_mount, fhp, &np);
		if (error) {
			m_freem(mrep);
			return (error);
		}
		newvp = NFSTOV(np);
	}
	error = nfs_loadattrcache(&newvp, &md, &dpos, (struct vattr *)0);
	if (error) {
		vrele(newvp);
		m_freem(mrep);
		return (error);
	}
	m_freem(mrep);
	*vpp = newvp;
	if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
		cnp->cn_flags |= SAVENAME;
	if ((cnp->cn_flags & MAKEENTRY) &&
	    (cnp->cn_nameiop != DELETE || !(flags & ISLASTCN))) {
		if ((nmp->nm_flag & NFSMNT_NQNFS) == 0)
			np->n_ctime = np->n_vattr.va_ctime.ts_sec;
		else if (nqlflag && reqtime > time.tv_sec)
			nqnfs_clientlease(nmp, np, nqlflag, cachable, reqtime,
				frev);
		cache_enter(dvp, *vpp, cnp);
	}
	return (0);
}

/*
 * nfs read call.
 * Just call nfs_bioread() to do the work.
 */
static int
nfs_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;

	if (vp->v_type != VREG)
		return (EPERM);
	return (nfs_bioread(vp, ap->a_uio, ap->a_ioflag, ap->a_cred));
}

/*
 * nfs readlink call
 */
static int
nfs_readlink(ap)
	struct vop_readlink_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;

	if (vp->v_type != VLNK)
		return (EPERM);
	return (nfs_bioread(vp, ap->a_uio, 0, ap->a_cred));
}

/*
 * Do a readlink rpc.
 * Called by nfs_doio() from below the buffer cache.
 */
int
nfs_readlinkrpc(vp, uiop, cred)
	register struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
{
	register u_long *tl;
	register caddr_t cp;
	register long t1;
	caddr_t bpos, dpos, cp2;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	long len;

	nfsstats.rpccnt[NFSPROC_READLINK]++;
	nfsm_reqhead(vp, NFSPROC_READLINK, NFSX_FH);
	nfsm_fhtom(vp);
	nfsm_request(vp, NFSPROC_READLINK, uiop->uio_procp, cred);
	nfsm_strsiz(len, NFS_MAXPATHLEN);
	nfsm_mtouio(uiop, len);
	nfsm_reqdone;
	return (error);
}

/*
 * nfs read rpc call
 * Ditto above
 */
int
nfs_readrpc(vp, uiop, cred)
	register struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
{
	register u_long *tl;
	register caddr_t cp;
	register long t1;
	caddr_t bpos, dpos, cp2;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct nfsmount *nmp;
	long len, retlen, tsiz;

	nmp = VFSTONFS(vp->v_mount);
	tsiz = uiop->uio_resid;
	if (uiop->uio_offset + tsiz > 0xffffffff &&
	    (nmp->nm_flag & NFSMNT_NQNFS) == 0)
		return (EFBIG);
	while (tsiz > 0) {
		nfsstats.rpccnt[NFSPROC_READ]++;
		len = (tsiz > nmp->nm_rsize) ? nmp->nm_rsize : tsiz;
		nfsm_reqhead(vp, NFSPROC_READ, NFSX_FH+NFSX_UNSIGNED*3);
		nfsm_fhtom(vp);
		nfsm_build(tl, u_long *, NFSX_UNSIGNED*3);
		if (nmp->nm_flag & NFSMNT_NQNFS) {
			txdr_hyper(&uiop->uio_offset, tl);
			*(tl + 2) = txdr_unsigned(len);
		} else {
			*tl++ = txdr_unsigned(uiop->uio_offset);
			*tl++ = txdr_unsigned(len);
			*tl = 0;
		}
		nfsm_request(vp, NFSPROC_READ, uiop->uio_procp, cred);
		nfsm_loadattr(vp, (struct vattr *)0);
		nfsm_strsiz(retlen, nmp->nm_rsize);
		nfsm_mtouio(uiop, retlen);
		m_freem(mrep);
		if (retlen < len)
			tsiz = 0;
		else
			tsiz -= len;
	}
nfsmout:
	return (error);
}

/*
 * nfs write call
 */
int
nfs_writerpc(vp, uiop, cred, ioflags)
	register struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
	int ioflags;
{
	register u_long *tl;
	register caddr_t cp;
	register long t1;
	caddr_t bpos, dpos, cp2;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct nfsmount *nmp;
	struct nfsnode *np = VTONFS(vp);
	u_quad_t frev;
	long len, tsiz;

	nmp = VFSTONFS(vp->v_mount);
	tsiz = uiop->uio_resid;
	if (uiop->uio_offset + tsiz > 0xffffffff &&
	    (nmp->nm_flag & NFSMNT_NQNFS) == 0)
		return (EFBIG);
	while (tsiz > 0) {
		nfsstats.rpccnt[NFSPROC_WRITE]++;
		len = (tsiz > nmp->nm_wsize) ? nmp->nm_wsize : tsiz;
		nfsm_reqhead(vp, NFSPROC_WRITE,
			NFSX_FH+NFSX_UNSIGNED*4+nfsm_rndup(len));
		nfsm_fhtom(vp);
		nfsm_build(tl, u_long *, NFSX_UNSIGNED * 4);
		if (nmp->nm_flag & NFSMNT_NQNFS) {
			txdr_hyper(&uiop->uio_offset, tl);
			tl += 2;
#ifdef notyet
			if (ioflags & IO_APPEND)
				*tl++ = txdr_unsigned(1);
			else
#endif
				*tl++ = 0;
		} else {
			*++tl = txdr_unsigned(uiop->uio_offset);
			tl += 2;
		}
		*tl = txdr_unsigned(len);
		nfsm_uiotom(uiop, len);
		nfsm_request(vp, NFSPROC_WRITE, uiop->uio_procp, cred);
		nfsm_loadattr(vp, (struct vattr *)0);
		if (nmp->nm_flag & NFSMNT_MYWRITE)
			VTONFS(vp)->n_mtime = VTONFS(vp)->n_vattr.va_mtime.ts_sec;
		else if ((nmp->nm_flag & NFSMNT_NQNFS) &&
			 NQNFS_CKCACHABLE(vp, NQL_WRITE)) {
			nfsm_dissect(tl, u_long *, 2*NFSX_UNSIGNED);
			fxdr_hyper(tl, &frev);
			if (frev > np->n_brev)
				np->n_brev = frev;
		}
		m_freem(mrep);
		tsiz -= len;
	}
nfsmout:
	if (error)
		uiop->uio_resid = tsiz;
	return (error);
}

/*
 * nfs mknod call
 * This is a kludge. Use a create rpc but with the IFMT bits of the mode
 * set to specify the file type and the size field for rdev.
 */
/* ARGSUSED */
static int
nfs_mknod(ap)
	struct vop_mknod_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	register struct vnode *dvp = ap->a_dvp;
	register struct vattr *vap = ap->a_vap;
	register struct componentname *cnp = ap->a_cnp;
	register struct nfsv2_sattr *sp;
	register u_long *tl;
	register caddr_t cp;
	register long t1, t2;
	struct vnode *newvp = 0;
	struct vattr vattr;
	char *cp2;
	caddr_t bpos, dpos;
	int error = 0, isnq;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	u_long rdev;

	isnq = (VFSTONFS(dvp->v_mount)->nm_flag & NFSMNT_NQNFS);
	if (vap->va_type == VCHR || vap->va_type == VBLK)
		rdev = txdr_unsigned(vap->va_rdev);
	else if (vap->va_type == VFIFO)
		rdev = 0xffffffff;
	else {
		VOP_ABORTOP(dvp, cnp);
		vput(dvp);
		return (EOPNOTSUPP);
	}
	error = VOP_GETATTR(dvp, &vattr, cnp->cn_cred, cnp->cn_proc);
	if (error) {
		VOP_ABORTOP(dvp, cnp);
		vput(dvp);
		return (error);
	}
	newvp = NULLVP;
	nfsstats.rpccnt[NFSPROC_CREATE]++;
	nfsm_reqhead(dvp, NFSPROC_CREATE,
	  NFSX_FH+NFSX_UNSIGNED+nfsm_rndup(cnp->cn_namelen)+NFSX_SATTR(isnq));
	nfsm_fhtom(dvp);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	nfsm_build(sp, struct nfsv2_sattr *, NFSX_SATTR(isnq));
	sp->sa_mode = vtonfs_mode(vap->va_type, vap->va_mode);
	sp->sa_uid = txdr_unsigned(cnp->cn_cred->cr_uid);
	sp->sa_gid = txdr_unsigned(vattr.va_gid);
	if (isnq) {
		sp->sa_nqrdev = rdev;
		sp->sa_nqflags = 0;
		txdr_nqtime(&vap->va_atime, &sp->sa_nqatime);
		txdr_nqtime(&vap->va_mtime, &sp->sa_nqmtime);
	} else {
		sp->sa_nfssize = rdev;
		txdr_nfstime(&vap->va_atime, &sp->sa_nfsatime);
		txdr_nfstime(&vap->va_mtime, &sp->sa_nfsmtime);
	}
	nfsm_request(dvp, NFSPROC_CREATE, cnp->cn_proc, cnp->cn_cred);
	nfsm_mtofh(dvp, newvp);
	nfsm_reqdone;
	if (!error && (cnp->cn_flags & MAKEENTRY))
		cache_enter(dvp, newvp, cnp);
	FREE(cnp->cn_pnbuf, M_NAMEI);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	VTONFS(dvp)->n_attrstamp = 0;
	vrele(dvp);
	if (newvp != NULLVP)
		vrele(newvp);
	return (error);
}

/*
 * nfs file create call
 */
static int
nfs_create(ap)
	struct vop_create_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	register struct vnode *dvp = ap->a_dvp;
	register struct vattr *vap = ap->a_vap;
	register struct componentname *cnp = ap->a_cnp;
	register struct nfsv2_sattr *sp;
	register u_long *tl;
	register caddr_t cp;
	register long t1, t2;
	caddr_t bpos, dpos, cp2;
	int error = 0, isnq;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct vattr vattr;

	error = VOP_GETATTR(dvp, &vattr, cnp->cn_cred, cnp->cn_proc);
	if (error) {
		VOP_ABORTOP(dvp, cnp);
		vput(dvp);
		return (error);
	}
	nfsstats.rpccnt[NFSPROC_CREATE]++;
	isnq = (VFSTONFS(dvp->v_mount)->nm_flag & NFSMNT_NQNFS);
	nfsm_reqhead(dvp, NFSPROC_CREATE,
	  NFSX_FH+NFSX_UNSIGNED+nfsm_rndup(cnp->cn_namelen)+NFSX_SATTR(isnq));
	nfsm_fhtom(dvp);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	nfsm_build(sp, struct nfsv2_sattr *, NFSX_SATTR(isnq));
	sp->sa_mode = vtonfs_mode(vap->va_type, vap->va_mode);
	sp->sa_uid = txdr_unsigned(cnp->cn_cred->cr_uid);
	sp->sa_gid = txdr_unsigned(vattr.va_gid);
	if (isnq) {
		u_quad_t qval = 0;

		txdr_hyper(&qval, &sp->sa_nqsize);
		sp->sa_nqflags = 0;
		sp->sa_nqrdev = -1;
		txdr_nqtime(&vap->va_atime, &sp->sa_nqatime);
		txdr_nqtime(&vap->va_mtime, &sp->sa_nqmtime);
	} else {
		sp->sa_nfssize = 0;
		txdr_nfstime(&vap->va_atime, &sp->sa_nfsatime);
		txdr_nfstime(&vap->va_mtime, &sp->sa_nfsmtime);
	}
	nfsm_request(dvp, NFSPROC_CREATE, cnp->cn_proc, cnp->cn_cred);
	nfsm_mtofh(dvp, *ap->a_vpp);
	nfsm_reqdone;
	if (!error && (cnp->cn_flags & MAKEENTRY))
		cache_enter(dvp, *ap->a_vpp, cnp);
	FREE(cnp->cn_pnbuf, M_NAMEI);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	VTONFS(dvp)->n_attrstamp = 0;
	vrele(dvp);
	return (error);
}

/*
 * nfs file remove call
 * To try and make nfs semantics closer to ufs semantics, a file that has
 * other processes using the vnode is renamed instead of removed and then
 * removed later on the last close.
 * - If v_usecount > 1
 *	  If a rename is not already in the works
 *	     call nfs_sillyrename() to set it up
 *     else
 *	  do the remove rpc
 */
static int
nfs_remove(ap)
	struct vop_remove_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_dvp;
		struct vnode * a_vp;
		struct componentname * a_cnp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct vnode *dvp = ap->a_dvp;
	register struct componentname *cnp = ap->a_cnp;
	register struct nfsnode *np = VTONFS(vp);
	register u_long *tl;
	register caddr_t cp;
	register long t2;
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct vattr vattr;

	if (vp->v_usecount > 1) {
		if (!np->n_sillyrename)
			error = nfs_sillyrename(dvp, vp, cnp);
		else if (VOP_GETATTR(vp, &vattr, cnp->cn_cred, cnp->cn_proc)
			 == 0 && vattr.va_nlink > 1)
			/*
			 * If we already have a silly name but there are more
			 * than one links, just proceed with the NFS remove
			 * request, as the bits will remain available (modulo
			 * network races). This avoids silently ignoring the
			 * attempted removal of a non-silly entry.
			 */
			goto doit;
	} else {
	doit:
		/*
		 * Purge the name cache so that the chance of a lookup for
		 * the name succeeding while the remove is in progress is
		 * minimized. Without node locking it can still happen, such
		 * that an I/O op returns ESTALE, but since you get this if
		 * another host removes the file..
		 */
		cache_purge(vp);
		/*
		 * Throw away biocache buffers. Mainly to avoid
		 * unnecessary delayed writes.
		 */
		error = nfs_vinvalbuf(vp, 0, cnp->cn_cred, cnp->cn_proc, 1);
		if (error == EINTR)
			return (error);
		/* Do the rpc */
		nfsstats.rpccnt[NFSPROC_REMOVE]++;
		nfsm_reqhead(dvp, NFSPROC_REMOVE,
			NFSX_FH+NFSX_UNSIGNED+nfsm_rndup(cnp->cn_namelen));
		nfsm_fhtom(dvp);
		nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
		nfsm_request(dvp, NFSPROC_REMOVE, cnp->cn_proc, cnp->cn_cred);
		nfsm_reqdone;
		FREE(cnp->cn_pnbuf, M_NAMEI);
		VTONFS(dvp)->n_flag |= NMODIFIED;
		VTONFS(dvp)->n_attrstamp = 0;
		/*
		 * Kludge City: If the first reply to the remove rpc is lost..
		 *   the reply to the retransmitted request will be ENOENT
		 *   since the file was in fact removed
		 *   Therefore, we cheat and return success.
		 */
		if (error == ENOENT)
			error = 0;
	}
	np->n_attrstamp = 0;
	vrele(dvp);
	vrele(vp);
	return (error);
}

/*
 * nfs file remove rpc called from nfs_inactive
 */
int
nfs_removeit(sp)
	register struct sillyrename *sp;
{
	register u_long *tl;
	register caddr_t cp;
	register long t2;
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;

	nfsstats.rpccnt[NFSPROC_REMOVE]++;
	nfsm_reqhead(sp->s_dvp, NFSPROC_REMOVE,
		NFSX_FH+NFSX_UNSIGNED+nfsm_rndup(sp->s_namlen));
	nfsm_fhtom(sp->s_dvp);
	nfsm_strtom(sp->s_name, sp->s_namlen, NFS_MAXNAMLEN);
	nfsm_request(sp->s_dvp, NFSPROC_REMOVE, NULL, sp->s_cred);
	nfsm_reqdone;
	VTONFS(sp->s_dvp)->n_flag |= NMODIFIED;
	VTONFS(sp->s_dvp)->n_attrstamp = 0;
	return (error);
}

/*
 * nfs file rename call
 */
static int
nfs_rename(ap)
	struct vop_rename_args  /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap;
{
	register struct vnode *fvp = ap->a_fvp;
	register struct vnode *tvp = ap->a_tvp;
	register struct vnode *fdvp = ap->a_fdvp;
	register struct vnode *tdvp = ap->a_tdvp;
	register struct componentname *tcnp = ap->a_tcnp;
	register struct componentname *fcnp = ap->a_fcnp;
	register u_long *tl;
	register caddr_t cp;
	register long t2;
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;

	/* Check for cross-device rename */
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
		goto out;
	}


	nfsstats.rpccnt[NFSPROC_RENAME]++;
	nfsm_reqhead(fdvp, NFSPROC_RENAME,
		(NFSX_FH+NFSX_UNSIGNED)*2+nfsm_rndup(fcnp->cn_namelen)+
		nfsm_rndup(fcnp->cn_namelen)); /* or fcnp->cn_cred?*/
	nfsm_fhtom(fdvp);
	nfsm_strtom(fcnp->cn_nameptr, fcnp->cn_namelen, NFS_MAXNAMLEN);
	nfsm_fhtom(tdvp);
	nfsm_strtom(tcnp->cn_nameptr, tcnp->cn_namelen, NFS_MAXNAMLEN);
	nfsm_request(fdvp, NFSPROC_RENAME, tcnp->cn_proc, tcnp->cn_cred);
	nfsm_reqdone;
	VTONFS(fdvp)->n_flag |= NMODIFIED;
	VTONFS(fdvp)->n_attrstamp = 0;
	VTONFS(tdvp)->n_flag |= NMODIFIED;
	VTONFS(tdvp)->n_attrstamp = 0;
	if (fvp->v_type == VDIR) {
		if (tvp != NULL && tvp->v_type == VDIR)
			cache_purge(tdvp);
		cache_purge(fdvp);
	}
out:
	if (tdvp == tvp)
		vrele(tdvp);
	else
		vput(tdvp);
	if (tvp)
		vput(tvp);
	vrele(fdvp);
	vrele(fvp);
	/*
	 * Kludge: Map ENOENT => 0 assuming that it is a reply to a retry.
	 */
	if (error == ENOENT)
		error = 0;
	return (error);
}

/*
 * nfs file rename rpc called from nfs_remove() above
 */
static int
nfs_renameit(sdvp, scnp, sp)
	struct vnode *sdvp;
	struct componentname *scnp;
	register struct sillyrename *sp;
{
	register u_long *tl;
	register caddr_t cp;
	register long t2;
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;

	nfsstats.rpccnt[NFSPROC_RENAME]++;
	nfsm_reqhead(sdvp, NFSPROC_RENAME,
		(NFSX_FH+NFSX_UNSIGNED)*2+nfsm_rndup(scnp->cn_namelen)+
		nfsm_rndup(sp->s_namlen));
	nfsm_fhtom(sdvp);
	nfsm_strtom(scnp->cn_nameptr, scnp->cn_namelen, NFS_MAXNAMLEN);
	nfsm_fhtom(sdvp);
	nfsm_strtom(sp->s_name, sp->s_namlen, NFS_MAXNAMLEN);
	nfsm_request(sdvp, NFSPROC_RENAME, scnp->cn_proc, scnp->cn_cred);
	nfsm_reqdone;
	FREE(scnp->cn_pnbuf, M_NAMEI);
	VTONFS(sdvp)->n_flag |= NMODIFIED;
	VTONFS(sdvp)->n_attrstamp = 0;
	return (error);
}

/*
 * nfs hard link create call
 */
static int
nfs_link(ap)
	struct vop_link_args /* {
		struct vnode *a_vp;
		struct vnode *a_tdvp;
		struct componentname *a_cnp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct vnode *tdvp = ap->a_tdvp;
	register struct componentname *cnp = ap->a_cnp;
	register u_long *tl;
	register caddr_t cp;
	register long t2;
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;

	if (vp->v_mount != tdvp->v_mount) {
		/*VOP_ABORTOP(vp, cnp);*/
		if (tdvp == vp)
			vrele(vp);
		else
			vput(vp);
		return (EXDEV);
	}

	/*
	 * Push all writes to the server, so that the attribute cache
	 * doesn't get "out of sync" with the server.
	 * XXX There should be a better way!
	 */
	VOP_FSYNC(tdvp, cnp->cn_cred, MNT_WAIT, cnp->cn_proc);

	nfsstats.rpccnt[NFSPROC_LINK]++;
	nfsm_reqhead(tdvp, NFSPROC_LINK,
		NFSX_FH*2+NFSX_UNSIGNED+nfsm_rndup(cnp->cn_namelen));
	nfsm_fhtom(tdvp);
	nfsm_fhtom(vp);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	nfsm_request(tdvp, NFSPROC_LINK, cnp->cn_proc, cnp->cn_cred);
	nfsm_reqdone;
	FREE(cnp->cn_pnbuf, M_NAMEI);
	VTONFS(tdvp)->n_attrstamp = 0;
	VTONFS(vp)->n_flag |= NMODIFIED;
	VTONFS(vp)->n_attrstamp = 0;
	vrele(vp);
	/*
	 * Kludge: Map EEXIST => 0 assuming that it is a reply to a retry.
	 */
	if (error == EEXIST)
		error = 0;
	return (error);
}

/*
 * nfs symbolic link create call
 */
/* start here */
static int
nfs_symlink(ap)
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap;
{
	register struct vnode *dvp = ap->a_dvp;
	register struct vattr *vap = ap->a_vap;
	register struct componentname *cnp = ap->a_cnp;
	register struct nfsv2_sattr *sp;
	register u_long *tl;
	register caddr_t cp;
	register long t2;
	caddr_t bpos, dpos;
	int slen, error = 0, isnq;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;

	nfsstats.rpccnt[NFSPROC_SYMLINK]++;
	slen = strlen(ap->a_target);
	isnq = (VFSTONFS(dvp->v_mount)->nm_flag & NFSMNT_NQNFS);
	nfsm_reqhead(dvp, NFSPROC_SYMLINK, NFSX_FH+2*NFSX_UNSIGNED+
	    nfsm_rndup(cnp->cn_namelen)+nfsm_rndup(slen)+NFSX_SATTR(isnq));
	nfsm_fhtom(dvp);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	nfsm_strtom(ap->a_target, slen, NFS_MAXPATHLEN);
	nfsm_build(sp, struct nfsv2_sattr *, NFSX_SATTR(isnq));
	sp->sa_mode = vtonfs_mode(VLNK, vap->va_mode);
	sp->sa_uid = txdr_unsigned(cnp->cn_cred->cr_uid);
	sp->sa_gid = txdr_unsigned(cnp->cn_cred->cr_gid);
	if (isnq) {
		quad_t qval = -1;

		txdr_hyper(&qval, &sp->sa_nqsize);
		sp->sa_nqflags = 0;
		txdr_nqtime(&vap->va_atime, &sp->sa_nqatime);
		txdr_nqtime(&vap->va_mtime, &sp->sa_nqmtime);
	} else {
		sp->sa_nfssize = -1;
		txdr_nfstime(&vap->va_atime, &sp->sa_nfsatime);
		txdr_nfstime(&vap->va_mtime, &sp->sa_nfsmtime);
	}
	nfsm_request(dvp, NFSPROC_SYMLINK, cnp->cn_proc, cnp->cn_cred);
	nfsm_reqdone;
	FREE(cnp->cn_pnbuf, M_NAMEI);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	VTONFS(dvp)->n_attrstamp = 0;
	vrele(dvp);
	/*
	 * Kludge: Map EEXIST => 0 assuming that it is a reply to a retry.
	 */
	if (error == EEXIST)
		error = 0;
	return (error);
}

/*
 * nfs make dir call
 */
static int
nfs_mkdir(ap)
	struct vop_mkdir_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	register struct vnode *dvp = ap->a_dvp;
	register struct vattr *vap = ap->a_vap;
	register struct componentname *cnp = ap->a_cnp;
	register struct vnode **vpp = ap->a_vpp;
	register struct nfsv2_sattr *sp;
	register u_long *tl;
	register caddr_t cp;
	register long t1, t2;
	register int len;
	caddr_t bpos, dpos, cp2;
	int error = 0, firsttry = 1, isnq;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct vattr vattr;

	error = VOP_GETATTR(dvp, &vattr, cnp->cn_cred, cnp->cn_proc);
	if (error) {
		VOP_ABORTOP(dvp, cnp);
		vput(dvp);
		return (error);
	}
	len = cnp->cn_namelen;
	isnq = (VFSTONFS(dvp->v_mount)->nm_flag & NFSMNT_NQNFS);
	nfsstats.rpccnt[NFSPROC_MKDIR]++;
	nfsm_reqhead(dvp, NFSPROC_MKDIR,
	  NFSX_FH+NFSX_UNSIGNED+nfsm_rndup(len)+NFSX_SATTR(isnq));
	nfsm_fhtom(dvp);
	nfsm_strtom(cnp->cn_nameptr, len, NFS_MAXNAMLEN);
	nfsm_build(sp, struct nfsv2_sattr *, NFSX_SATTR(isnq));
	sp->sa_mode = vtonfs_mode(VDIR, vap->va_mode);
	sp->sa_uid = txdr_unsigned(cnp->cn_cred->cr_uid);
	sp->sa_gid = txdr_unsigned(vattr.va_gid);
	if (isnq) {
		quad_t qval = -1;

		txdr_hyper(&qval, &sp->sa_nqsize);
		sp->sa_nqflags = 0;
		txdr_nqtime(&vap->va_atime, &sp->sa_nqatime);
		txdr_nqtime(&vap->va_mtime, &sp->sa_nqmtime);
	} else {
		sp->sa_nfssize = -1;
		txdr_nfstime(&vap->va_atime, &sp->sa_nfsatime);
		txdr_nfstime(&vap->va_mtime, &sp->sa_nfsmtime);
	}
	nfsm_request(dvp, NFSPROC_MKDIR, cnp->cn_proc, cnp->cn_cred);
	nfsm_mtofh(dvp, *vpp);
	nfsm_reqdone;
	VTONFS(dvp)->n_flag |= NMODIFIED;
	VTONFS(dvp)->n_attrstamp = 0;
	/*
	 * Kludge: Map EEXIST => 0 assuming that you have a reply to a retry
	 * if we can succeed in looking up the directory.
	 * "firsttry" is necessary since the macros may "goto nfsmout" which
	 * is above the if on errors. (Ugh)
	 */
	if (error == EEXIST && firsttry) {
		firsttry = 0;
		error = 0;
		nfsstats.rpccnt[NFSPROC_LOOKUP]++;
		*vpp = NULL;
		nfsm_reqhead(dvp, NFSPROC_LOOKUP,
		    NFSX_FH+NFSX_UNSIGNED+nfsm_rndup(len));
		nfsm_fhtom(dvp);
		nfsm_strtom(cnp->cn_nameptr, len, NFS_MAXNAMLEN);
		nfsm_request(dvp, NFSPROC_LOOKUP, cnp->cn_proc, cnp->cn_cred);
		nfsm_mtofh(dvp, *vpp);
		if ((*vpp)->v_type != VDIR) {
			vput(*vpp);
			error = EEXIST;
		}
		m_freem(mrep);
	}
	FREE(cnp->cn_pnbuf, M_NAMEI);
	vrele(dvp);
	return (error);
}

/*
 * nfs remove directory call
 */
static int
nfs_rmdir(ap)
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct vnode *dvp = ap->a_dvp;
	register struct componentname *cnp = ap->a_cnp;
	register u_long *tl;
	register caddr_t cp;
	register long t2;
	caddr_t bpos, dpos;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;

	if (dvp == vp) {
		vrele(dvp);
		vrele(dvp);
		FREE(cnp->cn_pnbuf, M_NAMEI);
		return (EINVAL);
	}
	nfsstats.rpccnt[NFSPROC_RMDIR]++;
	nfsm_reqhead(dvp, NFSPROC_RMDIR,
		NFSX_FH+NFSX_UNSIGNED+nfsm_rndup(cnp->cn_namelen));
	nfsm_fhtom(dvp);
	nfsm_strtom(cnp->cn_nameptr, cnp->cn_namelen, NFS_MAXNAMLEN);
	nfsm_request(dvp, NFSPROC_RMDIR, cnp->cn_proc, cnp->cn_cred);
	nfsm_reqdone;
	FREE(cnp->cn_pnbuf, M_NAMEI);
	VTONFS(dvp)->n_flag |= NMODIFIED;
	VTONFS(dvp)->n_attrstamp = 0;
	cache_purge(dvp);
	cache_purge(vp);
	vrele(vp);
	vrele(dvp);
	/*
	 * Kludge: Map ENOENT => 0 assuming that you have a reply to a retry.
	 */
	if (error == ENOENT)
		error = 0;
	return (error);
}

/*
 * nfs readdir call
 * Although cookie is defined as opaque, I translate it to/from net byte
 * order so that it looks more sensible. This appears consistent with the
 * Ultrix implementation of NFS.
 */
static int
nfs_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct nfsnode *np = VTONFS(vp);
	register struct uio *uio = ap->a_uio;
	int tresid, error;
	struct vattr vattr;

	if (vp->v_type != VDIR)
		return (EPERM);
	/*
	 * First, check for hit on the EOF offset cache
	 */
	if (uio->uio_offset != 0 && uio->uio_offset == np->n_direofoffset &&
	    (np->n_flag & NMODIFIED) == 0) {
		if (VFSTONFS(vp->v_mount)->nm_flag & NFSMNT_NQNFS) {
			if (NQNFS_CKCACHABLE(vp, NQL_READ)) {
				nfsstats.direofcache_hits++;
				return (0);
			}
		} else if (VOP_GETATTR(vp, &vattr, ap->a_cred, uio->uio_procp) == 0 &&
			np->n_mtime == vattr.va_mtime.ts_sec) {
			nfsstats.direofcache_hits++;
			return (0);
		}
	}

	/*
	 * Call nfs_bioread() to do the real work.
	 */
	tresid = uio->uio_resid;
	error = nfs_bioread(vp, uio, 0, ap->a_cred);

	if (!error && uio->uio_resid == tresid)
		nfsstats.direofcache_misses++;
	return (error);
}

/*
 * Readdir rpc call.
 * Called from below the buffer cache by nfs_doio().
 */
int
nfs_readdirrpc(vp, uiop, cred)
	register struct vnode *vp;
	struct uio *uiop;
	struct ucred *cred;
{
	register long len;
	register struct dirent *dp = 0;
	register u_long *tl;
	register caddr_t cp;
	register long t1;
	long tlen, lastlen = 0;
	caddr_t bpos, dpos, cp2;
	int error = 0;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct mbuf *md2;
	caddr_t dpos2;
	int siz;
	int more_dirs = 1;
	u_long off, savoff = 0;
	struct dirent *savdp = 0;
	struct nfsmount *nmp;
	struct nfsnode *np = VTONFS(vp);
	long tresid;

	nmp = VFSTONFS(vp->v_mount);
	tresid = uiop->uio_resid;
	/*
	 * Loop around doing readdir rpc's of size uio_resid or nm_rsize,
	 * whichever is smaller, truncated to a multiple of NFS_DIRBLKSIZ.
	 * The stopping criteria is EOF or buffer full.
	 */
	while (more_dirs && uiop->uio_resid >= NFS_DIRBLKSIZ) {
		nfsstats.rpccnt[NFSPROC_READDIR]++;
		nfsm_reqhead(vp, NFSPROC_READDIR,
			NFSX_FH + 2 * NFSX_UNSIGNED);
		nfsm_fhtom(vp);
		nfsm_build(tl, u_long *, 2 * NFSX_UNSIGNED);
		off = (u_long)uiop->uio_offset;
		*tl++ = txdr_unsigned(off);
		*tl = txdr_unsigned(((uiop->uio_resid > nmp->nm_rsize) ?
			nmp->nm_rsize : uiop->uio_resid) & ~(NFS_DIRBLKSIZ-1));
		nfsm_request(vp, NFSPROC_READDIR, uiop->uio_procp, cred);
		siz = 0;
		nfsm_dissect(tl, u_long *, NFSX_UNSIGNED);
		more_dirs = fxdr_unsigned(int, *tl);

		/* Save the position so that we can do nfsm_mtouio() later */
		dpos2 = dpos;
		md2 = md;

		/* loop thru the dir entries, doctoring them to 4bsd form */
#ifdef lint
		dp = (struct dirent *)0;
#endif /* lint */
		while (more_dirs && siz < uiop->uio_resid) {
			savoff = off;		/* Hold onto offset and dp */
			savdp = dp;
			nfsm_dissect(tl, u_long *, 2 * NFSX_UNSIGNED);
			dp = (struct dirent *)tl;
			dp->d_fileno = fxdr_unsigned(u_long, *tl++);
			len = fxdr_unsigned(int, *tl);
			if (len <= 0 || len > NFS_MAXNAMLEN) {
				error = EBADRPC;
				m_freem(mrep);
				goto nfsmout;
			}
			dp->d_namlen = (u_char)len;
			dp->d_type = DT_UNKNOWN;
			nfsm_adv(len);		/* Point past name */
			tlen = nfsm_rndup(len);
			/*
			 * This should not be necessary, but some servers have
			 * broken XDR such that these bytes are not null filled.
			 */
			if (tlen != len) {
				*dpos = '\0';	/* Null-terminate */
				nfsm_adv(tlen - len);
				len = tlen;
			}
			nfsm_dissect(tl, u_long *, 2 * NFSX_UNSIGNED);
			off = fxdr_unsigned(u_long, *tl);
			*tl++ = 0;	/* Ensures null termination of name */
			more_dirs = fxdr_unsigned(int, *tl);
			dp->d_reclen = len + 4 * NFSX_UNSIGNED;
			siz += dp->d_reclen;
		}
		/*
		 * If at end of rpc data, get the eof boolean
		 */
		if (!more_dirs) {
			nfsm_dissect(tl, u_long *, NFSX_UNSIGNED);
			more_dirs = (fxdr_unsigned(int, *tl) == 0);

			/*
			 * If at EOF, cache directory offset
			 */
			if (!more_dirs)
				np->n_direofoffset = off;
		}
		/*
		 * If there is too much to fit in the data buffer, use savoff and
		 * savdp to trim off the last record.
		 * --> we are not at eof
		 */
		if (siz > uiop->uio_resid) {
			off = savoff;
			siz -= dp->d_reclen;
			dp = savdp;
			more_dirs = 0;	/* Paranoia */
		}
		if (siz > 0) {
			lastlen = dp->d_reclen;
			md = md2;
			dpos = dpos2;
			nfsm_mtouio(uiop, siz);
			uiop->uio_offset = (off_t)off;
		} else
			more_dirs = 0;	/* Ugh, never happens, but in case.. */
		m_freem(mrep);
	}
	/*
	 * Fill last record, iff any, out to a multiple of NFS_DIRBLKSIZ
	 * by increasing d_reclen for the last record.
	 */
	if (uiop->uio_resid < tresid) {
		len = uiop->uio_resid & (NFS_DIRBLKSIZ - 1);
		if (len > 0) {
			dp = (struct dirent *)
				(uiop->uio_iov->iov_base - lastlen);
			dp->d_reclen += len;
			uiop->uio_iov->iov_base += len;
			uiop->uio_iov->iov_len -= len;
			uiop->uio_resid -= len;
		}
	}
nfsmout:
	return (error);
}

/*
 * Nqnfs readdir_and_lookup RPC. Used in place of nfs_readdirrpc().
 */
int
nfs_readdirlookrpc(vp, uiop, cred)
	struct vnode *vp;
	register struct uio *uiop;
	struct ucred *cred;
{
	register int len;
	register struct dirent *dp = 0;
	register u_long *tl;
	register caddr_t cp;
	register long t1;
	caddr_t bpos, dpos, cp2;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	struct nameidata nami, *ndp = &nami;
	struct componentname *cnp = &ndp->ni_cnd;
	u_long off, endoff = 0, fileno;
	time_t reqtime, ltime = 0;
	struct nfsmount *nmp;
	struct nfsnode *np;
	struct vnode *newvp;
	nfsv2fh_t *fhp;
	u_quad_t frev;
	int error = 0, tlen, more_dirs = 1, tresid, doit, bigenough, i;
	int cachable = 0;

	if (uiop->uio_iovcnt != 1)
		panic("nfs rdirlook");
	nmp = VFSTONFS(vp->v_mount);
	tresid = uiop->uio_resid;
	ndp->ni_dvp = vp;
	newvp = NULLVP;
	/*
	 * Loop around doing readdir rpc's of size uio_resid or nm_rsize,
	 * whichever is smaller, truncated to a multiple of NFS_DIRBLKSIZ.
	 * The stopping criteria is EOF or buffer full.
	 */
	while (more_dirs && uiop->uio_resid >= NFS_DIRBLKSIZ) {
		nfsstats.rpccnt[NQNFSPROC_READDIRLOOK]++;
		nfsm_reqhead(vp, NQNFSPROC_READDIRLOOK,
			NFSX_FH + 3 * NFSX_UNSIGNED);
		nfsm_fhtom(vp);
 		nfsm_build(tl, u_long *, 3 * NFSX_UNSIGNED);
		off = (u_long)uiop->uio_offset;
		*tl++ = txdr_unsigned(off);
		*tl++ = txdr_unsigned(((uiop->uio_resid > nmp->nm_rsize) ?
			nmp->nm_rsize : uiop->uio_resid) & ~(NFS_DIRBLKSIZ-1));
		if (nmp->nm_flag & NFSMNT_NQLOOKLEASE)
			*tl = txdr_unsigned(nmp->nm_leaseterm);
		else
			*tl = 0;
		reqtime = time.tv_sec;
		nfsm_request(vp, NQNFSPROC_READDIRLOOK, uiop->uio_procp, cred);
		nfsm_dissect(tl, u_long *, NFSX_UNSIGNED);
		more_dirs = fxdr_unsigned(int, *tl);

		/* loop thru the dir entries, doctoring them to 4bsd form */
		bigenough = 1;
		while (more_dirs && bigenough) {
			doit = 1;
			nfsm_dissect(tl, u_long *, 4 * NFSX_UNSIGNED);
			if (nmp->nm_flag & NFSMNT_NQLOOKLEASE) {
				cachable = fxdr_unsigned(int, *tl++);
				ltime = reqtime + fxdr_unsigned(int, *tl++);
				fxdr_hyper(tl, &frev);
			}
			nfsm_dissect(fhp, nfsv2fh_t *, NFSX_FH);
			if (!bcmp(VTONFS(vp)->n_fh.fh_bytes, (caddr_t)fhp, NFSX_FH)) {
				VREF(vp);
				newvp = vp;
				np = VTONFS(vp);
			} else {
				error = nfs_nget(vp->v_mount, fhp, &np);
				if (error)
					doit = 0;
				newvp = NFSTOV(np);
			}
			error = nfs_loadattrcache(&newvp, &md, &dpos,
				(struct vattr *)0);
			if (error)
				doit = 0;
			nfsm_dissect(tl, u_long *, 2 * NFSX_UNSIGNED);
			fileno = fxdr_unsigned(u_long, *tl++);
			len = fxdr_unsigned(int, *tl);
			if (len <= 0 || len > NFS_MAXNAMLEN) {
				error = EBADRPC;
				m_freem(mrep);
				goto nfsmout;
			}
			tlen = (len + 4) & ~0x3;
			if ((tlen + DIRHDSIZ) > uiop->uio_resid)
				bigenough = 0;
			if (bigenough && doit) {
				dp = (struct dirent *)uiop->uio_iov->iov_base;
				dp->d_fileno = fileno;
				dp->d_namlen = len;
				dp->d_reclen = tlen + DIRHDSIZ;
				dp->d_type =
				    IFTODT(VTTOIF(np->n_vattr.va_type));
				uiop->uio_resid -= DIRHDSIZ;
				uiop->uio_iov->iov_base += DIRHDSIZ;
				uiop->uio_iov->iov_len -= DIRHDSIZ;
				cnp->cn_nameptr = uiop->uio_iov->iov_base;
				cnp->cn_namelen = len;
				ndp->ni_vp = newvp;
				nfsm_mtouio(uiop, len);
				cp = uiop->uio_iov->iov_base;
				tlen -= len;
				for (i = 0; i < tlen; i++)
					*cp++ = '\0';
				uiop->uio_iov->iov_base += tlen;
				uiop->uio_iov->iov_len -= tlen;
				uiop->uio_resid -= tlen;
				cnp->cn_hash = 0;
				for (cp = cnp->cn_nameptr, i = 1; i <= len; i++, cp++)
					cnp->cn_hash += (unsigned char)*cp * i;
				if ((nmp->nm_flag & NFSMNT_NQLOOKLEASE) &&
					ltime > time.tv_sec)
					nqnfs_clientlease(nmp, np, NQL_READ,
						cachable, ltime, frev);
				if (cnp->cn_namelen <= NCHNAMLEN)
				    cache_enter(ndp->ni_dvp, ndp->ni_vp, cnp);
			} else {
				nfsm_adv(nfsm_rndup(len));
			}
			if (newvp != NULLVP) {
				vrele(newvp);
				newvp = NULLVP;
			}
			nfsm_dissect(tl, u_long *, 2 * NFSX_UNSIGNED);
			if (bigenough)
				endoff = off = fxdr_unsigned(u_long, *tl++);
			else
				endoff = fxdr_unsigned(u_long, *tl++);
			more_dirs = fxdr_unsigned(int, *tl);
		}
		/*
		 * If at end of rpc data, get the eof boolean
		 */
		if (!more_dirs) {
			nfsm_dissect(tl, u_long *, NFSX_UNSIGNED);
			more_dirs = (fxdr_unsigned(int, *tl) == 0);

			/*
			 * If at EOF, cache directory offset
			 */
			if (!more_dirs)
				VTONFS(vp)->n_direofoffset = endoff;
		}
		if (uiop->uio_resid < tresid)
			uiop->uio_offset = (off_t)off;
		else
			more_dirs = 0;
		m_freem(mrep);
	}
	/*
	 * Fill last record, iff any, out to a multiple of NFS_DIRBLKSIZ
	 * by increasing d_reclen for the last record.
	 */
	if (uiop->uio_resid < tresid) {
		len = uiop->uio_resid & (NFS_DIRBLKSIZ - 1);
		if (len > 0) {
			dp->d_reclen += len;
			uiop->uio_iov->iov_base += len;
			uiop->uio_iov->iov_len -= len;
			uiop->uio_resid -= len;
		}
	}
nfsmout:
	if (newvp != NULLVP)
		vrele(newvp);
	return (error);
}
static char hextoasc[] = "0123456789abcdef";

/*
 * Silly rename. To make the NFS filesystem that is stateless look a little
 * more like the "ufs" a remove of an active vnode is translated to a rename
 * to a funny looking filename that is removed by nfs_inactive on the
 * nfsnode. There is the potential for another process on a different client
 * to create the same funny name between the nfs_lookitup() fails and the
 * nfs_rename() completes, but...
 */
static int
nfs_sillyrename(dvp, vp, cnp)
	struct vnode *dvp, *vp;
	struct componentname *cnp;
{
	register struct nfsnode *np;
	register struct sillyrename *sp;
	int error;
	short pid;

	cache_purge(dvp);
	np = VTONFS(vp);
#ifdef SILLYSEPARATE
	MALLOC(sp, struct sillyrename *, sizeof (struct sillyrename),
		M_NFSREQ, M_WAITOK);
#else
	sp = &np->n_silly;
#endif
	sp->s_cred = crdup(cnp->cn_cred);
	sp->s_dvp = dvp;
	VREF(dvp);

	/* Fudge together a funny name */
	pid = cnp->cn_proc->p_pid;
	bcopy(".nfsAxxxx4.4", sp->s_name, 13);
	sp->s_namlen = 12;
	sp->s_name[8] = hextoasc[pid & 0xf];
	sp->s_name[7] = hextoasc[(pid >> 4) & 0xf];
	sp->s_name[6] = hextoasc[(pid >> 8) & 0xf];
	sp->s_name[5] = hextoasc[(pid >> 12) & 0xf];

	/* Try lookitups until we get one that isn't there */
	while (nfs_lookitup(sp, (nfsv2fh_t *)0, cnp->cn_proc) == 0) {
		sp->s_name[4]++;
		if (sp->s_name[4] > 'z') {
			error = EINVAL;
			goto bad;
		}
	}
	error = nfs_renameit(dvp, cnp, sp);
	if (error)
		goto bad;
	nfs_lookitup(sp, &np->n_fh, cnp->cn_proc);
	np->n_sillyrename = sp;
	return (0);
bad:
	vrele(sp->s_dvp);
	crfree(sp->s_cred);
#ifdef SILLYSEPARATE
	free((caddr_t)sp, M_NFSREQ);
#endif
	return (error);
}

/*
 * Look up a file name for silly rename stuff.
 * Just like nfs_lookup() except that it doesn't load returned values
 * into the nfsnode table.
 * If fhp != NULL it copies the returned file handle out
 */
static int
nfs_lookitup(sp, fhp, procp)
	register struct sillyrename *sp;
	nfsv2fh_t *fhp;
	struct proc *procp;
{
	register struct vnode *vp = sp->s_dvp;
	register u_long *tl;
	register caddr_t cp;
	register long t1, t2;
	caddr_t bpos, dpos, cp2;
	int error = 0, isnq;
	struct mbuf *mreq, *mrep, *md, *mb, *mb2;
	long len;

	isnq = (VFSTONFS(vp->v_mount)->nm_flag & NFSMNT_NQNFS);
	nfsstats.rpccnt[NFSPROC_LOOKUP]++;
	len = sp->s_namlen;
	nfsm_reqhead(vp, NFSPROC_LOOKUP, NFSX_FH+NFSX_UNSIGNED+nfsm_rndup(len));
	if (isnq) {
		nfsm_build(tl, u_long *, NFSX_UNSIGNED);
		*tl = 0;
	}
	nfsm_fhtom(vp);
	nfsm_strtom(sp->s_name, len, NFS_MAXNAMLEN);
	nfsm_request(vp, NFSPROC_LOOKUP, procp, sp->s_cred);
	if (fhp != NULL) {
		if (isnq)
			nfsm_dissect(tl, u_long *, NFSX_UNSIGNED);
		nfsm_dissect(cp, caddr_t, NFSX_FH);
		bcopy(cp, (caddr_t)fhp, NFSX_FH);
	}
	nfsm_reqdone;
	return (error);
}

/*
 * Kludge City..
 * - make nfs_bmap() essentially a no-op that does no translation
 * - do nfs_strategy() by faking physical I/O with nfs_readrpc/nfs_writerpc
 *   after mapping the physical addresses into Kernel Virtual space in the
 *   nfsiobuf area.
 *   (Maybe I could use the process's page mapping, but I was concerned that
 *    Kernel Write might not be enabled and also figured copyout() would do
 *    a lot more work than bcopy() and also it currently happens in the
 *    context of the swapper process (2).
 */
static int
nfs_bmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;

	if (ap->a_vpp != NULL)
		*ap->a_vpp = vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn * btodb(vp->v_mount->mnt_stat.f_iosize);
	if (ap->a_runp != NULL)
		*ap->a_runp = 0;
	return (0);
}

/*
 * Strategy routine.
 * For async requests when nfsiod(s) are running, queue the request by
 * calling nfs_asyncio(), otherwise just all nfs_doio() to do the
 * request.
 */
static int
nfs_strategy(ap)
	struct vop_strategy_args *ap;
{
	register struct buf *bp = ap->a_bp;
	struct ucred *cr;
	struct proc *p;
	int error = 0;

	if ((bp->b_flags & (B_PHYS|B_ASYNC)) == (B_PHYS|B_ASYNC))
		panic("nfs physio/async");
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
	if ((bp->b_flags & B_ASYNC) == 0 ||
		nfs_asyncio(bp, NOCRED))
		error = nfs_doio(bp, cr, p);
	return (error);
}

/*
 * Mmap a file
 *
 * NB Currently unsupported.
 */
/* ARGSUSED */
static int
nfs_mmap(ap)
	struct vop_mmap_args /* {
		struct vnode *a_vp;
		int  a_fflags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{

	return (EINVAL);
}

/*
 * Flush all the blocks associated with a vnode.
 * 	Walk through the buffer pool and push any dirty pages
 *	associated with the vnode.
 */
/* ARGSUSED */
static int
nfs_fsync(ap)
	struct vop_fsync_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_vp;
		struct ucred * a_cred;
		int  a_waitfor;
		struct proc * a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct nfsnode *np = VTONFS(vp);
	register struct buf *bp;
	struct buf *nbp;
	struct nfsmount *nmp;
	int s, error = 0, slptimeo = 0, slpflag = 0;

	nmp = VFSTONFS(vp->v_mount);
	if (nmp->nm_flag & NFSMNT_INT)
		slpflag = PCATCH;
loop:
	s = splbio();
	for (bp = vp->v_dirtyblkhd.lh_first; bp; bp = nbp) {
		nbp = bp->b_vnbufs.le_next;
		if (bp->b_flags & B_BUSY) {
			if (ap->a_waitfor != MNT_WAIT)
				continue;
			bp->b_flags |= B_WANTED;
			error = tsleep((caddr_t)bp, slpflag | (PRIBIO + 1),
				"nfsfsync", slptimeo);
			splx(s);
			if (error) {
			    if (nfs_sigintr(nmp, (struct nfsreq *)0, ap->a_p))
				return (EINTR);
			    if (slpflag == PCATCH) {
				slpflag = 0;
				slptimeo = 2 * hz;
			    }
			}
			goto loop;
		}
		if ((bp->b_flags & B_DELWRI) == 0)
			panic("nfs_fsync: not dirty");
		bremfree(bp);
		bp->b_flags |= B_BUSY;
		splx(s);
		bp->b_flags |= B_ASYNC;
		VOP_BWRITE(bp);
		goto loop;
	}
	splx(s);
	if (ap->a_waitfor == MNT_WAIT) {
		while (vp->v_numoutput) {
			vp->v_flag |= VBWAIT;
			error = tsleep((caddr_t)&vp->v_numoutput,
				slpflag | (PRIBIO + 1), "nfsfsync", slptimeo);
			if (error) {
			    if (nfs_sigintr(nmp, (struct nfsreq *)0, ap->a_p))
				return (EINTR);
			    if (slpflag == PCATCH) {
				slpflag = 0;
				slptimeo = 2 * hz;
			    }
			}
		}
		if (vp->v_dirtyblkhd.lh_first) {
			goto loop;
		}
	}
	if (np->n_flag & NWRITEERR) {
		error = np->n_error;
		np->n_flag &= ~NWRITEERR;
	}
	return (error);
}

/*
 * Return POSIX pathconf information applicable to nfs.
 *
 * Currently the NFS protocol does not support getting such
 * information from the remote server.
 */
/* ARGSUSED */
static int
nfs_pathconf(ap)
	struct vop_pathconf_args /* {
		struct vnode *a_vp;
		int a_name;
		int *a_retval;
	} */ *ap;
{

	return (EINVAL);
}

/*
 * NFS advisory byte-level locks.
 * Currently unsupported.
 */
static int
nfs_advlock(ap)
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		caddr_t  a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap;
{
	register struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * The following kludge is to allow diskless support to work
	 * until a real NFS lockd is implemented. Basically, just pretend
	 * that this is a local lock.
	 */
	return (lf_advlock(ap, &(np->n_lockf), np->n_size));
}

/*
 * Print out the contents of an nfsnode.
 */
static int
nfs_print(ap)
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct nfsnode *np = VTONFS(vp);

	printf("tag VT_NFS, fileid %ld fsid 0x%lx",
		np->n_vattr.va_fileid, np->n_vattr.va_fsid);
	if (vp->v_type == VFIFO)
		fifo_printinfo(vp);
	printf("\n");
	return (0);
}

/*
 * NFS directory offset lookup.
 * Currently unsupported.
 */
static int
nfs_blkatoff(ap)
	struct vop_blkatoff_args /* {
		struct vnode *a_vp;
		off_t a_offset;
		char **a_res;
		struct buf **a_bpp;
	} */ *ap;
{

	return (EOPNOTSUPP);
}

/*
 * NFS flat namespace allocation.
 * Currently unsupported.
 */
static int
nfs_valloc(ap)
	struct vop_valloc_args /* {
		struct vnode *a_pvp;
		int a_mode;
		struct ucred *a_cred;
		struct vnode **a_vpp;
	} */ *ap;
{

	return (EOPNOTSUPP);
}

/*
 * NFS flat namespace free.
 * Currently unsupported.
 */
static int
nfs_vfree(ap)
	struct vop_vfree_args /* {
		struct vnode *a_pvp;
		ino_t a_ino;
		int a_mode;
	} */ *ap;
{

	return (EOPNOTSUPP);
}

/*
 * NFS file truncation.
 */
static int
nfs_truncate(ap)
	struct vop_truncate_args /* {
		struct vnode *a_vp;
		off_t a_length;
		int a_flags;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{

	/* Use nfs_setattr */
	printf("nfs_truncate: need to implement!!");
	return (EOPNOTSUPP);
}

/*
 * NFS update.
 */
static int
nfs_update(ap)
	struct vop_update_args /* {
		struct vnode *a_vp;
		struct timeval *a_ta;
		struct timeval *a_tm;
		int a_waitfor;
	} */ *ap;
{

#if 0
	/* Use nfs_setattr */
	printf("nfs_update: need to implement!!");
#endif
	return (EOPNOTSUPP);
}

/*
 * nfs special file access vnode op.
 * Essentially just get vattr and then imitate iaccess() since the device is
 * local to the client.
 */
static int
nfsspec_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	register struct vattr *vap;
	register gid_t *gp;
	register struct ucred *cred = ap->a_cred;
	struct vnode *vp = ap->a_vp;
	mode_t mode = ap->a_mode;
	struct vattr vattr;
	register int i;
	int error;

	/*
	 * Disallow write attempts on filesystems mounted read-only;
	 * unless the file is a socket, fifo, or a block or character
	 * device resident on the filesystem.
	 */
	if ((mode & VWRITE) && (vp->v_mount->mnt_flag & MNT_RDONLY)) {
		switch (vp->v_type) {
		case VREG: case VDIR: case VLNK:
			return (EROFS);
		}
	}
	/*
	 * If you're the super-user,
	 * you always get access.
	 */
	if (cred->cr_uid == 0)
		return (0);
	vap = &vattr;
	error = VOP_GETATTR(ap->a_vp, vap, cred, ap->a_p);
	if (error)
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
	return ((vap->va_mode & mode) == mode ? 0 : EACCES);
}

/*
 * Read wrapper for special devices.
 */
static int
nfsspec_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	register struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set access flag.
	 */
	np->n_flag |= NACC;
	np->n_atim = time;
	return (VOCALL(spec_vnodeop_p, VOFFSET(vop_read), ap));
}

/*
 * Write wrapper for special devices.
 */
static int
nfsspec_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	register struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set update flag.
	 */
	np->n_flag |= NUPD;
	np->n_mtim = time;
	return (VOCALL(spec_vnodeop_p, VOFFSET(vop_write), ap));
}

/*
 * Close wrapper for special devices.
 *
 * Update the times on the nfsnode then do device close.
 */
static int
nfsspec_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct nfsnode *np = VTONFS(vp);
	struct vattr vattr;

	if (np->n_flag & (NACC | NUPD)) {
		np->n_flag |= NCHG;
		if (vp->v_usecount == 1 &&
		    (vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
			VATTR_NULL(&vattr);
			if (np->n_flag & NACC) {
				vattr.va_atime.ts_sec = np->n_atim.tv_sec;
				vattr.va_atime.ts_nsec =
				    np->n_atim.tv_usec * 1000;
			}
			if (np->n_flag & NUPD) {
				vattr.va_mtime.ts_sec = np->n_mtim.tv_sec;
				vattr.va_mtime.ts_nsec =
				    np->n_mtim.tv_usec * 1000;
			}
			(void)VOP_SETATTR(vp, &vattr, ap->a_cred, ap->a_p);
		}
	}
	return (VOCALL(spec_vnodeop_p, VOFFSET(vop_close), ap));
}

/*
 * Read wrapper for fifos.
 */
static int
nfsfifo_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	register struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set access flag.
	 */
	np->n_flag |= NACC;
	np->n_atim = time;
	return (VOCALL(fifo_vnodeop_p, VOFFSET(vop_read), ap));
}

/*
 * Write wrapper for fifos.
 */
static int
nfsfifo_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	register struct nfsnode *np = VTONFS(ap->a_vp);

	/*
	 * Set update flag.
	 */
	np->n_flag |= NUPD;
	np->n_mtim = time;
	return (VOCALL(fifo_vnodeop_p, VOFFSET(vop_write), ap));
}

/*
 * Close wrapper for fifos.
 *
 * Update the times on the nfsnode then do fifo close.
 */
static int
nfsfifo_close(ap)
	struct vop_close_args /* {
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	register struct vnode *vp = ap->a_vp;
	register struct nfsnode *np = VTONFS(vp);
	struct vattr vattr;

	if (np->n_flag & (NACC | NUPD)) {
		if (np->n_flag & NACC)
			np->n_atim = time;
		if (np->n_flag & NUPD)
			np->n_mtim = time;
		np->n_flag |= NCHG;
		if (vp->v_usecount == 1 &&
		    (vp->v_mount->mnt_flag & MNT_RDONLY) == 0) {
			VATTR_NULL(&vattr);
			if (np->n_flag & NACC) {
				vattr.va_atime.ts_sec = np->n_atim.tv_sec;
				vattr.va_atime.ts_nsec =
				    np->n_atim.tv_usec * 1000;
			}
			if (np->n_flag & NUPD) {
				vattr.va_mtime.ts_sec = np->n_mtim.tv_sec;
				vattr.va_mtime.ts_nsec =
				    np->n_mtim.tv_usec * 1000;
			}
			(void)VOP_SETATTR(vp, &vattr, ap->a_cred, ap->a_p);
		}
	}
	return (VOCALL(fifo_vnodeop_p, VOFFSET(vop_close), ap));
}
