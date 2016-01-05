/*-
 * Copyright 2014 Olivier Houchard <cognet@FreeBSD.org>
 * Copyright 2014 Svatopluk Kraus <onwahe@gmail.com>
 * Copyright 2014 Michal Meloun <meloun@miracle.cz>
 * Copyright 2014 Andrew Turner <andrew@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#include "opt_ktrace.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/signalvar.h>
#include <sys/ktr.h>
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>

#include <machine/acle-compat.h>
#include <machine/cpu.h>
#include <machine/cpu-v6.h>
#include <machine/frame.h>
#include <machine/machdep.h>
#include <machine/pcb.h>
#include <machine/vmparam.h>

#ifdef KDB
#include <sys/kdb.h>
#include <machine/db_machdep.h>
#endif

extern char fusubailout[];
extern char cachebailout[];

#ifdef DEBUG
int last_fault_code;	/* For the benefit of pmap_fault_fixup() */
#endif

struct ksig {
	int sig;
	u_long code;
	vm_offset_t	addr;
};

typedef int abort_func_t(struct trapframe *, u_int, u_int, u_int, u_int,
    struct thread *, struct ksig *);

static abort_func_t abort_fatal;
static abort_func_t abort_align;
static abort_func_t abort_icache;

struct abort {
	abort_func_t	*func;
	const char	*desc;
};

/*
 * How are the aborts handled?
 *
 * Undefined Code:
 *  - Always fatal as we do not know what does it mean.
 * Imprecise External Abort:
 *  - Always fatal, but can be handled somehow in the future.
 *    Now, due to PCIe buggy harware, ignored.
 * Precise External Abort:
 *  - Always fatal, but who knows in the future???
 * Debug Event:
 *  - Special handling.
 * External Translation Abort (L1 & L2)
 *  - Always fatal as something is screwed up in page tables or harware.
 * Domain Fault (L1 & L2):
 *  - Always fatal as we do not play game with domains.
 * Alignment Fault:
 *  - Everything should be aligned in kernel including user to kernel and
 *    vice versa data copying, so we ignore pcb_onfault, and it's always fatal.
 *    We generate signal in case of abort from user mode.
 * Instruction cache maintenance:
 *  - According to manual, this is translation fault during cache maintenance
 *    operation. So, it could be really complex in SMP case and fuzzy too
 *    for cache operations working on virtual addresses. For now, we will
 *    consider this abort as fatal. In fact, no cache maintenance on
 *    not mapped virtual addresses should be called. As cache maintenance
 *    operation (except DMB, DSB, and Flush Prefetch Buffer) are priviledged,
 *    the abort is fatal for user mode as well for now. (This is good place to
 *    note that cache maintenance on virtual address fill TLB.)
 * Acces Bit (L1 & L2):
 *  - Fast hardware emulation for kernel and user mode.
 * Translation Fault (L1 & L2):
 *  - Standard fault mechanism is held including vm_fault().
 * Permission Fault (L1 & L2):
 *  - Fast harware emulation of modify bits and in other cases, standard
 *    fault mechanism is held including vm_fault().
 */

static const struct abort aborts[] = {
	{abort_fatal,	"Undefined Code (0x000)"},
	{abort_align,	"Alignment Fault"},
	{abort_fatal,	"Debug Event"},
	{NULL,		"Access Bit (L1)"},
	{NULL,		"Instruction cache maintenance"},
	{NULL,		"Translation Fault (L1)"},
	{NULL,		"Access Bit (L2)"},
	{NULL,		"Translation Fault (L2)"},

	{abort_fatal,	"External Abort"},
	{abort_fatal,	"Domain Fault (L1)"},
	{abort_fatal,	"Undefined Code (0x00A)"},
	{abort_fatal,	"Domain Fault (L2)"},
	{abort_fatal,	"External Translation Abort (L1)"},
	{NULL,		"Permission Fault (L1)"},
	{abort_fatal,	"External Translation Abort (L2)"},
	{NULL,		"Permission Fault (L2)"},

	{abort_fatal,	"TLB Conflict Abort"},
	{abort_fatal,	"Undefined Code (0x401)"},
	{abort_fatal,	"Undefined Code (0x402)"},
	{abort_fatal,	"Undefined Code (0x403)"},
	{abort_fatal,	"Undefined Code (0x404)"},
	{abort_fatal,	"Undefined Code (0x405)"},
	{abort_fatal,	"Asynchronous External Abort"},
	{abort_fatal,	"Undefined Code (0x407)"},

	{abort_fatal,	"Asynchronous Parity Error on Memory Access"},
	{abort_fatal,	"Parity Error on Memory Access"},
	{abort_fatal,	"Undefined Code (0x40A)"},
	{abort_fatal,	"Undefined Code (0x40B)"},
	{abort_fatal,	"Parity Error on Translation (L1)"},
	{abort_fatal,	"Undefined Code (0x40D)"},
	{abort_fatal,	"Parity Error on Translation (L2)"},
	{abort_fatal,	"Undefined Code (0x40F)"}
};


