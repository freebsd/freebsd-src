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
 *
 *	from: @(#)trap.c	7.4 (Berkeley) 5/13/91
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * 386 Trap and System call handling
 */

#include "opt_clock.h"
#include "opt_cpu.h"
#include "opt_hwpmc_hooks.h"
#include "opt_isa.h"
#include "opt_kdb.h"
#include "opt_ktrace.h"
#include "opt_npx.h"
#include "opt_trap.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/pioctl.h>
#include <sys/ptrace.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
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
#ifdef HWPMC_HOOKS
#include <sys/pmckern.h>
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
#include <machine/pcb.h>
#ifdef SMP
#include <machine/smp.h>
#endif
#include <machine/tss.h>
#include <machine/vm86.h>

#ifdef POWERFAIL_NMI
#include <sys/syslog.h>
#include <machine/clock.h>
#endif

extern void trap(struct trapframe *frame);
extern void syscall(struct trapframe *frame);

static int trap_pfault(struct trapframe *, int, vm_offset_t);
static void trap_fatal(struct trapframe *, vm_offset_t);
void dblfault_handler(void);

extern inthand_t IDTVEC(lcall_syscall);

#define MAX_TRAP_MSG		30
static char *trap_msg[] = {
	"",					/*  0 unused */
	"privileged instruction fault",		/*  1 T_PRIVINFLT */
	"",					/*  2 unused */
	"breakpoint instruction fault",		/*  3 T_BPTFLT */
	"",					/*  4 unused */
	"",					/*  5 unused */
	"arithmetic trap",			/*  6 T_ARITHTRAP */
	"",					/*  7 unused */
	"",					/*  8 unused */
	"general protection fault",		/*  9 T_PROTFLT */
	"trace trap",				/* 10 T_TRCTRAP */
	"",					/* 11 unused */
	"page fault",				/* 12 T_PAGEFLT */
	"",					/* 13 unused */
	"alignment fault",			/* 14 T_ALIGNFLT */
	"",					/* 15 unused */
	"",					/* 16 unused */
	"",					/* 17 unused */
	"integer divide fault",			/* 18 T_DIVIDE */
	"non-maskable interrupt trap",		/* 19 T_NMI */
	"overflow trap",			/* 20 T_OFLOW */
	"FPU bounds check fault",		/* 21 T_BOUND */
	"FPU device not available",		/* 22 T_DNA */
	"double fault",				/* 23 T_DOUBLEFLT */
	"FPU operand fetch fault",		/* 24 T_FPOPFLT */
	"invalid TSS fault",			/* 25 T_TSSFLT */
	"segment not present fault",		/* 26 T_SEGNPFLT */
	"stack fault",				/* 27 T_STKFLT */
	"machine check trap",			/* 28 T_MCHK */
	"SIMD floating-point exception",	/* 29 T_XMMFLT */
	"reserved (unknown) fault",		/* 30 T_RESERVED */
};

#if defined(I586_CPU) && !defined(NO_F00F_HACK)
extern int has_f00f_bug;
#endif

#ifdef KDB
static int kdb_on_nmi = 1;
SYSCTL_INT(_machdep, OID_AUTO, kdb_on_nmi, CTLFLAG_RW,
	&kdb_on_nmi, 0, "Go to KDB on NMI");
#endif
static int panic_on_nmi = 1;
SYSCTL_INT(_machdep, OID_AUTO, panic_on_nmi, CTLFLAG_RW,
	&panic_on_nmi, 0, "Panic on NMI");

extern char *syscallnames[];

/*
 * Exception, fault, and trap interface to the FreeBSD kernel.
 * This common code is called from assembly language IDT gate entry
 * routines that prepare a suitable stack frame, and restore this
 * frame after the exception has been processed.
 */

void
trap(struct trapframe *frame)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	int i = 0, ucode = 0, code;
	u_int type;
	register_t addr = 0;
	vm_offset_t eva;
	ksiginfo_t ksi;
#ifdef POWERFAIL_NMI
	static int lastalert = 0;
#endif

	PCPU_INC(cnt.v_trap);
	type = frame->tf_trapno;

#ifdef SMP
#ifdef STOP_NMI
	/* Handler for NMI IPIs used for stopping CPUs. */
	if (type == T_NMI) {
	         if (ipi_nmi_handler() == 0)
	                   goto out;
	}
#endif /* STOP_NMI */
#endif /* SMP */

#ifdef KDB
	if (kdb_active) {
		kdb_reenter();
		goto out;
	}
#endif

#ifdef	HWPMC_HOOKS
	/*
	 * CPU PMCs interrupt using an NMI so we check for that first.
	 * If the HWPMC module is active, 'pmc_hook' will point to
	 * the function to be called.  A return value of '1' from the
	 * hook means that the NMI was handled by it and that we can
	 * return immediately.
	 */
	if (type == T_NMI && pmc_intr &&
	    (*pmc_intr)(PCPU_GET(cpuid), (uintptr_t) frame->tf_eip,
		TRAPF_USERMODE(frame)))
	    goto out;
#endif

	if ((frame->tf_eflags & PSL_I) == 0) {
		/*
		 * Buggy application or kernel code has disabled
		 * interrupts and then trapped.  Enabling interrupts
		 * now is wrong, but it is better than running with
		 * interrupts disabled until they are accidentally
		 * enabled later.
		 */
		if (ISPL(frame->tf_cs) == SEL_UPL || (frame->tf_eflags & PSL_VM))
			printf(
			    "pid %ld (%s): trap %d with interrupts disabled\n",
			    (long)curproc->p_pid, curproc->p_comm, type);
		else if (type != T_BPTFLT && type != T_TRCTRAP &&
			 frame->tf_eip != (int)cpu_switch_load_gs) {
			/*
			 * XXX not quite right, since this may be for a
			 * multiple fault in user mode.
			 */
			printf("kernel trap %d with interrupts disabled\n",
			    type);
			/*
			 * Page faults need interrupts disabled until later,
			 * and we shouldn't enable interrupts while holding
			 * a spin lock or if servicing an NMI.
			 */
			if (type != T_NMI && type != T_PAGEFLT &&
			    td->td_md.md_spinlock_count == 0)
				enable_intr();
		}
	}

	eva = 0;
	code = frame->tf_err;
	if (type == T_PAGEFLT) {
		/*
		 * For some Cyrix CPUs, %cr2 is clobbered by
		 * interrupts.  This problem is worked around by using
		 * an interrupt gate for the pagefault handler.  We
		 * are finally ready to read %cr2 and then must
		 * reenable interrupts.
		 *
		 * If we get a page fault while in a critical section, then
		 * it is most likely a fatal kernel page fault.  The kernel
		 * is already going to panic trying to get a sleep lock to
		 * do the VM lookup, so just consider it a fatal trap so the
		 * kernel can print out a useful trap message and even get
		 * to the debugger.
		 *
		 * If we get a page fault while holding a non-sleepable
		 * lock, then it is most likely a fatal kernel page fault.
		 * If WITNESS is enabled, then it's going to whine about
		 * bogus LORs with various VM locks, so just skip to the
		 * fatal trap handling directly.
		 */
		eva = rcr2();
		if (td->td_critnest != 0 ||
		    WITNESS_CHECK(WARN_SLEEPOK | WARN_GIANTOK, NULL,
		    "Kernel page fault") != 0)
			trap_fatal(frame, eva);
		else
			enable_intr();
	}

        if ((ISPL(frame->tf_cs) == SEL_UPL) ||
	    ((frame->tf_eflags & PSL_VM) && 
		!(PCPU_GET(curpcb)->pcb_flags & PCB_VM86CALL))) {
		/* user trap */

		td->td_pticks = 0;
		td->td_frame = frame;
		addr = frame->tf_eip;
		if (td->td_ucred != p->p_ucred) 
			cred_update_thread(td);

		switch (type) {
		case T_PRIVINFLT:	/* privileged instruction fault */
			i = SIGILL;
			ucode = ILL_PRVOPC;
			break;

		case T_BPTFLT:		/* bpt instruction fault */
		case T_TRCTRAP:		/* trace trap */
			enable_intr();
			frame->tf_eflags &= ~PSL_T;
			i = SIGTRAP;
			ucode = (type == T_TRCTRAP ? TRAP_TRACE : TRAP_BRKPT);
			break;

		case T_ARITHTRAP:	/* arithmetic trap */
#ifdef DEV_NPX
			ucode = npxtrap();
			if (ucode == -1)
				goto userout;
#else
			ucode = 0;
#endif
			i = SIGFPE;
			break;

			/*
			 * The following two traps can happen in
			 * vm86 mode, and, if so, we want to handle
			 * them specially.
			 */
		case T_PROTFLT:		/* general protection fault */
		case T_STKFLT:		/* stack fault */
			if (frame->tf_eflags & PSL_VM) {
				i = vm86_emulate((struct vm86frame *)frame);
				if (i == 0)
					goto user;
				break;
			}
			i = SIGBUS;
			ucode = (type == T_PROTFLT) ? BUS_OBJERR : BUS_ADRERR;
			break;
		case T_SEGNPFLT:	/* segment not present fault */
			i = SIGBUS;
			ucode = BUS_ADRERR;
			break;
		case T_TSSFLT:		/* invalid TSS fault */
			i = SIGBUS;
			ucode = BUS_OBJERR;
			break;
		case T_DOUBLEFLT:	/* double fault */
		default:
			i = SIGBUS;
			ucode = BUS_OBJERR;
			break;

		case T_PAGEFLT:		/* page fault */
#ifdef KSE
			if (td->td_pflags & TDP_SA)
				thread_user_enter(td);
#endif

			i = trap_pfault(frame, TRUE, eva);
#if defined(I586_CPU) && !defined(NO_F00F_HACK)
			if (i == -2) {
				/*
				 * The f00f hack workaround has triggered, so
				 * treat the fault as an illegal instruction 
				 * (T_PRIVINFLT) instead of a page fault.
				 */
				type = frame->tf_trapno = T_PRIVINFLT;

				/* Proceed as in that case. */
				ucode = ILL_PRVOPC;
				i = SIGILL;
				break;
			}
#endif
			if (i == -1)
				goto userout;
			if (i == 0)
				goto user;

			if (i == SIGSEGV)
				ucode = SEGV_MAPERR;
			else {
				i = SIGSEGV; /* XXX hack */
				ucode = SEGV_ACCERR;
			}
			addr = eva;
			break;

		case T_DIVIDE:		/* integer divide fault */
			ucode = FPE_INTDIV;
			i = SIGFPE;
			break;

#ifdef DEV_ISA
		case T_NMI:
#ifdef POWERFAIL_NMI
#ifndef TIMER_FREQ
#  define TIMER_FREQ 1193182
#endif
			mtx_lock(&Giant);
			if (time_second - lastalert > 10) {
				log(LOG_WARNING, "NMI: power fail\n");
				sysbeep(TIMER_FREQ/880, hz);
				lastalert = time_second;
			}
			mtx_unlock(&Giant);
			goto userout;
#else /* !POWERFAIL_NMI */
			/* machine/parity/power fail/"kitchen sink" faults */
			/* XXX Giant */
			if (isa_nmi(code) == 0) {
#ifdef KDB
				/*
				 * NMI can be hooked up to a pushbutton
				 * for debugging.
				 */
				if (kdb_on_nmi) {
					printf ("NMI ... going to debugger\n");
					kdb_trap(type, 0, frame);
				}
#endif /* KDB */
				goto userout;
			} else if (panic_on_nmi)
				panic("NMI indicates hardware failure");
			break;
#endif /* POWERFAIL_NMI */
#endif /* DEV_ISA */

		case T_OFLOW:		/* integer overflow fault */
			ucode = FPE_INTOVF;
			i = SIGFPE;
			break;

		case T_BOUND:		/* bounds check fault */
			ucode = FPE_FLTSUB;
			i = SIGFPE;
			break;

		case T_DNA:
#ifdef DEV_NPX
			/* transparent fault (due to context switch "late") */
			if (npxdna())
				goto userout;
#endif
			printf("pid %d killed due to lack of floating point\n",
				p->p_pid);
			i = SIGKILL;
			ucode = 0;
			break;

		case T_FPOPFLT:		/* FPU operand fetch fault */
			ucode = ILL_COPROC;
			i = SIGILL;
			break;

		case T_XMMFLT:		/* SIMD floating-point exception */
			ucode = 0; /* XXX */
			i = SIGFPE;
			break;
		}
	} else {
		/* kernel trap */

		KASSERT(cold || td->td_ucred != NULL,
		    ("kernel trap doesn't have ucred"));
		switch (type) {
		case T_PAGEFLT:			/* page fault */
			(void) trap_pfault(frame, FALSE, eva);
			goto out;

		case T_DNA:
#ifdef DEV_NPX
			/*
			 * The kernel is apparently using npx for copying.
			 * XXX this should be fatal unless the kernel has
			 * registered such use.
			 */
			if (npxdna())
				goto out;
#endif
			break;

			/*
			 * The following two traps can happen in
			 * vm86 mode, and, if so, we want to handle
			 * them specially.
			 */
		case T_PROTFLT:		/* general protection fault */
		case T_STKFLT:		/* stack fault */
			if (frame->tf_eflags & PSL_VM) {
				i = vm86_emulate((struct vm86frame *)frame);
				if (i != 0)
					/*
					 * returns to original process
					 */
					vm86_trap((struct vm86frame *)frame);
				goto out;
			}
			if (type == T_STKFLT)
				break;

			/* FALL THROUGH */

		case T_SEGNPFLT:	/* segment not present fault */
			if (PCPU_GET(curpcb)->pcb_flags & PCB_VM86CALL)
				break;

			/*
			 * Invalid %fs's and %gs's can be created using
			 * procfs or PT_SETREGS or by invalidating the
			 * underlying LDT entry.  This causes a fault
			 * in kernel mode when the kernel attempts to
			 * switch contexts.  Lose the bad context
			 * (XXX) so that we can continue, and generate
			 * a signal.
			 */
			if (frame->tf_eip == (int)cpu_switch_load_gs) {
				PCPU_GET(curpcb)->pcb_gs = 0;
#if 0				
				PROC_LOCK(p);
				psignal(p, SIGBUS);
				PROC_UNLOCK(p);
#endif				
				goto out;
			}

			if (td->td_intr_nesting_level != 0)
				break;

			/*
			 * Invalid segment selectors and out of bounds
			 * %eip's and %esp's can be set up in user mode.
			 * This causes a fault in kernel mode when the
			 * kernel tries to return to user mode.  We want
			 * to get this fault so that we can fix the
			 * problem here and not have to check all the
			 * selectors and pointers when the user changes
			 * them.
			 */
			if (frame->tf_eip == (int)doreti_iret) {
				frame->tf_eip = (int)doreti_iret_fault;
				goto out;
			}
			if (frame->tf_eip == (int)doreti_popl_ds) {
				frame->tf_eip = (int)doreti_popl_ds_fault;
				goto out;
			}
			if (frame->tf_eip == (int)doreti_popl_es) {
				frame->tf_eip = (int)doreti_popl_es_fault;
				goto out;
			}
			if (frame->tf_eip == (int)doreti_popl_fs) {
				frame->tf_eip = (int)doreti_popl_fs_fault;
				goto out;
			}
			if (PCPU_GET(curpcb)->pcb_onfault != NULL) {
				frame->tf_eip =
				    (int)PCPU_GET(curpcb)->pcb_onfault;
				goto out;
			}
			break;

		case T_TSSFLT:
			/*
			 * PSL_NT can be set in user mode and isn't cleared
			 * automatically when the kernel is entered.  This
			 * causes a TSS fault when the kernel attempts to
			 * `iret' because the TSS link is uninitialized.  We
			 * want to get this fault so that we can fix the
			 * problem here and not every time the kernel is
			 * entered.
			 */
			if (frame->tf_eflags & PSL_NT) {
				frame->tf_eflags &= ~PSL_NT;
				goto out;
			}
			break;

		case T_TRCTRAP:	 /* trace trap */
			if (frame->tf_eip == (int)IDTVEC(lcall_syscall)) {
				/*
				 * We've just entered system mode via the
				 * syscall lcall.  Continue single stepping
				 * silently until the syscall handler has
				 * saved the flags.
				 */
				goto out;
			}
			if (frame->tf_eip == (int)IDTVEC(lcall_syscall) + 1) {
				/*
				 * The syscall handler has now saved the
				 * flags.  Stop single stepping it.
				 */
				frame->tf_eflags &= ~PSL_T;
				goto out;
			}
			/*
			 * Ignore debug register trace traps due to
			 * accesses in the user's address space, which
			 * can happen under several conditions such as
			 * if a user sets a watchpoint on a buffer and
			 * then passes that buffer to a system call.
			 * We still want to get TRCTRAPS for addresses
			 * in kernel space because that is useful when
			 * debugging the kernel.
			 */
			/* XXX Giant */
			if (user_dbreg_trap() && 
			   !(PCPU_GET(curpcb)->pcb_flags & PCB_VM86CALL)) {
				/*
				 * Reset breakpoint bits because the
				 * processor doesn't
				 */
				load_dr6(rdr6() & 0xfffffff0);
				goto out;
			}
			/*
			 * FALLTHROUGH (TRCTRAP kernel mode, kernel address)
			 */
		case T_BPTFLT:
			/*
			 * If KDB is enabled, let it handle the debugger trap.
			 * Otherwise, debugger traps "can't happen".
			 */
#ifdef KDB
			if (kdb_trap(type, 0, frame))
				goto out;
#endif
			break;

#ifdef DEV_ISA
		case T_NMI:
#ifdef POWERFAIL_NMI
			mtx_lock(&Giant);
			if (time_second - lastalert > 10) {
				log(LOG_WARNING, "NMI: power fail\n");
				sysbeep(TIMER_FREQ/880, hz);
				lastalert = time_second;
			}
			mtx_unlock(&Giant);
			goto out;
#else /* !POWERFAIL_NMI */
			/* XXX Giant */
			/* machine/parity/power fail/"kitchen sink" faults */
			if (isa_nmi(code) == 0) {
#ifdef KDB
				/*
				 * NMI can be hooked up to a pushbutton
				 * for debugging.
				 */
				if (kdb_on_nmi) {
					printf ("NMI ... going to debugger\n");
					kdb_trap(type, 0, frame);
				}
#endif /* KDB */
				goto out;
			} else if (panic_on_nmi == 0)
				goto out;
			/* FALLTHROUGH */
#endif /* POWERFAIL_NMI */
#endif /* DEV_ISA */
		}

		trap_fatal(frame, eva);
		goto out;
	}

	/* Translate fault for emulators (e.g. Linux) */
	if (*p->p_sysent->sv_transtrap)
		i = (*p->p_sysent->sv_transtrap)(i, type);

	ksiginfo_init_trap(&ksi);
	ksi.ksi_signo = i;
	ksi.ksi_code = ucode;
	ksi.ksi_addr = (void *)addr;
	ksi.ksi_trapno = type;
	trapsignal(td, &ksi);

#ifdef DEBUG
	if (type <= MAX_TRAP_MSG) {
		uprintf("fatal process exception: %s",
			trap_msg[type]);
		if ((type == T_PAGEFLT) || (type == T_PROTFLT))
			uprintf(", fault VA = 0x%lx", (u_long)eva);
		uprintf("\n");
	}
#endif

user:
	userret(td, frame);
	mtx_assert(&Giant, MA_NOTOWNED);
userout:
out:
	return;
}

