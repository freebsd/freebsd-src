/*-
 * Copyright (c) 2001 Dag-Erling Coïdan Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/ctype.h>
#include <sys/dirent.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/sbuf.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <fs/pseudofs/pseudofs.h>
#include <fs/pseudofs/pseudofs_internal.h>

/*
 * Verify permissions
 */
static int
pfs_access(struct vop_access_args *va)
{
	struct vnode *vn = va->a_vp;
	struct vattr vattr;
	int error;
	
	error = VOP_GETATTR(vn, &vattr, va->a_cred, va->a_p);
	if (error)
		return (error);
	error = vaccess(vn->v_type, vattr.va_mode, vattr.va_uid,
	    vattr.va_gid, va->a_mode, va->a_cred, NULL);
	return (error);
}

/*
 * Close a file or directory
 */
static int
pfs_close(struct vop_close_args *va)
{
	return (0);
}

/*
 * Get file attributes
 */
static int
pfs_getattr(struct vop_getattr_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_node *pn = (struct pfs_node *)vn->v_data;
	struct vattr *vap = va->a_vap;

	VATTR_NULL(vap);
	vap->va_type = vn->v_type;
	vap->va_mode = pn->pn_mode;
	vap->va_fileid = pn->pn_fileno;
	vap->va_flags = 0;
	vap->va_blocksize = PAGE_SIZE;
	vap->va_bytes = vap->va_size = 0;
	vap->va_fsid = vn->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_nlink = 1;
	nanotime(&vap->va_ctime);
	vap->va_atime = vap->va_mtime = vap->va_ctime;
	vap->va_uid = pn->pn_uid;
	vap->va_gid = pn->pn_gid;

	return (0);
}

/*
 * Look up a file or directory
 */
static int
pfs_lookup(struct vop_lookup_args *va)
{
	struct vnode *dvp = va->a_dvp;
	struct vnode **vpp = va->a_vpp;
	struct componentname *cnp = va->a_cnp;
#if 0
	struct pfs_info *pi = (struct pfs_info *)dvp->v_mount->mnt_data;
#endif
	struct pfs_node *pd = (struct pfs_node *)dvp->v_data, *pn;
	struct proc *p;
	char *pname;
	int error, i;
	pid_t pid;

	if (dvp->v_type != VDIR)
		return (ENOTDIR);
	
	/* don't support CREATE, RENAME or DELETE */
	if (cnp->cn_nameiop != LOOKUP)
		return (EROFS);

	/* shortcut */
	if (cnp->cn_namelen >= PFS_NAMELEN)
		return (ENOENT);
	
	/* self */
	pname = cnp->cn_nameptr;
	if (cnp->cn_namelen == 1 && *pname == '.') {
		pn = pd;
		*vpp = dvp;
		VREF(dvp);
		goto got_vnode;
	}

	/* parent */
	if (cnp->cn_flags & ISDOTDOT) {
		if (pd->pn_type == pfstype_root)
			return (EIO);
		KASSERT(pd->pn_parent, ("non-root directory has no parent"));
		return pfs_vncache_alloc(dvp->v_mount, vpp, pd->pn_parent);
	}

	/* process dependent */
	for (i = 0, pid = 0; i < cnp->cn_namelen && isdigit(pname[i]); ++i)
		pid = pid * 10 + pname[i] - '0';
	/* XXX assume that 8 digits is the maximum safe length for a pid */
	if (i == cnp->cn_namelen && i < 8) {
		/* see if this directory has process-dependent children */
		for (pn = pd->pn_nodes; pn->pn_type; ++pn)
			if (pn->pn_type == pfstype_procdep)
				break;
		if (pn->pn_type) {
			/* XXX pfind(0) should DTRT here */
			p = pid ? pfind(pid) : &proc0;
			if (p == NULL)
				return (ENOENT);
			if (p_can(cnp->cn_proc, p, P_CAN_SEE, NULL)) {
				/* pretend it doesn't exist */
				PROC_UNLOCK(p);
				return (ENOENT);
			}
#if 0
			if (!pn->pn_shadow)
				pfs_create_shadow(pn, p);
			pn = pn->pn_shadow;
			PROC_UNLOCK(p);
			goto got_pnode;
#else
			/* not yet implemented */
			PROC_UNLOCK(p);
			return (EIO);
#endif
		}
	}
	
	/* something else */
	for (pn = pd->pn_nodes; pn->pn_type; ++pn) {
		for (i = 0; i < cnp->cn_namelen && pn->pn_name[i]; ++i)
			if (pname[i] != pn->pn_name[i])
				break;
		if (i == cnp->cn_namelen)
			goto got_pnode;
	}

	return (ENOENT);
 got_pnode:
	if (!pn->pn_parent)
		pn->pn_parent = pd;
	error = pfs_vncache_alloc(dvp->v_mount, vpp, pn);
	if (error)
		return error;
 got_vnode:
	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(dvp, *vpp, cnp);
	return (0);
}

