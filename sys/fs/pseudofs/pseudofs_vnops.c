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
#include <sys/fcntl.h>
#include <sys/lock.h>
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

#if 0
#define PFS_TRACE(foo) \
	do { \
		printf("pseudofs: %s(): ", __FUNCTION__); \
		printf foo ; \
		printf("\n"); \
	} while (0)
#define PFS_RETURN(err) \
	do { \
		printf("pseudofs: %s(): returning %d\n", __FUNCTION__, err); \
		return (err); \
	} while (0)
#else
#define PFS_TRACE(foo) \
	do { /* nothing */ } while (0)
#define PFS_RETURN(err) \
	return (err)
#endif

/*
 * Verify permissions
 */
static int
pfs_access(struct vop_access_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = (struct pfs_vdata *)vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	struct vattr vattr;
	int error;
	
	PFS_TRACE((pn->pn_name));
	
	error = VOP_GETATTR(vn, &vattr, va->a_cred, va->a_td);
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
	PFS_RETURN (0);
}

/*
 * Get file attributes
 */
static int
pfs_getattr(struct vop_getattr_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = (struct pfs_vdata *)vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	struct vattr *vap = va->a_vap;
	struct proc *proc;
	int error = 0;

	PFS_TRACE((pn->pn_name));
	
	VATTR_NULL(vap);
	vap->va_type = vn->v_type;
	vap->va_fileid = pn->pn_fileno;
	vap->va_flags = 0;
	vap->va_blocksize = PAGE_SIZE;
	vap->va_bytes = vap->va_size = 0;
	vap->va_fsid = vn->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_nlink = 1;
	nanotime(&vap->va_ctime);
	vap->va_atime = vap->va_mtime = vap->va_ctime;

	switch (pn->pn_type) {
	case pfstype_procdir:
		/* XXX needs p_cansee */
	case pfstype_root:
	case pfstype_dir:
		vap->va_mode = 0555;
		break;
	case pfstype_file:
	case pfstype_symlink:
		vap->va_mode = 0444;
		break;
	default:
		printf("shouldn't be here!\n");
		vap->va_mode = 0;
		break;
	}

	if (pvd->pvd_pid != NO_PID) {
		if ((proc = pfind(pvd->pvd_pid)) == NULL)
			PFS_RETURN (ENOENT);
		vap->va_uid = proc->p_ucred->cr_ruid;
		vap->va_gid = proc->p_ucred->cr_rgid;
		if (pn->pn_attr != NULL)
			error = (pn->pn_attr)(curthread, proc, pn, vap);
		PROC_UNLOCK(proc);
	} else {
		vap->va_uid = 0;
		vap->va_gid = 0;
	}
		
	PFS_RETURN (error);
}

/*
 * Look up a file or directory
 */
static int
pfs_lookup(struct vop_lookup_args *va)
{
	struct vnode *vn = va->a_dvp;
	struct vnode **vpp = va->a_vpp;
	struct componentname *cnp = va->a_cnp;
	struct pfs_vdata *pvd = (struct pfs_vdata *)vn->v_data;
	struct pfs_node *pd = pvd->pvd_pn;
	struct pfs_node *pn, *pdn = NULL;
	struct proc *proc;
	pid_t pid = pvd->pvd_pid;
	char *pname;
	int error, i, namelen;

	PFS_TRACE(("%.*s", (int)cnp->cn_namelen, cnp->cn_nameptr));
	
	if (vn->v_type != VDIR)
		PFS_RETURN (ENOTDIR);
	
	/*
	 * Don't support DELETE or RENAME.  CREATE is supported so
	 * that O_CREAT will work, but the lookup will still fail if
	 * the file does not exist.
	 */
	if (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)
		PFS_RETURN (EOPNOTSUPP);

	/* shortcut: check if the name is too long */
	if (cnp->cn_namelen >= PFS_NAMELEN)
		PFS_RETURN (ENOENT);

	/* check that owner process exists */
	if (pvd->pvd_pid != NO_PID) {
		if ((proc = pfind(pvd->pvd_pid)) == NULL)
		        PFS_RETURN (ENOENT);
		PROC_UNLOCK(proc);
	}
	
	/* self */
	namelen = cnp->cn_namelen;
	pname = cnp->cn_nameptr;
	if (namelen == 1 && *pname == '.') {
		pn = pd;
		*vpp = vn;
		VREF(vn);
		goto got_vnode;
	}

	/* parent */
	if (cnp->cn_flags & ISDOTDOT) {
		if (pd->pn_type == pfstype_root)
			PFS_RETURN (EIO);
		KASSERT(pd->pn_parent, ("non-root directory has no parent"));
		/*
		 * This one is tricky.  Descendents of procdir nodes
		 * inherit their parent's process affinity, but
		 * there's no easy reverse mapping.  For simplicity,
		 * we assume that if this node is a procdir, its
		 * parent isn't (which is correct as long as
		 * descendents of procdir nodes are never procdir
		 * nodes themselves)
		 */
		if (pd->pn_type == pfstype_procdir)
			pid = NO_PID;
		error = pfs_vncache_alloc(vn->v_mount, vpp, pd->pn_parent, pid);
		if (error)
			PFS_RETURN (error);
		goto got_vnode;
	}

	/* named node */
	for (pn = pd->pn_nodes; pn->pn_type; ++pn)
		if (pn->pn_type == pfstype_procdir)
			pdn = pn;
		else if (pn->pn_name[namelen] == '\0'
		    && bcmp(pname, pn->pn_name, namelen) == 0)
			goto got_pnode;

	/* process dependent node */
	if ((pn = pdn) != NULL) {
		pid = 0;
		for (pid = 0, i = 0; i < namelen && isdigit(pname[i]); ++i)
			if ((pid = pid * 10 + pname[i] - '0') > PID_MAX)
				break;
		if (i == cnp->cn_namelen)
			goto got_pnode;
	}
		
	PFS_RETURN (ENOENT);
 got_pnode:
	if (!pn->pn_parent)
		pn->pn_parent = pd;
	error = pfs_vncache_alloc(vn->v_mount, vpp, pn, pid);
	if (error)
		PFS_RETURN (error);
 got_vnode:
	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(vn, *vpp, cnp);
	PFS_RETURN (0);
}

