/*
 * Copyright (c) 2000-2001 Boris Popov
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/lockf.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_zone.h>


#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>

#include <fs/smbfs/smbfs.h>
#include <fs/smbfs/smbfs_node.h>
#include <fs/smbfs/smbfs_subr.h>

/*
 * Prototypes for SMBFS vnode operations
 */
static int smbfs_create(struct vop_create_args *);
static int smbfs_mknod(struct vop_mknod_args *);
static int smbfs_open(struct vop_open_args *);
static int smbfs_close(struct vop_close_args *);
static int smbfs_access(struct vop_access_args *);
static int smbfs_getattr(struct vop_getattr_args *);
static int smbfs_setattr(struct vop_setattr_args *);
static int smbfs_read(struct vop_read_args *);
static int smbfs_write(struct vop_write_args *);
static int smbfs_fsync(struct vop_fsync_args *);
static int smbfs_remove(struct vop_remove_args *);
static int smbfs_link(struct vop_link_args *);
static int smbfs_lookup(struct vop_lookup_args *);
static int smbfs_rename(struct vop_rename_args *);
static int smbfs_mkdir(struct vop_mkdir_args *);
static int smbfs_rmdir(struct vop_rmdir_args *);
static int smbfs_symlink(struct vop_symlink_args *);
static int smbfs_readdir(struct vop_readdir_args *);
static int smbfs_bmap(struct vop_bmap_args *);
static int smbfs_strategy(struct vop_strategy_args *);
static int smbfs_print(struct vop_print_args *);
static int smbfs_pathconf(struct vop_pathconf_args *ap);
static int smbfs_advlock(struct vop_advlock_args *);
#ifndef FB_RELENG3
static int smbfs_getextattr(struct vop_getextattr_args *ap);
#endif

vop_t **smbfs_vnodeop_p;
static struct vnodeopv_entry_desc smbfs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_access_desc,		(vop_t *) smbfs_access },
	{ &vop_advlock_desc,		(vop_t *) smbfs_advlock },
	{ &vop_bmap_desc,		(vop_t *) smbfs_bmap },
	{ &vop_close_desc,		(vop_t *) smbfs_close },
	{ &vop_create_desc,		(vop_t *) smbfs_create },
	{ &vop_fsync_desc,		(vop_t *) smbfs_fsync },
	{ &vop_getattr_desc,		(vop_t *) smbfs_getattr },
	{ &vop_getpages_desc,		(vop_t *) smbfs_getpages },
	{ &vop_inactive_desc,		(vop_t *) smbfs_inactive },
	{ &vop_ioctl_desc,		(vop_t *) smbfs_ioctl },
	{ &vop_islocked_desc,		(vop_t *) vop_stdislocked },
	{ &vop_link_desc,		(vop_t *) smbfs_link },
	{ &vop_lock_desc,		(vop_t *) vop_stdlock },
	{ &vop_lookup_desc,		(vop_t *) smbfs_lookup },
	{ &vop_mkdir_desc,		(vop_t *) smbfs_mkdir },
	{ &vop_mknod_desc,		(vop_t *) smbfs_mknod },
	{ &vop_open_desc,		(vop_t *) smbfs_open },
	{ &vop_pathconf_desc,		(vop_t *) smbfs_pathconf },
	{ &vop_print_desc,		(vop_t *) smbfs_print },
	{ &vop_putpages_desc,		(vop_t *) smbfs_putpages },
	{ &vop_read_desc,		(vop_t *) smbfs_read },
	{ &vop_readdir_desc,		(vop_t *) smbfs_readdir },
	{ &vop_reclaim_desc,		(vop_t *) smbfs_reclaim },
	{ &vop_remove_desc,		(vop_t *) smbfs_remove },
	{ &vop_rename_desc,		(vop_t *) smbfs_rename },
	{ &vop_rmdir_desc,		(vop_t *) smbfs_rmdir },
	{ &vop_setattr_desc,		(vop_t *) smbfs_setattr },
	{ &vop_strategy_desc,		(vop_t *) smbfs_strategy },
	{ &vop_symlink_desc,		(vop_t *) smbfs_symlink },
	{ &vop_unlock_desc,		(vop_t *) vop_stdunlock },
	{ &vop_write_desc,		(vop_t *) smbfs_write },
#ifndef FB_RELENG3
	{ &vop_getextattr_desc, 	(vop_t *) smbfs_getextattr },
/*	{ &vop_setextattr_desc,		(vop_t *) smbfs_setextattr },*/
#endif
	{ NULL, NULL }
};

static struct vnodeopv_desc smbfs_vnodeop_opv_desc =
	{ &smbfs_vnodeop_p, smbfs_vnodeop_entries };

VNODEOP_SET(smbfs_vnodeop_opv_desc);

