/*-
 * Copyright (C) 1994, David Greenman
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (C) 2010 Konstantin Belousov <kib@freebsd.org>
 *
 * This code is derived from software contributed to Berkeley by
 * the University of Utah, and William Jolitz.
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

#include "opt_capsicum.h"
#include "opt_ktrace.h"
#include "opt_kdtrace.h"

__FBSDID("$FreeBSD$");

#include <sys/capability.h>
#include <sys/ktr.h>
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif
#include <security/audit/audit.h>

static inline int
syscallenter(struct thread *td, struct syscall_args *sa)
{
	struct proc *p;
	int error, traced;

	PCPU_INC(cnt.v_syscall);
	p = td->td_proc;

	td->td_pticks = 0;
	if (td->td_ucred != p->p_ucred)
		cred_update_thread(td);
	traced = (p->p_flag & P_TRACED) != 0;
	if (traced || td->td_dbgflags & TDB_USERWR) {
		PROC_LOCK(p);
		td->td_dbgflags &= ~TDB_USERWR;
		if (traced)
			td->td_dbgflags |= TDB_SCE;
		PROC_UNLOCK(p);
	}
	error = (p->p_sysent->sv_fetch_syscall_args)(td, sa);
#ifdef KTRACE
	if (KTRPOINT(td, KTR_SYSCALL))
		ktrsyscall(sa->code, sa->narg, sa->args);
#endif

	CTR6(KTR_SYSC,
"syscall: td=%p pid %d %s (%#lx, %#lx, %#lx)",
	    td, td->td_proc->p_pid, syscallname(p, sa->code),
	    sa->args[0], sa->args[1], sa->args[2]);

	if (error == 0) {
		STOPEVENT(p, S_SCE, sa->narg);
		if (p->p_flag & P_TRACED) {
			PROC_LOCK(p);
			td->td_dbg_sc_code = sa->code;
			td->td_dbg_sc_narg = sa->narg;
			if (p->p_stops & S_PT_SCE)
				ptracestop((td), SIGTRAP);
			PROC_UNLOCK(p);
		}
		if (td->td_dbgflags & TDB_USERWR) {
			/*
			 * Reread syscall number and arguments if
			 * debugger modified registers or memory.
			 */
			error = (p->p_sysent->sv_fetch_syscall_args)(td, sa);
			PROC_LOCK(p);
			td->td_dbg_sc_code = sa->code;
			td->td_dbg_sc_narg = sa->narg;
			PROC_UNLOCK(p);
#ifdef KTRACE
			if (KTRPOINT(td, KTR_SYSCALL))
				ktrsyscall(sa->code, sa->narg, sa->args);
#endif
			if (error != 0)
				goto retval;
		}

#ifdef CAPABILITY_MODE
		/*
		 * In capability mode, we only allow access to system calls
		 * flagged with SYF_CAPENABLED.
		 */
		if (IN_CAPABILITY_MODE(td) &&
		    !(sa->callp->sy_flags & SYF_CAPENABLED)) {
			error = ECAPMODE;
			goto retval;
		}
#endif

		error = syscall_thread_enter(td, sa->callp);
		if (error != 0)
			goto retval;

#ifdef KDTRACE_HOOKS
		/*
		 * If the systrace module has registered it's probe
		 * callback and if there is a probe active for the
		 * syscall 'entry', process the probe.
		 */
		if (systrace_probe_func != NULL && sa->callp->sy_entry != 0)
			(*systrace_probe_func)(sa->callp->sy_entry, sa->code,
			    sa->callp, sa->args, 0);
#endif

		AUDIT_SYSCALL_ENTER(sa->code, td);
		error = (sa->callp->sy_call)(td, sa->args);
		AUDIT_SYSCALL_EXIT(error, td);

		/* Save the latest error return value. */
		if ((td->td_pflags & TDP_NERRNO) == 0)
			td->td_errno = error;

#ifdef KDTRACE_HOOKS
		/*
		 * If the systrace module has registered it's probe
		 * callback and if there is a probe active for the
		 * syscall 'return', process the probe.
		 */
		if (systrace_probe_func != NULL && sa->callp->sy_return != 0)
			(*systrace_probe_func)(sa->callp->sy_return, sa->code,
			    sa->callp, NULL, (error) ? -1 : td->td_retval[0]);
