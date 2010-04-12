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

extern char *syscallnames[];

/* #define DEBUG 1 */
#ifdef DEBUG
static void
report_seg_fault(const char *segn, struct trapframe *frame)
{
	struct proc_ldt *pldt;
	struct trapframe *pf;

	pldt = curproc->p_md.md_ldt;
	printf("%d: %s load fault %lx %p %d\n",
	    curproc->p_pid, segn, frame->tf_err,
	    pldt != NULL ? pldt->ldt_base : NULL,
	    pldt != NULL ? pldt->ldt_refcnt : 0);
	kdb_backtrace();
	pf = (struct trapframe *)frame->tf_rsp;
	printf("rdi %lx\n", pf->tf_rdi);
	printf("rsi %lx\n", pf->tf_rsi);
	printf("rdx %lx\n", pf->tf_rdx);
	printf("rcx %lx\n", pf->tf_rcx);
	printf("r8  %lx\n", pf->tf_r8);
	printf("r9  %lx\n", pf->tf_r9);
	printf("rax %lx\n", pf->tf_rax);
	printf("rbx %lx\n", pf->tf_rbx);
	printf("rbp %lx\n", pf->tf_rbp);
	printf("r10 %lx\n", pf->tf_r10);
	printf("r11 %lx\n", pf->tf_r11);
	printf("r12 %lx\n", pf->tf_r12);
	printf("r13 %lx\n", pf->tf_r13);
	printf("r14 %lx\n", pf->tf_r14);
	printf("r15 %lx\n", pf->tf_r15);
	printf("fs  %x\n", pf->tf_fs);
	printf("gs  %x\n", pf->tf_gs);
	printf("es  %x\n", pf->tf_es);
	printf("ds  %x\n", pf->tf_ds);
	printf("tno %x\n", pf->tf_trapno);
	printf("adr %lx\n", pf->tf_addr);
	printf("flg %x\n", pf->tf_flags);
	printf("err %lx\n", pf->tf_err);
	printf("rip %lx\n", pf->tf_rip);
	printf("cs  %lx\n", pf->tf_cs);
	printf("rfl %lx\n", pf->tf_rflags);
	printf("rsp %lx\n", pf->tf_rsp);
	printf("ss  %lx\n", pf->tf_ss);
}
#endif

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
	 * want to fault. On returning from the the probe, the no-fault
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
			printf(
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
#ifdef DEBUG
			report_seg_fault("hlt", frame);
#endif
			/*
			 * We shouldn't enable interrupts while holding a
			 * spin lock or servicing an NMI.
			 */
			if (type != T_NMI && td->td_md.md_spinlock_count == 0)
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
					if (SV_CURPROC_ABI() ==
					    SV_ABI_FREEBSD &&
					    p->p_osrel >= 700004) {
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
			/*
			 * The kernel is apparently using fpu for copying.
			 * XXX this should be fatal unless the kernel has
			 * registered such use.
			 */
			printf("fpudna in kernel mode!\n");
#ifdef KDB
			kdb_backtrace();
#endif
			fpudna();
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
#ifdef DEBUG
				report_seg_fault("ds", frame);
#endif
				frame->tf_rip = (long)ds_load_fault;
				frame->tf_ds = _udatasel;
				goto out;
			}
			if (frame->tf_rip == (long)ld_es) {
#ifdef DEBUG
				report_seg_fault("es", frame);
#endif
				frame->tf_rip = (long)es_load_fault;
				frame->tf_es = _udatasel;
				goto out;
			}
			if (frame->tf_rip == (long)ld_fs) {
#ifdef DEBUG
				report_seg_fault("fs", frame);
#endif
				frame->tf_rip = (long)fs_load_fault;
				frame->tf_fs = _ufssel;
				goto out;
			}
			if (frame->tf_rip == (long)ld_gs) {
#ifdef DEBUG
				report_seg_fault("gs", frame);
#endif
				frame->tf_rip = (long)gs_load_fault;
				frame->tf_gs = _ugssel;
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

#ifdef DEBUG
{
	register_t rg,rgk, rf;

	if (type <= MAX_TRAP_MSG) {
		uprintf("fatal process exception: %s",
			trap_msg[type]);
		if ((type == T_PAGEFLT) || (type == T_PROTFLT))
			uprintf(", fault VA = 0x%lx", frame->tf_addr);
		uprintf("\n");
	}
	rf = rdmsr(0xc0000100);
	rg = rdmsr(0xc0000101);
	rgk = rdmsr(0xc0000102);
	uprintf("pid %d TRAP %d rip %lx err %lx addr %lx cs %lx ss %lx ds %x "
		"es %x fs %x fsbase %lx %lx gs %x gsbase %lx %lx %lx\n",
		curproc->p_pid, type, frame->tf_rip, frame->tf_err,
		frame->tf_addr,
		frame->tf_cs, frame->tf_ss, frame->tf_ds, frame->tf_es,
		frame->tf_fs, td->td_pcb->pcb_fsbase, rf,
		frame->tf_gs, td->td_pcb->pcb_gsbase, rg, rgk);
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
		rv = vm_fault(map, va, ftype, VM_FAULT_NORMAL);

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

struct syscall_args {
	u_int code;
	struct sysent *callp;
	register_t args[8];
	register_t *argp;
	int narg;
};

static int
fetch_syscall_args(struct thread *td, struct syscall_args *sa)
{
	struct proc *p;
	struct trapframe *frame;
	caddr_t params;
	int reg, regcnt, error;

	p = td->td_proc;
	frame = td->td_frame;
	reg = 0;
	regcnt = 6;

	params = (caddr_t)frame->tf_rsp + sizeof(register_t);
	sa->code = frame->tf_rax;

	if (p->p_sysent->sv_prepsyscall) {
		(*p->p_sysent->sv_prepsyscall)(frame, (int *)sa->args,
		    &sa->code, &params);
	} else {
		if (sa->code == SYS_syscall || sa->code == SYS___syscall) {
			sa->code = frame->tf_rdi;
			reg++;
			regcnt--;
		}
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
	sa->argp = &frame->tf_rdi;
	sa->argp += reg;
	bcopy(sa->argp, sa->args, sizeof(sa->args[0]) * regcnt);
	if (sa->narg > regcnt) {
		KASSERT(params != NULL, ("copyin args with no params!"));
		error = copyin(params, &sa->args[regcnt],
	    	    (sa->narg - regcnt) * sizeof(sa->args[0]));
	}
	sa->argp = &sa->args[0];

	/*
	 * This may result in two records if debugger modified
	 * registers or memory during sleep at stop/ptrace point.
	 */
#ifdef KTRACE
	if (KTRPOINT(td, KTR_SYSCALL))
		ktrsyscall(sa->code, sa->narg, sa->argp);
#endif
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
	struct proc *p;
	struct syscall_args sa;
	register_t orig_tf_rflags;
	int error;
	ksiginfo_t ksi;

	PCPU_INC(cnt.v_syscall);
	td = curthread;
	p = td->td_proc;
	td->td_syscalls++;

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
	orig_tf_rflags = frame->tf_rflags;
	if (p->p_flag & P_TRACED) {
		PROC_LOCK(p);
		td->td_dbgflags &= ~TDB_USERWR;
		PROC_UNLOCK(p);
	}
	error = fetch_syscall_args(td, &sa);

	CTR4(KTR_SYSC, "syscall enter thread %p pid %d proc %s code %d", td,
	    td->td_proc->p_pid, td->td_name, sa.code);

	if (error == 0) {
		td->td_retval[0] = 0;
		td->td_retval[1] = frame->tf_rdx;

		STOPEVENT(p, S_SCE, sa.narg);
		PTRACESTOP_SC(p, td, S_PT_SCE);
		if (td->td_dbgflags & TDB_USERWR) {
			/*
			 * Reread syscall number and arguments if
			 * debugger modified registers or memory.
			 */
			error = fetch_syscall_args(td, &sa);
			if (error != 0)
				goto retval;
			td->td_retval[1] = frame->tf_rdx;
		}

#ifdef KDTRACE_HOOKS
		/*
		 * If the systrace module has registered it's probe
		 * callback and if there is a probe active for the
		 * syscall 'entry', process the probe.
		 */
		if (systrace_probe_func != NULL && sa.callp->sy_entry != 0)
			(*systrace_probe_func)(sa.callp->sy_entry, sa.code,
			    sa.callp, sa.args);
#endif

		AUDIT_SYSCALL_ENTER(sa.code, td);
		error = (*sa.callp->sy_call)(td, sa.argp);
		AUDIT_SYSCALL_EXIT(error, td);

		/* Save the latest error return value. */
		td->td_errno = error;

#ifdef KDTRACE_HOOKS
		/*
		 * If the systrace module has registered it's probe
		 * callback and if there is a probe active for the
		 * syscall 'return', process the probe.
		 */
		if (systrace_probe_func != NULL && sa.callp->sy_return != 0)
			(*systrace_probe_func)(sa.callp->sy_return, sa.code,
			    sa.callp, sa.args);
#endif
	}
 retval:
	cpu_set_syscall_retval(td, error);

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

	/*
	 * Check for misbehavior.
	 */
	WITNESS_WARN(WARN_PANIC, NULL, "System call %s returning",
	    (sa.code >= 0 && sa.code < SYS_MAXSYSCALL) ?
	     syscallnames[sa.code] : "???");
	KASSERT(td->td_critnest == 0,
	    ("System call %s returning in a critical section",
	    (sa.code >= 0 && sa.code < SYS_MAXSYSCALL) ?
	     syscallnames[sa.code] : "???"));
	KASSERT(td->td_locks == 0,
	    ("System call %s returning with %d locks held",
	    (sa.code >= 0 && sa.code < SYS_MAXSYSCALL) ?
	     syscallnames[sa.code] : "???", td->td_locks));

	/*
	 * Handle reschedule and other end-of-syscall issues
	 */
	userret(td, frame);

	CTR4(KTR_SYSC, "syscall exit thread %p pid %d proc %s code %d", td,
	    td->td_proc->p_pid, td->td_name, sa.code);

#ifdef KTRACE
	if (KTRPOINT(td, KTR_SYSRET))
		ktrsysret(sa.code, error, td->td_retval[0]);
#endif

	/*
	 * This works because errno is findable through the
	 * register set.  If we ever support an emulation where this
	 * is not the case, this code will need to be revisited.
	 */
	STOPEVENT(p, S_SCX, sa.code);

	PTRACESTOP_SC(p, td, S_PT_SCX);
}