static int
smbfs_access(ap)
	struct vop_access_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct ucred *cred = ap->a_cred;
	u_int mode = ap->a_mode;
	struct smbmount *smp = VTOSMBFS(vp);
	int error = 0;

	SMBVDEBUG("\n");
	if ((mode & VWRITE) && (vp->v_mount->mnt_flag & MNT_RDONLY)) {
		switch (vp->v_type) {
		    case VREG: case VDIR: case VLNK:
			return EROFS;
		    default:
			break;
		}
	}
	if (cred->cr_uid == 0)
		return 0;
	if (cred->cr_uid != smp->sm_args.uid) {
		mode >>= 3;
		if (!groupmember(smp->sm_args.gid, cred))
			mode >>= 3;
	}
	error = (((vp->v_type == VREG) ? smp->sm_args.file_mode : smp->sm_args.dir_mode) & mode) == mode ? 0 : EACCES;
	return error;
}

/* ARGSUSED */
static int
smbfs_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
	struct vattr vattr;
	int mode = ap->a_mode;
	int error, accmode;

	SMBVDEBUG("%s,%d\n", np->n_name, np->n_opencount);
	if (vp->v_type != VREG && vp->v_type != VDIR) { 
		SMBFSERR("open eacces vtype=%d\n", vp->v_type);
		return EACCES;
	}
	if (vp->v_type == VDIR) {
		np->n_opencount++;
		return 0;
	}
	if (np->n_flag & NMODIFIED) {
		if ((error = smbfs_vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p, 1)) == EINTR)
			return error;
		smbfs_attr_cacheremove(vp);
		error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
		if (error)
			return error;
		np->n_mtime.tv_sec = vattr.va_mtime.tv_sec;
	} else {
		error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
		if (error)
			return error;
		if (np->n_mtime.tv_sec != vattr.va_mtime.tv_sec) {
			error = smbfs_vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p, 1);
			if (error == EINTR)
				return error;
			np->n_mtime.tv_sec = vattr.va_mtime.tv_sec;
		}
	}
	if (np->n_opencount) {
		np->n_opencount++;
		return 0;
	}
	accmode = SMB_AM_OPENREAD;
	if ((vp->v_mount->mnt_flag & MNT_RDONLY) == 0)
		accmode = SMB_AM_OPENRW;
	smb_makescred(&scred, ap->a_p, ap->a_cred);
	error = smbfs_smb_open(np, accmode, &scred);
	if (error) {
		if (mode & FWRITE)
			return EACCES;
		accmode = SMB_AM_OPENREAD;
		error = smbfs_smb_open(np, accmode, &scred);
	}
	if (!error) {
		np->n_opencount++;
	}
	smbfs_attr_cacheremove(vp);
	return error;
}

static int
smbfs_closel(struct vop_close_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	struct proc *p = ap->a_p;
	struct smb_cred scred;
	struct vattr vattr;
	int error;

	SMBVDEBUG("name=%s, pid=%d, c=%d\n",np->n_name, p->p_pid, np->n_opencount);

	smb_makescred(&scred, p, ap->a_cred);

	if (np->n_opencount == 0) {
		SMBERROR("Negative opencount\n");
		return 0;
	}
	np->n_opencount--;
	if (vp->v_type == VDIR) {
		if (np->n_opencount)
			return 0;
		if (np->n_dirseq) {
			smbfs_findclose(np->n_dirseq, &scred);
			np->n_dirseq = NULL;
		}
		error = 0;
	} else {
		error = smbfs_vinvalbuf(vp, V_SAVE, ap->a_cred, p, 1);
		if (np->n_opencount)
			return error;
		VOP_GETATTR(vp, &vattr, ap->a_cred, p);
		error = smbfs_smb_close(np->n_mount->sm_share, np->n_fid, 
			   &np->n_mtime, &scred);
	}
	smbfs_attr_cacheremove(vp);
	return error;
}

/*
 * XXX: VOP_CLOSE() usually called without lock held which is suck. Here we
 * do some heruistic to determine if vnode should be locked.
 */
static int
smbfs_close(ap)
	struct vop_close_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct proc *p = ap->a_p;
	int error, dolock;

	VI_LOCK(vp);
	dolock = (vp->v_flag & VXLOCK) == 0;
	if (dolock)
		vn_lock(vp, LK_EXCLUSIVE | LK_RETRY | LK_INTERLOCK, p);
	else
		VI_UNLOCK(vp);
	error = smbfs_closel(ap);
	if (dolock)
		VOP_UNLOCK(vp, 0, p);
	return error;
}

/*
 * smbfs_getattr call from vfs.
 */
