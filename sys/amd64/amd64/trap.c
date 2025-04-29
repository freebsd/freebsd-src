/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
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
 */

#include <sys/cdefs.h>
/*
 * AMD64 Trap and System call handling
 */

#include "opt_clock.h"
#include "opt_cpu.h"
#include "opt_hwpmc_hooks.h"
#include "opt_isa.h"
#include "opt_kdb.h"

#include <sys/param.h>
#include <sys/asan.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/msan.h>
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
#include <machine/trap.h>
#include <machine/tss.h>

#ifdef KDTRACE_HOOKS
#include <sys/dtrace_bsd.h>
#endif

extern inthand_t IDTVEC(bpt), IDTVEC(bpt_pti), IDTVEC(dbg),
    IDTVEC(fast_syscall), IDTVEC(fast_syscall_pti), IDTVEC(fast_syscall32),
    IDTVEC(int0x80_syscall_pti), IDTVEC(int0x80_syscall);

void __noinline trap(struct trapframe *frame);
void trap_check(struct trapframe *frame);
void dblfault_handler(struct trapframe *frame);

static int trap_pfault(struct trapframe *, bool, int *, int *);
static void trap_diag(struct trapframe *, vm_offset_t);
static void trap_fatal(struct trapframe *, vm_offset_t);
#ifdef KDTRACE_HOOKS
static bool trap_user_dtrace(struct trapframe *,
    int (**hook)(struct trapframe *));
#endif

static const char UNKNOWN[] = "unknown";
static const char *const trap_msg[] = {
	[0] =			UNKNOWN,			/* unused */
	[T_PRIVINFLT] =		"privileged instruction fault",
	[2] =			UNKNOWN,			/* unused */
	[T_BPTFLT] =		"breakpoint instruction fault",
	[4] =			UNKNOWN,			/* unused */
	[5] =			UNKNOWN,			/* unused */
	[T_ARITHTRAP] =		"arithmetic trap",
	[7] =			UNKNOWN,			/* unused */
	[8] =			UNKNOWN,			/* unused */
	[T_PROTFLT] =		"general protection fault",
	[T_TRCTRAP] =		"debug exception",
	[11] =			UNKNOWN,			/* unused */
	[T_PAGEFLT] =		"page fault",
	[13] =			UNKNOWN,			/* unused */
	[T_ALIGNFLT] =		"alignment fault",
	[15] =			UNKNOWN,			/* unused */
	[16] =			UNKNOWN,			/* unused */
	[17] =			UNKNOWN,			/* unused */
	[T_DIVIDE] =		"integer divide fault",
	[T_NMI] =		"non-maskable interrupt trap",
	[T_OFLOW] =		"overflow trap",
	[T_BOUND] =		"FPU bounds check fault",
	[T_DNA] =		"FPU device not available",
	[T_DOUBLEFLT] =		"double fault",
	[T_FPOPFLT] =		"FPU operand fetch fault",
	[T_TSSFLT] =		"invalid TSS fault",
	[T_SEGNPFLT] =		"segment not present fault",
	[T_STKFLT] =		"stack fault",
	[T_MCHK] =		"machine check trap",
	[T_XMMFLT] =		"SIMD floating-point exception",
	[T_RESERVED] =		"reserved (unknown) fault",
	[31] =			UNKNOWN,			/* reserved */
	[T_DTRACE_RET] =	"DTrace pid return trap",
};

static const char *
traptype_to_msg(u_int type)
{
	return (type < nitems(trap_msg) ? trap_msg[type] :
	    "unknown/reserved trap");
}

static int uprintf_signal;
SYSCTL_INT(_machdep, OID_AUTO, uprintf_signal, CTLFLAG_RWTUN,
    &uprintf_signal, 0,
    "Print debugging information on trap signal to ctty");

u_long cnt_efirt_faults;
int print_efirt_faults = 1;

/*
 * Control L1D flush on return from NMI.
 *
 * Tunable  can be set to the following values:
 * 0 - only enable flush on return from NMI if required by vmm.ko (default)
 * >1 - always flush on return from NMI.
 *
 * Post-boot, the sysctl indicates if flushing is currently enabled.
 */
int nmi_flush_l1d_sw;
SYSCTL_INT(_machdep, OID_AUTO, nmi_flush_l1d_sw, CTLFLAG_RWTUN,
    &nmi_flush_l1d_sw, 0,
    "Flush L1 Data Cache on NMI exit, software bhyve L1TF mitigation assist");

