/* $FreeBSD$ */
/* From: src/sys/alpha/alpha/trap.c,v 1.33 */
/* $NetBSD: trap.c,v 1.31 1998/03/26 02:21:46 thorpej Exp $ */

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include "opt_ddb.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mutex.h>
#include <sys/ktr.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/lock.h>
#include <sys/vmmeter.h>
#include <sys/sysent.h>
#include <sys/syscall.h>
#include <sys/pioctl.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/reg.h>
#include <machine/pal.h>
#include <machine/fpu.h>
#include <machine/smp.h>
#include <machine/mutex.h>

#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

#ifdef DDB
#include <ddb/ddb.h>
#endif

u_int32_t want_resched;

static int unaligned_fixup(struct trapframe *framep, struct proc *p);

/*
 * Define the code needed before returning to user mode, for
 * trap and syscall.
 */
static int
userret(register struct proc *p, u_int64_t pc, u_quad_t oticks, int have_giant)
{
	int sig, s;

	/* take pending signals */
	while ((sig = CURSIG(p)) != 0) {
		if (have_giant == 0) {
			mtx_enter(&Giant, MTX_DEF);
			have_giant = 1;
		}
		postsig(sig);
	}
	p->p_priority = p->p_usrpri;
	if (want_resched) {
		/*
		 * Since we are curproc, a clock interrupt could
		 * change our priority without changing run queues
		 * (the running process is not kept on a run queue).
		 * If this happened after we setrunqueue ourselves but
		 * before we switch()'ed, we might not be on the queue
		 * indicated by our priority.
		 */
		s = splstatclock();
		DROP_GIANT_NOSWITCH();
		mtx_enter(&sched_lock, MTX_SPIN);
		setrunqueue(p);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		mtx_exit(&sched_lock, MTX_SPIN);
		PICKUP_GIANT();
		splx(s);
		while ((sig = CURSIG(p)) != 0) {
			if (have_giant == 0) {
				mtx_enter(&Giant, MTX_DEF);
				have_giant = 1;
			}
			postsig(sig);
		}
	}

	/*
	 * If profiling, charge recent system time to the trapped pc.
	 */
	if (p->p_flag & P_PROFIL) {
		if (have_giant == 0) {
			mtx_enter(&Giant, MTX_DEF);
			have_giant = 1;
		}
		addupc_task(p, pc, (int)(p->p_sticks - oticks) * psratio);
	}

	curpriority = p->p_priority;
	return (have_giant);
}

static const char *ia64_vector_names[] = {
	"VHPT Translation",			/* 0 */
	"Instruction TLB",			/* 1 */
	"Data TLB",				/* 2 */
	"Alternate Instruction TLB",		/* 3 */
	"Alternate Data TLB",			/* 4 */
	"Data Nested TLB",			/* 5 */
	"Instruction Key Miss",			/* 6 */
	"Data Key Miss",			/* 7 */
	"Dirty-Bit",				/* 8 */
	"Instruction Access-Bit",		/* 9 */
	"Data Access-Bit",			/* 10 */
	"Break Instruction",			/* 11 */
	"External Interrupt",			/* 12 */
	"Reserved 13",				/* 13 */
	"Reserved 14",				/* 14 */
	"Reserved 15",				/* 15 */
	"Reserved 16",				/* 16 */
	"Reserved 17",				/* 17 */
	"Reserved 18",				/* 18 */
	"Reserved 19",				/* 19 */
	"Page Not Present",			/* 20 */
	"Key Permission",			/* 21 */
	"Instruction Access Rights",		/* 22 */
	"Data Access Rights",			/* 23 */
	"General Exception",			/* 24 */
	"Disabled FP-Register",			/* 25 */
	"NaT Consumption",			/* 26 */
	"Speculation",				/* 27 */
	"Reserved 28",				/* 28 */
	"Debug",				/* 29 */
	"Unaligned Reference",			/* 30 */
	"Unsupported Data Reference",		/* 31 */
	"Floating-point Fault",			/* 32 */
	"Floating-point Trap",			/* 33 */
	"Lower-Privilege Transfer Trap",	/* 34 */
	"Taken Branch Trap",			/* 35 */
	"Single Step Trap",			/* 36 */
	"Reserved 37",				/* 37 */
	"Reserved 38",				/* 38 */
	"Reserved 39",				/* 39 */
	"Reserved 40",				/* 40 */
	"Reserved 41",				/* 41 */
	"Reserved 42",				/* 42 */
	"Reserved 43",				/* 43 */
	"Reserved 44",				/* 44 */
	"IA-32 Exception",			/* 45 */
	"IA-32 Intercept",			/* 46 */
	"IA-32 Interrupt",			/* 47 */
	"Reserved 48",				/* 48 */
	"Reserved 49",				/* 49 */
	"Reserved 50",				/* 50 */
	"Reserved 51",				/* 51 */
	"Reserved 52",				/* 52 */
	"Reserved 53",				/* 53 */
	"Reserved 54",				/* 54 */
	"Reserved 55",				/* 55 */
	"Reserved 56",				/* 56 */
	"Reserved 57",				/* 57 */
	"Reserved 58",				/* 58 */
	"Reserved 59",				/* 59 */
	"Reserved 60",				/* 60 */
	"Reserved 61",				/* 61 */
	"Reserved 62",				/* 62 */
	"Reserved 63",				/* 63 */
	"Reserved 64",				/* 64 */
	"Reserved 65",				/* 65 */
	"Reserved 66",				/* 66 */
	"Reserved 67",				/* 67 */
};

