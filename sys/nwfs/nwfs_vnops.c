/*
 * Copyright (c) 1999, 2000 Boris Popov
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
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/unistd.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <machine/mutex.h>

#include <netncp/ncp.h>
#include <netncp/ncp_conn.h>
#include <netncp/ncp_subr.h>
#include <netncp/nwerror.h>
#include <netncp/ncp_nls.h>

#include <nwfs/nwfs.h>
#include <nwfs/nwfs_node.h>
#include <nwfs/nwfs_subr.h>

/*
 * Prototypes for NWFS vnode operations
 */
static int nwfs_create(struct vop_create_args *);
static int nwfs_mknod(struct vop_mknod_args *);
static int nwfs_open(struct vop_open_args *);
static int nwfs_close(struct vop_close_args *);
static int nwfs_access(struct vop_access_args *);
static int nwfs_getattr(struct vop_getattr_args *);
static int nwfs_setattr(struct vop_setattr_args *);
static int nwfs_read(struct vop_read_args *);
static int nwfs_write(struct vop_write_args *);
static int nwfs_fsync(struct vop_fsync_args *);
static int nwfs_remove(struct vop_remove_args *);
static int nwfs_link(struct vop_link_args *);
static int nwfs_lookup(struct vop_lookup_args *);
static int nwfs_rename(struct vop_rename_args *);
static int nwfs_mkdir(struct vop_mkdir_args *);
static int nwfs_rmdir(struct vop_rmdir_args *);
static int nwfs_symlink(struct vop_symlink_args *);
static int nwfs_readdir(struct vop_readdir_args *);
static int nwfs_bmap(struct vop_bmap_args *);
static int nwfs_strategy(struct vop_strategy_args *);
static int nwfs_print(struct vop_print_args *);
static int nwfs_pathconf(struct vop_pathconf_args *ap);

/* Global vfs data structures for nwfs */
vop_t **nwfs_vnodeop_p;
static struct vnodeopv_entry_desc nwfs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *) vop_defaultop },
	{ &vop_access_desc,		(vop_t *) nwfs_access },
	{ &vop_bmap_desc,		(vop_t *) nwfs_bmap },
	{ &vop_open_desc,		(vop_t *) nwfs_open },
	{ &vop_close_desc,		(vop_t *) nwfs_close },
	{ &vop_create_desc,		(vop_t *) nwfs_create },
	{ &vop_fsync_desc,		(vop_t *) nwfs_fsync },
	{ &vop_getattr_desc,		(vop_t *) nwfs_getattr },
	{ &vop_getpages_desc,		(vop_t *) nwfs_getpages },
	{ &vop_putpages_desc,		(vop_t *) nwfs_putpages },
	{ &vop_ioctl_desc,		(vop_t *) nwfs_ioctl },
	{ &vop_inactive_desc,		(vop_t *) nwfs_inactive },
	{ &vop_islocked_desc,		(vop_t *) vop_stdislocked },
	{ &vop_link_desc,		(vop_t *) nwfs_link },
	{ &vop_lock_desc,		(vop_t *) vop_stdlock },
	{ &vop_lookup_desc,		(vop_t *) nwfs_lookup },
	{ &vop_mkdir_desc,		(vop_t *) nwfs_mkdir },
	{ &vop_mknod_desc,		(vop_t *) nwfs_mknod },
	{ &vop_pathconf_desc,		(vop_t *) nwfs_pathconf },
	{ &vop_print_desc,		(vop_t *) nwfs_print },
	{ &vop_read_desc,		(vop_t *) nwfs_read },
	{ &vop_readdir_desc,		(vop_t *) nwfs_readdir },
	{ &vop_reclaim_desc,		(vop_t *) nwfs_reclaim },
	{ &vop_remove_desc,		(vop_t *) nwfs_remove },
	{ &vop_rename_desc,		(vop_t *) nwfs_rename },
	{ &vop_rmdir_desc,		(vop_t *) nwfs_rmdir },
	{ &vop_setattr_desc,		(vop_t *) nwfs_setattr },
	{ &vop_strategy_desc,		(vop_t *) nwfs_strategy },
	{ &vop_symlink_desc,		(vop_t *) nwfs_symlink },
	{ &vop_unlock_desc,		(vop_t *) vop_stdunlock },
	{ &vop_write_desc,		(vop_t *) nwfs_write },
	{ NULL, NULL }
};
static struct vnodeopv_desc nwfs_vnodeop_opv_desc =
	{ &nwfs_vnodeop_p, nwfs_vnodeop_entries };

