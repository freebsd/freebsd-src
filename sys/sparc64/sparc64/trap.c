/*-
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
 */
/*-
 * Copyright (c) 2001 Jake Burkholder.
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
 *
 * $FreeBSD$
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/sysent.h>
#include <sys/user.h>

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
#include <machine/tte.h>
#include <machine/tlb.h>
#include <machine/tsb.h>

void trap(struct trapframe *tf);
int trap_mmu_fault(struct proc *p, struct trapframe *tf);

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
	"clock",
	"bad spill",
	"bad fill",
	"breakpoint",
};

void
trap(struct trapframe *tf)
{
	u_quad_t sticks;
	struct proc *p;
	int error;
	int ucode;
	int type;
	int sig;

	KASSERT(PCPU_GET(curproc) != NULL, ("trap: curproc NULL"));
	KASSERT(PCPU_GET(curpcb) != NULL, ("trap: curpcb NULL"));

	p = PCPU_GET(curproc);
	type = T_TYPE(tf->tf_type);
	ucode = type;	/* XXX */

	if ((type & T_KERNEL) == 0) {
		mtx_lock_spin(&sched_lock);
		sticks = p->p_sticks;
		mtx_unlock_spin(&sched_lock);
	}

	switch (type) {
	case T_FP_DISABLED:
		if (fp_enable_proc(p))
			goto user;
		else {
			sig = SIGFPE;
			goto trapsig;
		}
		break;
	case T_IMMU_MISS:
	case T_DMMU_MISS:
	case T_DMMU_PROT:
		mtx_lock(&Giant);
		error = trap_mmu_fault(p, tf);
		mtx_unlock(&Giant);
		if (error == 0)
			goto user;
		break;
	case T_INTR:
		intr_dispatch(T_LEVEL(tf->tf_type), tf);
		goto user;
#ifdef DDB
	case T_BREAKPOINT | T_KERNEL:
		if (kdb_trap(tf) != 0)
			goto out;
		break;
#endif
	case T_DMMU_MISS | T_KERNEL:
	case T_DMMU_PROT | T_KERNEL:
		mtx_lock(&Giant);
		error = trap_mmu_fault(p, tf);
		mtx_unlock(&Giant);
		if (error == 0)
			goto out;
		break;
	case T_INTR | T_KERNEL:
		intr_dispatch(T_LEVEL(tf->tf_type), tf);
		goto out;
	default:
		break;
	}
	panic("trap: %s", trap_msg[type & ~T_KERNEL]);

trapsig:
	mtx_lock(&Giant);
	/* Translate fault for emulators. */
	if (p->p_sysent->sv_transtrap != NULL)
		sig = (p->p_sysent->sv_transtrap)(sig, type);

	trapsignal(p, sig, ucode);
	mtx_unlock(&Giant);
user:
	userret(p, tf, sticks);
	if (mtx_owned(&Giant))
		mtx_unlock(&Giant);
out:
	return;
}

int
trap_mmu_fault(struct proc *p, struct trapframe *tf)
{
	struct mmuframe *mf;
	struct vmspace *vm;
	vm_offset_t va;
	vm_prot_t type;
	int rv;

	KASSERT(p->p_vmspace != NULL, ("trap_dmmu_miss: vmspace NULL"));

	type = 0;
	rv = KERN_FAILURE;
	mf = tf->tf_arg;
	va = TLB_TAR_VA(mf->mf_tar);
	switch (tf->tf_type) {
	case T_DMMU_MISS | T_KERNEL:
		/*
		 * If the context is nucleus this is a soft fault on kernel
		 * memory, just fault in the pages.
		 */
		if (TLB_TAR_CTX(mf->mf_tar) == TLB_CTX_KERNEL) {
			rv = vm_fault(kernel_map, va, VM_PROT_READ,
			    VM_FAULT_NORMAL);
			break;
		}

		/*
		 * Don't allow kernel mode faults on user memory unless
		 * pcb_onfault is set.
		 */
		if (PCPU_GET(curpcb)->pcb_onfault == NULL)
			break;
		/* Fallthrough. */
	case T_IMMU_MISS:
	case T_DMMU_MISS:
		/*
		 * First try the tsb.  The primary tsb was already searched.
		 */
		vm = p->p_vmspace;
		if (tsb_miss(&vm->vm_pmap, tf->tf_type, mf) == 0) {
			rv = KERN_SUCCESS;
			break;
		}

		/*
		 * Not found, call the vm system.
		 */

		if (tf->tf_type == T_IMMU_MISS)
			type = VM_PROT_EXECUTE | VM_PROT_READ;
		else
			type = VM_PROT_READ;

		/*
		 * Keep the process from being swapped out at this critical
		 * time.
		 */
		PROC_LOCK(p);
		++p->p_lock;
		PROC_UNLOCK(p);

		/*
		 * Grow the stack if necessary.  vm_map_growstack only fails
		 * if the va falls into a growable stack region and the stack
		 * growth fails.  If it succeeds, or the va was not within a
		 * growable stack region, fault in the user page.
		 */
		if (vm_map_growstack(p, va) != KERN_SUCCESS)
			rv = KERN_FAILURE;
		else
			rv = vm_fault(&vm->vm_map, va, type, VM_FAULT_NORMAL);

		/*
		 * Now the process can be swapped again.
		 */
		PROC_LOCK(p);
		--p->p_lock;
		PROC_UNLOCK(p);
		break;
	case T_DMMU_PROT | T_KERNEL:
		/*
		 * Protection faults should not happen on kernel memory.
		 */
		if (TLB_TAR_CTX(mf->mf_tar) == TLB_CTX_KERNEL)
			break;

		/*
		 * Don't allow kernel mode faults on user memory unless
		 * pcb_onfault is set.
		 */
		if (PCPU_GET(curpcb)->pcb_onfault == NULL)
			break;
		/* Fallthrough. */
	case T_DMMU_PROT:
		/*
		 * Only look in the tsb.  Write access to an unmapped page
		 * causes a miss first, so the page must have already been
		 * brought in by vm_fault, we just need to find the tte and
		 * update the write bit.  XXX How do we tell them vm system
		 * that we are now writing?
		 */
		vm = p->p_vmspace;
		if (tsb_miss(&vm->vm_pmap, tf->tf_type, mf) == 0)
			rv = KERN_SUCCESS;
		break;
	default:
		break;
	}
	if (rv == KERN_SUCCESS)
		return (0);
	if (tf->tf_type & T_KERNEL) {
		if (PCPU_GET(curpcb)->pcb_onfault != NULL &&
		    TLB_TAR_CTX(mf->mf_tar) != TLB_CTX_KERNEL) {
			tf->tf_tpc = (u_long)PCPU_GET(curpcb)->pcb_onfault;
			tf->tf_tnpc = tf->tf_tpc + 4;
			return (0);
		}
	}
	return (rv == KERN_PROTECTION_FAILURE ? SIGBUS : SIGSEGV);
}