static void
printtrap(int vector, int imm, struct trapframe *framep, int isfatal, int user)
{
	printf("\n");
	printf("%s %s trap:\n", isfatal? "fatal" : "handled",
	       user ? "user" : "kernel");
	printf("\n");
	printf("    trap vector = 0x%x (%s)\n",
	       vector, ia64_vector_names[vector]);
	printf("    cr.iip      = 0x%lx\n", framep->tf_cr_iip);
	printf("    cr.ipsr     = 0x%lx\n", framep->tf_cr_ipsr);
	printf("    cr.isr      = 0x%lx\n", framep->tf_cr_isr);
	printf("    cr.ifa      = 0x%lx\n", framep->tf_cr_ifa);
	printf("    cr.iim      = 0x%x\n", imm);
	printf("    curproc     = %p\n", curproc);
	if (curproc != NULL)
		printf("        pid = %d, comm = %s\n", curproc->p_pid,
		       curproc->p_comm);
	printf("\n");
}

/*
 * Trap is called from exception.s to handle most types of processor traps.
 * System calls are broken out for efficiency and ASTs are broken out
 * to make the code a bit cleaner and more representative of the
 * architecture.
 */
/*ARGSUSED*/
void
trap(int vector, int imm, struct trapframe *framep)
{
	struct proc *p;
	int i;
	u_int64_t ucode;
	u_quad_t sticks;
	int user;

	cnt.v_trap++;
	p = curproc;
	ucode = 0;

	user = ((framep->tf_cr_ipsr & IA64_PSR_CPL) == IA64_PSR_CPL_USER);
	if (user) {
		sticks = p->p_sticks;
		p->p_md.md_tf = framep;
	} else {
		sticks = 0;		/* XXX bogus -Wuninitialized warning */
	}

	switch (vector) {
	case IA64_VEC_UNALIGNED_REFERENCE:
		/*
		 * If user-land, do whatever fixups, printing, and
		 * signalling is appropriate (based on system-wide
		 * and per-process unaligned-access-handling flags).
		 */
		if (user) {
			mtx_enter(&Giant, MTX_DEF);
			if ((i = unaligned_fixup(framep, p)) == 0) {
				mtx_exit(&Giant, MTX_DEF);
				goto out;
			}
			mtx_exit(&Giant, MTX_DEF);
			ucode = framep->tf_cr_ifa;	/* VA */
			break;
		}

		/*
		 * Unaligned access from kernel mode is always an error,
		 * EVEN IF A COPY FAULT HANDLER IS SET!
		 *
		 * It's an error if a copy fault handler is set because
		 * the various routines which do user-initiated copies
		 * do so in a bcopy-like manner.  In other words, the
		 * kernel never assumes that pointers provided by the
		 * user are properly aligned, and so if the kernel
		 * does cause an unaligned access it's a kernel bug.
		 */
		goto dopanic;

	case IA64_VEC_FLOATING_POINT_FAULT:
	case IA64_VEC_FLOATING_POINT_TRAP:
		/* 
		 * If user-land, give a SIGFPE if software completion
		 * is not requested or if the completion fails.
		 */
		if (user) {
			i = SIGFPE;
			ucode = /*a0*/ 0;		/* exception summary */
			break;
		}

		/* Always fatal in kernel.  Should never happen. */
		goto dopanic;

	case IA64_VEC_BREAK:
		goto dopanic;

	case IA64_VEC_DISABLED_FP:
		/*
		 * on exit from the kernel, if proc == fpcurproc,
		 * FP is enabled.
		 */
		if (PCPU_GET(fpcurproc) == p) {
			printf("trap: fp disabled for fpcurproc == %p", p);
			goto dopanic;
		}
	
		ia64_fpstate_switch(p);
		goto out;
		break;

	case IA64_VEC_PAGE_NOT_PRESENT:
	case IA64_VEC_INST_ACCESS_RIGHTS:
	case IA64_VEC_DATA_ACCESS_RIGHTS:
	{
		vm_offset_t va = framep->tf_cr_ifa;
		struct vmspace *vm = NULL;
		vm_map_t map;
		vm_prot_t ftype = 0;
		int rv;

		mtx_enter(&Giant, MTX_DEF);
		/*
		 * If it was caused by fuswintr or suswintr,
		 * just punt.  Note that we check the faulting
		 * address against the address accessed by
		 * [fs]uswintr, in case another fault happens
		 * when they are running.
			 */
		if (!user &&
		    p != NULL &&
		    p->p_addr->u_pcb.pcb_onfault ==
		    (unsigned long)fswintrberr &&
		    p->p_addr->u_pcb.pcb_accessaddr == va) {
			framep->tf_cr_iip = p->p_addr->u_pcb.pcb_onfault;
			p->p_addr->u_pcb.pcb_onfault = 0;
			mtx_exit(&Giant, MTX_DEF);
			goto out;
		}

		/*
		 * It is only a kernel address space fault iff:
		 *	1. !user and
		 *	2. pcb_onfault not set or
		 *	3. pcb_onfault set but kernel space data fault
		 * The last can occur during an exec() copyin where the
		 * argument space is lazy-allocated.
		 *
		 * For the purposes of the Linux emulator, we allow
		 * kernel accesses to a small region of the
		 * user stack which the emulator uses to
		 * translate syscall arguments.
		 */
		if (!user 
		    && ((va >= VM_MIN_KERNEL_ADDRESS) 
			|| (p == NULL) 
			|| (p->p_addr->u_pcb.pcb_onfault == 0))) {
			if (va >= trunc_page(PS_STRINGS
					     - szsigcode
					     - SPARE_USRSPACE)
			    && va < round_page(PS_STRINGS
					       - szsigcode)) {
				vm = p->p_vmspace;
				map = &vm->vm_map;
			} else {
				map = kernel_map;
			}
		} else {
			vm = p->p_vmspace;
			map = &vm->vm_map;
		}

		if (framep->tf_cr_isr & IA64_ISR_X)
			ftype = VM_PROT_EXECUTE;
		else if (framep->tf_cr_isr & IA64_ISR_R)
			ftype = VM_PROT_READ;
		else
			ftype = VM_PROT_WRITE;
	
		va = trunc_page((vm_offset_t)va);

		if (map != kernel_map) {
			/*
			 * Keep swapout from messing with us
			 * during this critical time.
			 */
			++p->p_lock;

			/*
			 * Grow the stack if necessary
			 */
			/* grow_stack returns false only if va falls into
			 * a growable stack region and the stack growth
			 * fails.  It returns true if va was not within
			 * a growable stack region, or if the stack 
			 * growth succeeded.
			 */
			if (!grow_stack (p, va)) {
				rv = KERN_FAILURE;
				--p->p_lock;
				goto nogo;
			}


			/* Fault in the user page: */
			rv = vm_fault(map, va, ftype,
				      (ftype & VM_PROT_WRITE)
				      ? VM_FAULT_DIRTY
				      : VM_FAULT_NORMAL);

			--p->p_lock;
		} else {
			/*
			 * Don't have to worry about process
			 * locking or stacks in the kernel.
			 */
			rv = vm_fault(map, va, ftype, VM_FAULT_NORMAL);
		}
				
	nogo:;
		/*
		 * If this was a stack access we keep track of the
		 * maximum accessed stack size.  Also, if vm_fault
		 * gets a protection failure it is due to accessing
		 * the stack region outside the current limit and
		 * we need to reflect that as an access error.
		 */
		if (map != kernel_map &&
		    (caddr_t)va >= vm->vm_maxsaddr
		    && (caddr_t)va < (caddr_t)USRSTACK) {
			if (rv == KERN_SUCCESS) {
				unsigned nss;
	
				nss = ia64_btop(round_page(USRSTACK - va));
				if (nss > vm->vm_ssize)
					vm->vm_ssize = nss;
			} else if (rv == KERN_PROTECTION_FAILURE)
				rv = KERN_INVALID_ADDRESS;
		}
		if (rv == KERN_SUCCESS) {
			mtx_exit(&Giant, MTX_DEF);
			goto out;
		}

		mtx_exit(&Giant, MTX_DEF);
		ucode = va;
		i = SIGSEGV;
#ifdef DEBUG
		printtrap(vector, imm, framep, 1, user);
#endif
		break;
	}

	default:
		goto dopanic;
	}

#ifdef DEBUG
	printtrap(vector, imm, framep, 1, user);
#endif
	trapsignal(p, i, ucode);
out:
	if (user) {
		if (userret(p, framep->tf_cr_iip, sticks, 0))
			mtx_exit(&Giant, MTX_DEF);
	}
	return;

dopanic:
	printtrap(vector, imm, framep, 1, user);

	/* XXX dump registers */

#ifdef DDB
	kdb_trap(vector, framep);
#endif

	panic("trap");
}