static int
smbfs_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	struct vattr *va=ap->a_vap;
	struct smbfattr fattr;
	struct smb_cred scred;
	u_int32_t oldsize;
	int error;

	SMBVDEBUG("%lx: '%s' %d\n", (long)vp, np->n_name, (vp->v_flag & VROOT) != 0);
	error = smbfs_attr_cachelookup(vp, va);
	if (!error)
		return 0;
	SMBVDEBUG("not in the cache\n");
	smb_makescred(&scred, ap->a_p, ap->a_cred);
	oldsize = np->n_size;
	error = smbfs_smb_lookup(np, NULL, 0, &fattr, &scred);
	if (error) {
		SMBVDEBUG("error %d\n", error);
		return error;
	}
	smbfs_attr_cacheenter(vp, &fattr);
	smbfs_attr_cachelookup(vp, va);
	if (np->n_opencount)
		np->n_size = oldsize;
	return 0;
}

static int
smbfs_setattr(ap)
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	struct vattr *vap = ap->a_vap;
	struct timespec *mtime, *atime;
	struct smb_cred scred;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	u_quad_t tsize = 0;
	int isreadonly, doclose, error = 0;

	SMBVDEBUG("\n");
	if (vap->va_flags != VNOVAL)
		return EOPNOTSUPP;
	isreadonly = (vp->v_mount->mnt_flag & MNT_RDONLY);
	/*
	 * Disallow write attempts if the filesystem is mounted read-only.
	 */
  	if ((vap->va_uid != (uid_t)VNOVAL || vap->va_gid != (gid_t)VNOVAL || 
	     vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL ||
	     vap->va_mode != (mode_t)VNOVAL) && isreadonly)
		return EROFS;
	smb_makescred(&scred, ap->a_p, ap->a_cred);
	if (vap->va_size != VNOVAL) {
 		switch (vp->v_type) {
 		    case VDIR:
 			return EISDIR;
 		    case VREG:
			break;
 		    default:
			return EINVAL;
  		};
		if (isreadonly)
			return EROFS;
		doclose = 0;
		vnode_pager_setsize(vp, (u_long)vap->va_size);
 		tsize = np->n_size;
 		np->n_size = vap->va_size;
		if (np->n_opencount == 0) {
			error = smbfs_smb_open(np, SMB_AM_OPENRW, &scred);
			if (error == 0)
				doclose = 1;
		}
		if (error == 0)
			error = smbfs_smb_setfsize(np, vap->va_size, &scred);
		if (doclose)
			smbfs_smb_close(ssp, np->n_fid, NULL, &scred);
		if (error) {
			np->n_size = tsize;
			vnode_pager_setsize(vp, (u_long)tsize);
			return error;
		}
  	}
	mtime = atime = NULL;
	if (vap->va_mtime.tv_sec != VNOVAL)
		mtime = &vap->va_mtime;
	if (vap->va_atime.tv_sec != VNOVAL)
		atime = &vap->va_atime;
	if (mtime != atime) {
#if 0
		if (mtime == NULL)
			mtime = &np->n_mtime;
		if (atime == NULL)
			atime = &np->n_atime;
#endif
		/*
		 * If file is opened, then we can use handle based calls.
		 * If not, use path based ones.
		 */
		if (np->n_opencount == 0) {
			if (vcp->vc_flags & SMBV_WIN95) {
				error = VOP_OPEN(vp, FWRITE, ap->a_cred, ap->a_p);
				if (!error) {
/*				error = smbfs_smb_setfattrNT(np, 0, mtime, atime, &scred);
				VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);*/
				if (mtime)
					np->n_mtime = *mtime;
				VOP_CLOSE(vp, FWRITE, ap->a_cred, ap->a_p);
				}
			} else if ((vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS)) {
				error = smbfs_smb_setptime2(np, mtime, atime, 0, &scred);
/*				error = smbfs_smb_setpattrNT(np, 0, mtime, atime, &scred);*/
			} else if (SMB_DIALECT(vcp) >= SMB_DIALECT_LANMAN2_0) {
				error = smbfs_smb_setptime2(np, mtime, atime, 0, &scred);
			} else {
				error = smbfs_smb_setpattr(np, 0, mtime, &scred);
			}
		} else {
			if (vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS) {
				error = smbfs_smb_setfattrNT(np, 0, mtime, atime, &scred);
			} else if (SMB_DIALECT(vcp) >= SMB_DIALECT_LANMAN1_0) {
				error = smbfs_smb_setftime(np, mtime, atime, &scred);
			} else {
				/*
				 * I have no idea how to handle this for core
				 * level servers. The possible solution is to
				 * update mtime after file is closed.
				 */
				 SMBERROR("can't update times on an opened file\n");
			}
		}
	}
	/*
	 * Invalidate attribute cache in case if server doesn't set
	 * required attributes.
	 */
	smbfs_attr_cacheremove(vp);	/* invalidate cache */
	VOP_GETATTR(vp, vap, ap->a_cred, ap->a_p);
	np->n_mtime.tv_sec = vap->va_mtime.tv_sec;
	return error;
}
/*
 * smbfs_read call.
 */
