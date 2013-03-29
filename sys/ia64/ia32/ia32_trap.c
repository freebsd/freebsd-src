/*-
 * Copyright (c) 2004 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pioctl.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/signalvar.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <machine/cpu.h>
#include <machine/fpu.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <x86/include/psl.h>

#include <security/audit/audit.h>

#include <compat/ia32/ia32_util.h>

void
ia32_set_syscall_retval(struct thread *td, int error)
{
	struct proc *p;
	struct trapframe *tf;

	tf = td->td_frame;

	switch (error) {
	case 0:
		tf->tf_scratch.gr8 = td->td_retval[0];	/* eax */
		tf->tf_scratch.gr10 = td->td_retval[1];	/* edx */
		ia64_set_eflag(ia64_get_eflag() & ~PSL_C);
		break;

	case ERESTART:
		/*
		 * Reconstruct pc, assuming lcall $X,y is 7 bytes,
		 * int 0x80 is 2 bytes. XXX Assume int 0x80.
		 */
		tf->tf_special.iip -= 2;
		break;

	case EJUSTRETURN:
		break;

	default:
		p = td->td_proc;
		if (p->p_sysent->sv_errsize) {
			if (error >= p->p_sysent->sv_errsize)
				error = -1;	/* XXX */
			else
				error = p->p_sysent->sv_errtbl[error];
		}
		tf->tf_scratch.gr8 = error;
		ia64_set_eflag(ia64_get_eflag() | PSL_C);
		break;
	}
}

int
ia32_fetch_syscall_args(struct thread *td, struct syscall_args *sa)
{
	struct trapframe *tf;
	struct proc *p;
	uint32_t args[8];
	caddr_t params;
	int error, i;

	tf = td->td_frame;
	p = td->td_proc;

	params = (caddr_t)(tf->tf_special.sp & ((1L<<32)-1)) +
	    sizeof(uint32_t);
	sa->code = tf->tf_scratch.gr8;		/* eax */

	if (sa->code == SYS_syscall) {
		/* Code is first argument, followed by actual args. */
		sa->code = fuword32(params);
		params += sizeof(int);
	} else if (sa->code == SYS___syscall) {
		/*
		 * Like syscall, but code is a quad, so as to maintain
		 * quad alignment for the rest of the arguments.  We
		 * use a 32-bit fetch in case params is not aligned.
		 */
		sa->code = fuword32(params);
		params += sizeof(quad_t);
	}

	if (p->p_sysent->sv_mask)
		sa->code &= p->p_sysent->sv_mask;
	if (sa->code >= p->p_sysent->sv_size)
		sa->callp = &p->p_sysent->sv_table[0];
	else
		sa->callp = &p->p_sysent->sv_table[sa->code];
	sa->narg = sa->callp->sy_narg;

	if (params != NULL && sa->narg != 0)
		error = copyin(params, (caddr_t)args, sa->narg * sizeof(int));
	else
		error = 0;
	sa->args = &sa->args32[0];

	if (error == 0) {
		for (i = 0; i < sa->narg; i++)
			sa->args32[i] = args[i];
		td->td_retval[0] = 0;
		td->td_retval[1] = tf->tf_scratch.gr10;	/* edx */
	}

	return (error);
}

#include "../../kern/subr_syscall.c"

static void
ia32_syscall(struct trapframe *tf)
{
	struct thread *td;
	struct syscall_args sa;
	register_t eflags;
	int error;
	ksiginfo_t ksi;

	td = curthread;
	eflags = ia64_get_eflag();

	error = syscallenter(td, &sa);

	/*
	 * Traced syscall.
	 */
	if ((eflags & PSL_T) && !(eflags & PSL_VM)) {
		ia64_set_eflag(ia64_get_eflag() & ~PSL_T);
		ksiginfo_init_trap(&ksi);
		ksi.ksi_signo = SIGTRAP;
		ksi.ksi_code = TRAP_TRACE;
		ksi.ksi_addr = (void *)tf->tf_special.iip;
		trapsignal(td, &ksi);
	}

	syscallret(td, error, &sa);
}

/*
 * ia32_trap() is called from exception.S to handle the IA-32 specific
 * interruption vectors.
 */
void
ia32_trap(int vector, struct trapframe *tf)
{
	struct proc *p;
	struct thread *td;
	uint64_t ucode;
	int sig;
	ksiginfo_t ksi;

	KASSERT(TRAPF_USERMODE(tf), ("%s: In kernel mode???", __func__));

	ia64_set_fpsr(IA64_FPSR_DEFAULT);
	PCPU_INC(cnt.v_trap);

	td = curthread;
	td->td_frame = tf;
	td->td_pticks = 0;
	p = td->td_proc;
	if (td->td_ucred != p->p_ucred)
		cred_update_thread(td);
	sig = 0;
	ucode = 0;
	switch (vector) {
	case IA64_VEC_IA32_EXCEPTION:
		switch ((tf->tf_special.isr >> 16) & 0xffff) {
		case IA32_EXCEPTION_DIVIDE:
			ucode = FPE_INTDIV;
			sig = SIGFPE;
			break;
		case IA32_EXCEPTION_DEBUG:
		case IA32_EXCEPTION_BREAK:
			sig = SIGTRAP;
			break;
		case IA32_EXCEPTION_OVERFLOW:
			ucode = FPE_INTOVF;
			sig = SIGFPE;
			break;
		case IA32_EXCEPTION_BOUND:
			ucode = FPE_FLTSUB;
			sig = SIGFPE;
			break;
		case IA32_EXCEPTION_DNA:
			ucode = 0;
			sig = SIGFPE;
			break;
		case IA32_EXCEPTION_NOT_PRESENT:
		case IA32_EXCEPTION_STACK_FAULT:
		case IA32_EXCEPTION_GPFAULT:
			ucode = (tf->tf_special.isr & 0xffff) + BUS_SEGM_FAULT;
			sig = SIGBUS;
			break;
		case IA32_EXCEPTION_FPERROR:
			ucode = 0;	/* XXX */
			sig = SIGFPE;
			break;
		case IA32_EXCEPTION_ALIGNMENT_CHECK:
			ucode = tf->tf_special.ifa;	/* VA */
			sig = SIGBUS;
			break;
		case IA32_EXCEPTION_STREAMING_SIMD:
			ucode = 0; /* XXX */
			sig = SIGFPE;
			break;
		default:
			trap_panic(vector, tf);
			break;
		}
		break;

	case IA64_VEC_IA32_INTERCEPT:
		/* XXX Maybe need to emulate ia32 instruction. */
		trap_panic(vector, tf);

	case IA64_VEC_IA32_INTERRUPT:
		/* INT n instruction - probably a syscall. */
		if (((tf->tf_special.isr >> 16) & 0xffff) == 0x80) {
			ia32_syscall(tf);
			goto out;
		}
		ucode = (tf->tf_special.isr >> 16) & 0xffff;
		sig = SIGILL;
		break;

	default:
		/* Should never happen of course. */
		trap_panic(vector, tf);
		break;
	}

	KASSERT(sig != 0, ("%s: signal not set", __func__));

	ksiginfo_init_trap(&ksi);
	ksi.ksi_signo = sig;
	ksi.ksi_code = (int)ucode; /* XXX */
	/* ksi.ksi_addr */
	trapsignal(td, &ksi);

out:
	userret(td, tf);
	do_ast(tf);
}
