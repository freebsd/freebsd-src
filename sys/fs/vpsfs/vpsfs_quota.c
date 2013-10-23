/*-
 * Copyright (c) 2009-2013 Klaus P. Ohrhallinger <k@7he.at>
 * All rights reserved.
 *
 * Development of this software was partly funded by:
 *    TransIP.nl <http://www.transip.nl/>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

static const char vpsid[] =
    "$Id: vpsfs_quota.c 178 2013-06-13 10:36:51Z klaus $";


#include "opt_global.h"

#ifdef VPS

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/dirent.h>
#include <sys/namei.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/filedesc.h>
#include <sys/taskqueue.h>

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

#include <fs/vpsfs/vpsfs.h>

int vn_fullpath1(struct thread *td, struct vnode *vp, struct vnode *rdir,
    char *buf, char **retbuf, u_int buflen);

#define DIRENT_MINSIZE (sizeof(struct dirent) - (MAXNAMLEN+1) + 4)

static int vpsfs_readdir(struct thread *td, struct vnode *vp,
    int dirbuflen, struct vpsfs_limits *,
    void (*cbfunc)(struct vnode *, struct vpsfs_limits *, struct thread *));

static void vpsfs_calcvnode(struct vnode *vp, struct vpsfs_limits *limits,
    struct thread *td);

/*
 *
 * Read a file with quota information from disk when mounting.
 * XXX Hide this quota file in upper layer.
 * Keep information from file up to date with all allocations/removals
 * and sync to disk every now and then.
 * Recalculate and write file to disk on request (ioctl).
 *
 */

static void
vpsfs_sync_task(void *context, int pending)
{
	struct vpsfs_mount *mount;

	mount = (struct vpsfs_mount *)context;

	mtx_lock(&mount->vpsfs_mtx);
	mount->limits_sync_task_enqueued = 0;
	mtx_unlock(&mount->vpsfs_mtx);

	if (vpsfs_write_usage(mount, mount->vpsfsm_limits) != 0)
		printf("%s: WARNING: couldn't sync!\n", __func__);

	mount->limits_last_sync = ticks;
}

static void
vpsfs_sched_sync(struct vpsfs_mount *mount)
{

	mtx_lock(&mount->vpsfs_mtx);
	if (!mount->limits_sync_task_enqueued &&
	    (ticks - mount->limits_last_sync) > (hz * 60)) {
		VPSFSDEBUG("%s: ticks=%u mount->limits_last_sync=%u "
		    "hz=%u\n", __func__, ticks, mount->limits_last_sync,
		    hz);
		TASK_INIT(&mount->limits_sync_task, 0, vpsfs_sync_task,
		    mount);
		taskqueue_enqueue(taskqueue_thread,
		    &mount->limits_sync_task);
		mount->limits_sync_task_enqueued = 1;
	}
	mtx_unlock(&mount->vpsfs_mtx);
}

int
vpsfs_limit_alloc(struct vpsfs_mount *mount, size_t space, size_t nodes)
{
	struct vpsfs_limits *lp;
	int rs, rn;
	int warn;
	int error;

	lp = mount->vpsfsm_limits;

	/* XXX lock */
	if (lp->space_hard != 0 && (lp->space_used + space >
	    lp->space_hard))
		/* hard */
		rs = 2;
	else if (lp->space_soft != 0 && (lp->space_used + space >
	    lp->space_soft))
		/* soft */
		rs = 1;
	else
		/* fine */
		rs = 0;

	if (lp->nodes_hard != 0 && (lp->nodes_used + nodes >
	    lp->nodes_hard))
		/* hard */
		rn = 2;
	else if (lp->nodes_soft != 0 && (lp->nodes_used + nodes >
	    lp->nodes_soft))
		/* soft */
		rn = 1;
	else
		/* fine */
		rn = 0;

	error = warn = 0;
	if (rs == 0 && rn == 0)
		/* fine */
		;
	else if (rs <= 1 && rn <= 1)
		/* warning */
		warn = 1;
	else
		/* deny */
		error = ENOSPC;

	if (warn)
		VPSFSDEBUG("%s: WARNING: hit soft limit !\n", __func__);

	if (error == 0) {
		VPSFSDEBUG("%s: space_used: %zu -> %zu    "
		    "nodes_used: %zu -> %zu\n", __func__,
		    lp->space_used, lp->space_used + space,
		    lp->nodes_used, lp->nodes_used + nodes);

		/* actually allocate */
		lp->space_used += space;
		lp->nodes_used += nodes;
	} else {
		VPSFSDEBUG("%s: DENIED allocation\n", __func__);
	}

	/* XXX unlock */

	vpsfs_sched_sync(mount);

	return (0);
}