static int
trap_pfault(frame, usermode, eva)
	struct trapframe *frame;
	int usermode;
	vm_offset_t eva;
{
	vm_offset_t va;
	struct vmspace *vm = NULL;
	vm_map_t map;
	int rv = 0;
	vm_prot_t ftype;
	struct thread *td = curthread;
	struct proc *p = td->td_proc;

	va = trunc_page(eva);
	if (va >= KERNBASE) {
		/*
		 * Don't allow user-mode faults in kernel address space.
		 * An exception:  if the faulting address is the invalid
		 * instruction entry in the IDT, then the Intel Pentium
		 * F00F bug workaround was triggered, and we need to
		 * treat it is as an illegal instruction, and not a page
		 * fault.
		 */
#if defined(I586_CPU) && !defined(NO_F00F_HACK)
		if ((eva == (unsigned int)&idt[6]) && has_f00f_bug)
			return -2;
#endif
		if (usermode)
			goto nogo;

		map = kernel_map;
	} else {
		/*
		 * This is a fault on non-kernel virtual memory.
		 * vm is initialized above to NULL. If curproc is NULL
		 * or curproc->p_vmspace is NULL the fault is fatal.
		 */
		if (p != NULL)
			vm = p->p_vmspace;

		if (vm == NULL)
			goto nogo;

		map = &vm->vm_map;
	}

	/*
	 * PGEX_I is defined only if the execute disable bit capability is
	 * supported and enabled.
	 */
	if (frame->tf_err & PGEX_W)
		ftype = VM_PROT_WRITE;
#ifdef PAE
	else if ((frame->tf_err & PGEX_I) && pg_nx != 0)
		ftype = VM_PROT_EXECUTE;
#endif
	else
		ftype = VM_PROT_READ;

	if (map != kernel_map) {
		/*
		 * Keep swapout from messing with us during this
		 *	critical time.
		 */
		PROC_LOCK(p);
		++p->p_lock;
		PROC_UNLOCK(p);

		/* Fault in the user page: */
		rv = vm_fault(map, va, ftype,
			      (ftype & VM_PROT_WRITE) ? VM_FAULT_DIRTY
						      : VM_FAULT_NORMAL);

		PROC_LOCK(p);
		--p->p_lock;
		PROC_UNLOCK(p);
	} else {
		/*
		 * Don't have to worry about process locking or stacks in the
		 * kernel.
		 */
		rv = vm_fault(map, va, ftype, VM_FAULT_NORMAL);
	}
	if (rv == KERN_SUCCESS)
		return (0);
nogo:
	if (!usermode) {
		if (td->td_intr_nesting_level == 0 &&
		    PCPU_GET(curpcb)->pcb_onfault != NULL) {
			frame->tf_eip = (int)PCPU_GET(curpcb)->pcb_onfault;
			return (0);
		}
		trap_fatal(frame, eva);
		return (-1);
	}

	return((rv == KERN_PROTECTION_FAILURE) ? SIGBUS : SIGSEGV);
}

