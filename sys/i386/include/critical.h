/*-
 * Copyright (c) 2002 Matthew Dillon.  All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file contains prototypes and high-level inlines related to
 * machine-level critical function support:
 *
 *	cpu_critical_enter()		- inlined
 *	cpu_critical_exit()		- inlined
 *	cpu_critical_fork_exit()	- prototyped
 *	related support functions residing
 *	in <arch>/<arch>/critical.c	- prototyped
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_CRITICAL_H_
#define	_MACHINE_CRITICAL_H_

__BEGIN_DECLS

/*
 * Prototypes - see <arch>/<arch>/critical.c
 */
void cpu_critical_fork_exit(void);

#if defined(__GNUC__) || defined(__INTEL_COMPILER)

/*
 *	cpu_critical_enter:
 *
 *	This routine is called from critical_enter() on the 0->1 transition
 *	of td_critnest, prior to it being incremented to 1.
 *
 *	If new-style critical section handling we do not have to do anything.
 *	However, as a side effect any interrupts occuring while td_critnest
 *	is non-zero will be deferred.
 */
static __inline void
cpu_critical_enter(struct thread *td)
{
	td->td_md.md_savecrit = intr_disable();
}

/*
 *	cpu_critical_exit:
 *
 *	This routine is called from critical_exit() on a 1->0 transition
 *	of td_critnest, after it has been decremented to 0.  We are
 *	exiting the last critical section.
 *
 *	Note that the td->critnest (1->0) transition interrupt race against
 *	our int_pending/unpend() check below is handled by the interrupt
 *	code for us, so we do not have to do anything fancy.
 */
static __inline void
cpu_critical_exit(struct thread *td)
{
	intr_restore(td->td_md.md_savecrit);
}

#else /* !(__GNUC__ || __INTEL_COMPILER) */

void cpu_critical_enter(struct thread *td);
void cpu_critical_exit(struct thread *td);

#endif        /* __GNUC__ || __INTEL_COMPILER */

__END_DECLS

#endif /* !_MACHINE_CRITICAL_H_ */

