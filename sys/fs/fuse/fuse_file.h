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

#ifndef _FUSE_FILE_H_
#define _FUSE_FILE_H_

#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/vnode.h>

/* 
 * The fufh type is the access mode of the fuse file handle.  It's the portion
 * of the open(2) flags related to permission.
 */
typedef enum fufh_type {
	FUFH_INVALID = -1,
	FUFH_RDONLY  = O_RDONLY,
	FUFH_WRONLY  = O_WRONLY,
	FUFH_RDWR    = O_RDWR,
	FUFH_EXEC    = O_EXEC,
} fufh_type_t;

/*
 * FUSE File Handles
 *
 * The FUSE protocol says that a server may assign a unique 64-bit file handle
 * every time that a file is opened.  Effectively, that's once for each file
 * descriptor.
 *
 * Unfortunately, the VFS doesn't help us here.  VOPs don't have a
 * struct file* argument.  fileops do, but many syscalls bypass the fileops
 * layer and go straight to a vnode.  Some, like writing from cache, can't
 * track a file handle even in theory.  The entire concept of the file handle
 * is a product of FUSE's Linux origins; Linux lacks vnodes and almost every
 * file system operation takes a struct file* argument.
 *
 * Since FreeBSD's VFS is more file descriptor-agnostic, we must store FUSE
 * filehandles in the vnode.  One option would be to only store a single file
 * handle and never open FUSE files concurrently.  That's what NetBSD does.
 * But that violates FUSE's security model.  FUSE expects the server to do all
 * authorization (except when mounted with -o default_permissions).  In order
 * to do that, the server needs us to send FUSE_OPEN every time somebody opens
 * a new file descriptor.
 *
 * Another option would be to never open FUSE files concurrently, but send a
 * FUSE_ACCESS prior to every open after the first.  That would give the server
 * the opportunity to authorize the access.  Unfortunately, the FUSE protocol
 * makes ACCESS optional.  File systems that don't implement it are assumed to
 * authorize everything.  A survey of 32 fuse file systems showed that only 14
 * implemented access.  Among the laggards were a few that really ought to be
 * doing server-side authorization.
 *
 * So we do something hacky, similar to what OpenBSD, Illumos, and OSXFuse do.
 * we store a list of file handles, one for each combination of vnode, uid,
 * gid, pid, and access mode.  When opening a file, we first check whether
 * there's already a matching file handle.  If so, we reuse it.  If not, we
 * send FUSE_OPEN and create a new file handle.  That minimizes the number of
 * open file handles while still allowing the server to authorize stuff.
 *
 * VOPs that need a file handle search through the list for a close match.
 * They can't be guaranteed of finding an exact match because, for example, a
 * process may have changed its UID since opening the file.  Also, most VOPs
 * don't know exactly what permission they need.  Is O_RDWR required or is
 * O_RDONLY good enough?  So the file handle we end up using may not be exactly
 * the one we're supposed to use with that file descriptor.  But if the FUSE
 * file system isn't too picky, it will work.  (FWIW even Linux sometimes
 * guesses the file handle, during writes from cache or most SETATTR
 * operations).
 *
 * I suspect this mess is part of the reason why neither NFS nor 9P have an
 * equivalent of FUSE file handles.
 */
struct fuse_filehandle {
	LIST_ENTRY(fuse_filehandle) next;

	/* The filehandle returned by FUSE_OPEN */
	uint64_t fh_id;

	/*
	 * flags returned by FUSE_OPEN
	 * Supported flags: FOPEN_DIRECT_IO, FOPEN_KEEP_CACHE
	 * Unsupported:
	 *     FOPEN_NONSEEKABLE: Adding support would require a new per-file
	 *     or per-vnode attribute, which would have to be checked by
	 *     kern_lseek (and others) for every file system.  The benefit is
	 *     dubious, since I'm unaware of any file systems in ports that use
	 *     this flag.
	 */
	uint32_t fuse_open_flags;

	/* The access mode of the file handle */
	fufh_type_t fufh_type;

	/* Credentials used to open the file */
	gid_t gid;
	pid_t pid;
	uid_t uid;
};

#define FUFH_IS_VALID(f)  ((f)->fufh_type != FUFH_INVALID)

/*
 * Get the flags to use for FUSE_CREATE, FUSE_OPEN and FUSE_RELEASE
 *
 * These are supposed to be the same as the flags argument to open(2).
 * However, since we can't reliably associate a fuse_filehandle with a specific
 * file descriptor it would would be dangerous to include anything more than
 * the access mode flags.  For example, suppose we open a file twice, once with
 * O_APPEND and once without.  Then the user pwrite(2)s to offset using the
 * second file descriptor.  If fusefs uses the first file handle, then the
 * server may append the write to the end of the file rather than at offset 0.
 * To prevent problems like this, we only ever send the portion of flags
 * related to access mode.
 *
 * It's essential to send that portion, because FUSE uses it for server-side
 * authorization.
 */
static inline int
fufh_type_2_fflags(fufh_type_t type)
{
	int oflags = -1;

	switch (type) {
	case FUFH_RDONLY:
	case FUFH_WRONLY:
	case FUFH_RDWR:
	case FUFH_EXEC:
		oflags = type;
		break;
	default:
		break;
	}

	return oflags;
}

bool fuse_filehandle_validrw(struct vnode *vp, int mode,
	struct ucred *cred, pid_t pid);
int fuse_filehandle_get(struct vnode *vp, int fflag,
                        struct fuse_filehandle **fufhp, struct ucred *cred,
			pid_t pid);
int fuse_filehandle_get_anyflags(struct vnode *vp,
                        struct fuse_filehandle **fufhp, struct ucred *cred,
			pid_t pid);
int fuse_filehandle_getrw(struct vnode *vp, int fflag,
                          struct fuse_filehandle **fufhp, struct ucred *cred,
			  pid_t pid);

void fuse_filehandle_init(struct vnode *vp, fufh_type_t fufh_type,
		          struct fuse_filehandle **fufhp, struct thread *td,
			  struct ucred *cred, struct fuse_open_out *foo);
int fuse_filehandle_open(struct vnode *vp, int mode,
                         struct fuse_filehandle **fufhp, struct thread *td,
                         struct ucred *cred);
int fuse_filehandle_close(struct vnode *vp, struct fuse_filehandle *fufh,
                          struct thread *td, struct ucred *cred);

void fuse_file_init(void);
void fuse_file_destroy(void);

#endif /* _FUSE_FILE_H_ */