VNODEOP_SET(nwfs_vnodeop_opv_desc);

void nwfs_createname(char *_name,struct vnode *vp,struct componentname *cnp);


void nwfs_createname(char *_name,struct vnode *vp,struct componentname *cnp){
	strncpy(_name, cnp->cn_nameptr, cnp->cn_namelen);
	_name[cnp->cn_namelen] = '\0';
	ncp_str_upper(_name);
}
/*
 * nwfs_access vnode op
 * for now just return ok
 */
static int
nwfs_access(ap)
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
	struct nwmount *nmp = VTONWFS(vp);
	int error = 0;

	NCPVNDEBUG("\n");
	if ((ap->a_mode & VWRITE) && (vp->v_mount->mnt_flag & MNT_RDONLY)) {
		switch (vp->v_type) {
		    case VREG: case VDIR: case VLNK:
			return (EROFS);
		    default:
			break;
		}
	}
	if (cred->cr_uid == 0)
		return 0;
	if (cred->cr_uid != nmp->m.uid) {
		mode >>= 3;
		if (!groupmember(nmp->m.gid, cred))
			mode >>= 3;
	}
	error = (((vp->v_type == VREG) ? nmp->m.file_mode : nmp->m.dir_mode) & mode) == mode ? 0 : EACCES;
	return error;
}
/*
 * nwfs_open vnode op
 */
/* ARGSUSED */
static int
nwfs_open(ap)
	struct vop_open_args /* {
		struct vnode *a_vp;
		int  a_mode;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	int mode = ap->a_mode;
	struct nwnode *np = VTONW(vp);
	struct ncp_open_info no;
	struct nwmount *nmp = VTONWFS(vp);
	struct vattr vattr;
	int error, nwm;

	NCPVNDEBUG("%s,%d\n",np->n_name, np->opened);
	if (vp->v_type != VREG && vp->v_type != VDIR) { 
		NCPFATAL("open vtype = %d\n", vp->v_type);
		return (EACCES);
	}
	if (vp->v_type == VDIR) return 0;	/* nothing to do now */
	if (np->n_flag & NMODIFIED) {
		if ((error = nwfs_vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p, 1)) == EINTR)
			return (error);
		np->n_atime = 0;
		error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
		if (error) return (error);
		np->n_mtime = vattr.va_mtime.tv_sec;
	} else {
		error = VOP_GETATTR(vp, &vattr, ap->a_cred, ap->a_p);
		if (error) return (error);
		if (np->n_mtime != vattr.va_mtime.tv_sec) {
			if ((error = nwfs_vinvalbuf(vp, V_SAVE,	ap->a_cred, ap->a_p, 1)) == EINTR)
				return (error);
			np->n_mtime = vattr.va_mtime.tv_sec;
		}
	}
	if (np->opened) {
		np->opened++;
		return 0;
	}
	nwm = AR_READ;
	if ((vp->v_mount->mnt_flag & MNT_RDONLY) == 0)
		nwm |= AR_WRITE;
	error = ncp_open_create_file_or_subdir(nmp, vp, 0, NULL, OC_MODE_OPEN,
					       0, nwm, &no, ap->a_p, ap->a_cred);
	if (error) {
		if (mode & FWRITE)
			return EACCES;
		nwm = AR_READ;
		error = ncp_open_create_file_or_subdir(nmp, vp, 0, NULL, OC_MODE_OPEN, 0,
						   nwm, &no, ap->a_p,ap->a_cred);
	}
	if (!error) {
		np->opened++;
		np->n_fh = no.fh;
		np->n_origfh = no.origfh;
	}
	np->n_atime = 0;
	return (error);
}

