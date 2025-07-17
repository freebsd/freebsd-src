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
#include "opt_zstdio.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#if defined(ZSTDIO)
#define TARFS_ZIO 1
#else
#undef TARFS_ZIO
#endif

#ifdef ZSTDIO
#define ZSTD_STATIC_LINKING_ONLY
#include <contrib/zstd/lib/zstd.h>
#endif

#include <fs/tarfs/tarfs.h>
#include <fs/tarfs/tarfs_dbg.h>

#ifdef TARFS_DEBUG
SYSCTL_NODE(_vfs_tarfs, OID_AUTO, zio, CTLFLAG_RD, 0,
    "Tar filesystem decompression layer");
COUNTER_U64_DEFINE_EARLY(tarfs_zio_inflated);
SYSCTL_COUNTER_U64(_vfs_tarfs_zio, OID_AUTO, inflated, CTLFLAG_RD,
    &tarfs_zio_inflated, "Amount of compressed data inflated.");
COUNTER_U64_DEFINE_EARLY(tarfs_zio_consumed);
SYSCTL_COUNTER_U64(_vfs_tarfs_zio, OID_AUTO, consumed, CTLFLAG_RD,
    &tarfs_zio_consumed, "Amount of compressed data consumed.");
COUNTER_U64_DEFINE_EARLY(tarfs_zio_bounced);
SYSCTL_COUNTER_U64(_vfs_tarfs_zio, OID_AUTO, bounced, CTLFLAG_RD,
    &tarfs_zio_bounced, "Amount of decompressed data bounced.");

static int
tarfs_sysctl_handle_zio_reset(SYSCTL_HANDLER_ARGS)
{
	unsigned int tmp;
	int error;

	tmp = 0;
	if ((error = SYSCTL_OUT(req, &tmp, sizeof(tmp))) != 0)
		return (error);
	if (req->newptr != NULL) {
		if ((error = SYSCTL_IN(req, &tmp, sizeof(tmp))) != 0)
			return (error);
		counter_u64_zero(tarfs_zio_inflated);
		counter_u64_zero(tarfs_zio_consumed);
		counter_u64_zero(tarfs_zio_bounced);
	}
	return (0);
}

SYSCTL_PROC(_vfs_tarfs_zio, OID_AUTO, reset,
    CTLTYPE_INT | CTLFLAG_MPSAFE | CTLFLAG_RW,
    NULL, 0, tarfs_sysctl_handle_zio_reset, "IU",
    "Reset compression counters.");
#endif

MALLOC_DEFINE(M_TARFSZSTATE, "tarfs zstate", "tarfs decompression state");
MALLOC_DEFINE(M_TARFSZBUF, "tarfs zbuf", "tarfs decompression buffers");

#define XZ_MAGIC		(uint8_t[]){ 0xfd, 0x37, 0x7a, 0x58, 0x5a }
#define ZLIB_MAGIC		(uint8_t[]){ 0x1f, 0x8b, 0x08 }
#define ZSTD_MAGIC		(uint8_t[]){ 0x28, 0xb5, 0x2f, 0xfd }

#ifdef ZSTDIO
struct tarfs_zstd {
	ZSTD_DStream *zds;
};
#endif

/* XXX review use of curthread / uio_td / td_cred */

/*
 * Reads from the tar file according to the provided uio.  If the archive
 * is compressed and raw is false, reads the decompressed stream;
 * otherwise, reads directly from the original file.  Returns 0 on success
 * and a positive errno value on failure.
 */
int
tarfs_io_read(struct tarfs_mount *tmp, bool raw, struct uio *uiop)
{
	void *rl = NULL;
	off_t off = uiop->uio_offset;
	size_t len = uiop->uio_resid;
	int error;

	if (raw || tmp->znode == NULL) {
		rl = vn_rangelock_rlock(tmp->vp, off, off + len);
		error = vn_lock(tmp->vp, LK_SHARED);
		if (error == 0) {
			error = VOP_READ(tmp->vp, uiop, IO_NODELOCKED,
			    uiop->uio_td->td_ucred);
			VOP_UNLOCK(tmp->vp);
		}
		vn_rangelock_unlock(tmp->vp, rl);
	} else {
		error = vn_lock(tmp->znode, LK_EXCLUSIVE);
		if (error == 0) {
			error = VOP_READ(tmp->znode, uiop,
			    IO_DIRECT | IO_NODELOCKED,
			    uiop->uio_td->td_ucred);
			VOP_UNLOCK(tmp->znode);
		}
	}
	TARFS_DPF(IO, "%s(%zu, %zu) = %d (resid %zd)\n", __func__,
	    (size_t)off, len, error, uiop->uio_resid);
	return (error);
}