/*
 * Open a file or directory.
 */
static int
pfs_open(struct vop_open_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = (struct pfs_vdata *)vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	int mode = va->a_mode;
	struct proc *proc;
	int error;

	PFS_TRACE(("%s (mode 0x%x)", pn->pn_name, mode));

	/* check if the requested mode is permitted */
	if (((mode & FREAD) && !(mode & PFS_RD)) ||
	    ((mode & FWRITE) && !(mode & PFS_WR)))
		PFS_RETURN (EPERM);

	/* we don't support locking */
	if ((mode & O_SHLOCK) || (mode & O_EXLOCK))
		PFS_RETURN (EOPNOTSUPP);
	
	error = 0;
	if (pvd->pvd_pid != NO_PID) {
		if ((proc = pfind(pvd->pvd_pid)) == NULL)
		        PFS_RETURN (ENOENT);
		/* XXX should lock va->a_td->td_proc? */
		if (p_cansee(va->a_td->td_proc, proc) != 0)
			error = ENOENT;
		PROC_UNLOCK(proc);
	}

	PFS_RETURN (error);
}

/*
 * Read from a file
 */
static int
pfs_read(struct vop_read_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = (struct pfs_vdata *)vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	struct uio *uio = va->a_uio;
	struct proc *proc = NULL;
	struct sbuf *sb = NULL;
	char *ps;
	int error, xlen;

	PFS_TRACE((pn->pn_name));
	
	if (vn->v_type != VREG)
		PFS_RETURN (EINVAL);

	if (!(pn->pn_flags & PFS_RD))
		PFS_RETURN (EBADF);

	if (pvd->pvd_pid != NO_PID) {
		if ((proc = pfind(pvd->pvd_pid)) == NULL)
			PFS_RETURN (EIO);
		_PHOLD(proc);
		PROC_UNLOCK(proc);
	}

	if (pn->pn_flags & PFS_RAWRD) {
		error = (pn->pn_func)(curthread, proc, pn, NULL, uio);
		if (proc != NULL)
			PRELE(proc);
		PFS_RETURN (error);
	}
	
	sb = sbuf_new(sb, NULL, uio->uio_offset + uio->uio_resid, 0);
	if (sb == NULL) {
		if (proc != NULL)
			PRELE(proc);
		PFS_RETURN (EIO);
	}

	error = (pn->pn_func)(curthread, proc, pn, sb, uio);

	if (proc != NULL)
		PRELE(proc);

	if (error) {
		sbuf_delete(sb);
		PFS_RETURN (error);
	}
	
	/* XXX we should possibly detect and handle overflows */
	sbuf_finish(sb);
	ps = sbuf_data(sb) + uio->uio_offset;
	xlen = sbuf_len(sb) - uio->uio_offset;
	xlen = imin(xlen, uio->uio_resid);
	error = (xlen <= 0 ? 0 : uiomove(ps, xlen, uio));
	sbuf_delete(sb);
	PFS_RETURN (error);
}