static int
smbfs_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;

	SMBVDEBUG("\n");
	if (vp->v_type != VREG && vp->v_type != VDIR)
		return EPERM;
	return smbfs_readvnode(vp, uio, ap->a_cred);
}

static int
smbfs_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;

	SMBVDEBUG("%d,ofs=%d,sz=%d\n",vp->v_type, (int)uio->uio_offset, uio->uio_resid);
	if (vp->v_type != VREG)
		return (EPERM);
	return smbfs_writevnode(vp, uio, ap->a_cred,ap->a_ioflag);
}
/*
 * smbfs_create call
 * Create a regular file. On entry the directory to contain the file being
 * created is locked.  We must release before we return. We must also free
 * the pathname buffer pointed at by cnp->cn_pnbuf, always on error, or
 * only if the SAVESTART bit in cn_flags is clear on success.
 */
static int
smbfs_create(ap)
	struct vop_create_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	struct vnode *dvp = ap->a_dvp;
	struct vattr *vap = ap->a_vap;
	struct vnode **vpp=ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct smbnode *dnp = VTOSMB(dvp);
	struct vnode *vp;
	struct vattr vattr;
	struct smbfattr fattr;
	struct smb_cred scred;
	char *name = cnp->cn_nameptr;
	int nmlen = cnp->cn_namelen;
	int error;
	

	SMBVDEBUG("\n");
	*vpp = NULL;
	if (vap->va_type != VREG)
		return EOPNOTSUPP;
	if ((error = VOP_GETATTR(dvp, &vattr, cnp->cn_cred, cnp->cn_proc)))
		return error;
	smb_makescred(&scred, cnp->cn_proc, cnp->cn_cred);
	
	error = smbfs_smb_create(dnp, name, nmlen, &scred);
	if (error)
		return error;
	error = smbfs_smb_lookup(dnp, name, nmlen, &fattr, &scred);
	if (error)
		return error;
	error = smbfs_nget(VTOVFS(dvp), dvp, name, nmlen, &fattr, &vp);
	if (error)
		return error;
	*vpp = vp;
	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(dvp, vp, cnp);
	return error;
}

static int
smbfs_remove(ap)
	struct vop_remove_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_dvp;
		struct vnode * a_vp;
		struct componentname * a_cnp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
/*	struct vnode *dvp = ap->a_dvp;*/
	struct componentname *cnp = ap->a_cnp;
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
	int error;

	if (vp->v_type == VDIR || np->n_opencount || vp->v_usecount != 1)
		return EPERM;
	smb_makescred(&scred, cnp->cn_proc, cnp->cn_cred);
	error = smbfs_smb_delete(np, &scred);
	cache_purge(vp);
	return error;
}

/*
 * smbfs_file rename call
 */
static int
smbfs_rename(ap)
	struct vop_rename_args  /* {
		struct vnode *a_fdvp;
		struct vnode *a_fvp;
		struct componentname *a_fcnp;
		struct vnode *a_tdvp;
		struct vnode *a_tvp;
		struct componentname *a_tcnp;
	} */ *ap;
{
	struct vnode *fvp = ap->a_fvp;
	struct vnode *tvp = ap->a_tvp;
	struct vnode *fdvp = ap->a_fdvp;
	struct vnode *tdvp = ap->a_tdvp;
	struct componentname *tcnp = ap->a_tcnp;
/*	struct componentname *fcnp = ap->a_fcnp;*/
	struct smb_cred scred;
	u_int16_t flags = 6;
	int error=0;

	/* Check for cross-device rename */
	if ((fvp->v_mount != tdvp->v_mount) ||
	    (tvp && (fvp->v_mount != tvp->v_mount))) {
		error = EXDEV;
		goto out;
	}

	if (tvp && tvp->v_usecount > 1) {
		error = EBUSY;
		goto out;
	}
	flags = 0x10;			/* verify all writes */
	if (fvp->v_type == VDIR) {
		flags |= 2;
	} else if (fvp->v_type == VREG) {
		flags |= 1;
	} else
		return EINVAL;
	smb_makescred(&scred, tcnp->cn_proc, tcnp->cn_cred);
	/*
	 * It seems that Samba doesn't implement SMB_COM_MOVE call...
	 */
#ifdef notnow
	if (SMB_DIALECT(SSTOCN(smp->sm_share)) >= SMB_DIALECT_LANMAN1_0) {
		error = smbfs_smb_move(VTOSMB(fvp), VTOSMB(tdvp),
		    tcnp->cn_nameptr, tcnp->cn_namelen, flags, &scred);
	} else
#endif
	{
		/*
		 * We have to do the work atomicaly
		 */
		if (tvp && tvp != fvp) {
			error = smbfs_smb_delete(VTOSMB(tvp), &scred);
			if (error)
				goto out;
		}
		error = smbfs_smb_rename(VTOSMB(fvp), VTOSMB(tdvp),
		    tcnp->cn_nameptr, tcnp->cn_namelen, &scred);
	}

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
	smbfs_attr_cacheremove(fdvp);
	smbfs_attr_cacheremove(tdvp);
#ifdef possible_mistake
	vgone(fvp);
	if (tvp)
		vgone(tvp);
#endif
	return error;
}

