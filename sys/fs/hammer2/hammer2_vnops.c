/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2022 Tomohiro Kusumi <tkusumi@netbsd.org>
 * Copyright (c) 2011-2022 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@dragonflybsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/dirent.h>
#include <sys/limits.h>
#include <sys/malloc.h>
#include <sys/namei.h>
#include <sys/uio.h>
#include <sys/unistd.h>
#include <sys/vnode.h>

#include <vm/vnode_pager.h>

#include "hammer2.h"

static int
hammer2_inactive(struct vop_inactive_args *ap)
{
	struct vnode *vp = ap->a_vp;
	hammer2_inode_t *ip = VTOI(vp);

	if (ip->meta.mode == 0)
		vrecycle(vp);

	return (0);
}

static int
hammer2_reclaim(struct vop_reclaim_args *ap)
{
	struct vnode *vp = ap->a_vp;
	hammer2_inode_t *ip = VTOI(vp);

	vfs_hash_remove(vp);

	vp->v_data = NULL;
	ip->vp = NULL;

	hammer2_inode_drop(ip);

	return (0);
}

static int
hammer2_access(struct vop_access_args *ap)
{
	struct vnode *vp = ap->a_vp;
	hammer2_inode_t *ip = VTOI(vp);
	uid_t uid;
	gid_t gid;
	mode_t mode;

	if (vp->v_type == VCHR || vp->v_type == VBLK)
		return (EOPNOTSUPP);

	/*
	 * Disallow write attempts unless the file is a socket,
	 * fifo resident on the filesystem.
	 */
	if (ap->a_accmode & VWRITE) {
		switch (vp->v_type) {
		case VDIR:
		case VLNK:
		case VREG:
			return (EROFS);
			/* NOT REACHED */
		default:
			break;
		}
	}

	uid = hammer2_to_unix_xid(&ip->meta.uid);
	gid = hammer2_to_unix_xid(&ip->meta.gid);
	mode = ip->meta.mode;

	return (vaccess(vp->v_type, mode, uid, gid, ap->a_accmode, ap->a_cred));
}

static int
hammer2_getattr(struct vop_getattr_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;
	hammer2_inode_t *ip = VTOI(vp);
	hammer2_pfs_t *pmp = ip->pmp;

	vap->va_fsid = pmp->mp->mnt_stat.f_fsid.val[0];
	vap->va_fileid = ip->meta.inum;
	vap->va_mode = ip->meta.mode;
	vap->va_nlink = ip->meta.nlinks;
	vap->va_uid = hammer2_to_unix_xid(&ip->meta.uid);
	vap->va_gid = hammer2_to_unix_xid(&ip->meta.gid);
	vap->va_rdev = NODEV;
	vap->va_size = ip->meta.size;
	vap->va_flags = ip->meta.uflags;
	hammer2_time_to_timespec(ip->meta.ctime, &vap->va_ctime);
	hammer2_time_to_timespec(ip->meta.mtime, &vap->va_mtime);
	hammer2_time_to_timespec(ip->meta.mtime, &vap->va_atime);
	vap->va_gen = 1;
	vap->va_blocksize = vp->v_mount->mnt_stat.f_iosize;
	if (ip->meta.type == HAMMER2_OBJTYPE_DIRECTORY) {
		/*
		 * Can't really calculate directory use sans the files under
		 * it, just assume one block for now.
		 */
		vap->va_bytes = HAMMER2_INODE_BYTES;
	} else {
		vap->va_bytes = hammer2_inode_data_count(ip);
	}
	vap->va_type = hammer2_get_vtype(ip->meta.type);
	vap->va_filerev = 0;

	return (0);
}

