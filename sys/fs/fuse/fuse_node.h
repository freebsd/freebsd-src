/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2007-2009 Google Inc. and Amit Singh
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of Google Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * Copyright (C) 2005 Csaba Henk.
 * All rights reserved.
 *
 * Copyright (c) 2019 The FreeBSD Foundation
 *
 * Portions of this software were developed by BFF Storage Systems, LLC under
 * sponsorship from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _FUSE_NODE_H_
#define _FUSE_NODE_H_

#include <sys/fnv_hash.h>
#include <sys/types.h>
#include <sys/mutex.h>
#include <sys/buf.h>

#include "fuse_file.h"

#define	FN_REVOKED		0x00000020
#define	FN_FLUSHINPROG		0x00000040
#define	FN_FLUSHWANT		0x00000080
/* 
 * Indicates that the file's size is dirty; the kernel has changed it but not
 * yet send the change to the daemon.  When this bit is set, the
 * cache_attrs.va_size field does not time out.
 */
#define	FN_SIZECHANGE		0x00000100
/*
 * Whether I/O to this vnode should bypass the cache.
 * XXX BUG: this should be part of the file handle, not the vnode data.
 * https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=293088
 */
#define	FN_DIRECTIO		0x00000200
/* Indicates that parent_nid is valid */
#define	FN_PARENT_NID		0x00000400

/* 
 * Indicates that the file's cached timestamps are dirty.  They will be flushed
 * during the next SETATTR or WRITE.  Until then, the cached fields will not
 * time out.
 */
#define	FN_MTIMECHANGE		0x00000800
#define	FN_CTIMECHANGE		0x00001000
#define	FN_ATIMECHANGE		0x00002000

/* vop_delayed_setsize should truncate the file */
#define FN_DELAYED_TRUNCATE	0x00004000

#define CACHED_ATTR_LOCK(vp)				\
do {							\
	ASSERT_VOP_LOCKED(vp, __func__);		\
	if (VOP_ISLOCKED(vp) != LK_EXCLUSIVE)		\
		mtx_lock(&VTOFUD(vp)->cached_attr_mtx);	\
} while(0)

#define CACHED_ATTR_UNLOCK(vp)					\
do {								\
	ASSERT_VOP_LOCKED(vp, __func__);			\
	if (VOP_ISLOCKED(vp) != LK_EXCLUSIVE)			\
		mtx_unlock(&VTOFUD(vp)->cached_attr_mtx);	\
} while(0)

struct fuse_vnode_data {
	/* self's node id, similar to an inode number. Immutable. */
	uint64_t	nid;
	/*
	 * Generation number.  Distinguishes files with same nid but that don't
	 * overlap in time.  Immutable.
	 */
	uint64_t	generation;

	/* parent's node id.  Protected by the vnode lock. */
	uint64_t	parent_nid;

	/** I/O **/
	/*
	 * List of file handles for all of the vnode's open file descriptors.
	 * Protected by the vnode lock.
	 */
	LIST_HEAD(, fuse_filehandle)	handles;

	/* Protects flag, attr_cache_timeout and cached_attrs */
	struct mtx	cached_attr_mtx;

	/*
	 * The monotonic time after which the attr cache is invalid
	 * Protected by an exclusive vnode lock or the cached_attr_mtx
	 */
	struct bintime	attr_cache_timeout;

	/*
	 * Monotonic time after which the entry is invalid.  Used for lookups
	 * by nodeid instead of pathname.  Protected by the vnode lock.
	 */
	struct bintime	entry_cache_timeout;

	/*
	 * Monotonic time of the last FUSE operation that modified the file
	 * size.  Used to avoid races between mutator ops like VOP_SETATTR and
	 * unlocked accessor ops like VOP_LOOKUP.  Protected by the vnode lock.
	 */
	struct timespec	last_local_modify;

	/* Protected by an exclusive vnode lock or the cached_attr_mtx */
	struct vattr	cached_attrs;

	/* Number of FUSE_LOOKUPs minus FUSE_FORGETs. Protected by vnode lock */
	uint64_t	nlookup;

	/*
	 * Misc flags.  Protected by an exclusive vnode lock or the
	 * cached_attr_mtx, because some of the flags reflect the contents of
	 * cached_attrs.
	 */
	uint32_t	flag;

