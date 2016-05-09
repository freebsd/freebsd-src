/*-
 * Copyright (c) 2015 John H. Baldwin <jhb@FreeBSD.org>
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
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <sysdecode.h>

#if defined(__i386__) || defined(__amd64__)
/*
 * Linux syscalls return negative errno's, we do positive and map them
 * Reference:
 *   FreeBSD: src/sys/sys/errno.h
 *   Linux:   linux-2.6.17.8/include/asm-generic/errno-base.h
 *            linux-2.6.17.8/include/asm-generic/errno.h
 */
static int bsd_to_linux_errno[ELAST + 1] = {
	-0,  -1,  -2,  -3,  -4,  -5,  -6,  -7,  -8,  -9,
	-10, -35, -12, -13, -14, -15, -16, -17, -18, -19,
	-20, -21, -22, -23, -24, -25, -26, -27, -28, -29,
	-30, -31, -32, -33, -34, -11,-115,-114, -88, -89,
	-90, -91, -92, -93, -94, -95, -96, -97, -98, -99,
	-100,-101,-102,-103,-104,-105,-106,-107,-108,-109,
	-110,-111, -40, -36,-112,-113, -39, -11, -87,-122,
	-116, -66,  -6,  -6,  -6,  -6,  -6, -37, -38,  -9,
	  -6,  -6, -43, -42, -75,-125, -84, -95, -16, -74,
	 -72, -67, -71
};
#endif

#if defined(__aarch64__) || defined(__amd64__)
#include <contrib/cloudabi/cloudabi_types_common.h>

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
#endif

int
sysdecode_abi_to_freebsd_errno(enum sysdecode_abi abi, int error)
{

	switch (abi) {
	case SYSDECODE_ABI_FREEBSD:
	case SYSDECODE_ABI_FREEBSD32:
		return (error);
#if defined(__i386__) || defined(__amd64__)
	case SYSDECODE_ABI_LINUX:
	case SYSDECODE_ABI_LINUX32: {
		unsigned int i;

		/*
		 * This is imprecise since it returns the first
		 * matching errno.
		 */
		for (i = 0; i < nitems(bsd_to_linux_errno); i++) {
			if (error == bsd_to_linux_errno[i])
				return (i);
		}
		break;
	}
#endif
#if defined(__aarch64__) || defined(__amd64__)
	case SYSDECODE_ABI_CLOUDABI64:
		if (error >= 0 &&
		    (unsigned int)error < nitems(cloudabi_errno_table))
			return (cloudabi_errno_table[error]);
		break;
#endif
	default:
		break;
	}
	return (INT_MAX);
}

int
sysdecode_freebsd_to_abi_errno(enum sysdecode_abi abi, int error)
{

	switch (abi) {
	case SYSDECODE_ABI_FREEBSD:
	case SYSDECODE_ABI_FREEBSD32:
		return (error);
#if defined(__i386__) || defined(__amd64__)
	case SYSDECODE_ABI_LINUX:
	case SYSDECODE_ABI_LINUX32:
		if (error >= 0 && error <= ELAST)
			return (bsd_to_linux_errno[error]);
		break;
#endif
#if defined(__aarch64__) || defined(__amd64__)
	case SYSDECODE_ABI_CLOUDABI64: {
		unsigned int i;

		for (i = 0; i < nitems(cloudabi_errno_table); i++) {
			if (error == cloudabi_errno_table[i])
				return (i);
		}
		break;
	}
#endif
	default:
		break;
	}
	return (INT_MAX);
}

