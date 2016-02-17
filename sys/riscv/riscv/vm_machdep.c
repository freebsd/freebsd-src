/*-
 * Copyright (c) 2015 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/limits.h>
#include <sys/proc.h>
#include <sys/sf_buf.h>
#include <sys/signal.h>
#include <sys/unistd.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/uma.h>
#include <vm/uma_int.h>

#include <machine/riscvreg.h>
#include <machine/cpu.h>
#include <machine/pcb.h>
#include <machine/frame.h>

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb, set up the stack so that the child
 * ready to run and return to user mode.
 */
void
cpu_fork(struct thread *td1, struct proc *p2, struct thread *td2, int flags)
{
	struct pcb *pcb2;
	struct trapframe *tf;

	if ((flags & RFPROC) == 0)
		return;

	pcb2 = (struct pcb *)(td2->td_kstack +
	    td2->td_kstack_pages * PAGE_SIZE) - 1;

	td2->td_pcb = pcb2;
	bcopy(td1->td_pcb, pcb2, sizeof(*pcb2));

	td2->td_pcb->pcb_l1addr =
	    vtophys(vmspace_pmap(td2->td_proc->p_vmspace)->pm_l1);

	tf = (struct trapframe *)STACKALIGN((struct trapframe *)pcb2 - 1);
	bcopy(td1->td_frame, tf, sizeof(*tf));

	/* Clear syscall error flag */
	tf->tf_t[0] = 0;

	/* Arguments for child */
	tf->tf_a[0] = 0;
	tf->tf_a[1] = 0;
	tf->tf_sstatus = SSTATUS_PIE;

	td2->td_frame = tf;

	/* Set the return value registers for fork() */
	td2->td_pcb->pcb_s[0] = (uintptr_t)fork_return;
	td2->td_pcb->pcb_s[1] = (uintptr_t)td2;
	td2->td_pcb->pcb_ra = (uintptr_t)fork_trampoline;
	td2->td_pcb->pcb_sp = (uintptr_t)td2->td_frame;

	/* Setup to release spin count in fork_exit(). */
	td2->td_md.md_spinlock_count = 1;
	td2->td_md.md_saved_sstatus_ie = 1;
}

void
cpu_reset(void)
{

	printf("cpu_reset");
	while(1)
		__asm volatile("wfi" ::: "memory");
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
cpu_set_syscall_retval(struct thread *td, int error)
{
	struct trapframe *frame;

	frame = td->td_frame;

	switch (error) {
	case 0:
		frame->tf_a[0] = td->td_retval[0];
		frame->tf_a[1] = td->td_retval[1];
		frame->tf_t[0] = 0;		/* syscall succeeded */
		break;
	case ERESTART:
		frame->tf_sepc -= 4;		/* prev instruction */
		break;
	case EJUSTRETURN:
		break;
	default:
		frame->tf_a[0] = error;
		frame->tf_t[0] = 1;		/* syscall error */
		break;
	}
}

/*
 * Initialize machine state (pcb and trap frame) for a new thread about to
 * upcall. Put enough state in the new thread's PCB to get it to go back
 * userret(), where we can intercept it again to set the return (upcall)
 * Address and stack, along with those from upcals that are from other sources
 * such as those generated in thread_userret() itself.
 */
void
cpu_set_upcall(struct thread *td, struct thread *td0)
{

	bcopy(td0->td_frame, td->td_frame, sizeof(struct trapframe));
	bcopy(td0->td_pcb, td->td_pcb, sizeof(struct pcb));

	td->td_pcb->pcb_s[0] = (uintptr_t)fork_return;
	td->td_pcb->pcb_s[1] = (uintptr_t)td;
	td->td_pcb->pcb_ra = (uintptr_t)fork_trampoline;
	td->td_pcb->pcb_sp = (uintptr_t)td->td_frame;

	/* Setup to release spin count in fork_exit(). */
	td->td_md.md_spinlock_count = 1;
	td->td_md.md_saved_sstatus_ie = 1;
}

/*
 * Set that machine state for performing an upcall that has to
 * be done in thread_userret() so that those upcalls generated
 * in thread_userret() itself can be done as well.
 */
void
cpu_set_upcall_kse(struct thread *td, void (*entry)(void *), void *arg,
	stack_t *stack)
{
	struct trapframe *tf = td->td_frame;

	tf->tf_sp = STACKALIGN((uintptr_t)stack->ss_sp + stack->ss_size);
	tf->tf_sepc = (register_t)entry;
	tf->tf_a[0] = (register_t)arg;
}

int
cpu_set_user_tls(struct thread *td, void *tls_base)
{
	struct pcb *pcb;

	if ((uintptr_t)tls_base >= VM_MAXUSER_ADDRESS)
		return (EINVAL);

	pcb = td->td_pcb;
	pcb->pcb_tp = (register_t)tls_base;

	return (0);
}

void
cpu_thread_exit(struct thread *td)
{
}

void
cpu_thread_alloc(struct thread *td)
{

	td->td_pcb = (struct pcb *)(td->td_kstack +
	    td->td_kstack_pages * PAGE_SIZE) - 1;
	td->td_frame = (struct trapframe *)STACKALIGN(
	    (caddr_t)td->td_pcb - 8 - sizeof(struct trapframe));
}

void
cpu_thread_free(struct thread *td)
{
}

void
cpu_thread_clean(struct thread *td)
{
}

/*
 * Intercept the return address from a freshly forked process that has NOT
 * been scheduled yet.
 *
 * This is needed to make kernel threads stay in kernel mode.
 */
void
cpu_set_fork_handler(struct thread *td, void (*func)(void *), void *arg)
{

	td->td_pcb->pcb_s[0] = (uintptr_t)func;
	td->td_pcb->pcb_s[1] = (uintptr_t)arg;
	td->td_pcb->pcb_ra = (uintptr_t)fork_trampoline;
	td->td_pcb->pcb_sp = (uintptr_t)td->td_frame;
}

void
cpu_exit(struct thread *td)
{
}

void
swi_vm(void *v)
{

	/* Nothing to do here - busdma bounce buffers are not implemented. */
}