int
vpsfs_limit_free(struct vpsfs_mount *mount, size_t space, size_t nodes)
{
	struct vpsfs_limits *lp;

	lp = mount->vpsfsm_limits;

	/* XXX lock */

	VPSFSDEBUG("%s: space_used: %zu -> %zu    nodes_used: %zu -> %zu\n",
		__func__,
		lp->space_used, lp->space_used - space,
		lp->nodes_used, lp->nodes_used - nodes);

	lp->space_used -= space;
	lp->nodes_used -= nodes;

	/* XXX unlock */

	vpsfs_sched_sync(mount);

	return (0);
}

int
vpsfs_write_usage(struct vpsfs_mount *mount, struct vpsfs_limits *limits)
{
	struct nameidata nd;
	struct iovec aiov[1];
	struct uio auio;
	struct thread *td;
	struct vnode *vp2;
	struct vnode *vp;
	struct mount *mp;
	char *filename;
	char *retbuf;
	char *buf;
	int error;
	int flags;

	VPSFSDEBUG("%s: mount=%p limits=%p\n", __func__, mount, limits);

	error = 0;
	td = curthread;
	vp = VPSFSVPTOLOWERVP(mount->vpsfsm_rootvp);
	vref(vp);

	/* determine pathname of quota file */

	filename = malloc(MAXPATHLEN, M_TEMP, M_WAITOK | M_ZERO);

	buf = malloc(MAXPATHLEN, M_TEMP, M_WAITOK | M_ZERO);
	retbuf = "-";

	error = vn_fullpath1(td, vp, td->td_proc->p_fd->fd_rdir, buf,
	    &retbuf, MAXPATHLEN);
	if (error) {
		VPSFSDEBUG("%s: vn_fullpath1(): error=%d\n",
		    __func__, error);
		goto out;
	}

	VPSFSDEBUG("%s: fullpath of lower fs root vnode relative to "
	    "processes rootdir is [%s]\n", __func__, retbuf);
	snprintf(filename, MAXPATHLEN, "%s/%s", retbuf, "vpsfs_usage");
	VPSFSDEBUG("%s: filename [%s]\n", __func__, filename);

	/* now open/create file */
	NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_SYSSPACE, filename, td);
	flags = FWRITE | O_NOFOLLOW | O_CREAT;
	error = vn_open(&nd, &flags, 0, NULL);
	if (error) {
		VPSFSDEBUG("%s: vn_open(): error=%d\n", __func__, error);
		goto out;
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp2 = nd.ni_vp;
	vref(vp2);
	VOP_UNLOCK(vp2, 0);

	/* write information to file */

	auio.uio_iov = &aiov[0];
	auio.uio_offset = 0;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_WRITE;
	aiov[0].iov_base = (caddr_t)limits;
	aiov[0].iov_len = sizeof(*limits);
	auio.uio_resid = sizeof(*limits);
	auio.uio_iovcnt = 1;
	auio.uio_td = td;

	crhold(td->td_ucred);
	vn_start_write(vp2, &mp, V_WAIT);
	vn_lock(vp2, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_WRITE(vp2, &auio, IO_UNIT | IO_SYNC, td->td_ucred);
	VOP_UNLOCK(vp2, 0);
	vn_finished_write(mp);
	(void) vn_close(vp2, FWRITE, td->td_ucred, td);
	crfree(td->td_ucred);
	vrele(vp2);
	if (error) {
		VPSFSDEBUG("%s: VOP_WRITE(): error=%d\n", __func__, error);
		goto out;
	}

	VPSFSDEBUG("%s: usage: space=%zu nodes=%zu\n",
		__func__, limits->space_used, limits->nodes_used);

  out:
	free(buf, M_TEMP);
	free(filename, M_TEMP);

	vrele(vp);

	return (error);
}

/* Read usage information from disk. */
int
vpsfs_read_usage(struct vpsfs_mount *mount, struct vpsfs_limits *limits)
{
	struct nameidata nd;
	struct iovec aiov[1];
	struct uio auio;
	struct thread *td;
	struct vnode *vp2;
	struct vnode *vp;
	char *filename;
	char *retbuf;
	char *buf;
	int needs_recalc;
	int error;
	int flags;

	VPSFSDEBUG("%s: mount=%p limits=%p\n", __func__, mount, limits);

	error = 0;
	needs_recalc = 0;
	td = curthread;
	vp = VPSFSVPTOLOWERVP(mount->vpsfsm_rootvp);
	vref(vp);

	/* determine pathname of quota file */

	filename = malloc(MAXPATHLEN, M_TEMP, M_WAITOK | M_ZERO);

	buf = malloc(MAXPATHLEN, M_TEMP, M_WAITOK | M_ZERO);
	retbuf = "-";

	error = vn_fullpath1(td, vp, td->td_proc->p_fd->fd_rdir, buf,
	    &retbuf, MAXPATHLEN);
	if (error) {
		VPSFSDEBUG("%s: vn_fullpath1(): error=%d\n",
		    __func__, error);
		goto out;
	}

	VPSFSDEBUG("%s: fullpath of lower fs root vnode relative to "
	    "processes rootdir is [%s]\n", __func__, retbuf);
	snprintf(filename, MAXPATHLEN, "%s/%s", retbuf, "vpsfs_usage");
	VPSFSDEBUG("%s: filename [%s]\n", __func__, filename);

	/* now open file */
	NDINIT(&nd, LOOKUP, NOFOLLOW, UIO_SYSSPACE, filename, td);
	flags = FREAD | O_NOFOLLOW;
	error = vn_open(&nd, &flags, 0, NULL);
	if (error == ENOENT) {
		VPSFSDEBUG("%s: file [%s] doesn't exist, recalculating!\n",
			__func__, filename);
		needs_recalc = 1;
		goto out;
	} else if (error != 0) {
		VPSFSDEBUG("%s: vn_open(): error=%d\n", __func__, error);
		goto out;
	}
	NDFREE(&nd, NDF_ONLY_PNBUF);
	vp2 = nd.ni_vp;
	vref(vp2);
	VOP_UNLOCK(vp2, 0);

	/* read information from file */

	auio.uio_iov = &aiov[0];
	auio.uio_offset = 0;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_READ;
	aiov[0].iov_base = (caddr_t)limits;
	aiov[0].iov_len = sizeof(*limits);
	auio.uio_resid = sizeof(*limits);
	auio.uio_iovcnt = 1;
	auio.uio_td = td;

	crhold(td->td_ucred);
	vn_lock(vp2, LK_EXCLUSIVE | LK_RETRY);
	error = VOP_READ(vp2, &auio, IO_UNIT | IO_SYNC, td->td_ucred);
	VOP_UNLOCK(vp2, 0);
	(void) vn_close(vp2, FREAD, td->td_ucred, td);
	crfree(td->td_ucred);
	vrele(vp2);
	if (error) {
		VPSFSDEBUG("%s: VOP_READ(): error=%d\n", __func__, error);
		goto out;
	}

  out:
	free(buf, M_TEMP);
	free(filename, M_TEMP);

	vrele(vp);

	if (needs_recalc)
		error = vpsfs_calcusage(mount, limits);

	VPSFSDEBUG("%s: usage: space=%zu nodes=%zu\n",
		__func__, limits->space_used, limits->nodes_used);

	return (error);
}

int
vpsfs_calcusage_path(const char *path, struct vpsfs_limits *limits)
{
	struct nameidata nd;
	struct thread *td;
	struct mount *mp;
	int error;

	td = curthread;

	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | AUDITVNODE1, UIO_SYSSPACE,
	    path, td);
	if ((error = namei(&nd)))
		return (error);
	NDFREE(&nd, NDF_ONLY_PNBUF);
	mp = nd.ni_vp->v_mount;
	vfs_ref(mp);
	vput(nd.ni_vp);

	if (vpsfs_mount_is_vpsfs(mp) == 0) {
		error = EINVAL;
		goto fail;
	}

	error = vpsfs_calcusage(MOUNTTOVPSFSMOUNT(mp), limits);

  fail:
	vfs_rel(mp);

	return (error);
}

