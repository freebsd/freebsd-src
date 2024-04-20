/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 Juniper Networks, Inc.
 * Copyright (c) 2022-2023 Klara, Inc.
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

#include "opt_tarfs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/dirent.h>
#include <sys/fcntl.h>
#include <sys/limits.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/vnode.h>

#include <fs/tarfs/tarfs.h>
#include <fs/tarfs/tarfs_dbg.h>

static int
tarfs_open(struct vop_open_args *ap)
{
	struct tarfs_node *tnp;
	struct vnode *vp;

	vp = ap->a_vp;
	MPASS(VOP_ISLOCKED(vp));
	tnp = VP_TO_TARFS_NODE(vp);

	TARFS_DPF(VNODE, "%s(%p=%s, %o)\n", __func__,
	    tnp, tnp->name, ap->a_mode);

	if (vp->v_type != VREG && vp->v_type != VDIR)
		return (EOPNOTSUPP);

	vnode_create_vobject(vp, tnp->size, ap->a_td);
	return (0);
}

static int
tarfs_close(struct vop_close_args *ap)
{
#ifdef TARFS_DEBUG
	struct tarfs_node *tnp;
	struct vnode *vp;

	vp = ap->a_vp;

	MPASS(VOP_ISLOCKED(vp));
	tnp = VP_TO_TARFS_NODE(vp);

	TARFS_DPF(VNODE, "%s(%p=%s)\n", __func__,
	    tnp, tnp->name);
#else
	(void)ap;
#endif
	return (0);
}

static int
tarfs_access(struct vop_access_args *ap)
{
	struct tarfs_node *tnp;
	struct vnode *vp;
	accmode_t accmode;
	struct ucred *cred;
	int error;

	vp = ap->a_vp;
	accmode = ap->a_accmode;
	cred = ap->a_cred;

	MPASS(VOP_ISLOCKED(vp));
	tnp = VP_TO_TARFS_NODE(vp);

	TARFS_DPF(VNODE, "%s(%p=%s, %o)\n", __func__,
	    tnp, tnp->name, accmode);

	switch (vp->v_type) {
	case VDIR:
	case VLNK:
	case VREG:
		if ((accmode & VWRITE) != 0)
			return (EROFS);
		break;
	case VBLK:
	case VCHR:
	case VFIFO:
		break;
	default:
		return (EINVAL);
	}

	if ((accmode & VWRITE) != 0)
		return (EPERM);

	error = vaccess(vp->v_type, tnp->mode, tnp->uid,
	    tnp->gid, accmode, cred);
	return (error);
}

static int
tarfs_bmap(struct vop_bmap_args *ap)
{
	struct tarfs_node *tnp;
	struct vnode *vp;
	off_t off;
	uint64_t iosize;
	int ra, rb, rmax;

	vp = ap->a_vp;
	iosize = vp->v_mount->mnt_stat.f_iosize;

	if (ap->a_bop != NULL)
		*ap->a_bop = &vp->v_bufobj;
	if (ap->a_bnp != NULL)
		*ap->a_bnp = ap->a_bn * btodb(iosize);
	if (ap->a_runp == NULL)
		return (0);

	tnp = VP_TO_TARFS_NODE(vp);
	off = ap->a_bn * iosize;

	ra = rb = 0;
	for (u_int i = 0; i < tnp->nblk; i++) {
		off_t bs, be;

		bs = tnp->blk[i].o;
		be = tnp->blk[i].o + tnp->blk[i].l;
		if (off > be)
			continue;
		else if (off < bs) {
			/* We're in a hole. */
			ra = bs - off < iosize ?
			    0 : howmany(bs - (off + iosize), iosize);
			rb = howmany(off - (i == 0 ?
			    0 : tnp->blk[i - 1].o + tnp->blk[i - 1].l),
			    iosize);
			break;
		} else {
			/* We'll be reading from the backing file. */
			ra = be - off < iosize ?
			    0 : howmany(be - (off + iosize), iosize);
			rb = howmany(off - bs, iosize);
			break;
		}
	}

	rmax = vp->v_mount->mnt_iosize_max / iosize - 1;
	*ap->a_runp = imin(ra, rmax);
	if (ap->a_runb != NULL)
		*ap->a_runb = imin(rb, rmax);
	return (0);
}

