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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/counter.h>
#include <sys/module.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/sx.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/sdt.h>
#include <sys/sysctl.h>

#include "fuse.h"
#include "fuse_file.h"
#include "fuse_internal.h"
#include "fuse_io.h"
#include "fuse_ipc.h"
#include "fuse_node.h"

MALLOC_DEFINE(M_FUSE_FILEHANDLE, "fuse_filefilehandle", "FUSE file handle");

SDT_PROVIDER_DECLARE(fusefs);
/* 
 * Fuse trace probe:
 * arg0: verbosity.  Higher numbers give more verbose messages
 * arg1: Textual message
 */
SDT_PROBE_DEFINE2(fusefs, , file, trace, "int", "char*");

static counter_u64_t fuse_fh_count;

SYSCTL_COUNTER_U64(_vfs_fusefs_stats, OID_AUTO, filehandle_count, CTLFLAG_RD,
    &fuse_fh_count, "number of open FUSE filehandles");

/* Get the FUFH type for a particular access mode */
static inline fufh_type_t
fflags_2_fufh_type(int fflags)
{
	if ((fflags & FREAD) && (fflags & FWRITE))
		return FUFH_RDWR;
	else if (fflags & (FWRITE))
		return FUFH_WRONLY;
	else if (fflags & (FREAD))
		return FUFH_RDONLY;
	else if (fflags & (FEXEC))
		return FUFH_EXEC;
	else
		panic("FUSE: What kind of a flag is this (%x)?", fflags);
}

int
fuse_filehandle_open(struct vnode *vp, int a_mode,
    struct fuse_filehandle **fufhp, struct thread *td, struct ucred *cred)
{
	struct mount *mp = vnode_mount(vp);
	struct fuse_data *data = fuse_get_mpdata(mp);
	struct fuse_dispatcher fdi;
	const struct fuse_open_out default_foo = {
		.fh = 0,
		.open_flags = FOPEN_KEEP_CACHE,
		.padding = 0
	};
	struct fuse_open_in *foi = NULL;
	const struct fuse_open_out *foo;
	fufh_type_t fufh_type;
	int dataflags = data->dataflags;
	int err = 0;
	int oflags = 0;
	int op = FUSE_OPEN;
	int relop = FUSE_RELEASE;
	int fsess_no_op_support = FSESS_NO_OPEN_SUPPORT;

	fufh_type = fflags_2_fufh_type(a_mode);
	oflags = fufh_type_2_fflags(fufh_type);

	if (vnode_isdir(vp)) {
		op = FUSE_OPENDIR;
		relop = FUSE_RELEASEDIR;
		fsess_no_op_support = FSESS_NO_OPENDIR_SUPPORT;
		/* vn_open_vnode already rejects FWRITE on directories */
		MPASS(fufh_type == FUFH_RDONLY || fufh_type == FUFH_EXEC);
	}
	fdisp_init(&fdi, sizeof(*foi));
	if (fsess_not_impl(mp, op) && dataflags & fsess_no_op_support) {
		/* The operation implicitly succeeds */
		foo = &default_foo;
	} else {
		fdisp_make_vp(&fdi, op, vp, td, cred);

		foi = fdi.indata;
		foi->flags = oflags;

		err = fdisp_wait_answ(&fdi);
		if (err == ENOSYS && dataflags & fsess_no_op_support) {
			/* The operation implicitly succeeds */
			foo = &default_foo;
			fsess_set_notimpl(mp, op);
			fsess_set_notimpl(mp, relop);
			err = 0;
		} else if (err) {
			SDT_PROBE2(fusefs, , file, trace, 1,
				"OUCH ... daemon didn't give fh");
			if (err == ENOENT)
				fuse_internal_vnode_disappear(vp);
			goto out;
		} else {
			foo = fdi.answ;
		}
	}

	fuse_filehandle_init(vp, fufh_type, fufhp, td, cred, foo);
	fuse_vnode_open(vp, foo->open_flags, td);

out:
	if (foi)
		fdisp_destroy(&fdi);
	return err;
}

int
fuse_filehandle_close(struct vnode *vp, struct fuse_filehandle *fufh,
    struct thread *td, struct ucred *cred)
{
	struct mount *mp = vnode_mount(vp);
	struct fuse_dispatcher fdi;
	struct fuse_release_in *fri;

	int err = 0;
	int op = FUSE_RELEASE;

	if (fuse_isdeadfs(vp)) {
		goto out;
	}
	if (vnode_isdir(vp))
		op = FUSE_RELEASEDIR;

	if (fsess_not_impl(mp, op))
		goto out;

	fdisp_init(&fdi, sizeof(*fri));
	fdisp_make_vp(&fdi, op, vp, td, cred);
	fri = fdi.indata;
	fri->fh = fufh->fh_id;
	fri->flags = fufh_type_2_fflags(fufh->fufh_type);
	/* 
	 * If the file has a POSIX lock then we're supposed to set lock_owner.
	 * If not, then lock_owner is undefined.  So we may as well always set
	 * it.
	 */
	fri->lock_owner = td->td_proc->p_pid;

	err = fdisp_wait_answ(&fdi);
	fdisp_destroy(&fdi);

out:
	counter_u64_add(fuse_fh_count, -1);
	LIST_REMOVE(fufh, next);
	free(fufh, M_FUSE_FILEHANDLE);

	return err;
}

/*
 * Check for a valid file handle, first the type requested, but if that
 * isn't valid, try for FUFH_RDWR.
 * Return true if there is any file handle with the correct credentials and
 * a fufh type that includes the provided one.
 * A pid of 0 means "don't care"
 */