static int
nwfs_close(ap)
	struct vop_close_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_vp;
		int  a_fflag;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct nwnode *np = VTONW(vp);
	int error;

	NCPVNDEBUG("name=%s,pid=%d,c=%d\n",np->n_name,ap->a_p->p_pid,np->opened);

	if (vp->v_type == VDIR) return 0;	/* nothing to do now */
	error = 0;
	mtx_enter(&vp->v_interlock, MTX_DEF);
	if (np->opened == 0) {
		mtx_exit(&vp->v_interlock, MTX_DEF);
		return 0;
	}
	mtx_exit(&vp->v_interlock, MTX_DEF);
	error = nwfs_vinvalbuf(vp, V_SAVE, ap->a_cred, ap->a_p, 1);
	mtx_enter(&vp->v_interlock, MTX_DEF);
	if (np->opened == 0) {
		mtx_exit(&vp->v_interlock, MTX_DEF);
		return 0;
	}
	if (--np->opened == 0) {
		mtx_exit(&vp->v_interlock, MTX_DEF);
		error = ncp_close_file(NWFSTOCONN(VTONWFS(vp)), &np->n_fh, 
		   ap->a_p, ap->a_cred);
	} else
		mtx_exit(&vp->v_interlock, MTX_DEF);
	np->n_atime = 0;
	return (error);
}

/*
 * nwfs_getattr call from vfs.
 */
static int
nwfs_getattr(ap)
	struct vop_getattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct nwnode *np = VTONW(vp);
	struct vattr *va=ap->a_vap;
	struct nwmount *nmp = VTONWFS(vp);
	struct nw_entry_info fattr;
	int error;
	u_int32_t oldsize;

	NCPVNDEBUG("%lx:%d: '%s' %d\n", (long)vp, nmp->n_volume, np->n_name, (vp->v_flag & VROOT) != 0);
	error = nwfs_attr_cachelookup(vp,va);
	if (!error) return 0;
	NCPVNDEBUG("not in cache\n");
	oldsize = np->n_size;
	if (np->n_flag & NVOLUME) {
		error = ncp_obtain_info(nmp, np->n_fid.f_id, 0, NULL, &fattr,
		    ap->a_p,ap->a_cred);
	} else {
		error = ncp_obtain_info(nmp, np->n_fid.f_parent, np->n_nmlen, 
		    np->n_name, &fattr, ap->a_p, ap->a_cred);
	}
	if (error) {
		NCPVNDEBUG("error %d\n", error);
		return error;
	}
	nwfs_attr_cacheenter(vp, &fattr);
	*va = np->n_vattr;
	if (np->opened)
		np->n_size = oldsize;
	return (0);
}
/*
 * nwfs_setattr call from vfs.
 */
static int
nwfs_setattr(ap)
	struct vop_setattr_args /* {
		struct vnode *a_vp;
		struct vattr *a_vap;
		struct ucred *a_cred;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct nwnode *np = VTONW(vp);
	struct vattr *vap = ap->a_vap;
	u_quad_t tsize=0;
	int error = 0;

	NCPVNDEBUG("\n");
	if (vap->va_flags != VNOVAL)
		return (EOPNOTSUPP);
	/*
	 * Disallow write attempts if the filesystem is mounted read-only.
	 */
  	if ((vap->va_uid != (uid_t)VNOVAL || vap->va_gid != (gid_t)VNOVAL || 
	     vap->va_atime.tv_sec != VNOVAL || vap->va_mtime.tv_sec != VNOVAL ||
	     vap->va_mode != (mode_t)VNOVAL) &&(vp->v_mount->mnt_flag & MNT_RDONLY))
		return (EROFS);
	if (vap->va_size != VNOVAL) {
 		switch (vp->v_type) {
 		case VDIR:
 			return (EISDIR);
 		case VREG:
			/*
			 * Disallow write attempts if the filesystem is
			 * mounted read-only.
			 */
			if (vp->v_mount->mnt_flag & MNT_RDONLY)
				return (EROFS);
			vnode_pager_setsize(vp, (u_long)vap->va_size);
 			tsize = np->n_size;
 			np->n_size = vap->va_size;
			break;
 		default:
			return EINVAL;
  		};
  	}
	error = ncp_setattr(vp, vap, ap->a_cred, ap->a_p);
	if (error && vap->va_size != VNOVAL) {
		np->n_size = tsize;
		vnode_pager_setsize(vp, (u_long)tsize);
	}
	np->n_atime = 0;	/* invalidate cache */
	VOP_GETATTR(vp, vap, ap->a_cred, ap->a_p);
	np->n_mtime = vap->va_mtime.tv_sec;
	return (0);
}
/*
 * nwfs_read call.
 */
