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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/unistd.h>
#include <sys/user.h>

#include <dev/ofw/openfirm.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/md_var.h>

void
cpu_exit(struct proc *p)
{
	TODO;
}

void
cpu_fork(struct proc *p1, struct proc *p2, int flags)
{
	struct trapframe *tf;
	struct frame *fp;
	struct pcb *pcb;

	if ((flags & RFPROC) == 0)
		return;

	pcb = &p2->p_addr->u_pcb;
	if ((p1->p_frame->tf_tstate & TSTATE_PEF) != 0) {
		mtx_lock_spin(&sched_lock);
		savefpctx(&p1->p_addr->u_pcb.pcb_fpstate);
		mtx_unlock_spin(&sched_lock);
	}
	bcopy(&p1->p_addr->u_pcb, pcb, sizeof(*pcb));

	tf = (struct trapframe *)((caddr_t)pcb + UPAGES * PAGE_SIZE) - 1;
	bcopy(p1->p_frame, tf, sizeof(*tf));
	p2->p_frame = tf;

	fp = (struct frame *)tf - 1;
	fp->f_local[0] = (u_long)fork_return;
	fp->f_local[1] = (u_long)p2;
	fp->f_local[2] = (u_long)tf;
	pcb->pcb_fp = (u_long)fp - SPOFF;
	pcb->pcb_pc = (u_long)fork_trampoline - 8;
}

void
cpu_reset(void)
{
	OF_exit();
}

void
cpu_set_fork_handler(struct proc *p, void (*func)(void *), void *arg)
{
	struct frame *fp;
	struct pcb *pcb;

	pcb = &p->p_addr->u_pcb;
	fp = (struct frame *)(pcb->pcb_fp + SPOFF);
	fp->f_local[0] = (u_long)func;
	fp->f_local[1] = (u_long)arg;
}

void
cpu_wait(struct proc *p)
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