static int
hammer2_setattr(struct vop_setattr_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct vattr *vap = ap->a_vap;

	if (vap->va_flags != (u_long)VNOVAL || vap->va_uid != (uid_t)VNOVAL ||
	    vap->va_gid != (gid_t)VNOVAL || vap->va_atime.tv_sec != VNOVAL ||
	    vap->va_mtime.tv_sec != VNOVAL || vap->va_mode != (mode_t)VNOVAL)
		return (EROFS);

	if (vap->va_size != (u_quad_t)VNOVAL) {
		switch (vp->v_type) {
		case VDIR:
			return (EISDIR);
		case VLNK:
		case VREG:
			return (EROFS);
		case VCHR:
		case VBLK:
		case VSOCK:
		case VFIFO:
		case VNON:
		case VBAD:
		case VMARKER:
			return (0);
		}
	}
	return (0);
}

static int
hammer2_write_dirent(struct uio *uio, ino_t d_fileno, uint8_t d_type,
    uint16_t d_namlen, const char *d_name, int *errorp)
{
	struct dirent dirent;
	size_t reclen;

	reclen = _GENERIC_DIRLEN(d_namlen);
	if (reclen > uio->uio_resid)
		return (1); /* uio has no space left, end this readdir */

	dirent.d_fileno = d_fileno;
	dirent.d_off = uio->uio_offset + reclen;
	dirent.d_reclen = reclen;
	dirent.d_type = d_type;
	dirent.d_namlen = d_namlen;
	bcopy(d_name, dirent.d_name, d_namlen);
	dirent_terminate(&dirent);

	*errorp = uiomove(&dirent, reclen, uio);

	return (0); /* uio has space left */
}