/*
 * Open a file or directory.
 */
static int
pfs_open(struct vop_open_args *va)
{
	/* XXX */
	return (0);
}

/*
 * Read from a file
 */
static int
pfs_read(struct vop_read_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_node *pn = vn->v_data;
	struct uio *uio = va->a_uio;
	struct sbuf *sb = NULL;
	char *ps;
	int error, xlen;

	if (vn->v_type != VREG)
		return (EINVAL);

	sb = sbuf_new(sb, NULL, uio->uio_offset + uio->uio_resid, 0);
	if (sb == NULL)
		return (EIO);

	error = (pn->pn_func)(pn, curproc, sb);
	
	/* XXX we should possibly detect and handle overflows */
	sbuf_finish(sb);
	ps = sbuf_data(sb) + uio->uio_offset;
	xlen = sbuf_len(sb) - uio->uio_offset;
	xlen = imin(xlen, uio->uio_resid);
	error = (xlen <= 0 ? 0 : uiomove(ps, xlen, uio));
	sbuf_delete(sb);
	return (error);
}

/*
 * Return directory entries.
 */
static int
pfs_readdir(struct vop_readdir_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_info *pi;
	struct pfs_node *pd, *pn;
	struct dirent entry;
	struct uio *uio;
#if 0
	struct proc *p;
#endif
	off_t offset;
	int error, i, resid;

	if (vn->v_type != VDIR)
		return (ENOTDIR);
	pi = (struct pfs_info *)vn->v_mount->mnt_data;
	pd = (struct pfs_node *)vn->v_data;
	pn = pd->pn_nodes;
	uio = va->a_uio;

	/* only allow reading entire entries */
	offset = uio->uio_offset;
	resid = uio->uio_resid;
	if (offset < 0 || offset % PFS_DELEN != 0 || resid < PFS_DELEN)
		return (EINVAL);

	/* skip unwanted entries */
	for (; pn->pn_type && offset > 0; ++pn, offset -= PFS_DELEN)
		/* nothing */ ;

	/* fill in entries */
	entry.d_reclen = PFS_DELEN;
	for (; pn->pn_type && resid > 0; ++pn) {
		if (!pn->pn_parent)
			pn->pn_parent = pd;
		if (!pn->pn_fileno)
			pfs_fileno_alloc(pi, pn);
		entry.d_fileno = pn->pn_fileno;
		/* PFS_DELEN was picked to fit PFS_NAMLEN */
		for (i = 0; i < PFS_NAMELEN - 1 && pn->pn_name[i] != '\0'; ++i)
			entry.d_name[i] = pn->pn_name[i];
		entry.d_name[i] = 0;
		entry.d_namlen = i;
		switch (pn->pn_type) {
		case pfstype_root:
		case pfstype_dir:
		case pfstype_this:
		case pfstype_parent:
			entry.d_type = DT_DIR;
			break;
		case pfstype_file:
			entry.d_type = DT_REG;
			break;
		case pfstype_symlink:
			entry.d_type = DT_LNK;
			break;
		case pfstype_procdep:
			/* don't handle process-dependent nodes here */
			continue;
		default:
			panic("%s has unexpected node type: %d", pn->pn_name, pn->pn_type);
		}
		if ((error = uiomove((caddr_t)&entry, PFS_DELEN, uio)))
			return (error);
		offset += PFS_DELEN;
		resid -= PFS_DELEN;
	}
#if 0
	for (pn = pd->pn_nodes; pn->pn_type && resid > 0; ++pn) {
		if (pn->pn_type != pfstype_procdep)
			continue;
			
		sx_slock(&allproc_lock);
		p = LIST_FIRST(&allproc);
		
		sx_sunlock(&allproc_lock);
		offset += PFS_DELEN;
		resid -= PFS_DELEN;
		break;
	}
#endif
	
	uio->uio_offset += offset;
	return (0);
}

