/*-
 * Copyright (c) 2014 Andrew Turner
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <machine/frame.h>

/* Called from exception.S */
void do_el1h_sync(struct trapframe *);

int
cpu_fetch_syscall_args(struct thread *td, struct syscall_args *sa)
{

	panic("cpu_fetch_syscall_args");
}

static void
data_abort(struct trapframe *frame, uint64_t esr, int lower)
{
	struct vm_map *map;
	struct thread *td;
	struct proc *p;
	vm_prot_t ftype;
	vm_offset_t va;
	uint64_t far;
	int error;

	__asm __volatile("mrs %x0, far_el1" : "=r"(far));

	td = curthread;
	p = td->td_proc;

	if (lower)
		map = &td->td_proc->p_vmspace->vm_map;
	else {
		/* The top bit tells us which range to use */
		if ((far >> 63) == 1)
			map = kernel_map;
		else
			map = &td->td_proc->p_vmspace->vm_map;
	}

	va = trunc_page(far);
	ftype = ((esr >> 6) & 1) ? VM_PROT_READ | VM_PROT_WRITE : VM_PROT_READ;

	if (map != kernel_map) {
		/*
		 * Keep swapout from messing with us during this
		 *	critical time.
		 */
		PROC_LOCK(p);
		++p->p_lock;
		PROC_UNLOCK(p);

		/* Fault in the user page: */
		error = vm_fault(map, va, ftype, VM_FAULT_NORMAL);

		PROC_LOCK(p);
		--p->p_lock;
		PROC_UNLOCK(p);
	} else {
		/*
		 * Don't have to worry about process locking or stacks in the
		 * kernel.
		 */
		error = vm_fault(map, va, ftype, VM_FAULT_NORMAL);
	}

	if (error != 0)
		panic("vm_fault failed");
}

void
do_el1h_sync(struct trapframe *frame)
{
	uint32_t exception;
	uint64_t esr;
	u_int reg;

	/* Read the esr register to get the exception details */
	__asm __volatile("mrs %x0, esr_el1" : "=&r"(esr));
	KASSERT((esr & (1 << 25)) != 0,
	    ("Invalid instruction length in exception"));

	exception = (esr >> 26) & 0x3f;

	printf("In do_el1h_sync %llx %llx %x\n", frame->tf_elr, esr, exception);

	for (reg = 0; reg < 31; reg++) {
		printf(" %sx%d: %llx\n", (reg < 10) ? " " : "", reg, frame->tf_x[reg]);
	}
	printf("  sp: %llx\n", frame->tf_sp);
	printf("  lr: %llx\n", frame->tf_lr);
	printf(" elr: %llx\n", frame->tf_elr);
	printf("spsr: %llx\n", frame->tf_spsr);
	switch(exception) {
	case 0x25:
		data_abort(frame, esr, 0);
		break;
	case 0x3c:
		printf("Breakpoint %x\n", (uint32_t)(esr & 0xffffff));
		panic("breakpoint");
		break;
	default:
		panic("Unknown exception %x\n", exception);
	}
	printf("Done do_el1h_sync\n");
}