static int
hammer2_readdir(struct vop_readdir_args *ap)
{
	hammer2_xop_readdir_t *xop;
	hammer2_inode_t *ip = VTOI(ap->a_vp);
	const hammer2_inode_data_t *ripdata;
	hammer2_blockref_t bref;
	hammer2_tid_t inum;
	hammer2_key_t lkey;
	struct uio *uio = ap->a_uio;
	off_t saveoff = uio->uio_offset;
	off_t *cookies;
	int ncookies, r, dtype;
	int cookie_index = 0, eofflag = 0, error = 0;
	uint16_t namlen;
	const char *dname;

	/* Setup cookies directory entry cookies if requested. */
	if (ap->a_ncookies) {
		ncookies = uio->uio_resid / 16 + 1;
		if (ncookies > 1024)
			ncookies = 1024;
		cookies = malloc(ncookies * sizeof(off_t), M_TEMP, M_WAITOK);
	} else {
		ncookies = -1;
		cookies = NULL;
	}

	hammer2_inode_lock(ip, HAMMER2_RESOLVE_SHARED);

	/*
	 * Handle artificial entries.  To ensure that only positive 64 bit
	 * quantities are returned to userland we always strip off bit 63.
	 * The hash code is designed such that codes 0x0000-0x7FFF are not
	 * used, allowing us to use these codes for articial entries.
	 *
	 * Entry 0 is used for '.' and entry 1 is used for '..'.  Do not
	 * allow '..' to cross the mount point into (e.g.) the super-root.
	 */
	if (saveoff == 0) {
		inum = ip->meta.inum & HAMMER2_DIRHASH_USERMSK;
		r = hammer2_write_dirent(uio, inum, DT_DIR, 1, ".", &error);
		if (r)
			goto done;
		if (cookies)
			cookies[cookie_index] = saveoff;
		++saveoff;
		++cookie_index;
		if (cookie_index == ncookies)
			goto done;
	}

	if (saveoff == 1) {
		inum = ip->meta.inum & HAMMER2_DIRHASH_USERMSK;
		if (ip != ip->pmp->iroot)
			inum = ip->meta.iparent & HAMMER2_DIRHASH_USERMSK;
		r = hammer2_write_dirent(uio, inum, DT_DIR, 2, "..", &error);
		if (r)
			goto done;
		if (cookies)
			cookies[cookie_index] = saveoff;
		++saveoff;
		++cookie_index;
		if (cookie_index == ncookies)
			goto done;
	}

	lkey = saveoff | HAMMER2_DIRHASH_VISIBLE;
	if (error)
		goto done;

	/* Use XOP for remaining entries. */
	xop = hammer2_xop_alloc(ip);
	xop->lkey = lkey;
	hammer2_xop_start(&xop->head, &hammer2_readdir_desc);

	for (;;) {
		error = hammer2_xop_collect(&xop->head, 0);
		error = hammer2_error_to_errno(error);
		if (error)
			break;
		if (cookie_index == ncookies)
			break;
		hammer2_cluster_bref(&xop->head.cluster, &bref);

		if (bref.type == HAMMER2_BREF_TYPE_INODE) {
			ripdata = &hammer2_xop_gdata(&xop->head)->ipdata;
			dtype = hammer2_get_dtype(ripdata->meta.type);
			saveoff = bref.key & HAMMER2_DIRHASH_USERMSK;
			r = hammer2_write_dirent(uio,
			    ripdata->meta.inum & HAMMER2_DIRHASH_USERMSK,
			    dtype, ripdata->meta.name_len, ripdata->filename,
			    &error);
			hammer2_xop_pdata(&xop->head);
			if (r)
				break;
			if (cookies)
				cookies[cookie_index] = saveoff;
			++cookie_index;
		} else if (bref.type == HAMMER2_BREF_TYPE_DIRENT) {
			dtype = hammer2_get_dtype(bref.embed.dirent.type);
			saveoff = bref.key & HAMMER2_DIRHASH_USERMSK;
			namlen = bref.embed.dirent.namlen;
			if (namlen <= sizeof(bref.check.buf))
				dname = bref.check.buf;
			else
				dname = hammer2_xop_gdata(&xop->head)->buf;
			r = hammer2_write_dirent(uio, bref.embed.dirent.inum,
			    dtype, namlen, dname, &error);
			if (namlen > sizeof(bref.check.buf))
				hammer2_xop_pdata(&xop->head);
			if (r)
				break;
			if (cookies)
				cookies[cookie_index] = saveoff;
			++cookie_index;
		} else {
			/* XXX chain error */
			hprintf("bad blockref type %d\n", bref.type);
		}
	}
	hammer2_xop_retire(&xop->head, HAMMER2_XOPMASK_VOP);
	if (error == ENOENT) {
		error = 0;
		eofflag = 1;
		saveoff = (hammer2_key_t)-1;
	} else {
		saveoff = bref.key & HAMMER2_DIRHASH_USERMSK;
	}
done:
	hammer2_inode_unlock(ip);

	if (ap->a_eofflag)
		*ap->a_eofflag = eofflag;
	uio->uio_offset = saveoff & ~HAMMER2_DIRHASH_VISIBLE;

	if (error && cookie_index == 0) {
		if (cookies) {
			free(cookies, M_TEMP);
			*ap->a_ncookies = 0;
			*ap->a_cookies = NULL;
		}
	} else {
		if (cookies) {
			*ap->a_ncookies = cookie_index;
			*ap->a_cookies = cookies;
		}
	}

	return (error);
}

/*
 * Perform read operations on a file or symlink given an unlocked
 * inode and uio.
 */
