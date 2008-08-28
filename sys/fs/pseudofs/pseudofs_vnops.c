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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_pseudofs.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/ctype.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
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
 * Returns the fileno, adjusted for target pid
 */
static uint32_t
pn_fileno(struct pfs_node *pn, pid_t pid)
{

	KASSERT(pn->pn_fileno > 0,
	    ("%s(): no fileno allocated", __func__));
	if (pid != NO_PID)
		return (pn->pn_fileno * NO_PID + pid);
	return (pn->pn_fileno);
}

/*
 * Returns non-zero if given file is visible to given thread.
 */
static int
pfs_visible_proc(struct thread *td, struct pfs_node *pn, struct proc *proc)
{
	int visible;

	if (proc == NULL)
		return (0);

	PROC_LOCK_ASSERT(proc, MA_OWNED);

	visible = ((proc->p_flag & P_WEXIT) == 0);
	if (visible)
		visible = (p_cansee(td, proc) == 0);
	if (visible && pn->pn_vis != NULL)
		visible = pn_vis(td, proc, pn);
	if (!visible)
		return (0);
	return (1);
}

static int
pfs_visible(struct thread *td, struct pfs_node *pn, pid_t pid, struct proc **p)
{
	struct proc *proc;

	PFS_TRACE(("%s (pid: %d, req: %d)",
	    pn->pn_name, pid, td->td_proc->p_pid));

	if (p)
		*p = NULL;
	if (pid == NO_PID)
		PFS_RETURN (1);
	if ((proc = pfind(pid)) == NULL)
		PFS_RETURN (0);
	if (pfs_visible_proc(td, pn, proc)) {
		if (p)
			*p = proc;
		else
			PROC_UNLOCK(proc);
		PFS_RETURN (1);
	}
	PROC_UNLOCK(proc);
	PFS_RETURN (0);
}

/*
 * Verify permissions
 */
static int
pfs_access(struct vop_access_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = vn->v_data;
	struct vattr vattr;
	int error;

	PFS_TRACE(("%s", pvd->pvd_pn->pn_name));
	(void)pvd;

	error = VOP_GETATTR(vn, &vattr, va->a_cred);
	if (error)
		PFS_RETURN (error);
	error = vaccess(vn->v_type, vattr.va_mode, vattr.va_uid,
	    vattr.va_gid, va->a_mode, va->a_cred, NULL);
	PFS_RETURN (error);
}

/*
 * Close a file or directory
 */
static int
pfs_close(struct vop_close_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	struct proc *proc;
	int error;

	PFS_TRACE(("%s", pn->pn_name));
	pfs_assert_not_owned(pn);

	/*
	 * Do nothing unless this is the last close and the node has a
	 * last-close handler.
	 */
	if (vrefcnt(vn) > 1 || pn->pn_close == NULL)
		PFS_RETURN (0);

	if (pvd->pvd_pid != NO_PID) {
		proc = pfind(pvd->pvd_pid);
	} else {
		proc = NULL;
	}

	error = pn_close(va->a_td, proc, pn);

	if (proc != NULL)
		PROC_UNLOCK(proc);

	PFS_RETURN (error);
}

/*
 * Get file attributes
 */