static int
nwfs_read(ap)
	struct vop_read_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio=ap->a_uio;
	int error;
	NCPVNDEBUG("nwfs_read:\n");

	if (vp->v_type != VREG && vp->v_type != VDIR)
		return (EPERM);
	error = nwfs_readvnode(vp, uio, ap->a_cred);
	return error;
}

static int
nwfs_write(ap)
	struct vop_write_args /* {
		struct vnode *a_vp;
		struct uio *a_uio;
		int  a_ioflag;
		struct ucred *a_cred;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct uio *uio = ap->a_uio;
	int error;

	NCPVNDEBUG("%d,ofs=%d,sz=%d\n",vp->v_type, (int)uio->uio_offset, uio->uio_resid);

	if (vp->v_type != VREG)
		return (EPERM);
	error = nwfs_writevnode(vp, uio, ap->a_cred,ap->a_ioflag);
	return(error);
}
/*
 * nwfs_create call
 * Create a regular file. On entry the directory to contain the file being
 * created is locked.  We must release before we return. We must also free
 * the pathname buffer pointed at by cnp->cn_pnbuf, always on error, or
 * only if the SAVESTART bit in cn_flags is clear on success.
 */
static int
nwfs_create(ap)
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
	struct vnode *vp = (struct vnode *)0;
	int error = 0, fmode;
	struct vattr vattr;
	struct nwnode *np;
	struct ncp_open_info no;
	struct nwmount *nmp=VTONWFS(dvp);
	ncpfid fid;
	

	NCPVNDEBUG("\n");
	*vpp = NULL;
	if (vap->va_type == VSOCK)
		return (EOPNOTSUPP);
	if ((error = VOP_GETATTR(dvp, &vattr, cnp->cn_cred, cnp->cn_proc))) {
		return (error);
	}
	fmode = AR_READ | AR_WRITE;
/*	if (vap->va_vaflags & VA_EXCLUSIVE)
		fmode |= AR_DENY_READ | AR_DENY_WRITE;*/
	
	error = ncp_open_create_file_or_subdir(nmp,dvp,cnp->cn_namelen,cnp->cn_nameptr, 
			   OC_MODE_CREATE | OC_MODE_OPEN | OC_MODE_REPLACE,
			   0, fmode, &no, cnp->cn_proc, cnp->cn_cred);
	if (!error) {
		error = ncp_close_file(NWFSTOCONN(nmp), &no.fh, cnp->cn_proc,cnp->cn_cred);
		fid.f_parent = VTONW(dvp)->n_fid.f_id;
		fid.f_id = no.fattr.dirEntNum;
		error = nwfs_nget(VTOVFS(dvp), fid, &no.fattr, dvp, &vp);
		if (!error) {
			np = VTONW(vp);
			np->opened = 0;
			*vpp = vp;
		}
		if (cnp->cn_flags & MAKEENTRY)
			cache_enter(dvp, vp, cnp);
	}
	return (error);
}

/*
 * nwfs_remove call. It isn't possible to emulate UFS behaivour because
 * NetWare doesn't allow delete/rename operations on an opened file.
 */
static int
nwfs_remove(ap)
	struct vop_remove_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_dvp;
		struct vnode * a_vp;
		struct componentname * a_cnp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct nwnode *np = VTONW(vp);
	struct nwmount *nmp = VTONWFS(vp);
	int error;

	if (vp->v_type == VDIR || np->opened || vp->v_usecount != 1) {
		error = EPERM;
	} else if (!ncp_conn_valid(NWFSTOCONN(nmp))) {
		error = EIO;
	} else {
		cache_purge(vp);
		error = ncp_DeleteNSEntry(nmp, VTONW(dvp)->n_fid.f_id,
		    cnp->cn_namelen,cnp->cn_nameptr,cnp->cn_proc,cnp->cn_cred);
		if (error == 0)
			np->n_flag |= NSHOULDFREE;
		else if (error == 0x899c)
			error = EACCES;
	}
	return (error);
}

/*
 * nwfs_file rename call
 */