/*
 * Reads from the tar file into the provided buffer.  If the archive is
 * compressed and raw is false, reads the decompressed stream; otherwise,
 * reads directly from the original file.  Returns the number of bytes
 * read on success, 0 on EOF, and a negative errno value on failure.
 */
ssize_t
tarfs_io_read_buf(struct tarfs_mount *tmp, bool raw,
    void *buf, off_t off, size_t len)
{
	struct uio auio;
	struct iovec aiov;
	ssize_t res;
	int error;

	if (len == 0) {
		TARFS_DPF(IO, "%s(%zu, %zu) null\n", __func__,
		    (size_t)off, len);
		return (0);
	}
	aiov.iov_base = buf;
	aiov.iov_len = len;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = off;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_resid = len;
	auio.uio_td = curthread;
	error = tarfs_io_read(tmp, raw, &auio);
	if (error != 0) {
		TARFS_DPF(IO, "%s(%zu, %zu) error %d\n", __func__,
		    (size_t)off, len, error);
		return (-error);
	}
	res = len - auio.uio_resid;
	if (res == 0 && len != 0) {
		TARFS_DPF(IO, "%s(%zu, %zu) eof\n", __func__,
		    (size_t)off, len);
	} else {
		TARFS_DPF(IO, "%s(%zu, %zu) read %zd | %*D\n", __func__,
		    (size_t)off, len, res,
		    (int)(res > 8 ? 8 : res), (uint8_t *)buf, " ");
	}
	return (res);
}

#ifdef ZSTDIO
static void *
tarfs_zstate_alloc(void *opaque, size_t size)
{

	(void)opaque;
	return (malloc(size, M_TARFSZSTATE, M_WAITOK));
}
#endif

#ifdef ZSTDIO
static void
tarfs_zstate_free(void *opaque, void *address)
{

	(void)opaque;
	free(address, M_TARFSZSTATE);
}
#endif

#ifdef ZSTDIO
static ZSTD_customMem tarfs_zstd_mem = {
	tarfs_zstate_alloc,
	tarfs_zstate_free,
	NULL,
};
#endif

#ifdef TARFS_ZIO
/*
 * Updates the decompression frame index, recording the current input and
 * output offsets in a new index entry, and growing the index if
 * necessary.
 */
static void
tarfs_zio_update_index(struct tarfs_zio *zio, off_t i, off_t o)
{

	if (++zio->curidx >= zio->nidx) {
		if (++zio->nidx > zio->szidx) {
			zio->szidx *= 2;
			zio->idx = realloc(zio->idx,
			    zio->szidx * sizeof(*zio->idx),
			    M_TARFSZSTATE, M_ZERO | M_WAITOK);
			TARFS_DPF(ALLOC, "%s: resized zio index\n", __func__);
		}
		zio->idx[zio->curidx].i = i;
		zio->idx[zio->curidx].o = o;
		TARFS_DPF(ZIDX, "%s: index %u = i %zu o %zu\n", __func__,
		    zio->curidx, (size_t)zio->idx[zio->curidx].i,
		    (size_t)zio->idx[zio->curidx].o);
	}
	MPASS(zio->idx[zio->curidx].i == i);
	MPASS(zio->idx[zio->curidx].o == o);
}
#endif

/*
 * VOP_ACCESS for zio node.
 */
static int
tarfs_zaccess(struct vop_access_args *ap)
{
	struct vnode *vp = ap->a_vp;
	struct tarfs_zio *zio = vp->v_data;
	struct tarfs_mount *tmp = zio->tmp;
	accmode_t accmode = ap->a_accmode;
	int error = EPERM;

	if (accmode == VREAD) {
		error = vn_lock(tmp->vp, LK_SHARED);
		if (error == 0) {
			error = VOP_ACCESS(tmp->vp, accmode, ap->a_cred, ap->a_td);
			VOP_UNLOCK(tmp->vp);
		}
	}
	TARFS_DPF(ZIO, "%s(%d) = %d\n", __func__, accmode, error);
	return (error);
}