static void
trap_fatal(frame, eva)
	struct trapframe *frame;
	vm_offset_t eva;
{
	int code, ss, esp;
	u_int type;
	struct soft_segment_descriptor softseg;
	char *msg;

	code = frame->tf_err;
	type = frame->tf_trapno;
	sdtossd(&gdt[IDXSEL(frame->tf_cs & 0xffff)].sd, &softseg);

	if (type <= MAX_TRAP_MSG)
		msg = trap_msg[type];
	else
		msg = "UNKNOWN";
	printf("\n\nFatal trap %d: %s while in %s mode\n", type, msg,
	    frame->tf_eflags & PSL_VM ? "vm86" :
	    ISPL(frame->tf_cs) == SEL_UPL ? "user" : "kernel");
#ifdef SMP
	/* two separate prints in case of a trap on an unmapped page */
	printf("cpuid = %d; ", PCPU_GET(cpuid));
	printf("apic id = %02x\n", PCPU_GET(apic_id));
#endif
	if (type == T_PAGEFLT) {
		printf("fault virtual address	= 0x%x\n", eva);
		printf("fault code		= %s %s, %s\n",
			code & PGEX_U ? "user" : "supervisor",
			code & PGEX_W ? "write" : "read",
			code & PGEX_P ? "protection violation" : "page not present");
	}
	printf("instruction pointer	= 0x%x:0x%x\n",
	       frame->tf_cs & 0xffff, frame->tf_eip);
        if ((ISPL(frame->tf_cs) == SEL_UPL) || (frame->tf_eflags & PSL_VM)) {
		ss = frame->tf_ss & 0xffff;
		esp = frame->tf_esp;
	} else {
		ss = GSEL(GDATA_SEL, SEL_KPL);
		esp = (int)&frame->tf_esp;
	}
	printf("stack pointer	        = 0x%x:0x%x\n", ss, esp);
	printf("frame pointer	        = 0x%x:0x%x\n", ss, frame->tf_ebp);
	printf("code segment		= base 0x%x, limit 0x%x, type 0x%x\n",
	       softseg.ssd_base, softseg.ssd_limit, softseg.ssd_type);
	printf("			= DPL %d, pres %d, def32 %d, gran %d\n",
	       softseg.ssd_dpl, softseg.ssd_p, softseg.ssd_def32,
	       softseg.ssd_gran);
	printf("processor eflags	= ");
	if (frame->tf_eflags & PSL_T)
		printf("trace trap, ");
	if (frame->tf_eflags & PSL_I)
		printf("interrupt enabled, ");
	if (frame->tf_eflags & PSL_NT)
		printf("nested task, ");
	if (frame->tf_eflags & PSL_RF)
		printf("resume, ");
	if (frame->tf_eflags & PSL_VM)
		printf("vm86, ");
	printf("IOPL = %d\n", (frame->tf_eflags & PSL_IOPL) >> 12);
	printf("current process		= ");
	if (curproc) {
		printf("%lu (%s)\n", (u_long)curproc->p_pid, curproc->p_comm);
	} else {
		printf("Idle\n");
	}

#ifdef KDB
	if (debugger_on_panic || kdb_active) {
		frame->tf_err = eva;	/* smuggle fault address to ddb */
		if (kdb_trap(type, 0, frame)) {
			frame->tf_err = code;	/* restore error code */
			return;
		}
		frame->tf_err = code;		/* restore error code */
	}
#endif
	printf("trap number		= %d\n", type);
	if (type <= MAX_TRAP_MSG)
		panic("%s", trap_msg[type]);
	else
		panic("unknown/reserved trap");
}