/*
 * somtime it will come true...
 */
static int
smbfs_link(ap)
	struct vop_link_args /* {
		struct vnode *a_tdvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	return EOPNOTSUPP;
}

/*
 * smbfs_symlink link create call.
 * Sometime it will be functional...
 */
static int
smbfs_symlink(ap)
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap;
{
	return EOPNOTSUPP;
}

static int
smbfs_mknod(ap) 
	struct vop_mknod_args /* {
	} */ *ap;
{
	return EOPNOTSUPP;
}

static int
smbfs_mkdir(ap)
	struct vop_mkdir_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	struct vnode *dvp = ap->a_dvp;
/*	struct vattr *vap = ap->a_vap;*/
	struct vnode *vp;
	struct componentname *cnp = ap->a_cnp;
	struct smbnode *dnp = VTOSMB(dvp);
	struct vattr vattr;
	struct smb_cred scred;
	struct smbfattr fattr;
	char *name = cnp->cn_nameptr;
	int len = cnp->cn_namelen;
	int error;

	if ((error = VOP_GETATTR(dvp, &vattr, cnp->cn_cred, cnp->cn_proc))) {
		return error;
	}	
	if ((name[0] == '.') && ((len == 1) || ((len == 2) && (name[1] == '.'))))
		return EEXIST;
	smb_makescred(&scred, cnp->cn_proc, cnp->cn_cred);
	error = smbfs_smb_mkdir(dnp, name, len, &scred);
	if (error)
		return error;
	error = smbfs_smb_lookup(dnp, name, len, &fattr, &scred);
	if (error)
		return error;
	error = smbfs_nget(VTOVFS(dvp), dvp, name, len, &fattr, &vp);
	if (error)
		return error;
	*ap->a_vpp = vp;
	return 0;
}

/*
 * smbfs_remove directory call
 */
static int
smbfs_rmdir(ap)
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
/*	struct smbmount *smp = VTOSMBFS(vp);*/
	struct smbnode *dnp = VTOSMB(dvp);
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
	int error;

	if (dvp == vp)
		return EINVAL;

	smb_makescred(&scred, cnp->cn_proc, cnp->cn_cred);
	error = smbfs_smb_rmdir(np, &scred);
	dnp->n_flag |= NMODIFIED;
	smbfs_attr_cacheremove(dvp);
/*	cache_purge(dvp);*/
	cache_purge(vp);
	return error;
}

/*
 * smbfs_readdir call
 */
static int
smbfs_readdir(ap)
	struct vop_readdir_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		struct ucred *a_cred;
		int *a_eofflag;
		u_long *a_cookies;
		int a_ncookies;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	int error;

	if (vp->v_type != VDIR)
		return (EPERM);
#ifdef notnow
	if (ap->a_ncookies) {
		printf("smbfs_readdir: no support for cookies now...");
		return (EOPNOTSUPP);
	}
#endif
	error = smbfs_readvnode(vp, uio, ap->a_cred);
	return error;
}

/* ARGSUSED */
static int
smbfs_fsync(ap)
	struct vop_fsync_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_vp;
		struct ucred * a_cred;
		int  a_waitfor;
		struct proc * a_p;
	} */ *ap;
{
/*	return (smb_flush(ap->a_vp, ap->a_cred, ap->a_waitfor, ap->a_p, 1));*/
    return (0);
}

static 
int smbfs_print (ap) 
	struct vop_print_args /* {
	struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);

	if (np == NULL) {
		printf("no smbnode data\n");
		return (0);
	}
	printf("tag VT_SMBFS, name = %s, parent = %p, opencount = %d",
	    np->n_name, np->n_parent ? SMBTOV(np->n_parent) : NULL,
	    np->n_opencount);
	lockmgr_printinfo(&vp->v_lock);
	printf("\n");
	return (0);
}

static int
smbfs_pathconf (ap)
	struct vop_pathconf_args  /* {
	struct vnode *vp;
	int name;
	register_t *retval;
	} */ *ap;
{
	struct smbmount *smp = VFSTOSMBFS(VTOVFS(ap->a_vp));
	struct smb_vc *vcp = SSTOVC(smp->sm_share);
	register_t *retval = ap->a_retval;
	int error = 0;
	
	switch (ap->a_name) {
	    case _PC_LINK_MAX:
		*retval = 0;
		break;
	    case _PC_NAME_MAX:
		*retval = (vcp->vc_flags & SMBV_LONGNAMES) ? 255 : 12;
		break;
	    case _PC_PATH_MAX:
		*retval = 800;	/* XXX: a correct one ? */
		break;
	    default:
		error = EINVAL;
	}
	return error;
}