static int
tarfs_getattr(struct vop_getattr_args *ap)
{
	struct tarfs_node *tnp;
	struct vnode *vp;
	struct vattr *vap;

	vp = ap->a_vp;
	vap = ap->a_vap;
	tnp = VP_TO_TARFS_NODE(vp);

	TARFS_DPF(VNODE, "%s(%p=%s)\n", __func__,
	    tnp, tnp->name);

	vap->va_type = vp->v_type;
	vap->va_mode = tnp->mode;
	vap->va_nlink = tnp->nlink;
	vap->va_gid = tnp->gid;
	vap->va_uid = tnp->uid;
	vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	vap->va_fileid = tnp->ino;
	vap->va_size = tnp->size;
	vap->va_blocksize = vp->v_mount->mnt_stat.f_iosize;
	vap->va_atime = tnp->atime;
	vap->va_ctime = tnp->ctime;
	vap->va_mtime = tnp->mtime;
	vap->va_birthtime = tnp->birthtime;
	vap->va_gen = tnp->gen;
	vap->va_flags = tnp->flags;
	vap->va_rdev = (vp->v_type == VBLK || vp->v_type == VCHR) ?
	    tnp->rdev : NODEV;
	vap->va_bytes = round_page(tnp->physize);
	vap->va_filerev = 0;

	return (0);
}

static int
tarfs_lookup(struct vop_cachedlookup_args *ap)
{
	struct tarfs_mount *tmp;
	struct tarfs_node *dirnode, *parent, *tnp;
	struct componentname *cnp;
	struct vnode *dvp, **vpp;
#ifdef TARFS_DEBUG
	struct vnode *vp;
#endif
	int error;

	dvp = ap->a_dvp;
	vpp = ap->a_vpp;
	cnp = ap->a_cnp;

	*vpp = NULLVP;
	dirnode = VP_TO_TARFS_NODE(dvp);
	parent = dirnode->parent;
	tmp = dirnode->tmp;
	tnp = NULL;

	TARFS_DPF(LOOKUP, "%s(%p=%s, %.*s)\n", __func__,
	    dirnode, dirnode->name,
	    (int)cnp->cn_namelen, cnp->cn_nameptr);

	error = VOP_ACCESS(dvp, VEXEC, cnp->cn_cred, curthread);
	if (error != 0)
		return (error);

	if (cnp->cn_flags & ISDOTDOT) {
		/* Do not allow .. on the root node */
		if (parent == NULL || parent == dirnode)
			return (ENOENT);

		/* Allocate a new vnode on the matching entry */
		error = vn_vget_ino(dvp, parent->ino, cnp->cn_lkflags,
		    vpp);
		if (error != 0)
			return (error);
	} else if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
		VREF(dvp);
		*vpp = dvp;
#ifdef TARFS_DEBUG
	} else if (dirnode == dirnode->tmp->root &&
	    (vp = dirnode->tmp->znode) != NULL &&
	    cnp->cn_namelen == TARFS_ZIO_NAMELEN &&
	    memcmp(cnp->cn_nameptr, TARFS_ZIO_NAME, TARFS_ZIO_NAMELEN) == 0) {
		error = vn_lock(vp, cnp->cn_lkflags);
		if (error != 0)
			return (error);
		vref(vp);
		*vpp = vp;
		return (0);
#endif
	} else {
		tnp = tarfs_lookup_node(dirnode, NULL, cnp);
		if (tnp == NULL) {
			TARFS_DPF(LOOKUP, "%s(%p=%s, %.*s): file not found\n", __func__,
			    dirnode, dirnode->name,
			    (int)cnp->cn_namelen, cnp->cn_nameptr);
			return (ENOENT);
		}

		if ((cnp->cn_flags & ISLASTCN) == 0 &&
		    (tnp->type != VDIR && tnp->type != VLNK))
			return (ENOTDIR);

		error = VFS_VGET(tmp->vfs, tnp->ino, cnp->cn_lkflags, vpp);
		if (error != 0)
			return (error);
	}

#ifdef	TARFS_DEBUG
	if (tnp == NULL)
		tnp = VP_TO_TARFS_NODE(*vpp);
	TARFS_DPF(LOOKUP, "%s: found vnode %p, tarfs_node %p\n", __func__,
	    *vpp, tnp);
