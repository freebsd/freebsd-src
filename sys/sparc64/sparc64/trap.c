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
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/ktr.h>
#include <sys/kse.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/pioctl.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/user.h>
#include <sys/vmmeter.h>
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

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
#include <machine/smp.h>
#include <machine/trap.h>
#include <machine/tstate.h>
#include <machine/tte.h>
#include <machine/tlb.h>
#include <machine/tsb.h>
#include <machine/watch.h>

void trap(struct trapframe *tf);
void syscall(struct trapframe *tf);

static int trap_pfault(struct thread *td, struct trapframe *tf);

extern char copy_fault[];
extern char copy_nofault_begin[];
extern char copy_nofault_end[];

extern char fs_fault[];
extern char fs_nofault_begin[];
extern char fs_nofault_end[];
extern char fs_nofault_intr_begin[];
extern char fs_nofault_intr_end[];

extern char *syscallnames[];

const char *trap_msg[] = {
	"reserved",
	"instruction access exception",
	"instruction access error",
	"instruction access protection",
	"illtrap instruction",
	"illegal instruction",
	"privileged opcode",
	"floating point disabled",
	"floating point exception ieee 754",
	"floating point exception other",
	"tag overflow",
	"division by zero",
	"data access exception",
	"data access error",
	"data access protection",
	"memory address not aligned",
	"privileged action",
	"async data error",
	"trap instruction 16",
	"trap instruction 17",
	"trap instruction 18",
	"trap instruction 19",
	"trap instruction 20",
	"trap instruction 21",
	"trap instruction 22",
	"trap instruction 23",
	"trap instruction 24",
	"trap instruction 25",
	"trap instruction 26",
	"trap instruction 27",
	"trap instruction 28",
	"trap instruction 29",
	"trap instruction 30",
	"trap instruction 31",
	"interrupt",
	"physical address watchpoint",
	"virtual address watchpoint",
	"corrected ecc error",
	"fast instruction access mmu miss",
	"fast data access mmu miss",
	"spill",
	"fill",
	"fill",
	"breakpoint",
	"clean window",
	"range check",
	"fix alignment",
	"integer overflow",
	"syscall",
	"restore physical watchpoint",
	"restore virtual watchpoint",
	"kernel stack fault",
};

int debugger_on_signal = 0;
SYSCTL_INT(_debug, OID_AUTO, debugger_on_signal, CTLFLAG_RW,
    &debugger_on_signal, 0, "");

