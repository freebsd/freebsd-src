/*
 * ibcs2_errno.h
 * Copyright (c) 1995 Scott Bartram
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Scott Bartram.
 * 4. The name of the author may not be used to endorse or promote products
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
 */

#ifndef _IBCS2_ERRNO_H
#define _IBCS2_ERRNO_H

#define _SCO_NET 1

#define IBCS2_EPERM		1
#define IBCS2_ENOENT		2
#define IBCS2_ESRCH		3
#define IBCS2_EINTR		4
#define IBCS2_EIO		5
#define IBCS2_ENXIO		6
#define IBCS2_E2BIG		7
#define IBCS2_ENOEXEC		8
#define IBCS2_EBADF		9
#define IBCS2_ECHILD		10
#define IBCS2_EAGAIN		11
#define IBCS2_ENOMEM		12
#define IBCS2_EACCES		13
#define IBCS2_EFAULT		14
#define IBCS2_ENOTBLK		15
#define IBCS2_EBUSY		16
#define IBCS2_EEXIST		17
#define IBCS2_EXDEV		18
#define IBCS2_ENODEV		19
#define IBCS2_ENOTDIR		20
#define IBCS2_EISDIR		21
#define IBCS2_EINVAL		22
#define IBCS2_ENFILE		23
#define IBCS2_EMFILE		24
#define IBCS2_ENOTTY		25
#define IBCS2_ETXTBSY		26
#define IBCS2_EFBIG		27
#define IBCS2_ENOSPC		28
#define IBCS2_ESPIPE		29
#define IBCS2_EROFS		30
#define IBCS2_EMLINK		31
#define IBCS2_EPIPE		32
#define IBCS2_EDOM		33
#define IBCS2_ERANGE		34
#define IBCS2_ENOMSG		35
#define IBCS2_EIDRM		36
#define IBCS2_ECHRNG		37
#define IBCS2_EL2NSYNC		38
#define IBCS2_EL3HLT		39
#define IBCS2_EL3RST		40
#define IBCS2_ELNRNG		41
#define IBCS2_EUNATCH		42
#define IBCS2_ENOCSI		43
#define IBCS2_EL2HLT		44
#define IBCS2_EDEADLK		45
#define IBCS2_ENOLCK		46
#define IBCS2_ENOSTR		60
#define IBCS2_ENODATA		61
#define IBCS2_ETIME		62
#define IBCS2_ENOSR		63
#define IBCS2_ENONET		64
#define IBCS2_ENOPKG		65
#define IBCS2_EREMOTE		66
#define IBCS2_ENOLINK		67
#define IBCS2_EADV		68
#define IBCS2_ESRMNT		69
#define IBCS2_ECOMM		70
#define IBCS2_EPROTO		71
#define IBCS2_EMULTIHOP		74
#define IBCS2_ELBIN		75
#define IBCS2_EDOTDOT		76
#define IBCS2_EBADMSG		77
#define IBCS2_ENAMETOOLONG	78
#define IBCS2_EOVERFLOW		79
#define IBCS2_ENOTUNIQ		80
#define IBCS2_EBADFD		81
#define IBCS2_EREMCHG		82
#define IBCS2_EILSEQ		88
#define IBCS2_ENOSYS		89

#if defined(_SCO_NET)			/* not strict iBCS2 */
#define IBCS2_EWOULDBLOCK	90
#define IBCS2_EINPROGRESS	91
#define IBCS2_EALREADY		92
#define IBCS2_ENOTSOCK		93
#define IBCS2_EDESTADDRREQ	94
#define IBCS2_EMSGSIZE		95
#define IBCS2_EPROTOTYPE	96
#define IBCS2_EPROTONOSUPPORT	97
#define IBCS2_ESOCKTNOSUPPORT	98
#define IBCS2_EOPNOTSUPP	99
#define IBCS2_EPFNOSUPPORT	100
#define IBCS2_EAFNOSUPPORT	101
#define IBCS2_EADDRINUSE	102
#define IBCS2_EADDRNOTAVAIL	103
#define IBCS2_ENETDOWN		104
#define IBCS2_ENETUNREACH	105
#define IBCS2_ENETRESET		106
#define IBCS2_ECONNABORTED	107
#define IBCS2_ECONNRESET	108
#define IBCS2_ENOBUFS		IBCS2_ENOSR
#define IBCS2_EISCONN		110
#define IBCS2_ENOTCONN		111
#define IBCS2_ESHUTDOWN		112
#define IBCS2_ETOOMANYREFS	113
#define IBCS2_ETIMEDOUT		114
#define IBCS2_ECONNREFUSED	115
#define IBCS2_EHOSTDOWN		116
#define IBCS2_EHOSTUNREACH	117
#define IBCS2_ENOPROTOOPT	118
#define IBCS2_ENOTEMPTY		145
#define IBCS2_ELOOP		150
#else
#define IBCS2_ELOOP		90
#define IBCS2_EWOULDBLOCK	90
#define IBCS2_ERESTART		91
#define IBCS2_ESTRPIPE		92
#define IBCS2_ENOTEMPTY		93
#define IBCS2_EUSERS		94
#endif

#define IBCS2_ESTALE		151
#define IBCS2_EIORESID		500

extern int bsd2ibcs_errno[];

#endif /* _IBCS2_ERRNO_H */