/*
 * VOP_GETATTR for zio node.
 */
static int
tarfs_zgetattr(struct vop_getattr_args *ap)
{
	struct vattr va;
	struct vnode *vp = ap->a_vp;
	struct tarfs_zio *zio = vp->v_data;
	struct tarfs_mount *tmp = zio->tmp;
	struct vattr *vap = ap->a_vap;
	int error = 0;

	VATTR_NULL(vap);
	error = vn_lock(tmp->vp, LK_SHARED);
	if (error == 0) {
		error = VOP_GETATTR(tmp->vp, &va, ap->a_cred);
		VOP_UNLOCK(tmp->vp);
		if (error == 0) {
			vap->va_type = VREG;
			vap->va_mode = va.va_mode;
			vap->va_nlink = 1;
			vap->va_gid = va.va_gid;
			vap->va_uid = va.va_uid;
			vap->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
			vap->va_fileid = TARFS_ZIOINO;
			vap->va_size = zio->idx[zio->nidx - 1].o;
			vap->va_blocksize = vp->v_mount->mnt_stat.f_iosize;
			vap->va_atime = va.va_atime;
			vap->va_ctime = va.va_ctime;
			vap->va_mtime = va.va_mtime;
			vap->va_birthtime = tmp->root->birthtime;
			vap->va_bytes = va.va_bytes;
		}
	}
	TARFS_DPF(ZIO, "%s() = %d\n", __func__, error);
	return (error);
}

#ifdef ZSTDIO
/*
 * VOP_READ for zio node, zstd edition.
 */
