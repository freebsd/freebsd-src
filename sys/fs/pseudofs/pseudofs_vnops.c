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

#ifdef PSEUDOFS_TRACE
static int pfs_trace;
SYSCTL_INT(_vfs_pfs, OID_AUTO, trace, CTLFLAG_RW, &pfs_trace, 0,
    "enable tracing of pseudofs vnode operations");

#define PFS_TRACE(foo) \
	do { \
		if (pfs_trace) { \
			printf("%s(): line %d: ", __func__, __LINE__); \
			printf foo ; \
			printf("\n"); \
		} \
	} while (0)
#define PFS_RETURN(err) \
	do { \
		if (pfs_trace) { \
			printf("%s(): line %d: returning %d\n", \
			    __func__, __LINE__, err); \
		} \
		return (err); \
	} while (0)
#else
#define PFS_TRACE(foo) \
	do { /* nothing */ } while (0)
#define PFS_RETURN(err) \
	return (err)
#endif

/*
 * Returns non-zero if given file is visible to given process
 */
static int
pfs_visible(struct thread *td, struct pfs_node *pn, pid_t pid)
{
	struct proc *proc;
	int r;

	PFS_TRACE(("%s (pid: %d, req: %d)",
	    pn->pn_name, pid, td->td_proc->p_pid));

	if (pn->pn_flags & PFS_DISABLED)
		PFS_RETURN (0);

	r = 1;
	if (pid != NO_PID) {
		if ((proc = pfind(pid)) == NULL)
			PFS_RETURN (0);
		if (p_cansee(td, proc) != 0 ||
		    (pn->pn_vis != NULL && !(pn->pn_vis)(td, proc, pn)))
			r = 0;
		PROC_UNLOCK(proc);
	}
	PFS_RETURN (r);
}

/*
 * Verify permissions
 */
static int
pfs_access(struct vop_access_args *va)
{
	struct vnode *vn = va->a_vp;
	struct vattr vattr;
	int error;

	PFS_TRACE((((struct pfs_vdata *)vn->v_data)->pvd_pn->pn_name));

	error = VOP_GETATTR(vn, &vattr, va->a_cred, va->a_td);
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
	struct pfs_vdata *pvd = (struct pfs_vdata *)vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	struct proc *proc;
	int error;

	PFS_TRACE((pn->pn_name));

	/*
	 * Do nothing unless this is the last close and the node has a
	 * last-close handler.
	 */
	if (vrefcnt(vn) > 1 || pn->pn_close == NULL)
		PFS_RETURN (0);

	if (pvd->pvd_pid != NO_PID)
		proc = pfind(pvd->pvd_pid);
	else
		proc = NULL;

	error = (pn->pn_close)(va->a_td, proc, pn);

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
	struct pfs_vdata *pvd = (struct pfs_vdata *)vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	struct vattr *vap = va->a_vap;
	struct proc *proc;
	int error = 0;

	PFS_TRACE((pn->pn_name));

	if (!pfs_visible(curthread, pn, pvd->pvd_pid))
		PFS_RETURN (ENOENT);

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
			error = (pn->pn_attr)(va->a_td, proc, pn, vap);
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
	struct pfs_vdata *pvd = (struct pfs_vdata *)vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	struct proc *proc = NULL;
	int error;

	PFS_TRACE(("%s: %lx", pn->pn_name, va->a_command));

	if (vn->v_type != VREG)
		PFS_RETURN (EINVAL);

	if (pn->pn_ioctl == NULL)
		PFS_RETURN (ENOTTY);

	/*
	 * This is necessary because process' privileges may
	 * have changed since the open() call.
	 */
	if (!pfs_visible(curthread, pn, pvd->pvd_pid))
		PFS_RETURN (EIO);

	/* XXX duplicates bits of pfs_visible() */
	if (pvd->pvd_pid != NO_PID) {
		if ((proc = pfind(pvd->pvd_pid)) == NULL)
			PFS_RETURN (EIO);
		_PHOLD(proc);
		PROC_UNLOCK(proc);
	}

	error = (pn->pn_ioctl)(curthread, proc, pn, va->a_command, va->a_data);

	if (proc != NULL)
		PRELE(proc);

	PFS_RETURN (error);
}

/*
 * Perform getextattr
 */