/*
 * Process a system call.
 *
 * System calls are strange beasts.  They are passed the syscall number
 * in r15, and the arguments in the registers (as normal).  They return
 * an error flag in r10 (if r10 != 0 on return, the syscall had an error),
 * and the return value (if any) in r8 and r9.
 *
 * The assembly stub takes care of moving the call number into a register
 * we can get to, and moves all of the argument registers into a stack 
 * buffer.  On return, it restores r8-r10 from the frame before
 * returning to the user process. 
 */
void
syscall(int code, u_int64_t *args, struct trapframe *framep)
{
	struct sysent *callp;
	struct proc *p;
	int error = 0;
	u_int64_t oldip, oldri;
	u_quad_t sticks;

	mtx_enter(&Giant, MTX_DEF);

	cnt.v_syscall++;
	p = curproc;
	p->p_md.md_tf = framep;
	sticks = p->p_sticks;

	/*
	 * Skip past the break instruction. Remember old address in case
	 * we have to restart.
	 */
	oldip = framep->tf_cr_iip;
	oldri = framep->tf_cr_ipsr & IA64_PSR_RI;
	framep->tf_cr_ipsr += IA64_PSR_RI_1;
	if ((framep->tf_cr_ipsr & IA64_PSR_RI) > IA64_PSR_RI_2) {
		framep->tf_cr_ipsr &= ~IA64_PSR_RI;
		framep->tf_cr_iip += 16;
	}
			   
#ifdef DIAGNOSTIC
	ia64_fpstate_check(p);
#endif

	if (p->p_sysent->sv_prepsyscall) {
		/* (*p->p_sysent->sv_prepsyscall)(framep, args, &code, &params); */
		panic("prepsyscall");
	} else {
		/*
		 * syscall() and __syscall() are handled the same on
		 * the ia64, as everything is 64-bit aligned, anyway.
		 */
		if (code == SYS_syscall || code == SYS___syscall) {
			/*
			 * Code is first argument, followed by actual args.
			 */
			code = args[0];
			args++;
		}
	}

 	if (p->p_sysent->sv_mask)
 		code &= p->p_sysent->sv_mask;

 	if (code >= p->p_sysent->sv_size)
 		callp = &p->p_sysent->sv_table[0];
  	else
 		callp = &p->p_sysent->sv_table[code];

#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p->p_tracep, code, (callp->sy_narg & SYF_ARGMASK), args);
#endif
	if (error == 0) {
		p->p_retval[0] = 0;
		p->p_retval[1] = 0;

		STOPEVENT(p, S_SCE, (callp->sy_narg & SYF_ARGMASK));

		error = (*callp->sy_call)(p, args);
	}


	switch (error) {
	case 0:
		framep->tf_r[FRAME_R8] = p->p_retval[0];
		framep->tf_r[FRAME_R9] = p->p_retval[1];
		framep->tf_r[FRAME_R10] = 0;
		break;
	case ERESTART:
		framep->tf_cr_iip = oldip;
		framep->tf_cr_ipsr =
			(framep->tf_cr_ipsr & ~IA64_PSR_RI) | oldri;
		break;
	case EJUSTRETURN:
		break;
	default:
		if (p->p_sysent->sv_errsize) {
			if (error >= p->p_sysent->sv_errsize)
				error = -1; /* XXX */
			else
				error = p->p_sysent->sv_errtbl[error];
		}
		framep->tf_r[FRAME_R8] = error;
		framep->tf_r[FRAME_R10] = 1;
		break;
	}

        /*
         * Reinitialize proc pointer `p' as it may be different
         * if this is a child returning from fork syscall.
         */
	p = curproc;

	userret(p, framep->tf_cr_iip, sticks, 1);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, code, error, p->p_retval[0]);
