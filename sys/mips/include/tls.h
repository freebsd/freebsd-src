/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2012 Oleksandr Tymoshenko
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 * 
 */

#ifndef	__MIPS_TLS_H__
#define	__MIPS_TLS_H__

#include <sys/_tls_variant_i.h>
#include <machine/sysarch.h>

/*
 * TLS parameters
 */

#define	TLS_DTV_OFFSET	0x8000
#define	TLS_TCB_ALIGN	8
#define	TLS_TP_OFFSET	0x7000

#ifdef COMPAT_FREEBSD32
#define	TLS_TCB_SIZE32	8
#endif

#ifndef _KERNEL

static __inline void
_tcb_set(struct tcb *tcb)
{
	sysarch(MIPS_SET_TLS, tcb);
}

static __inline struct tcb *
_tcb_get(void)
{
	struct tcb *tcb;

#ifdef TLS_USE_SYSARCH
	sysarch(MIPS_GET_TLS, &tcb);
#else
	__asm__ __volatile__ (
	    ".set\tpush\n\t"
#ifdef __mips_n64
	    ".set\tmips64r2\n\t"
#else
	    ".set\tmips32r2\n\t"
#endif
	    "rdhwr\t%0, $29\n\t"
	    ".set\tpop"
	    : "=r" (tcb));
	tcb = (struct tcb *)((uintptr_t)tcb - TLS_TP_OFFSET - TLS_TCB_SIZE);
#endif
	return (tcb);
}

#endif

#endif	/* __MIPS_TLS_H__ */
