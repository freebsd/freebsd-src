/*-
 * Copyright (c) 2001, Jake Burkholder
 * Copyright (C) 1994, David Greenman
 * Copyright (c) 1990, 1993
 *      The Regents of the University of California.  All rights reserved.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      from: @(#)trap.c        7.4 (Berkeley) 5/13/91
 * 	from: FreeBSD: src/sys/i386/i386/trap.c,v 1.197 2001/07/19
 * $FreeBSD$
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/pioctl.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/sysent.h>
#include <sys/user.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>

#include <machine/clock.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/pcb.h>
#include <machine/pv.h>
#include <machine/trap.h>
#include <machine/tstate.h>
#include <machine/tte.h>
#include <machine/tlb.h>
#include <machine/tsb.h>
#include <machine/watch.h>

void trap(struct trapframe *tf);
int trap_mmu_fault(struct proc *p, struct trapframe *tf);
void syscall(struct proc *p, struct trapframe *tf, u_int sticks);

const char *trap_msg[] = {
	"reserved",
	"power on reset",
	"watchdog reset",
	"externally initiated reset",
	"software initiated reset",
	"red state exception",
	"instruction access exception",
	"instruction access error",
	"illegal instruction",
	"privileged opcode",
	"floating point disabled",
	"floating point exception ieee 754",
	"floating point exception other",
	"tag overflow",
	"division by zero",
	"data access exception",
	"data access error",
	"memory address not aligned",
	"lddf memory address not aligned",
	"stdf memory address not aligned",
	"privileged action",
	"interrupt vector",
	"physical address watchpoint",
	"virtual address watchpoint",
	"corrected ecc error",
	"fast instruction access mmu miss",
	"fast data access mmu miss",
	"fast data access protection",
	"clock",
	"bad spill",
	"bad fill",
	"breakpoint",
	"syscall",
};

void
trap(struct trapframe *tf)
{
	u_int sticks;
	struct proc *p;
	int error;
	int ucode;
	int type;
	int sig;
	int mask;

	KASSERT(PCPU_GET(curproc) != NULL, ("trap: curproc NULL"));
	KASSERT(PCPU_GET(curpcb) != NULL, ("trap: curpcb NULL"));

	p = PCPU_GET(curproc);
	type = T_TYPE(tf->tf_type);
	ucode = type;	/* XXX */

	if ((type & T_KERNEL) == 0)
		sticks = p->p_sticks;

	switch (type) {
	case T_FP_DISABLED:
		if (fp_enable_proc(p))
			goto user;
		else {
			sig = SIGFPE;
			goto trapsig;
		}
		break;
	case T_IMMU_MISS:
	case T_DMMU_MISS:
	case T_DMMU_PROT:
		mtx_lock(&Giant);
		error = trap_mmu_fault(p, tf);
		mtx_unlock(&Giant);
		if (error == 0)
			goto user;
		break;
	case T_INTR:
		intr_dispatch(T_LEVEL(tf->tf_type), tf);
		goto user;
	case T_SYSCALL:
		/* syscall() calls userret(), so we need goto out; */
		syscall(p, tf, sticks);
		goto out;
#ifdef DDB
	case T_BREAKPOINT | T_KERNEL:
		if (kdb_trap(tf) != 0)
			goto out;
		break;
#endif
	case T_WATCH_VIRT | T_KERNEL:
		/*
		 * At the moment, just print the information from the trap,
		 * remove the watchpoint, use evil magic to execute the
		 * instruction (we temporarily save the instruction at
		 * %tnpc, write a trap instruction, resume, and reset the
		 * watch point when the trap arrives).
		 * To make sure that no interrupt gets in between and creates
		 * a potentially large window where the watchpoint is inactive,
		 * disable interrupts temporarily.
		 * This is obviously fragile and evilish.
		 */
		printf("Virtual watchpoint triggered, tpc=0x%lx, tnpc=0x%lx\n",
		    tf->tf_tpc, tf->tf_tnpc);
		PCPU_SET(wp_pstate, (tf->tf_tstate & TSTATE_PSTATE_MASK) >>
		    TSTATE_PSTATE_SHIFT);
		tf->tf_tstate &= ~TSTATE_IE;
		wrpr(pstate, rdpr(pstate), PSTATE_IE);
		PCPU_SET(wp_insn, *((u_int *)tf->tf_tnpc));
		*((u_int *)tf->tf_tnpc) = 0x91d03002;	/* ta %xcc, 2 */
		flush(tf->tf_tnpc);
		PCPU_SET(wp_va, watch_virt_get(&mask));
		PCPU_SET(wp_mask, mask);
		watch_virt_clear();
		goto out;
	case T_RESTOREWP | T_KERNEL:
		/*
		 * Undo the tweaks tone for T_WATCH, reset the watch point and
		 * contunue execution.
		 * Note that here, we run with interrupts enabled, so there
		 * is a small chance that we will be interrupted before we
		 * could reset the watch point.
		 */
		tf->tf_tstate = (tf->tf_tstate & ~TSTATE_PSTATE_MASK) |
		    PCPU_GET(wp_pstate) << TSTATE_PSTATE_SHIFT;
		watch_virt_set_mask(PCPU_GET(wp_va), PCPU_GET(wp_mask));
		*(u_int *)tf->tf_tpc = PCPU_GET(wp_insn);
		flush(tf->tf_tpc);
		goto out;
	case T_DMMU_MISS | T_KERNEL:
	case T_DMMU_PROT | T_KERNEL:
		mtx_lock(&Giant);
		error = trap_mmu_fault(p, tf);
		mtx_unlock(&Giant);
		if (error == 0)
			goto out;
		break;
	case T_INTR | T_KERNEL:
		intr_dispatch(T_LEVEL(tf->tf_type), tf);
		goto out;
	default:
		break;
	}
	panic("trap: %s", trap_msg[type & ~T_KERNEL]);