int
vpsfs_calcusage(struct vpsfs_mount *mount, struct vpsfs_limits *limits)
{
	struct thread *td = curthread;
	//struct vpsfs_limits *limits;
	struct vnode *lrootvp;
	struct vnode *vp;
	struct vattr va;
	int dirbuflen;
	int error = 0;

	VPSFSDEBUG("%s: mount=%p\n", __func__, mount);

	//limits = malloc(sizeof(*limits), M_TEMP, M_WAITOK | M_ZERO);

	vp = lrootvp = VPSFSVPTOLOWERVP(mount->vpsfsm_rootvp);
	VOP_LOCK(lrootvp, LK_SHARED | LK_RETRY);

	error = VOP_GETATTR(lrootvp, &va, td->td_ucred);
	if (error) {
		//free(limits, M_TEMP);
		return (error);
	}

	dirbuflen = DEV_BSIZE;
	if (dirbuflen < va.va_blocksize)
		dirbuflen = va.va_blocksize;

	error = vpsfs_readdir(td, lrootvp, dirbuflen, limits,
	    vpsfs_calcvnode);

	VOP_UNLOCK(lrootvp, 0);

	VPSFSDEBUG("%s: limits: space_used=%zu nodes_used=%zu\n",
		__func__, limits->space_used, limits->nodes_used);

	if (mount->vpsfsm_limits != limits)
		memcpy(mount->vpsfsm_limits, limits, sizeof(*limits));

	error = vpsfs_write_usage(mount, limits);

	//free(limits, M_TEMP);

	return (error);
}