/*
 * Read a symbolic link
 */
static int
pfs_readlink(struct vop_readlink_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_node *pn = vn->v_data;
	struct uio *uio = va->a_uio;
	char buf[MAXPATHLEN], *ps;
	struct sbuf sb;
	int error, xlen;

	if (vn->v_type != VLNK)
		return (EINVAL);

	/* sbuf_new() can't fail with a static buffer */
	sbuf_new(&sb, buf, sizeof buf, 0);

	error = (pn->pn_func)(pn, curproc, &sb);
	
	/* XXX we should detect and handle overflows */
	sbuf_finish(&sb);
	ps = sbuf_data(&sb) + uio->uio_offset;
	xlen = sbuf_len(&sb) - uio->uio_offset;
	xlen = imin(xlen, uio->uio_resid);
	error = (xlen <= 0 ? 0 : uiomove(ps, xlen, uio));
	sbuf_delete(&sb);
	return (error);
}

/*
 * Reclaim a vnode
 */
static int
pfs_reclaim(struct vop_reclaim_args *va)
{
	return (pfs_vncache_free(va->a_vp));
}

/*
 * Set attributes
 */
static int
pfs_setattr(struct vop_setattr_args *va)
{
	if (va->a_vap->va_flags != VNOVAL)
		return (EOPNOTSUPP);
	return (0);
}

/*
 * Dummy operations
 */
static int pfs_erofs(void *va)		{ return (EROFS); }
static int pfs_null(void *va)		{ return (0); }

/*
 * Vnode operations
 */
vop_t **pfs_vnodeop_p;
static struct vnodeopv_entry_desc pfs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *)vop_defaultop	},
	{ &vop_access_desc,		(vop_t *)pfs_access	},
	{ &vop_close_desc,		(vop_t *)pfs_close	},
	{ &vop_create_desc,		(vop_t *)pfs_erofs	},
	{ &vop_getattr_desc,		(vop_t *)pfs_getattr	},
	{ &vop_link_desc,		(vop_t *)pfs_erofs	},
	{ &vop_lookup_desc,		(vop_t *)pfs_lookup	},
	{ &vop_mkdir_desc,		(vop_t *)pfs_erofs	},
	{ &vop_open_desc,		(vop_t *)pfs_open	},
	{ &vop_read_desc,		(vop_t *)pfs_read	},
	{ &vop_readdir_desc,		(vop_t *)pfs_readdir	},
	{ &vop_readlink_desc,		(vop_t *)pfs_readlink	},
	{ &vop_reclaim_desc,		(vop_t *)pfs_reclaim	},
	{ &vop_remove_desc,		(vop_t *)pfs_erofs	},
	{ &vop_rename_desc,		(vop_t *)pfs_erofs	},
	{ &vop_rmdir_desc,		(vop_t *)pfs_erofs	},
	{ &vop_setattr_desc,		(vop_t *)pfs_setattr	},
	{ &vop_symlink_desc,		(vop_t *)pfs_erofs	},
	{ &vop_write_desc,		(vop_t *)pfs_erofs	},
	/* XXX I've probably forgotten a few that need pfs_erofs */
	{ NULL,				(vop_t *)NULL		}
};

static struct vnodeopv_desc pfs_vnodeop_opv_desc =
	{ &pfs_vnodeop_p, pfs_vnodeop_entries };

VNODEOP_SET(pfs_vnodeop_opv_desc);