/*
 * Iterate through directory entries
 */
static int
pfs_iterate(struct pfs_info *pi, struct pfs_node **pn, struct proc **p)
{
	if ((*pn)->pn_type == pfstype_none)
		return (-1);

	if ((*pn)->pn_type != pfstype_procdir)
		++*pn;
	
	while ((*pn)->pn_type == pfstype_procdir) {
		if (*p == NULL)
			*p = LIST_FIRST(&allproc);
		else
			*p = LIST_NEXT(*p, p_list);
		if (*p != NULL)
			return (0);
		++*pn;
	}
	
	if ((*pn)->pn_type == pfstype_none)
		return (-1);
	
	return (0);
}

/*
 * Return directory entries.
 */
static int
pfs_readdir(struct vop_readdir_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_info *pi = (struct pfs_info *)vn->v_mount->mnt_data;
	struct pfs_vdata *pvd = (struct pfs_vdata *)vn->v_data;
	struct pfs_node *pd = pvd->pvd_pn;
	struct pfs_node *pn;
	struct dirent entry;
	struct uio *uio;
	struct proc *p;
	off_t offset;
	int error, i, resid;

	PFS_TRACE((pd->pn_name));
	
	if (vn->v_type != VDIR)
		PFS_RETURN (ENOTDIR);
	uio = va->a_uio;

	/* only allow reading entire entries */
	offset = uio->uio_offset;
	resid = uio->uio_resid;
	if (offset < 0 || offset % PFS_DELEN != 0 || resid < PFS_DELEN)
		PFS_RETURN (EINVAL);

	/* skip unwanted entries */
	sx_slock(&allproc_lock);
	for (pn = pd->pn_nodes, p = NULL; offset > 0; offset -= PFS_DELEN)
		if (pfs_iterate(pi, &pn, &p) == -1)
			break;
	
	/* fill in entries */
	entry.d_reclen = PFS_DELEN;
	while (pfs_iterate(pi, &pn, &p) != -1 && resid > 0) {
		if (!pn->pn_parent)
			pn->pn_parent = pd;
		if (!pn->pn_fileno)
			pfs_fileno_alloc(pi, pn);
		if (pvd->pvd_pid != NO_PID)
			entry.d_fileno = pn->pn_fileno * NO_PID + pvd->pvd_pid;
		else
			entry.d_fileno = pn->pn_fileno;
		/* PFS_DELEN was picked to fit PFS_NAMLEN */
		for (i = 0; i < PFS_NAMELEN - 1 && pn->pn_name[i] != '\0'; ++i)
			entry.d_name[i] = pn->pn_name[i];
		entry.d_name[i] = 0;
		entry.d_namlen = i;
		switch (pn->pn_type) {
		case pfstype_procdir:
			KASSERT(p != NULL,
			    ("reached procdir node with p == NULL"));
			entry.d_fileno = pn->pn_fileno * NO_PID + p->p_pid;
			entry.d_namlen = snprintf(entry.d_name,
			    PFS_NAMELEN, "%d", p->p_pid);
			/* fall through */
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
		default:
			sx_sunlock(&allproc_lock);
			panic("%s has unexpected node type: %d", pn->pn_name, pn->pn_type);
		}
		if ((error = uiomove((caddr_t)&entry, PFS_DELEN, uio))) {
			sx_sunlock(&allproc_lock);
			PFS_RETURN (error);
		}
		offset += PFS_DELEN;
		resid -= PFS_DELEN;
	}

	sx_sunlock(&allproc_lock);
	uio->uio_offset += offset;
	PFS_RETURN (0);
}

/*
 * Read a symbolic link
 */