#endif	/* TARFS_DEBUG */

	/* Store the result of the cache if MAKEENTRY is specified in flags */
	if ((cnp->cn_flags & MAKEENTRY) != 0 && cnp->cn_nameiop != CREATE)
		cache_enter(dvp, *vpp, cnp);

	return (error);
}

static int
tarfs_readdir(struct vop_readdir_args *ap)
{
	struct dirent cde = { };
	struct tarfs_node *current, *tnp;
	struct vnode *vp;
	struct uio *uio;
	int *eofflag;
	uint64_t **cookies;
	int *ncookies;
	off_t off;
	u_int idx, ndirents;
	int error;

	vp = ap->a_vp;
	uio = ap->a_uio;
	eofflag = ap->a_eofflag;
	cookies = ap->a_cookies;
	ncookies = ap->a_ncookies;

	if (vp->v_type != VDIR)
		return (ENOTDIR);

	tnp = VP_TO_TARFS_NODE(vp);
	off = uio->uio_offset;
	current = NULL;
	ndirents = 0;

	TARFS_DPF(VNODE, "%s(%p=%s, %zu, %zd)\n", __func__,
	    tnp, tnp->name, uio->uio_offset, uio->uio_resid);

	if (uio->uio_offset == TARFS_COOKIE_EOF) {
		TARFS_DPF(VNODE, "%s: EOF\n", __func__);
		return (0);
	}

	if (uio->uio_offset == TARFS_COOKIE_DOT) {
		TARFS_DPF(VNODE, "%s: Generating . entry\n", __func__);
		/* fake . entry */
		cde.d_fileno = tnp->ino;
		cde.d_type = DT_DIR;
		cde.d_namlen = 1;
		cde.d_name[0] = '.';
		cde.d_name[1] = '\0';
		cde.d_reclen = GENERIC_DIRSIZ(&cde);
		if (cde.d_reclen > uio->uio_resid)
			goto full;
		dirent_terminate(&cde);
		error = uiomove(&cde, cde.d_reclen, uio);
		if (error)
			return (error);
		/* next is .. */
		uio->uio_offset = TARFS_COOKIE_DOTDOT;
		ndirents++;
	}

	if (uio->uio_offset == TARFS_COOKIE_DOTDOT) {
		TARFS_DPF(VNODE, "%s: Generating .. entry\n", __func__);
		/* fake .. entry */
		MPASS(tnp->parent != NULL);
		TARFS_NODE_LOCK(tnp->parent);
		cde.d_fileno = tnp->parent->ino;
		TARFS_NODE_UNLOCK(tnp->parent);
		cde.d_type = DT_DIR;
		cde.d_namlen = 2;
		cde.d_name[0] = '.';
		cde.d_name[1] = '.';
		cde.d_name[2] = '\0';
		cde.d_reclen = GENERIC_DIRSIZ(&cde);
		if (cde.d_reclen > uio->uio_resid)
			goto full;
		dirent_terminate(&cde);
		error = uiomove(&cde, cde.d_reclen, uio);
		if (error)
			return (error);
		/* next is first child */
		current = TAILQ_FIRST(&tnp->dir.dirhead);
		if (current == NULL)
			goto done;
		uio->uio_offset = current->ino;
		TARFS_DPF(VNODE, "%s: [%u] setting current node to %p=%s\n",
		    __func__, ndirents, current, current->name);
		ndirents++;
	}

	/* resuming previous call */
	if (current == NULL) {
		current = tarfs_lookup_dir(tnp, uio->uio_offset);
		if (current == NULL) {
			error = EINVAL;
			goto done;
		}
		uio->uio_offset = current->ino;
		TARFS_DPF(VNODE, "%s: [%u] setting current node to %p=%s\n",
		    __func__, ndirents, current, current->name);
	}

	for (;;) {
		cde.d_fileno = current->ino;
		switch (current->type) {
		case VBLK:
			cde.d_type = DT_BLK;
			break;
		case VCHR:
			cde.d_type = DT_CHR;
			break;
		case VDIR:
			cde.d_type = DT_DIR;
			break;
		case VFIFO:
			cde.d_type = DT_FIFO;
			break;
		case VLNK:
			cde.d_type = DT_LNK;
			break;
		case VREG:
			cde.d_type = DT_REG;
			break;
		default:
			panic("%s: tarfs_node %p, type %d\n", __func__,
			    current, current->type);
		}
		cde.d_namlen = current->namelen;
		MPASS(tnp->namelen < sizeof(cde.d_name));
		(void)memcpy(cde.d_name, current->name, current->namelen);
		cde.d_name[current->namelen] = '\0';
		cde.d_reclen = GENERIC_DIRSIZ(&cde);
		if (cde.d_reclen > uio->uio_resid)
			goto full;
		dirent_terminate(&cde);
		error = uiomove(&cde, cde.d_reclen, uio);
		if (error != 0)
			goto done;
		ndirents++;
		/* next sibling */
		current = TAILQ_NEXT(current, dirents);
		if (current == NULL)
			goto done;
		uio->uio_offset = current->ino;
		TARFS_DPF(VNODE, "%s: [%u] setting current node to %p=%s\n",
		    __func__, ndirents, current, current->name);
	}
full:
	if (cde.d_reclen > uio->uio_resid) {
		TARFS_DPF(VNODE, "%s: out of space, returning\n",
		    __func__);
		error = (ndirents == 0) ? EINVAL : 0;
	}
done:
	TARFS_DPF(VNODE, "%s: %u entries written\n", __func__, ndirents);
	TARFS_DPF(VNODE, "%s: saving cache information\n", __func__);
	if (current == NULL) {
		uio->uio_offset = TARFS_COOKIE_EOF;
		tnp->dir.lastcookie = 0;
		tnp->dir.lastnode = NULL;
	} else {
		tnp->dir.lastcookie = current->ino;
		tnp->dir.lastnode = current;
	}

	if (eofflag != NULL) {
		TARFS_DPF(VNODE, "%s: Setting EOF flag\n", __func__);
		*eofflag = (error == 0 && current == NULL);
	}

	/* Update for NFS */
	if (error == 0 && cookies != NULL && ncookies != NULL) {
		TARFS_DPF(VNODE, "%s: Updating NFS cookies\n", __func__);
		current = NULL;
		*cookies = malloc(ndirents * sizeof(off_t), M_TEMP, M_WAITOK);
		*ncookies = ndirents;
		for (idx = 0; idx < ndirents; idx++) {
			if (off == TARFS_COOKIE_DOT)
				off = TARFS_COOKIE_DOTDOT;
			else {
				if (off == TARFS_COOKIE_DOTDOT) {
					current = TAILQ_FIRST(&tnp->dir.dirhead);
				} else if (current != NULL) {
					current = TAILQ_NEXT(current, dirents);
				} else {
					current = tarfs_lookup_dir(tnp, off);
					current = TAILQ_NEXT(current, dirents);
				}
				if (current == NULL)
					off = TARFS_COOKIE_EOF;
				else
					off = current->ino;
			}

			TARFS_DPF(VNODE, "%s: [%u] offset %zu\n", __func__,
			    idx, off);
			(*cookies)[idx] = off;
		}
		MPASS(uio->uio_offset == off);
	}

	return (error);
}

