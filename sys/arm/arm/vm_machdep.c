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
 * Redistribution and use in source and binary :forms, with or without
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#include <sys/socketvar.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/unistd.h>
#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/sysarch.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>
#include <vm/vm_pageout.h>
#include <vm/uma.h>
#include <vm/uma_int.h>

#include <machine/md_var.h>
#include <machine/vfp.h>

/*
 * struct switchframe and trapframe must both be a multiple of 8
 * for correct stack alignment.
 */
CTASSERT(sizeof(struct switchframe) == 24);
CTASSERT(sizeof(struct trapframe) == 80);

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb, set up the stack so that the child
 * ready to run and return to user mode.
 */
void
cpu_fork(register struct thread *td1, register struct proc *p2,
    struct thread *td2, int flags)
{
	struct pcb *pcb2;
	struct trapframe *tf;
	struct switchframe *sf;
	struct mdproc *mdp2;

	if ((flags & RFPROC) == 0)
		return;
	pcb2 = (struct pcb *)(td2->td_kstack + td2->td_kstack_pages * PAGE_SIZE) - 1;
#ifdef __XSCALE__
#ifndef CPU_XSCALE_CORE3
	pmap_use_minicache(td2->td_kstack, td2->td_kstack_pages * PAGE_SIZE);
#endif
#endif
	td2->td_pcb = pcb2;
	bcopy(td1->td_pcb, pcb2, sizeof(*pcb2));
	mdp2 = &p2->p_md;
	bcopy(&td1->td_proc->p_md, mdp2, sizeof(*mdp2));
	pcb2->un_32.pcb32_sp = td2->td_kstack +
	    USPACE_SVC_STACK_TOP - sizeof(*pcb2);
	pcb2->pcb_vfpcpu = -1;
	pcb2->pcb_vfpstate.fpscr = VFPSCR_DN | VFPSCR_FZ;
	pmap_activate(td2);
	td2->td_frame = tf = (struct trapframe *)STACKALIGN(
	    pcb2->un_32.pcb32_sp - sizeof(struct trapframe));
	*tf = *td1->td_frame;
	sf = (struct switchframe *)tf - 1;
	sf->sf_r4 = (u_int)fork_return;
	sf->sf_r5 = (u_int)td2;
	sf->sf_pc = (u_int)fork_trampoline;
	tf->tf_spsr &= ~PSR_C_bit;
	tf->tf_r0 = 0;
	tf->tf_r1 = 0;
	pcb2->un_32.pcb32_sp = (u_int)sf;
	KASSERT((pcb2->un_32.pcb32_sp & 7) == 0,
	    ("cpu_fork: Incorrect stack alignment"));

	/* Setup to release spin count in fork_exit(). */
	td2->td_md.md_spinlock_count = 1;
	td2->td_md.md_saved_cspr = 0;
#ifdef ARM_TP_ADDRESS
	td2->td_md.md_tp = *(register_t *)ARM_TP_ADDRESS;
#else
	td2->td_md.md_tp = (register_t) get_tls();
#endif
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
	int fixup;
#ifdef __ARMEB__
	u_int call;
#endif

	frame = td->td_frame;
	fixup = 0;

#ifdef __ARMEB__
	/*
	 * __syscall returns an off_t while most other syscalls return an
	 * int. As an off_t is 64-bits and an int is 32-bits we need to
	 * place the returned data into r1. As the lseek and frerebsd6_lseek
	 * syscalls also return an off_t they do not need this fixup.
	 */
#ifdef __ARM_EABI__
	call = frame->tf_r7;
#else
	call = *(u_int32_t *)(frame->tf_pc - INSN_SIZE) & 0x000fffff;
#endif
	if (call == SYS___syscall) {
		register_t *ap = &frame->tf_r0;
		register_t code = ap[_QUAD_LOWWORD];
		if (td->td_proc->p_sysent->sv_mask)
			code &= td->td_proc->p_sysent->sv_mask;
		fixup = (code != SYS_freebsd6_lseek && code != SYS_lseek)
		    ? 1 : 0;
	}
#endif