void
trap(struct trapframe *tf)
{
	struct thread *td;
	struct proc *p;
	u_int sticks;
	int error;
	int ucode;
#ifdef DDB
	int mask;
#endif
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

	CTR4(KTR_TRAP, "trap: %s type=%s (%s) pil=%#lx",
	    p->p_comm, trap_msg[type & ~T_KERNEL],
	    ((type & T_KERNEL) ? "kernel" : "user"),
	    rdpr(pil));

	if ((type & T_KERNEL) == 0) {
		sticks = td->td_kse->ke_sticks;
		td->td_frame = tf;
		if (td->td_ucred != p->p_ucred)
			cred_update_thread(td);
		if ((p->p_flag & P_WEXIT) && (p->p_singlethread != td)) {
			PROC_LOCK(p);
			mtx_lock_spin(&sched_lock);
			thread_exit();
			/* NOTREACHED */
		}
 	} else {
 		sticks = 0;
if ((type & ~T_KERNEL) != T_BREAKPOINT)
		KASSERT(cold || td->td_ucred != NULL,
		    ("kernel trap doesn't have ucred"));
	}

	switch (type) {

	/*
	 * User Mode Traps
	 */
	case T_MEM_ADDRESS_NOT_ALIGNED:
		sig = SIGILL;
		goto trapsig;
#if 0
	case T_ALIGN_LDDF:
	case T_ALIGN_STDF:
		sig = SIGBUS;
		goto trapsig;
#endif
	case T_BREAKPOINT:
		sig = SIGTRAP;
		goto trapsig;
	case T_DIVISION_BY_ZERO:
		sig = SIGFPE;
		goto trapsig;
	case T_FP_DISABLED:
	case T_FP_EXCEPTION_IEEE_754:
	case T_FP_EXCEPTION_OTHER:
		sig = SIGFPE;
		goto trapsig;
	case T_DATA_ERROR:
	case T_DATA_EXCEPTION:
	case T_INSTRUCTION_ERROR:
	case T_INSTRUCTION_EXCEPTION:
		sig = SIGILL;	/* XXX */
		goto trapsig;
	case T_DATA_MISS:
	case T_DATA_PROTECTION:
	case T_INSTRUCTION_MISS:
		error = trap_pfault(td, tf);
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
		goto user;
	case T_FILL_RET:
		if (rwindow_load(td, tf, 1)) {
			PROC_LOCK(p);
			sigexit(td, SIGILL);
			/* Not reached. */
		}
		goto user;
	case T_ILLEGAL_INSTRUCTION:
		sig = SIGILL;
		goto trapsig;
	case T_PRIVILEGED_ACTION:
	case T_PRIVILEGED_OPCODE:
		sig = SIGBUS;
		goto trapsig;
	case T_TRAP_INSTRUCTION_16:
	case T_TRAP_INSTRUCTION_17:
	case T_TRAP_INSTRUCTION_18:
	case T_TRAP_INSTRUCTION_19:
	case T_TRAP_INSTRUCTION_20:
	case T_TRAP_INSTRUCTION_21:
	case T_TRAP_INSTRUCTION_22:
	case T_TRAP_INSTRUCTION_23:
	case T_TRAP_INSTRUCTION_24:
	case T_TRAP_INSTRUCTION_25:
	case T_TRAP_INSTRUCTION_26:
	case T_TRAP_INSTRUCTION_27:
	case T_TRAP_INSTRUCTION_28:
	case T_TRAP_INSTRUCTION_29:
	case T_TRAP_INSTRUCTION_30:
	case T_TRAP_INSTRUCTION_31:
		sig = SIGILL;
		goto trapsig;
	case T_SPILL:
		if (rwindow_save(td)) {
			PROC_LOCK(p);
			sigexit(td, SIGILL);
			/* Not reached. */
		}
		goto user;
	case T_TAG_OFERFLOW:
		sig = SIGEMT;
		goto trapsig;

	/*
	 * Kernel Mode Traps
	 */
#ifdef DDB
	case T_BREAKPOINT | T_KERNEL:
	case T_KSTACK_FAULT | T_KERNEL:
		if (kdb_trap(tf) != 0)
			goto out;
		break;
#endif
	case T_DATA_EXCEPTION | T_KERNEL:
	case T_MEM_ADDRESS_NOT_ALIGNED | T_KERNEL:
		if ((tf->tf_sfsr & MMU_SFSR_FV) == 0 ||
		    MMU_SFSR_GET_ASI(tf->tf_sfsr) != ASI_AIUP)
			break;
		if (tf->tf_tpc >= (u_long)copy_nofault_begin &&
		    tf->tf_tpc <= (u_long)copy_nofault_end) {
			tf->tf_tpc = (u_long)copy_fault;
			tf->tf_tnpc = tf->tf_tpc + 4;
			goto out;
		}
		if (tf->tf_tpc >= (u_long)fs_nofault_begin &&
		    tf->tf_tpc <= (u_long)fs_nofault_end) {
			tf->tf_tpc = (u_long)fs_fault;
			tf->tf_tnpc = tf->tf_tpc + 4;
			goto out;
		}
		break;
	case T_DATA_MISS | T_KERNEL:
	case T_DATA_PROTECTION | T_KERNEL:
	case T_INSTRUCTION_MISS | T_KERNEL:
		error = trap_pfault(td, tf);
		if (error == 0)
			goto out;
		break;
#ifdef DDB
	case T_PA_WATCHPOINT | T_KERNEL:
		TR3("trap: watch phys pa=%#lx tpc=%#lx, tnpc=%#lx",
		    watch_phys_get(&mask), tf->tf_tpc, tf->tf_tnpc);
		PCPU_SET(wp_pstate, (tf->tf_tstate & TSTATE_PSTATE_MASK) >>
		    TSTATE_PSTATE_SHIFT);
		tf->tf_tstate &= ~TSTATE_IE;
		intr_disable();
		PCPU_SET(wp_insn, *((u_int *)tf->tf_tnpc));
		*((u_int *)tf->tf_tnpc) = 0x91d03002;	/* ta %xcc, 2 */
		flush(tf->tf_tnpc);
		PCPU_SET(wp_va, watch_phys_get(&mask));
		PCPU_SET(wp_mask, mask);
		watch_phys_clear();
		goto out;
	case T_VA_WATCHPOINT | T_KERNEL:
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
		/*
		 * This has no matching intr_restore; the PSTATE_IE state of the
		 * trapping code will be restored when the watch point is
		 * restored.
		 */
		intr_disable();
		PCPU_SET(wp_insn, *((u_int *)tf->tf_tnpc));
		*((u_int *)tf->tf_tnpc) = 0x91d03003;	/* ta %xcc, 3 */
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
#endif
	default:
		break;
	}
	panic("trap: %s", trap_msg[type & ~T_KERNEL]);

trapsig:
	/* Translate fault for emulators. */
	if (p->p_sysent->sv_transtrap != NULL)
		sig = (p->p_sysent->sv_transtrap)(sig, type);
	if (debugger_on_signal && (sig == 4 || sig == 10 || sig == 11))
		Debugger("trapsig");
	trapsignal(p, sig, ucode);
user:
	userret(td, tf, sticks);
	mtx_assert(&Giant, MA_NOTOWNED);
#ifdef DIAGNOSTIC
	cred_free_thread(td);
#endif
out:
	CTR1(KTR_TRAP, "trap: td=%p return", td);
	return;
}