static int
tarfs_read(struct vop_read_args *ap)
{
	struct tarfs_node *tnp;
	struct uio *uiop;
	struct vnode *vp;
	size_t len;
	off_t resid;
	int error;

	uiop = ap->a_uio;
	vp = ap->a_vp;

	if (vp->v_type == VCHR || vp->v_type == VBLK)
		return (EOPNOTSUPP);

	if (vp->v_type != VREG)
		return (EISDIR);

	if (uiop->uio_offset < 0)
		return (EINVAL);

	tnp = VP_TO_TARFS_NODE(vp);
	error = 0;

	TARFS_DPF(VNODE, "%s(%p=%s, %zu, %zd)\n", __func__,
	    tnp, tnp->name, uiop->uio_offset, uiop->uio_resid);

	while ((resid = uiop->uio_resid) > 0) {
		if (tnp->size <= uiop->uio_offset)
			break;
		len = MIN(tnp->size - uiop->uio_offset, resid);
		if (len == 0)
			break;

		error = tarfs_read_file(tnp, len, uiop);
		if (error != 0 || resid == uiop->uio_resid)
			break;
	}

	return (error);
}

static int
tarfs_readlink(struct vop_readlink_args *ap)
{
	struct tarfs_node *tnp;
	struct uio *uiop;
	struct vnode *vp;
	int error;

	uiop = ap->a_uio;
	vp = ap->a_vp;

	MPASS(uiop->uio_offset == 0);
	MPASS(vp->v_type == VLNK);

	tnp = VP_TO_TARFS_NODE(vp);

	TARFS_DPF(VNODE, "%s(%p=%s)\n", __func__,
	    tnp, tnp->name);

	error = uiomove(tnp->link.name,
	    MIN(tnp->size, uiop->uio_resid), uiop);

	return (error);
}