#endif

	/*
	 * This works because errno is findable through the
	 * register set.  If we ever support an emulation where this
	 * is not the case, this code will need to be revisited.
	 */
	STOPEVENT(p, S_SCX, code);
	mtx_exit(&Giant, MTX_DEF);
}

/*
 * Process the tail end of a fork() for the child.
 */
void
child_return(p)
	struct proc *p;
{
	int have_giant;

	/*
	 * Return values in the frame set by cpu_fork().
	 */

	have_giant = userret(p, p->p_md.md_tf->tf_cr_iip, 0,
			     mtx_owned(&Giant));
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET)) {
		if (have_giant == 0) {
			mtx_enter(&Giant, MTX_DEF);
			have_giant = 1;
		}
		ktrsysret(p->p_tracep, SYS_fork, 0, 0);
	}
#endif

	if (have_giant)
		mtx_exit(&Giant, MTX_DEF);
}

/*
 * Process an asynchronous software trap.
 * This is relatively easy.
 */
void
ast(framep)
	struct trapframe *framep;
{
	register struct proc *p;
	u_quad_t sticks;

	mtx_enter(&Giant, MTX_DEF);

	p = curproc;
	sticks = p->p_sticks;
	p->p_md.md_tf = framep;

	if ((framep->tf_cr_ipsr & IA64_PSR_CPL) != IA64_PSR_CPL_USER)
		panic("ast and not user");

