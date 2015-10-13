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
#include "opt_npx.h"
#include "opt_stack.h"
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
#ifdef HWPMC_HOOKS
#include <sys/pmckern.h>
PMC_SOFT_DEFINE( , , page_fault, all);
PMC_SOFT_DEFINE( , , page_fault, read);
PMC_SOFT_DEFINE( , , page_fault, write);
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
#include <x86/mca.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#ifdef SMP
#include <machine/smp.h>
#endif
#include <machine/stack.h>
#include <machine/tss.h>
#include <machine/vm86.h>

#ifdef POWERFAIL_NMI
#include <sys/syslog.h>
#include <machine/clock.h>
#endif

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>
#endif

extern void trap(struct trapframe *frame);
extern void syscall(struct trapframe *frame);

static int trap_pfault(struct trapframe *, int, vm_offset_t);
static void trap_fatal(struct trapframe *, vm_offset_t);
void dblfault_handler(void);

extern inthand_t IDTVEC(lcall_syscall);

#define MAX_TRAP_MSG		32
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
	"",					/* 31 unused (reserved) */
	"DTrace pid return trap",               /* 32 T_DTRACE_RET */
};

#if defined(I586_CPU) && !defined(NO_F00F_HACK)
int has_f00f_bug = 0;		/* Initialized so that it can be patched. */
#endif

#ifdef KDB
static int kdb_on_nmi = 1;
SYSCTL_INT(_machdep, OID_AUTO, kdb_on_nmi, CTLFLAG_RWTUN,
	&kdb_on_nmi, 0, "Go to KDB on NMI");
#endif
static int panic_on_nmi = 1;
SYSCTL_INT(_machdep, OID_AUTO, panic_on_nmi, CTLFLAG_RWTUN,
	&panic_on_nmi, 0, "Panic on NMI");
static int prot_fault_translation = 0;
SYSCTL_INT(_machdep, OID_AUTO, prot_fault_translation, CTLFLAG_RW,
	&prot_fault_translation, 0, "Select signal to deliver on protection fault");
static int uprintf_signal;
SYSCTL_INT(_machdep, OID_AUTO, uprintf_signal, CTLFLAG_RW,
    &uprintf_signal, 0,
    "Print debugging information on trap signal to ctty");

/*
 * Exception, fault, and trap interface to the FreeBSD kernel.
 * This common code is called from assembly language IDT gate entry
 * routines that prepare a suitable stack frame, and restore this
 * frame after the exception has been processed.
 */