static int
pfs_getextattr(struct vop_getextattr_args *va)
{
	struct vnode *vn = va->a_vp;
	struct pfs_vdata *pvd = (struct pfs_vdata *)vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	struct proc *proc = NULL;
	int error;

	PFS_TRACE((pn->pn_name));

	if (!pfs_visible(curthread, pn, pvd->pvd_pid))
		PFS_RETURN (ENOENT);

	if (pn->pn_getextattr == NULL)
		PFS_RETURN (EOPNOTSUPP);

	/*
	 * This is necessary because either process' privileges may
	 * have changed since the open() call.
	 */
	if (!pfs_visible(curthread, pn, pvd->pvd_pid))
		PFS_RETURN (EIO);

	/* XXX duplicates bits of pfs_visible() */
	if (pvd->pvd_pid != NO_PID) {
		if ((proc = pfind(pvd->pvd_pid)) == NULL)
			PFS_RETURN (EIO);
		_PHOLD(proc);
		PROC_UNLOCK(proc);
	}

	error = (pn->pn_getextattr)(curthread, proc, pn, va->a_attrnamespace,
	    va->a_name, va->a_uio, va->a_size, va->a_cred);

	if (proc != NULL)
		PRELE(proc);

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
	struct pfs_vdata *pvd = (struct pfs_vdata *)vn->v_data;
	struct pfs_node *pd = pvd->pvd_pn;
	struct pfs_node *pn, *pdn = NULL;
	pid_t pid = pvd->pvd_pid;
	char *pname;
	int error, i, namelen;

	PFS_TRACE(("%.*s", (int)cnp->cn_namelen, cnp->cn_nameptr));

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

	/* check that parent directory is visisble... */
	if (!pfs_visible(curthread, pd, pvd->pvd_pid))
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
		VOP_UNLOCK(vn, 0, cnp->cn_thread);
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
		pn = pd->pn_parent;
		goto got_pnode;
	}

	/* named node */
	for (pn = pd->pn_nodes; pn != NULL; pn = pn->pn_next)
		if (pn->pn_type == pfstype_procdir)
			pdn = pn;
		else if (pn->pn_name[namelen] == '\0' &&
		    bcmp(pname, pn->pn_name, namelen) == 0)
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
	if (pn != pd->pn_parent && !pn->pn_parent)
		pn->pn_parent = pd;
	if (!pfs_visible(curthread, pn, pvd->pvd_pid)) {
		error = ENOENT;
		goto failed;
	}

	error = pfs_vncache_alloc(vn->v_mount, vpp, pn, pid);
	if (error)
		goto failed;

	if (cnp->cn_flags & ISDOTDOT)
		vn_lock(vn, LK_EXCLUSIVE|LK_RETRY, cnp->cn_thread);
	if (cnp->cn_flags & MAKEENTRY)
		cache_enter(vn, *vpp, cnp);
	PFS_RETURN (0);
 failed:
	if (cnp->cn_flags & ISDOTDOT)
		vn_lock(vn, LK_EXCLUSIVE|LK_RETRY, cnp->cn_thread);
	PFS_RETURN(error);
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

	PFS_TRACE(("%s (mode 0x%x)", pn->pn_name, mode));

	/*
	 * check if the file is visible to the caller
	 *
	 * XXX Not sure if this is necessary, as the VFS system calls
	 * XXX pfs_lookup() and pfs_access() first, and pfs_lookup()
	 * XXX calls pfs_visible().  There's a race condition here, but
	 * XXX calling pfs_visible() from here doesn't really close it,
	 * XXX and the only consequence of that race is an EIO further
	 * XXX down the line.
	 */
	if (!pfs_visible(va->a_td, pn, pvd->pvd_pid))
		PFS_RETURN (ENOENT);

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
	struct pfs_vdata *pvd = (struct pfs_vdata *)vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	struct uio *uio = va->a_uio;
	struct proc *proc = NULL;
	struct sbuf *sb = NULL;
	int error;
	unsigned int buflen, offset, resid;

	PFS_TRACE((pn->pn_name));

	if (vn->v_type != VREG)
		PFS_RETURN (EINVAL);

	if (!(pn->pn_flags & PFS_RD))
		PFS_RETURN (EBADF);

	if (pn->pn_func == NULL)
		PFS_RETURN (EIO);

	/*
	 * This is necessary because either process' privileges may
	 * have changed since the open() call.
	 */
	if (!pfs_visible(curthread, pn, pvd->pvd_pid))
		PFS_RETURN (EIO);

	/* XXX duplicates bits of pfs_visible() */
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

	/* Beaucoup sanity checks so we don't ask for bogus allocation. */
	if (uio->uio_offset < 0 || uio->uio_resid < 0 ||
	    (offset = uio->uio_offset) != uio->uio_offset ||
	    (resid = uio->uio_resid) != uio->uio_resid ||
	    (buflen = offset + resid) < offset || buflen > INT_MAX) {
		if (proc != NULL)
			PRELE(proc);
		PFS_RETURN (EINVAL);
	}
	if (buflen > MAXPHYS) {
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

	error = (pn->pn_func)(curthread, proc, pn, sb, uio);

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
pfs_iterate(struct thread *td, pid_t pid, struct pfs_node *pd,
	    struct pfs_node **pn, struct proc **p)
{
	sx_assert(&allproc_lock, SX_LOCKED);
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
	}

	if ((*pn) == NULL)
		return (-1);

	if (!pfs_visible(td, *pn, *p ? (*p)->p_pid : pid))
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
	struct pfs_info *pi = (struct pfs_info *)vn->v_mount->mnt_data;
	struct pfs_vdata *pvd = (struct pfs_vdata *)vn->v_data;
	struct pfs_node *pd = pvd->pvd_pn;
	pid_t pid = pvd->pvd_pid;
	struct pfs_node *pn;
	struct dirent entry;
	struct uio *uio;
	struct proc *p;
	off_t offset;
	int error, i, resid;
	char *buf, *ent;

	PFS_TRACE((pd->pn_name));

	if (vn->v_type != VDIR)
		PFS_RETURN (ENOTDIR);
	uio = va->a_uio;

	/* check if the directory is visible to the caller */
	if (!pfs_visible(curthread, pd, pid))
		PFS_RETURN (ENOENT);

	/* only allow reading entire entries */
	offset = uio->uio_offset;
	resid = uio->uio_resid;
	if (offset < 0 || offset % PFS_DELEN != 0 ||
	    (resid && resid < PFS_DELEN))
		PFS_RETURN (EINVAL);
	if (resid == 0)
		PFS_RETURN (0);

	/* skip unwanted entries */
	sx_slock(&allproc_lock);
	for (pn = NULL, p = NULL; offset > 0; offset -= PFS_DELEN)
		if (pfs_iterate(curthread, pid, pd, &pn, &p) == -1) {
			/* nothing left... */
			sx_sunlock(&allproc_lock);
			PFS_RETURN (0);
		}

	/* fill in entries */
	ent = buf = malloc(resid, M_IOV, M_WAITOK);
	entry.d_reclen = PFS_DELEN;
	while (pfs_iterate(curthread, pid, pd, &pn, &p) != -1 &&
	    resid >= PFS_DELEN) {
		if (!pn->pn_parent)
			pn->pn_parent = pd;
		if (!pn->pn_fileno)
			pfs_fileno_alloc(pi, pn);
		if (pid != NO_PID)
			entry.d_fileno = pn->pn_fileno * NO_PID + pid;
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
			panic("%s has unexpected node type: %d", pn->pn_name, pn->pn_type);
		}
		PFS_TRACE((entry.d_name));
		bcopy(&entry, ent, PFS_DELEN); /* XXX waste of cycles */
		offset += PFS_DELEN;
		resid -= PFS_DELEN;
		ent += PFS_DELEN;
	}
	sx_sunlock(&allproc_lock);
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
	struct pfs_vdata *pvd = (struct pfs_vdata *)vn->v_data;
	struct pfs_node *pn = pvd->pvd_pn;
	struct uio *uio = va->a_uio;
	struct proc *proc = NULL;
	char buf[MAXPATHLEN];
	struct sbuf sb;
	int error;

	PFS_TRACE((pn->pn_name));

	if (vn->v_type != VLNK)
		PFS_RETURN (EINVAL);

	if (pn->pn_func == NULL)
		PFS_RETURN (EIO);

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
	PFS_TRACE((((struct pfs_vdata *)va->a_vp->v_data)->pvd_pn->pn_name));

	return (pfs_vncache_free(va->a_vp));
}

/*
 * Set attributes
 */
static int
pfs_setattr(struct vop_setattr_args *va)
{
	PFS_TRACE((((struct pfs_vdata *)va->a_vp->v_data)->pvd_pn->pn_name));

	PFS_RETURN (EOPNOTSUPP);
}

/*
 * Read from a file
 */
static int
pfs_write(struct vop_write_args *va)
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

	if (pn->pn_func == NULL)
		PFS_RETURN (EIO);

	/*
	 * This is necessary because either process' privileges may
	 * have changed since the open() call.
	 */
	if (!pfs_visible(curthread, pn, pvd->pvd_pid))
		PFS_RETURN (EIO);

	/* XXX duplicates bits of pfs_visible() */
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
