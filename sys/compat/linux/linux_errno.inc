/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 1994-1996 Søren Schmidt
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
 *
 * $FreeBSD$
 */

/*
 * Linux syscalls return negative errno's, we do positive and map them
 * Reference:
 *   FreeBSD: src/sys/sys/errno.h
 *   Linux:   include/uapi/asm-generic/errno-base.h
 *            include/uapi/asm-generic/errno.h
 */
const int linux_errtbl[ELAST + 1] = {
	/* [0, 9] */
	[0] = -0,
	[EPERM] = -1,
	[ENOENT] = -2,
	[ESRCH] = -3,
	[EINTR] = -4,
	[EIO] = -5,
	[ENXIO] = -6,
	[E2BIG] = -7,
	[ENOEXEC] = -8,
	[EBADF] = -9,

	/* [10, 19] */
	[ECHILD] = -10,
	[EDEADLK] = -35,
	[ENOMEM] = -12,
	[EACCES] = -13,
	[EFAULT] = -14,
	[ENOTBLK] = -15,
	[EBUSY] = -16,
	[EEXIST] = -17,
	[EXDEV] = -18,
	[ENODEV] = -19,

	/* [20, 29] */
	[ENOTDIR] = -20,
	[EISDIR] = -21,
	[EINVAL] = -22,
	[ENFILE] = -23,
	[EMFILE] = -24,
	[ENOTTY] = -25,
	[ETXTBSY] = -26,
	[EFBIG] = -27,
	[ENOSPC] = -28,
	[ESPIPE] = -29,

	/* [30, 39] */
	[EROFS] = -30,
	[EMLINK] = -31,
	[EPIPE] = -32,
	[EDOM] = -33,
	[ERANGE] = -34,
	[EAGAIN] = -11,
	[EINPROGRESS] = -115,
	[EALREADY] = -114,
	[ENOTSOCK] = -88,
	[EDESTADDRREQ] = -89,

	/* [40, 49] */
	[EMSGSIZE] = -90,
	[EPROTOTYPE] = -91,
	[ENOPROTOOPT] = -92,
	[EPROTONOSUPPORT] = -93,
	[ESOCKTNOSUPPORT] = -94,
	[EOPNOTSUPP] = -95,
	[EPFNOSUPPORT] = -96,
	[EAFNOSUPPORT] = -97,
	[EADDRINUSE] = -98,
	[EADDRNOTAVAIL] = -99,

	/* [50, 59] */
	[ENETDOWN] = -100,
	[ENETUNREACH] = -101,
	[ENETRESET] = -102,
	[ECONNABORTED] = -103,
	[ECONNRESET] = -104,
	[ENOBUFS] = -105,
	[EISCONN] = -106,
	[ENOTCONN] = -107,
	[ESHUTDOWN] = -108,
	[ETOOMANYREFS] = -109,

	/* [60, 69] */
	[ETIMEDOUT] = -110,
	[ECONNREFUSED] = -111,
	[ELOOP] = -40,
	[ENAMETOOLONG] = -36,
	[EHOSTDOWN] = -112,
	[EHOSTUNREACH] = -113,
	[ENOTEMPTY] = -39,
	[EPROCLIM] = -11,
	[EUSERS] = -87,
	[EDQUOT] = -122,

	/* [70, 79] */
	[ESTALE] = -116,
	[EREMOTE] = -66,
	[EBADRPC] = -6,		/* EBADRPC -> ENXIO */
	[ERPCMISMATCH] = -6,	/* ERPCMISMATCH -> ENXIO */
	[EPROGUNAVAIL] = -6,	/* EPROGUNAVAIL -> ENXIO */
	[EPROGMISMATCH] = -6,	/* EPROGMISMATCH -> ENXIO */
	[EPROCUNAVAIL] = -6,	/* EPROCUNAVAIL -> ENXIO */
	[ENOLCK] = -37,
	[ENOSYS] = -38,
	[EFTYPE] = -9,

	/* [80, 89] */
	[EAUTH] = -6,		/* EAUTH -> ENXIO */
	[ENEEDAUTH] = -6,	/* ENEEDAUTH -> ENXIO */
	[EIDRM] = -43,
	[ENOMSG] = -42,
	[EOVERFLOW] = -75,
	[ECANCELED] = -125,
	[EILSEQ] = -84,
	[ENOATTR] = -61,
	[EDOOFUS] = -22,	/* EDOOFUS -> EINVAL */
	[EBADMSG] = -74,

	/* [90, 99] */
	[EMULTIHOP] = -72,
	[ENOLINK] = -67,
	[EPROTO] = -71,
	[ENOTCAPABLE] = -1,	/* ENOTCAPABLE -> EPERM */
	[ECAPMODE] = -1,	/* ECAPMODE -> EPERM */
	[ENOTRECOVERABLE] = -131,	/* ENOTRECOVERABLE */
	[EOWNERDEAD] = -130,	/* EOWNERDEAD */
	[EINTEGRITY] = -22,	/* EINTEGRITY -> EINVAL */
};

_Static_assert(ELAST == 97,
    "missing errno entries in linux_errtbl");