trapsig:
	/* Translate fault for emulators. */
	if (p->p_sysent->sv_transtrap != NULL)
		sig = (p->p_sysent->sv_transtrap)(sig, type);
	trapsignal(p, sig, ucode);
user:
	userret(p, tf, sticks);
	if (mtx_owned(&Giant))
		mtx_unlock(&Giant);
out:
	return;
}

int
trap_mmu_fault(struct proc *p, struct trapframe *tf)
{
	struct mmuframe *mf;
	struct vmspace *vm;
	vm_offset_t va;
	vm_prot_t type;
	int rv;

	KASSERT(p->p_vmspace != NULL, ("trap_dmmu_miss: vmspace NULL"));

	type = 0;
	rv = KERN_FAILURE;
	mf = tf->tf_arg;
	va = TLB_TAR_VA(mf->mf_tar);
	switch (tf->tf_type) {
	case T_DMMU_MISS | T_KERNEL:
		/*
		 * If the context is nucleus this is a soft fault on kernel
		 * memory, just fault in the pages.
		 */
		if (TLB_TAR_CTX(mf->mf_tar) == TLB_CTX_KERNEL) {
			rv = vm_fault(kernel_map, va, VM_PROT_READ,
			    VM_FAULT_NORMAL);
			break;
		}

		/*
		 * Don't allow kernel mode faults on user memory unless
		 * pcb_onfault is set.
		 */
		if (PCPU_GET(curpcb)->pcb_onfault == NULL)
			break;
		/* Fallthrough. */
	case T_IMMU_MISS:
	case T_DMMU_MISS:
		/*
		 * First try the tsb.  The primary tsb was already searched.
		 */
		vm = p->p_vmspace;
		if (tsb_miss(&vm->vm_pmap, tf->tf_type, mf) == 0) {
			rv = KERN_SUCCESS;
			break;
		}

		/*
		 * Not found, call the vm system.
		 */

		if (tf->tf_type == T_IMMU_MISS)
			type = VM_PROT_EXECUTE | VM_PROT_READ;
		else
			type = VM_PROT_READ;

		/*
		 * Keep the process from being swapped out at this critical
		 * time.
		 */
		PROC_LOCK(p);
		++p->p_lock;
		PROC_UNLOCK(p);

		/*
		 * Grow the stack if necessary.  vm_map_growstack only fails
		 * if the va falls into a growable stack region and the stack
		 * growth fails.  If it succeeds, or the va was not within a
		 * growable stack region, fault in the user page.
		 */
		if (vm_map_growstack(p, va) != KERN_SUCCESS)
			rv = KERN_FAILURE;
		else
			rv = vm_fault(&vm->vm_map, va, type, VM_FAULT_NORMAL);

		/*
		 * Now the process can be swapped again.
		 */
		PROC_LOCK(p);
		--p->p_lock;
		PROC_UNLOCK(p);
		break;
	case T_DMMU_PROT | T_KERNEL:
		/*
		 * Protection faults should not happen on kernel memory.
		 */
		if (TLB_TAR_CTX(mf->mf_tar) == TLB_CTX_KERNEL)
			break;

		/*
		 * Don't allow kernel mode faults on user memory unless
		 * pcb_onfault is set.
		 */
		if (PCPU_GET(curpcb)->pcb_onfault == NULL)
			break;
		/* Fallthrough. */
	case T_DMMU_PROT:
		/*
		 * Only look in the tsb.  Write access to an unmapped page
		 * causes a miss first, so the page must have already been
		 * brought in by vm_fault, we just need to find the tte and
		 * update the write bit.  XXX How do we tell them vm system
		 * that we are now writing?
		 */
		vm = p->p_vmspace;
		if (tsb_miss(&vm->vm_pmap, tf->tf_type, mf) == 0)
			rv = KERN_SUCCESS;
		break;
	default:
		break;
	}
	if (rv == KERN_SUCCESS)
		return (0);
	if (tf->tf_type & T_KERNEL) {
		if (PCPU_GET(curpcb)->pcb_onfault != NULL &&
		    TLB_TAR_CTX(mf->mf_tar) != TLB_CTX_KERNEL) {
			tf->tf_tpc = (u_long)PCPU_GET(curpcb)->pcb_onfault;
			tf->tf_tnpc = tf->tf_tpc + 4;
			return (0);
		}
	}
	return (rv == KERN_PROTECTION_FAILURE ? SIGBUS : SIGSEGV);
}