static int
trap_pfault(struct thread *td, struct trapframe *tf)
{
	struct vmspace *vm;
	struct pcb *pcb;
	struct proc *p;
	vm_offset_t va;
	vm_prot_t prot;
	u_long ctx;
	int flags;
	int type;
	int rv;

	p = td->td_proc;
	KASSERT(td->td_pcb != NULL, ("trap_pfault: pcb NULL"));
	KASSERT(p->p_vmspace != NULL, ("trap_pfault: vmspace NULL"));

	rv = KERN_SUCCESS;
	ctx = TLB_TAR_CTX(tf->tf_tar);
	pcb = td->td_pcb;
	type = tf->tf_type & ~T_KERNEL;
	va = TLB_TAR_VA(tf->tf_tar);

	CTR4(KTR_TRAP, "trap_pfault: td=%p pm_ctx=%#lx va=%#lx ctx=%#lx",
	    td, p->p_vmspace->vm_pmap.pm_context[PCPU_GET(cpuid)], va, ctx);

	if (type == T_DATA_PROTECTION) {
		prot = VM_PROT_WRITE;
		flags = VM_FAULT_DIRTY;
	} else {
		if (type == T_DATA_MISS)
			prot = VM_PROT_READ;
		else
			prot = VM_PROT_READ | VM_PROT_EXECUTE;
		flags = VM_FAULT_NORMAL;
	}

	if (ctx != TLB_CTX_KERNEL) {
		if ((tf->tf_tstate & TSTATE_PRIV) != 0 &&
		    (tf->tf_tpc >= (u_long)fs_nofault_intr_begin &&
		     tf->tf_tpc <= (u_long)fs_nofault_intr_end)) {
			tf->tf_tpc = (u_long)fs_fault;
			tf->tf_tnpc = tf->tf_tpc + 4;
			return (0);
		}

		/*
		 * This is a fault on non-kernel virtual memory.
		 */
		vm = p->p_vmspace;

		/*
		 * Keep swapout from messing with us during this
		 * critical time.
		 */
		PROC_LOCK(p);
		++p->p_lock;
		PROC_UNLOCK(p);

		/* Fault in the user page. */
		rv = vm_fault(&vm->vm_map, va, prot, flags);

		/*
		 * Now the process can be swapped again.
		 */
		PROC_LOCK(p);
		--p->p_lock;
		PROC_UNLOCK(p);
	} else {
		/*
		 * This is a fault on kernel virtual memory.  Attempts to
		 * access kernel memory from user mode cause privileged
		 * action traps, not page fault.
		 */
		KASSERT(tf->tf_tstate & TSTATE_PRIV,
		    ("trap_pfault: fault on nucleus context from user mode"));

		/*
		 * Don't have to worry about process locking or stacks in the
		 * kernel.
		 */
		rv = vm_fault(kernel_map, va, prot, VM_FAULT_NORMAL);
	}

	CTR3(KTR_TRAP, "trap_pfault: return td=%p va=%#lx rv=%d",
	    td, va, rv);
	if (rv == KERN_SUCCESS)
		return (0);
	if (ctx != TLB_CTX_KERNEL && (tf->tf_tstate & TSTATE_PRIV) != 0) {
		if (tf->tf_tpc >= (u_long)fs_nofault_begin &&
		    tf->tf_tpc <= (u_long)fs_nofault_end) {
			tf->tf_tpc = (u_long)fs_fault;
			tf->tf_tnpc = tf->tf_tpc + 4;
			return (0);
		}
		if (tf->tf_tpc >= (u_long)copy_nofault_begin &&
		    tf->tf_tpc <= (u_long)copy_nofault_end) {
			tf->tf_tpc = (u_long)copy_fault;
			tf->tf_tnpc = tf->tf_tpc + 4;
			return (0);
		}
	}
	return ((rv == KERN_PROTECTION_FAILURE) ? SIGBUS : SIGSEGV);
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
	if (td->td_ucred != p->p_ucred)
		cred_update_thread(td);
	if (p->p_flag & P_KSES) {
		/*
		 * If we are doing a syscall in a KSE environment,
		 * note where our mailbox is. There is always the
		 * possibility that we could do this lazily (in sleep()),
		 * but for now do it every time.
		 */
		td->td_mailbox = (void *)fuword((caddr_t)td->td_kse->ke_mailbox
		    + offsetof(struct kse_mailbox, kmbx_current_thread));
		if ((td->td_mailbox == NULL) ||
		    (td->td_mailbox == (void *)-1)) {
			td->td_mailbox = NULL;  /* single thread it.. */
			td->td_flags &= ~TDF_UNBOUND;
		} else {
			td->td_flags |= TDF_UNBOUND;
		}
	}
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

	if (narg <= regcnt) {
		argp = &tf->tf_out[reg];
		error = 0;
	} else {
		KASSERT(narg <= sizeof(args) / sizeof(args[0]),
		    ("Too many syscall arguments!"));
		argp = args;
		bcopy(&tf->tf_out[reg], args, sizeof(args[0]) * regcnt);
		error = copyin((void *)(tf->tf_out[6] + SPOFF +
		    offsetof(struct frame, fr_pad[6])),
		    &args[regcnt], (narg - regcnt) * sizeof(args[0]));
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
	if (KTRPOINT(td, KTR_SYSCALL))
		ktrsyscall(code, narg, argp);
#endif
	if (error == 0) {
		td->td_retval[0] = 0;
		td->td_retval[1] = 0;

		STOPEVENT(p, S_SCE, narg);	/* MP aware */

		error = (*callp->sy_call)(td, argp);

		CTR5(KTR_SYSC, "syscall: p=%p error=%d %s return %#lx %#lx ", p,
		    error, syscallnames[code], td->td_retval[0],
		    td->td_retval[1]);
	}
	
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
	 * Release Giant if we had to get it.  Don't use mtx_owned(),
	 * we want to catch broken syscalls.
	 */
	if ((callp->sy_narg & SYF_MPSAFE) == 0)
		mtx_unlock(&Giant);

	/*
	 * Handle reschedule and other end-of-syscall issues
	 */
	userret(td, tf, sticks);

#ifdef KTRACE
	if (KTRPOINT(td, KTR_SYSRET))
		ktrsysret(code, error, td->td_retval[0]);
#endif
	/*
	 * This works because errno is findable through the
	 * register set.  If we ever support an emulation where this
	 * is not the case, this code will need to be revisited.
	 */
	STOPEVENT(p, S_SCX, code);

#ifdef DIAGNOSTIC
	cred_free_thread(td);
#endif
#ifdef WITNESS
	if (witness_list(td)) {
		panic("system call %s returning with mutex(s) held\n",
		    syscallnames[code]);
	}
#endif
	mtx_assert(&sched_lock, MA_NOTOWNED);
	mtx_assert(&Giant, MA_NOTOWNED);
}
