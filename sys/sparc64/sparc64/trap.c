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
#include "opt_ktr.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/ktr.h>
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
int trap_mmu_fault(struct thread *td, struct trapframe *tf);
void syscall(struct trapframe *tf);

u_long trap_mask = 0xffffffffffffffffL & ~(1 << T_INTR);

extern char fsbail[];

extern char *syscallnames[];

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
	"spill",
	"fill",
	"fill",
	"breakpoint",
	"syscall",
	"restore physical watchpoint",
	"restore virtual watchpoint",
	"trap instruction",
};

int unaligned_fixup(struct thread *td, struct trapframe *tf);
int emulate_insn(struct thread *td, struct trapframe *tf);

void
trap(struct trapframe *tf)
{
	struct thread *td;
	struct proc *p;
	u_int sticks;
	int error;
	int ucode;
	int mask;
	int type;
	int sig;

	KASSERT(PCPU_GET(curthread) != NULL, ("trap: curthread NULL"));
	KASSERT(PCPU_GET(curthread)->td_kse != NULL, ("trap: curkse NULL"));
	KASSERT(PCPU_GET(curthread)->td_proc != NULL, ("trap: curproc NULL"));

	atomic_add_int(&cnt.v_trap, 1);

	td = PCPU_GET(curthread);
	p = td->td_proc;

	error = 0;
	type = tf->tf_type;
	ucode = type;	/* XXX */

	CTR5(KTR_TRAP, "trap: %s type=%s (%s) ws=%#lx ow=%#lx",
	    p->p_comm, trap_msg[type & ~T_KERNEL],
	    ((type & T_KERNEL) ? "kernel" : "user"),
	    rdpr(wstate), rdpr(otherwin));

	if ((type & T_KERNEL) == 0) {
		sticks = td->td_kse->ke_sticks;
		td->td_frame = tf;
		KASSERT(td->td_ucred == NULL, ("already have a ucred"));
		PROC_LOCK(p);
		td->td_ucred = crhold(p->p_ucred);
		PROC_UNLOCK(p);
 	} else {
 		sticks = 0;
		KASSERT(cold || td->td_ucred != NULL,
		    ("kernel trap doesn't have ucred"));
	}

	switch (type) {

	/*
	 * User Mode Traps
	 */
	case T_ALIGN:
		if ((sig = unaligned_fixup(td, tf)) == 0) {
			TF_DONE(tf);
			goto user;
		}
		goto trapsig;
	case T_ALIGN_LDDF:
	case T_ALIGN_STDF:
		sig = SIGBUS;
		goto trapsig;
	case T_BREAKPOINT:
		sig = SIGTRAP;
		goto trapsig;
	case T_DIVIDE:
		sig = SIGFPE;
		goto trapsig;
	case T_FP_DISABLED:
		if (fp_enable_thread(td, tf))
			goto user;
		/* Fallthrough. */
	case T_FP_IEEE:
		sig = SIGFPE;
		goto trapsig;
	case T_FP_OTHER:
		if ((sig = fp_exception_other(td, tf)) == 0) {
			TF_DONE(tf);
			goto user;
		}
		goto trapsig;
	case T_DATA_ERROR:
	case T_DATA_EXCPTN:
	case T_INSN_ERROR:
	case T_INSN_EXCPTN:
		sig = SIGILL;	/* XXX */
		goto trapsig;
	case T_DMMU_MISS:
	case T_DMMU_PROT:
	case T_IMMU_MISS:
		error = trap_mmu_fault(td, tf);
		if (error == 0)
			goto user;
		sig = error;
		goto trapsig;
	case T_FILL:
		if (rwindow_load(td, tf, 2)) {
			PROC_LOCK(p);
			sigexit(td, SIGILL);
			/* Not reached. */
		}
		goto userout;
	case T_FILL_RET:
		if (rwindow_load(td, tf, 1)) {
			PROC_LOCK(p);
			sigexit(td, SIGILL);
			/* Not reached. */
		}
		goto userout;
	case T_INSN_ILLEGAL:
		if ((sig = emulate_insn(td, tf)) == 0) {
			TF_DONE(tf);
			goto user;
		}
		goto trapsig;
	case T_PRIV_ACTION:
	case T_PRIV_OPCODE:
		sig = SIGBUS;
		goto trapsig;
	case T_SOFT:
		sig = SIGILL;
		goto trapsig;
	case T_SPILL:
		if (rwindow_save(td)) {
			PROC_LOCK(p);
			sigexit(td, SIGILL);
			/* Not reached. */
		}
		goto userout;
	case T_TAG_OVFLW:
		sig = SIGEMT;
		goto trapsig;

	/*
	 * Kernel Mode Traps
	 */
#ifdef DDB
	case T_BREAKPOINT | T_KERNEL:
		if (kdb_trap(tf) != 0)
			goto out;
		break;
#endif
	case T_DMMU_MISS | T_KERNEL:
	case T_DMMU_PROT | T_KERNEL:
	case T_IMMU_MISS | T_KERNEL:
		error = trap_mmu_fault(td, tf);
		if (error == 0)
			goto out;
		break;
	case T_WATCH_PHYS | T_KERNEL:
		TR3("trap: watch phys pa=%#lx tpc=%#lx, tnpc=%#lx",
		    watch_phys_get(&mask), tf->tf_tpc, tf->tf_tnpc);
		PCPU_SET(wp_pstate, (tf->tf_tstate & TSTATE_PSTATE_MASK) >>
		    TSTATE_PSTATE_SHIFT);
		tf->tf_tstate &= ~TSTATE_IE;
		wrpr(pstate, rdpr(pstate), PSTATE_IE);
		PCPU_SET(wp_insn, *((u_int *)tf->tf_tnpc));
		*((u_int *)tf->tf_tnpc) = 0x91d03002;	/* ta %xcc, 2 */
		flush(tf->tf_tnpc);
		PCPU_SET(wp_va, watch_phys_get(&mask));
		PCPU_SET(wp_mask, mask);
		watch_phys_clear();
		goto out;
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
		TR3("trap: watch virt pa=%#lx tpc=%#lx, tnpc=%#lx",
		    watch_virt_get(&mask), tf->tf_tpc, tf->tf_tnpc);
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
	case T_RSTRWP_PHYS | T_KERNEL:
		tf->tf_tstate = (tf->tf_tstate & ~TSTATE_PSTATE_MASK) |
		    PCPU_GET(wp_pstate) << TSTATE_PSTATE_SHIFT;
		watch_phys_set_mask(PCPU_GET(wp_va), PCPU_GET(wp_mask));
		*(u_int *)tf->tf_tpc = PCPU_GET(wp_insn);
		flush(tf->tf_tpc);
		goto out;
	case T_RSTRWP_VIRT | T_KERNEL:
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
	userret(td, tf, sticks);
userout:
	mtx_assert(&Giant, MA_NOTOWNED);
	mtx_lock(&Giant);
	crfree(td->td_ucred);
	mtx_unlock(&Giant);
	td->td_ucred = NULL;
out:
	CTR1(KTR_TRAP, "trap: td=%p return", td);
	return;
}

int
trap_mmu_fault(struct thread *td, struct trapframe *tf)
{
	struct mmuframe *mf;
	struct vmspace *vm;
	struct stte *stp;
	struct pcb *pcb;
	struct tte tte;
	struct proc *p;
	vm_offset_t va;
	vm_prot_t prot;
	u_long ctx;
	pmap_t pm;
	int flags;
	int type;
	int rv;

	p = td->td_proc;
	KASSERT(td->td_pcb != NULL, ("trap_dmmu_miss: pcb NULL"));
	KASSERT(p->p_vmspace != NULL, ("trap_dmmu_miss: vmspace NULL"));

	rv = KERN_SUCCESS;
	mf = (struct mmuframe *)tf->tf_arg;
	ctx = TLB_TAR_CTX(mf->mf_tar);
	pcb = td->td_pcb;
	type = tf->tf_type & ~T_KERNEL;
	va = TLB_TAR_VA(mf->mf_tar);
	stp = NULL;

	CTR4(KTR_TRAP, "trap_mmu_fault: td=%p pm_ctx=%#lx va=%#lx ctx=%#lx",
	    td, p->p_vmspace->vm_pmap.pm_context, va, ctx);

	if (type == T_DMMU_PROT) {
		prot = VM_PROT_WRITE;
		flags = VM_FAULT_DIRTY;
	} else {
		if (type == T_DMMU_MISS)
			prot = VM_PROT_READ;
		else
			prot = VM_PROT_READ | VM_PROT_EXECUTE;
		flags = VM_FAULT_NORMAL;
	}

	if (ctx == TLB_CTX_KERNEL) {
		mtx_lock(&Giant);
		rv = vm_fault(kernel_map, va, prot, VM_FAULT_NORMAL);
		mtx_unlock(&Giant);
		if (rv == KERN_SUCCESS) {
			stp = tsb_kvtostte(va);
			tte = stp->st_tte;
			if (type == T_IMMU_MISS)
				tlb_store(TLB_DTLB | TLB_ITLB, va, ctx, tte);
			else
				tlb_store(TLB_DTLB, va, ctx, tte);
		}
	} else if (tf->tf_type & T_KERNEL &&
	    (td->td_intr_nesting_level != 0 || pcb->pcb_onfault == NULL ||
	    pcb->pcb_onfault == fsbail)) {
		rv = KERN_FAILURE;
	} else {
		mtx_lock(&Giant);
		vm = p->p_vmspace;
		pm = &vm->vm_pmap;
		stp = tsb_stte_lookup(pm, va);
		if (stp == NULL || type == T_DMMU_PROT) {
			/*
			 * Keep the process from being swapped out at this
			 * critical time.
			 */
			PROC_LOCK(p);
			++p->p_lock;
			PROC_UNLOCK(p);
		
			/*
			 * Grow the stack if necessary.  vm_map_growstack only
			 * fails if the va falls into a growable stack region
			 * and the stack growth fails.  If it succeeds, or the
			 * va was not within a growable stack region, fault in
			 * the user page.
			 */
			if (vm_map_growstack(p, va) != KERN_SUCCESS)
				rv = KERN_FAILURE;
			else
				rv = vm_fault(&vm->vm_map, va, prot, flags);
		
			/*
			 * Now the process can be swapped again.
			 */
			PROC_LOCK(p);
			--p->p_lock;
			PROC_UNLOCK(p);
		} else {
			stp = tsb_stte_promote(pm, va, stp);
			stp->st_tte.tte_data |= TD_REF;
			switch (type) {
			case T_IMMU_MISS:
				if ((stp->st_tte.tte_data & TD_EXEC) == 0) {
					rv = KERN_FAILURE;
					break;
				}
				tlb_store(TLB_DTLB | TLB_ITLB, va, ctx,
				    stp->st_tte);
				break;
			case T_DMMU_PROT:
				if ((stp->st_tte.tte_data & TD_SW) == 0) {
					rv = KERN_FAILURE;
					break;
				}
				/* Fallthrough. */
			case T_DMMU_MISS:
				tlb_store(TLB_DTLB, va, ctx, stp->st_tte);
				break;
			}
		}
		mtx_unlock(&Giant);
	}
	CTR3(KTR_TRAP, "trap_mmu_fault: return p=%p va=%#lx rv=%d", p, va, rv);
	if (rv == KERN_SUCCESS)
		return (0);
	if (tf->tf_type & T_KERNEL) {
		if (pcb->pcb_onfault != NULL && ctx != TLB_CTX_KERNEL) {
			tf->tf_tpc = (u_long)pcb->pcb_onfault;
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
syscall(struct trapframe *tf)
{
	struct sysent *callp;
	struct thread *td;
	register_t args[8];
	register_t *argp;
	struct proc *p;
	u_int sticks;
	u_long code;
	u_long tpc;
	int reg;
	int regcnt;
	int narg;
	int error;

	KASSERT(PCPU_GET(curthread) != NULL, ("trap: curthread NULL"));
	KASSERT(PCPU_GET(curthread)->td_kse != NULL, ("trap: curkse NULL"));
	KASSERT(PCPU_GET(curthread)->td_proc != NULL, ("trap: curproc NULL"));

	atomic_add_int(&cnt.v_syscall, 1);

	td = PCPU_GET(curthread);
	p = td->td_proc;

	narg = 0;
	error = 0;
	reg = 0;
	regcnt = REG_MAXARGS;

	sticks = td->td_kse->ke_sticks;
	td->td_frame = tf;
	KASSERT(td->td_ucred == NULL, ("already have a ucred"));
	PROC_LOCK(p);
	td->td_ucred = crhold(p->p_ucred);
	PROC_UNLOCK(p);	
	code = tf->tf_global[1];

	/*
	 * For syscalls, we don't want to retry the faulting instruction
	 * (usually), instead we need to advance one instruction.
	 */
	tpc = tf->tf_tpc;
	TF_DONE(tf);

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

	CTR5(KTR_SYSC, "syscall: td=%p %s(%#lx, %#lx, %#lx)", td,
	    syscallnames[code], argp[0], argp[1], argp[2]);

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
	td->td_retval[0] = 0;
	td->td_retval[1] = 0;

	STOPEVENT(p, S_SCE, narg);	/* MP aware */

	error = (*callp->sy_call)(td, argp);

	CTR5(KTR_SYSC, "syscall: p=%p error=%d %s return %#lx %#lx ", p,
	    error, syscallnames[code], td->td_retval[0], td->td_retval[1]);
	
	/*
	 * MP SAFE (we may or may not have the MP lock at this point)
	 */
	switch (error) {
	case 0:
		tf->tf_out[0] = td->td_retval[0];
		tf->tf_out[1] = td->td_retval[1];
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
	userret(td, tf, sticks);

#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET)) {
		ktrsysret(p->p_tracep, code, error, td->td_retval[0]);
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

	mtx_lock(&Giant);
	crfree(td->td_ucred);
	mtx_unlock(&Giant);
	td->td_ucred = NULL;
#ifdef WITNESS
	if (witness_list(td)) {
		panic("system call %s returning with mutex(s) held\n",
		    syscallnames[code]);
	}
#endif
	mtx_assert(&sched_lock, MA_NOTOWNED);
	mtx_assert(&Giant, MA_NOTOWNED);
}