static int
nwfs_rename(ap)
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
	struct componentname *fcnp = ap->a_fcnp;
	struct nwmount *nmp=VTONWFS(fvp);
	u_int16_t oldtype = 6;
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
	if (tvp && tvp != fvp) {
		error = ncp_DeleteNSEntry(nmp, VTONW(tdvp)->n_fid.f_id,
		    tcnp->cn_namelen, tcnp->cn_nameptr, 
		    tcnp->cn_proc, tcnp->cn_cred);
		if (error == 0x899c) error = EACCES;
		if (error)
			goto out;
	}
	if (fvp->v_type == VDIR) {
		oldtype |= NW_TYPE_SUBDIR;
	} else if (fvp->v_type == VREG) {
		oldtype |= NW_TYPE_FILE;
	} else
		return EINVAL;
	error = ncp_nsrename(NWFSTOCONN(nmp), nmp->n_volume, nmp->name_space, 
		oldtype, &nmp->m.nls,
		VTONW(fdvp)->n_fid.f_id, fcnp->cn_nameptr, fcnp->cn_namelen,
		VTONW(tdvp)->n_fid.f_id, tcnp->cn_nameptr, tcnp->cn_namelen,
		tcnp->cn_proc,tcnp->cn_cred);

	if (error == 0x8992)
		error = EEXIST;
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
	nwfs_attr_cacheremove(fdvp);
	nwfs_attr_cacheremove(tdvp);
	/*
	 * Need to get rid of old vnodes, because netware will change
	 * file id on rename
	 */
	vgone(fvp);
	if (tvp)
		vgone(tvp);
	/*
	 * Kludge: Map ENOENT => 0 assuming that it is a reply to a retry.
	 */
	if (error == ENOENT)
		error = 0;
	return (error);
}

/*
 * nwfs hard link create call
 * Netware filesystems don't know what links are.
 */
static int
nwfs_link(ap)
	struct vop_link_args /* {
		struct vnode *a_tdvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	return EOPNOTSUPP;
}

/*
 * nwfs_symlink link create call
 * Netware filesystems don't know what symlinks are.
 */
static int
nwfs_symlink(ap)
	struct vop_symlink_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
		char *a_target;
	} */ *ap;
{
	return (EOPNOTSUPP);
}

static int nwfs_mknod(ap) 
	struct vop_mknod_args /* {
	} */ *ap;
{
	return (EOPNOTSUPP);
}

/*
 * nwfs_mkdir call
 */
static int
nwfs_mkdir(ap)
	struct vop_mkdir_args /* {
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
		struct vattr *a_vap;
	} */ *ap;
{
	struct vnode *dvp = ap->a_dvp;
/*	struct vattr *vap = ap->a_vap;*/
	struct componentname *cnp = ap->a_cnp;
	int len=cnp->cn_namelen;
	struct ncp_open_info no;
	struct nwnode *np;
	struct vnode *newvp = (struct vnode *)0;
	ncpfid fid;
	int error = 0;
	struct vattr vattr;
	char *name=cnp->cn_nameptr;

	if ((error = VOP_GETATTR(dvp, &vattr, cnp->cn_cred, cnp->cn_proc))) {
		return (error);
	}	
	if ((name[0] == '.') && ((len == 1) || ((len == 2) && (name[1] == '.')))) {
		return EEXIST;
	}
	if (ncp_open_create_file_or_subdir(VTONWFS(dvp),dvp, cnp->cn_namelen,
			cnp->cn_nameptr,OC_MODE_CREATE, aDIR, 0xffff,
			&no, cnp->cn_proc, cnp->cn_cred) != 0) {
		error = EACCES;
	} else {
		error = 0;
        }
	if (!error) {
		fid.f_parent = VTONW(dvp)->n_fid.f_id;
		fid.f_id = no.fattr.dirEntNum;
		error = nwfs_nget(VTOVFS(dvp), fid, &no.fattr, dvp, &newvp);
		if (!error) {
			np = VTONW(newvp);
			newvp->v_type = VDIR;
			*ap->a_vpp = newvp;
		}
	}
	return (error);
}

/*
 * nwfs_remove directory call
 */