static int
tarfs_zread_zstd(struct tarfs_zio *zio, struct uio *uiop)
{
	void *ibuf = NULL, *obuf = NULL, *rl = NULL;
	struct uio auio;
	struct iovec aiov;
	struct tarfs_mount *tmp = zio->tmp;
	struct tarfs_zstd *zstd = zio->zstd;
	struct thread *td = curthread;
	ZSTD_inBuffer zib;
	ZSTD_outBuffer zob;
	off_t zsize;
	off_t ipos, opos;
	size_t ilen, olen;
	size_t zerror;
	off_t off = uiop->uio_offset;
	size_t len = uiop->uio_resid;
	size_t resid = uiop->uio_resid;
	size_t bsize;
	int error;
	bool reset = false;

	/* do we have to rewind? */
	if (off < zio->opos) {
		while (zio->curidx > 0 && off < zio->idx[zio->curidx].o)
			zio->curidx--;
		reset = true;
	}
	/* advance to the nearest index entry */
	if (off > zio->opos) {
		// XXX maybe do a binary search instead
		while (zio->curidx < zio->nidx - 1 &&
		    off >= zio->idx[zio->curidx + 1].o) {
			zio->curidx++;
			reset = true;
		}
	}
	/* reset the decompression stream if needed */
	if (reset) {
		zio->ipos = zio->idx[zio->curidx].i;
		zio->opos = zio->idx[zio->curidx].o;
		ZSTD_resetDStream(zstd->zds);
		TARFS_DPF(ZIDX, "%s: skipping to index %u = i %zu o %zu\n", __func__,
		    zio->curidx, (size_t)zio->ipos, (size_t)zio->opos);
	} else {
		TARFS_DPF(ZIDX, "%s: continuing at i %zu o %zu\n", __func__,
		    (size_t)zio->ipos, (size_t)zio->opos);
	}

	/*
	 * Set up a temporary buffer for compressed data.  Use the size
	 * recommended by the zstd library; this is usually 128 kB, but
	 * just in case, make sure it's a multiple of the page size and no
	 * larger than MAXBSIZE.
	 */
	bsize = roundup(ZSTD_CStreamOutSize(), PAGE_SIZE);
	if (bsize > MAXBSIZE)
		bsize = MAXBSIZE;
	ibuf = malloc(bsize, M_TEMP, M_WAITOK);
	zib.src = NULL;
	zib.size = 0;
	zib.pos = 0;

	/*
	 * Set up the decompression buffer.  If the target is not in
	 * kernel space, we will have to set up a bounce buffer.
	 *
	 * TODO: to avoid using a bounce buffer, map destination pages
	 * using vm_fault_quick_hold_pages().
	 */
	MPASS(zio->opos <= off);
	MPASS(uiop->uio_iovcnt == 1);
	MPASS(uiop->uio_iov->iov_len >= len);
	if (uiop->uio_segflg == UIO_SYSSPACE) {
		zob.dst = uiop->uio_iov->iov_base;
	} else {
		TARFS_DPF(BOUNCE, "%s: allocating %zu-byte bounce buffer\n",
		    __func__, len);
		zob.dst = obuf = malloc(len, M_TEMP, M_WAITOK);
	}
	zob.size = len;
	zob.pos = 0;

	/* lock tarball */
	rl = vn_rangelock_rlock(tmp->vp, zio->ipos, OFF_MAX);
	error = vn_lock(tmp->vp, LK_SHARED);
	if (error != 0) {
		goto fail_unlocked;
	}
	/* check size */
	error = vn_getsize_locked(tmp->vp, &zsize, td->td_ucred);
	if (error != 0) {
		goto fail;
	}
	if (zio->ipos >= zsize) {
		/* beyond EOF */
		goto fail;
	}

	while (resid > 0) {
		if (zib.pos == zib.size) {
			/* request data from the underlying file */
			aiov.iov_base = ibuf;
			aiov.iov_len = bsize;
			auio.uio_iov = &aiov;
			auio.uio_iovcnt = 1;
			auio.uio_offset = zio->ipos;
			auio.uio_segflg = UIO_SYSSPACE;
			auio.uio_rw = UIO_READ;
			auio.uio_resid = aiov.iov_len;
			auio.uio_td = td;
			error = VOP_READ(tmp->vp, &auio, IO_NODELOCKED,
			    td->td_ucred);
			if (error != 0)
				goto fail;
			TARFS_DPF(ZIO, "%s: req %zu+%zu got %zu+%zu\n", __func__,
			    (size_t)zio->ipos, bsize,
			    (size_t)zio->ipos, bsize - auio.uio_resid);
			zib.src = ibuf;
			zib.size = bsize - auio.uio_resid;
			zib.pos = 0;
		}
		MPASS(zib.pos <= zib.size);
		if (zib.pos == zib.size) {
			TARFS_DPF(ZIO, "%s: end of file after i %zu o %zu\n", __func__,
			    (size_t)zio->ipos, (size_t)zio->opos);
			goto fail;
		}
		if (zio->opos < off) {
			/* to be discarded */
			zob.size = min(off - zio->opos, len);
			zob.pos = 0;
		} else {
			zob.size = len;
			zob.pos = zio->opos - off;
		}
		ipos = zib.pos;
		opos = zob.pos;
		/* decompress as much as possible */
		zerror = ZSTD_decompressStream(zstd->zds, &zob, &zib);
		zio->ipos += ilen = zib.pos - ipos;
		zio->opos += olen = zob.pos - opos;
		if (zio->opos > off)
			resid -= olen;
		if (ZSTD_isError(zerror)) {
			TARFS_DPF(ZIO, "%s: inflate failed after i %zu o %zu: %s\n", __func__,
			    (size_t)zio->ipos, (size_t)zio->opos, ZSTD_getErrorName(zerror));
			error = EIO;
			goto fail;
		}
		if (zerror == 0 && olen == 0) {
			TARFS_DPF(ZIO, "%s: end of stream after i %zu o %zu\n", __func__,
			    (size_t)zio->ipos, (size_t)zio->opos);
			break;
		}
		if (zerror == 0) {
			TARFS_DPF(ZIO, "%s: end of frame after i %zu o %zu\n", __func__,
			    (size_t)zio->ipos, (size_t)zio->opos);
			tarfs_zio_update_index(zio, zio->ipos, zio->opos);
		}
		TARFS_DPF(ZIO, "%s: inflated %zu\n", __func__, olen);
#ifdef TARFS_DEBUG
		counter_u64_add(tarfs_zio_inflated, olen);
#endif
	}
fail:
	VOP_UNLOCK(tmp->vp);
fail_unlocked:
	if (error == 0) {
		if (uiop->uio_segflg == UIO_SYSSPACE) {
			uiop->uio_resid = resid;
		} else if (len > resid) {
			TARFS_DPF(BOUNCE, "%s: bounced %zu bytes\n", __func__,
			    len - resid);
			error = uiomove(obuf, len - resid, uiop);
#ifdef TARFS_DEBUG
			counter_u64_add(tarfs_zio_bounced, len - resid);
#endif
		}
	}
	if (obuf != NULL) {
		TARFS_DPF(BOUNCE, "%s: freeing bounce buffer\n", __func__);
		free(obuf, M_TEMP);
	}
	if (rl != NULL)
		vn_rangelock_unlock(tmp->vp, rl);
	if (ibuf != NULL)
		free(ibuf, M_TEMP);
	TARFS_DPF(ZIO, "%s(%zu, %zu) = %d (resid %zd)\n", __func__,
	    (size_t)off, len, error, uiop->uio_resid);
#ifdef TARFS_DEBUG
	counter_u64_add(tarfs_zio_consumed, len - uiop->uio_resid);
#endif
	if (error != 0) {
		zio->curidx = 0;
		zio->ipos = zio->idx[0].i;
		zio->opos = zio->idx[0].o;
		ZSTD_resetDStream(zstd->zds);
	}
	return (error);
}
#endif