void
trap(struct trapframe *frame)
{
#ifdef KDTRACE_HOOKS
	struct reg regs;
#endif
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
	/* Handler for NMI IPIs used for stopping CPUs. */
	if (type == T_NMI) {
	         if (ipi_nmi_handler() == 0)
	                   goto out;
	}
#endif /* SMP */

#ifdef KDB
	if (kdb_active) {
		kdb_reenter();
		goto out;
	}
#endif

	if (type == T_RESERVED) {
		trap_fatal(frame, 0);
		goto out;
	}

	if (type == T_NMI) {
#ifdef HWPMC_HOOKS
		/*
		 * CPU PMCs interrupt using an NMI so we check for that first.
		 * If the HWPMC module is active, 'pmc_hook' will point to
		 * the function to be called.  A non-zero return value from the
		 * hook means that the NMI was consumed by it and that we can
		 * return immediately.
		 */
		if (pmc_intr != NULL &&
		    (*pmc_intr)(PCPU_GET(cpuid), frame) != 0)
			goto out;
#endif

#ifdef STACK
		if (stack_nmi_handler(frame) != 0)
			goto out;
#endif
	}

	if (type == T_MCHK) {
		mca_intr();
		goto out;
	}

#ifdef KDTRACE_HOOKS
	/*
	 * A trap can occur while DTrace executes a probe. Before
	 * executing the probe, DTrace blocks re-scheduling and sets
	 * a flag in its per-cpu flags to indicate that it doesn't
	 * want to fault. On returning from the probe, the no-fault
	 * flag is cleared and finally re-scheduling is enabled.
	 */
	if ((type == T_PROTFLT || type == T_PAGEFLT) &&
	    dtrace_trap_func != NULL && (*dtrace_trap_func)(frame, type))
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
			uprintf(
			    "pid %ld (%s): trap %d with interrupts disabled\n",
			    (long)curproc->p_pid, curthread->td_name, type);
		else if (type != T_NMI && type != T_BPTFLT &&
		    type != T_TRCTRAP &&
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
			 * a spin lock.
			 */
			if (type != T_PAGEFLT &&
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
		 * are finally ready to read %cr2 and conditionally
		 * reenable interrupts.  If we hold a spin lock, then
		 * we must not reenable interrupts.  This might be a
		 * spurious page fault.
		 */
		eva = rcr2();
		if (td->td_md.md_spinlock_count == 0)
			enable_intr();
	}

        if ((ISPL(frame->tf_cs) == SEL_UPL) ||
	    ((frame->tf_eflags & PSL_VM) && 
		!(curpcb->pcb_flags & PCB_VM86CALL))) {
		/* user trap */

		td->td_pticks = 0;
		td->td_frame = frame;
		addr = frame->tf_eip;
		if (td->td_cowgen != p->p_cowgen)
			thread_cow_update(td);

		switch (type) {
		case T_PRIVINFLT:	/* privileged instruction fault */
			i = SIGILL;
			ucode = ILL_PRVOPC;
			break;

		case T_BPTFLT:		/* bpt instruction fault */
		case T_TRCTRAP:		/* trace trap */
			enable_intr();
#ifdef KDTRACE_HOOKS
			if (type == T_BPTFLT) {
				fill_frame_regs(frame, &regs);
				if (dtrace_pid_probe_ptr != NULL &&
				    dtrace_pid_probe_ptr(&regs) == 0)
					goto out;
			}
#endif
			frame->tf_eflags &= ~PSL_T;
			i = SIGTRAP;
			ucode = (type == T_TRCTRAP ? TRAP_TRACE : TRAP_BRKPT);
			break;

		case T_ARITHTRAP:	/* arithmetic trap */
#ifdef DEV_NPX
			ucode = npxtrap_x87();
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
		case T_ALIGNFLT:
			i = SIGBUS;
			ucode = BUS_ADRALN;
			break;
		case T_DOUBLEFLT:	/* double fault */
		default:
			i = SIGBUS;
			ucode = BUS_OBJERR;
			break;

		case T_PAGEFLT:		/* page fault */

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
				if (prot_fault_translation == 0) {
					/*
					 * Autodetect.
					 * This check also covers the images
					 * without the ABI-tag ELF note.
					 */
					if (SV_CURPROC_ABI() == SV_ABI_FREEBSD
					    && p->p_osrel >= P_OSREL_SIGSEGV) {
						i = SIGSEGV;
						ucode = SEGV_ACCERR;
					} else {
						i = SIGBUS;
						ucode = BUS_PAGE_FAULT;
					}
				} else if (prot_fault_translation == 1) {
					/*
					 * Always compat mode.
					 */
					i = SIGBUS;
					ucode = BUS_PAGE_FAULT;
				} else {
					/*
					 * Always SIGSEGV mode.
					 */
					i = SIGSEGV;
					ucode = SEGV_ACCERR;
				}
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
			if (time_second - lastalert > 10) {
				log(LOG_WARNING, "NMI: power fail\n");
				sysbeep(880, hz);
				lastalert = time_second;
			}
			goto userout;
#else /* !POWERFAIL_NMI */
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
			KASSERT(PCB_USER_FPU(td->td_pcb),
			    ("kernel FPU ctx has leaked"));
			/* transparent fault (due to context switch "late") */
			if (npxdna())
				goto userout;
#endif
			uprintf("pid %d killed due to lack of floating point\n",
				p->p_pid);
			i = SIGKILL;
			ucode = 0;
			break;

		case T_FPOPFLT:		/* FPU operand fetch fault */
			ucode = ILL_COPROC;
			i = SIGILL;
			break;

		case T_XMMFLT:		/* SIMD floating-point exception */
#if defined(DEV_NPX) && !defined(CPU_DISABLE_SSE) && defined(I686_CPU)
			ucode = npxtrap_sse();
			if (ucode == -1)
				goto userout;
#else
			ucode = 0;
#endif
			i = SIGFPE;
			break;
#ifdef KDTRACE_HOOKS
		case T_DTRACE_RET:
			enable_intr();
			fill_frame_regs(frame, &regs);
			if (dtrace_return_probe_ptr != NULL &&
			    dtrace_return_probe_ptr(&regs) == 0)
				goto out;
			break;
#endif
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
			KASSERT(!PCB_USER_FPU(td->td_pcb),
			    ("Unregistered use of FPU in kernel"));
			if (npxdna())
				goto out;
#endif
			break;

		case T_ARITHTRAP:	/* arithmetic trap */
		case T_XMMFLT:		/* SIMD floating-point exception */
		case T_FPOPFLT:		/* FPU operand fetch fault */
			/*
			 * XXXKIB for now disable any FPU traps in kernel
			 * handler registration seems to be overkill
			 */
			trap_fatal(frame, 0);
			goto out;

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
			if (curpcb->pcb_flags & PCB_VM86CALL)
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
				curpcb->pcb_gs = 0;
#if 0				
				PROC_LOCK(p);
				kern_psignal(p, SIGBUS);
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
			if (curpcb->pcb_onfault != NULL) {
				frame->tf_eip =
				    (int)curpcb->pcb_onfault;
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
			if (user_dbreg_trap() && 
			   !(curpcb->pcb_flags & PCB_VM86CALL)) {
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
			if (time_second - lastalert > 10) {
				log(LOG_WARNING, "NMI: power fail\n");
				sysbeep(880, hz);
				lastalert = time_second;
			}
			goto out;
#else /* !POWERFAIL_NMI */
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
	if (uprintf_signal) {
		uprintf("pid %d comm %s: signal %d err %x code %d type %d "
		    "addr 0x%x esp 0x%08x eip 0x%08x "
		    "<%02x %02x %02x %02x %02x %02x %02x %02x>\n",
		    p->p_pid, p->p_comm, i, frame->tf_err, ucode, type, addr,
		    frame->tf_esp, frame->tf_eip,
		    fubyte((void *)(frame->tf_eip + 0)),
		    fubyte((void *)(frame->tf_eip + 1)),
		    fubyte((void *)(frame->tf_eip + 2)),
		    fubyte((void *)(frame->tf_eip + 3)),
		    fubyte((void *)(frame->tf_eip + 4)),
		    fubyte((void *)(frame->tf_eip + 5)),
		    fubyte((void *)(frame->tf_eip + 6)),
		    fubyte((void *)(frame->tf_eip + 7)));
	}
	KASSERT((read_eflags() & PSL_I) != 0, ("interrupts disabled"));
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
	KASSERT(PCB_USER_FPU(td->td_pcb),
	    ("Return from trap with kernel FPU ctx leaked"));
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
	vm_map_t map;
	int rv = 0;
	vm_prot_t ftype;
	struct thread *td = curthread;
	struct proc *p = td->td_proc;

	if (__predict_false((td->td_pflags & TDP_NOFAULTING) != 0)) {
		/*
		 * Due to both processor errata and lazy TLB invalidation when
		 * access restrictions are removed from virtual pages, memory
		 * accesses that are allowed by the physical mapping layer may
		 * nonetheless cause one spurious page fault per virtual page. 
		 * When the thread is executing a "no faulting" section that
		 * is bracketed by vm_fault_{disable,enable}_pagefaults(),
		 * every page fault is treated as a spurious page fault,
		 * unless it accesses the same virtual address as the most
		 * recent page fault within the same "no faulting" section.
		 */
		if (td->td_md.md_spurflt_addr != eva ||
		    (td->td_pflags & TDP_RESETSPUR) != 0) {
			/*
			 * Do nothing to the TLB.  A stale TLB entry is
			 * flushed automatically by a page fault.
			 */
			td->td_md.md_spurflt_addr = eva;
			td->td_pflags &= ~TDP_RESETSPUR;
			return (0);
		}
	} else {
		/*
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
		if (td->td_critnest != 0 ||
		    WITNESS_CHECK(WARN_SLEEPOK | WARN_GIANTOK, NULL,
		    "Kernel page fault") != 0) {
			trap_fatal(frame, eva);
			return (-1);
		}
	}
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
			return (-2);
#endif
		if (usermode)
			goto nogo;

		map = kernel_map;
	} else {
		map = &p->p_vmspace->vm_map;

		/*
		 * When accessing a user-space address, kernel must be
		 * ready to accept the page fault, and provide a
		 * handling routine.  Since accessing the address
		 * without the handler is a bug, do not try to handle
		 * it normally, and panic immediately.
		 */
		if (!usermode && (td->td_intr_nesting_level != 0 ||
		    curpcb->pcb_onfault == NULL)) {
			trap_fatal(frame, eva);
			return (-1);
		}
	}

	/*
	 * PGEX_I is defined only if the execute disable bit capability is
	 * supported and enabled.
	 */
	if (frame->tf_err & PGEX_W)
		ftype = VM_PROT_WRITE;
#if defined(PAE) || defined(PAE_TABLES)
	else if ((frame->tf_err & PGEX_I) && pg_nx != 0)
		ftype = VM_PROT_EXECUTE;
#endif
	else
		ftype = VM_PROT_READ;

	/* Fault in the page. */
	rv = vm_fault(map, va, ftype, VM_FAULT_NORMAL);
	if (rv == KERN_SUCCESS) {
#ifdef HWPMC_HOOKS
		if (ftype == VM_PROT_READ || ftype == VM_PROT_WRITE) {
			PMC_SOFT_CALL_TF( , , page_fault, all, frame);
			if (ftype == VM_PROT_READ)
				PMC_SOFT_CALL_TF( , , page_fault, read,
				    frame);
			else
				PMC_SOFT_CALL_TF( , , page_fault, write,
				    frame);
		}
#endif
		return (0);
	}
nogo:
	if (!usermode) {
		if (td->td_intr_nesting_level == 0 &&
		    curpcb->pcb_onfault != NULL) {
			frame->tf_eip = (int)curpcb->pcb_onfault;
			return (0);
		}
		trap_fatal(frame, eva);
		return (-1);
	}
	return ((rv == KERN_PROTECTION_FAILURE) ? SIGBUS : SIGSEGV);
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
	printf("current process		= %d (%s)\n",
	    curproc->p_pid, curthread->td_name);

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
#ifdef KDTRACE_HOOKS
	if (dtrace_doubletrap_func != NULL)
		(*dtrace_doubletrap_func)();
#endif
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

int
cpu_fetch_syscall_args(struct thread *td, struct syscall_args *sa)
{
	struct proc *p;
	struct trapframe *frame;
	caddr_t params;
	long tmp;
	int error;

	p = td->td_proc;
	frame = td->td_frame;

	params = (caddr_t)frame->tf_esp + sizeof(int);
	sa->code = frame->tf_eax;

	/*
	 * Need to check if this is a 32 bit or 64 bit syscall.
	 */
	if (sa->code == SYS_syscall) {
		/*
		 * Code is first argument, followed by actual args.
		 */
		error = fueword(params, &tmp);
		if (error == -1)
			return (EFAULT);
		sa->code = tmp;
		params += sizeof(int);
	} else if (sa->code == SYS___syscall) {
		/*
		 * Like syscall, but code is a quad, so as to maintain
		 * quad alignment for the rest of the arguments.
		 */
		error = fueword(params, &tmp);
		if (error == -1)
			return (EFAULT);
		sa->code = tmp;
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
		error = copyin(params, (caddr_t)sa->args,
		    (u_int)(sa->narg * sizeof(int)));
	else
		error = 0;

	if (error == 0) {
		td->td_retval[0] = 0;
		td->td_retval[1] = frame->tf_edx;
	}
		
	return (error);
}

#include "../../kern/subr_syscall.c"

/*
 * syscall - system call request C handler.  A system call is
 * essentially treated as a trap by reusing the frame layout.
 */
void
syscall(struct trapframe *frame)
{
	struct thread *td;
	struct syscall_args sa;
	register_t orig_tf_eflags;
	int error;
	ksiginfo_t ksi;

#ifdef DIAGNOSTIC
	if (ISPL(frame->tf_cs) != SEL_UPL) {
		panic("syscall");
		/* NOT REACHED */
	}
#endif
	orig_tf_eflags = frame->tf_eflags;

	td = curthread;
	td->td_frame = frame;

	error = syscallenter(td, &sa);

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

	KASSERT(PCB_USER_FPU(td->td_pcb),
	    ("System call %s returning with kernel FPU ctx leaked",
	     syscallname(td->td_proc, sa.code)));
	KASSERT(td->td_pcb->pcb_save == get_pcb_user_save_td(td),
	    ("System call %s returning with mangled pcb_save",
	     syscallname(td->td_proc, sa.code)));

	syscallret(td, error, &sa);
}
