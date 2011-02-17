/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	from: @(#)cpu.h 5.4 (Berkeley) 5/9/91
 *	from: FreeBSD: src/sys/i386/include/cpu.h,v 1.62 2001/06/29
 * $FreeBSD$
 */

#ifndef	_MACHINE_CPU_H_
#define	_MACHINE_CPU_H_

#include <machine/frame.h>
#include <machine/tstate.h>
extern void cpu_yield(void);

#define	TRAPF_PC(tfp)		((tfp)->tf_tpc)
#define	TRAPF_USERMODE(tfp)	(((tfp)->tf_tstate & TSTATE_PRIV) == 0)

#define	cpu_getstack(td)	((td)->td_frame->tf_sp)
#define	cpu_setstack(td, sp)	((td)->td_frame->tf_sp = (sp))
#define	cpu_spinwait()          

#ifdef _KERNEL

extern	char btext[];
extern	char etext[];

void	cpu_halt(void);
void	cpu_reset(void);
void	fork_trampoline(void);
void	swi_vm(void *v);

static __inline u_int64_t
get_cyclecount(void)
{

	return (rd(tick));
}

#define UNIMPLEMENTED panic("%s not implemented", __FUNCTION__)

#define likely(x)  __builtin_expect((x),1)
#define unlikely(x)  __builtin_expect((x),0)
#endif

#endif /* !_MACHINE_CPU_H_ */