static __inline void
call_trapsignal(struct thread *td, int sig, int code, vm_offset_t addr)
{
	ksiginfo_t ksi;

	CTR4(KTR_TRAP, "%s: addr: %#x, sig: %d, code: %d",
	   __func__, addr, sig, code);

	/*
	 * TODO: some info would be nice to know
	 * if we are serving data or prefetch abort.
	 */

	ksiginfo_init_trap(&ksi);
	ksi.ksi_signo = sig;
	ksi.ksi_code = code;
	ksi.ksi_addr = (void *)addr;
	trapsignal(td, &ksi);
}

/*
 * abort_imprecise() handles the following abort:
 *
 *  FAULT_EA_IMPREC - Imprecise External Abort
 *
 * The imprecise means that we don't know where the abort happened,
 * thus FAR is undefined. The abort should not never fire, but hot
 * plugging or accidental harware failure can be the cause of it.
 * If the abort happens, it can even be on different (thread) context.
 * Without any additional support, the abort is fatal, as we do not
 * know what really happened.
 *
 * QQQ: Some additional functionality, like pcb_onfault but global,
 *      can be implemented. Imprecise handlers could be registered
 *      which tell us if the abort is caused by something they know
 *      about. They should return one of three codes like:
 *		FAULT_IS_MINE,
 *		FAULT_CAN_BE_MINE,
 *		FAULT_IS_NOT_MINE.
 *      The handlers should be called until some of them returns
 *      FAULT_IS_MINE value or all was called. If all handlers return
 *	FAULT_IS_NOT_MINE value, then the abort is fatal.
 */
static __inline void
abort_imprecise(struct trapframe *tf, u_int fsr, u_int prefetch, u_int usermode)
{
	/* XXXX  We can got imprecise abort as result of access
	 * to not-present PCI/PCIe configuration space.
	 */
#if 0
	goto out;
#endif
	abort_fatal(tf, FAULT_EA_IMPREC, fsr, 0, prefetch, curthread, NULL);

	/*
	 * Returning from this function means that we ignore
	 * the abort for good reason. Note that imprecise abort
	 * could fire any time even in user mode.
	 */

#if 0
out:
	if (usermode)
		userret(curthread, tf);
#endif
}

/*
 * abort_debug() handles the following abort:
 *
 *  FAULT_DEBUG - Debug Event
 *
 */
static __inline void
abort_debug(struct trapframe *tf, u_int fsr, u_int prefetch, u_int usermode,
    u_int far)
{
	if (usermode) {
		struct thread *td;

		td = curthread;
		call_trapsignal(td, SIGTRAP, TRAP_BRKPT, far);
		userret(td, tf);
	} else {
#ifdef KDB
		kdb_trap(T_BREAKPOINT, 0, tf);
#else
		printf("No debugger in kernel.\n");
#endif
	}
}

/*
 * Abort handler.
 *
 * FAR, FSR, and everything what can be lost after enabling
 * interrupts must be grabbed before the interrupts will be
 * enabled. Note that when interrupts will be enabled, we
 * could even migrate to another CPU ...
 *
 * TODO: move quick cases to ASM
 */