static int
smbfs_strategy (ap) 
	struct vop_strategy_args /* {
	struct buf *a_bp
	} */ *ap;
{
	struct buf *bp=ap->a_bp;
	struct ucred *cr;
	struct proc *p;
	int error = 0;

	SMBVDEBUG("\n");
	if (bp->b_flags & B_PHYS)
		panic("smbfs physio");
	if (bp->b_flags & B_ASYNC)
		p = (struct proc *)0;
	else
		p = curproc;	/* XXX */
	if (bp->b_iocmd == BIO_READ)
		cr = bp->b_rcred;
	else
		cr = bp->b_wcred;

	if ((bp->b_flags & B_ASYNC) == 0 )
		error = smbfs_doio(bp, cr, p);
	return error;
}

static int
smbfs_bmap(ap)
	struct vop_bmap_args /* {
		struct vnode *a_vp;
		daddr_t  a_bn;
		struct vnode **a_vpp;
		daddr_t *a_bnp;
		int *a_runp;
		int *a_runb;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;

	if (ap->a_vpp != NULL)
		*ap->a_vpp = vp;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn * btodb(vp->v_mount->mnt_stat.f_iosize);
	if (ap->a_runp != NULL)
		*ap->a_runp = 0;
	if (ap->a_runb != NULL)
		*ap->a_runb = 0;
	return (0);
}

int
smbfs_ioctl(ap)
	struct vop_ioctl_args /* {
		struct vnode *a_vp;
		u_long a_command;
		caddr_t a_data;
		int fflag;
		struct ucred *cred;
		struct proc *p;
	} */ *ap;
{
	return EINVAL;
}

static char smbfs_atl[] = "rhsvda";
static int
smbfs_getextattr(struct vop_getextattr_args *ap)
/* {
        IN struct vnode *a_vp;
        IN char *a_name;
        INOUT struct uio *a_uio;
        IN struct ucred *a_cred;
        IN struct proc *a_p;
};
*/
{
	struct vnode *vp = ap->a_vp;
	struct proc *p = ap->a_p;
	struct ucred *cred = ap->a_cred;
	struct uio *uio = ap->a_uio;
	const char *name = ap->a_name;
	struct smbnode *np = VTOSMB(vp);
	struct vattr vattr;
	char buf[10];
	int i, attr, error;

	error = VOP_ACCESS(vp, VREAD, cred, p);
	if (error)
		return error;
	error = VOP_GETATTR(vp, &vattr, cred, p);
	if (error)
		return error;
	if (strcmp(name, "dosattr") == 0) {
		attr = np->n_dosattr;
		for (i = 0; i < 6; i++, attr >>= 1)
			buf[i] = (attr & 1) ? smbfs_atl[i] : '-';
		buf[i] = 0;
		error = uiomove(buf, i, uio);
		
	} else
		error = EINVAL;
	return error;
}

/*
 * Since we expected to support F_GETLK (and SMB protocol has no such function),
 * it is necessary to use lf_advlock(). It would be nice if this function had
 * a callback mechanism because it will help to improve a level of consistency.
 */
int
smbfs_advlock(ap)
	struct vop_advlock_args /* {
		struct vnode *a_vp;
		caddr_t  a_id;
		int  a_op;
		struct flock *a_fl;
		int  a_flags;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	struct flock *fl = ap->a_fl;
	caddr_t id = (caddr_t)1 /* ap->a_id */;
/*	int flags = ap->a_flags;*/
	struct proc *p = curproc;
	struct smb_cred scred;
	off_t start, end, size;
	int error, lkop;

	if (vp->v_type == VDIR) {
		/*
		 * SMB protocol have no support for directory locking.
		 * Although locks can be processed on local machine, I don't
		 * think that this is a good idea, because some programs
		 * can work wrong assuming directory is locked. So, we just
		 * return 'operation not supported
		 */
		 return EOPNOTSUPP;
	}
	size = np->n_size;
	switch (fl->l_whence) {
	    case SEEK_SET:
	    case SEEK_CUR:
		start = fl->l_start;
		break;
	    case SEEK_END:
		start = fl->l_start + size;
	    default:
		return EINVAL;
	}
	if (start < 0)
		return EINVAL;
	if (fl->l_len == 0)
		end = -1;
	else {
		end = start + fl->l_len - 1;
		if (end < start)
			return EINVAL;
	}
	smb_makescred(&scred, p, p ? p->p_ucred : NULL);
	switch (ap->a_op) {
	    case F_SETLK:
		switch (fl->l_type) {
		    case F_WRLCK:
			lkop = SMB_LOCK_EXCL;
			break;
		    case F_RDLCK:
			lkop = SMB_LOCK_SHARED;
			break;
		    case F_UNLCK:
			lkop = SMB_LOCK_RELEASE;
			break;
		    default:
			return EINVAL;
		}
		error = lf_advlock(ap, &np->n_lockf, size);
		if (error)
			break;
		lkop = SMB_LOCK_EXCL;
		error = smbfs_smb_lock(np, lkop, id, start, end, &scred);
		if (error) {
			ap->a_op = F_UNLCK;
			lf_advlock(ap, &np->n_lockf, size);
		}
		break;
	    case F_UNLCK:
		lf_advlock(ap, &np->n_lockf, size);
		error = smbfs_smb_lock(np, SMB_LOCK_RELEASE, id, start, end, &scred);
		break;
	    case F_GETLK:
		error = lf_advlock(ap, &np->n_lockf, size);
		break;
	    default:
		return EINVAL;
	}
	return error;
}

