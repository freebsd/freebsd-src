/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
 * Copyright (c) 1989, 1990 William Jolitz
 * Copyright (c) 1994 John Dyson
 * Copyright (c) 2001 Jake Burkholder.
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
 * 	from: FreeBSD: src/sys/i386/i386/vm_machdep.c,v 1.167 2001/07/12
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/unistd.h>
#include <sys/user.h>
#include <sys/vmmeter.h>

#include <dev/ofw/openfirm.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/tstate.h>

void
cpu_exit(struct thread *td)
{
}

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb, set up the stack so that the child
 * ready to run and return to user mode.
 */
void
cpu_fork(struct thread *td1, struct proc *p2, int flags)
{
	struct thread *td2;
	struct trapframe *tf;
	struct frame *fp;
	struct pcb *pcb;

	KASSERT(td1 == curthread || td1 == thread0,
	    ("cpu_fork: p1 not curproc and not proc0"));

	if ((flags & RFPROC) == 0)
		return;

	td2 = &p2->p_thread;
	pcb = (struct pcb *)(td2->td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;
	td2->td_pcb = pcb;

	/*
	 * Ensure that p1's pcb is up to date.
	 */
	td1->td_pcb->pcb_y = rd(y);
	td1->td_pcb->pcb_fpstate.fp_fprs = rd(fprs);
	if ((td1->td_frame->tf_tstate & TSTATE_PEF) != 0) {
		mtx_lock_spin(&sched_lock);
		savefpctx(&td1->td_pcb->pcb_fpstate);
		mtx_unlock_spin(&sched_lock);
	}
	/* Make sure the copied windows are spilled. */
	__asm __volatile("flushw");
	/* Copy the pcb (this will copy the windows saved in the pcb, too). */
	bcopy(td1->td_pcb, pcb, sizeof(*pcb));
	pcb->pcb_cwp = 2;

	/*
	 * Create a new fresh stack for the new process.
	 * Copy the trap frame for the return to user mode as if from a
	 * syscall.  This copies most of the user mode register values.
	 */
	tf = (struct trapframe *)pcb - 1;
	bcopy(td1->td_frame, tf, sizeof(*tf));

	tf->tf_out[0] = 0;			/* Child returns zero */
	tf->tf_out[1] = 1;			/* XXX i386 returns 1 in %edx */
	tf->tf_tstate &= ~(TSTATE_XCC_C | TSTATE_CWP_MASK);	/* success */

	td2->td_frame = tf;
	fp = (struct frame *)tf - 1;
	fp->f_local[0] = (u_long)fork_return;
	fp->f_local[1] = (u_long)td2;
	fp->f_local[2] = (u_long)tf;
	pcb->pcb_cwp = 0;
	pcb->pcb_fp = (u_long)fp - SPOFF;
	pcb->pcb_pc = (u_long)fork_trampoline - 8;

	/*
	 * Now, cpu_switch() can schedule the new process.
	 */
}

void
cpu_reset(void)
{
	OF_exit();
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
	struct frame *fp;
	struct pcb *pcb;

	pcb = td->td_pcb;
	fp = (struct frame *)(pcb->pcb_fp + SPOFF);
	fp->f_local[0] = (u_long)func;
	fp->f_local[1] = (u_long)arg;
}

void
cpu_wait(struct proc *p)
{
}

int
is_physical_memory(vm_offset_t addr)
{

	TODO;
}

void
swi_vm(void *v)
{
	TODO;
}

int
vm_fault_quick(caddr_t v, int prot)
{
	TODO;
	return (0);
}

void
vmapbuf(struct buf *bp)
{
	TODO;
}

void
vunmapbuf(struct buf *bp)
{
	TODO;
}
