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
 * AMD64 Trap and System call handling
 */

#include "opt_clock.h"
#include "opt_cpu.h"
#include "opt_hwpmc_hooks.h"
#include "opt_isa.h"
#include "opt_kdb.h"
#include "opt_stack.h"

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

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>
#endif

extern void __noinline trap(struct trapframe *frame);
extern void trap_check(struct trapframe *frame);
extern void syscall(struct trapframe *frame);
void dblfault_handler(struct trapframe *frame);

static int trap_pfault(struct trapframe *, int);
static void trap_fatal(struct trapframe *, vm_offset_t);

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
	"DTrace pid return trap",		/* 32 T_DTRACE_RET */
};

#ifdef KDB
static int kdb_on_nmi = 1;
SYSCTL_INT(_machdep, OID_AUTO, kdb_on_nmi, CTLFLAG_RWTUN,
	&kdb_on_nmi, 0, "Go to KDB on NMI");
#endif
static int panic_on_nmi = 1;
SYSCTL_INT(_machdep, OID_AUTO, panic_on_nmi, CTLFLAG_RWTUN,
	&panic_on_nmi, 0, "Panic on NMI");
static int prot_fault_translation;
SYSCTL_INT(_machdep, OID_AUTO, prot_fault_translation, CTLFLAG_RWTUN,
    &prot_fault_translation, 0,
    "Select signal to deliver on protection fault");
