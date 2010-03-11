/*-
 * Copyright (C) 1994, David Greenman
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * 386 Trap and System call handling
 */

#include "opt_clock.h"
#include "opt_cpu.h"
#include "opt_isa.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/pioctl.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/ptrace.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/uio.h>
#include <sys/vmmeter.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif
#include <security/audit/audit.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>

#include <machine/cpu.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>

#define	IDTVEC(name)	__CONCAT(X,name)

extern inthand_t IDTVEC(int0x80_syscall), IDTVEC(rsvd);
extern const char *freebsd32_syscallnames[];

void ia32_syscall(struct trapframe *frame);	/* Called from asm code */

struct ia32_syscall_args {
	u_int code;
	caddr_t params;
	struct sysent *callp;
	u_int64_t args64[8];
	int narg;
};

static int
fetch_ia32_syscall_args(struct thread *td, struct ia32_syscall_args *sa)
{
	struct proc *p;
	struct trapframe *frame;
	u_int32_t args[8];
	int error, i;

	p = td->td_proc;
	frame = td->td_frame;

	sa->params = (caddr_t)frame->tf_rsp + sizeof(u_int32_t);
	sa->code = frame->tf_rax;

	if (p->p_sysent->sv_prepsyscall) {
		/*
		 * The prep code is MP aware.
		 */
		(*p->p_sysent->sv_prepsyscall)(frame, args, &sa->code,
		    &sa->params);
	} else {
		/*
		 * Need to check if this is a 32 bit or 64 bit syscall.
		 * fuword is MP aware.
		 */
		if (sa->code == SYS_syscall) {
			/*
			 * Code is first argument, followed by actual args.
			 */
			sa->code = fuword32(sa->params);
			sa->params += sizeof(int);
		} else if (sa->code == SYS___syscall) {
			/*
			 * Like syscall, but code is a quad, so as to maintain
			 * quad alignment for the rest of the arguments.
			 * We use a 32-bit fetch in case params is not
			 * aligned.
			 */
			sa->code = fuword32(sa->params);
			sa->params += sizeof(quad_t);
		}
	}
 	if (p->p_sysent->sv_mask)
 		sa->code &= p->p_sysent->sv_mask;
 	if (sa->code >= p->p_sysent->sv_size)
 		sa->callp = &p->p_sysent->sv_table[0];
  	else
 		sa->callp = &p->p_sysent->sv_table[sa->code];
	sa->narg = sa->callp->sy_narg;

	if (sa->params != NULL && sa->narg != 0)
		error = copyin(sa->params, (caddr_t)args,
		    (u_int)(sa->narg * sizeof(int)));
	else
		error = 0;

	for (i = 0; i < sa->narg; i++)
		sa->args64[i] = args[i];

#ifdef KTRACE
	if (KTRPOINT(td, KTR_SYSCALL))
		ktrsyscall(sa->code, sa->narg, sa->args64);
#endif

	return (error);
}

void
ia32_syscall(struct trapframe *frame)
{
	struct thread *td;
	struct proc *p;
	struct ia32_syscall_args sa;
	register_t orig_tf_rflags;
	int error;
	ksiginfo_t ksi;

	PCPU_INC(cnt.v_syscall);
	td = curthread;
	p = td->td_proc;
	td->td_syscalls++;

	td->td_pticks = 0;
	td->td_frame = frame;
	if (td->td_ucred != p->p_ucred) 
		cred_update_thread(td);
	orig_tf_rflags = frame->tf_rflags;
	if (p->p_flag & P_TRACED) {
		PROC_LOCK(p);
		td->td_dbgflags &= ~TDB_USERWR;
		PROC_UNLOCK(p);
	}
	error = fetch_ia32_syscall_args(td, &sa);

	CTR4(KTR_SYSC, "syscall enter thread %p pid %d proc %s code %d", td,
	    td->td_proc->p_pid, td->td_name, sa.code);

	if (error == 0) {
		td->td_retval[0] = 0;
		td->td_retval[1] = frame->tf_rdx;

		STOPEVENT(p, S_SCE, sa.narg);
		PTRACESTOP_SC(p, td, S_PT_SCE);
		if (td->td_dbgflags & TDB_USERWR) {
			/*
			 * Reread syscall number and arguments if
			 * debugger modified registers or memory.
			 */
			error = fetch_ia32_syscall_args(td, &sa);
			if (error != 0)
				goto retval;
			td->td_retval[1] = frame->tf_rdx;
		}

		AUDIT_SYSCALL_ENTER(sa.code, td);
		error = (*sa.callp->sy_call)(td, sa.args64);
		AUDIT_SYSCALL_EXIT(error, td);

		/* Save the latest error return value. */
		td->td_errno = error;
	}
 retval:
	cpu_set_syscall_retval(td, error);

	/*
	 * Traced syscall.
	 */
	if (orig_tf_rflags & PSL_T) {
		frame->tf_rflags &= ~PSL_T;
		ksiginfo_init_trap(&ksi);
		ksi.ksi_signo = SIGTRAP;
		ksi.ksi_code = TRAP_TRACE;
		ksi.ksi_addr = (void *)frame->tf_rip;
		trapsignal(td, &ksi);
	}

	/*
	 * Check for misbehavior.
	 */
	WITNESS_WARN(WARN_PANIC, NULL, "System call %s returning",
	    (sa.code >= 0 && sa.code < SYS_MAXSYSCALL) ?
	     freebsd32_syscallnames[sa.code] : "???");
	KASSERT(td->td_critnest == 0,
	    ("System call %s returning in a critical section",
	    (sa.code >= 0 && sa.code < SYS_MAXSYSCALL) ?
	     freebsd32_syscallnames[sa.code] : "???"));
	KASSERT(td->td_locks == 0,
	    ("System call %s returning with %d locks held",
	    (sa.code >= 0 && sa.code < SYS_MAXSYSCALL) ?
	     freebsd32_syscallnames[sa.code] : "???", td->td_locks));

	/*
	 * Handle reschedule and other end-of-syscall issues
	 */
	userret(td, frame);

	CTR4(KTR_SYSC, "syscall exit thread %p pid %d proc %s code %d", td,
	    td->td_proc->p_pid, td->td_proc->p_comm, sa.code);
#ifdef KTRACE
	if (KTRPOINT(td, KTR_SYSRET))
		ktrsysret(sa.code, error, td->td_retval[0]);
#endif

	/*
	 * This works because errno is findable through the
	 * register set.  If we ever support an emulation where this
	 * is not the case, this code will need to be revisited.
	 */
	STOPEVENT(p, S_SCX, sa.code);
 
	PTRACESTOP_SC(p, td, S_PT_SCX);
}


static void
ia32_syscall_enable(void *dummy)
{

 	setidt(IDT_SYSCALL, &IDTVEC(int0x80_syscall), SDT_SYSIGT, SEL_UPL, 0);
}

static void
ia32_syscall_disable(void *dummy)
{

 	setidt(IDT_SYSCALL, &IDTVEC(rsvd), SDT_SYSIGT, SEL_KPL, 0);
}

SYSINIT(ia32_syscall, SI_SUB_EXEC, SI_ORDER_ANY, ia32_syscall_enable, NULL);
SYSUNINIT(ia32_syscall, SI_SUB_EXEC, SI_ORDER_ANY, ia32_syscall_disable, NULL);