static int
hammer2_read_file(hammer2_inode_t *ip, struct uio *uio, int ioflag)
{
	struct vnode *vp = ip->vp;
	struct buf *bp;
	hammer2_off_t isize = ip->meta.size;
	hammer2_key_t lbase;
	daddr_t lbn;
	int lblksize, loff, n, seqcount = 0, error = 0;

	if (ioflag)
		seqcount = ioflag >> IO_SEQSHIFT;

	while (uio->uio_resid > 0 && uio->uio_offset < isize) {
		lblksize = hammer2_calc_logical(ip, uio->uio_offset, &lbase,
		    NULL);
		lbn = lbase / lblksize;
		bp = NULL;

		if ((lbn + 1) * lblksize >= isize)
			error = bread(ip->vp, lbn, lblksize, NOCRED, &bp);
		else if ((vp->v_mount->mnt_flag & MNT_NOCLUSTERR) == 0)
			error = cluster_read(vp, isize, lbn, lblksize, NOCRED,
			    uio->uio_resid, seqcount, 0, &bp);
		else
			error = bread(ip->vp, lbn, lblksize, NOCRED, &bp);
		if (error) {
			brelse(bp);
			bp = NULL;
			break;
		}

		loff = (int)(uio->uio_offset - lbase);
		n = lblksize - loff;
		if (n > uio->uio_resid)
			n = uio->uio_resid;
		if (n > isize - uio->uio_offset)
			n = (int)(isize - uio->uio_offset);
		error = uiomove((char *)bp->b_data + loff, n, uio);
		if (error) {
			brelse(bp);
			bp = NULL;
			break;
		}
		vfs_bio_brelse(bp, ioflag);
	}

	return (error);
}

static int
hammer2_readlink(struct vop_readlink_args *ap)
{
	struct vnode *vp = ap->a_vp;
	hammer2_inode_t *ip = VTOI(vp);

	if (vp->v_type != VLNK)
		return (EINVAL);

	return (hammer2_read_file(ip, ap->a_uio, 0));
}

static int
hammer2_read(struct vop_read_args *ap)
{
	struct vnode *vp = ap->a_vp;
	hammer2_inode_t *ip = VTOI(vp);

	if (vp->v_type == VDIR)
		return (EISDIR);
	if (vp->v_type != VREG)
		return (EINVAL);

	return (hammer2_read_file(ip, ap->a_uio, ap->a_ioflag));
}

static int
hammer2_bmap(struct vop_bmap_args *ap)
{
	hammer2_xop_bmap_t *xop;
	hammer2_dev_t *hmp;
	hammer2_inode_t *ip = VTOI(ap->a_vp);
	int error;

	hmp = ip->pmp->pfs_hmps[0];
	if (ap->a_bop != NULL)
		*ap->a_bop = &hmp->devvp->v_bufobj;
	if (ap->a_bnp == NULL)
		return (0);
	if (ap->a_runp != NULL)
		*ap->a_runp = 0; /* unsupported */
	if (ap->a_runb != NULL)
		*ap->a_runb = 0; /* unsupported */

	xop = hammer2_xop_alloc(ip);
	xop->lbn = ap->a_bn;
	hammer2_xop_start(&xop->head, &hammer2_bmap_desc);

	error = hammer2_xop_collect(&xop->head, 0);
	error = hammer2_error_to_errno(error);
	if (error) {
		/* No physical block assigned. */
		if (error == ENOENT) {
			error = 0;
			if (ap->a_bnp)
				*ap->a_bnp = -1;
		}
		goto done;
	}
	if (ap->a_bnp)
		*ap->a_bnp = xop->pbn;
done:
	hammer2_xop_retire(&xop->head, HAMMER2_XOPMASK_VOP);

	return (error);
}

