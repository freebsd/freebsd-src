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
#include <sys/ktr.h>
#include <sys/sysproto.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/smp.h>
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

#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

#ifdef DDB
#include <ddb/ddb.h>
#endif

extern int unaligned_fixup(struct trapframe *framep, struct thread *td);

#ifdef WITNESS
extern char *syscallnames[];
#endif

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

struct bitname {
	u_int64_t mask;
	const char* name;
};

static void
printbits(u_int64_t mask, struct bitname *bn, int count)
{
	int i, first = 1;
	u_int64_t bit;

	for (i = 0; i < count; i++) {
		/*
		 * Handle fields wider than one bit.
		 */
		bit = bn[i].mask & ~(bn[i].mask - 1);
		if (bn[i].mask > bit) {
			if (first)
				first = 0;
			else
				printf(",");
			printf("%s=%ld", bn[i].name,
			       (mask & bn[i].mask) / bit);
		} else if (mask & bit) {
			if (first)
				first = 0;
			else
				printf(",");
			printf("%s", bn[i].name);
		}
	}
}

struct bitname psr_bits[] = {
	{IA64_PSR_BE,	"be"},
	{IA64_PSR_UP,	"up"},
	{IA64_PSR_AC,	"ac"},
	{IA64_PSR_MFL,	"mfl"},
	{IA64_PSR_MFH,	"mfh"},
	{IA64_PSR_IC,	"ic"},
	{IA64_PSR_I,	"i"},
	{IA64_PSR_PK,	"pk"},
	{IA64_PSR_DT,	"dt"},
	{IA64_PSR_DFL,	"dfl"},
	{IA64_PSR_DFH,	"dfh"},
	{IA64_PSR_SP,	"sp"},
	{IA64_PSR_PP,	"pp"},
	{IA64_PSR_DI,	"di"},
	{IA64_PSR_SI,	"si"},
	{IA64_PSR_DB,	"db"},
	{IA64_PSR_LP,	"lp"},
	{IA64_PSR_TB,	"tb"},
	{IA64_PSR_RT,	"rt"},
	{IA64_PSR_CPL,	"cpl"},
	{IA64_PSR_IS,	"is"},
	{IA64_PSR_MC,	"mc"},
	{IA64_PSR_IT,	"it"},
	{IA64_PSR_ID,	"id"},
	{IA64_PSR_DA,	"da"},
	{IA64_PSR_DD,	"dd"},
	{IA64_PSR_SS,	"ss"},
	{IA64_PSR_RI,	"ri"},
	{IA64_PSR_ED,	"ed"},
	{IA64_PSR_BN,	"bn"},
	{IA64_PSR_IA,	"ia"},
};

static void
printpsr(u_int64_t psr)
{
	printbits(psr, psr_bits, sizeof(psr_bits)/sizeof(psr_bits[0]));
}

struct bitname isr_bits[] = {
	{IA64_ISR_X,	"x"},
	{IA64_ISR_W,	"w"},
	{IA64_ISR_R,	"r"},
	{IA64_ISR_NA,	"na"},
	{IA64_ISR_SP,	"sp"},
	{IA64_ISR_RS,	"rs"},
	{IA64_ISR_IR,	"ir"},
	{IA64_ISR_NI,	"ni"},
	{IA64_ISR_SO,	"so"},
	{IA64_ISR_EI,	"ei"},
	{IA64_ISR_ED,	"ed"},
};