static int
nwfs_rmdir(ap)
	struct vop_rmdir_args /* {
		struct vnode *a_dvp;
		struct vnode *a_vp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct vnode *dvp = ap->a_dvp;
	struct componentname *cnp = ap->a_cnp;
	struct nwnode *np = VTONW(vp);
	struct nwmount *nmp = VTONWFS(vp);
	struct nwnode *dnp = VTONW(dvp);
	int error = EIO;

	if (dvp == vp)
		return EINVAL;

	error = ncp_DeleteNSEntry(nmp, dnp->n_fid.f_id, 
		cnp->cn_namelen, cnp->cn_nameptr,cnp->cn_proc,cnp->cn_cred);
	if (error == 0)
		np->n_flag |= NSHOULDFREE;
	else if (error == NWE_DIR_NOT_EMPTY)
		error = ENOTEMPTY;
	dnp->n_flag |= NMODIFIED;
	nwfs_attr_cacheremove(dvp);
	cache_purge(dvp);
	cache_purge(vp);
	return (error);
}

/*
 * nwfs_readdir call
 */
static int
nwfs_readdir(ap)
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
	if (ap->a_ncookies) {
		printf("nwfs_readdir: no support for cookies now...");
		return (EOPNOTSUPP);
	}

	error = nwfs_readvnode(vp, uio, ap->a_cred);
	return error;
}
/* ARGSUSED */
static int
nwfs_fsync(ap)
	struct vop_fsync_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode * a_vp;
		struct ucred * a_cred;
		int  a_waitfor;
		struct proc * a_p;
	} */ *ap;
{
/*	return (nfs_flush(ap->a_vp, ap->a_cred, ap->a_waitfor, ap->a_p, 1));*/
    return (0);
}

/* ARGSUSED */
static 
int nwfs_print (ap) 
	struct vop_print_args /* {
		struct vnode *a_vp;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct nwnode *np = VTONW(vp);

	printf("nwfs node: name = '%s', fid = %d, pfid = %d\n",
	    np->n_name, np->n_fid.f_id, np->n_fid.f_parent);
	return (0);
}

static int nwfs_pathconf (ap)
	struct vop_pathconf_args  /* {
	struct vnode *vp;
	int name;
	register_t *retval;
	} */ *ap;
{
	int name=ap->a_name, error=0;
	register_t *retval=ap->a_retval;
	
	switch(name){
		case _PC_LINK_MAX:
		        *retval=0;
			break;
		case _PC_NAME_MAX:
			*retval=NCP_MAX_FILENAME; /* XXX from nwfsnode */
			break;
		case _PC_PATH_MAX:
			*retval=NCP_MAXPATHLEN; /* XXX from nwfsnode */
			break;
		default:
			error=EINVAL;
	}
	return(error);
}

static int nwfs_strategy (ap) 
	struct vop_strategy_args /* {
	struct buf *a_bp
	} */ *ap;
{
	struct buf *bp=ap->a_bp;
	struct ucred *cr;
	struct proc *p;
	int error = 0;

	NCPVNDEBUG("\n");
	if (bp->b_flags & B_PHYS)
		panic("nwfs physio");
	if (bp->b_flags & B_ASYNC)
		p = (struct proc *)0;
	else
		p = curproc;	/* XXX */
	if (bp->b_iocmd == BIO_READ)
		cr = bp->b_rcred;
	else
		cr = bp->b_wcred;
	/*
	 * If the op is asynchronous and an i/o daemon is waiting
	 * queue the request, wake it up and wait for completion
	 * otherwise just do it ourselves.
	 */
	if ((bp->b_flags & B_ASYNC) == 0 )
		error = nwfs_doio(bp, cr, p);
	return (error);
}

static int
nwfs_bmap(ap)
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
nwfs_nget(struct mount *mp, ncpfid fid, struct nw_entry_info *fap,
	  struct vnode *dvp, struct vnode **vpp)
{
	int error;
	struct nwnode *newnp;
	struct vnode *vp;

