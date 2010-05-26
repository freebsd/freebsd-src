/*-
 * Copyright (C) 1994, David Greenman
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2007 The FreeBSD Foundation
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
__FBSDID("$FreeBSD$");

#include "opt_ktrace.h"
#ifdef __i386__
#include "opt_npx.h"
#endif
#include "opt_sched.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pmckern.h>
#include <sys/proc.h>
#include <sys/ktr.h>
#include <sys/pioctl.h>
#include <sys/ptrace.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/systm.h>
#include <sys/vmmeter.h>
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif
#include <security/audit/audit.h>

#include <machine/cpu.h>
#include <machine/pcb.h>

#ifdef XEN
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#endif

#include <security/mac/mac_framework.h>

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
#if 0
#ifdef DIAGNOSTIC
	/* Check that we called signotify() enough. */
	PROC_LOCK(p);
	thread_lock(td);
	if (SIGPENDING(td) && ((td->td_flags & TDF_NEEDSIGCHK) == 0 ||
	    (td->td_flags & TDF_ASTPENDING) == 0))
		printf("failed to set signal flags properly for ast()\n");
	thread_unlock(td);
	PROC_UNLOCK(p);
#endif
#endif
#ifdef KTRACE
	KTRUSERRET(td);
#endif
	/*
	 * If this thread tickled GEOM, we need to wait for the giggling to
	 * stop before we return to userland
	 */
	if (td->td_pflags & TDP_GEOM)
		g_waitidle();

	/*
	 * Charge system time if profiling.
	 */
	if (p->p_flag & P_PROFIL) {
		addupc_task(td, TRAPF_PC(frame), td->td_pticks * psratio);
	}
	/*
	 * Let the scheduler adjust our priority etc.
	 */
	sched_userret(td);
	KASSERT(td->td_locks == 0,
	    ("userret: Returning with %d locks held.", td->td_locks));
#ifdef XEN
	PT_UPDATES_FLUSH();
#endif
}

/*
 * Process an asynchronous software trap.
 * This is relatively easy.
 * This function will return with preemption disabled.
 */
void
ast(struct trapframe *framep)
{
	struct thread *td;
	struct proc *p;
	int flags;
	int sig;
#if defined(DEV_NPX) && !defined(SMP)
	int ucode;
	ksiginfo_t ksi;
#endif

	td = curthread;
	p = td->td_proc;

	CTR3(KTR_SYSC, "ast: thread %p (pid %d, %s)", td, p->p_pid,
            p->p_comm);
	KASSERT(TRAPF_USERMODE(framep), ("ast in kernel mode"));
	WITNESS_WARN(WARN_PANIC, NULL, "Returning to user mode");
	mtx_assert(&Giant, MA_NOTOWNED);
	THREAD_LOCK_ASSERT(td, MA_NOTOWNED);
	td->td_frame = framep;
	td->td_pticks = 0;

	/*
	 * This updates the td_flag's for the checks below in one
	 * "atomic" operation with turning off the astpending flag.
	 * If another AST is triggered while we are handling the
	 * AST's saved in flags, the astpending flag will be set and
	 * ast() will be called again.
	 */
	thread_lock(td);
	flags = td->td_flags;
	td->td_flags &= ~(TDF_ASTPENDING | TDF_NEEDSIGCHK | TDF_NEEDSUSPCHK |
	    TDF_NEEDRESCHED | TDF_ALRMPEND | TDF_PROFPEND | TDF_MACPEND);
	thread_unlock(td);
	PCPU_INC(cnt.v_trap);

	if (td->td_ucred != p->p_ucred) 
		cred_update_thread(td);
	if (td->td_pflags & TDP_OWEUPC && p->p_flag & P_PROFIL) {
		addupc_task(td, td->td_profil_addr, td->td_profil_ticks);
		td->td_profil_ticks = 0;
		td->td_pflags &= ~TDP_OWEUPC;
	}
	if (flags & TDF_ALRMPEND) {
		PROC_LOCK(p);
		psignal(p, SIGVTALRM);
		PROC_UNLOCK(p);
	}
#if defined(DEV_NPX) && !defined(SMP)
	if (PCPU_GET(curpcb)->pcb_flags & PCB_NPXTRAP) {
		atomic_clear_int(&PCPU_GET(curpcb)->pcb_flags,
		    PCB_NPXTRAP);
		ucode = npxtrap();
		if (ucode != -1) {
			ksiginfo_init_trap(&ksi);
			ksi.ksi_signo = SIGFPE;
			ksi.ksi_code = ucode;
			trapsignal(td, &ksi);
		}
	}
#endif
	if (flags & TDF_PROFPEND) {
		PROC_LOCK(p);
		psignal(p, SIGPROF);
		PROC_UNLOCK(p);
	}
#ifdef MAC
	if (flags & TDF_MACPEND)
		mac_thread_userret(td);
#endif
	if (flags & TDF_NEEDRESCHED) {
#ifdef KTRACE
		if (KTRPOINT(td, KTR_CSW))
			ktrcsw(1, 1);
#endif
		thread_lock(td);
		sched_prio(td, td->td_user_pri);
		mi_switch(SW_INVOL | SWT_NEEDRESCHED, NULL);
		thread_unlock(td);
#ifdef KTRACE
		if (KTRPOINT(td, KTR_CSW))
			ktrcsw(0, 1);
#endif
	}

	/*
	 * Check for signals. Unlocked reads of p_pendingcnt or
	 * p_siglist might cause process-directed signal to be handled
	 * later.
	 */
	if (flags & TDF_NEEDSIGCHK || p->p_pendingcnt > 0 ||
	    !SIGISEMPTY(p->p_siglist)) {
		PROC_LOCK(p);
		mtx_lock(&p->p_sigacts->ps_mtx);
		while ((sig = cursig(td, SIG_STOP_ALLOWED)) != 0)
			postsig(sig);
		mtx_unlock(&p->p_sigacts->ps_mtx);
		PROC_UNLOCK(p);
	}
	/*
	 * We need to check to see if we have to exit or wait due to a
	 * single threading requirement or some other STOP condition.
	 */
	if (flags & TDF_NEEDSUSPCHK) {
		PROC_LOCK(p);
		thread_suspend_check(0);
		PROC_UNLOCK(p);
	}

	if (td->td_pflags & TDP_OLDMASK) {
		td->td_pflags &= ~TDP_OLDMASK;
		kern_sigprocmask(td, SIG_SETMASK, &td->td_oldsigmask, NULL, 0);
	}

	userret(td, framep);
	mtx_assert(&Giant, MA_NOTOWNED);
}