static int
hammer2_nresolve(struct vop_cachedlookup_args *ap)
{
	hammer2_xop_nresolve_t *xop;
	hammer2_inode_t *ip, *dip;
	struct vnode *vp, *dvp;
	struct componentname *cnp = ap->a_cnp;
	int nameiop = cnp->cn_nameiop;
	int error;
	u_int64_t flags = cnp->cn_flags;

	KKASSERT(ap->a_vpp);
	*ap->a_vpp = NULL;

	dvp = ap->a_dvp;
	dip = VTOI(dvp);
	xop = hammer2_xop_alloc(dip);

	hammer2_xop_setname(&xop->head, cnp->cn_nameptr, cnp->cn_namelen);

	hammer2_inode_lock(dip, HAMMER2_RESOLVE_SHARED);
	hammer2_xop_start(&xop->head, &hammer2_nresolve_desc);

	error = hammer2_xop_collect(&xop->head, 0);
	error = hammer2_error_to_errno(error);
	if (error)
		ip = NULL;
	else
		ip = hammer2_inode_get(dip->pmp, &xop->head, -1, -1);
	hammer2_inode_unlock(dip);

	if (ip) {
		error = hammer2_igetv(ip, LK_EXCLUSIVE, &vp);
		if (error == 0) {
			*ap->a_vpp = vp;
			if (flags & MAKEENTRY)
				cache_enter(dvp, vp, cnp);
		} else if (error == ENOENT) {
			if (flags & MAKEENTRY)
				cache_enter(dvp, NULL, cnp);
		}
		hammer2_inode_unlock(ip);
	} else {
		if (flags & MAKEENTRY)
			cache_enter(dvp, NULL, cnp);
		if ((flags & ISLASTCN) &&
		    (nameiop == CREATE || nameiop == RENAME))
			error = EROFS;
		else
			error = ENOENT;
	}
	hammer2_xop_retire(&xop->head, HAMMER2_XOPMASK_VOP);

	return (error);
}

static int
hammer2_open(struct vop_open_args *ap)
{
	struct vnode *vp = ap->a_vp;
	hammer2_inode_t *ip = VTOI(vp);

	if (vp->v_type == VCHR || vp->v_type == VBLK)
		return (EOPNOTSUPP);

	vnode_create_vobject(vp, ip->meta.size, ap->a_td);

	return (0);
}

static int
hammer2_ioctl(struct vop_ioctl_args *ap)
{
	hammer2_inode_t *ip = VTOI(ap->a_vp);

	return (hammer2_ioctl_impl(ip, ap->a_command, ap->a_data, ap->a_fflag,
	    ap->a_cred));
}

static int
hammer2_print(struct vop_print_args *ap)
{
	struct vnode *vp = ap->a_vp;
	hammer2_inode_t *ip = VTOI(vp);
	hammer2_dev_t *hmp = ip->pmp->pfs_hmps[0];

	vn_printf(hmp->devvp, "\tino %ju", (uintmax_t)ip->meta.inum);
	if (vp->v_type == VFIFO)
		fifo_printinfo(vp);
	printf("\n");

	return (0);
}

static int
hammer2_pathconf(struct vop_pathconf_args *ap)
{
	int error = 0;

	switch (ap->a_name) {
	case _PC_LINK_MAX:
		*ap->a_retval = INT_MAX;
		break;
	case _PC_NAME_MAX:
		*ap->a_retval = HAMMER2_INODE_MAXNAME;
		break;
	case _PC_PIPE_BUF:
		if (ap->a_vp->v_type == VDIR || ap->a_vp->v_type == VFIFO)
			*ap->a_retval = PIPE_BUF;
		else
			error = EINVAL;
		break;
	case _PC_CHOWN_RESTRICTED:
		*ap->a_retval = 1;
		break;
	case _PC_NO_TRUNC:
		*ap->a_retval = 0;
		break;
	case _PC_MIN_HOLE_SIZE:
		*ap->a_retval = ap->a_vp->v_mount->mnt_stat.f_iosize;
		break;
	case _PC_PRIO_IO:
		*ap->a_retval = 0;
		break;
	case _PC_SYNC_IO:
		*ap->a_retval = 0;
		break;
	case _PC_ALLOC_SIZE_MIN:
		*ap->a_retval = ap->a_vp->v_mount->mnt_stat.f_bsize;
		break;
	case _PC_FILESIZEBITS:
		*ap->a_retval = 64;
		break;
	case _PC_REC_INCR_XFER_SIZE:
		*ap->a_retval = ap->a_vp->v_mount->mnt_stat.f_iosize;
		break;
	case _PC_REC_MAX_XFER_SIZE:
		*ap->a_retval = -1;	/* means ``unlimited'' */
		break;
	case _PC_REC_MIN_XFER_SIZE:
		*ap->a_retval = ap->a_vp->v_mount->mnt_stat.f_iosize;
		break;
	case _PC_REC_XFER_ALIGN:
		*ap->a_retval = ap->a_vp->v_mount->mnt_stat.f_bsize;
		break;
	case _PC_SYMLINK_MAX:
		*ap->a_retval = HAMMER2_INODE_MAXNAME;
		break;
	default:
		error = vop_stdpathconf(ap);
		break;
	}

	return (error);
}

