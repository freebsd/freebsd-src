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
 * $FreeBSD$
 */

/*
 * 386 Trap and System call handling
 */

#include "opt_clock.h"
#include "opt_cpu.h"
#include "opt_ddb.h"
#include "opt_isa.h"
#include "opt_ktrace.h"
#include "opt_npx.h"
#include "opt_trap.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/pioctl.h>
#include <sys/ipl.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
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

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>

#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#ifdef SMP
#include <machine/smp.h>
#endif
#include <machine/tss.h>

#include <i386/isa/icu.h>
#include <i386/isa/intr_machdep.h>

#ifdef POWERFAIL_NMI
#include <sys/syslog.h>
#include <machine/clock.h>
#endif

#include <machine/vm86.h>

#include <ddb/ddb.h>

#include <sys/sysctl.h>

int (*pmath_emulate) __P((struct trapframe *));

extern void trap __P((struct trapframe frame));
extern int trapwrite __P((unsigned addr));
extern void syscall __P((struct trapframe frame));
extern void ast __P((struct trapframe *framep));

static int trap_pfault __P((struct trapframe *, int, vm_offset_t));
static void trap_fatal __P((struct trapframe *, vm_offset_t));
void dblfault_handler __P((void));

extern inthand_t IDTVEC(lcall_syscall);

#define MAX_TRAP_MSG		28
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
};

#if defined(I586_CPU) && !defined(NO_F00F_HACK)
extern int has_f00f_bug;
#endif

#ifdef DDB
static int ddb_on_nmi = 1;
SYSCTL_INT(_machdep, OID_AUTO, ddb_on_nmi, CTLFLAG_RW,
	&ddb_on_nmi, 0, "Go to DDB on NMI");
#endif
static int panic_on_nmi = 1;
SYSCTL_INT(_machdep, OID_AUTO, panic_on_nmi, CTLFLAG_RW,
	&panic_on_nmi, 0, "Panic on NMI");

#ifdef WITNESS
extern char *syscallnames[];
#endif

void
userret(p, frame, oticks)
	struct proc *p;
	struct trapframe *frame;
	u_quad_t oticks;
{
	int sig;

	while ((sig = CURSIG(p)) != 0)
		postsig(sig);

	mtx_lock_spin(&sched_lock);
	p->p_pri.pri_level = p->p_pri.pri_user;
	if (resched_wanted(p)) {
		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we setrunqueue ourselves but before we
		 * mi_switch()'ed, we might not be on the queue indicated by
		 * our priority.
		 */
		DROP_GIANT_NOSWITCH();
		setrunqueue(p);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		mtx_unlock_spin(&sched_lock);
		PICKUP_GIANT();
		while ((sig = CURSIG(p)) != 0)
			postsig(sig);
		mtx_lock_spin(&sched_lock);
	}

	/*
	 * Charge system time if profiling.
	 */
	if (p->p_sflag & PS_PROFIL) {
		mtx_unlock_spin(&sched_lock);
		/* XXX - do we need Giant? */
		if (!mtx_owned(&Giant))
			mtx_lock(&Giant);
		addupc_task(p, TRAPF_PC(frame),
			    (u_int)(p->p_sticks - oticks) * psratio);
	} else
		mtx_unlock_spin(&sched_lock);
}

/*
 * Exception, fault, and trap interface to the FreeBSD kernel.
 * This common code is called from assembly language IDT gate entry
 * routines that prepare a suitable stack frame, and restore this
 * frame after the exception has been processed.
 */

void
trap(frame)
	struct trapframe frame;
{
	struct proc *p = curproc;
	u_quad_t sticks = 0;
	int i = 0, ucode = 0, type, code;
	vm_offset_t eva;
#ifdef POWERFAIL_NMI
	static int lastalert = 0;
#endif

	atomic_add_int(&cnt.v_trap, 1);

	if ((frame.tf_eflags & PSL_I) == 0) {
		/*
		 * Buggy application or kernel code has disabled
		 * interrupts and then trapped.  Enabling interrupts
		 * now is wrong, but it is better than running with
		 * interrupts disabled until they are accidentally
		 * enabled later.  XXX This is really bad if we trap
		 * while holding a spin lock.
		 */
		type = frame.tf_trapno;
		if (ISPL(frame.tf_cs) == SEL_UPL || (frame.tf_eflags & PSL_VM))
			printf(
			    "pid %ld (%s): trap %d with interrupts disabled\n",
			    (long)curproc->p_pid, curproc->p_comm, type);
		else if (type != T_BPTFLT && type != T_TRCTRAP) {
			/*
			 * XXX not quite right, since this may be for a
			 * multiple fault in user mode.
			 */
			printf("kernel trap %d with interrupts disabled\n",
			    type);
			/*
			 * We should walk p_heldmtx here and see if any are
			 * spin mutexes, and not do this if so.
			 */
			enable_intr();
		}
	}

