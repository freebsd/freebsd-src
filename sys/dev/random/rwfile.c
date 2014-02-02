/*-
 * Copyright (c) 2013 Mark R V Murray
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_random.h"

#ifdef RANDOM_RWFILE

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/namei.h>
#include <sys/fcntl.h>
#include <sys/vnode.h>

#include <dev/random/rwfile.h>

int
randomdev_read_file(const char *filename, void *buf, size_t length)
{
	struct nameidata nd;
	struct thread* td = curthread;
	int error;
	ssize_t resid;
	int flags;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, filename, td);
	flags = FREAD;
	error = vn_open(&nd, &flags, 0, NULL);
	if (error == 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		if (nd.ni_vp->v_type != VREG)
			error = ENOEXEC;
		else
			error = vn_rdwr(UIO_READ, nd.ni_vp, buf, length, 0, UIO_SYSSPACE, IO_NODELOCKED, td->td_ucred, NOCRED, &resid, td);
		VOP_UNLOCK(nd.ni_vp, 0);
		vn_close(nd.ni_vp, FREAD, td->td_ucred, td);
	}

	return (error);
}

int
randomdev_write_file(const char *filename, void *buf, size_t length)
{
	struct nameidata nd;
	struct thread* td = curthread;
	int error;
	ssize_t resid;
	int flags;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, filename, td);
	flags = FWRITE | O_CREAT | O_TRUNC;
	error = vn_open(&nd, &flags, 0, NULL);
	if (error == 0) {
		NDFREE(&nd, NDF_ONLY_PNBUF);
		if (nd.ni_vp->v_type != VREG)
			error = ENOEXEC;
		else
			error = vn_rdwr(UIO_WRITE, nd.ni_vp, buf, length, 0, UIO_SYSSPACE, IO_NODELOCKED, td->td_ucred, NOCRED, &resid, td);

		VOP_UNLOCK(nd.ni_vp, 0);
		vn_close(nd.ni_vp, FREAD, td->td_ucred, td);
	}

	return (error);
}

#endif
