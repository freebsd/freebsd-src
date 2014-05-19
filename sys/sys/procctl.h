/*-
 * Copyright (c) 2013 Advanced Computing Technologies LLC
 * Written by: John H. Baldwin <jhb@FreeBSD.org>
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

#ifndef	_SYS_PROCCTL_H_
#define	_SYS_PROCCTL_H_

#define	PROC_SPROTECT		1	/* set protected state */

/* Operations for PROC_SPROTECT (passed in integer arg). */
#define	PPROT_OP(x)	((x) & 0xf)
#define	PPROT_SET	1
#define	PPROT_CLEAR	2

/* Flags for PROC_SPROTECT (ORed in with operation). */
#define	PPROT_FLAGS(x)	((x) & ~0xf)
#define	PPROT_DESCEND	0x10
#define	PPROT_INHERIT	0x20

#ifndef _KERNEL
#include <sys/types.h>
#include <sys/wait.h>

__BEGIN_DECLS
int	procctl(idtype_t, id_t, int, void *);
__END_DECLS

#endif

#endif /* !_SYS_PROCCTL_H_ */
