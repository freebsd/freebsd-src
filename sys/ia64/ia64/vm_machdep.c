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

#include "opt_kstack_pages.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socketvar.h>
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

static void	sf_buf_init(void *arg);
SYSINIT(sock_sf, SI_SUB_MBUF, SI_ORDER_ANY, sf_buf_init, NULL)

/*
 * Expanded sf_freelist head. Really an SLIST_HEAD() in disguise, with the
 * sf_freelist head with the sf_lock mutex.
 */
static struct {
	SLIST_HEAD(, sf_buf) sf_head;
	struct mtx sf_lock;
} sf_freelist;

static u_int	sf_buf_alloc_want;

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
	intptr_t sp;

	sp = td->td_kstack + KSTACK_PAGES * PAGE_SIZE;
	sp -= sizeof(struct pcb);
	td->td_pcb = (struct pcb *)sp;
	sp -= sizeof(struct trapframe);
	td->td_frame = (struct trapframe *)sp;
	td->td_frame->tf_length = sizeof(struct trapframe);
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
	struct pcb *pcb;
	struct trapframe *tf;

	tf = td->td_frame;
	KASSERT(tf != NULL, ("foo"));
	bcopy(td0->td_frame, tf, sizeof(*tf));
	tf->tf_length = sizeof(struct trapframe);
	tf->tf_flags = FRAME_SYSCALL;
	tf->tf_special.ndirty = 0;
	tf->tf_scratch.gr8 = 0;
	tf->tf_scratch.gr9 = 1;
	tf->tf_scratch.gr10 = 0;

	pcb = td->td_pcb;
	KASSERT(pcb != NULL, ("foo"));
	bcopy(td0->td_pcb, pcb, sizeof(*pcb));
	pcb->pcb_special.bspstore = td->td_kstack;
	pcb->pcb_special.pfs = 0;
	pcb->pcb_current_pmap = vmspace_pmap(td->td_proc->p_vmspace);
	pcb->pcb_special.sp = (uintptr_t)tf - 16;
	pcb->pcb_special.rp = FDESC_FUNC(fork_trampoline);
	cpu_set_fork_handler(td, (void (*)(void*))fork_return, td);
}

void
cpu_set_upcall_kse(struct thread *td, struct kse_upcall *ku)
{
	struct ia64_fdesc *fd;
	struct trapframe *tf;
	uint64_t ndirty, stack;

	tf = td->td_frame;

	KASSERT((tf->tf_special.ndirty & ~PAGE_MASK) == 0,
	    ("Whoa there! We have more than 8KB of dirty registers!"));

	fd = ku->ku_func;
	ndirty = tf->tf_special.ndirty;
	stack = (uint64_t)ku->ku_stack.ss_sp;

	bzero(&tf->tf_special, sizeof(tf->tf_special));
	tf->tf_special.gp = fuword(&fd->gp);
	tf->tf_special.sp = (stack + ku->ku_stack.ss_size - 16) & ~15;
	tf->tf_special.rsc = 0xf;
	tf->tf_special.fpsr = IA64_FPSR_DEFAULT;
	tf->tf_special.psr = IA64_PSR_IC | IA64_PSR_I | IA64_PSR_IT |
	    IA64_PSR_DT | IA64_PSR_RT | IA64_PSR_DFH | IA64_PSR_BN |
	    IA64_PSR_CPL_USER;

	if (tf->tf_flags & FRAME_SYSCALL) {
		tf->tf_special.rp = fuword(&fd->func);
		tf->tf_special.pfs = (3UL<<62) | (1UL<<7) | 1UL;
		tf->tf_special.bspstore = stack + 8;
		suword((caddr_t)stack, (uint64_t)ku->ku_mailbox);
	} else {
		tf->tf_special.iip = fuword(&fd->func);
		tf->tf_special.cfm = (1UL<<63) | (1UL<<7) | 1UL;
		tf->tf_special.bspstore = stack;
		tf->tf_special.ndirty = 8;
		stack = td->td_kstack + ndirty - 8;
		if ((stack & 0x1ff) == 0x1f8) {
			*(uint64_t*)stack = 0;
			tf->tf_special.ndirty += 8;
			stack -= 8;
		}
		*(uint64_t*)stack = (uint64_t)ku->ku_mailbox;
	}
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

/*
 * Allocate a pool of sf_bufs (sendfile(2) or "super-fast" if you prefer. :-))
 */
static void
sf_buf_init(void *arg)
{
	struct sf_buf *sf_bufs;
	int i;

	mtx_init(&sf_freelist.sf_lock, "sf_bufs list lock", NULL, MTX_DEF);
	mtx_lock(&sf_freelist.sf_lock);
	SLIST_INIT(&sf_freelist.sf_head);
	sf_bufs = malloc(nsfbufs * sizeof(struct sf_buf), M_TEMP,
	    M_NOWAIT | M_ZERO);
	for (i = 0; i < nsfbufs; i++)
		SLIST_INSERT_HEAD(&sf_freelist.sf_head, sf_bufs+i, free_list);
	sf_buf_alloc_want = 0;
	mtx_unlock(&sf_freelist.sf_lock);
}

/*
 * Get an sf_buf from the freelist. Will block if none are available.
 */
struct sf_buf *
sf_buf_alloc(struct vm_page *m)
{
	struct sf_buf *sf;
	int error;

	mtx_lock(&sf_freelist.sf_lock);
	while ((sf = SLIST_FIRST(&sf_freelist.sf_head)) == NULL) {
		sf_buf_alloc_want++;
		error = msleep(&sf_freelist, &sf_freelist.sf_lock, PVM|PCATCH,
		    "sfbufa", 0);
		sf_buf_alloc_want--;

		/* If we got a signal, don't risk going back to sleep. */
		if (error)
			break;
	}
	if (sf != NULL) {
		SLIST_REMOVE_HEAD(&sf_freelist.sf_head, free_list);
		sf->m = m;
		sf->kva = IA64_PHYS_TO_RR7(m->phys_addr);
	}
	mtx_unlock(&sf_freelist.sf_lock);
	return (sf);
}

/*
 * Detach mapped page and release resources back to the system.
 */
void
sf_buf_free(void *addr, void *args)
{
	struct sf_buf *sf;
	struct vm_page *m;

	sf = args;
	m = sf->m;
	vm_page_lock_queues();
	vm_page_unwire(m, 0);
	/*
	 * Check for the object going away on us. This can happen since we
	 * don't hold a reference to it. If so, we're responsible for freeing
	 * the page.
	 */
	if (m->wire_count == 0 && m->object == NULL)
		vm_page_free(m);
	vm_page_unlock_queues();
	sf->m = NULL;
	mtx_lock(&sf_freelist.sf_lock);
	SLIST_INSERT_HEAD(&sf_freelist.sf_head, sf, free_list);
	if (sf_buf_alloc_want > 0)
		wakeup_one(&sf_freelist);
	mtx_unlock(&sf_freelist.sf_lock);
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
