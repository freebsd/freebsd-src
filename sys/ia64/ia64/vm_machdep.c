/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
 * Copyright (c) 1989, 1990 William Jolitz
 * Copyright (c) 1994 John Dyson
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department, and William Jolitz.
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
 *	from: @(#)vm_machdep.c	7.3 (Berkeley) 5/13/91
 *	Utah $Hdr: vm_machdep.c 1.16.1.1 89/06/23$
 * $FreeBSD$
 */
/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/fpu.h>
#include <machine/md_var.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <sys/user.h>

#include <i386/include/psl.h>

void
cpu_thread_exit(struct thread *td)
{
}

void
cpu_thread_clean(struct thread *td)
{
}

void
cpu_thread_setup(struct thread *td)
{
}

void
cpu_set_upcall(struct thread *td, void *pcb)
{
}

void
cpu_set_upcall_kse(struct thread *td, struct kse_upcall *ku)
{
}

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb, set up the stack so that the child
 * ready to run and return to user mode.
 */
void
cpu_fork(struct thread *td1, struct proc *p2 __unused, struct thread *td2,
    int flags)
{
	char *stackp;

	KASSERT(td1 == curthread || td1 == &thread0,
	    ("cpu_fork: td1 not curthread and not thread0"));

	if ((flags & RFPROC) == 0)
		return;

	/*
	 * Save the preserved registers and the high FP registers in the
	 * PCB if we're the parent (ie td1 == curthread) so that we have
	 * a valid PCB. This also causes a RSE flush. We don't have to
	 * do that otherwise, because there wouldn't be anything important
	 * to save.
	 */
	if (td1 == curthread) {
		if (savectx(td1->td_pcb) != 0)
			panic("unexpected return from savectx()");
		ia64_highfp_save(td1);
	}

	/*
	 * create the child's kernel stack and backing store. We basicly
	 * create an image of the parent's stack and backing store and
	 * adjust where necessary.
	 */
	stackp = (char *)(td2->td_kstack + KSTACK_PAGES * PAGE_SIZE);

	stackp -= sizeof(struct pcb);
	td2->td_pcb = (struct pcb *)stackp;
	bcopy(td1->td_pcb, td2->td_pcb, sizeof(struct pcb));

	stackp -= sizeof(struct trapframe);
	td2->td_frame = (struct trapframe *)stackp;
	bcopy(td1->td_frame, td2->td_frame, sizeof(struct trapframe));
	td2->td_frame->tf_length = sizeof(struct trapframe);

	bcopy((void*)td1->td_kstack, (void*)td2->td_kstack,
	    td2->td_frame->tf_special.ndirty);

	/* Set-up the return values as expected by the fork() libc stub. */
	if (td2->td_frame->tf_special.psr & IA64_PSR_IS) {
		td2->td_frame->tf_scratch.gr8 = 0;
		td2->td_frame->tf_scratch.gr10 = 1;
	} else {
		td2->td_frame->tf_scratch.gr8 = 0;
		td2->td_frame->tf_scratch.gr9 = 1;
		td2->td_frame->tf_scratch.gr10 = 0;
	}

	td2->td_pcb->pcb_special.bspstore = td2->td_kstack +
	    td2->td_frame->tf_special.ndirty;
	td2->td_pcb->pcb_special.pfs = 0;
	td2->td_pcb->pcb_current_pmap = vmspace_pmap(td2->td_proc->p_vmspace);

	td2->td_pcb->pcb_special.sp = (uintptr_t)stackp - 16;
	td2->td_pcb->pcb_special.rp = FDESC_FUNC(fork_trampoline);
	cpu_set_fork_handler(td2, (void (*)(void*))fork_return, td2);
}

/*
 * Intercept the return address from a freshly forked process that has NOT
 * been scheduled yet.
 *
 * This is needed to make kernel threads stay in kernel mode.
 */
void
cpu_set_fork_handler(td, func, arg)
	struct thread *td;
	void (*func)(void *);
	void *arg;
{
	td->td_frame->tf_scratch.gr2 = (u_int64_t)func;
	td->td_frame->tf_scratch.gr3 = (u_int64_t)arg;
}

/*
 * cpu_exit is called as the last action during exit.
 * We drop the fp state (if we have it) and switch to a live one.
 * When the proc is reaped, cpu_wait() will gc the VM state.
 */
void
cpu_exit(struct thread *td)
{

	/* Throw away the high FP registers. */
	ia64_highfp_drop(td);
}

void
cpu_sched_exit(td)
	register struct thread *td;
{
}

void
cpu_wait(p)
	struct proc *p;
{
}

/*
 * Force reset the processor by invalidating the entire address space!
 */
void
cpu_reset()
{

	cpu_boot(0);
}

/*
 * Software interrupt handler for queued VM system processing.
 */   
void  
swi_vm(void *dummy) 
{     
#if 0
	if (busdma_swi_pending != 0)
		busdma_swi();
#endif
}

/*
 * Tell whether this address is in some physical memory region.
 * Currently used by the kernel coredump code in order to avoid
 * dumping the ``ISA memory hole'' which could cause indefinite hangs,
 * or other unpredictable behaviour.
 */


int
is_physical_memory(addr)
	vm_offset_t addr;
{
	/*
	 * stuff other tests for known memory-mapped devices (PCI?)
	 * here
	 */

	return 1;
}