#ifdef HAVE_SYSCALL_ARGS_DEF
const char *
syscallname(struct proc *p, u_int code)
{
	static const char unknown[] = "unknown";

	if (p->p_sysent->sv_syscallnames == NULL)
		return (unknown);
	return (p->p_sysent->sv_syscallnames[code]);
}

int
syscallenter(struct thread *td, struct syscall_args *sa)
{
	struct proc *p;
	int error, traced;

	PCPU_INC(cnt.v_syscall);
	p = td->td_proc;
	td->td_syscalls++;

	td->td_pticks = 0;
	if (td->td_ucred != p->p_ucred)
		cred_update_thread(td);
	if (p->p_flag & P_TRACED) {
		traced = 1;
		PROC_LOCK(p);
		td->td_dbgflags &= ~TDB_USERWR;
		td->td_dbgflags |= TDB_SCE;
		PROC_UNLOCK(p);
	} else
		traced = 0;
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
		PTRACESTOP_SC(p, td, S_PT_SCE);
		if (td->td_dbgflags & TDB_USERWR) {
			/*
			 * Reread syscall number and arguments if
			 * debugger modified registers or memory.
			 */
			error = (p->p_sysent->sv_fetch_syscall_args)(td, sa);
#ifdef KTRACE
			if (KTRPOINT(td, KTR_SYSCALL))
				ktrsyscall(sa->code, sa->narg, sa->args);
#endif
			if (error != 0)
				goto retval;
		}

#ifdef KDTRACE_HOOKS
		/*
		 * If the systrace module has registered it's probe
		 * callback and if there is a probe active for the
		 * syscall 'entry', process the probe.
		 */
		if (systrace_probe_func != NULL && sa->callp->sy_entry != 0)
			(*systrace_probe_func)(sa->callp->sy_entry, sa->code,
			    sa->callp, sa->args);
#endif

		AUDIT_SYSCALL_ENTER(sa->code, td);
		error = (sa->callp->sy_call)(td, sa->args);
		AUDIT_SYSCALL_EXIT(error, td);

		/* Save the latest error return value. */
		td->td_errno = error;

#ifdef KDTRACE_HOOKS
		/*
		 * If the systrace module has registered it's probe
		 * callback and if there is a probe active for the
		 * syscall 'return', process the probe.
		 */
		if (systrace_probe_func != NULL && sa->callp->sy_return != 0)
			(*systrace_probe_func)(sa->callp->sy_return, sa->code,
			    sa->callp, sa->args);
#endif
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

void
syscallret(struct thread *td, int error, struct syscall_args *sa __unused)
{
	struct proc *p;
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

	/*
	 * Handle reschedule and other end-of-syscall issues
	 */
	userret(td, td->td_frame);

	CTR4(KTR_SYSC, "syscall %s exit thread %p pid %d proc %s",
	    syscallname(p, sa->code), td, td->td_proc->p_pid, td->td_name);

#ifdef KTRACE
	if (KTRPOINT(td, KTR_SYSRET))
		ktrsysret(sa->code, error, td->td_retval[0]);
#endif

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
	PTRACESTOP_SC(p, td, S_PT_SCX);
	if (traced || (td->td_dbgflags & TDB_EXEC) != 0) {
		PROC_LOCK(p);
		td->td_dbgflags &= ~(TDB_SCX | TDB_EXEC);
		PROC_UNLOCK(p);
	}
}
#endif /* HAVE_SYSCALL_ARGS_DEF */