	eva = 0;

#if defined(I586_CPU) && !defined(NO_F00F_HACK)
restart:
#endif

	type = frame.tf_trapno;
	code = frame.tf_err;

        if ((ISPL(frame.tf_cs) == SEL_UPL) ||
	    ((frame.tf_eflags & PSL_VM) && !in_vm86call)) {
		/* user trap */

		mtx_lock_spin(&sched_lock);
		sticks = p->p_sticks;
		mtx_unlock_spin(&sched_lock);
		p->p_md.md_regs = &frame;

		switch (type) {
		case T_PRIVINFLT:	/* privileged instruction fault */
			ucode = type;
			i = SIGILL;
			break;

		case T_BPTFLT:		/* bpt instruction fault */
		case T_TRCTRAP:		/* trace trap */
			frame.tf_eflags &= ~PSL_T;
			i = SIGTRAP;
			break;

		case T_ARITHTRAP:	/* arithmetic trap */
			ucode = code;
			i = SIGFPE;
			break;

			/*
			 * The following two traps can happen in
			 * vm86 mode, and, if so, we want to handle
			 * them specially.
			 */
		case T_PROTFLT:		/* general protection fault */
		case T_STKFLT:		/* stack fault */
			if (frame.tf_eflags & PSL_VM) {
				mtx_lock(&Giant);
				i = vm86_emulate((struct vm86frame *)&frame);
				mtx_unlock(&Giant);
				if (i == 0)
					goto user;
				break;
			}
			/* FALL THROUGH */

		case T_SEGNPFLT:	/* segment not present fault */
		case T_TSSFLT:		/* invalid TSS fault */
		case T_DOUBLEFLT:	/* double fault */
		default:
			ucode = code + BUS_SEGM_FAULT ;
			i = SIGBUS;
			break;

		case T_PAGEFLT:		/* page fault */
			/*
			 * For some Cyrix CPUs, %cr2 is clobbered by
			 * interrupts.  This problem is worked around by using
			 * an interrupt gate for the pagefault handler.  We
			 * are finally ready to read %cr2 and then must
			 * reenable interrupts.
			 */
			eva = rcr2();
			enable_intr();
			mtx_lock(&Giant);
			i = trap_pfault(&frame, TRUE, eva);
			mtx_unlock(&Giant);
#if defined(I586_CPU) && !defined(NO_F00F_HACK)
			if (i == -2) {
				/*
				 * f00f hack workaround has triggered, treat
				 * as illegal instruction not page fault.
				 */
				frame.tf_trapno = T_PRIVINFLT;
				goto restart;
			}
#endif
			if (i == -1)
				goto out;
			if (i == 0)
				goto user;

			ucode = T_PAGEFLT;
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
			goto out;
#else /* !POWERFAIL_NMI */
			/* machine/parity/power fail/"kitchen sink" faults */
			/* XXX Giant */
			if (isa_nmi(code) == 0) {
#ifdef DDB
				/*
				 * NMI can be hooked up to a pushbutton
				 * for debugging.
				 */
				if (ddb_on_nmi) {
					printf ("NMI ... going to debugger\n");
					kdb_trap (type, 0, &frame);
				}
#endif /* DDB */
				goto out;
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
				goto out;
#endif
			if (!pmath_emulate) {
				i = SIGFPE;
				ucode = FPE_FPU_NP_TRAP;
				break;
			}
			mtx_lock(&Giant);
			i = (*pmath_emulate)(&frame);
			mtx_unlock(&Giant);
			if (i == 0) {
				if (!(frame.tf_eflags & PSL_T))
					goto out;
				frame.tf_eflags &= ~PSL_T;
				i = SIGTRAP;
			}
			/* else ucode = emulator_only_knows() XXX */
			break;

		case T_FPOPFLT:		/* FPU operand fetch fault */
			ucode = T_FPOPFLT;
			i = SIGILL;
			break;
		}
	} else {
		/* kernel trap */

		switch (type) {
		case T_PAGEFLT:			/* page fault */
			/*
			 * For some Cyrix CPUs, %cr2 is clobbered by
			 * interrupts.  This problem is worked around by using
			 * an interrupt gate for the pagefault handler.  We
			 * are finally ready to read %cr2 and then must
			 * reenable interrupts.
			 */
			eva = rcr2();
			enable_intr();
			mtx_lock(&Giant);
			(void) trap_pfault(&frame, FALSE, eva);
			mtx_unlock(&Giant);
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
			if (frame.tf_eflags & PSL_VM) {
				mtx_lock(&Giant);
				i = vm86_emulate((struct vm86frame *)&frame);
				mtx_unlock(&Giant);
				if (i != 0)
					/*
					 * returns to original process
					 */
					vm86_trap((struct vm86frame *)&frame);
				goto out;
			}
			if (type == T_STKFLT)
				break;

			/* FALL THROUGH */

		case T_SEGNPFLT:	/* segment not present fault */
			if (in_vm86call)
				break;

			if (p->p_intr_nesting_level != 0)
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
			if (frame.tf_eip == (int)cpu_switch_load_gs) {
				PCPU_GET(curpcb)->pcb_gs = 0;
				PROC_LOCK(p);
				psignal(p, SIGBUS);
				PROC_UNLOCK(p);
				goto out;
			}

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
			if (frame.tf_eip == (int)doreti_iret) {
				frame.tf_eip = (int)doreti_iret_fault;
				goto out;
			}
			if (frame.tf_eip == (int)doreti_popl_ds) {
				frame.tf_eip = (int)doreti_popl_ds_fault;
				goto out;
			}
			if (frame.tf_eip == (int)doreti_popl_es) {
				frame.tf_eip = (int)doreti_popl_es_fault;
				goto out;
			}
			if (frame.tf_eip == (int)doreti_popl_fs) {
				frame.tf_eip = (int)doreti_popl_fs_fault;
				goto out;
			}
			if (PCPU_GET(curpcb) != NULL &&
			    PCPU_GET(curpcb)->pcb_onfault != NULL) {
				frame.tf_eip =
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
			if (frame.tf_eflags & PSL_NT) {
				frame.tf_eflags &= ~PSL_NT;
				goto out;
			}
			break;

		case T_TRCTRAP:	 /* trace trap */
			if (frame.tf_eip == (int)IDTVEC(lcall_syscall)) {
				/*
				 * We've just entered system mode via the
				 * syscall lcall.  Continue single stepping
				 * silently until the syscall handler has
				 * saved the flags.
				 */
				goto out;
			}
			if (frame.tf_eip == (int)IDTVEC(lcall_syscall) + 1) {
				/*
				 * The syscall handler has now saved the
				 * flags.  Stop single stepping it.
				 */
				frame.tf_eflags &= ~PSL_T;
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
			if (user_dbreg_trap() && !in_vm86call) {
				/*
				 * Reset breakpoint bits because the
				 * processor doesn't
				 */
				load_dr6(rdr6() & 0xfffffff0);
				goto out;
			}
			/*
			 * Fall through (TRCTRAP kernel mode, kernel address)
			 */
		case T_BPTFLT:
			/*
			 * If DDB is enabled, let it handle the debugger trap.
			 * Otherwise, debugger traps "can't happen".
			 */
#ifdef DDB
			/* XXX Giant */
			if (kdb_trap (type, 0, &frame))
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
#ifdef DDB
				/*
				 * NMI can be hooked up to a pushbutton
				 * for debugging.
				 */
				if (ddb_on_nmi) {
					printf ("NMI ... going to debugger\n");
					kdb_trap (type, 0, &frame);
				}
#endif /* DDB */
				goto out;
			} else if (panic_on_nmi == 0)
				goto out;
			/* FALL THROUGH */
#endif /* POWERFAIL_NMI */
#endif /* DEV_ISA */
		}

		mtx_lock(&Giant);
		trap_fatal(&frame, eva);
		mtx_unlock(&Giant);
		goto out;
	}

	mtx_lock(&Giant);
	/* Translate fault for emulators (e.g. Linux) */
	if (*p->p_sysent->sv_transtrap)
		i = (*p->p_sysent->sv_transtrap)(i, type);

	trapsignal(p, i, ucode);

#ifdef DEBUG
	if (type <= MAX_TRAP_MSG) {
		uprintf("fatal process exception: %s",
			trap_msg[type]);
		if ((type == T_PAGEFLT) || (type == T_PROTFLT))
			uprintf(", fault VA = 0x%lx", (u_long)eva);
		uprintf("\n");
	}
#endif
	mtx_unlock(&Giant);

user:
	userret(p, &frame, sticks);
	if (mtx_owned(&Giant))
		mtx_unlock(&Giant);
out:
	return;
}

#ifdef notyet
/*
 * This version doesn't allow a page fault to user space while
 * in the kernel. The rest of the kernel needs to be made "safe"
 * before this can be used. I think the only things remaining
 * to be made safe are the iBCS2 code and the process tracing/
 * debugging code.
 */
static int
trap_pfault(frame, usermode, eva)
	struct trapframe *frame;
	int usermode;
	vm_offset_t eva;
{
	vm_offset_t va;
	struct vmspace *vm = NULL;
	vm_map_t map = 0;
	int rv = 0;
	vm_prot_t ftype;
	struct proc *p = curproc;

	if (frame->tf_err & PGEX_W)
		ftype = VM_PROT_WRITE;
	else
		ftype = VM_PROT_READ;

	va = trunc_page(eva);
	if (va < VM_MIN_KERNEL_ADDRESS) {
		vm_offset_t v;
		vm_page_t mpte;

		if (p == NULL ||
		    (!usermode && va < VM_MAXUSER_ADDRESS &&
		     (p->p_intr_nesting_level != 0 ||
		      PCPU_GET(curpcb) == NULL ||
		      PCPU_GET(curpcb)->pcb_onfault == NULL))) {
			trap_fatal(frame, eva);
			return (-1);
		}

		/*
		 * This is a fault on non-kernel virtual memory.
		 * vm is initialized above to NULL. If curproc is NULL
		 * or curproc->p_vmspace is NULL the fault is fatal.
		 */
		vm = p->p_vmspace;
		if (vm == NULL)
			goto nogo;

		map = &vm->vm_map;

		/*
		 * Keep swapout from messing with us during this
		 *	critical time.
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
			      (ftype & VM_PROT_WRITE) ? VM_FAULT_DIRTY
						      : VM_FAULT_NORMAL);

		PROC_LOCK(p);
		--p->p_lock;
		PROC_UNLOCK(p);
	} else {
		/*
		 * Don't allow user-mode faults in kernel address space.
		 */
		if (usermode)
			goto nogo;

		/*
		 * Since we know that kernel virtual address addresses
		 * always have pte pages mapped, we just have to fault
		 * the page.
		 */
		rv = vm_fault(kernel_map, va, ftype, VM_FAULT_NORMAL);
	}

	if (rv == KERN_SUCCESS)
		return (0);
nogo:
	if (!usermode) {
		if (p->p_intr_nesting_level == 0 &&
		    PCPU_GET(curpcb) != NULL &&
		    PCPU_GET(curpcb)->pcb_onfault != NULL) {
			frame->tf_eip = (int)PCPU_GET(curpcb)->pcb_onfault;
			return (0);
		}
		trap_fatal(frame, eva);
		return (-1);
	}

	/* kludge to pass faulting virtual address to sendsig */
	frame->tf_err = eva;

	return((rv == KERN_PROTECTION_FAILURE) ? SIGBUS : SIGSEGV);
}
#endif

int
trap_pfault(frame, usermode, eva)
	struct trapframe *frame;
	int usermode;
	vm_offset_t eva;
{
	vm_offset_t va;
	struct vmspace *vm = NULL;
	vm_map_t map = 0;
	int rv = 0;
	vm_prot_t ftype;
	struct proc *p = curproc;

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

	if (frame->tf_err & PGEX_W)
		ftype = VM_PROT_WRITE;
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
		if (p->p_intr_nesting_level == 0 &&
		    PCPU_GET(curpcb) != NULL &&
		    PCPU_GET(curpcb)->pcb_onfault != NULL) {
			frame->tf_eip = (int)PCPU_GET(curpcb)->pcb_onfault;
			return (0);
		}
		trap_fatal(frame, eva);
		return (-1);
	}

	/* kludge to pass faulting virtual address to sendsig */
	frame->tf_err = eva;

	return((rv == KERN_PROTECTION_FAILURE) ? SIGBUS : SIGSEGV);
}

static void
trap_fatal(frame, eva)
	struct trapframe *frame;
	vm_offset_t eva;
{
	int code, type, ss, esp;
	struct soft_segment_descriptor softseg;

	code = frame->tf_err;
	type = frame->tf_trapno;
	sdtossd(&gdt[IDXSEL(frame->tf_cs & 0xffff)].sd, &softseg);

	if (type <= MAX_TRAP_MSG)
		printf("\n\nFatal trap %d: %s while in %s mode\n",
			type, trap_msg[type],
        		frame->tf_eflags & PSL_VM ? "vm86" :
			ISPL(frame->tf_cs) == SEL_UPL ? "user" : "kernel");
#ifdef SMP
	/* two separate prints in case of a trap on an unmapped page */
	printf("cpuid = %d; ", PCPU_GET(cpuid));
	printf("lapic.id = %08x\n", lapic.id);
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
		printf("%lu (%s)\n",
		    (u_long)curproc->p_pid, curproc->p_comm ?
		    curproc->p_comm : "");
	} else {
		printf("Idle\n");
	}

#ifdef KDB
	if (kdb_trap(&psl))
		return;
#endif
#ifdef DDB
	if ((debugger_on_panic || db_active) && kdb_trap(type, 0, frame))
		return;
#endif
	printf("trap number		= %d\n", type);
	if (type <= MAX_TRAP_MSG)
		panic(trap_msg[type]);
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
	printf("lapic.id = %08x\n", lapic.id);
#endif
	panic("double fault");
}

