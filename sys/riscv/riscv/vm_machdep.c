/*-
 * Copyright (c) 2015-2018 Ruslan Bukin <br@bsdpad.com>
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
#include <machine/cpufunc.h>
#include <machine/pcb.h>
#include <machine/frame.h>
#include <machine/sbi.h>

#if __riscv_xlen == 64
#define	TP_OFFSET	16	/* sizeof(struct tcb) */
#endif

static void
cpu_set_pcb_frame(struct thread *td)
{
	td->td_pcb = (struct pcb *)((char *)td->td_kstack +
	    td->td_kstack_pages * PAGE_SIZE) - 1;

	/*
	 * td->td_frame + TF_SIZE will be the saved kernel stack pointer whilst
	 * in userspace, so keep it aligned so it's also aligned when we
	 * subtract TF_SIZE in the trap handler (and here for the initial stack
	 * pointer). This also keeps the struct kernframe just afterwards
	 * aligned no matter what's in it or struct pcb.
	 *
	 * NB: TF_SIZE not sizeof(struct trapframe) as we need the rounded
	 * value to match the trap handler.
	 */
	td->td_frame = (struct trapframe *)(STACKALIGN(
	    (char *)td->td_pcb - sizeof(struct kernframe)) - TF_SIZE);
}

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

	/* RISCVTODO: save the FPU state here */

	cpu_set_pcb_frame(td2);

	pcb2 = td2->td_pcb;
	bcopy(td1->td_pcb, pcb2, sizeof(*pcb2));

	tf = td2->td_frame;
	bcopy(td1->td_frame, tf, sizeof(*tf));

	/* Clear syscall error flag */
	tf->tf_t[0] = 0;

	/* Arguments for child */
	tf->tf_a[0] = 0;
	tf->tf_a[1] = 0;
	tf->tf_sstatus |= (SSTATUS_SPIE); /* Enable interrupts. */
	tf->tf_sstatus &= ~(SSTATUS_SPP); /* User mode. */

	/* Set the return value registers for fork() */
	td2->td_pcb->pcb_s[0] = (uintptr_t)fork_return;
	td2->td_pcb->pcb_s[1] = (uintptr_t)td2;
	td2->td_pcb->pcb_ra = (uintptr_t)fork_trampoline;
	td2->td_pcb->pcb_sp = (uintptr_t)td2->td_frame;

	/* Setup to release spin count in fork_exit(). */
	td2->td_md.md_spinlock_count = 1;
	td2->td_md.md_saved_sstatus_ie = (SSTATUS_SIE);
}

void
cpu_reset(void)
{

	sbi_system_reset(SBI_SRST_TYPE_COLD_REBOOT, SBI_SRST_REASON_NONE);

	while(1);
}

void
cpu_set_syscall_retval(struct thread *td, int error)
{
	struct trapframe *frame;

	frame = td->td_frame;

	if (__predict_true(error == 0)) {
		frame->tf_a[0] = td->td_retval[0];
		frame->tf_a[1] = td->td_retval[1];
		frame->tf_t[0] = 0;		/* syscall succeeded */
		return;
	}

	switch (error) {
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
 * Initialize machine state, mostly pcb and trap frame for a new
 * thread, about to return to userspace.  Put enough state in the new
 * thread's PCB to get it to go back to the fork_return(), which
 * finalizes the thread state and handles peculiarities of the first
 * return to userspace for the new thread.
 */
void
cpu_copy_thread(struct thread *td, struct thread *td0)
{

	bcopy(td0->td_frame, td->td_frame, sizeof(struct trapframe));
	bcopy(td0->td_pcb, td->td_pcb, sizeof(struct pcb));

	td->td_pcb->pcb_s[0] = (uintptr_t)fork_return;
	td->td_pcb->pcb_s[1] = (uintptr_t)td;
	td->td_pcb->pcb_ra = (uintptr_t)fork_trampoline;
	td->td_pcb->pcb_sp = (uintptr_t)td->td_frame;

	/* Setup to release spin count in fork_exit(). */
	td->td_md.md_spinlock_count = 1;
	td->td_md.md_saved_sstatus_ie = (SSTATUS_SIE);
}

/*
 * Set that machine state for performing an upcall that starts
 * the entry function with the given argument.
 */
int
cpu_set_upcall(struct thread *td, void (*entry)(void *), void *arg,
	stack_t *stack)
{
	struct trapframe *tf;

	tf = td->td_frame;

	tf->tf_sp = STACKALIGN((uintptr_t)stack->ss_sp + stack->ss_size);
	tf->tf_sepc = (register_t)entry;
	tf->tf_a[0] = (register_t)arg;
	return (0);
}

int
cpu_set_user_tls(struct thread *td, void *tls_base)
{

	if ((uintptr_t)tls_base >= VM_MAXUSER_ADDRESS)
		return (EINVAL);

	/*
	 * The user TLS is set by modifying the trapframe's tp value, which
	 * will be restored when returning to userspace.
	 */
	td->td_frame->tf_tp = (register_t)tls_base + TP_OFFSET;

	return (0);
}

void
cpu_thread_exit(struct thread *td)
{
}

void
cpu_thread_alloc(struct thread *td)
{
	cpu_set_pcb_frame(td);
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
cpu_fork_kthread_handler(struct thread *td, void (*func)(void *), void *arg)
{

	td->td_pcb->pcb_s[0] = (uintptr_t)func;
	td->td_pcb->pcb_s[1] = (uintptr_t)arg;
	td->td_pcb->pcb_ra = (uintptr_t)fork_trampoline;
	td->td_pcb->pcb_sp = (uintptr_t)td->td_frame;
}

void
cpu_update_pcb(struct thread *td)
{
}

void
cpu_exit(struct thread *td)
{
}

bool
cpu_exec_vmspace_reuse(struct proc *p __unused, vm_map_t map __unused)
{

	return (true);
}

int
cpu_procctl(struct thread *td __unused, int idtype __unused, id_t id __unused,
    int com __unused, void *data __unused)
{

	return (EINVAL);
}

void
cpu_sync_core(void)
{
	fence_i();
}
