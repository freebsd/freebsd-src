/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (C) 1994, David Greenman
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2007, 2022 The FreeBSD Foundation
 *
 * This code is derived from software contributed to Berkeley by
 * the University of Utah, and William Jolitz.
 *
 * Portions of this software were developed by A. Joseph Koshy under
 * sponsorship from the FreeBSD Foundation and Google, Inc.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	from: @(#)trap.c	7.4 (Berkeley) 5/13/91
 */

#include <sys/cdefs.h>
#include "opt_hwpmc_hooks.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/msan.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/ktr.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/syscall.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/systm.h>
#include <sys/vmmeter.h>

#include <machine/cpu.h>

#ifdef VIMAGE
#include <net/vnet.h>
#endif

#ifdef	HWPMC_HOOKS
#include <sys/pmckern.h>
#endif

#ifdef EPOCH_TRACE
#include <sys/epoch.h>
#endif

volatile uint32_t __read_frequently hpts_that_need_softclock = 0;

void	(*tcp_hpts_softclock)(void);

/*
 * Define the code needed before returning to user mode, for trap and
 * syscall.
 */
void
userret(struct thread *td, struct trapframe *frame)
{
	struct proc *p = td->td_proc;

	CTR3(KTR_SYSC, "userret: thread %p (pid %d, %s)", td, p->p_pid,
            td->td_name);
	KASSERT((p->p_flag & P_WEXIT) == 0,
	    ("Exiting process returns to usermode"));
#ifdef DIAGNOSTIC
	/*
	 * Check that we called signotify() enough.  For
	 * multi-threaded processes, where signal distribution might
	 * change due to other threads changing sigmask, the check is
	 * racy and cannot be performed reliably.
	 * If current process is vfork child, indicated by P_PPWAIT, then
	 * issignal() ignores stops, so we block the check to avoid
	 * classifying pending signals.
	 */
	if (p->p_numthreads == 1) {
		PROC_LOCK(p);
		thread_lock(td);
		if ((p->p_flag & P_PPWAIT) == 0 &&
		    (td->td_pflags & TDP_SIGFASTBLOCK) == 0 &&
		    SIGPENDING(td) && !td_ast_pending(td, TDA_AST) &&
		    !td_ast_pending(td, TDA_SIG)) {
			thread_unlock(td);
			panic(
			    "failed to set signal flags for ast p %p "
			    "td %p td_ast %#x fl %#x",
			    p, td, td->td_ast, td->td_flags);
		}
		thread_unlock(td);
		PROC_UNLOCK(p);
	}
#endif

	/*
	 * Charge system time if profiling.
	 */
	if (__predict_false(p->p_flag & P_PROFIL))
		addupc_task(td, TRAPF_PC(frame), td->td_pticks * psratio);

#ifdef HWPMC_HOOKS
	if (PMC_THREAD_HAS_SAMPLES(td))
		PMC_CALL_HOOK(td, PMC_FN_THR_USERRET, NULL);
#endif
	/*
	 * Calling tcp_hpts_softclock() here allows us to avoid frequent,
	 * expensive callouts that trash the cache and lead to a much higher
	 * number of interrupts and context switches.  Testing on busy web
	 * servers at Netflix has shown that this improves CPU use by 7% over
	 * relying only on callouts to drive HPTS, and also results in idle
	 * power savings on mostly idle servers.
	 * This was inspired by the paper "Soft Timers: Efficient Microsecond
	 * Software Timer Support for Network Processing"
	 * by Mohit Aron and Peter Druschel.
	 */
	tcp_hpts_softclock();
	/*
	 * Let the scheduler adjust our priority etc.
	 */
	sched_userret(td);

	/*
	 * Check for misbehavior.
	 *
	 * In case there is a callchain tracing ongoing because of
	 * hwpmc(4), skip the scheduler pinning check.
	 * hwpmc(4) subsystem, infact, will collect callchain informations
	 * at ast() checkpoint, which is past userret().
	 */
	WITNESS_WARN(WARN_PANIC, NULL, "userret: returning");
	KASSERT(td->td_critnest == 0,
	    ("userret: Returning in a critical section"));
	KASSERT(td->td_locks == 0,
	    ("userret: Returning with %d locks held", td->td_locks));
	KASSERT(td->td_rw_rlocks == 0,
	    ("userret: Returning with %d rwlocks held in read mode",
	    td->td_rw_rlocks));
	KASSERT(td->td_sx_slocks == 0,
	    ("userret: Returning with %d sx locks held in shared mode",
	    td->td_sx_slocks));
	KASSERT(td->td_lk_slocks == 0,
	    ("userret: Returning with %d lockmanager locks held in shared mode",
	    td->td_lk_slocks));
	KASSERT((td->td_pflags & TDP_NOFAULTING) == 0,
	    ("userret: Returning with pagefaults disabled"));
	if (__predict_false(!THREAD_CAN_SLEEP())) {
#ifdef EPOCH_TRACE
		epoch_trace_list(curthread);
#endif
		KASSERT(0, ("userret: Returning with sleep disabled"));
	}
	KASSERT(td->td_pinned == 0 || (td->td_pflags & TDP_CALLCHAIN) != 0,
	    ("userret: Returning with pinned thread"));
	KASSERT(td->td_vp_reserved == NULL,
	    ("userret: Returning with preallocated vnode"));
	KASSERT((td->td_flags & (TDF_SBDRY | TDF_SEINTR | TDF_SERESTART)) == 0,
	    ("userret: Returning with stop signals deferred"));
	KASSERT(td->td_vslock_sz == 0,
	    ("userret: Returning with vslock-wired space"));
#ifdef VIMAGE
	/* Unfortunately td_vnet_lpush needs VNET_DEBUG. */
	VNET_ASSERT(curvnet == NULL,
	    ("%s: Returning on td %p (pid %d, %s) with vnet %p set in %s",
	    __func__, td, p->p_pid, td->td_name, curvnet,
	    (td->td_vnet_lpush != NULL) ? td->td_vnet_lpush : "N/A"));
#endif
}

static void
ast_prep(struct thread *td, int tda __unused)
{
	VM_CNT_INC(v_trap);
	td->td_pticks = 0;
	if (td->td_cowgen != atomic_load_int(&td->td_proc->p_cowgen))
		thread_cow_update(td);

}

struct ast_entry {
	int	ae_flags;
	int	ae_tdp;
	void	(*ae_f)(struct thread *td, int ast);
};

_Static_assert(TDAI(TDA_MAX) <= UINT_MAX, "Too many ASTs");

static struct ast_entry ast_entries[TDA_MAX] __read_mostly = {
	[TDA_AST] = { .ae_f = ast_prep, .ae_flags = ASTR_UNCOND},
};

void
ast_register(int ast, int flags, int tdp,
    void (*f)(struct thread *, int asts))
{
	struct ast_entry *ae;

	MPASS(ast < TDA_MAX);
	MPASS((flags & ASTR_TDP) == 0 || ((flags & ASTR_ASTF_REQUIRED) != 0
	    && __bitcount(tdp) == 1));
	ae = &ast_entries[ast];
	MPASS(ae->ae_f == NULL);
	ae->ae_flags = flags;
	ae->ae_tdp = tdp;
	atomic_interrupt_fence();
	ae->ae_f = f;
}

/*
 * XXXKIB Note that the deregistration of an AST handler does not
 * drain threads possibly executing it, which affects unloadable
 * modules.  The issue is either handled by the subsystem using
 * handlers, or simply ignored.  Fixing the problem is considered not
 * worth the overhead.
 */
void
ast_deregister(int ast)
{
	struct ast_entry *ae;

	MPASS(ast < TDA_MAX);
	ae = &ast_entries[ast];
	MPASS(ae->ae_f != NULL);
	ae->ae_f = NULL;
	atomic_interrupt_fence();
	ae->ae_flags = 0;
	ae->ae_tdp = 0;
}

void
ast_sched_locked(struct thread *td, int tda)
{
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	MPASS(tda < TDA_MAX);

	td->td_ast |= TDAI(tda);
}

void
ast_unsched_locked(struct thread *td, int tda)
{
	THREAD_LOCK_ASSERT(td, MA_OWNED);
	MPASS(tda < TDA_MAX);

	td->td_ast &= ~TDAI(tda);
}

void
ast_sched(struct thread *td, int tda)
{
	thread_lock(td);
	ast_sched_locked(td, tda);
	thread_unlock(td);
}

void
ast_sched_mask(struct thread *td, int ast)
{
	thread_lock(td);
	td->td_ast |= ast;
	thread_unlock(td);
}

static bool
ast_handler_calc_tdp_run(struct thread *td, const struct ast_entry *ae)
{
	return ((ae->ae_flags & ASTR_TDP) == 0 ||
	    (td->td_pflags & ae->ae_tdp) != 0);
}

/*
 * Process an asynchronous software trap.
 */
static void
ast_handler(struct thread *td, struct trapframe *framep, bool dtor)
{
	struct ast_entry *ae;
	void (*f)(struct thread *td, int asts);
	int a, td_ast;
	bool run;

	if (framep != NULL) {
		kmsan_mark(framep, sizeof(*framep), KMSAN_STATE_INITED);
		td->td_frame = framep;
	}

	if (__predict_true(!dtor)) {
		WITNESS_WARN(WARN_PANIC, NULL, "Returning to user mode");
		mtx_assert(&Giant, MA_NOTOWNED);
		THREAD_LOCK_ASSERT(td, MA_NOTOWNED);

		/*
		 * This updates the td_ast for the checks below in one
		 * atomic operation with turning off all scheduled AST's.
		 * If another AST is triggered while we are handling the
		 * AST's saved in td_ast, the td_ast is again non-zero and
		 * ast() will be called again.
		 */
		thread_lock(td);
		td_ast = td->td_ast;
		td->td_ast = 0;
		thread_unlock(td);
	} else {
		/*
		 * The td thread's td_lock is not guaranteed to exist,
		 * the thread might be not initialized enough when it's
		 * destructor is called.  It is safe to read and
		 * update td_ast without locking since the thread is
		 * not runnable or visible to other threads.
		 */
		td_ast = td->td_ast;
		td->td_ast = 0;
	}

	CTR3(KTR_SYSC, "ast: thread %p (pid %d, %s)", td, td->td_proc->p_pid,
            td->td_proc->p_comm);
	KASSERT(framep == NULL || TRAPF_USERMODE(framep),
	    ("ast in kernel mode"));

	for (a = 0; a < nitems(ast_entries); a++) {
		ae = &ast_entries[a];
		f = ae->ae_f;
		if (f == NULL)
			continue;
		atomic_interrupt_fence();

		run = false;
		if (__predict_false(framep == NULL)) {
			if ((ae->ae_flags & ASTR_KCLEAR) != 0)
				run = ast_handler_calc_tdp_run(td, ae);
		} else {
			if ((ae->ae_flags & ASTR_UNCOND) != 0)
				run = true;
			else if ((ae->ae_flags & ASTR_ASTF_REQUIRED) != 0 &&
			    (td_ast & TDAI(a)) != 0)
				run = ast_handler_calc_tdp_run(td, ae);
		}
		if (run)
			f(td, td_ast);
	}
}

void
ast(struct trapframe *framep)
{
	struct thread *td;

	td = curthread;
	ast_handler(td, framep, false);
	userret(td, framep);
}

void
ast_kclear(struct thread *td)
{
	ast_handler(td, NULL, td != curthread);
}

const char *
syscallname(struct proc *p, u_int code)
{
	static const char unknown[] = "unknown";
	struct sysentvec *sv;

	sv = p->p_sysent;
	if (sv->sv_syscallnames == NULL || code >= sv->sv_size)
		return (unknown);
	return (sv->sv_syscallnames[code]);
}
