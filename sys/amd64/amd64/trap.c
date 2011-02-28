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
#include "opt_kdtrace.h"
#include "opt_ktrace.h"

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

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>

#include <machine/cpu.h>
#include <machine/intr_machdep.h>
#include <machine/mca.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#ifdef SMP
#include <machine/smp.h>
#endif
#include <machine/tss.h>

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>

/*
 * This is a hook which is initialised by the dtrace module
 * to handle traps which might occur during DTrace probe
 * execution.
 */
dtrace_trap_func_t	dtrace_trap_func;

dtrace_doubletrap_func_t	dtrace_doubletrap_func;

/*
 * This is a hook which is initialised by the systrace module
 * when it is loaded. This keeps the DTrace syscall provider
 * implementation opaque. 
 */
systrace_probe_func_t	systrace_probe_func;

/*
 * These hooks are necessary for the pid, usdt and fasttrap providers.
 */
dtrace_fasttrap_probe_ptr_t	dtrace_fasttrap_probe_ptr;
dtrace_pid_probe_ptr_t		dtrace_pid_probe_ptr;
dtrace_return_probe_ptr_t	dtrace_return_probe_ptr;
#endif

extern void trap(struct trapframe *frame);
extern void syscall(struct trapframe *frame);
void dblfault_handler(struct trapframe *frame);

static int trap_pfault(struct trapframe *, int);
static void trap_fatal(struct trapframe *, vm_offset_t);

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

#ifdef KDB
static int kdb_on_nmi = 1;
SYSCTL_INT(_machdep, OID_AUTO, kdb_on_nmi, CTLFLAG_RW,
	&kdb_on_nmi, 0, "Go to KDB on NMI");
#endif
static int panic_on_nmi = 1;
SYSCTL_INT(_machdep, OID_AUTO, panic_on_nmi, CTLFLAG_RW,
	&panic_on_nmi, 0, "Panic on NMI");
static int prot_fault_translation = 0;
SYSCTL_INT(_machdep, OID_AUTO, prot_fault_translation, CTLFLAG_RW,
	&prot_fault_translation, 0, "Select signal to deliver on protection fault");

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

#ifdef	HWPMC_HOOKS
	/*
	 * CPU PMCs interrupt using an NMI.  If the PMC module is
	 * active, pass the 'rip' value to the PMC module's interrupt
	 * handler.  A return value of '1' from the handler means that
	 * the NMI was handled by it and we can return immediately.
	 */
	if (type == T_NMI && pmc_intr &&
	    (*pmc_intr)(PCPU_GET(cpuid), frame))
		goto out;
#endif

	if (type == T_MCHK) {
		if (!mca_intr())
			trap_fatal(frame, 0);
		goto out;
	}

#ifdef KDTRACE_HOOKS
	/*
	 * A trap can occur while DTrace executes a probe. Before
	 * executing the probe, DTrace blocks re-scheduling and sets
	 * a flag in it's per-cpu flags to indicate that it doesn't
	 * want to fault. On returning from the probe, the no-fault
	 * flag is cleared and finally re-scheduling is enabled.
	 *
	 * If the DTrace kernel module has registered a trap handler,
	 * call it and if it returns non-zero, assume that it has
	 * handled the trap and modified the trap frame so that this
	 * function can return normally.
	 */
	if (dtrace_trap_func != NULL)
		if ((*dtrace_trap_func)(frame, type))
			goto out;
	if (type == T_DTRACE_PROBE || type == T_DTRACE_RET ||
	    type == T_BPTFLT) {
		struct reg regs;

		regs.r_r15 = frame->tf_r15;
		regs.r_r14 = frame->tf_r14;
		regs.r_r13 = frame->tf_r13;
		regs.r_r12 = frame->tf_r12;
		regs.r_r11 = frame->tf_r11;
		regs.r_r10 = frame->tf_r10;
		regs.r_r9  = frame->tf_r9;
		regs.r_r8  = frame->tf_r8;
		regs.r_rdi = frame->tf_rdi;
		regs.r_rsi = frame->tf_rsi;
		regs.r_rbp = frame->tf_rbp;
		regs.r_rbx = frame->tf_rbx;
		regs.r_rdx = frame->tf_rdx;
		regs.r_rcx = frame->tf_rcx;
		regs.r_rax = frame->tf_rax;
		regs.r_rip = frame->tf_rip;
		regs.r_cs = frame->tf_cs;
		regs.r_rflags = frame->tf_rflags;
		regs.r_rsp = frame->tf_rsp;
		regs.r_ss = frame->tf_ss;
		if (frame->tf_flags & TF_HASSEGS) {
			regs.r_ds = frame->tf_ds;
			regs.r_es = frame->tf_es;
			regs.r_fs = frame->tf_fs;
			regs.r_gs = frame->tf_gs;
		} else {
			regs.r_ds = 0;
			regs.r_es = 0;
			regs.r_fs = 0;
			regs.r_gs = 0;
		}
		if (type == T_DTRACE_PROBE &&
		    dtrace_fasttrap_probe_ptr != NULL &&
		    dtrace_fasttrap_probe_ptr(&regs) == 0)
				goto out;
		if (type == T_BPTFLT &&
		    dtrace_pid_probe_ptr != NULL &&
		    dtrace_pid_probe_ptr(&regs) == 0)
				goto out;
		if (type == T_DTRACE_RET &&
		    dtrace_return_probe_ptr != NULL &&
		    dtrace_return_probe_ptr(&regs) == 0)
			goto out;

	}