static int
pfs_readlink(struct vop_readlink_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = (struct pfs_vdata *)vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	struct uio *uio = va->a_uio;
	struct proc *proc = NULL;
	char buf[MAXPATHLEN], *ps;
	struct sbuf sb;
	int error, xlen;

	PFS_TRACE((pn->pn_name));
	
	if (vn->v_type != VLNK)
		PFS_RETURN (EINVAL);

	if (pvd->pvd_pid != NO_PID) {
		if ((proc = pfind(pvd->pvd_pid)) == NULL)
			PFS_RETURN (EIO);
		_PHOLD(proc);
		PROC_UNLOCK(proc);
	}
	
	/* sbuf_new() can't fail with a static buffer */
	sbuf_new(&sb, buf, sizeof buf, 0);

	error = (pn->pn_func)(curthread, proc, pn, &sb, NULL);

	if (proc != NULL)
		PRELE(proc);
	
	if (error) {
		sbuf_delete(&sb);
		PFS_RETURN (error);
	}
	
	/* XXX we should detect and handle overflows */
	sbuf_finish(&sb);
	ps = sbuf_data(&sb) + uio->uio_offset;
	xlen = sbuf_len(&sb) - uio->uio_offset;
	xlen = imin(xlen, uio->uio_resid);
	error = (xlen <= 0 ? 0 : uiomove(ps, xlen, uio));
	sbuf_delete(&sb);
	PFS_RETURN (error);
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
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = (struct pfs_vdata *)vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;

	PFS_TRACE((pn->pn_name));
	
	if (va->a_vap->va_flags != (u_long)VNOVAL)
		PFS_RETURN (EOPNOTSUPP);
	/* XXX it's a bit more complex than that, really... */
	PFS_RETURN (0);
}

/*
 * Read from a file
 */
static int
pfs_write(struct vop_read_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = (struct pfs_vdata *)vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	struct uio *uio = va->a_uio;
	struct proc *proc = NULL;
	struct sbuf sb;
	int error;

	PFS_TRACE((pn->pn_name));
	
	if (vn->v_type != VREG)
		PFS_RETURN (EINVAL);

	if (!(pn->pn_flags & PFS_WR))
		PFS_RETURN (EBADF);

	if (pvd->pvd_pid != NO_PID) {
		if ((proc = pfind(pvd->pvd_pid)) == NULL)
			PFS_RETURN (EIO);
		_PHOLD(proc);
		PROC_UNLOCK(proc);
	}

	if (pn->pn_flags & PFS_RAWWR) {
		error = (pn->pn_func)(curthread, proc, pn, NULL, uio);
		if (proc != NULL)
			PRELE(proc);
		PFS_RETURN (error);
	}

	sbuf_uionew(&sb, uio, &error);
	if (error)
		PFS_RETURN (error);
	
	error = (pn->pn_func)(curthread, proc, pn, &sb, uio);

	if (proc != NULL)
		PRELE(proc);
	
	sbuf_delete(&sb);
	PFS_RETURN (error);
}

/*
 * Dummy operations */
static int pfs_badop(void *va)		{ return (EOPNOTSUPP); }
#if 0
static int pfs_erofs(void *va)		{ return (EROFS); }
static int pfs_null(void *va)		{ return (0); }
#endif

/*
 * Vnode operations
 */
vop_t **pfs_vnodeop_p;
static struct vnodeopv_entry_desc pfs_vnodeop_entries[] = {
	{ &vop_default_desc,		(vop_t *)vop_defaultop	},
	{ &vop_access_desc,		(vop_t *)pfs_access	},
	{ &vop_close_desc,		(vop_t *)pfs_close	},
	{ &vop_create_desc,		(vop_t *)pfs_badop	},
	{ &vop_getattr_desc,		(vop_t *)pfs_getattr	},
	{ &vop_link_desc,		(vop_t *)pfs_badop	},
	{ &vop_lookup_desc,		(vop_t *)pfs_lookup	},
	{ &vop_mkdir_desc,		(vop_t *)pfs_badop	},
	{ &vop_mknod_desc,		(vop_t *)pfs_badop	},
	{ &vop_open_desc,		(vop_t *)pfs_open	},
	{ &vop_read_desc,		(vop_t *)pfs_read	},
	{ &vop_readdir_desc,		(vop_t *)pfs_readdir	},
	{ &vop_readlink_desc,		(vop_t *)pfs_readlink	},
	{ &vop_reclaim_desc,		(vop_t *)pfs_reclaim	},
	{ &vop_remove_desc,		(vop_t *)pfs_badop	},
	{ &vop_rename_desc,		(vop_t *)pfs_badop	},
	{ &vop_rmdir_desc,		(vop_t *)pfs_badop	},
	{ &vop_setattr_desc,		(vop_t *)pfs_setattr	},
	{ &vop_symlink_desc,		(vop_t *)pfs_badop	},
	{ &vop_write_desc,		(vop_t *)pfs_write	},
	/* XXX I've probably forgotten a few that need pfs_erofs */
	{ NULL,				(vop_t *)NULL		}
};

static struct vnodeopv_desc pfs_vnodeop_opv_desc =
	{ &pfs_vnodeop_p, pfs_vnodeop_entries };

VNODEOP_SET(pfs_vnodeop_opv_desc);