#endif
		syscall_thread_exit(td, sa->callp);
		CTR4(KTR_SYSC, "syscall: p=%p error=%d return %#lx %#lx",
		    p, error, td->td_retval[0], td->td_retval[1]);
	}
 retval:
	if (traced) {
		PROC_LOCK(p);
		td->td_dbgflags &= ~TDB_SCE;
		PROC_UNLOCK(p);
	}
	(p->p_sysent->sv_set_syscall_retval)(td, error);
	return (error);
}

static inline void
syscallret(struct thread *td, int error, struct syscall_args *sa __unused)
{
	struct proc *p, *p2;
	int traced;

	p = td->td_proc;

	/*
	 * Check for misbehavior.
	 */
	WITNESS_WARN(WARN_PANIC, NULL, "System call %s returning",
	    syscallname(p, sa->code));
	KASSERT(td->td_critnest == 0,
	    ("System call %s returning in a critical section",
	    syscallname(p, sa->code)));
	KASSERT(td->td_locks == 0,
	    ("System call %s returning with %d locks held",
	     syscallname(p, sa->code), td->td_locks));
	KASSERT((td->td_pflags & TDP_NOFAULTING) == 0,
	    ("System call %s returning with pagefaults disabled",
	     syscallname(p, sa->code)));
	KASSERT((td->td_pflags & TDP_NOSLEEPING) == 0,
	    ("System call %s returning with sleep disabled",
	     syscallname(p, sa->code)));

	/*
	 * Handle reschedule and other end-of-syscall issues
	 */
	userret(td, td->td_frame);

	CTR4(KTR_SYSC, "syscall %s exit thread %p pid %d proc %s",
	    syscallname(p, sa->code), td, td->td_proc->p_pid, td->td_name);

#ifdef KTRACE
	if (KTRPOINT(td, KTR_SYSRET)) {
		ktrsysret(sa->code, (td->td_pflags & TDP_NERRNO) == 0 ?
		    error : td->td_errno, td->td_retval[0]);
	}
#endif
	td->td_pflags &= ~TDP_NERRNO;

	if (p->p_flag & P_TRACED) {
		traced = 1;
		PROC_LOCK(p);
		td->td_dbgflags |= TDB_SCX;
		PROC_UNLOCK(p);
	} else
		traced = 0;
	/*
	 * This works because errno is findable through the
	 * register set.  If we ever support an emulation where this
	 * is not the case, this code will need to be revisited.
	 */
	STOPEVENT(p, S_SCX, sa->code);
	if (traced || (td->td_dbgflags & (TDB_EXEC | TDB_FORK)) != 0) {
		PROC_LOCK(p);
		/*
		 * If tracing the execed process, trap to the debugger
		 * so that breakpoints can be set before the program
		 * executes.  If debugger requested tracing of syscall
		 * returns, do it now too.
		 */
		if (traced &&
		    ((td->td_dbgflags & (TDB_FORK | TDB_EXEC)) != 0 ||
		    (p->p_stops & S_PT_SCX) != 0))
			ptracestop(td, SIGTRAP);
		td->td_dbgflags &= ~(TDB_SCX | TDB_EXEC | TDB_FORK);
		PROC_UNLOCK(p);
	}

	if (td->td_pflags & TDP_RFPPWAIT) {
		/*
		 * Preserve synchronization semantics of vfork.  If
		 * waiting for child to exec or exit, fork set
		 * P_PPWAIT on child, and there we sleep on our proc
		 * (in case of exit).
		 *
		 * Do it after the ptracestop() above is finished, to
		 * not block our debugger until child execs or exits
		 * to finish vfork wait.
		 */
		td->td_pflags &= ~TDP_RFPPWAIT;
		p2 = td->td_rfppwait_p;
		PROC_LOCK(p2);
		while (p2->p_flag & P_PPWAIT)
			cv_wait(&p2->p_pwait, &p2->p_mtx);
		PROC_UNLOCK(p2);
	}
}