/*
 * Compensate for 386 brain damage (missing URKR).
 * This is a little simpler than the pagefault handler in trap() because
 * it the page tables have already been faulted in and high addresses
 * are thrown out early for other reasons.
 */
int trapwrite(addr)
	unsigned addr;
{
	struct proc *p;
	vm_offset_t va;
	struct vmspace *vm;
	int rv;

	va = trunc_page((vm_offset_t)addr);
	/*
	 * XXX - MAX is END.  Changed > to >= for temp. fix.
	 */
	if (va >= VM_MAXUSER_ADDRESS)
		return (1);

	p = curproc;
	vm = p->p_vmspace;

	PROC_LOCK(p);
	++p->p_lock;
	PROC_UNLOCK(p);

	if (!grow_stack (p, va))
		rv = KERN_FAILURE;
	else
		/*
		 * fault the data page
		 */
		rv = vm_fault(&vm->vm_map, va, VM_PROT_WRITE, VM_FAULT_DIRTY);

	PROC_LOCK(p);
	--p->p_lock;
	PROC_UNLOCK(p);

	if (rv != KERN_SUCCESS)
		return 1;

	return (0);
}

/*
 *	syscall -	MP aware system call request C handler
 *
 *	A system call is essentially treated as a trap except that the
 *	MP lock is not held on entry or return.  We are responsible for
 *	obtaining the MP lock if necessary and for handling ASTs
 *	(e.g. a task switch) prior to return.
 *
 *	In general, only simple access and manipulation of curproc and
 *	the current stack is allowed without having to hold MP lock.
 */