static int
pfs_getattr(struct vop_getattr_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	struct vattr *vap = va->a_vap;
	struct proc *proc;
	int error = 0;

	PFS_TRACE(("%s", pn->pn_name));
	pfs_assert_not_owned(pn);

	if (!pfs_visible(curthread, pn, pvd->pvd_pid, &proc))
		PFS_RETURN (ENOENT);

	VATTR_NULL(vap);
	vap->va_type = vn->v_type;
	vap->va_fileid = pn_fileno(pn, pvd->pvd_pid);
	vap->va_flags = 0;
	vap->va_blocksize = PAGE_SIZE;
	vap->va_bytes = vap->va_size = 0;
	vap->va_fsid = vn->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_nlink = 1;
	nanotime(&vap->va_ctime);
	vap->va_atime = vap->va_mtime = vap->va_ctime;

	switch (pn->pn_type) {
	case pfstype_procdir:
	case pfstype_root:
	case pfstype_dir:
#if 0
		pfs_lock(pn);
		/* compute link count */
		pfs_unlock(pn);
#endif
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

	if (proc != NULL) {
		vap->va_uid = proc->p_ucred->cr_ruid;
		vap->va_gid = proc->p_ucred->cr_rgid;
		if (pn->pn_attr != NULL)
			error = pn_attr(curthread, proc, pn, vap);
		PROC_UNLOCK(proc);
	} else {
		vap->va_uid = 0;
		vap->va_gid = 0;
	}

	PFS_RETURN (error);
}

/*
 * Perform an ioctl
 */
static int
pfs_ioctl(struct vop_ioctl_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	struct proc *proc;
	int error;

	PFS_TRACE(("%s: %lx", pn->pn_name, va->a_command));
	pfs_assert_not_owned(pn);

	if (vn->v_type != VREG)
		PFS_RETURN (EINVAL);

	if (pn->pn_ioctl == NULL)
		PFS_RETURN (ENOTTY);

	/*
	 * This is necessary because process' privileges may
	 * have changed since the open() call.
	 */
	if (!pfs_visible(curthread, pn, pvd->pvd_pid, &proc))
		PFS_RETURN (EIO);

	error = pn_ioctl(curthread, proc, pn, va->a_command, va->a_data);

	if (proc != NULL)
		PROC_UNLOCK(proc);

	PFS_RETURN (error);
}

/*
 * Perform getextattr
 */
static int
pfs_getextattr(struct vop_getextattr_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	struct proc *proc;
	int error;

	PFS_TRACE(("%s", pn->pn_name));
	pfs_assert_not_owned(pn);

	/*
	 * This is necessary because either process' privileges may
	 * have changed since the open() call.
	 */
	if (!pfs_visible(curthread, pn, pvd->pvd_pid, &proc))
		PFS_RETURN (EIO);

	if (pn->pn_getextattr == NULL)
		error = EOPNOTSUPP;
	else
		error = pn_getextattr(curthread, proc, pn,
		    va->a_attrnamespace, va->a_name, va->a_uio,
		    va->a_size, va->a_cred);

	if (proc != NULL)
		PROC_UNLOCK(proc);

	pfs_unlock(pn);
	PFS_RETURN (error);
}

/*
 * Look up a file or directory
 */
static int
pfs_lookup(struct vop_cachedlookup_args *va)
{
	struct vnode *vn = va->a_dvp;
	struct vnode **vpp = va->a_vpp;
	struct componentname *cnp = va->a_cnp;
	struct pfs_vdata *pvd = vn->v_data;
	struct pfs_node *pd = pvd->pvd_pn;
	struct pfs_node *pn, *pdn = NULL;
	pid_t pid = pvd->pvd_pid;
	char *pname;
	int error, i, namelen, visible;

	PFS_TRACE(("%.*s", (int)cnp->cn_namelen, cnp->cn_nameptr));
	pfs_assert_not_owned(pd);

	if (vn->v_type != VDIR)
		PFS_RETURN (ENOTDIR);

	error = VOP_ACCESS(vn, VEXEC, cnp->cn_cred, cnp->cn_thread);
	if (error)
		PFS_RETURN (error);

	/*
	 * Don't support DELETE or RENAME.  CREATE is supported so
	 * that O_CREAT will work, but the lookup will still fail if
	 * the file does not exist.
	 */
	if ((cnp->cn_flags & ISLASTCN) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		PFS_RETURN (EOPNOTSUPP);

	/* shortcut: check if the name is too long */
	if (cnp->cn_namelen >= PFS_NAMELEN)
		PFS_RETURN (ENOENT);

	/* check that parent directory is visible... */
	if (!pfs_visible(curthread, pd, pvd->pvd_pid, NULL))
		PFS_RETURN (ENOENT);

	/* self */
	namelen = cnp->cn_namelen;
	pname = cnp->cn_nameptr;
	if (namelen == 1 && pname[0] == '.') {
		pn = pd;
		*vpp = vn;
		VREF(vn);
		PFS_RETURN (0);
	}

	/* parent */
	if (cnp->cn_flags & ISDOTDOT) {
		if (pd->pn_type == pfstype_root)
			PFS_RETURN (EIO);
		VOP_UNLOCK(vn, 0);
		KASSERT(pd->pn_parent != NULL,
		    ("%s(): non-root directory has no parent", __func__));
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
		pfs_lock(pd);
		pn = pd->pn_parent;
		pfs_unlock(pd);
		goto got_pnode;
	}

	pfs_lock(pd);

	/* named node */
	for (pn = pd->pn_nodes; pn != NULL; pn = pn->pn_next)
		if (pn->pn_type == pfstype_procdir)
			pdn = pn;
		else if (pn->pn_name[namelen] == '\0' &&
		    bcmp(pname, pn->pn_name, namelen) == 0) {
			pfs_unlock(pd);
			goto got_pnode;
		}

	/* process dependent node */
	if ((pn = pdn) != NULL) {
		pid = 0;
		for (pid = 0, i = 0; i < namelen && isdigit(pname[i]); ++i)
			if ((pid = pid * 10 + pname[i] - '0') > PID_MAX)
				break;
		if (i == cnp->cn_namelen) {
			pfs_unlock(pd);
			goto got_pnode;
		}
	}

	pfs_unlock(pd);

	PFS_RETURN (ENOENT);

 got_pnode:
	pfs_assert_not_owned(pd);
	pfs_assert_not_owned(pn);
	visible = pfs_visible(curthread, pn, pid, NULL);
	if (!visible) {
		error = ENOENT;
		goto failed;
	}

	error = pfs_vncache_alloc(vn->v_mount, vpp, pn, pid);
	if (error)
		goto failed;

	if (cnp->cn_flags & ISDOTDOT)
		vn_lock(vn, LK_EXCLUSIVE|LK_RETRY);
	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(vn, *vpp, cnp);
	PFS_RETURN (0);
 failed:
	if (cnp->cn_flags & ISDOTDOT)
		vn_lock(vn, LK_EXCLUSIVE|LK_RETRY);
	PFS_RETURN(error);
}

/*
 * Open a file or directory.
 */
static int
pfs_open(struct vop_open_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	int mode = va->a_mode;

	PFS_TRACE(("%s (mode 0x%x)", pn->pn_name, mode));
	pfs_assert_not_owned(pn);

	/* check if the requested mode is permitted */
	if (((mode & FREAD) && !(mode & PFS_RD)) ||
	    ((mode & FWRITE) && !(mode & PFS_WR)))
		PFS_RETURN (EPERM);

	/* we don't support locking */
	if ((mode & O_SHLOCK) || (mode & O_EXLOCK))
		PFS_RETURN (EOPNOTSUPP);

	PFS_RETURN (0);
}

/*
 * Read from a file
 */
static int
pfs_read(struct vop_read_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	struct uio *uio = va->a_uio;
	struct proc *proc;
	struct sbuf *sb = NULL;
	int error;
	unsigned int buflen, offset, resid;

	PFS_TRACE(("%s", pn->pn_name));
	pfs_assert_not_owned(pn);

	if (vn->v_type != VREG)
		PFS_RETURN (EINVAL);

	if (!(pn->pn_flags & PFS_RD))
		PFS_RETURN (EBADF);

	if (pn->pn_fill == NULL)
		PFS_RETURN (EIO);

	/*
	 * This is necessary because either process' privileges may
	 * have changed since the open() call.
	 */
	if (!pfs_visible(curthread, pn, pvd->pvd_pid, &proc))
		PFS_RETURN (EIO);
	if (proc != NULL) {
		_PHOLD(proc);
		PROC_UNLOCK(proc);
	}

	if (pn->pn_flags & PFS_RAWRD) {
		PFS_TRACE(("%lu resid", (unsigned long)uio->uio_resid));
		error = pn_fill(curthread, proc, pn, NULL, uio);
		PFS_TRACE(("%lu resid", (unsigned long)uio->uio_resid));
		if (proc != NULL)
			PRELE(proc);
		PFS_RETURN (error);
	}

	/* beaucoup sanity checks so we don't ask for bogus allocation */
	if (uio->uio_offset < 0 || uio->uio_resid < 0 ||
	    (offset = uio->uio_offset) != uio->uio_offset ||
	    (resid = uio->uio_resid) != uio->uio_resid ||
	    (buflen = offset + resid + 1) < offset || buflen > INT_MAX) {
		if (proc != NULL)
			PRELE(proc);
		PFS_RETURN (EINVAL);
	}
	if (buflen > MAXPHYS + 1) {
		if (proc != NULL)
			PRELE(proc);
		PFS_RETURN (EIO);
	}

	sb = sbuf_new(sb, NULL, buflen, 0);
	if (sb == NULL) {
		if (proc != NULL)
			PRELE(proc);
		PFS_RETURN (EIO);
	}

	error = pn_fill(curthread, proc, pn, sb, uio);

	if (proc != NULL)
		PRELE(proc);

	if (error) {
		sbuf_delete(sb);
		PFS_RETURN (error);
	}

	sbuf_finish(sb);
	error = uiomove_frombuf(sbuf_data(sb), sbuf_len(sb), uio);
	sbuf_delete(sb);
	PFS_RETURN (error);
}

/*
 * Iterate through directory entries
 */
static int
pfs_iterate(struct thread *td, struct proc *proc, struct pfs_node *pd,
	    struct pfs_node **pn, struct proc **p)
{
	int visible;

	sx_assert(&allproc_lock, SX_SLOCKED);
	pfs_assert_owned(pd);
 again:
	if (*pn == NULL) {
		/* first node */
		*pn = pd->pn_nodes;
	} else if ((*pn)->pn_type != pfstype_procdir) {
		/* next node */
		*pn = (*pn)->pn_next;
	}
	if (*pn != NULL && (*pn)->pn_type == pfstype_procdir) {
		/* next process */
		if (*p == NULL)
			*p = LIST_FIRST(&allproc);
		else
			*p = LIST_NEXT(*p, p_list);
		/* out of processes: next node */
		if (*p == NULL)
			*pn = (*pn)->pn_next;
		else
			PROC_LOCK(*p);
	}

	if ((*pn) == NULL)
		return (-1);

	if (*p != NULL) {
		visible = pfs_visible_proc(td, *pn, *p);
		PROC_UNLOCK(*p);
	} else if (proc != NULL) {
		visible = pfs_visible_proc(td, *pn, proc);
	} else {
		visible = 1;
	}
	if (!visible)
		goto again;

	return (0);
}

/*
 * Return directory entries.
 */
static int
pfs_readdir(struct vop_readdir_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = vn->v_data;
	struct pfs_node *pd = pvd->pvd_pn;
	pid_t pid = pvd->pvd_pid;
	struct proc *p, *proc;
	struct pfs_node *pn;
	struct dirent *entry;
	struct uio *uio;
	off_t offset;
	int error, i, resid;
	char *buf, *ent;

	KASSERT(pd->pn_info == vn->v_mount->mnt_data,
	    ("%s(): pn_info does not match mountpoint", __func__));
	PFS_TRACE(("%s pid %lu", pd->pn_name, (unsigned long)pid));
	pfs_assert_not_owned(pd);

	if (vn->v_type != VDIR)
		PFS_RETURN (ENOTDIR);
	uio = va->a_uio;

	/* only allow reading entire entries */
	offset = uio->uio_offset;
	resid = uio->uio_resid;
	if (offset < 0 || offset % PFS_DELEN != 0 ||
	    (resid && resid < PFS_DELEN))
		PFS_RETURN (EINVAL);
	if (resid == 0)
		PFS_RETURN (0);

	/* can't do this while holding the proc lock... */
	buf = malloc(resid, M_IOV, M_WAITOK | M_ZERO);
	sx_slock(&allproc_lock);
	pfs_lock(pd);

        /* check if the directory is visible to the caller */
        if (!pfs_visible(curthread, pd, pid, &proc)) {
		sx_sunlock(&allproc_lock);
		pfs_unlock(pd);
		free(buf, M_IOV);
                PFS_RETURN (ENOENT);
	}
	KASSERT(pid == NO_PID || proc != NULL,
	    ("%s(): no process for pid %lu", __func__, (unsigned long)pid));

	/* skip unwanted entries */
	for (pn = NULL, p = NULL; offset > 0; offset -= PFS_DELEN) {
		if (pfs_iterate(curthread, proc, pd, &pn, &p) == -1) {
			/* nothing left... */
			if (proc != NULL)
				PROC_UNLOCK(proc);
			pfs_unlock(pd);
			sx_sunlock(&allproc_lock);
			free(buf, M_IOV);
			PFS_RETURN (0);
		}
	}

	/* fill in entries */
	ent = buf;
	while (pfs_iterate(curthread, proc, pd, &pn, &p) != -1 &&
	    resid >= PFS_DELEN) {
		entry = (struct dirent *)ent;
		entry->d_reclen = PFS_DELEN;
		entry->d_fileno = pn_fileno(pn, pid);
		/* PFS_DELEN was picked to fit PFS_NAMLEN */
		for (i = 0; i < PFS_NAMELEN - 1 && pn->pn_name[i] != '\0'; ++i)
			entry->d_name[i] = pn->pn_name[i];
		entry->d_name[i] = 0;
		entry->d_namlen = i;
		switch (pn->pn_type) {
		case pfstype_procdir:
			KASSERT(p != NULL,
			    ("reached procdir node with p == NULL"));
			entry->d_namlen = snprintf(entry->d_name,
			    PFS_NAMELEN, "%d", p->p_pid);
			/* fall through */
		case pfstype_root:
		case pfstype_dir:
		case pfstype_this:
		case pfstype_parent:
			entry->d_type = DT_DIR;
			break;
		case pfstype_file:
			entry->d_type = DT_REG;
			break;
		case pfstype_symlink:
			entry->d_type = DT_LNK;
			break;
		default:
			panic("%s has unexpected node type: %d", pn->pn_name, pn->pn_type);
		}
		PFS_TRACE(("%s", entry->d_name));
		offset += PFS_DELEN;
		resid -= PFS_DELEN;
		ent += PFS_DELEN;
	}
	if (proc != NULL)
		PROC_UNLOCK(proc);
	pfs_unlock(pd);
	sx_sunlock(&allproc_lock);
	PFS_TRACE(("%zd bytes", ent - buf));
	error = uiomove(buf, ent - buf, uio);
	free(buf, M_IOV);
	PFS_RETURN (error);
}

/*
 * Read a symbolic link
 */
static int
pfs_readlink(struct vop_readlink_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	struct uio *uio = va->a_uio;
	struct proc *proc = NULL;
	char buf[PATH_MAX];
	struct sbuf sb;
	int error;

	PFS_TRACE(("%s", pn->pn_name));
	pfs_assert_not_owned(pn);

	if (vn->v_type != VLNK)
		PFS_RETURN (EINVAL);

	if (pn->pn_fill == NULL)
		PFS_RETURN (EIO);

	if (pvd->pvd_pid != NO_PID) {
		if ((proc = pfind(pvd->pvd_pid)) == NULL)
			PFS_RETURN (EIO);
		if (proc->p_flag & P_WEXIT) {
			PROC_UNLOCK(proc);
			PFS_RETURN (EIO);
		}
		_PHOLD(proc);
		PROC_UNLOCK(proc);
	}

	/* sbuf_new() can't fail with a static buffer */
	sbuf_new(&sb, buf, sizeof buf, 0);

	error = pn_fill(curthread, proc, pn, &sb, NULL);

	if (proc != NULL)
		PRELE(proc);

	if (error) {
		sbuf_delete(&sb);
		PFS_RETURN (error);
	}

	sbuf_finish(&sb);
	error = uiomove_frombuf(sbuf_data(&sb), sbuf_len(&sb), uio);
	sbuf_delete(&sb);
	PFS_RETURN (error);
}

/*
 * Reclaim a vnode
 */
static int
pfs_reclaim(struct vop_reclaim_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;

	PFS_TRACE(("%s", pn->pn_name));
	pfs_assert_not_owned(pn);

	return (pfs_vncache_free(va->a_vp));
}

/*
 * Set attributes
 */
static int
pfs_setattr(struct vop_setattr_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;

	PFS_TRACE(("%s", pn->pn_name));
	pfs_assert_not_owned(pn);

	PFS_RETURN (EOPNOTSUPP);
}

/*
 * Write to a file
 */
static int
pfs_write(struct vop_write_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	struct uio *uio = va->a_uio;
	struct proc *proc;
	struct sbuf sb;
	int error;

	PFS_TRACE(("%s", pn->pn_name));
	pfs_assert_not_owned(pn);

	if (vn->v_type != VREG)
		PFS_RETURN (EINVAL);
	KASSERT(pn->pn_type != pfstype_file,
	    ("%s(): VREG vnode refers to non-file pfs_node", __func__));

	if (!(pn->pn_flags & PFS_WR))
		PFS_RETURN (EBADF);

	if (pn->pn_fill == NULL)
		PFS_RETURN (EIO);

	/*
	 * This is necessary because either process' privileges may
	 * have changed since the open() call.
	 */
	if (!pfs_visible(curthread, pn, pvd->pvd_pid, &proc))
		PFS_RETURN (EIO);
	if (proc != NULL) {
		_PHOLD(proc);
		PROC_UNLOCK(proc);
	}

	if (pn->pn_flags & PFS_RAWWR) {
		pfs_lock(pn);
		error = pn_fill(curthread, proc, pn, NULL, uio);
		pfs_unlock(pn);
		if (proc != NULL)
			PRELE(proc);
		PFS_RETURN (error);
	}

	sbuf_uionew(&sb, uio, &error);
	if (error) {
		if (proc != NULL)
			PRELE(proc);
		PFS_RETURN (error);
	}

	error = pn_fill(curthread, proc, pn, &sb, uio);

	sbuf_delete(&sb);
	if (proc != NULL)
		PRELE(proc);
	PFS_RETURN (error);
}

/*
 * Vnode operations
 */
struct vop_vector pfs_vnodeops = {
	.vop_default =		&default_vnodeops,

	.vop_access =		pfs_access,
	.vop_cachedlookup =	pfs_lookup,
	.vop_close =		pfs_close,
	.vop_create =		VOP_EOPNOTSUPP,
	.vop_getattr =		pfs_getattr,
	.vop_getextattr =	pfs_getextattr,
	.vop_ioctl =		pfs_ioctl,
	.vop_link =		VOP_EOPNOTSUPP,
	.vop_lookup =		vfs_cache_lookup,
	.vop_mkdir =		VOP_EOPNOTSUPP,
	.vop_mknod =		VOP_EOPNOTSUPP,
	.vop_open =		pfs_open,
	.vop_read =		pfs_read,
	.vop_readdir =		pfs_readdir,
	.vop_readlink =		pfs_readlink,
	.vop_reclaim =		pfs_reclaim,
	.vop_remove =		VOP_EOPNOTSUPP,
	.vop_rename =		VOP_EOPNOTSUPP,
	.vop_rmdir =		VOP_EOPNOTSUPP,
	.vop_setattr =		pfs_setattr,
	.vop_symlink =		VOP_EOPNOTSUPP,
	.vop_write =		pfs_write,
	/* XXX I've probably forgotten a few that need VOP_EOPNOTSUPP */
};
