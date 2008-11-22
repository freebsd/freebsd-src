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
/*-
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
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/sf_buf.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/fpu.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/pcb.h>

#include <dev/ofw/openfirm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb, set up the stack so that the child
 * ready to run and return to user mode.
 */
void
cpu_fork(struct thread *td1, struct proc *p2, struct thread *td2, int flags)
{
	struct	proc *p1;
	struct	trapframe *tf;
	struct	callframe *cf;
	struct	pcb *pcb;

	KASSERT(td1 == curthread || td1 == &thread0,
	    ("cpu_fork: p1 not curproc and not proc0"));
	CTR3(KTR_PROC, "cpu_fork: called td1=%08x p2=%08x flags=%x", (u_int)td1, (u_int)p2, flags);

	if ((flags & RFPROC) == 0)
		return;

	p1 = td1->td_proc;

	pcb = (struct pcb *)((td2->td_kstack +
	    td2->td_kstack_pages * PAGE_SIZE - sizeof(struct pcb)) & ~0x2fU);
	td2->td_pcb = pcb;

	/* Copy the pcb */
	bcopy(td1->td_pcb, pcb, sizeof(struct pcb));

	/*
	 * Create a fresh stack for the new process.
	 * Copy the trap frame for the return to user mode as if from a
	 * syscall.  This copies most of the user mode register values.
	 */
	tf = (struct trapframe *)pcb - 1;
	bcopy(td1->td_frame, tf, sizeof(*tf));

	/* Set up trap frame. */
	tf->fixreg[FIRSTARG] = 0;
	tf->fixreg[FIRSTARG + 1] = 0;
	tf->cr &= ~0x10000000;

	td2->td_frame = tf;

	cf = (struct callframe *)tf - 1;
	memset(cf, 0, sizeof(struct callframe));
	cf->cf_func = (register_t)fork_return;
	cf->cf_arg0 = (register_t)td2;
	cf->cf_arg1 = (register_t)tf;

	pcb->pcb_sp = (register_t)cf;
	pcb->pcb_lr = (register_t)fork_trampoline;
	pcb->pcb_usr = kernel_pmap->pm_sr[USER_SR];

	/* Setup to release sched_lock in fork_exit(). */
	td2->td_md.md_spinlock_count = 1;
	td2->td_md.md_saved_msr = PSL_KERNSET;

	/*
 	 * Now cpu_switch() can schedule the new process.
	 */
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
	struct	callframe *cf;

	CTR3(KTR_PROC, "cpu_set_fork_handler: called with td=%08x func=%08x arg=%08x",
	    (u_int)td, (u_int)func, (u_int)arg);

	cf = (struct callframe *)td->td_pcb->pcb_sp;

	cf->cf_func = (register_t)func;
	cf->cf_arg0 = (register_t)arg;
}

void
cpu_exit(td)
	register struct thread *td;
{
}

/* Temporary helper */
void
cpu_throw(struct thread *old, struct thread *new)
{

	cpu_switch(old, new);
	panic("cpu_throw() didn't");
}

/*
 * Reset back to firmware.
 */
void
cpu_reset()
{
	OF_exit();
}

/*
 * Allocate an sf_buf for the given vm_page.  On this machine, however, there
 * is no sf_buf object.  Instead, an opaque pointer to the given vm_page is
 * returned.
 */
struct sf_buf *
sf_buf_alloc(struct vm_page *m, int pri)
{

	return ((struct sf_buf *)m);
}

/*
 * Free the sf_buf.  In fact, do nothing because there are no resources
 * associated with the sf_buf.
 */
void
sf_buf_free(struct sf_buf *sf)
{
}

/*
 * Software interrupt handler for queued VM system processing.
 */
void
swi_vm(void *dummy)
{
#if 0 /* XXX: Don't have busdma stuff yet */
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

/*
 * KSE functions
 */
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
	struct pcb *pcb;

	pcb = (struct pcb *)((td->td_kstack + td->td_kstack_pages * PAGE_SIZE -
	    sizeof(struct pcb)) & ~0x2fU);
	td->td_pcb = pcb;
	td->td_frame = (struct trapframe *)pcb - 1;
}

void
cpu_thread_swapin(struct thread *td)
{
}

void
cpu_thread_swapout(struct thread *td)
{
}

void
cpu_set_upcall(struct thread *td, struct thread *td0)
{
	struct pcb *pcb2;
	struct trapframe *tf;
	struct callframe *cf;

	pcb2 = td->td_pcb;

	/* Copy the upcall pcb */
	bcopy(td0->td_pcb, pcb2, sizeof(*pcb2));

	/* Create a stack for the new thread */
	tf = td->td_frame;
	bcopy(td0->td_frame, tf, sizeof(struct trapframe));
	tf->fixreg[FIRSTARG] = 0;
	tf->fixreg[FIRSTARG + 1] = 0;
	tf->cr &= ~0x10000000;

	/* Set registers for trampoline to user mode. */
	cf = (struct callframe *)tf - 1;
	memset(cf, 0, sizeof(struct callframe));
	cf->cf_func = (register_t)fork_return;
	cf->cf_arg0 = (register_t)td;
	cf->cf_arg1 = (register_t)tf;

	pcb2->pcb_sp = (register_t)cf;
	pcb2->pcb_lr = (register_t)fork_trampoline;
	pcb2->pcb_usr = kernel_pmap->pm_sr[USER_SR];

	/* Setup to release sched_lock in fork_exit(). */
	td->td_md.md_spinlock_count = 1;
	td->td_md.md_saved_msr = PSL_KERNSET;
}

void
cpu_set_upcall_kse(struct thread *td, void (*entry)(void *), void *arg,
	stack_t *stack)
{
        struct trapframe *tf;
        uint32_t sp;

	tf = td->td_frame;
	/* align stack and alloc space for frame ptr and saved LR */
        sp = ((uint32_t)stack->ss_sp + stack->ss_size
		- 2*sizeof(u_int32_t)) & ~0x1f;
	bzero(tf, sizeof(struct trapframe));

	tf->fixreg[1] = (register_t)sp;
        tf->fixreg[3] = (register_t)arg;
        tf->srr0 = (register_t)entry;
        tf->srr1 = PSL_MBO | PSL_USERSET | PSL_FE_DFLT;
        td->td_pcb->pcb_flags = 0;

        td->td_retval[0] = (register_t)entry;
        td->td_retval[1] = 0;
}

int
cpu_set_user_tls(struct thread *td, void *tls_base)
{

	td->td_frame->fixreg[2] = (register_t)tls_base + 0x7008;
	return (0);
}