static int
smbfs_pathcheck(struct smbmount *smp, const char *name, int nmlen, int nameiop)
{
	static const char *badchars = "*/\[]:<>=;?";
	static const char *badchars83 = " +|,";
	const char *cp;
	int i, error;

	if (nameiop == LOOKUP)
		return 0;
	error = ENOENT;
	if (SMB_DIALECT(SSTOVC(smp->sm_share)) < SMB_DIALECT_LANMAN2_0) {
		/*
		 * Name should conform 8.3 format
		 */
		if (nmlen > 12)
			return ENAMETOOLONG;
		cp = index(name, '.');
		if (cp == NULL)
			return error;
		if (cp == name || (cp - name) > 8)
			return error;
		cp = index(cp + 1, '.');
		if (cp != NULL)
			return error;
		for (cp = name, i = 0; i < nmlen; i++, cp++)
			if (index(badchars83, *cp) != NULL)
				return error;
	}
	for (cp = name, i = 0; i < nmlen; i++, cp++)
		if (index(badchars, *cp) != NULL)
			return error;
	return 0;
}

#ifndef PDIRUNLOCK
#define	PDIRUNLOCK	0
#endif

/*
 * Things go even weird without fixed inode numbers...
 */
int
smbfs_lookup(ap)
	struct vop_lookup_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	struct proc *p = cnp->cn_proc;
	struct vnode *dvp = ap->a_dvp;
	struct vnode **vpp = ap->a_vpp;
	struct vnode *vp;
	struct smbmount *smp;
	struct mount *mp = dvp->v_mount;
	struct smbnode *dnp;
	struct smbfattr fattr, *fap;
	struct smb_cred scred;
	char *name = cnp->cn_nameptr;
	int flags = cnp->cn_flags;
	int nameiop = cnp->cn_nameiop;
	int nmlen = cnp->cn_namelen;
	int lockparent, wantparent, error, islastcn, isdot;
	
	SMBVDEBUG("\n");
	cnp->cn_flags &= ~PDIRUNLOCK;
	if (dvp->v_type != VDIR)
		return ENOTDIR;
	if ((flags & ISDOTDOT) && (dvp->v_flag & VROOT)) {
		SMBFSERR("invalid '..'\n");
		return EIO;
	}
#ifdef SMB_VNODE_DEBUG
	{
		char *cp, c;

		cp = name + nmlen;
		c = *cp;
		*cp = 0;
		SMBVDEBUG("%d '%s' in '%s' id=d\n", nameiop, name, 
			VTOSMB(dvp)->n_name);
		*cp = c;
	}