static void
vpsfs_calcvnode(struct vnode *vp, struct vpsfs_limits *limits,
    struct thread *td)
{
	struct stat *sb;
	int error;

	if (vp->v_type != VREG)
		return;

	sb = malloc(sizeof(*sb), M_TEMP, M_WAITOK | M_ZERO);

	error = vn_stat(vp, sb, td->td_ucred, td->td_ucred, td);
	if (error) {
		VPSFSDEBUG("%s: vn_stat() error=%d\n",
			__func__, error);
		goto out;
	}

	/* multiple links are counted only as (size / link count) */

	limits->space_used += sb->st_size / sb->st_nlink;
	limits->nodes_used += 1;

 out:
	free(sb, M_TEMP);
	return;
}

/*
 * XXX Better perform it without recursing functions !
 */

static int
vpsfs_readdir(struct thread *td, struct vnode *vp, int dirbuflen,
    struct vpsfs_limits *limits,
    void (*cbfunc)(struct vnode *, struct vpsfs_limits *, struct thread *))
{
	struct componentname *cnp;
	struct dirent *dp;
	struct vnode *vp2;
	char *dirbuf;
	char *cpos = NULL;
	off_t off;
	int eofflag;
	int len;
	int error = 0;

	//VPSFSDEBUG("%s: vp=%p\n", __func__, vp);
	dirbuf = (char *)malloc(dirbuflen, M_TEMP, M_WAITOK);

	/* Walk directory tree ... */
	off = 0;
	len = 0;
	do {
		error = get_next_dirent(vp, &dp, dirbuf, dirbuflen, &off,
					&cpos, &len, &eofflag, td);
		if (error) {
			VPSFSDEBUG("%s: get_next_dirent() error=%d\n",
				__func__, error);
			goto out;
		}

		/*
		VPSFSDEBUG("%s: dp=%p dp->d_type=%d dp->d_name=[%s]\n",
			__func__, dp, dp->d_type, dp->d_name);
		*/

		if (dp->d_type == DT_DIR &&
		    (strcmp(dp->d_name, ".")==0 ||
		    strcmp(dp->d_name, "..")==0))
			continue;

		cnp = malloc(sizeof(*cnp), M_TEMP, M_WAITOK | M_ZERO);
		cnp->cn_thread = td;
		cnp->cn_cred = td->td_ucred;
		cnp->cn_lkflags = LK_SHARED;
		cnp->cn_nameiop = LOOKUP;
		cnp->cn_pnbuf = dp->d_name;
		cnp->cn_consume = strlen(dp->d_name);
		cnp->cn_nameptr = dp->d_name;
		cnp->cn_namelen = strlen(dp->d_name);

		error = VOP_LOOKUP(vp, &vp2, cnp);
		free(cnp, M_TEMP);
		if (error) {
			VPSFSDEBUG("%s: VOP_LOOKUP() error=%d\n",
				__func__, error);
			goto out;
		}

		cbfunc(vp2, limits, td);

		if (dp->d_type == DT_DIR &&
		    strcmp(dp->d_name, ".") &&
		    strcmp(dp->d_name, "..")) {

			if (vp2->v_mount != vp->v_mount) {
				VPSFSDEBUG("%s: vp=%p vp2=%p have different"
				    " mounts\n", __func__, vp, vp2);
				goto next;
			}

			error = vpsfs_readdir(td, vp2, dirbuflen, limits,
			    cbfunc);
			if (error) {
				VPSFSDEBUG("%s: vpsfs_readdir() error=%d\n",
					__func__, error);
				vput(vp2);
				goto out;
			}
		}

	  next:
		vput(vp2);

	} while (len > 0 || !eofflag);

 out:
	free(dirbuf, M_TEMP);
	return (error);
}

#endif /* VPS */

/* EOF */