bool
fuse_filehandle_validrw(struct vnode *vp, int mode,
	struct ucred *cred, pid_t pid)
{
	struct fuse_vnode_data *fvdat = VTOFUD(vp);
	struct fuse_filehandle *fufh;
	fufh_type_t fufh_type = fflags_2_fufh_type(mode);

	/* 
	 * Unlike fuse_filehandle_get, we want to search for a filehandle with
	 * the exact cred, and no fallback
	 */
	LIST_FOREACH(fufh, &fvdat->handles, next) {
		if (fufh->fufh_type == fufh_type &&
		    fufh->uid == cred->cr_uid &&
		    fufh->gid == cred->cr_rgid &&
		    (pid == 0 || fufh->pid == pid))
			return true;
	}

	if (fufh_type == FUFH_EXEC)
		return false;

	/* Fallback: find a RDWR list entry with the right cred */
	LIST_FOREACH(fufh, &fvdat->handles, next) {
		if (fufh->fufh_type == FUFH_RDWR &&
		    fufh->uid == cred->cr_uid &&
		    fufh->gid == cred->cr_rgid &&
		    (pid == 0 || fufh->pid == pid))
			return true;
	}

	return false;
}

int
fuse_filehandle_get(struct vnode *vp, int fflag,
    struct fuse_filehandle **fufhp, struct ucred *cred, pid_t pid)
{
	struct fuse_vnode_data *fvdat = VTOFUD(vp);
	struct fuse_filehandle *fufh;
	fufh_type_t fufh_type;

	fufh_type = fflags_2_fufh_type(fflag);
	/* cred can be NULL for in-kernel clients */
	if (cred == NULL)
		goto fallback;

	LIST_FOREACH(fufh, &fvdat->handles, next) {
		if (fufh->fufh_type == fufh_type &&
		    fufh->uid == cred->cr_uid &&
		    fufh->gid == cred->cr_rgid &&
		    (pid == 0 || fufh->pid == pid))
			goto found;
	}

fallback:
	/* Fallback: find a list entry with the right flags */
	LIST_FOREACH(fufh, &fvdat->handles, next) {
		if (fufh->fufh_type == fufh_type)
			break;
	}

	if (fufh == NULL)
		return EBADF;

found:
	if (fufhp != NULL)
		*fufhp = fufh;
	return 0;
}

/* Get a file handle with any kind of flags */
int
fuse_filehandle_get_anyflags(struct vnode *vp,
    struct fuse_filehandle **fufhp, struct ucred *cred, pid_t pid)
{
	struct fuse_vnode_data *fvdat = VTOFUD(vp);
	struct fuse_filehandle *fufh;

	if (cred == NULL)
		goto fallback;

	LIST_FOREACH(fufh, &fvdat->handles, next) {
		if (fufh->uid == cred->cr_uid &&
		    fufh->gid == cred->cr_rgid &&
		    (pid == 0 || fufh->pid == pid))
			goto found;
	}

fallback:
	/* Fallback: find any list entry */
	fufh = LIST_FIRST(&fvdat->handles);

	if (fufh == NULL)
		return EBADF;

found:
	if (fufhp != NULL)
		*fufhp = fufh;
	return 0;
}

int
fuse_filehandle_getrw(struct vnode *vp, int fflag,
    struct fuse_filehandle **fufhp, struct ucred *cred, pid_t pid)
{
	int err;

	err = fuse_filehandle_get(vp, fflag, fufhp, cred, pid);
	if (err)
		err = fuse_filehandle_get(vp, FREAD | FWRITE, fufhp, cred, pid);
	return err;
}

void
fuse_filehandle_init(struct vnode *vp, fufh_type_t fufh_type,
    struct fuse_filehandle **fufhp, struct thread *td, const struct ucred *cred,
    const struct fuse_open_out *foo)
{
	struct fuse_vnode_data *fvdat = VTOFUD(vp);
	struct fuse_filehandle *fufh;

	fufh = malloc(sizeof(struct fuse_filehandle), M_FUSE_FILEHANDLE,
		M_WAITOK);
	MPASS(fufh != NULL);
	fufh->fh_id = foo->fh;
	fufh->fufh_type = fufh_type;
	fufh->gid = cred->cr_rgid;
	fufh->uid = cred->cr_uid;
	fufh->pid = td->td_proc->p_pid;
	fufh->fuse_open_flags = foo->open_flags;
	if (!FUFH_IS_VALID(fufh)) {
		panic("FUSE: init: invalid filehandle id (type=%d)", fufh_type);
	}
	LIST_INSERT_HEAD(&fvdat->handles, fufh, next);
	if (fufhp != NULL)
		*fufhp = fufh;

	counter_u64_add(fuse_fh_count, 1);

	if (foo->open_flags & FOPEN_DIRECT_IO) {
		ASSERT_VOP_ELOCKED(vp, __func__);
		VTOFUD(vp)->flag |= FN_DIRECTIO;
		fuse_io_invalbuf(vp, td);
	} else {
		if ((foo->open_flags & FOPEN_KEEP_CACHE) == 0)
			fuse_io_invalbuf(vp, td);
	        VTOFUD(vp)->flag &= ~FN_DIRECTIO;
	}

}

void
fuse_file_init(void)
{
	fuse_fh_count = counter_u64_alloc(M_WAITOK);
}

void
fuse_file_destroy(void)
{
	counter_u64_free(fuse_fh_count);
}