	*vpp = NULL;
	error = nwfs_allocvp(mp, fid, &vp);
	if (error)
		return error;
	newnp = VTONW(vp);
	if (fap) {
		if (newnp->n_flag & NNEW) {
			newnp->n_nmlen = fap->nameLen;
			bcopy(fap->entryName, newnp->n_name, newnp->n_nmlen);
			newnp->n_name[fap->nameLen] = 0;
		}
		newnp->n_attr = fap->attributes;
		newnp->n_dosfid = fap->DosDirNum;
		vp->v_type = newnp->n_attr & aDIR ? VDIR : VREG;
		nwfs_attr_cacheenter(vp, fap);
	}
	if (dvp) {
		newnp->n_parent = VTONW(dvp)->n_fid;
		if ((newnp->n_flag & NNEW) && vp->v_type == VDIR) {
			if ((dvp->v_flag & VROOT) == 0) {
				newnp->n_refparent = 1;
				vref(dvp);	/* vhold */
			}
		}
	} else {
		if ((newnp->n_flag & NNEW) && vp->v_type == VREG)
			printf("new vnode '%s' borned without parent ?\n",newnp->n_name);
	}
	newnp->n_flag &= ~NNEW;
	*vpp = vp;
	return 0;
}

/*
 * How to keep the brain busy ...
 * Currently lookup routine can make two lookup for vnode. This can be
 * avoided by reorg the code.
 */
int
nwfs_lookup(ap)
	struct vop_lookup_args /* {
		struct vnodeop_desc *a_desc;
		struct vnode *a_dvp;
		struct vnode **a_vpp;
		struct componentname *a_cnp;
	} */ *ap;
{
	struct componentname *cnp = ap->a_cnp;
	struct vnode *dvp = ap->a_dvp;
	struct vnode **vpp = ap->a_vpp;
	int flags = cnp->cn_flags;
	struct vnode *vp;
	struct nwmount *nmp;
	struct mount *mp = dvp->v_mount;
	struct nwnode *dnp, *npp;
	struct nw_entry_info fattr, *fap;
	ncpfid fid;
	int nameiop=cnp->cn_nameiop, islastcn;
	int lockparent, wantparent, error = 0, notfound;
	struct proc *p = cnp->cn_proc;
	char _name[cnp->cn_namelen+1];
	bcopy(cnp->cn_nameptr,_name,cnp->cn_namelen);
	_name[cnp->cn_namelen]=0;
	
	if (dvp->v_type != VDIR)
		return (ENOTDIR);
	if ((flags & ISDOTDOT) && (dvp->v_flag & VROOT)) {
		printf("nwfs_lookup: invalid '..'\n");
		return EIO;
	}

	NCPVNDEBUG("%d '%s' in '%s' id=d\n", nameiop, _name, 
		VTONW(dvp)->n_name/*, VTONW(dvp)->n_name*/);

	islastcn = flags & ISLASTCN;
	if (islastcn && (mp->mnt_flag & MNT_RDONLY) && (nameiop != LOOKUP))
		return (EROFS);
	if ((error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred, p)))
		return (error);
	lockparent = flags & LOCKPARENT;
	wantparent = flags & (LOCKPARENT|WANTPARENT);
	nmp = VFSTONWFS(mp);
	dnp = VTONW(dvp);