/* Maximum number of arguments that can be passed via the out registers. */
#define	REG_MAXARGS	6

/*
 * Syscall handler. The arguments to the syscall are passed in the o registers
 * by the caller, and are saved in the trap frame. The syscall number is passed
 * in %g1 (and also saved in the trap frame).
 */
void
syscall(struct proc *p, struct trapframe *tf, u_int sticks)
{
	struct sysent *callp;
	u_long code;
	u_long tpc;
	int reg;
	int regcnt;
	int narg;
	int error;
	register_t args[8];
	void *argp;

	narg = 0;
	error = 0;
	reg = 0;
	regcnt = REG_MAXARGS;
	code = tf->tf_global[1];
	atomic_add_int(&cnt.v_syscall, 1);
	/*
	 * For syscalls, we don't want to retry the faulting instruction
	 * (usually), instead we need to advance one instruction.
	 */
	tpc = tf->tf_tpc;
	tf->tf_tpc = tf->tf_tnpc;
	tf->tf_tnpc += 4;

	if (p->p_sysent->sv_prepsyscall) {
		/*
		 * The prep code is MP aware.
		 */
#if 0
		(*p->p_sysent->sv_prepsyscall)(tf, args, &code, &params);
#endif	
	} else 	if (code == SYS_syscall || code == SYS___syscall) {
		code = tf->tf_out[reg++];
		regcnt--;
	}

 	if (p->p_sysent->sv_mask)
 		code &= p->p_sysent->sv_mask;

 	if (code >= p->p_sysent->sv_size)
 		callp = &p->p_sysent->sv_table[0];
  	else
 		callp = &p->p_sysent->sv_table[code];

	narg = callp->sy_narg & SYF_ARGMASK;

	if (narg <= regcnt)
		argp = &tf->tf_out[reg];
	else {
		KASSERT(narg <= sizeof(args) / sizeof(args[0]),
		    ("Too many syscall arguments!"));
		argp = args;
		bcopy(&tf->tf_out[reg], args, sizeof(args[0]) * regcnt);
		error = copyin((void *)(tf->tf_out[6] + SPOFF +
		    offsetof(struct frame, f_pad[6])),
		    &args[reg + regcnt], (narg - regcnt) * sizeof(args[0]));
		if (error != 0)
			goto bad;
	}

	/*
	 * Try to run the syscall without the MP lock if the syscall
	 * is MP safe.
	 */
	if ((callp->sy_narg & SYF_MPSAFE) == 0)
		mtx_lock(&Giant);

#ifdef KTRACE
	/*
	 * We have to obtain the MP lock no matter what if 
	 * we are ktracing
	 */
	if (KTRPOINT(p, KTR_SYSCALL)) {
		ktrsyscall(p->p_tracep, code, narg, args);
	}
#endif
	p->p_retval[0] = 0;
	p->p_retval[1] = tf->tf_out[1];

	STOPEVENT(p, S_SCE, narg);	/* MP aware */

	error = (*callp->sy_call)(p, argp);
	
	/*
	 * MP SAFE (we may or may not have the MP lock at this point)
	 */
	switch (error) {
	case 0:
		tf->tf_out[0] = p->p_retval[0];
		tf->tf_out[1] = p->p_retval[1];
		tf->tf_tstate &= ~TSTATE_XCC_C;
		break;

	case ERESTART:
		/*
		 * Undo the tpc advancement we have done above, we want to
		 * reexecute the system call.
		 */
		tf->tf_tpc = tpc;
		tf->tf_tnpc -= 4;
		break;

	case EJUSTRETURN:
		break;

	default:
bad:
 		if (p->p_sysent->sv_errsize) {
 			if (error >= p->p_sysent->sv_errsize)
  				error = -1;	/* XXX */
   			else
  				error = p->p_sysent->sv_errtbl[error];
		}
		tf->tf_out[0] = error;
		tf->tf_tstate |= TSTATE_XCC_C;
		break;
	}

	/*
	 * Handle reschedule and other end-of-syscall issues
	 */
	userret(p, tf, sticks);

#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET)) {
		ktrsysret(p->p_tracep, code, error, p->p_retval[0]);
	}
#endif

	/*
	 * Release Giant if we had to get it.  Don't use mtx_owned(),
	 * we want to catch broken syscalls.
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
	if (witness_list(p)) {
		panic("system call %s returning with mutex(s) held\n",
		    syscallnames[code]);
	}
#endif
	mtx_assert(&sched_lock, MA_NOTOWNED);
	mtx_assert(&Giant, MA_NOTOWNED);
	
}