static void printisr(u_int64_t isr)
{
	printbits(isr, isr_bits, sizeof(isr_bits)/sizeof(isr_bits[0]));
}

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
	printf("    cr.ipsr     = 0x%lx (", framep->tf_cr_ipsr);
	printpsr(framep->tf_cr_ipsr);
	printf(")\n");
	printf("    cr.isr      = 0x%lx (", framep->tf_cr_isr);
	printisr(framep->tf_cr_isr);
	printf(")\n");
	printf("    cr.ifa      = 0x%lx\n", framep->tf_cr_ifa);
	printf("    cr.iim      = 0x%x\n", imm);
	printf("    curthread   = %p\n", curthread);
	if (curthread != NULL)
		printf("        pid = %d, comm = %s\n",
		       curthread->td_proc->p_pid, curthread->td_proc->p_comm);
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
	struct thread *td;
	struct proc *p;
	int i;
	u_int64_t ucode;
	u_int sticks;
	int user;

	cnt.v_trap++;
	td = curthread;
	p = td->td_proc;
	ucode = 0;

	/*
	 * Make sure we have a sane floating-point state in case the
	 * user has trashed it.
	 */
	ia64_set_fpsr(IA64_FPSR_DEFAULT);

	user = ((framep->tf_cr_ipsr & IA64_PSR_CPL) == IA64_PSR_CPL_USER);
	if (user) {
		sticks = td->td_kse->ke_sticks;
		td->td_frame = framep;
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
			mtx_lock(&Giant);
			i = unaligned_fixup(framep, td);
			mtx_unlock(&Giant);
			if (i == 0)
				goto out;
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

	case IA64_VEC_DISABLED_FP:
		/*
		 * on exit from the kernel, if thread == fpcurthread,
		 * FP is enabled.
		 */
		if (PCPU_GET(fpcurthread) == td) {
			printf("trap: fp disabled for fpcurthread == %p", td);
			goto dopanic;
		}
	
		ia64_fpstate_switch(td);
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

		/*
		 * If it was caused by fuswintr or suswintr,
		 * just punt.  Note that we check the faulting
		 * address against the address accessed by
		 * [fs]uswintr, in case another fault happens
		 * when they are running.
		 */
		if (!user &&
		    td != NULL &&
		    td->td_pcb->pcb_onfault == (unsigned long)fswintrberr &&
		    td->td_pcb->pcb_accessaddr == va) {
			framep->tf_cr_iip = td->td_pcb->pcb_onfault;
			framep->tf_cr_ipsr &= ~IA64_PSR_RI;
			td->td_pcb->pcb_onfault = 0;
			goto out;
		}
		mtx_lock(&Giant);

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
			|| (td == NULL) 
			|| (td->td_pcb->pcb_onfault == 0))) {
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
		else if (framep->tf_cr_isr & IA64_ISR_W)
			ftype = VM_PROT_WRITE;
		else
			ftype = VM_PROT_READ;
	
		va = trunc_page((vm_offset_t)va);

		if (map != kernel_map) {
			/*
			 * Keep swapout from messing with us
			 * during this critical time.
			 */
			PROC_LOCK(p);
			++p->p_lock;
			PROC_UNLOCK(p);

			/*
			 * Grow the stack if necessary
			 */
			/* grow_stack returns false only if va falls into
			 * a growable stack region and the stack growth
			 * fails.  It returns true if va was not within
			 * a growable stack region, or if the stack 
			 * growth succeeded.
			 */
			if (!grow_stack (p, va))
				rv = KERN_FAILURE;
			else
				/* Fault in the user page: */
				rv = vm_fault(map, va, ftype,
				      (ftype & VM_PROT_WRITE)
				      ? VM_FAULT_DIRTY
				      : VM_FAULT_NORMAL);

			PROC_LOCK(p);
			--p->p_lock;
			PROC_UNLOCK(p);
		} else {
			/*
			 * Don't have to worry about process
			 * locking or stacks in the kernel.
			 */
			rv = vm_fault(map, va, ftype, VM_FAULT_NORMAL);
		}
				
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
		mtx_unlock(&Giant);
		if (rv == KERN_SUCCESS)
			goto out;

		if (!user) {
			/* Check for copyin/copyout fault */
			if (td != NULL &&
			    td->td_pcb->pcb_onfault != 0) {
				framep->tf_cr_iip =
					td->td_pcb->pcb_onfault;
				framep->tf_cr_ipsr &= ~IA64_PSR_RI;
				td->td_pcb->pcb_onfault = 0;
				goto out;
			}
			goto dopanic;
		}
		ucode = va;
		i = SIGSEGV;
		break;

	case IA64_VEC_SINGLE_STEP_TRAP:
	case IA64_VEC_DEBUG:
	case IA64_VEC_TAKEN_BRANCH_TRAP:
	case IA64_VEC_BREAK:
		/*
		 * These are always fatal in kernel, and should never happen.
		 */
		if (!user) {
#ifdef DDB
			/*
			 * ...unless, of course, DDB is configured.
			 */
			if (kdb_trap(vector, framep))
				return;

			/*
			 * If we get here, DDB did _not_ handle the
			 * trap, and we need to PANIC!
			 */
#endif
			goto dopanic;
		}
		i = SIGTRAP;
		break;

	case IA64_VEC_GENERAL_EXCEPTION:
	case IA64_VEC_UNSUPP_DATA_REFERENCE:
	case IA64_VEC_LOWER_PRIVILEGE_TRANSFER:
		if (user) {
			ucode = vector;
			i = SIGBUS;
			break;
		}
		goto dopanic;
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
		userret(td, framep, sticks);
		mtx_assert(&Giant, MA_NOTOWNED);
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
	struct thread *td;
	struct proc *p;
	int error = 0;
	u_int64_t oldip, oldri;
	u_int sticks;

	cnt.v_syscall++;
	td = curthread;
	p = td->td_proc;

	td->td_frame = framep;
	sticks = td->td_kse->ke_sticks;

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
	ia64_fpstate_check(td);
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

	/*
	 * Try to run the syscall without Giant if the syscall is MP safe.
	 */
	if ((callp->sy_narg & SYF_MPSAFE) == 0)
		mtx_lock(&Giant);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p->p_tracep, code, (callp->sy_narg & SYF_ARGMASK), args);
#endif
	if (error == 0) {
		td->td_retval[0] = 0;
		td->td_retval[1] = 0;

		STOPEVENT(p, S_SCE, (callp->sy_narg & SYF_ARGMASK));

		error = (*callp->sy_call)(td, args);
	}


	switch (error) {
	case 0:
		framep->tf_r[FRAME_R8] = td->td_retval[0];
		framep->tf_r[FRAME_R9] = td->td_retval[1];
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

	userret(td, framep, sticks);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, code, error, td->td_retval[0]);
#endif
	/*
	 * Release Giant if we had to get it.
	 */
	if ((callp->sy_narg & SYF_MPSAFE) == 0)
		mtx_unlock(&Giant);

	/*
	 * This works because errno is findable through the
	 * register set.  If we ever support an emulation where this
	 * is not the case, this code will need to be revisited.
	 */
	STOPEVENT(p, S_SCX, code);

#ifdef WITNESS
	if (witness_list(td)) {
		panic("system call %s returning with mutex(s) held\n",
		    syscallnames[code]);
	}
#endif
	mtx_assert(&sched_lock, MA_NOTOWNED);
	mtx_assert(&Giant, MA_NOTOWNED);
}