static int
tarfs_reclaim(struct vop_reclaim_args *ap)
{
	struct tarfs_node *tnp;
	struct vnode *vp;

	vp = ap->a_vp;
	tnp = VP_TO_TARFS_NODE(vp);

	vfs_hash_remove(vp);

	TARFS_NODE_LOCK(tnp);
	tnp->vnode = NULLVP;
	vp->v_data = NULL;
	TARFS_NODE_UNLOCK(tnp);

	return (0);
}

static int
tarfs_print(struct vop_print_args *ap)
{
	struct tarfs_node *tnp;
	struct vnode *vp;

	vp = ap->a_vp;
	tnp = VP_TO_TARFS_NODE(vp);

	printf("tag tarfs, tarfs_node %p, links %lu\n",
	    tnp, (unsigned long)tnp->nlink);
	printf("\tmode 0%o, owner %d, group %d, size %zd\n",
	    tnp->mode, tnp->uid, tnp->gid,
	    tnp->size);

	if (vp->v_type == VFIFO)
		fifo_printinfo(vp);

	printf("\n");

	return (0);
}

static int
tarfs_strategy(struct vop_strategy_args *ap)
{
	struct uio auio;
	struct iovec iov;
	struct tarfs_node *tnp;
	struct buf *bp;
	off_t off;
	size_t len;
	int error;

	tnp = VP_TO_TARFS_NODE(ap->a_vp);
	bp = ap->a_bp;
	MPASS(bp->b_iocmd == BIO_READ);
	MPASS(bp->b_iooffset >= 0);
	MPASS(bp->b_bcount > 0);
	MPASS(bp->b_bufsize >= bp->b_bcount);
	TARFS_DPF(VNODE, "%s(%p=%s, %zu, %ld/%ld)\n", __func__, tnp,
	    tnp->name, (size_t)bp->b_iooffset, bp->b_bcount, bp->b_bufsize);
	iov.iov_base = bp->b_data;
	iov.iov_len = bp->b_bcount;
	off = bp->b_iooffset;
	len = bp->b_bcount;
	bp->b_resid = len;
	if (off > tnp->size) {
		/* XXX read beyond EOF - figure out correct handling */
		error = EIO;
		goto out;
	}
	if (off + len > tnp->size) {
		/* clip to file length */
		len = tnp->size - off;
	}
	auio.uio_iov = &iov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = off;
	auio.uio_resid = len;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_td = curthread;
	error = tarfs_read_file(tnp, len, &auio);
	bp->b_resid -= len - auio.uio_resid;
out:
	if (error != 0) {
		bp->b_ioflags |= BIO_ERROR;
		bp->b_error = error;
	}
	bp->b_flags |= B_DONE;
	return (0);
}

static int
tarfs_vptofh(struct vop_vptofh_args *ap)
{
	struct tarfs_fid *tfp;
	struct tarfs_node *tnp;

	tfp = (struct tarfs_fid *)ap->a_fhp;
	tnp = VP_TO_TARFS_NODE(ap->a_vp);

	tfp->len = sizeof(struct tarfs_fid);
	tfp->ino = tnp->ino;
	tfp->gen = tnp->gen;

	return (0);
}

struct vop_vector tarfs_vnodeops = {
	.vop_default =		&default_vnodeops,

	.vop_access =		tarfs_access,
	.vop_bmap =		tarfs_bmap,
	.vop_cachedlookup =	tarfs_lookup,
	.vop_close =		tarfs_close,
	.vop_getattr =		tarfs_getattr,
	.vop_lookup =		vfs_cache_lookup,
	.vop_open =		tarfs_open,
	.vop_print =		tarfs_print,
	.vop_read =		tarfs_read,
	.vop_readdir =		tarfs_readdir,
	.vop_readlink =		tarfs_readlink,
	.vop_reclaim =		tarfs_reclaim,
	.vop_strategy =		tarfs_strategy,
	.vop_vptofh =		tarfs_vptofh,
};
VFS_VOP_VECTOR_REGISTER(tarfs_vnodeops);
