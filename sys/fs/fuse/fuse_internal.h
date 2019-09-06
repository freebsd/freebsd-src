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

#ifndef _FUSE_INTERNAL_H_
#define _FUSE_INTERNAL_H_

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/vnode.h>

#include "fuse_ipc.h"
#include "fuse_node.h"

static inline bool
vfs_isrdonly(struct mount *mp)
{
	return ((mp->mnt_flag & MNT_RDONLY) != 0);
}

static inline struct mount *
vnode_mount(struct vnode *vp)
{
	return (vp->v_mount);
}

static inline bool
vnode_mountedhere(struct vnode *vp)
{
	return (vp->v_mountedhere != NULL);
}

static inline enum vtype
vnode_vtype(struct vnode *vp)
{
	return (vp->v_type);
}

static inline bool
vnode_isvroot(struct vnode *vp)
{
	return ((vp->v_vflag & VV_ROOT) != 0);
}

static inline bool
vnode_isreg(struct vnode *vp)
{
	return (vp->v_type == VREG);
}

static inline bool
vnode_isdir(struct vnode *vp)
{
	return (vp->v_type == VDIR);
}

static inline bool
vnode_islnk(struct vnode *vp)
{
	return (vp->v_type == VLNK);
}

static inline ssize_t
uio_resid(struct uio *uio)
{
	return (uio->uio_resid);
}

static inline off_t
uio_offset(struct uio *uio)
{
	return (uio->uio_offset);
}

static inline void
uio_setoffset(struct uio *uio, off_t offset)
{
	uio->uio_offset = offset;
}

static inline void
uio_setresid(struct uio *uio, ssize_t resid)
{
	uio->uio_resid = resid;
}

/* miscellaneous */

static inline bool
fuse_isdeadfs(struct vnode *vp)
{
	struct fuse_data *data = fuse_get_mpdata(vnode_mount(vp));

	return (data->dataflags & FSESS_DEAD);
}

static inline uint64_t
fuse_iosize(struct vnode *vp)
{
	return (vp->v_mount->mnt_stat.f_iosize);
}

/* access */

#define FVP_ACCESS_NOOP		0x01

#define FACCESS_VA_VALID	0x01
#define FACCESS_DO_ACCESS	0x02
#define FACCESS_STICKY		0x04
#define FACCESS_CHOWN		0x08
#define FACCESS_NOCHECKSPY	0x10
#define FACCESS_SETGID		0x12

#define FACCESS_XQUERIES	(FACCESS_STICKY | FACCESS_CHOWN | FACCESS_SETGID)

struct fuse_access_param {
	uid_t		xuid;
	gid_t		xgid;
	uint32_t	facc_flags;
};

static inline int
fuse_match_cred(struct ucred *basecred, struct ucred *usercred)
{
	if (basecred->cr_uid == usercred->cr_uid             &&
	    basecred->cr_uid == usercred->cr_ruid            &&
	    basecred->cr_uid == usercred->cr_svuid           &&
	    basecred->cr_groups[0] == usercred->cr_groups[0] &&
	    basecred->cr_groups[0] == usercred->cr_rgid      &&
	    basecred->cr_groups[0] == usercred->cr_svgid)
		return (0);

	return (EPERM);
}

int fuse_internal_access(struct vnode *vp, mode_t mode,
    struct fuse_access_param *facp, struct thread *td, struct ucred *cred);

/* attributes */
void fuse_internal_cache_attrs(struct vnode *vp, struct fuse_attr *attr,
	uint64_t attr_valid, uint32_t attr_valid_nsec, struct vattr *vap);

/* fsync */

int fuse_internal_fsync(struct vnode *vp, struct thread *td,
    struct ucred *cred, struct fuse_filehandle *fufh);
int fuse_internal_fsync_callback(struct fuse_ticket *tick, struct uio *uio);

/* readdir */

struct pseudo_dirent {
	uint32_t d_namlen;
};

int fuse_internal_readdir(struct vnode *vp, struct uio *uio,
    struct fuse_filehandle *fufh, struct fuse_iov *cookediov);
int fuse_internal_readdir_processdata(struct uio *uio, size_t reqsize,
    void *buf, size_t bufsize, void *param);

/* remove */

int fuse_internal_remove(struct vnode *dvp, struct vnode *vp,
    struct componentname *cnp, enum fuse_opcode op);

/* rename */

int fuse_internal_rename(struct vnode *fdvp, struct componentname *fcnp,
    struct vnode *tdvp, struct componentname *tcnp);

/* revoke */

void fuse_internal_vnode_disappear(struct vnode *vp);

/* strategy */

/* entity creation */

static inline int
fuse_internal_checkentry(struct fuse_entry_out *feo, enum vtype vtyp)
{
	if (vtyp != IFTOVT(feo->attr.mode)) {
		return (EINVAL);
	}

	if (feo->nodeid == FUSE_NULL_ID) {
		return (EINVAL);
	}

	if (feo->nodeid == FUSE_ROOT_ID) {
		return (EINVAL);
	}

	return (0);
}

int fuse_internal_newentry(struct vnode *dvp, struct vnode **vpp,
    struct componentname *cnp, enum fuse_opcode op, void *buf, size_t bufsize,
    enum vtype vtyp);

void fuse_internal_newentry_makerequest(struct mount *mp, uint64_t dnid,
    struct componentname *cnp, enum fuse_opcode op, void *buf, size_t bufsize,
    struct fuse_dispatcher *fdip);

int fuse_internal_newentry_core(struct vnode *dvp, struct vnode **vpp,
    struct componentname *cnp, enum vtype vtyp, struct fuse_dispatcher *fdip);

/* entity destruction */

int fuse_internal_forget_callback(struct fuse_ticket *tick, struct uio *uio);
void fuse_internal_forget_send(struct mount *mp, struct thread *td,
    struct ucred *cred, uint64_t nodeid, uint64_t nlookup);

/* fuse start/stop */

int fuse_internal_init_callback(struct fuse_ticket *tick, struct uio *uio);
void fuse_internal_send_init(struct fuse_data *data, struct thread *td);

#endif /* _FUSE_INTERNAL_H_ */