/*
printf("dvp %d:%d:%d\n", (int)mp, (int)dvp->v_flag & VROOT, (int)flags & ISDOTDOT);
*/
	error = ncp_pathcheck(cnp->cn_nameptr, cnp->cn_namelen, &nmp->m.nls, 
	    (nameiop == CREATE || nameiop == RENAME) && (nmp->m.nls.opt & NWHP_NOSTRICT) == 0);
	if (error) 
	    return ENOENT;

	error = cache_lookup(dvp, vpp, cnp);
	NCPVNDEBUG("cache_lookup returned %d\n",error);
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
			NCPVNDEBUG("cached '.'");
		} else if (flags & ISDOTDOT) {
			VOP_UNLOCK(dvp, 0, p);	/* unlock parent */
			error = vget(vp, LK_EXCLUSIVE, p);
			if (!error && lockparent && islastcn)
				error = vn_lock(dvp, LK_EXCLUSIVE, p);
		} else {
			error = vget(vp, LK_EXCLUSIVE, p);
			if (!lockparent || error || !islastcn)
				VOP_UNLOCK(dvp, 0, p);
		}
		if (!error) {
			if (vpid == vp->v_id) {
			   if (!VOP_GETATTR(vp, &vattr, cnp->cn_cred, p)
			    && vattr.va_ctime.tv_sec == VTONW(vp)->n_ctime) {
				if (nameiop != LOOKUP && islastcn)
					cnp->cn_flags |= SAVENAME;
				NCPVNDEBUG("use cached vnode");
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
		if (error)
			return (error);
	}
	/* not in cache, so ...  */
	error = 0;
	*vpp = NULLVP;
	fap = NULL;
	if (flags & ISDOTDOT) {
		if (NWCMPF(&dnp->n_parent, &nmp->n_rootent)) {
			fid = nmp->n_rootent;
			fap = NULL;
			notfound = 0;
		} else {
			error = nwfs_lookupnp(nmp, dnp->n_parent, p, &npp);
			if (error) {
				return error;
			}
			fid = dnp->n_parent;
			fap = &fattr;
			/*np = *npp;*/
			notfound = ncp_obtain_info(nmp, npp->n_dosfid,
			    0, NULL, fap, p, cnp->cn_cred);
		}
	} else {
		fap = &fattr;
		notfound = ncp_lookup(dvp, cnp->cn_namelen, cnp->cn_nameptr,
			fap, p, cnp->cn_cred);
		fid.f_id = fap->dirEntNum;
		if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
			fid.f_parent = dnp->n_fid.f_parent;
		} else
			fid.f_parent = dnp->n_fid.f_id;
		NCPVNDEBUG("call to ncp_lookup returned=%d\n",notfound);
	}
	if (notfound && notfound < 0x80 )
		return (notfound);	/* hard error */
	if (notfound) { /* entry not found */
		/* Handle RENAME or CREATE case... */
		if ((nameiop == CREATE || nameiop == RENAME) && wantparent && islastcn) {
			cnp->cn_flags |= SAVENAME;
			if (!lockparent)
				VOP_UNLOCK(dvp, 0, p);
			return (EJUSTRETURN);
		}
		return ENOENT;
	}/* else {
		NCPVNDEBUG("Found entry %s with id=%d\n", fap->entryName, fap->dirEntNum);
	}*/
	/* handle DELETE case ... */
	if (nameiop == DELETE && islastcn) { 	/* delete last component */
		error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred, cnp->cn_proc);
		if (error) return (error);
		if (NWCMPF(&dnp->n_fid, &fid)) {	/* we found ourselfs */
			VREF(dvp);
			*vpp = dvp;
			return 0;
		}
		error = nwfs_nget(mp, fid, fap, dvp, &vp);
		if (error) return (error);
		*vpp = vp;
		cnp->cn_flags |= SAVENAME;	/* I free it later */
		if (!lockparent) VOP_UNLOCK(dvp,0,p);
		return (0);
	}
	if (nameiop == RENAME && islastcn && wantparent) {
		error = VOP_ACCESS(dvp, VWRITE, cnp->cn_cred, cnp->cn_proc);
		if (error) return (error);
		if (NWCMPF(&dnp->n_fid, &fid)) return EISDIR;
		error = nwfs_nget(mp, fid, fap, dvp, &vp);
		if (error) return (error);
		*vpp = vp;
		cnp->cn_flags |= SAVENAME;
		if (!lockparent)
			VOP_UNLOCK(dvp,0,p);
		return (0);
	}
	if (flags & ISDOTDOT) {
		VOP_UNLOCK(dvp, 0, p);		/* race to get the inode */
		error = nwfs_nget(mp, fid, NULL, NULL, &vp);
		if (error) {
			vn_lock(dvp, LK_EXCLUSIVE | LK_RETRY, p);
			return (error);
		}
		if (lockparent && islastcn &&
		    (error = vn_lock(dvp, LK_EXCLUSIVE, p))) {
		    	vput(vp);
			return (error);
		}
		*vpp = vp;
	} else if (NWCMPF(&dnp->n_fid, &fid)) {
		vref(dvp);
		*vpp = dvp;
	} else {
		error = nwfs_nget(mp, fid, fap, dvp, &vp);
		if (error) return (error);
		*vpp = vp;
		NCPVNDEBUG("lookup: getnewvp!\n");
		if (!lockparent || !islastcn)
			VOP_UNLOCK(dvp, 0, p);
	}
	if ((cnp->cn_flags & MAKEENTRY)/* && !islastcn*/) {
		VTONW(*vpp)->n_ctime = VTONW(*vpp)->n_vattr.va_ctime.tv_sec;
		cache_enter(dvp, *vpp, cnp);
	}
	return (0);
}
