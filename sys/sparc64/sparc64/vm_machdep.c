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

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/tstate.h>

/* XXX: it seems that all that is in here should really be MI... */
void
cpu_exit(struct proc *p)
{

	PROC_LOCK(p);
	mtx_lock_spin(&sched_lock);
	while (mtx_owned(&Giant))
		mtx_unlock_flags(&Giant, MTX_NOSWITCH);

	/*
	 * We have to wait until after releasing all locks before
	 * changing p_stat.  If we block on a mutex then we will be
	 * back at SRUN when we resume and our parent will never
	 * harvest us.
	 */
	p->p_stat = SZOMB;

	wakeup(p->p_pptr);
	PROC_UNLOCK_NOSWITCH(p);

	cnt.v_swtch++;
	cpu_throw();
	panic("cpu_exit");
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
	p1->p_addr->u_pcb.pcb_y = rd(y);
	p1->p_addr->u_pcb.pcb_fpstate.fp_fprs = rd(fprs);
	if ((p1->p_frame->tf_tstate & TSTATE_PEF) != 0) {
		mtx_lock_spin(&sched_lock);
		savefpctx(&p1->p_addr->u_pcb.pcb_fpstate);
		mtx_unlock_spin(&sched_lock);
	}
	/* Make sure the copied windows are spilled. */
	__asm __volatile("flushw");
	/* Copy the pcb (this will copy the windows saved in the pcb, too). */
	bcopy(&p1->p_addr->u_pcb, pcb, sizeof(*pcb));

	tf = (struct trapframe *)((caddr_t)p2->p_addr + UPAGES * PAGE_SIZE) - 1;
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
	GIANT_REQUIRED;

	/* drop per-process resources */
	pmap_dispose_proc(p);

	/* and clean-out the vmspace */
	vmspace_free(p->p_vmspace);
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