void
syscall(frame)
	struct trapframe frame;
{
	caddr_t params;
	int i;
	struct sysent *callp;
	struct proc *p = curproc;
	u_quad_t sticks;
	int error;
	int narg;
	int args[8];
	u_int code;

	atomic_add_int(&cnt.v_syscall, 1);

#ifdef DIAGNOSTIC
	if (ISPL(frame.tf_cs) != SEL_UPL) {
		mtx_lock(&Giant);
		panic("syscall");
		/* NOT REACHED */
	}
#endif

	mtx_lock_spin(&sched_lock);
	sticks = p->p_sticks;
	mtx_unlock_spin(&sched_lock);

	p->p_md.md_regs = &frame;
	params = (caddr_t)frame.tf_esp + sizeof(int);
	code = frame.tf_eax;

	if (p->p_sysent->sv_prepsyscall) {
		/*
		 * The prep code is not MP aware.
		 */
		mtx_lock(&Giant);
		(*p->p_sysent->sv_prepsyscall)(&frame, args, &code, &params);
		mtx_unlock(&Giant);
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

	narg = callp->sy_narg & SYF_ARGMASK;

	/*
	 * copyin is MP aware, but the tracing code is not
	 */
	if (params && (i = narg * sizeof(int)) &&
	    (error = copyin(params, (caddr_t)args, (u_int)i))) {
		mtx_lock(&Giant);
#ifdef KTRACE
		if (KTRPOINT(p, KTR_SYSCALL))
			ktrsyscall(p->p_tracep, code, narg, args);
#endif
		goto bad;
	}

	/*
	 * Try to run the syscall without the MP lock if the syscall
	 * is MP safe.  We have to obtain the MP lock no matter what if 
	 * we are ktracing
	 */
	if ((callp->sy_narg & SYF_MPSAFE) == 0) {
		mtx_lock(&Giant);
	}

#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL)) {
		if (!mtx_owned(&Giant))
			mtx_lock(&Giant);
		ktrsyscall(p->p_tracep, code, narg, args);
	}
#endif
	p->p_retval[0] = 0;
	p->p_retval[1] = frame.tf_edx;