void
abort_handler(struct trapframe *tf, int prefetch)
{
	struct thread *td;
	vm_offset_t far, va;
	int idx, usermode;
	uint32_t fsr;
	struct ksig ksig;
	struct proc *p;
	struct pcb *pcb;
	struct vm_map *map;
	struct vmspace *vm;
	vm_prot_t ftype;
	int rv;
#ifdef INVARIANTS
	void *onfault;
#endif
	td = curthread;
	fsr = (prefetch) ? cp15_ifsr_get(): cp15_dfsr_get();
#if __ARM_ARCH >= 7
	far = (prefetch) ? cp15_ifar_get() : cp15_dfar_get();
#else
	far = (prefetch) ? TRAPF_PC(tf) : cp15_dfar_get();
#endif

	idx = FSR_TO_FAULT(fsr);
	usermode = TRAPF_USERMODE(tf);	/* Abort came from user mode? */
	if (usermode)
		td->td_frame = tf;

	CTR4(KTR_TRAP, "abort_handler: fsr %#x (idx %u) far %#x prefetch %u",
	fsr, idx, far, prefetch);

	/*
	 * Firstly, handle aborts that are not directly related to mapping.
	 */
	if (__predict_false(idx == FAULT_EA_IMPREC)) {
		abort_imprecise(tf, fsr, prefetch, usermode);
		return;
	}

	if (__predict_false(idx == FAULT_DEBUG)) {
		abort_debug(tf, fsr, prefetch, usermode, far);
		return;
	}

#ifdef ARM_NEW_PMAP
	rv = pmap_fault(PCPU_GET(curpmap), far, fsr, idx, usermode);
	if (rv == 0) {
		return;
	} else if (rv == EFAULT) {

		call_trapsignal(td, SIGSEGV, SEGV_MAPERR, far);
		userret(td, tf);
		return;
	}
#endif
	/*
	 * Now, when we handled imprecise and debug aborts, the rest of
	 * aborts should be really related to mapping.
	 *
	 */

	PCPU_INC(cnt.v_trap);

#ifdef KDB
	if (kdb_active) {
		kdb_reenter();
		goto out;
	}
#endif
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
		if (td->td_md.md_spurflt_addr != far ||
		    (td->td_pflags & TDP_RESETSPUR) != 0) {
			td->td_md.md_spurflt_addr = far;
			td->td_pflags &= ~TDP_RESETSPUR;

			tlb_flush_local(far & ~PAGE_MASK);
			return;
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
			abort_fatal(tf, idx, fsr, far, prefetch, td, &ksig);
			return;
		}
	}

	/* Re-enable interrupts if they were enabled previously. */
	if (td->td_md.md_spinlock_count == 0) {
		if (__predict_true(tf->tf_spsr & PSR_I) == 0)
			enable_interrupts(PSR_I);
		if (__predict_true(tf->tf_spsr & PSR_F) == 0)
			enable_interrupts(PSR_F);
	}

	p = td->td_proc;
	if (usermode) {
		td->td_pticks = 0;
		if (td->td_cowgen != p->p_cowgen)
			thread_cow_update(td);
	}

	/* Invoke the appropriate handler, if necessary. */
	if (__predict_false(aborts[idx].func != NULL)) {
		if ((aborts[idx].func)(tf, idx, fsr, far, prefetch, td, &ksig))
			goto do_trapsignal;
		goto out;
	}

	/*
	 * Don't pass faulting cache operation to vm_fault(). We don't want
	 * to handle all vm stuff at this moment.
	 */
	pcb = td->td_pcb;
	if (__predict_false(pcb->pcb_onfault == cachebailout)) {
		tf->tf_r0 = far;		/* return failing address */
		tf->tf_pc = (register_t)pcb->pcb_onfault;
		return;
	}

	/* Handle remaining I cache aborts. */
	if (idx == FAULT_ICACHE) {
		if (abort_icache(tf, idx, fsr, far, prefetch, td, &ksig))
			goto do_trapsignal;
		goto out;
	}

	/*
	 * At this point, we're dealing with one of the following aborts:
	 *
	 *  FAULT_TRAN_xx  - Translation
	 *  FAULT_PERM_xx  - Permission
	 *
	 * These are the main virtual memory-related faults signalled by
	 * the MMU.
	 */

	/* fusubailout is used by [fs]uswintr to avoid page faulting */
	pcb = td->td_pcb;
	if (__predict_false(pcb->pcb_onfault == fusubailout)) {
		tf->tf_r0 = EFAULT;
		tf->tf_pc = (register_t)pcb->pcb_onfault;
		return;
	}

	/*
	 * QQQ: ARM has a set of unprivileged load and store instructions
	 *      (LDRT/LDRBT/STRT/STRBT ...) which are supposed to be used
	 *      in other than user mode and OS should recognize their
	 *      aborts and behaved appropriately. However, there is no way
	 *      how to do that reasonably in general unless we restrict
	 *      the handling somehow. One way is to limit the handling for
	 *      aborts which come from undefined mode only.
	 *
	 *      Anyhow, we do not use these instructions and do not implement
	 *      any special handling for them.
	 */

	va = trunc_page(far);
	if (va >= KERNBASE) {
		/*
		 * Don't allow user-mode faults in kernel address space.
		 */
		if (usermode)
			goto nogo;

		map = kernel_map;
	} else {
		/*
		 * This is a fault on non-kernel virtual memory. If curproc
		 * is NULL or curproc->p_vmspace is NULL the fault is fatal.
		 */
		vm = (p != NULL) ? p->p_vmspace : NULL;
		if (vm == NULL)
			goto nogo;

		map = &vm->vm_map;
		if (!usermode && (td->td_intr_nesting_level != 0 ||
		    pcb->pcb_onfault == NULL)) {
			abort_fatal(tf, idx, fsr, far, prefetch, td, &ksig);
			return;
		}
	}

	ftype = (fsr & FSR_WNR) ? VM_PROT_WRITE : VM_PROT_READ;
	if (prefetch)
		ftype |= VM_PROT_EXECUTE;