	/* Vnode type.  Immutable */
	__enum_uint8(vtype)	vtype;

	/* State for clustered writes.  Protected by vnode lock */
	struct vn_clusterw clusterw;
};

/*
 * This overlays the fid structure (see mount.h). Mostly the same as the types
 * used by UFS and ext2.
 */
struct fuse_fid {
	uint16_t	len;	/* Length of structure. */
	uint16_t	pad;	/* Force 32-bit alignment. */
	uint32_t	gen;	/* Generation number. */
	uint64_t	nid;	/* FUSE node id. */
};

#define VTOFUD(vp) \
	((struct fuse_vnode_data *)((vp)->v_data))
#define VTOI(vp)    (VTOFUD(vp)->nid)

#define ASSERT_CACHED_ATTRS_LOCKED(vp)				\
do {								\
	ASSERT_VOP_LOCKED(vp, __func__);			\
	VNASSERT(VOP_ISLOCKED(vp) == LK_EXCLUSIVE ||		\
		mtx_owned(&VTOFUD(vp)->cached_attr_mtx), vp,	\
		("cached attrs not locked"));			\
} while(0)

static inline bool
fuse_vnode_attr_cache_valid(struct vnode *vp)
{
	struct fuse_vnode_data *fvdat = VTOFUD(vp);
	struct bintime now;

	ASSERT_CACHED_ATTRS_LOCKED(vp);

	getbinuptime(&now);
	return (bintime_cmp(&fvdat->attr_cache_timeout, &now, >));
}

static inline struct vattr*
VTOVA(struct vnode *vp)
{
	ASSERT_CACHED_ATTRS_LOCKED(vp);

	if (fuse_vnode_attr_cache_valid(vp))
		return &(VTOFUD(vp)->cached_attrs);
	else
		return NULL;
}

static inline void
fuse_vnode_clear_attr_cache(struct vnode *vp)
{
	ASSERT_CACHED_ATTRS_LOCKED(vp);

	bintime_clear(&VTOFUD(vp)->attr_cache_timeout);
}

static uint32_t inline
fuse_vnode_hash(uint64_t id)
{
	return (fnv_32_buf(&id, sizeof(id), FNV1_32_INIT));
}

#define VTOILLU(vp) ((uint64_t)(VTOFUD(vp) ? VTOI(vp) : 0))

#define FUSE_NULL_ID 0

extern struct vop_vector fuse_fifoops;
extern struct vop_vector fuse_vnops;

int fuse_vnode_cmp(struct vnode *vp, void *nidp);

static inline void
fuse_vnode_setparent(struct vnode *vp, struct vnode *dvp)
{
	if (dvp != NULL && vp->v_type == VDIR) {
		ASSERT_VOP_ELOCKED(vp, __func__); /* for parent_nid */

		MPASS(dvp->v_type == VDIR);
		VTOFUD(vp)->parent_nid = VTOI(dvp);
		VTOFUD(vp)->flag |= FN_PARENT_NID;
	} else {
		ASSERT_CACHED_ATTRS_LOCKED(vp);

		VTOFUD(vp)->flag &= ~FN_PARENT_NID;
	}
}

int fuse_vnode_size(struct vnode *vp, off_t *filesize, struct ucred *cred,
	struct thread *td);

void fuse_vnode_destroy(struct vnode *vp);

int fuse_vnode_get(struct mount *mp, struct fuse_entry_out *feo,
    uint64_t nodeid, struct vnode *dvp, struct vnode **vpp,
    struct componentname *cnp, __enum_uint8(vtype) vtyp);

void fuse_vnode_open(struct vnode *vp, int32_t fuse_open_flags,
    struct thread *td);

int fuse_vnode_savesize(struct vnode *vp, struct ucred *cred, pid_t pid);

int fuse_vnode_setsize(struct vnode *vp, off_t newsize, bool from_server);
int fuse_vnode_setsize_immediate(struct vnode *vp, bool shrink);

void fuse_vnode_undirty_cached_timestamps(struct vnode *vp, bool atime);

void fuse_vnode_update(struct vnode *vp, int flags);

void fuse_node_init(void);
void fuse_node_destroy(void);
#endif /* _FUSE_NODE_H_ */
