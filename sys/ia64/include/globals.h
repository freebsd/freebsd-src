/*-
 * Copyright (c) 1999 Luoqi Chen <luoqi@freebsd.org>
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

#ifndef	_MACHINE_GLOBALS_H_
#define	_MACHINE_GLOBALS_H_

#ifdef _KERNEL

register struct globaldata *globalp __asm__("r13");

#if 1
#define GLOBALP	globalp
#else
#define GLOBALP	((struct globaldata *) alpha_pal_rdval())
#endif

#define	PCPU_GET(name)		(GLOBALP->gd_##name)
#define PCPU_SET(name,value)	(GLOBALP->gd_##name = (value))

/*
 * The following set of macros works for UP kernel as well, but for maximum
 * performance we allow the global variables to be accessed directly. On the
 * other hand, kernel modules should always use these macros to maintain
 * portability between UP and SMP kernels.
 */
#define	CURPROC		PCPU_GET(curproc)
#define	curproc		PCPU_GET(curproc)
#define	idleproc	PCPU_GET(idleproc)
#define	curpcb		PCPU_GET(curpcb)
#define	switchtime	PCPU_GET(switchtime)
#define	switchticks	PCPU_GET(switchticks)
#define cpuid		PCPU_GET(cpuno)
#define	prevproc	PCPU_GET(curproc)	/* XXX - until ithreads */

#endif	/* _KERNEL */

#endif	/* !_MACHINE_GLOBALS_H_ */