/*
 * Table of handlers for various segment load faults.
 */
static const struct {
	uintptr_t	faddr;
	uintptr_t	fhandler;
} sfhandlers[] = {
	{
		.faddr = (uintptr_t)ld_ds,
		.fhandler = (uintptr_t)ds_load_fault,
	},
	{
		.faddr = (uintptr_t)ld_es,
		.fhandler = (uintptr_t)es_load_fault,
	},
	{
		.faddr = (uintptr_t)ld_fs,
		.fhandler = (uintptr_t)fs_load_fault,
	},
	{
		.faddr = (uintptr_t)ld_gs,
		.fhandler = (uintptr_t)gs_load_fault,
	},
	{
		.faddr = (uintptr_t)ld_gsbase,
		.fhandler = (uintptr_t)gsbase_load_fault
	},
	{
		.faddr = (uintptr_t)ld_fsbase,
		.fhandler = (uintptr_t)fsbase_load_fault,
	},
};

/*
 * Exception, fault, and trap interface to the FreeBSD kernel.
 * This common code is called from assembly language IDT gate entry
 * routines that prepare a suitable stack frame, and restore this
 * frame after the exception has been processed.
 */

void
trap(struct trapframe *frame)
{
	ksiginfo_t ksi;
	struct thread *td;
	struct proc *p;
	register_t addr, dr6;
	size_t i;
	int pf, signo, ucode;
	u_int type;

	td = curthread;
	p = td->td_proc;
	dr6 = 0;

	kasan_mark(frame, sizeof(*frame), sizeof(*frame), 0);
	kmsan_mark(frame, sizeof(*frame), KMSAN_STATE_INITED);

	VM_CNT_INC(v_trap);
	type = frame->tf_trapno;

#ifdef KDB
	if (kdb_active) {
		kdb_reenter();
		return;
	}
#endif
	if (type == T_NMI) {
		nmi_handle_intr(frame);
		return;
	}

	if (type == T_RESERVED) {
		trap_fatal(frame, 0);
		return;
	}

	if ((frame->tf_rflags & PSL_I) == 0) {
		/*
		 * Buggy application or kernel code has disabled
		 * interrupts and then trapped.  Enabling interrupts
		 * now is wrong, but it is better than running with
		 * interrupts disabled until they are accidentally
		 * enabled later.
		 */
		if (TRAPF_USERMODE(frame)) {
			uprintf(
			    "pid %ld (%s): trap %d (%s) "
			    "with interrupts disabled\n",
			    (long)curproc->p_pid, curthread->td_name, type,
			    trap_msg[type]);
		} else {
			switch (type) {
			case T_NMI:
			case T_BPTFLT:
			case T_TRCTRAP:
			case T_PROTFLT:
			case T_SEGNPFLT:
			case T_STKFLT:
				break;
			default:
				printf(
				    "kernel trap %d with interrupts disabled\n",
				    type);

				/*
				 * We shouldn't enable interrupts while holding a
				 * spin lock.
				 */
				if (td->td_md.md_spinlock_count == 0)
					enable_intr();
			}
		}
	}

	if (TRAPF_USERMODE(frame)) {
		/* user trap */

		td->td_pticks = 0;
		td->td_frame = frame;
		addr = frame->tf_rip;
		if (td->td_cowgen != atomic_load_int(&p->p_cowgen))
			thread_cow_update(td);

		switch (type) {
		case T_PRIVINFLT:	/* privileged instruction fault */
			signo = SIGILL;
			ucode = ILL_PRVOPC;
			break;

		case T_BPTFLT:		/* bpt instruction fault */
#ifdef KDTRACE_HOOKS
			if (trap_user_dtrace(frame, &dtrace_pid_probe_ptr))
				return;
#else
			enable_intr();
#endif
			signo = SIGTRAP;
			ucode = TRAP_BRKPT;
			break;

		case T_TRCTRAP:		/* debug exception */
			enable_intr();
			signo = SIGTRAP;
			ucode = TRAP_TRACE;
			dr6 = rdr6();
			if ((dr6 & DBREG_DR6_BS) != 0) {
				PROC_LOCK(td->td_proc);
				if ((td->td_dbgflags & TDB_STEP) != 0) {
					td->td_frame->tf_rflags &= ~PSL_T;
					td->td_dbgflags &= ~TDB_STEP;
				}
				PROC_UNLOCK(td->td_proc);
			}
			break;

		case T_ARITHTRAP:	/* arithmetic trap */
			ucode = fputrap_x87();
			if (ucode == -1)
				return;
			signo = SIGFPE;
			break;

		case T_PROTFLT:		/* general protection fault */
			signo = SIGBUS;
			ucode = BUS_OBJERR;
			break;
		case T_STKFLT:		/* stack fault */
		case T_SEGNPFLT:	/* segment not present fault */
			signo = SIGBUS;
			ucode = BUS_ADRERR;
			break;
		case T_TSSFLT:		/* invalid TSS fault */
			signo = SIGBUS;
			ucode = BUS_OBJERR;
			break;
		case T_ALIGNFLT:
			signo = SIGBUS;
			ucode = BUS_ADRALN;
			break;
		case T_DOUBLEFLT:	/* double fault */
		default:
			signo = SIGBUS;
			ucode = BUS_OBJERR;
			break;

		case T_PAGEFLT:		/* page fault */
			/*
			 * Can emulator handle this trap?
			 */
			if (*p->p_sysent->sv_trap != NULL &&
			    (*p->p_sysent->sv_trap)(td) == 0)
				return;

			pf = trap_pfault(frame, true, &signo, &ucode);
			if (pf == -1)
				return;
			if (pf == 0)
				goto userret;
			addr = frame->tf_addr;
			break;

		case T_DIVIDE:		/* integer divide fault */
			ucode = FPE_INTDIV;
			signo = SIGFPE;
			break;

		case T_OFLOW:		/* integer overflow fault */
			ucode = FPE_INTOVF;
			signo = SIGFPE;
			break;

		case T_BOUND:		/* bounds check fault */
			ucode = FPE_FLTSUB;
			signo = SIGFPE;
			break;

		case T_DNA:
			/* transparent fault (due to context switch "late") */
			KASSERT(PCB_USER_FPU(td->td_pcb),
			    ("kernel FPU ctx has leaked"));
			fpudna();
			return;

		case T_FPOPFLT:		/* FPU operand fetch fault */
			ucode = ILL_COPROC;
			signo = SIGILL;
			break;

		case T_XMMFLT:		/* SIMD floating-point exception */
			ucode = fputrap_sse();
			if (ucode == -1)
				return;
			signo = SIGFPE;
			break;
#ifdef KDTRACE_HOOKS
		case T_DTRACE_RET:
			(void)trap_user_dtrace(frame, &dtrace_return_probe_ptr);
			return;
#endif
		}
	} else {
		/* kernel trap */

		KASSERT(cold || td->td_ucred != NULL,
		    ("kernel trap doesn't have ucred"));

		/*
		 * Most likely, EFI RT faulted.  This check prevents
		 * kdb from handling breakpoints set on the BIOS text,
		 * if such option is ever needed.
		 */
		if ((td->td_pflags & TDP_EFIRT) != 0 &&
		    curpcb->pcb_onfault != NULL && type != T_PAGEFLT) {
			u_long cnt = atomic_fetchadd_long(&cnt_efirt_faults, 1);

			if ((print_efirt_faults == 1 && cnt == 0) ||
			    print_efirt_faults == 2) {
				trap_diag(frame, 0);
				printf("EFI RT fault %s\n",
				    traptype_to_msg(type));
			}
			frame->tf_rip = (long)curpcb->pcb_onfault;
			return;
		}

		switch (type) {
		case T_PAGEFLT:			/* page fault */
			(void)trap_pfault(frame, false, NULL, NULL);
			return;

		case T_DNA:
			if (PCB_USER_FPU(td->td_pcb))
				panic("Unregistered use of FPU in kernel");
			fpudna();
			return;

		case T_ARITHTRAP:	/* arithmetic trap */
		case T_XMMFLT:		/* SIMD floating-point exception */
		case T_FPOPFLT:		/* FPU operand fetch fault */
			/*
			 * For now, supporting kernel handler
			 * registration for FPU traps is overkill.
			 */
			trap_fatal(frame, 0);
			return;

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
			 *
			 * In case of PTI, the IRETQ faulted while the
			 * kernel used the pti stack, and exception
			 * frame records %rsp value pointing to that
			 * stack.  If we return normally to
			 * doreti_iret_fault, the trapframe is
			 * reconstructed on pti stack, and calltrap()
			 * called on it as well.  Due to the very
			 * limited pti stack size, kernel does not
			 * survive for too long.  Switch to the normal
			 * thread stack for the trap handling.
			 *
			 * Magic '5' is the number of qwords occupied by
			 * the hardware trap frame.
			 */
			if (frame->tf_rip == (long)doreti_iret) {
				KASSERT((read_rflags() & PSL_I) == 0,
				    ("interrupts enabled"));
				frame->tf_rip = (long)doreti_iret_fault;
				if ((PCPU_GET(curpmap)->pm_ucr3 !=
				    PMAP_NO_CR3) &&
				    (frame->tf_rsp == (uintptr_t)PCPU_GET(
				    pti_rsp0) - 5 * sizeof(register_t))) {
					frame->tf_rsp = PCPU_GET(rsp0) - 5 *
					    sizeof(register_t);
				}
				return;
			}

			for (i = 0; i < nitems(sfhandlers); i++) {
				if (frame->tf_rip == sfhandlers[i].faddr) {
					KASSERT((read_rflags() & PSL_I) == 0,
					    ("interrupts enabled"));
					frame->tf_rip = sfhandlers[i].fhandler;
					return;
				}
			}

			if (curpcb->pcb_onfault != NULL) {
				frame->tf_rip = (long)curpcb->pcb_onfault;
				return;
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
				return;
			}
			break;

		case T_TRCTRAP:	 /* debug exception */
			/* Clear any pending debug events. */
			dr6 = rdr6();
			load_dr6(0);

			/*
			 * Ignore debug register exceptions due to
			 * accesses in the user's address space, which
			 * can happen under several conditions such as
			 * if a user sets a watchpoint on a buffer and
			 * then passes that buffer to a system call.
			 * We still want to get TRCTRAPS for addresses
			 * in kernel space because that is useful when
			 * debugging the kernel.
			 */
			if (user_dbreg_trap(dr6))
				return;

			/*
			 * Malicious user code can configure a debug
			 * register watchpoint to trap on data access
			 * to the top of stack and then execute 'pop
			 * %ss; int 3'.  Due to exception deferral for
			 * 'pop %ss', the CPU will not interrupt 'int
			 * 3' to raise the DB# exception for the debug
			 * register but will postpone the DB# until
			 * execution of the first instruction of the
			 * BP# handler (in kernel mode).  Normally the
			 * previous check would ignore DB# exceptions
			 * for watchpoints on user addresses raised in
			 * kernel mode.  However, some CPU errata
			 * include cases where DB# exceptions do not
			 * properly set bits in %dr6, e.g. Haswell
			 * HSD23 and Skylake-X SKZ24.
			 *
			 * A deferred DB# can also be raised on the
			 * first instructions of system call entry
			 * points or single-step traps via similar use
			 * of 'pop %ss' or 'mov xxx, %ss'.
			 */
			if (pti) {
				if (frame->tf_rip ==
				    (uintptr_t)IDTVEC(fast_syscall_pti) ||
#ifdef COMPAT_FREEBSD32
				    frame->tf_rip ==
				    (uintptr_t)IDTVEC(int0x80_syscall_pti) ||
#endif
				    frame->tf_rip == (uintptr_t)IDTVEC(bpt_pti))
					return;
			} else {
				if (frame->tf_rip ==
				    (uintptr_t)IDTVEC(fast_syscall) ||
#ifdef COMPAT_FREEBSD32
				    frame->tf_rip ==
				    (uintptr_t)IDTVEC(int0x80_syscall) ||
#endif
				    frame->tf_rip == (uintptr_t)IDTVEC(bpt))
					return;
			}
			if (frame->tf_rip == (uintptr_t)IDTVEC(dbg) ||
			    /* Needed for AMD. */
			    frame->tf_rip == (uintptr_t)IDTVEC(fast_syscall32))
				return;
			/*
			 * FALLTHROUGH (TRCTRAP kernel mode, kernel address)
			 */
		case T_BPTFLT:
			/*
			 * If KDB is enabled, let it handle the debugger trap.
			 * Otherwise, debugger traps "can't happen".
			 */
#ifdef KDB
			if (kdb_trap(type, dr6, frame))
				return;
#endif
			break;
		}

		trap_fatal(frame, 0);
		return;
	}

	ksiginfo_init_trap(&ksi);
	ksi.ksi_signo = signo;
	ksi.ksi_code = ucode;
	ksi.ksi_trapno = type;
	ksi.ksi_addr = (void *)addr;
	if (uprintf_signal) {
		uprintf("pid %d comm %s: signal %d err %#lx code %d type %d "
		    "addr %#lx rsp %#lx rip %#lx rax %#lx "
		    "<%02x %02x %02x %02x %02x %02x %02x %02x>\n",
		    p->p_pid, p->p_comm, signo, frame->tf_err, ucode, type,
		    addr, frame->tf_rsp, frame->tf_rip, frame->tf_rax,
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

userret:
	userret(td, frame);
	KASSERT(PCB_USER_FPU(td->td_pcb),
	    ("Return from trap with kernel FPU ctx leaked"));
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

static bool
trap_is_smap(struct trapframe *frame)
{

	/*
	 * A page fault on a userspace address is classified as
	 * SMAP-induced if:
	 * - SMAP is supported;
	 * - kernel mode accessed present data page;
	 * - rflags.AC was cleared.
	 * Kernel must never access user space with rflags.AC cleared
	 * if SMAP is enabled.
	 */
	return ((cpu_stdext_feature & CPUID_STDEXT_SMAP) != 0 &&
	    (frame->tf_err & (PGEX_P | PGEX_U | PGEX_I | PGEX_RSV)) ==
	    PGEX_P && (frame->tf_rflags & PSL_AC) == 0);
}

static bool
trap_is_pti(struct trapframe *frame)
{

	return (PCPU_GET(curpmap)->pm_ucr3 != PMAP_NO_CR3 &&
	    pg_nx != 0 && (frame->tf_err & (PGEX_P | PGEX_W |
	    PGEX_U | PGEX_I)) == (PGEX_P | PGEX_U | PGEX_I) &&
	    (curpcb->pcb_saved_ucr3 & ~CR3_PCID_MASK) ==
	    (PCPU_GET(curpmap)->pm_cr3 & ~CR3_PCID_MASK));
}

/*
 * Handle all details of a page fault.
 * Returns:
 * -1 if this fault was fatal, typically from kernel mode
 *    (cannot happen, but we need to return something).
 * 0  if this fault was handled by updating either the user or kernel
 *    page table, execution can continue.
 * 1  if this fault was from usermode and it was not handled, a synchronous
 *    signal should be delivered to the thread.  *signo returns the signal
 *    number, *ucode gives si_code.
 */
static int
trap_pfault(struct trapframe *frame, bool usermode, int *signo, int *ucode)
{
	struct thread *td;
	struct proc *p;
	vm_map_t map;
	vm_offset_t eva;
	int rv;
	vm_prot_t ftype;

	MPASS(!usermode || (signo != NULL && ucode != NULL));

	td = curthread;
	p = td->td_proc;
	eva = frame->tf_addr;

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
	if (eva >= VM_MIN_KERNEL_ADDRESS) {
		/*
		 * Don't allow user-mode faults in kernel address space.
		 */
		if (usermode) {
			*signo = SIGSEGV;
			*ucode = SEGV_MAPERR;
			return (1);
		}

		map = kernel_map;
	} else {
		map = &p->p_vmspace->vm_map;

		/*
		 * When accessing a usermode address, kernel must be
		 * ready to accept the page fault, and provide a
		 * handling routine.  Since accessing the address
		 * without the handler is a bug, do not try to handle
		 * it normally, and panic immediately.
		 *
		 * If SMAP is enabled, filter SMAP faults also,
		 * because illegal access might occur to the mapped
		 * user address, causing infinite loop.
		 */
		if (!usermode && (td->td_intr_nesting_level != 0 ||
		    trap_is_smap(frame) || curpcb->pcb_onfault == NULL)) {
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
	 * User-mode protection key violation (PKU).  May happen
	 * either from usermode or from kernel if copyin accessed
	 * key-protected mapping.
	 */
	if ((frame->tf_err & PGEX_PK) != 0) {
		if (eva > VM_MAXUSER_ADDRESS) {
			trap_fatal(frame, eva);
			return (-1);
		}
		if (usermode) {
			*signo = SIGSEGV;
			*ucode = SEGV_PKUERR;
			return (1);
		}
		goto after_vmfault;
	}

	/*
	 * If nx protection of the usermode portion of kernel page
	 * tables caused trap, panic.
	 */
	if (usermode && trap_is_pti(frame))
		panic("PTI: pid %d comm %s tf_err %#lx", p->p_pid,
		    p->p_comm, frame->tf_err);

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
	rv = vm_fault_trap(map, eva, ftype, VM_FAULT_NORMAL, signo, ucode);
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

	if (usermode)
		return (1);
after_vmfault:
	if (td->td_intr_nesting_level == 0 &&
	    curpcb->pcb_onfault != NULL) {
		if ((td->td_pflags & TDP_EFIRT) != 0) {
			u_long cnt = atomic_fetchadd_long(&cnt_efirt_faults, 1);

			if ((print_efirt_faults == 1 && cnt == 0) ||
			    print_efirt_faults == 2) {
				trap_diag(frame, eva);
				printf("EFI RT page fault\n");
			}
		}
		frame->tf_rip = (long)curpcb->pcb_onfault;
		return (0);
	}
	trap_fatal(frame, eva);
	return (-1);
}

static void
trap_diag(struct trapframe *frame, vm_offset_t eva)
{
	int code, ss;
	u_int type;
	struct soft_segment_descriptor softseg;
	struct user_segment_descriptor *gdt;

	code = frame->tf_err;
	type = frame->tf_trapno;
	gdt = *PCPU_PTR(gdt);
	sdtossd(&gdt[IDXSEL(frame->tf_cs & 0xffff)], &softseg);

	printf("\n\nFatal trap %d: %s while in %s mode\n", type,
	    type < nitems(trap_msg) ? trap_msg[type] : UNKNOWN,
	    TRAPF_USERMODE(frame) ? "user" : "kernel");
#ifdef SMP
	/* two separate prints in case of a trap on an unmapped page */
	printf("cpuid = %d; ", PCPU_GET(cpuid));
	printf("apic id = %02x\n", PCPU_GET(apic_id));
#endif
	if (type == T_PAGEFLT) {
		printf("fault virtual address	= 0x%lx\n", eva);
		printf("fault code		= %s %s %s%s%s, %s\n",
			code & PGEX_U ? "user" : "supervisor",
			code & PGEX_W ? "write" : "read",
			code & PGEX_I ? "instruction" : "data",
			code & PGEX_PK ? " prot key" : "",
			code & PGEX_SGX ? " SGX" : "",
			code & PGEX_RSV ? "reserved bits in PTE" :
			code & PGEX_P ? "protection violation" : "page not present");
	}
	printf("instruction pointer	= 0x%lx:0x%lx\n",
	       frame->tf_cs & 0xffff, frame->tf_rip);
	ss = frame->tf_ss & 0xffff;
	printf("stack pointer	        = 0x%x:0x%lx\n", ss, frame->tf_rsp);
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

	printf("rdi: %016lx rsi: %016lx rdx: %016lx\n", frame->tf_rdi,
	    frame->tf_rsi, frame->tf_rdx);
	printf("rcx: %016lx  r8: %016lx  r9: %016lx\n", frame->tf_rcx,
	    frame->tf_r8, frame->tf_r9);
	printf("rax: %016lx rbx: %016lx rbp: %016lx\n", frame->tf_rax,
	    frame->tf_rbx, frame->tf_rbp);
	printf("r10: %016lx r11: %016lx r12: %016lx\n", frame->tf_r10,
	    frame->tf_r11, frame->tf_r12);
	printf("r13: %016lx r14: %016lx r15: %016lx\n", frame->tf_r13,
	    frame->tf_r14, frame->tf_r15);

	printf("trap number		= %d\n", type);
}

static void
trap_fatal(struct trapframe *frame, vm_offset_t eva)
{
	u_int type;

	type = frame->tf_trapno;
	trap_diag(frame, eva);
#ifdef KDB
	if (debugger_on_trap) {
		bool handled;

		kdb_why = KDB_WHY_TRAP;
		handled = kdb_trap(type, 0, frame);
		kdb_why = KDB_WHY_UNSET;
		if (handled)
			return;
	}
#endif
	panic("%s", traptype_to_msg(type));
}

#ifdef KDTRACE_HOOKS
/*
 * Invoke a userspace DTrace hook.  The hook pointer is cleared when no
 * userspace probes are enabled, so we must synchronize with DTrace to ensure
 * that a trapping thread is able to call the hook before it is cleared.
 */
static bool
trap_user_dtrace(struct trapframe *frame, int (**hookp)(struct trapframe *))
{
	int (*hook)(struct trapframe *);

	hook = atomic_load_ptr(hookp);
	enable_intr();
	if (hook != NULL)
		return ((hook)(frame) == 0);
	return (false);
}
#endif

/*
 * Double fault handler. Called when a fault occurs while writing
 * a frame for a trap/exception onto the stack. This usually occurs
 * when the stack overflows (such is the case with infinite recursion,
 * for example).
 */
void
dblfault_handler(struct trapframe *frame)
{
	kmsan_mark(frame, sizeof(*frame), KMSAN_STATE_INITED);
#ifdef KDTRACE_HOOKS
	if (dtrace_doubletrap_func != NULL)
		(*dtrace_doubletrap_func)();
#endif
	printf("\nFatal double fault\n"
	    "rip %#lx rsp %#lx rbp %#lx\n"
	    "rax %#lx rdx %#lx rbx %#lx\n"
	    "rcx %#lx rsi %#lx rdi %#lx\n"
	    "r8 %#lx r9 %#lx r10 %#lx\n"
	    "r11 %#lx r12 %#lx r13 %#lx\n"
	    "r14 %#lx r15 %#lx rflags %#lx\n"
	    "cs %#lx ss %#lx ds %#hx es %#hx fs %#hx gs %#hx\n"
	    "fsbase %#lx gsbase %#lx kgsbase %#lx\n",
	    frame->tf_rip, frame->tf_rsp, frame->tf_rbp,
	    frame->tf_rax, frame->tf_rdx, frame->tf_rbx,
	    frame->tf_rcx, frame->tf_rdi, frame->tf_rsi,
	    frame->tf_r8, frame->tf_r9, frame->tf_r10,
	    frame->tf_r11, frame->tf_r12, frame->tf_r13,
	    frame->tf_r14, frame->tf_r15, frame->tf_rflags,
	    frame->tf_cs, frame->tf_ss, frame->tf_ds, frame->tf_es,
	    frame->tf_fs, frame->tf_gs,
	    rdmsr(MSR_FSBASE), rdmsr(MSR_GSBASE), rdmsr(MSR_KGSBASE));
#ifdef SMP
	/* two separate prints in case of a trap on an unmapped page */
	printf("cpuid = %d; ", PCPU_GET(cpuid));
	printf("apic id = %02x\n", PCPU_GET(apic_id));
#endif
	panic("double fault");
}

static int __noinline
cpu_fetch_syscall_args_fallback(struct thread *td, struct syscall_args *sa)
{
	struct proc *p;
	struct trapframe *frame;
	syscallarg_t *argp;
	caddr_t params;
	int reg, regcnt, error;

	p = td->td_proc;
	frame = td->td_frame;
	reg = 0;
	regcnt = NARGREGS;

	if (sa->code == SYS_syscall || sa->code == SYS___syscall) {
		sa->code = frame->tf_rdi;
		reg++;
		regcnt--;
	}

	if (sa->code >= p->p_sysent->sv_size)
		sa->callp = &nosys_sysent;
	else
		sa->callp = &p->p_sysent->sv_table[sa->code];

	KASSERT(sa->callp->sy_narg <= nitems(sa->args),
	    ("Too many syscall arguments!"));
	argp = &frame->tf_rdi;
	argp += reg;
	memcpy(sa->args, argp, sizeof(sa->args[0]) * NARGREGS);
	if (sa->callp->sy_narg > regcnt) {
		params = (caddr_t)frame->tf_rsp + sizeof(register_t);
		error = copyin(params, &sa->args[regcnt],
		    (sa->callp->sy_narg - regcnt) * sizeof(sa->args[0]));
		if (__predict_false(error != 0))
			return (error);
	}

	td->td_retval[0] = 0;
	td->td_retval[1] = frame->tf_rdx;

	return (0);
}

int
cpu_fetch_syscall_args(struct thread *td)
{
	struct proc *p;
	struct trapframe *frame;
	struct syscall_args *sa;

	p = td->td_proc;
	frame = td->td_frame;
	sa = &td->td_sa;

	sa->code = frame->tf_rax;
	sa->original_code = sa->code;

	if (__predict_false(sa->code == SYS_syscall ||
	    sa->code == SYS___syscall ||
	    sa->code >= p->p_sysent->sv_size))
		return (cpu_fetch_syscall_args_fallback(td, sa));

	sa->callp = &p->p_sysent->sv_table[sa->code];
	KASSERT(sa->callp->sy_narg <= nitems(sa->args),
	    ("Too many syscall arguments!"));

	if (__predict_false(sa->callp->sy_narg > NARGREGS))
		return (cpu_fetch_syscall_args_fallback(td, sa));

	memcpy(sa->args, &frame->tf_rdi, sizeof(sa->args[0]) * NARGREGS);

	td->td_retval[0] = 0;
	td->td_retval[1] = frame->tf_rdx;

	return (0);
}

#include "../../kern/subr_syscall.c"

static void (*syscall_ret_l1d_flush)(void);
int syscall_ret_l1d_flush_mode;

static void
flush_l1d_hw(void)
{

	wrmsr(MSR_IA32_FLUSH_CMD, IA32_FLUSH_CMD_L1D);
}

static void __noinline
amd64_syscall_ret_flush_l1d_check(int error)
{
	void (*p)(void);

	if (error != EEXIST && error != EAGAIN && error != EXDEV &&
	    error != ENOENT && error != ENOTCONN && error != EINPROGRESS) {
		p = atomic_load_ptr(&syscall_ret_l1d_flush);
		if (p != NULL)
			p();
	}
}

static void __inline
amd64_syscall_ret_flush_l1d_check_inline(int error)
{

	if (__predict_false(error != 0))
		amd64_syscall_ret_flush_l1d_check(error);
}

void
amd64_syscall_ret_flush_l1d(int error)
{

	amd64_syscall_ret_flush_l1d_check_inline(error);
}

void
amd64_syscall_ret_flush_l1d_recalc(void)
{
	bool l1d_hw;

	l1d_hw = (cpu_stdext_feature3 & CPUID_STDEXT3_L1D_FLUSH) != 0;
again:
	switch (syscall_ret_l1d_flush_mode) {
	case 0:
		syscall_ret_l1d_flush = NULL;
		break;
	case 1:
		syscall_ret_l1d_flush = l1d_hw ? flush_l1d_hw :
		    flush_l1d_sw_abi;
		break;
	case 2:
		syscall_ret_l1d_flush = l1d_hw ? flush_l1d_hw : NULL;
		break;
	case 3:
		syscall_ret_l1d_flush = flush_l1d_sw_abi;
		break;
	default:
		syscall_ret_l1d_flush_mode = 1;
		goto again;
	}
}

static int
machdep_syscall_ret_flush_l1d(SYSCTL_HANDLER_ARGS)
{
	int error, val;

	val = syscall_ret_l1d_flush_mode;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	syscall_ret_l1d_flush_mode = val;
	amd64_syscall_ret_flush_l1d_recalc();
	return (0);
}
SYSCTL_PROC(_machdep, OID_AUTO, syscall_ret_flush_l1d, CTLTYPE_INT |
    CTLFLAG_RWTUN | CTLFLAG_NOFETCH | CTLFLAG_MPSAFE, NULL, 0,
    machdep_syscall_ret_flush_l1d, "I",
    "Flush L1D on syscall return with error (0 - off, 1 - on, "
    "2 - use hw only, 3 - use sw only)");

/*
 * System call handler for native binaries.  The trap frame is already
 * set up by the assembler trampoline and a pointer to it is saved in
 * td_frame.
 */
void
amd64_syscall(struct thread *td, int traced)
{
	ksiginfo_t ksi;

	kmsan_mark(td->td_frame, sizeof(*td->td_frame), KMSAN_STATE_INITED);

	KASSERT(TRAPF_USERMODE(td->td_frame),
	    ("%s: not from user mode", __func__));

	syscallenter(td);

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
	    ("System call %s returning with kernel FPU ctx leaked",
	     syscallname(td->td_proc, td->td_sa.code)));
	KASSERT(td->td_pcb->pcb_save == get_pcb_user_save_td(td),
	    ("System call %s returning with mangled pcb_save",
	     syscallname(td->td_proc, td->td_sa.code)));
	KASSERT(pmap_not_in_di(),
	    ("System call %s returning with leaked invl_gen %lu",
	    syscallname(td->td_proc, td->td_sa.code),
	    td->td_md.md_invl_gen.gen));

	syscallret(td);

	/*
	 * If the user-supplied value of %rip is not a canonical
	 * address, then some CPUs will trigger a ring 0 #GP during
	 * the sysret instruction.  However, the fault handler would
	 * execute in ring 0 with the user's %gs and %rsp which would
	 * not be safe.  Instead, use the full return path which
	 * catches the problem safely.
	 */
	if (__predict_false(td->td_frame->tf_rip >= (la57 ?
	    VM_MAXUSER_ADDRESS_LA57 : VM_MAXUSER_ADDRESS_LA48)))
		set_pcb_flags(td->td_pcb, PCB_FULL_IRET);

	amd64_syscall_ret_flush_l1d_check_inline(td->td_errno);
}
