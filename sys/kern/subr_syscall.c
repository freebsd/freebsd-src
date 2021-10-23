/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
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

__FBSDID("$FreeBSD$");

#include <sys/capsicum.h>
#include <sys/ktr.h>
#include <sys/vmmeter.h>
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif
#include <security/audit/audit.h>

static inline void
syscallenter(struct thread *td)
{
	struct proc *p;
	struct syscall_args *sa;
	struct sysent *se;
	int error, traced;
	bool sy_thr_static;

	VM_CNT_INC(v_syscall);
	p = td->td_proc;
	sa = &td->td_sa;

	td->td_pticks = 0;
	if (__predict_false(td->td_cowgen != p->p_cowgen))
		thread_cow_update(td);
	traced = (p->p_flag & P_TRACED) != 0;
	if (__predict_false(traced || td->td_dbgflags & TDB_USERWR)) {
		PROC_LOCK(p);
		td->td_dbgflags &= ~TDB_USERWR;
		if (traced)
			td->td_dbgflags |= TDB_SCE;
		PROC_UNLOCK(p);
	}
	error = (p->p_sysent->sv_fetch_syscall_args)(td);
	se = sa->callp;
#ifdef KTRACE
	if (KTRPOINT(td, KTR_SYSCALL))
		ktrsyscall(sa->code, se->sy_narg, sa->args);
#endif
	KTR_START4(KTR_SYSC, "syscall", syscallname(p, sa->code),
	    (uintptr_t)td, "pid:%d", td->td_proc->p_pid, "arg0:%p", sa->args[0],
	    "arg1:%p", sa->args[1], "arg2:%p", sa->args[2]);

	if (__predict_false(error != 0)) {
		td->td_errno = error;
		goto retval;
	}

	if (__predict_false(traced)) {
		PROC_LOCK(p);
		if (p->p_ptevents & PTRACE_SCE)
			ptracestop((td), SIGTRAP, NULL);
		PROC_UNLOCK(p);

		if ((td->td_dbgflags & TDB_USERWR) != 0) {
			/*
			 * Reread syscall number and arguments if debugger
			 * modified registers or memory.
			 */
			error = (p->p_sysent->sv_fetch_syscall_args)(td);
			se = sa->callp;
#ifdef KTRACE
			if (KTRPOINT(td, KTR_SYSCALL))
				ktrsyscall(sa->code, se->sy_narg, sa->args);
#endif
			if (error != 0) {
				td->td_errno = error;
				goto retval;
			}
		}
	}

#ifdef CAPABILITY_MODE
	/*
	 * In capability mode, we only allow access to system calls
	 * flagged with SYF_CAPENABLED.
	 */
	if (__predict_false(IN_CAPABILITY_MODE(td) &&
	    (se->sy_flags & SYF_CAPENABLED) == 0)) {
		td->td_errno = error = ECAPMODE;
		goto retval;
	}
#endif

	/*
	 * Fetch fast sigblock value at the time of syscall entry to
	 * handle sleepqueue primitives which might call cursig().
	 */
	if (__predict_false(sigfastblock_fetch_always))
		(void)sigfastblock_fetch(td);

	/* Let system calls set td_errno directly. */
	KASSERT((td->td_pflags & TDP_NERRNO) == 0,
	    ("%s: TDP_NERRNO set", __func__));

	sy_thr_static = (se->sy_thrcnt & SY_THR_STATIC) != 0;

	if (__predict_false(SYSTRACE_ENABLED() ||
	    AUDIT_SYSCALL_ENTER(sa->code, td) ||
	    !sy_thr_static)) {
		if (!sy_thr_static) {
			error = syscall_thread_enter(td, se);
			if (error != 0) {
				td->td_errno = error;
				goto retval;
			}
		}

#ifdef KDTRACE_HOOKS
		/* Give the syscall:::entry DTrace probe a chance to fire. */
		if (__predict_false(se->sy_entry != 0))
			(*systrace_probe_func)(sa, SYSTRACE_ENTRY, 0);
#endif
		error = (se->sy_call)(td, sa->args);
		/* Save the latest error return value. */
		if (__predict_false((td->td_pflags & TDP_NERRNO) != 0))
			td->td_pflags &= ~TDP_NERRNO;
		else
			td->td_errno = error;

		/*
		 * Note that some syscall implementations (e.g., sys_execve)
		 * will commit the audit record just before their final return.
		 * These were done under the assumption that nothing of interest
		 * would happen between their return and here, where we would
		 * normally commit the audit record.  These assumptions will
		 * need to be revisited should any substantial logic be added
		 * above.
		 */
		AUDIT_SYSCALL_EXIT(error, td);

#ifdef KDTRACE_HOOKS
		/* Give the syscall:::return DTrace probe a chance to fire. */
		if (__predict_false(se->sy_return != 0))
			(*systrace_probe_func)(sa, SYSTRACE_RETURN,
			    error ? -1 : td->td_retval[0]);
#endif

		if (!sy_thr_static)
			syscall_thread_exit(td, se);
	} else {
		error = (se->sy_call)(td, sa->args);
		/* Save the latest error return value. */
		if (__predict_false((td->td_pflags & TDP_NERRNO) != 0))
			td->td_pflags &= ~TDP_NERRNO;
		else
			td->td_errno = error;
	}

 retval:
	KTR_STOP4(KTR_SYSC, "syscall", syscallname(p, sa->code),
	    (uintptr_t)td, "pid:%d", td->td_proc->p_pid, "error:%d", error,
	    "retval0:%#lx", td->td_retval[0], "retval1:%#lx",
	    td->td_retval[1]);
	if (__predict_false(traced)) {
		PROC_LOCK(p);
		td->td_dbgflags &= ~TDB_SCE;
		PROC_UNLOCK(p);
	}
	(p->p_sysent->sv_set_syscall_retval)(td, error);
}