static int uprintf_signal;
SYSCTL_INT(_machdep, OID_AUTO, uprintf_signal, CTLFLAG_RWTUN,
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
	ksiginfo_t ksi;

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
		 * CPU PMCs interrupt using an NMI.  If the PMC module is
		 * active, pass the 'rip' value to the PMC module's interrupt
		 * handler.  A non-zero return value from the handler means that
		 * the NMI was consumed by it and we can return immediately.
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

	if ((frame->tf_rflags & PSL_I) == 0) {
		/*
		 * Buggy application or kernel code has disabled
		 * interrupts and then trapped.  Enabling interrupts
		 * now is wrong, but it is better than running with
		 * interrupts disabled until they are accidentally
		 * enabled later.
		 */
		if (TRAPF_USERMODE(frame))
			uprintf(
			    "pid %ld (%s): trap %d with interrupts disabled\n",
			    (long)curproc->p_pid, curthread->td_name, type);
		else if (type != T_NMI && type != T_BPTFLT &&
		    type != T_TRCTRAP) {
			/*
			 * XXX not quite right, since this may be for a
			 * multiple fault in user mode.
			 */
			printf("kernel trap %d with interrupts disabled\n",
			    type);

			/*
			 * We shouldn't enable interrupts while holding a
			 * spin lock.
			 */
			if (td->td_md.md_spinlock_count == 0)
				enable_intr();
		}
	}

	code = frame->tf_err;

	if (TRAPF_USERMODE(frame)) {
		/* user trap */

		td->td_pticks = 0;
		td->td_frame = frame;
		addr = frame->tf_rip;
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
			frame->tf_rflags &= ~PSL_T;
			i = SIGTRAP;
			ucode = (type == T_TRCTRAP ? TRAP_TRACE : TRAP_BRKPT);
			break;

		case T_ARITHTRAP:	/* arithmetic trap */
			ucode = fputrap_x87();
			if (ucode == -1)
				goto userout;
			i = SIGFPE;
			break;

		case T_PROTFLT:		/* general protection fault */
			i = SIGBUS;
			ucode = BUS_OBJERR;
			break;
		case T_STKFLT:		/* stack fault */
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
			/*
			 * Emulator can take care about this trap?
			 */
			if (*p->p_sysent->sv_trap != NULL &&
			    (*p->p_sysent->sv_trap)(td) == 0)
				goto userout;

			addr = frame->tf_addr;
			i = trap_pfault(frame, TRUE);
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
			break;

		case T_DIVIDE:		/* integer divide fault */
			ucode = FPE_INTDIV;
			i = SIGFPE;
			break;

#ifdef DEV_ISA
		case T_NMI:
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
			/* transparent fault (due to context switch "late") */
			KASSERT(PCB_USER_FPU(td->td_pcb),
			    ("kernel FPU ctx has leaked"));
			fpudna();
			goto userout;

		case T_FPOPFLT:		/* FPU operand fetch fault */
			ucode = ILL_COPROC;
			i = SIGILL;
			break;

		case T_XMMFLT:		/* SIMD floating-point exception */
			ucode = fputrap_sse();
			if (ucode == -1)
				goto userout;
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
			(void) trap_pfault(frame, FALSE);
			goto out;

		case T_DNA:
			if (PCB_USER_FPU(td->td_pcb))
				panic("Unregistered use of FPU in kernel");
			fpudna();
			goto out;

		case T_ARITHTRAP:	/* arithmetic trap */
		case T_XMMFLT:		/* SIMD floating-point exception */
		case T_FPOPFLT:		/* FPU operand fetch fault */
			/*
			 * For now, supporting kernel handler
			 * registration for FPU traps is overkill.
			 */
			trap_fatal(frame, 0);
			goto out;

		case T_STKFLT:		/* stack fault */
		case T_PROTFLT:		/* general protection fault */
		case T_SEGNPFLT:	/* segment not present fault */
			if (td->td_intr_nesting_level != 0)
				break;

			/*
			 * Invalid segment selectors and out of bounds
			 * %rip's and %rsp's can be set up in user mode.
			 * This causes a fault in kernel mode when the
			 * kernel tries to return to user mode.  We want
			 * to get this fault so that we can fix the
			 * problem here and not have to check all the
			 * selectors and pointers when the user changes
			 * them.
			 */
			if (frame->tf_rip == (long)doreti_iret) {
				frame->tf_rip = (long)doreti_iret_fault;
				goto out;
			}
			if (frame->tf_rip == (long)ld_ds) {
				frame->tf_rip = (long)ds_load_fault;
				goto out;
			}
			if (frame->tf_rip == (long)ld_es) {
				frame->tf_rip = (long)es_load_fault;
				goto out;
			}
			if (frame->tf_rip == (long)ld_fs) {
				frame->tf_rip = (long)fs_load_fault;
				goto out;
			}
			if (frame->tf_rip == (long)ld_gs) {
				frame->tf_rip = (long)gs_load_fault;
				goto out;
			}
			if (frame->tf_rip == (long)ld_gsbase) {
				frame->tf_rip = (long)gsbase_load_fault;
				goto out;
			}
			if (frame->tf_rip == (long)ld_fsbase) {
				frame->tf_rip = (long)fsbase_load_fault;
				goto out;
			}
			if (curpcb->pcb_onfault != NULL) {
				frame->tf_rip = (long)curpcb->pcb_onfault;
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
			if (frame->tf_rflags & PSL_NT) {
				frame->tf_rflags &= ~PSL_NT;
				goto out;
			}
			break;

		case T_TRCTRAP:	 /* trace trap */
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
			if (user_dbreg_trap()) {
				/*
				 * Reset breakpoint bits because the
				 * processor doesn't
				 */
				/* XXX check upper bits here */
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
#endif /* DEV_ISA */
		}

		trap_fatal(frame, 0);
		goto out;
	}

	/* Translate fault for emulators (e.g. Linux) */
	if (*p->p_sysent->sv_transtrap)
		i = (*p->p_sysent->sv_transtrap)(i, type);

	ksiginfo_init_trap(&ksi);
	ksi.ksi_signo = i;
	ksi.ksi_code = ucode;
	ksi.ksi_trapno = type;
	ksi.ksi_addr = (void *)addr;
	if (uprintf_signal) {
		uprintf("pid %d comm %s: signal %d err %lx code %d type %d "
		    "addr 0x%lx rsp 0x%lx rip 0x%lx "
		    "<%02x %02x %02x %02x %02x %02x %02x %02x>\n",
		    p->p_pid, p->p_comm, i, frame->tf_err, ucode, type, addr,
		    frame->tf_rsp, frame->tf_rip,
		    fubyte((void *)(frame->tf_rip + 0)),
		    fubyte((void *)(frame->tf_rip + 1)),
		    fubyte((void *)(frame->tf_rip + 2)),
		    fubyte((void *)(frame->tf_rip + 3)),
		    fubyte((void *)(frame->tf_rip + 4)),
		    fubyte((void *)(frame->tf_rip + 5)),
		    fubyte((void *)(frame->tf_rip + 6)),
		    fubyte((void *)(frame->tf_rip + 7)));
	}
	KASSERT((read_rflags() & PSL_I) != 0, ("interrupts disabled"));
	trapsignal(td, &ksi);

user:
	userret(td, frame);
	KASSERT(PCB_USER_FPU(td->td_pcb),
	    ("Return from trap with kernel FPU ctx leaked"));
userout:
out:
	return;
}

/*
 * Ensure that we ignore any DTrace-induced faults. This function cannot
 * be instrumented, so it cannot generate such faults itself.
 */
void
trap_check(struct trapframe *frame)
{

#ifdef KDTRACE_HOOKS
	if (dtrace_trap_func != NULL &&
	    (*dtrace_trap_func)(frame, frame->tf_trapno) != 0)
		return;
#endif
	trap(frame);
}

static int
trap_pfault(frame, usermode)
	struct trapframe *frame;
	int usermode;
{
	vm_offset_t va;
	vm_map_t map;
	int rv = 0;
	vm_prot_t ftype;
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	vm_offset_t eva = frame->tf_addr;

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
	if (va >= VM_MIN_KERNEL_ADDRESS) {
		/*
		 * Don't allow user-mode faults in kernel address space.
		 */
		if (usermode)
			goto nogo;

		map = kernel_map;
	} else {
		map = &p->p_vmspace->vm_map;

		/*
		 * When accessing a usermode address, kernel must be
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
	 * If the trap was caused by errant bits in the PTE then panic.
	 */
	if (frame->tf_err & PGEX_RSV) {
		trap_fatal(frame, eva);
		return (-1);
	}

	/*
	 * PGEX_I is defined only if the execute disable bit capability is
	 * supported and enabled.
	 */
	if (frame->tf_err & PGEX_W)
		ftype = VM_PROT_WRITE;
	else if ((frame->tf_err & PGEX_I) && pg_nx != 0)
		ftype = VM_PROT_EXECUTE;
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
			frame->tf_rip = (long)curpcb->pcb_onfault;
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
	int code, ss;
	u_int type;
	long esp;
	struct soft_segment_descriptor softseg;
	char *msg;

	code = frame->tf_err;
	type = frame->tf_trapno;
	sdtossd(&gdt[NGDT * PCPU_GET(cpuid) + IDXSEL(frame->tf_cs & 0xffff)],
	    &softseg);

	if (type <= MAX_TRAP_MSG)
		msg = trap_msg[type];
	else
		msg = "UNKNOWN";
	printf("\n\nFatal trap %d: %s while in %s mode\n", type, msg,
	    TRAPF_USERMODE(frame) ? "user" : "kernel");
#ifdef SMP
	/* two separate prints in case of a trap on an unmapped page */
	printf("cpuid = %d; ", PCPU_GET(cpuid));
	printf("apic id = %02x\n", PCPU_GET(apic_id));
#endif
	if (type == T_PAGEFLT) {
		printf("fault virtual address	= 0x%lx\n", eva);
		printf("fault code		= %s %s %s%s, %s\n",
			code & PGEX_U ? "user" : "supervisor",
			code & PGEX_W ? "write" : "read",
			code & PGEX_I ? "instruction" : "data",
			code & PGEX_RSV ? " rsv" : "",
			code & PGEX_P ? "protection violation" : "page not present");
	}
	printf("instruction pointer	= 0x%lx:0x%lx\n",
	       frame->tf_cs & 0xffff, frame->tf_rip);
	if (TF_HAS_STACKREGS(frame)) {
		ss = frame->tf_ss & 0xffff;
		esp = frame->tf_rsp;
	} else {
		ss = GSEL(GDATA_SEL, SEL_KPL);
		esp = (long)&frame->tf_rsp;
	}
	printf("stack pointer	        = 0x%x:0x%lx\n", ss, esp);
	printf("frame pointer	        = 0x%x:0x%lx\n", ss, frame->tf_rbp);
	printf("code segment		= base 0x%lx, limit 0x%lx, type 0x%x\n",
	       softseg.ssd_base, softseg.ssd_limit, softseg.ssd_type);
	printf("			= DPL %d, pres %d, long %d, def32 %d, gran %d\n",
	       softseg.ssd_dpl, softseg.ssd_p, softseg.ssd_long, softseg.ssd_def32,
	       softseg.ssd_gran);
	printf("processor eflags	= ");
	if (frame->tf_rflags & PSL_T)
		printf("trace trap, ");
	if (frame->tf_rflags & PSL_I)
		printf("interrupt enabled, ");
	if (frame->tf_rflags & PSL_NT)
		printf("nested task, ");
	if (frame->tf_rflags & PSL_RF)
		printf("resume, ");
	printf("IOPL = %ld\n", (frame->tf_rflags & PSL_IOPL) >> 12);
	printf("current process		= %d (%s)\n",
	    curproc->p_pid, curthread->td_name);

#ifdef KDB
	if (debugger_on_panic || kdb_active)
		if (kdb_trap(type, 0, frame))
			return;
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
 */
void
dblfault_handler(struct trapframe *frame)
{
#ifdef KDTRACE_HOOKS
	if (dtrace_doubletrap_func != NULL)
		(*dtrace_doubletrap_func)();
#endif
	printf("\nFatal double fault\n");
	printf("rip = 0x%lx\n", frame->tf_rip);
	printf("rsp = 0x%lx\n", frame->tf_rsp);
	printf("rbp = 0x%lx\n", frame->tf_rbp);
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
	register_t *argp;
	caddr_t params;
	int reg, regcnt, error;

	p = td->td_proc;
	frame = td->td_frame;
	reg = 0;
	regcnt = 6;

	params = (caddr_t)frame->tf_rsp + sizeof(register_t);
	sa->code = frame->tf_rax;

	if (sa->code == SYS_syscall || sa->code == SYS___syscall) {
		sa->code = frame->tf_rdi;
		reg++;
		regcnt--;
	}
 	if (p->p_sysent->sv_mask)
 		sa->code &= p->p_sysent->sv_mask;

 	if (sa->code >= p->p_sysent->sv_size)
 		sa->callp = &p->p_sysent->sv_table[0];
  	else
 		sa->callp = &p->p_sysent->sv_table[sa->code];

	sa->narg = sa->callp->sy_narg;
	KASSERT(sa->narg <= sizeof(sa->args) / sizeof(sa->args[0]),
	    ("Too many syscall arguments!"));
	error = 0;
	argp = &frame->tf_rdi;
	argp += reg;
	bcopy(argp, sa->args, sizeof(sa->args[0]) * regcnt);
	if (sa->narg > regcnt) {
		KASSERT(params != NULL, ("copyin args with no params!"));
		error = copyin(params, &sa->args[regcnt],
	    	    (sa->narg - regcnt) * sizeof(sa->args[0]));
	}

	if (error == 0) {
		td->td_retval[0] = 0;
		td->td_retval[1] = frame->tf_rdx;
	}

	return (error);
}

#include "../../kern/subr_syscall.c"

/*
 * System call handler for native binaries.  The trap frame is already
 * set up by the assembler trampoline and a pointer to it is saved in
 * td_frame.
 */
void
amd64_syscall(struct thread *td, int traced)
{
	struct syscall_args sa;
	int error;
	ksiginfo_t ksi;

#ifdef DIAGNOSTIC
	if (!TRAPF_USERMODE(td->td_frame)) {
		panic("syscall");
		/* NOT REACHED */
	}
#endif
	error = syscallenter(td, &sa);

	/*
	 * Traced syscall.
	 */
	if (__predict_false(traced)) {
		td->td_frame->tf_rflags &= ~PSL_T;
		ksiginfo_init_trap(&ksi);
		ksi.ksi_signo = SIGTRAP;
		ksi.ksi_code = TRAP_TRACE;
		ksi.ksi_addr = (void *)td->td_frame->tf_rip;
		trapsignal(td, &ksi);
	}

	KASSERT(PCB_USER_FPU(td->td_pcb),
	    ("System call %s returing with kernel FPU ctx leaked",
	     syscallname(td->td_proc, sa.code)));
	KASSERT(td->td_pcb->pcb_save == get_pcb_user_save_td(td),
	    ("System call %s returning with mangled pcb_save",
	     syscallname(td->td_proc, sa.code)));
	KASSERT(td->td_md.md_invl_gen.gen == 0,
	    ("System call %s returning with leaked invl_gen %lu",
	    syscallname(td->td_proc, sa.code), td->td_md.md_invl_gen.gen));


	syscallret(td, error, &sa);

	/*
	 * If the user-supplied value of %rip is not a canonical
	 * address, then some CPUs will trigger a ring 0 #GP during
	 * the sysret instruction.  However, the fault handler would
	 * execute in ring 0 with the user's %gs and %rsp which would
	 * not be safe.  Instead, use the full return path which
	 * catches the problem safely.
	 */
	if (td->td_frame->tf_rip >= VM_MAXUSER_ADDRESS)
		set_pcb_flags(td->td_pcb, PCB_FULL_IRET);
}