	cnt.v_soft++;

	PCPU_SET(astpending, 0);
	if (p->p_flag & P_OWEUPC) {
		p->p_flag &= ~P_OWEUPC;
		addupc_task(p, p->p_stats->p_prof.pr_addr,
			    p->p_stats->p_prof.pr_ticks);
	}

	userret(p, framep->tf_cr_iip, sticks, 1);

	mtx_exit(&Giant, MTX_DEF);
}

extern int	ia64_unaligned_print, ia64_unaligned_fix;
extern int	ia64_unaligned_sigbus;

static int
unaligned_fixup(struct trapframe *framep, struct proc *p)
{
	vm_offset_t va = framep->tf_cr_ifa;
	int doprint, dofix, dosigbus;
	int signal, size = 0;
	unsigned long uac;

	/*
	 * Figure out what actions to take.
	 */

	if (p)
		uac = p->p_md.md_flags & MDP_UAC_MASK;
	else
		uac = 0;

	doprint = ia64_unaligned_print && !(uac & MDP_UAC_NOPRINT);
	dofix = ia64_unaligned_fix && !(uac & MDP_UAC_NOFIX);
	dosigbus = ia64_unaligned_sigbus | (uac & MDP_UAC_SIGBUS);

	/*
	 * See if the user can access the memory in question.
	 * Even if it's an unknown opcode, SEGV if the access
	 * should have failed.
	 */
	if (!useracc((caddr_t)va, size ? size : 1, VM_PROT_WRITE)) {
		signal = SIGSEGV;
		goto out;
	}

	/*
	 * If we're supposed to be noisy, squawk now.
	 */
	if (doprint) {
		uprintf("pid %d (%s): unaligned access: va=0x%lx pc=0x%lx\n",
			p->p_pid, p->p_comm, va, p->p_md.md_tf->tf_cr_iip);
	}

	/*
	 * If we should try to fix it and know how, give it a shot.
	 *
	 * We never allow bad data to be unknowingly used by the
	 * user process.  That is, if we decide not to fix up an
	 * access we cause a SIGBUS rather than letting the user
	 * process go on without warning.
	 *
	 * If we're trying to do a fixup, we assume that things
	 * will be botched.  If everything works out OK, 
	 * unaligned_{load,store}_* clears the signal flag.
	 */
	signal = SIGBUS;
	if (dofix && size != 0) {
		/*
		 * XXX not done yet.
		 */
	} 

	/*
	 * Force SIGBUS if requested.
	 */
	if (dosigbus)
		signal = SIGBUS;

out:
	return (signal);
}