	STOPEVENT(p, S_SCE, narg);	/* MP aware */

	error = (*callp->sy_call)(p, args);

	/*
	 * MP SAFE (we may or may not have the MP lock at this point)
	 */
	switch (error) {
	case 0:
		frame.tf_eax = p->p_retval[0];
		frame.tf_edx = p->p_retval[1];
		frame.tf_eflags &= ~PSL_C;
		break;

	case ERESTART:
		/*
		 * Reconstruct pc, assuming lcall $X,y is 7 bytes,
		 * int 0x80 is 2 bytes. We saved this in tf_err.
		 */
		frame.tf_eip -= frame.tf_err;
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
		frame.tf_eax = error;
		frame.tf_eflags |= PSL_C;
		break;
	}

	/*
	 * Traced syscall.  trapsignal() is not MP aware.
	 */
	if ((frame.tf_eflags & PSL_T) && !(frame.tf_eflags & PSL_VM)) {
		if (!mtx_owned(&Giant))
			mtx_lock(&Giant);
		frame.tf_eflags &= ~PSL_T;
		trapsignal(p, SIGTRAP, 0);
	}

	/*
	 * Handle reschedule and other end-of-syscall issues
	 */
	userret(p, &frame, sticks);

#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET)) {
		if (!mtx_owned(&Giant))
			mtx_lock(&Giant);
		ktrsysret(p->p_tracep, code, error, p->p_retval[0]);
	}