static inline void
syscallret(struct thread *td)
{
	struct proc *p;
	struct syscall_args *sa;
	ksiginfo_t ksi;
	int traced;

	KASSERT(td->td_errno != ERELOOKUP,
	    ("ERELOOKUP not consumed syscall %d", td->td_sa.code));

	p = td->td_proc;
	sa = &td->td_sa;
	if (__predict_false(td->td_errno == ENOTCAPABLE ||
	    td->td_errno == ECAPMODE)) {
		if ((trap_enotcap ||
		    (p->p_flag2 & P2_TRAPCAP) != 0) && IN_CAPABILITY_MODE(td)) {
			ksiginfo_init_trap(&ksi);
			ksi.ksi_signo = SIGTRAP;
			ksi.ksi_errno = td->td_errno;
			ksi.ksi_code = TRAP_CAP;
			ksi.ksi_info.si_syscall = sa->original_code;
			trapsignal(td, &ksi);
		}
	}

	/*
	 * Handle reschedule and other end-of-syscall issues
	 */
	userret(td, td->td_frame);

#ifdef KTRACE
	if (KTRPOINT(td, KTR_SYSRET)) {
		ktrsysret(sa->code, td->td_errno, td->td_retval[0]);
	}
#endif

	traced = 0;
	if (__predict_false(p->p_flag & P_TRACED)) {
		traced = 1;
		PROC_LOCK(p);
		td->td_dbgflags |= TDB_SCX;
		PROC_UNLOCK(p);
	}
	if (__predict_false(traced ||
	    (td->td_dbgflags & (TDB_EXEC | TDB_FORK)) != 0)) {
		PROC_LOCK(p);
		/*
		 * Linux debuggers expect an additional stop for exec,
		 * between the usual syscall entry and exit.  Raise
		 * the exec event now and then clear TDB_EXEC so that
		 * the next stop is reported as a syscall exit by
		 * linux_ptrace_status().
		 */
		if ((td->td_dbgflags & TDB_EXEC) != 0 &&
		    SV_PROC_ABI(td->td_proc) == SV_ABI_LINUX) {
			ptracestop(td, SIGTRAP, NULL);
			td->td_dbgflags &= ~TDB_EXEC;
		}
		/*
		 * If tracing the execed process, trap to the debugger
		 * so that breakpoints can be set before the program
		 * executes.  If debugger requested tracing of syscall
		 * returns, do it now too.
		 */
		if (traced &&
		    ((td->td_dbgflags & (TDB_FORK | TDB_EXEC)) != 0 ||
		    (p->p_ptevents & PTRACE_SCX) != 0))
			ptracestop(td, SIGTRAP, NULL);
		td->td_dbgflags &= ~(TDB_SCX | TDB_EXEC | TDB_FORK);
		PROC_UNLOCK(p);
	}

	if (__predict_false(td->td_pflags & TDP_RFPPWAIT))
		fork_rfppwait(td);
}