	switch (error) {
	case 0:
		if (fixup) {
			frame->tf_r0 = 0;
			frame->tf_r1 = td->td_retval[0];
		} else {
			frame->tf_r0 = td->td_retval[0];
			frame->tf_r1 = td->td_retval[1];
		}
		frame->tf_spsr &= ~PSR_C_bit;   /* carry bit */
		break;
	case ERESTART:
		/*
		 * Reconstruct the pc to point at the swi.
		 */
		frame->tf_pc -= INSN_SIZE;
		break;
	case EJUSTRETURN:
		/* nothing to do */
		break;
	default:
		frame->tf_r0 = error;
		frame->tf_spsr |= PSR_C_bit;    /* carry bit */
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
	struct trapframe *tf;
	struct switchframe *sf;

	bcopy(td0->td_frame, td->td_frame, sizeof(struct trapframe));
	bcopy(td0->td_pcb, td->td_pcb, sizeof(struct pcb));
	tf = td->td_frame;
	sf = (struct switchframe *)tf - 1;
	sf->sf_r4 = (u_int)fork_return;
	sf->sf_r5 = (u_int)td;
	sf->sf_pc = (u_int)fork_trampoline;
	tf->tf_spsr &= ~PSR_C_bit;
	tf->tf_r0 = 0;
	td->td_pcb->un_32.pcb32_sp = (u_int)sf;
	KASSERT((td->td_pcb->un_32.pcb32_sp & 7) == 0,
	    ("cpu_set_upcall: Incorrect stack alignment"));

	/* Setup to release spin count in fork_exit(). */
	td->td_md.md_spinlock_count = 1;
	td->td_md.md_saved_cspr = 0;
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

	tf->tf_usr_sp = STACKALIGN((int)stack->ss_sp + stack->ss_size
	    - sizeof(struct trapframe));
	tf->tf_pc = (int)entry;
	tf->tf_r0 = (int)arg;
	tf->tf_spsr = PSR_USR32_MODE;
}

int
cpu_set_user_tls(struct thread *td, void *tls_base)
{

	td->td_md.md_tp = (register_t)tls_base;
	if (td == curthread) {
		critical_enter();
#ifdef ARM_TP_ADDRESS
		*(register_t *)ARM_TP_ADDRESS = (register_t)tls_base;
#else
		set_tls((void *)tls_base);
#endif
		critical_exit();
	}
	return (0);
}

void
cpu_thread_exit(struct thread *td)
{
}

void
cpu_thread_alloc(struct thread *td)
{
	td->td_pcb = (struct pcb *)(td->td_kstack + td->td_kstack_pages *
	    PAGE_SIZE) - 1;
	/*
	 * Ensure td_frame is aligned to an 8 byte boundary as it will be
	 * placed into the stack pointer which must be 8 byte aligned in
	 * the ARM EABI.
	 */
	td->td_frame = (struct trapframe *)STACKALIGN((u_int)td->td_kstack +
	    USPACE_SVC_STACK_TOP - sizeof(struct pcb) -
	    sizeof(struct trapframe));
#ifdef __XSCALE__
#ifndef CPU_XSCALE_CORE3
	pmap_use_minicache(td->td_kstack, td->td_kstack_pages * PAGE_SIZE);
#endif
#endif
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
	struct switchframe *sf;
	struct trapframe *tf;
	
	tf = td->td_frame;
	sf = (struct switchframe *)tf - 1;
	sf->sf_r4 = (u_int)func;
	sf->sf_r5 = (u_int)arg;
	td->td_pcb->un_32.pcb32_sp = (u_int)sf;
	KASSERT((td->td_pcb->un_32.pcb32_sp & 7) == 0,
	    ("cpu_set_fork_handler: Incorrect stack alignment"));
}

/*
 * Software interrupt handler for queued VM system processing.
 */
void
swi_vm(void *dummy)
{
	
	if (busdma_swi_pending)
		busdma_swi();
}

void
cpu_exit(struct thread *td)
{
}