#ifdef DEBUG
	last_fault_code = fsr;
#endif

#ifndef ARM_NEW_PMAP
	if (pmap_fault_fixup(vmspace_pmap(td->td_proc->p_vmspace), va, ftype,
	    usermode)) {
		goto out;
	}
#endif

#ifdef INVARIANTS
	onfault = pcb->pcb_onfault;
	pcb->pcb_onfault = NULL;
#endif

	/* Fault in the page. */
	rv = vm_fault(map, va, ftype, VM_FAULT_NORMAL);

#ifdef INVARIANTS
	pcb->pcb_onfault = onfault;
#endif

	if (__predict_true(rv == KERN_SUCCESS))
		goto out;
nogo:
	if (!usermode) {
		if (td->td_intr_nesting_level == 0 &&
		    pcb->pcb_onfault != NULL) {
			tf->tf_r0 = rv;
			tf->tf_pc = (int)pcb->pcb_onfault;
			return;
		}
		CTR2(KTR_TRAP, "%s: vm_fault() failed with %d", __func__, rv);
		abort_fatal(tf, idx, fsr, far, prefetch, td, &ksig);
		return;
	}

	ksig.sig = SIGSEGV;
	ksig.code = (rv == KERN_PROTECTION_FAILURE) ? SEGV_ACCERR : SEGV_MAPERR;
	ksig.addr = far;

do_trapsignal:
	call_trapsignal(td, ksig.sig, ksig.code, ksig.addr);
out:
	if (usermode)
		userret(td, tf);
}

/*
 * abort_fatal() handles the following data aborts:

 *  FAULT_DEBUG		- Debug Event
 *  FAULT_ACCESS_xx	- Acces Bit
 *  FAULT_EA_PREC	- Precise External Abort
 *  FAULT_DOMAIN_xx	- Domain Fault
 *  FAULT_EA_TRAN_xx	- External Translation Abort
 *  FAULT_EA_IMPREC	- Imprecise External Abort
 *  + all undefined codes for ABORT
 *
 * We should never see these on a properly functioning system.
 *
 * This function is also called by the other handlers if they
 * detect a fatal problem.
 *
 * Note: If 'l' is NULL, we assume we're dealing with a prefetch abort.
 */
static int
abort_fatal(struct trapframe *tf, u_int idx, u_int fsr, u_int far, u_int prefetch,
    struct thread *td, struct ksig *ksig)
{
	u_int usermode;
	const char *mode;
	const char *rw_mode;

	usermode = TRAPF_USERMODE(tf);
	mode = usermode ? "user" : "kernel";
	rw_mode  = fsr & FSR_WNR ? "write" : "read";
	disable_interrupts(PSR_I|PSR_F);