/*
 * VOP_READ for zio node.
 */
static int
tarfs_zread(struct vop_read_args *ap)
{
#if defined(TARFS_DEBUG) || defined(ZSTDIO)
	struct vnode *vp = ap->a_vp;
	struct tarfs_zio *zio = vp->v_data;
	struct uio *uiop = ap->a_uio;
#endif
#ifdef TARFS_DEBUG
	off_t off = uiop->uio_offset;
	size_t len = uiop->uio_resid;
#endif
	int error;

	TARFS_DPF(ZIO, "%s(%zu, %zu)\n", __func__,
	    (size_t)off, len);
#ifdef ZSTDIO
	if (zio->zstd != NULL) {
		error = tarfs_zread_zstd(zio, uiop);
	} else
#endif
		error = EFTYPE;
	TARFS_DPF(ZIO, "%s(%zu, %zu) = %d (resid %zd)\n", __func__,
	    (size_t)off, len, error, uiop->uio_resid);
	return (error);
}

/*
 * VOP_RECLAIM for zio node.
 */
static int
tarfs_zreclaim(struct vop_reclaim_args *ap)
{
	struct vnode *vp = ap->a_vp;

	TARFS_DPF(ZIO, "%s(%p)\n", __func__, vp);
	vp->v_data = NULL;
	return (0);
}

/*
 * VOP_STRATEGY for zio node.
 */
static int
tarfs_zstrategy(struct vop_strategy_args *ap)
{
	struct uio auio;
	struct iovec iov;
	struct vnode *vp = ap->a_vp;
	struct buf *bp = ap->a_bp;
	off_t off;
	size_t len;
	int error;

	iov.iov_base = bp->b_data;
	iov.iov_len = bp->b_bcount;
	off = bp->b_iooffset;
	len = bp->b_bcount;
	bp->b_resid = len;
	auio.uio_iov = &iov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = off;
	auio.uio_resid = len;
	auio.uio_segflg = UIO_SYSSPACE;
	auio.uio_rw = UIO_READ;
	auio.uio_td = curthread;
	error = VOP_READ(vp, &auio, IO_DIRECT | IO_NODELOCKED, bp->b_rcred);
	bp->b_flags |= B_DONE;
	if (error != 0) {
		bp->b_ioflags |= BIO_ERROR;
		bp->b_error = error;
	}
	return (0);
}

static struct vop_vector tarfs_znodeops = {
	.vop_default =		&default_vnodeops,

	.vop_access =		tarfs_zaccess,
	.vop_getattr =		tarfs_zgetattr,
	.vop_read =		tarfs_zread,
	.vop_reclaim =		tarfs_zreclaim,
	.vop_strategy =		tarfs_zstrategy,
};
VFS_VOP_VECTOR_REGISTER(tarfs_znodeops);

#ifdef TARFS_ZIO
/*
 * Initializes the decompression layer.
 */
