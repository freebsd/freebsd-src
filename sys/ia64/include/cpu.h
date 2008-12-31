/* $FreeBSD: src/sys/ia64/include/cpu.h,v 1.49.6.1 2008/11/25 02:59:29 kensmith Exp $ */
/* From: NetBSD: cpu.h,v 1.18 1997/09/23 23:17:49 mjacob Exp */

/*-
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: cpu.h 1.16 91/03/25$
 *
 *	@(#)cpu.h	8.4 (Berkeley) 1/5/94
 */

#ifndef _MACHINE_CPU_H_
#define _MACHINE_CPU_H_

#include <machine/frame.h>

#define	TRAPF_PC(tf)		((tf)->tf_special.iip)
#define	TRAPF_CPL(tf)		((tf)->tf_special.psr & IA64_PSR_CPL)
#define	TRAPF_USERMODE(tf)	(TRAPF_CPL(tf) != IA64_PSR_CPL_KERN)

#ifdef _KERNEL

#ifdef GPROF
extern char btext[];
extern char etext[];
#endif

/*
 * Return contents of in-cpu fast counter as a sort of "bogo-time"
 * for non-critical timing.
 */
#define	get_cyclecount		ia64_get_itc

/* Used by signaling code. */
#define	cpu_getstack(td)	((td)->td_frame->tf_special.sp)
#define	cpu_spinwait()		/* nothing */

void	cpu_halt(void);
void	cpu_reset(void);
void	fork_trampoline(void);				/* MAGIC */
void	swi_vm(void *);

#endif /* _KERNEL */

#endif /* _MACHINE_CPU_H_ */