#endif

	if ((frame->tf_rflags & PSL_I) == 0) {
		/*
		 * Buggy application or kernel code has disabled
		 * interrupts and then trapped.  Enabling interrupts
		 * now is wrong, but it is better than running with
		 * interrupts disabled until they are accidentally
		 * enabled later.
		 */
		if (ISPL(frame->tf_cs) == SEL_UPL)
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
	if (type == T_PAGEFLT) {
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
		    "Kernel page fault") != 0)
			trap_fatal(frame, frame->tf_addr);
	}

        if (ISPL(frame->tf_cs) == SEL_UPL) {
		/* user trap */

		td->td_pticks = 0;
		td->td_frame = frame;
		addr = frame->tf_rip;
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
			frame->tf_rflags &= ~PSL_T;
			i = SIGTRAP;
			ucode = (type == T_TRCTRAP ? TRAP_TRACE : TRAP_BRKPT);
			break;

		case T_ARITHTRAP:	/* arithmetic trap */
			ucode = fputrap();
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
		case T_DOUBLEFLT:	/* double fault */
		default:
			i = SIGBUS;
			ucode = BUS_OBJERR;
			break;

		case T_PAGEFLT:		/* page fault */
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
			(void) trap_pfault(frame, FALSE);
			goto out;

		case T_DNA:
			KASSERT(!PCB_USER_FPU(td->td_pcb),
			    ("Unregistered use of FPU in kernel"));
			fpudna();
			goto out;

		case T_ARITHTRAP:	/* arithmetic trap */
		case T_XMMFLT:		/* SIMD floating-point exception */
		case T_FPOPFLT:		/* FPU operand fetch fault */
			/*
			 * XXXKIB for now disable any FPU traps in kernel
			 * handler registration seems to be overkill
			 */
			trap_fatal(frame, 0);
			goto out;

		case T_STKFLT:		/* stack fault */
			break;

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
			if (PCPU_GET(curpcb)->pcb_onfault != NULL) {
				frame->tf_rip =
				    (long)PCPU_GET(curpcb)->pcb_onfault;
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
	trapsignal(td, &ksi);

user:
	userret(td, frame);
	mtx_assert(&Giant, MA_NOTOWNED);
	KASSERT(PCB_USER_FPU(td->td_pcb),
	    ("Return from trap with kernel FPU ctx leaked"));
userout:
out:
	return;
}

static int
trap_pfault(frame, usermode)
	struct trapframe *frame;
	int usermode;
{
	vm_offset_t va;
	struct vmspace *vm = NULL;
	vm_map_t map;
	int rv = 0;
	vm_prot_t ftype;
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	vm_offset_t eva = frame->tf_addr;

	va = trunc_page(eva);
	if (va >= VM_MIN_KERNEL_ADDRESS) {
		/*
		 * Don't allow user-mode faults in kernel address space.
		 */
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
	else if ((frame->tf_err & PGEX_I) && pg_nx != 0)
		ftype = VM_PROT_EXECUTE;
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
			frame->tf_rip = (long)PCPU_GET(curpcb)->pcb_onfault;
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
	    ISPL(frame->tf_cs) == SEL_UPL ? "user" : "kernel");
#ifdef SMP
	/* two separate prints in case of a trap on an unmapped page */
	printf("cpuid = %d; ", PCPU_GET(cpuid));
	printf("apic id = %02x\n", PCPU_GET(apic_id));
#endif
	if (type == T_PAGEFLT) {
		printf("fault virtual address	= 0x%lx\n", eva);
		printf("fault code		= %s %s %s, %s\n",
			code & PGEX_U ? "user" : "supervisor",
			code & PGEX_W ? "write" : "read",
			code & PGEX_I ? "instruction" : "data",
			code & PGEX_P ? "protection violation" : "page not present");
	}
	printf("instruction pointer	= 0x%lx:0x%lx\n",
	       frame->tf_cs & 0xffff, frame->tf_rip);
        if (ISPL(frame->tf_cs) == SEL_UPL) {
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
	printf("current process		= ");
	if (curproc) {
		printf("%lu (%s)\n",
		    (u_long)curproc->p_pid, curthread->td_name ?
		    curthread->td_name : "");
	} else {
		printf("Idle\n");
	}

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

/*
 *	syscall -	system call request C handler
 *
 *	A system call is essentially treated as a trap.
 */
void
syscall(struct trapframe *frame)
{
	struct thread *td;
	struct syscall_args sa;
	register_t orig_tf_rflags;
	int error;
	ksiginfo_t ksi;

#ifdef DIAGNOSTIC
	if (ISPL(frame->tf_cs) != SEL_UPL) {
		panic("syscall");
		/* NOT REACHED */
	}
#endif
	orig_tf_rflags = frame->tf_rflags;
	td = curthread;
	td->td_frame = frame;

	error = syscallenter(td, &sa);

	/*
	 * Traced syscall.
	 */
	if (orig_tf_rflags & PSL_T) {
		frame->tf_rflags &= ~PSL_T;
		ksiginfo_init_trap(&ksi);
		ksi.ksi_signo = SIGTRAP;
		ksi.ksi_code = TRAP_TRACE;
		ksi.ksi_addr = (void *)frame->tf_rip;
		trapsignal(td, &ksi);
	}

	KASSERT(PCB_USER_FPU(td->td_pcb),
	    ("System call %s returing with kernel FPU ctx leaked",
	     syscallname(td->td_proc, sa.code)));
	KASSERT(td->td_pcb->pcb_save == &td->td_pcb->pcb_user_save,
	    ("System call %s returning with mangled pcb_save",
	     syscallname(td->td_proc, sa.code)));

	syscallret(td, error, &sa);
}