#endif

	/*
	 * Release Giant if we had to get it
	 */
	if (mtx_owned(&Giant))
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

void
ast(framep)
	struct trapframe *framep;
{
	struct proc *p = CURPROC;
	u_quad_t sticks;

	KASSERT(TRAPF_USERMODE(framep), ("ast in kernel mode"));

	/*
	 * We check for a pending AST here rather than in the assembly as
	 * acquiring and releasing mutexes in assembly is not fun.
	 */
	mtx_lock_spin(&sched_lock);
	if (!(astpending(p) || resched_wanted(p))) {
		mtx_unlock_spin(&sched_lock);
		return;
	}

	sticks = p->p_sticks;
	p->p_md.md_regs = framep;

	astoff(p);
	cnt.v_soft++;
	mtx_intr_enable(&sched_lock);
	if (p->p_sflag & PS_OWEUPC) {
		p->p_sflag &= ~PS_OWEUPC;
		mtx_unlock_spin(&sched_lock);
		mtx_lock(&Giant);
		mtx_lock_spin(&sched_lock);
		addupc_task(p, p->p_stats->p_prof.pr_addr,
			    p->p_stats->p_prof.pr_ticks);
	}
	if (p->p_sflag & PS_ALRMPEND) {
		p->p_sflag &= ~PS_ALRMPEND;
		mtx_unlock_spin(&sched_lock);
		PROC_LOCK(p);
		psignal(p, SIGVTALRM);
		PROC_UNLOCK(p);
		mtx_lock_spin(&sched_lock);
	}
	if (p->p_sflag & PS_PROFPEND) {
		p->p_sflag &= ~PS_PROFPEND;
		mtx_unlock_spin(&sched_lock);
		PROC_LOCK(p);
		psignal(p, SIGPROF);
		PROC_UNLOCK(p);
	} else
		mtx_unlock_spin(&sched_lock);

	userret(p, framep, sticks);

	if (mtx_owned(&Giant))
		mtx_unlock(&Giant);
}