static int
hammer2_vptofh(struct vop_vptofh_args *ap)
{
	hammer2_inode_t *ip = VTOI(ap->a_vp);
	struct fid *fhp;

	KKASSERT(MAXFIDSZ >= 16);

	fhp = (struct fid *)ap->a_fhp;
	fhp->fid_len = offsetof(struct fid, fid_data[16]);
	((hammer2_tid_t *)fhp->fid_data)[0] = ip->meta.inum;
	((hammer2_tid_t *)fhp->fid_data)[1] = 0;

	return (0);
}

static daddr_t
hammer2_gbp_getblkno(struct vnode *vp, vm_ooffset_t off)
{
	int lblksize = hammer2_get_logical();

	return (off / lblksize);
}

static int
hammer2_gbp_getblksz(struct vnode *vp, daddr_t lbn, long *sz)
{
	int lblksize = hammer2_get_logical();

	*sz = lblksize;

	return (0);
}

static int use_buf_pager = 1;

static int
hammer2_getpages(struct vop_getpages_args *ap)
{
	struct vnode *vp = ap->a_vp;

	if (vp->v_type == VCHR || vp->v_type == VBLK)
		return (EOPNOTSUPP);

	if (use_buf_pager)
		return (vfs_bio_getpages(vp, ap->a_m, ap->a_count,
		    ap->a_rbehind, ap->a_rahead, hammer2_gbp_getblkno,
		    hammer2_gbp_getblksz));

	KKASSERT(0);
	/* panic: vnode_pager_generic_getpages: sector size 65536 too large */
	return (vnode_pager_generic_getpages(vp, ap->a_m, ap->a_count,
	    ap->a_rbehind, ap->a_rahead, NULL, NULL));
}

struct vop_vector hammer2_vnodeops = {
	.vop_default		= &default_vnodeops,
	.vop_inactive		= hammer2_inactive,
	.vop_reclaim		= hammer2_reclaim,
	.vop_access		= hammer2_access,
	.vop_getattr		= hammer2_getattr,
	.vop_setattr		= hammer2_setattr,
	.vop_readdir		= hammer2_readdir,
	.vop_readlink		= hammer2_readlink,
	.vop_read		= hammer2_read,
	.vop_bmap		= hammer2_bmap,
	.vop_cachedlookup	= hammer2_nresolve,
	.vop_lookup		= vfs_cache_lookup,
	.vop_open		= hammer2_open,
	.vop_ioctl		= hammer2_ioctl,
	.vop_print		= hammer2_print,
	.vop_pathconf		= hammer2_pathconf,
	.vop_vptofh		= hammer2_vptofh,
	.vop_getpages		= hammer2_getpages,
	.vop_strategy		= hammer2_strategy,
};
VFS_VOP_VECTOR_REGISTER(hammer2_vnodeops);

struct vop_vector hammer2_fifoops = {
	.vop_default		= &fifo_specops,
	.vop_inactive		= hammer2_inactive,
	.vop_reclaim		= hammer2_reclaim,
	.vop_access		= hammer2_access,
	.vop_getattr		= hammer2_getattr,
	.vop_setattr		= hammer2_setattr,
	.vop_print		= hammer2_print,
	.vop_pathconf		= hammer2_pathconf,
	.vop_vptofh		= hammer2_vptofh,
};
VFS_VOP_VECTOR_REGISTER(hammer2_fifoops);
