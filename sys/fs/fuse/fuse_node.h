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
 *
 * $FreeBSD$
 */

#ifndef _FUSE_NODE_H_
#define _FUSE_NODE_H_

#include <sys/fnv_hash.h>
#include <sys/types.h>
#include <sys/mutex.h>

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

struct fuse_vnode_data {
	/** self **/
	uint64_t	nid;
	uint64_t	generation;

	/** parent **/
	uint64_t	parent_nid;

	/** I/O **/
	/* List of file handles for all of the vnode's open file descriptors */
	LIST_HEAD(, fuse_filehandle)	handles;

	/** flags **/
	uint32_t	flag;

	/** meta **/
	/* The monotonic time after which the attr cache is invalid */
	struct bintime	attr_cache_timeout;
	/* 
	 * Monotonic time after which the entry is invalid.  Used for lookups
	 * by nodeid instead of pathname.
	 */
	struct bintime	entry_cache_timeout;
	struct vattr	cached_attrs;
	uint64_t	nlookup;
	enum vtype	vtype;
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
static inline bool
fuse_vnode_attr_cache_valid(struct vnode *vp)
{
	struct bintime now;

	getbinuptime(&now);
	return (bintime_cmp(&(VTOFUD(vp)->attr_cache_timeout), &now, >));
}

static inline struct vattr*
VTOVA(struct vnode *vp)
{
	if (fuse_vnode_attr_cache_valid(vp))
		return &(VTOFUD(vp)->cached_attrs);
	else
		return NULL;
}

static inline void
fuse_vnode_clear_attr_cache(struct vnode *vp)
{
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
		MPASS(dvp->v_type == VDIR);
		VTOFUD(vp)->parent_nid = VTOI(dvp);
		VTOFUD(vp)->flag |= FN_PARENT_NID;
	} else {
		VTOFUD(vp)->flag &= ~FN_PARENT_NID;
	}
}

int fuse_vnode_size(struct vnode *vp, off_t *filesize, struct ucred *cred,
	struct thread *td);

void fuse_vnode_destroy(struct vnode *vp);

int fuse_vnode_get(struct mount *mp, struct fuse_entry_out *feo,
    uint64_t nodeid, struct vnode *dvp, struct vnode **vpp,
    struct componentname *cnp, enum vtype vtyp);

void fuse_vnode_open(struct vnode *vp, int32_t fuse_open_flags,
    struct thread *td);

int fuse_vnode_savesize(struct vnode *vp, struct ucred *cred, pid_t pid);

int fuse_vnode_setsize(struct vnode *vp, off_t newsize, bool from_server);

void fuse_vnode_undirty_cached_timestamps(struct vnode *vp);

void fuse_vnode_update(struct vnode *vp, int flags);

void fuse_node_init(void);
void fuse_node_destroy(void);
#endif /* _FUSE_NODE_H_ */
