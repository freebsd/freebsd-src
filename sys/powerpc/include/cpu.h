/*
 * Copyright (C) 1995-1997 Wolfgang Solfrank.
 * Copyright (C) 1995-1997 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$NetBSD: cpu.h,v 1.11 2000/05/26 21:19:53 thorpej Exp $
 * $FreeBSD$
 */

#ifndef _MACHINE_CPU_H_
#define	_MACHINE_CPU_H_

#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/psl.h>

#define	CLKF_USERMODE(frame)	(((frame)->srr1 & PSL_PR) != 0)
#define	CLKF_BASEPRI(frame)	((frame)->pri == 0)
#define	CLKF_PC(frame)		((frame)->srr0)
#define	CLKF_INTR(frame)	((frame)->depth > 0)

#define	TRAPF_USERMODE(frame)	((frame)->srr1 & PSL_PR) != 0)
#define	TRAPF_PC(frame)		((frame)->srr0)

#define	cpu_swapout(p)
#define	cpu_number()		0

extern void delay __P((unsigned));
#define	DELAY(n)		delay(n)

extern int want_resched;
extern int astpending;

#define	need_proftick(p)	((p)->p_flag |= PS_OWEUPC, astpending = 1)

extern char bootpath[];

#if defined(_KERNEL) || defined(_STANDALONE)
#define	CACHELINESIZE	32
#endif

extern void __syncicache __P((void *, int));

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_CACHELINE	1
#define	CPU_MAXID	2
#define CPU_CONSDEV	1

#define	CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "cachelinesize", CTLTYPE_INT }, \
}

static __inline u_int64_t
get_cyclecount(void)
{
	u_int32_t upper, lower;
	u_int64_t time;

	__asm __volatile(
		"mftb %0\n"
		"mftbu %1"
		: "=r" (lower), "=r" (upper));

	time = (u_int64_t)upper;
	time = (time << 32) + lower;
	return (time);
}

#define	cpu_getstack(p)		((p)->p_frame->fixreg[1])

void	savectx __P((struct pcb *));

#endif	/* _MACHINE_CPU_H_ */