/*
 * Double fault handler. Called when a fault occurs while writing
 * a frame for a trap/exception onto the stack. This usually occurs
 * when the stack overflows (such is the case with infinite recursion,
 * for example).
 *
 * XXX Note that the current PTD gets replaced by IdlePTD when the
 * task switch occurs. This means that the stack that was active at
 * the time of the double fault is not available at <kstack> unless
 * the machine was idle when the double fault occurred. The downside
 * of this is that "trace <ebp>" in ddb won't work.
 */
void
dblfault_handler()
{
	printf("\nFatal double fault:\n");
	printf("eip = 0x%x\n", PCPU_GET(common_tss.tss_eip));
	printf("esp = 0x%x\n", PCPU_GET(common_tss.tss_esp));
	printf("ebp = 0x%x\n", PCPU_GET(common_tss.tss_ebp));
#ifdef SMP
	/* two separate prints in case of a trap on an unmapped page */
	printf("cpuid = %d; ", PCPU_GET(cpuid));
	printf("apic id = %02x\n", PCPU_GET(apic_id));
#endif
	panic("double fault");
}

/*
 *	syscall -	system call request C handler
 *
 *	A system call is essentially treated as a trap.
 */
void
syscall(struct trapframe *frame)
{
	caddr_t params;
	struct sysent *callp;
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	register_t orig_tf_eflags;
	int error;
	int narg;
	int args[8];
	u_int code;
	ksiginfo_t ksi;

	PCPU_INC(cnt.v_syscall);

#ifdef DIAGNOSTIC
	if (ISPL(frame->tf_cs) != SEL_UPL) {
		panic("syscall");
		/* NOT REACHED */
	}
#endif

	td->td_pticks = 0;
	td->td_frame = frame;
	if (td->td_ucred != p->p_ucred) 
		cred_update_thread(td);
#ifdef KSE
	if (p->p_flag & P_SA)
		thread_user_enter(td);
#endif
	params = (caddr_t)frame->tf_esp + sizeof(int);
	code = frame->tf_eax;
	orig_tf_eflags = frame->tf_eflags;

	if (p->p_sysent->sv_prepsyscall) {
		/*
		 * The prep code is MP aware.
		 */
		(*p->p_sysent->sv_prepsyscall)(frame, args, &code, &params);
	} else {
		/*
		 * Need to check if this is a 32 bit or 64 bit syscall.
		 * fuword is MP aware.
		 */
		if (code == SYS_syscall) {
			/*
			 * Code is first argument, followed by actual args.
			 */
			code = fuword(params);
			params += sizeof(int);
		} else if (code == SYS___syscall) {
			/*
			 * Like syscall, but code is a quad, so as to maintain
			 * quad alignment for the rest of the arguments.
			 */
			code = fuword(params);
			params += sizeof(quad_t);
		}
	}

 	if (p->p_sysent->sv_mask)
 		code &= p->p_sysent->sv_mask;

 	if (code >= p->p_sysent->sv_size)
 		callp = &p->p_sysent->sv_table[0];
  	else
 		callp = &p->p_sysent->sv_table[code];

	narg = callp->sy_narg;

	/*
	 * copyin and the ktrsyscall()/ktrsysret() code is MP-aware
	 */
	if (params != NULL && narg != 0)
		error = copyin(params, (caddr_t)args,
		    (u_int)(narg * sizeof(int)));
	else
		error = 0;
		
#ifdef KTRACE
	if (KTRPOINT(td, KTR_SYSCALL))
		ktrsyscall(code, narg, args);
#endif

	CTR4(KTR_SYSC, "syscall enter thread %p pid %d proc %s code %d", td,
	    td->td_proc->p_pid, td->td_name, code);

	td->td_syscalls++;

	if (error == 0) {
		td->td_retval[0] = 0;
		td->td_retval[1] = frame->tf_edx;

		STOPEVENT(p, S_SCE, narg);

		PTRACESTOP_SC(p, td, S_PT_SCE);

		AUDIT_SYSCALL_ENTER(code, td);
		error = (*callp->sy_call)(td, args);
		AUDIT_SYSCALL_EXIT(error, td);
	}

	switch (error) {
	case 0:
		frame->tf_eax = td->td_retval[0];
		frame->tf_edx = td->td_retval[1];
		frame->tf_eflags &= ~PSL_C;
		break;

	case ERESTART:
		/*
		 * Reconstruct pc, assuming lcall $X,y is 7 bytes,
		 * int 0x80 is 2 bytes. We saved this in tf_err.
		 */
		frame->tf_eip -= frame->tf_err;
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
		frame->tf_eax = error;
		frame->tf_eflags |= PSL_C;
		break;
	}

	/*
	 * Traced syscall.
	 */
	if ((orig_tf_eflags & PSL_T) && !(orig_tf_eflags & PSL_VM)) {
		frame->tf_eflags &= ~PSL_T;
		ksiginfo_init_trap(&ksi);
		ksi.ksi_signo = SIGTRAP;
		ksi.ksi_code = TRAP_TRACE;
		ksi.ksi_addr = (void *)frame->tf_eip;
		trapsignal(td, &ksi);
	}

	/*
	 * Check for misbehavior.
	 */
	WITNESS_WARN(WARN_PANIC, NULL, "System call %s returning",
	    (code >= 0 && code < SYS_MAXSYSCALL) ? syscallnames[code] : "???");
	KASSERT(td->td_critnest == 0,
	    ("System call %s returning in a critical section",
	    (code >= 0 && code < SYS_MAXSYSCALL) ? syscallnames[code] : "???"));
	KASSERT(td->td_locks == 0,
	    ("System call %s returning with %d locks held",
	    (code >= 0 && code < SYS_MAXSYSCALL) ? syscallnames[code] : "???",
	    td->td_locks));

	/*
	 * Handle reschedule and other end-of-syscall issues
	 */
	userret(td, frame);

	CTR4(KTR_SYSC, "syscall exit thread %p pid %d proc %s code %d", td,
	    td->td_proc->p_pid, td->td_name, code);

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

	PTRACESTOP_SC(p, td, S_PT_SCX);
}

