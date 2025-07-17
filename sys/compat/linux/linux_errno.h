/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 The FreeBSD Foundation
 *
 * This software was developed by Edward Tomasz Napierala under sponsorship
 * from the FreeBSD Foundation.
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

#ifndef _LINUX_ERRNO_H_
#define _LINUX_ERRNO_H_

#define	LINUX_EPERM		1
#define	LINUX_ENOENT		2
#define	LINUX_ESRCH		3
#define	LINUX_EINTR		4
#define	LINUX_EIO		5
#define	LINUX_ENXIO		6
#define	LINUX_E2BIG		7
#define	LINUX_ENOEXEC		8
#define	LINUX_EBADF		9

#define	LINUX_ECHILD		10
#define	LINUX_EAGAIN		11
#define	LINUX_ENOMEM		12
#define	LINUX_EACCES		13
#define	LINUX_EFAULT		14
#define	LINUX_ENOTBLK		15
#define	LINUX_EBUSY		16
#define	LINUX_EEXIST		17
#define	LINUX_EXDEV		18
#define	LINUX_ENODEV		19

#define	LINUX_ENOTDIR		20
#define	LINUX_EISDIR		21
#define	LINUX_EINVAL		22
#define	LINUX_ENFILE		23
#define	LINUX_EMFILE		24
#define	LINUX_ENOTTY		25
#define	LINUX_ETXTBSY		26
#define	LINUX_EFBIG		27
#define	LINUX_ENOSPC		28
#define	LINUX_ESPIPE		29

#define	LINUX_EROFS		30
#define	LINUX_EMLINK		31
#define	LINUX_EPIPE		32
#define	LINUX_EDOM		33
#define	LINUX_ERANGE		34
#define	LINUX_EDEADLK		35
#define	LINUX_ENAMETOOLONG	36
#define	LINUX_ENOLCK		37
#define	LINUX_ENOSYS		38
#define	LINUX_ENOTEMPTY		39

#define	LINUX_ELOOP		40
/* XXX: errno 41 is not defined in Linux. */
#define	LINUX_ENOMSG		42
#define	LINUX_EIDRM		43
#define	LINUX_ECHRNG		44
#define	LINUX_EL2NSYNC		45
#define	LINUX_EL3HLT		46
#define	LINUX_EL3RST		47
#define	LINUX_ELNRNG		48
#define	LINUX_EUNATCH		49

#define	LINUX_ENOCSI		50
#define	LINUX_EL2HLT		51
#define	LINUX_EBADE		52
#define	LINUX_EBADR		53
#define	LINUX_EXFULL		54
#define	LINUX_ENOANO		55
#define	LINUX_EBADRQC		56
#define	LINUX_EBADSLT		57
/* XXX: errno 58 is not defined in Linux. */
#define	LINUX_EBFONT		59

#define	LINUX_ENOSTR		60
#define	LINUX_ENODATA		61
#define	LINUX_ENOTIME		62
#define	LINUX_ENOSR		63
#define	LINUX_ENONET		64
#define	LINUX_ENOPKG		65
#define	LINUX_EREMOTE		66
#define	LINUX_ENOLINK		67
#define	LINUX_EADV		68
#define	LINUX_ESRMNT		69

#define	LINUX_ECOMM		70
#define	LINUX_EPROTO		71
#define	LINUX_EMULTIHOP		72
#define	LINUX_EDOTDOT		73
#define	LINUX_EBADMSG		74
#define	LINUX_EOVERFLOW		75
#define	LINUX_ENOTUNIQ		76
#define	LINUX_EBADFD		77
#define	LINUX_EREMCHG		78
#define	LINUX_ELIBACC		79

#define	LINUX_ELIBBAD		80
#define	LINUX_ELIBSCN		81
#define	LINUX_ELIBMAX		82
#define	LINUX_ELIBEXEC		83
#define	LINUX_EILSEQ		84
#define	LINUX_ERESTART		85
#define	LINUX_ESTRPIPE		86
#define	LINUX_EUSERS		87
#define	LINUX_ENOTSOCK		88
#define	LINUX_EDESTADDRREQ	89

#define	LINUX_EMSGSIZE		90
#define	LINUX_EPROTOTYPE	91
#define	LINUX_ENOPROTOOPT	92
#define	LINUX_EPROTONOTSUPPORT	93
#define	LINUX_ESOCKNOTSUPPORT	94
#define	LINUX_EOPNOTSUPPORT	95
#define	LINUX_EPFNOTSUPPORT	96
#define	LINUX_EAFNOTSUPPORT	97
#define	LINUX_EADDRINUSE	98
#define	LINUX_EADDRNOTAVAIL	99

#define	LINUX_ENETDOWN		100
#define	LINUX_ENETUNREACH	101
#define	LINUX_ENETRESET		102
#define	LINUX_ECONNABORTED	103
#define	LINUX_ECONNRESET	104
#define	LINUX_ENOBUFS		105
#define	LINUX_EISCONN		106
#define	LINUX_ENOTCONN		107
#define	LINUX_ESHUTDOWN		108
#define	LINUX_ETOOMANYREFS	109

#define	LINUX_ETIMEDOUT		110
#define	LINUX_ECONNREFUSED	111
#define	LINUX_EHOSTDOWN		112
#define	LINUX_EHOSTUNREACH	113
#define	LINUX_EALREADY		114
#define	LINUX_EINPROGRESS	115
#define	LINUX_ESTALE		116
#define	LINUX_EUCLEAN		117
#define	LINUX_ENOTNAM		118
#define	LINUX_ENAVAIL		119

#define	LINUX_EISNAM		120
#define	LINUX_EREMOTEIO		121
#define	LINUX_EDQUOT		122
#define	LINUX_ENOMEDIUM		123
#define	LINUX_EMEDIUMTYPE	124
#define	LINUX_ECANCELED		125
#define	LINUX_ENOKEY		126
#define	LINUX_EKEYEXPIRED	127
#define	LINUX_EKEYREVOKED	128
#define	LINUX_EKEYREJECTED	129

#define	LINUX_EOWNERDEAD	130
#define	LINUX_ENOTRECOVERABLE	131
#define	LINUX_ERFKILL		132
#define	LINUX_EHWPOISON		133

#define	LINUX_ELAST		LINUX_EHWPOISON

/*
 * This is a special "internal" errno that must never be returned
 * to a Linux process, but might be observed via ptrace(2).
 */
#define	LINUX_ERESTARTSYS	512

#endif /* _LINUX_ERRNO_H_ */