static struct tarfs_zio *
tarfs_zio_init(struct tarfs_mount *tmp, off_t i, off_t o)
{
	struct tarfs_zio *zio;
	struct vnode *zvp;

	zio = malloc(sizeof(*zio), M_TARFSZSTATE, M_ZERO | M_WAITOK);
	TARFS_DPF(ALLOC, "%s: allocated zio\n", __func__);
	zio->tmp = tmp;
	zio->szidx = 128;
	zio->idx = malloc(zio->szidx * sizeof(*zio->idx), M_TARFSZSTATE,
	    M_ZERO | M_WAITOK);
	zio->curidx = 0;
	zio->nidx = 1;
	zio->idx[zio->curidx].i = zio->ipos = i;
	zio->idx[zio->curidx].o = zio->opos = o;
	tmp->zio = zio;
	TARFS_DPF(ALLOC, "%s: allocated zio index\n", __func__);
	(void)getnewvnode("tarfsz", tmp->vfs, &tarfs_znodeops, &zvp);
	zvp->v_data = zio;
	zvp->v_type = VREG;
	zvp->v_mount = tmp->vfs;
	vn_set_state(zvp, VSTATE_CONSTRUCTED);
	tmp->znode = zvp;
	TARFS_DPF(ZIO, "%s: created zio node\n", __func__);
	return (zio);
}
#endif

/*
 * Initializes the I/O layer, including decompression if the signature of
 * a supported compression format is detected.  Returns 0 on success and a
 * positive errno value on failure.
 */
int
tarfs_io_init(struct tarfs_mount *tmp)
{
	uint8_t *block;
#ifdef TARFS_ZIO
	struct tarfs_zio *zio = NULL;
#endif
	ssize_t res;
	int error = 0;

	block = malloc(tmp->iosize, M_TEMP, M_ZERO | M_WAITOK);
	res = tarfs_io_read_buf(tmp, true, block, 0, tmp->iosize);
	if (res < 0) {
		return (-res);
	}
	if (memcmp(block, XZ_MAGIC, sizeof(XZ_MAGIC)) == 0) {
		printf("xz compression not supported\n");
		error = EOPNOTSUPP;
		goto bad;
	} else if (memcmp(block, ZLIB_MAGIC, sizeof(ZLIB_MAGIC)) == 0) {
		printf("zlib compression not supported\n");
		error = EOPNOTSUPP;
		goto bad;
	} else if (memcmp(block, ZSTD_MAGIC, sizeof(ZSTD_MAGIC)) == 0) {
#ifdef ZSTDIO
		zio = tarfs_zio_init(tmp, 0, 0);
		zio->zstd = malloc(sizeof(*zio->zstd), M_TARFSZSTATE, M_WAITOK);
		zio->zstd->zds = ZSTD_createDStream_advanced(tarfs_zstd_mem);
		(void)ZSTD_initDStream(zio->zstd->zds);
#else
		printf("zstd compression not supported\n");
		error = EOPNOTSUPP;
		goto bad;
#endif
	}
bad:
	free(block, M_TEMP);
	return (error);
}

#ifdef TARFS_ZIO
/*
 * Tears down the decompression layer.
 */
static int
tarfs_zio_fini(struct tarfs_mount *tmp)
{
	struct tarfs_zio *zio = tmp->zio;
	int error = 0;

	if (tmp->znode != NULL) {
		error = vn_lock(tmp->znode, LK_EXCLUSIVE);
		if (error != 0) {
			TARFS_DPF(ALLOC, "%s: failed to lock znode", __func__);
			return (error);
		}
		tmp->znode->v_mount = NULL;
		vgone(tmp->znode);
		vput(tmp->znode);
		tmp->znode = NULL;
	}
#ifdef ZSTDIO
	if (zio->zstd != NULL) {
		TARFS_DPF(ALLOC, "%s: freeing zstd state\n", __func__);
		ZSTD_freeDStream(zio->zstd->zds);
		free(zio->zstd, M_TARFSZSTATE);
	}
#endif
	if (zio->idx != NULL) {
		TARFS_DPF(ALLOC, "%s: freeing index\n", __func__);
		free(zio->idx, M_TARFSZSTATE);
	}
	TARFS_DPF(ALLOC, "%s: freeing zio\n", __func__);
	free(zio, M_TARFSZSTATE);
	tmp->zio = NULL;
	return (error);
}
#endif

/*
 * Tears down the I/O layer, including the decompression layer if
 * applicable.
 */
int
tarfs_io_fini(struct tarfs_mount *tmp)
{
	int error = 0;

#ifdef TARFS_ZIO
	if (tmp->zio != NULL) {
		error = tarfs_zio_fini(tmp);
	}
#endif
	return (error);
}