#endif
	islastcn = flags & ISLASTCN;
	if (islastcn && (mp->mnt_flag & MNT_RDONLY) && (nameiop != LOOKUP))
		return EROFS;
	if ((error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred, p)) != 0)
		return error;
	lockparent = flags & LOCKPARENT;
	wantparent = flags & (LOCKPARENT|WANTPARENT);
	smp = VFSTOSMBFS(mp);
	dnp = VTOSMB(dvp);
	isdot = (nmlen == 1 && name[0] == '.');

	error = smbfs_pathcheck(smp, cnp->cn_nameptr, cnp->cn_namelen, nameiop);

	if (error) 
		return ENOENT;

	error = cache_lookup(dvp, vpp, cnp);
	SMBVDEBUG("cache_lookup returned %d\n", error);
	if (error > 0)
		return error;
	if (error) {		/* name was found */
		struct vattr vattr;
		int vpid;

		vp = *vpp;
		vpid = vp->v_id;
		if (dvp == vp) {	/* lookup on current */
			vref(vp);
			error = 0;
			SMBVDEBUG("cached '.'\n");
		} else if (flags & ISDOTDOT) {
			VOP_UNLOCK(dvp, 0, p);	/* unlock parent */
			cnp->cn_flags |= PDIRUNLOCK;
			error = vget(vp, LK_EXCLUSIVE, p);
			if (!error && lockparent && islastcn) {
				error = vn_lock(dvp, LK_EXCLUSIVE, p);
				if (error == 0)
					cnp->cn_flags &= ~PDIRUNLOCK;
			}
		} else {
			error = vget(vp, LK_EXCLUSIVE, p);
			if (!lockparent || error || !islastcn) {
				VOP_UNLOCK(dvp, 0, p);
				cnp->cn_flags |= PDIRUNLOCK;
			}
		}
		if (!error) {
			if (vpid == vp->v_id) {
			   if (!VOP_GETATTR(vp, &vattr, cnp->cn_cred, p)
			/*    && vattr.va_ctime.tv_sec == VTOSMB(vp)->n_ctime*/) {
				if (nameiop != LOOKUP && islastcn)
					cnp->cn_flags |= SAVENAME;
				SMBVDEBUG("use cached vnode\n");
				return (0);
			   }
			   cache_purge(vp);
			}
			vput(vp);
			if (lockparent && dvp != vp && islastcn)
				VOP_UNLOCK(dvp, 0, p);
		}
		error = vn_lock(dvp, LK_EXCLUSIVE, p);
		*vpp = NULLVP;
		if (error) {
			cnp->cn_flags |= PDIRUNLOCK;
			return (error);
		}
		cnp->cn_flags &= ~PDIRUNLOCK;
	}
	/* 
	 * entry is not in the cache or has been expired
	 */
	error = 0;
	*vpp = NULLVP;
	smb_makescred(&scred, p, cnp->cn_cred);
	fap = &fattr;
	if (flags & ISDOTDOT) {
		error = smbfs_smb_lookup(dnp->n_parent, NULL, 0, fap, &scred);
		SMBVDEBUG("result of dotdot lookup: %d\n", error);
	} else {
		fap = &fattr;
		error = smbfs_smb_lookup(dnp, name, nmlen, fap, &scred);
/*		if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.')*/
		SMBVDEBUG("result of smbfs_smb_lookup: %d\n", error);
	}
	if (error && error != ENOENT)
		return error;
	if (error) {			/* entry not found */
		/*
		 * Handle RENAME or CREATE case...
		 */
		if ((nameiop == CREATE || nameiop == RENAME) && wantparent && islastcn) {
			cnp->cn_flags |= SAVENAME;
			if (!lockparent) {
				VOP_UNLOCK(dvp, 0, p);
				cnp->cn_flags |= PDIRUNLOCK;
			}
			return (EJUSTRETURN);
		}
		return ENOENT;
	}/* else {
		SMBVDEBUG("Found entry %s with id=%d\n", fap->entryName, fap->dirEntNum);
	}*/
	/*
	 * handle DELETE case ...
	 */
	if (nameiop == DELETE && islastcn) { 	/* delete last component */
		error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred, p);
		if (error)
			return error;
		if (isdot) {
			VREF(dvp);
			*vpp = dvp;
			return 0;
		}
		error = smbfs_nget(mp, dvp, name, nmlen, fap, &vp);
		if (error)
			return error;
		*vpp = vp;
		cnp->cn_flags |= SAVENAME;
		if (!lockparent) {
			VOP_UNLOCK(dvp, 0, p);
			cnp->cn_flags |= PDIRUNLOCK;
		}
		return 0;
	}
	if (nameiop == RENAME && islastcn && wantparent) {
		error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred, p);
		if (error)
			return error;
		if (isdot)
			return EISDIR;
		error = smbfs_nget(mp, dvp, name, nmlen, fap, &vp);
		if (error)
			return error;
		*vpp = vp;
		cnp->cn_flags |= SAVENAME;
		if (!lockparent) {
			VOP_UNLOCK(dvp, 0, p);
			cnp->cn_flags |= PDIRUNLOCK;
		}
		return 0;
	}
	if (flags & ISDOTDOT) {
		VOP_UNLOCK(dvp, 0, p);
		error = smbfs_nget(mp, dvp, name, nmlen, NULL, &vp);
		if (error) {
			vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY, p);
			return error;
		}
		if (lockparent && islastcn) {
			error = vn_lock(dvp, LK_EXCLUSIVE, p);
			if (error) {
				cnp->cn_flags |= PDIRUNLOCK;
				vput(vp);
				return error;
			}
		}
		*vpp = vp;
	} else if (isdot) {
		vref(dvp);
		*vpp = dvp;
	} else {
		error = smbfs_nget(mp, dvp, name, nmlen, fap, &vp);
		if (error)
			return error;
		*vpp = vp;
		SMBVDEBUG("lookup: getnewvp!\n");
		if (!lockparent || !islastcn) {
			VOP_UNLOCK(dvp, 0, p);
			cnp->cn_flags |= PDIRUNLOCK;
		}
	}
	if ((cnp->cn_flags & MAKEENTRY)/* && !islastcn*/) {
/*		VTOSMB(*vpp)->n_ctime = VTOSMB(*vpp)->n_vattr.va_ctime.tv_sec;*/
		cache_enter(dvp, *vpp, cnp);
	}
	return 0;
}