	if (td != NULL) {
		printf("Fatal %s mode data abort: '%s' on %s\n", mode,
		    aborts[idx].desc, rw_mode);
		printf("trapframe: %p\nFSR=%08x, FAR=", tf, fsr);
		if (idx != FAULT_EA_IMPREC)
			printf("%08x, ", far);
		else
			printf("Invalid,  ");
		printf("spsr=%08x\n", tf->tf_spsr);
	} else {
		printf("Fatal %s mode prefetch abort at 0x%08x\n",
		    mode, tf->tf_pc);
		printf("trapframe: %p, spsr=%08x\n", tf, tf->tf_spsr);
	}

	printf("r0 =%08x, r1 =%08x, r2 =%08x, r3 =%08x\n",
	    tf->tf_r0, tf->tf_r1, tf->tf_r2, tf->tf_r3);
	printf("r4 =%08x, r5 =%08x, r6 =%08x, r7 =%08x\n",
	    tf->tf_r4, tf->tf_r5, tf->tf_r6, tf->tf_r7);
	printf("r8 =%08x, r9 =%08x, r10=%08x, r11=%08x\n",
	    tf->tf_r8, tf->tf_r9, tf->tf_r10, tf->tf_r11);
	printf("r12=%08x, ", tf->tf_r12);

	if (usermode)
		printf("usp=%08x, ulr=%08x",
		    tf->tf_usr_sp, tf->tf_usr_lr);
	else
		printf("ssp=%08x, slr=%08x",
		    tf->tf_svc_sp, tf->tf_svc_lr);
	printf(", pc =%08x\n\n", tf->tf_pc);

#ifdef KDB
	if (debugger_on_panic || kdb_active)
		kdb_trap(fsr, 0, tf);
#endif
	panic("Fatal abort");
	/*NOTREACHED*/
}

/*
 * abort_align() handles the following data abort:
 *
 *  FAULT_ALIGN - Alignment fault
 *
 * Every memory access should be correctly aligned in kernel including
 * user to kernel and vice versa data copying, so we ignore pcb_onfault,
 * and it's always fatal. We generate a signal in case of abort from user mode.
 */
static int
abort_align(struct trapframe *tf, u_int idx, u_int fsr, u_int far, u_int prefetch,
    struct thread *td, struct ksig *ksig)
{
	u_int usermode;

	usermode = TRAPF_USERMODE(tf);

	/*
	 * Alignment faults are always fatal if they occur in any but user mode.
	 *
	 * XXX The old trap code handles pcb fault even for alignment traps.
	 * Unfortunately, we don't known why and if is this need.
	 */
	if (!usermode) {
		if (td->td_intr_nesting_level == 0 && td != NULL &&
		    td->td_pcb->pcb_onfault != NULL) {
			printf("%s: Got alignment fault with pcb_onfault set"
			    ", please report this issue\n", __func__);
			tf->tf_r0 = EFAULT;;
			tf->tf_pc = (int)td->td_pcb->pcb_onfault;
			return (0);
		}
		abort_fatal(tf, idx, fsr, far, prefetch, td, ksig);
	}
	/* Deliver a bus error signal to the process */
	ksig->code = 0;
	ksig->sig = SIGBUS;
	ksig->addr = far;
	return (1);
}

/*
 * abort_icache() handles the following data abort:
 *
 * FAULT_ICACHE - Instruction cache maintenance
 *
 * According to manual, FAULT_ICACHE is translation fault during cache
 * maintenance operation. In fact, no cache maintenance operation on
 * not mapped virtual addresses should be called. As cache maintenance
 * operation (except DMB, DSB, and Flush Prefetch Buffer) are priviledged,
 * the abort is concider as fatal for now. However, all the matter with
 * cache maintenance operation on virtual addresses could be really complex
 * and fuzzy in SMP case, so maybe in future standard fault mechanism
 * should be held here including vm_fault() calling.
 */
static int
abort_icache(struct trapframe *tf, u_int idx, u_int fsr, u_int far, u_int prefetch,
    struct thread *td, struct ksig *ksig)
{
	abort_fatal(tf, idx, fsr, far, prefetch, td, ksig);
	return(0);
}
