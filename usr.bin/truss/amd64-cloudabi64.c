/*-
 * Copyright (c) 2015 Nuxi, https://nuxi.nl/
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/ptrace.h>

#include <machine/psl.h>

#include <errno.h>
#include <stdio.h>

#include <compat/cloudabi/cloudabi_syscalldefs.h>

#include "cloudabi64_syscalls.h"
#include "truss.h"

static int
amd64_cloudabi64_fetch_args(struct trussinfo *trussinfo, unsigned int narg)
{
	struct current_syscall *cs;
	struct reg regs;
	lwpid_t tid;

	tid = trussinfo->curthread->tid;
	if (ptrace(PT_GETREGS, tid, (caddr_t)&regs, 0) == -1) {
		fprintf(trussinfo->outfile, "-- CANNOT READ REGISTERS --\n");
		return (-1);
	}

	cs = &trussinfo->curthread->cs;
	if (narg >= 1)
		cs->args[0] = regs.r_rdi;
	if (narg >= 2)
		cs->args[1] = regs.r_rsi;
	if (narg >= 3)
		cs->args[2] = regs.r_rdx;
	if (narg >= 4)
		cs->args[3] = regs.r_rcx;
	if (narg >= 5)
		cs->args[4] = regs.r_r8;
	if (narg >= 6)
		cs->args[5] = regs.r_r9;
	return (0);
}

static const int cloudabi_errno_table[] = {
	[CLOUDABI_E2BIG]		= E2BIG,
	[CLOUDABI_EACCES]		= EACCES,
	[CLOUDABI_EADDRINUSE]		= EADDRINUSE,
	[CLOUDABI_EADDRNOTAVAIL]	= EADDRNOTAVAIL,
	[CLOUDABI_EAFNOSUPPORT]		= EAFNOSUPPORT,
	[CLOUDABI_EAGAIN]		= EAGAIN,
	[CLOUDABI_EALREADY]		= EALREADY,
	[CLOUDABI_EBADF]		= EBADF,
	[CLOUDABI_EBADMSG]		= EBADMSG,
	[CLOUDABI_EBUSY]		= EBUSY,
	[CLOUDABI_ECANCELED]		= ECANCELED,
	[CLOUDABI_ECHILD]		= ECHILD,
	[CLOUDABI_ECONNABORTED]		= ECONNABORTED,
	[CLOUDABI_ECONNREFUSED]		= ECONNREFUSED,
	[CLOUDABI_ECONNRESET]		= ECONNRESET,
	[CLOUDABI_EDEADLK]		= EDEADLK,
	[CLOUDABI_EDESTADDRREQ]		= EDESTADDRREQ,
	[CLOUDABI_EDOM]			= EDOM,
	[CLOUDABI_EDQUOT]		= EDQUOT,
	[CLOUDABI_EEXIST]		= EEXIST,
	[CLOUDABI_EFAULT]		= EFAULT,
	[CLOUDABI_EFBIG]		= EFBIG,
	[CLOUDABI_EHOSTUNREACH]		= EHOSTUNREACH,
	[CLOUDABI_EIDRM]		= EIDRM,
	[CLOUDABI_EILSEQ]		= EILSEQ,
	[CLOUDABI_EINPROGRESS]		= EINPROGRESS,
	[CLOUDABI_EINTR]		= EINTR,
	[CLOUDABI_EINVAL]		= EINVAL,
	[CLOUDABI_EIO]			= EIO,
	[CLOUDABI_EISCONN]		= EISCONN,
	[CLOUDABI_EISDIR]		= EISDIR,
	[CLOUDABI_ELOOP]		= ELOOP,
	[CLOUDABI_EMFILE]		= EMFILE,
	[CLOUDABI_EMLINK]		= EMLINK,
	[CLOUDABI_EMSGSIZE]		= EMSGSIZE,
	[CLOUDABI_EMULTIHOP]		= EMULTIHOP,
	[CLOUDABI_ENAMETOOLONG]		= ENAMETOOLONG,
	[CLOUDABI_ENETDOWN]		= ENETDOWN,
	[CLOUDABI_ENETRESET]		= ENETRESET,
	[CLOUDABI_ENETUNREACH]		= ENETUNREACH,
	[CLOUDABI_ENFILE]		= ENFILE,
	[CLOUDABI_ENOBUFS]		= ENOBUFS,
	[CLOUDABI_ENODEV]		= ENODEV,
	[CLOUDABI_ENOENT]		= ENOENT,
	[CLOUDABI_ENOEXEC]		= ENOEXEC,
	[CLOUDABI_ENOLCK]		= ENOLCK,
	[CLOUDABI_ENOLINK]		= ENOLINK,
	[CLOUDABI_ENOMEM]		= ENOMEM,
	[CLOUDABI_ENOMSG]		= ENOMSG,
	[CLOUDABI_ENOPROTOOPT]		= ENOPROTOOPT,
	[CLOUDABI_ENOSPC]		= ENOSPC,
	[CLOUDABI_ENOSYS]		= ENOSYS,
	[CLOUDABI_ENOTCONN]		= ENOTCONN,
	[CLOUDABI_ENOTDIR]		= ENOTDIR,
	[CLOUDABI_ENOTEMPTY]		= ENOTEMPTY,
	[CLOUDABI_ENOTRECOVERABLE]	= ENOTRECOVERABLE,
	[CLOUDABI_ENOTSOCK]		= ENOTSOCK,
	[CLOUDABI_ENOTSUP]		= ENOTSUP,
	[CLOUDABI_ENOTTY]		= ENOTTY,
	[CLOUDABI_ENXIO]		= ENXIO,
	[CLOUDABI_EOVERFLOW]		= EOVERFLOW,
	[CLOUDABI_EOWNERDEAD]		= EOWNERDEAD,
	[CLOUDABI_EPERM]		= EPERM,
	[CLOUDABI_EPIPE]		= EPIPE,
	[CLOUDABI_EPROTO]		= EPROTO,
	[CLOUDABI_EPROTONOSUPPORT]	= EPROTONOSUPPORT,
	[CLOUDABI_EPROTOTYPE]		= EPROTOTYPE,
	[CLOUDABI_ERANGE]		= ERANGE,
	[CLOUDABI_EROFS]		= EROFS,
	[CLOUDABI_ESPIPE]		= ESPIPE,
	[CLOUDABI_ESRCH]		= ESRCH,
	[CLOUDABI_ESTALE]		= ESTALE,
	[CLOUDABI_ETIMEDOUT]		= ETIMEDOUT,
	[CLOUDABI_ETXTBSY]		= ETXTBSY,
	[CLOUDABI_EXDEV]		= EXDEV,
	[CLOUDABI_ENOTCAPABLE]		= ENOTCAPABLE,
};

static int
amd64_cloudabi64_fetch_retval(struct trussinfo *trussinfo, long *retval,
    int *errorp)
{
	struct reg regs;
	lwpid_t tid;

	tid = trussinfo->curthread->tid;
	if (ptrace(PT_GETREGS, tid, (caddr_t)&regs, 0) == -1) {
		fprintf(trussinfo->outfile, "-- CANNOT READ REGISTERS --\n");
		return (-1);
	}

	retval[0] = regs.r_rax;
	retval[1] = regs.r_rdx;
	*errorp = (regs.r_rflags & PSL_C) != 0;
	if (*errorp && *retval >= 0 && *retval < nitems(cloudabi_errno_table) &&
	    cloudabi_errno_table[*retval] != 0)
		*retval = cloudabi_errno_table[*retval];
	return (0);
}

static struct procabi amd64_cloudabi64 = {
	"CloudABI ELF64",
	syscallnames,
	nitems(syscallnames),
	amd64_cloudabi64_fetch_args,
	amd64_cloudabi64_fetch_retval
};

PROCABI(amd64_cloudabi64);
