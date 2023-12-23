/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sysctl.h>

#include <machine/cpu.h>

/*
 * Profiling system call.
 *
 * The scale factor is a fixed point number with 16 bits of fraction, so that
 * 1.0 is represented as 0x10000.  A scale factor of 0 turns off profiling.
 */
#ifndef _SYS_SYSPROTO_H_
struct profil_args {
	caddr_t	samples;
	size_t	size;
	size_t	offset;
	u_int	scale;
};
#endif
/* ARGSUSED */
int
sys_profil(struct thread *td, struct profil_args *uap)
{
	struct uprof *upp;
	struct proc *p;

	if (uap->scale > (1 << 16))
		return (EINVAL);

	p = td->td_proc;
	if (uap->scale == 0) {
		PROC_LOCK(p);
		stopprofclock(p);
		PROC_UNLOCK(p);
		return (0);
	}
	PROC_LOCK(p);
	upp = &td->td_proc->p_stats->p_prof;
	PROC_PROFLOCK(p);
	upp->pr_off = uap->offset;
	upp->pr_scale = uap->scale;
	upp->pr_base = uap->samples;
	upp->pr_size = uap->size;
	PROC_PROFUNLOCK(p);
	startprofclock(p);
	PROC_UNLOCK(p);

	return (0);
}

/*
 * Scale is a fixed-point number with the binary point 16 bits
 * into the value, and is <= 1.0.  pc is at most 32 bits, so the
 * intermediate result is at most 48 bits.
 */
#define	PC_TO_INDEX(pc, prof) \
	((int)(((u_quad_t)((pc) - (prof)->pr_off) * \
	    (u_quad_t)((prof)->pr_scale)) >> 16) & ~1)

/*
 * Collect user-level profiling statistics; called on a profiling tick,
 * when a process is running in user-mode.  This routine may be called
 * from an interrupt context.  We perform the update with an AST
 * that will vector us to trap() with a context in which copyin and
 * copyout will work.  Trap will then call addupc_task().
 *
 * Note that we may (rarely) not get around to the AST soon enough, and
 * lose profile ticks when the next tick overwrites this one, but in this
 * case the system is overloaded and the profile is probably already
 * inaccurate.
 */
void
addupc_intr(struct thread *td, uintfptr_t pc, u_int ticks)
{
	struct uprof *prof;

	if (ticks == 0)
		return;
	prof = &td->td_proc->p_stats->p_prof;
	PROC_PROFLOCK(td->td_proc);
	if (pc < prof->pr_off || PC_TO_INDEX(pc, prof) >= prof->pr_size) {
		PROC_PROFUNLOCK(td->td_proc);
		return;			/* out of range; ignore */
	}

	PROC_PROFUNLOCK(td->td_proc);
	td->td_profil_addr = pc;
	td->td_profil_ticks = ticks;
	td->td_pflags |= TDP_OWEUPC;
	ast_sched(td, TDA_OWEUPC);
}

/*
 * Actually update the profiling statistics.  If the update fails, we
 * simply turn off profiling.
 */
void
addupc_task(struct thread *td, uintfptr_t pc, u_int ticks)
{
	struct proc *p = td->td_proc; 
	struct uprof *prof;
	caddr_t addr;
	u_int i;
	u_short v;
	int stop = 0;

	if (ticks == 0)
		return;

	PROC_LOCK(p);
	if (!(p->p_flag & P_PROFIL)) {
		PROC_UNLOCK(p);
		return;
	}
	p->p_profthreads++;
	prof = &p->p_stats->p_prof;
	PROC_PROFLOCK(p);
	if (pc < prof->pr_off ||
	    (i = PC_TO_INDEX(pc, prof)) >= prof->pr_size) {
		PROC_PROFUNLOCK(p);
		goto out;
	}

	addr = prof->pr_base + i;
	PROC_PROFUNLOCK(p);
	PROC_UNLOCK(p);
	if (copyin(addr, &v, sizeof(v)) == 0) {
		v += ticks;
		if (copyout(&v, addr, sizeof(v)) == 0) {
			PROC_LOCK(p);
			goto out;
		}
	}
	stop = 1;
	PROC_LOCK(p);

out:
	if (--p->p_profthreads == 0) {
		if (p->p_flag & P_STOPPROF) {
			wakeup(&p->p_profthreads);
			p->p_flag &= ~P_STOPPROF;
			stop = 0;
		}
	}
	if (stop)
		stopprofclock(p);
	PROC_UNLOCK(p);
}
