/*
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1994 Christos Zoulas
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * $FreeBSD: src/sys/svr4/svr4_errno.h,v 1.3 1999/08/28 00:51:14 peter Exp $
 */

#ifndef	_SVR4_ERRNO_H_
#define	_SVR4_ERRNO_H_

#define	SVR4_EPERM		1
#define	SVR4_ENOENT		2
#define	SVR4_ESRCH		3
#define	SVR4_EINTR		4
#define	SVR4_EIO		5
#define	SVR4_ENXIO		6
#define	SVR4_E2BIG		7
#define	SVR4_ENOEXEC		8
#define	SVR4_EBADF		9
#define	SVR4_ECHILD		10
#define	SVR4_EAGAIN		11
#define	SVR4_ENOMEM		12
#define	SVR4_EACCES		13
#define	SVR4_EFAULT		14
#define	SVR4_ENOTBLK		15
#define	SVR4_EBUSY		16
#define	SVR4_EEXIST		17
#define	SVR4_EXDEV		18
#define	SVR4_ENODEV		19
#define	SVR4_ENOTDIR		20
#define	SVR4_EISDIR		21
#define	SVR4_EINVAL		22
#define	SVR4_ENFILE		23
#define	SVR4_EMFILE		24
#define	SVR4_ENOTTY		25
#define	SVR4_ETXTBSY		26
#define	SVR4_EFBIG		27
#define	SVR4_ENOSPC		28
#define	SVR4_ESPIPE		29
#define	SVR4_EROFS		30
#define	SVR4_EMLINK		31
#define	SVR4_EPIPE		32
#define	SVR4_EDOM		33
#define	SVR4_ERANGE		34
#define	SVR4_ENOMSG		35
#define	SVR4_EIDRM		36
#define	SVR4_ECHRNG		37
#define	SVR4_EL2NSYNC		38
#define	SVR4_EL3HLT		39
#define	SVR4_EL3RST		40
#define	SVR4_ELNRNG		41
#define	SVR4_EUNATCH		42
#define	SVR4_ENOCSI		43
#define	SVR4_EL2HLT		44
#define	SVR4_EDEADLK		45
#define	SVR4_ENOLCK		46
#define	SVR4_EBADE		50
#define	SVR4_EBADR		51
#define	SVR4_EXFULL		52
#define	SVR4_ENOANO		53
#define	SVR4_EBADRQC		54
#define	SVR4_EBADSLT		55
#define	SVR4_EDEADLOCK		56
#define	SVR4_EBFONT		57
#define	SVR4_ENOSTR		60
#define	SVR4_ENODATA		61
#define	SVR4_ETIME		62
#define	SVR4_ENOSR		63
#define	SVR4_ENONET		64
#define	SVR4_ENOPKG		65
#define	SVR4_EREMOTE		66
#define	SVR4_ENOLINK		67
#define	SVR4_EADV		68
#define	SVR4_ESRMNT		69
#define	SVR4_ECOMM		70
#define	SVR4_EPROTO		71
#define	SVR4_EMULTIHOP		74
#define	SVR4_EBADMSG		77
#define	SVR4_ENAMETOOLONG	78
#define	SVR4_EOVERFLOW		79
#define	SVR4_ENOTUNIQ		80
#define	SVR4_EBADFD		81
#define	SVR4_EREMCHG		82
#define	SVR4_ELIBACC		83
#define	SVR4_ELIBBAD		84
#define	SVR4_ELIBSCN		85
#define	SVR4_ELIBMAX		86
#define	SVR4_ELIBEXEC		87
#define	SVR4_EILSEQ		88
#define	SVR4_ENOSYS		89
#define	SVR4_ELOOP		90
#define	SVR4_ERESTART		91
#define	SVR4_ESTRPIPE		92
#define	SVR4_ENOTEMPTY		93
#define	SVR4_EUSERS		94
#define	SVR4_ENOTSOCK		95
#define	SVR4_EDESTADDRREQ	96
#define	SVR4_EMSGSIZE		97
#define	SVR4_EPROTOTYPE		98
#define	SVR4_ENOPROTOOPT	99
#define	SVR4_EPROTONOSUPPORT	120
#define	SVR4_ESOCKTNOSUPPORT	121
#define	SVR4_EOPNOTSUPP		122
#define	SVR4_EPFNOSUPPORT	123
#define	SVR4_EAFNOSUPPORT	124
#define	SVR4_EADDRINUSE		125
#define	SVR4_EADDRNOTAVAIL	126
#define	SVR4_ENETDOWN		127
#define	SVR4_ENETUNREACH	128
#define	SVR4_ENETRESET		129
#define	SVR4_ECONNABORTED	130
#define	SVR4_ECONNRESET		131
#define	SVR4_ENOBUFS		132
#define	SVR4_EISCONN		133
#define	SVR4_ENOTCONN		134
#define	SVR4_EUCLEAN		135
#define	SVR4_ENOTNAM		137
#define	SVR4_ENAVAIL		138
#define	SVR4_EISNAM		139
#define	SVR4_EREMOTEIO		140
#define	SVR4_EINIT		141
#define	SVR4_EREMDEV		142
#define	SVR4_ESHUTDOWN		143
#define	SVR4_ETOOMANYREFS	144
#define	SVR4_ETIMEDOUT		145
#define	SVR4_ECONNREFUSED	146
#define	SVR4_EHOSTDOWN		147
#define	SVR4_EHOSTUNREACH	148
#define	SVR4_EWOULDBLOCK	SVR4_EAGAIN
#define	SVR4_EALREADY		149
#define	SVR4_EINPROGRESS	150
#define	SVR4_ESTALE		151
#define	SVR4_EIORESID		500

/*
 * These ones are not translated...
 */
#define	SVR4_EPROCLIM		SVR4_ENOSYS
#define	SVR4_EDQUOT		SVR4_ENOSYS
#define	SVR4_EBADRPC		SVR4_ENOSYS
#define	SVR4_ERPCMISMATCH	SVR4_ENOSYS
#define	SVR4_EPROGUNAVAIL	SVR4_ENOSYS
#define	SVR4_EPROGMISMATCH	SVR4_ENOSYS
#define	SVR4_EPROCUNAVAIL	SVR4_ENOSYS
#define	SVR4_EFTYPE		SVR4_ENOSYS
#define	SVR4_EAUTH		SVR4_ENOSYS
#define	SVR4_ENEEDAUTH		SVR4_ENOSYS

#endif /* !_SVR4_ERRNO_H_ */
