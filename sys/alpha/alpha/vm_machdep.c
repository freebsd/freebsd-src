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
#include <sys/mutex.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/fpu.h>
#include <machine/md_var.h>
#include <machine/prom.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <sys/user.h>

/*
 * quick version of vm_fault
 */
int
vm_fault_quick(v, prot)
	caddr_t v;
	int prot;
{
	int r;
	if (prot & VM_PROT_WRITE)
		r = subyte(v, fubyte(v));
	else
		r = fubyte(v);
	return(r);
}

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb, set up the stack so that the child
 * ready to run and return to user mode.
 */
void
cpu_fork(td1, p2, td2, flags)
	register struct thread *td1;
	register struct proc *p2;
	register struct thread *td2;
	int flags;
{
	struct proc *p1;
	struct trapframe *p2tf;

	if ((flags & RFPROC) == 0)
		return;

	p1 = td1->td_proc;
	td2->td_pcb = (struct pcb *)
	    (td2->td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;
	td2->td_md.md_flags = td1->td_md.md_flags & MDTD_FPUSED;
	p2->p_md.md_uac = p1->p_md.md_uac;

	/*
	 * Cache the physical address of the pcb, so we can
	 * swap to it easily.
	 */
	td2->td_md.md_pcbpaddr = (void*)vtophys((vm_offset_t)td2->td_pcb);

	/*
	 * Copy floating point state from the FP chip to the PCB
	 * if this process has state stored there.
	 */
	alpha_fpstate_save(td1, 0);

	/*
	 * Copy pcb and stack from proc p1 to p2.  We do this as
	 * cheaply as possible, copying only the active part of the
	 * stack.  The stack and pcb need to agree. Make sure that the 
	 * new process has FEN disabled.
	 */
	bcopy(td1->td_pcb, td2->td_pcb, sizeof(struct pcb));
	td2->td_pcb->pcb_hw.apcb_usp = alpha_pal_rdusp();
	td2->td_pcb->pcb_hw.apcb_flags &= ~ALPHA_PCB_FLAGS_FEN;

	/*
	 * Set the floating point state.
	 */
	if ((td2->td_pcb->pcb_fp_control & IEEE_INHERIT) == 0) {
		td2->td_pcb->pcb_fp_control = 0;
		td2->td_pcb->pcb_fp.fpr_cr = (FPCR_DYN_NORMAL
						   | FPCR_INVD | FPCR_DZED
						   | FPCR_OVFD | FPCR_INED
						   | FPCR_UNFD);
	}

	/*
	 * Arrange for a non-local goto when the new process
	 * is started, to resume here, returning nonzero from setjmp.
	 */
#ifdef DIAGNOSTIC
	alpha_fpstate_check(td1);
#endif

	/*
	 * Create the child's kernel stack, from scratch.
	 *
	 * Pick a stack pointer, leaving room for a trapframe;
	 * copy trapframe from parent so return to user mode
	 * will be to right address, with correct registers.
	 */
	td2->td_frame = (struct trapframe *)td2->td_pcb - 1;
	bcopy(td1->td_frame, td2->td_frame, sizeof(struct trapframe));

	/*
	 * Set up return-value registers as fork() libc stub expects.
	 */
	p2tf = td2->td_frame;
	p2tf->tf_regs[FRAME_V0] = 0;	/* child's pid (linux)	*/
	p2tf->tf_regs[FRAME_A3] = 0;	/* no error		*/
	p2tf->tf_regs[FRAME_A4] = 1;	/* is child (FreeBSD)	*/

	/*
	 * Arrange for continuation at fork_return(), which
	 * will return to exception_return().  Note that the child
	 * process doesn't stay in the kernel for long!
	 */
	td2->td_pcb->pcb_hw.apcb_ksp = (u_int64_t)p2tf;
	td2->td_pcb->pcb_context[0] = (u_int64_t)fork_return;	  /* s0: a0 */
	td2->td_pcb->pcb_context[1] = (u_int64_t)exception_return;/* s1: ra */
	td2->td_pcb->pcb_context[2] = (u_long)td2;		  /* s2: a1 */
	td2->td_pcb->pcb_context[7] = (u_int64_t)fork_trampoline; /* ra: magic*/
#ifdef SMP
	/*
	 * We start off at a nesting level of 1 within the kernel.
	 */
	td2->td_md.md_kernnest = 1;
#endif
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
	/*
	 * Note that the trap frame follows the args, so the function
	 * is really called like this:  func(arg, frame);
	 */
	td->td_pcb->pcb_context[0] = (u_long) func;
	td->td_pcb->pcb_context[2] = (u_long) arg;
}

/*
 * cpu_exit is called as the last action during exit.
 * We release the address space of the process, block interrupts,
 * and call switch_exit.  switch_exit switches to proc0's PCB and stack,
 * then jumps into the middle of cpu_switch, as if it were switching
 * from proc0.
 */
void
cpu_exit(struct thread *td)
{

	alpha_fpstate_drop(td);
}

void
cpu_sched_exit(td)
	register struct thread *td;
{
}

void
cpu_thread_exit(struct thread *td)
{

	return;
}

void
cpu_thread_dtor(struct thread *td)
{
}

void
cpu_thread_setup(struct thread *td)
{

	td->td_pcb =
	     (struct pcb *)(td->td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;
	td->td_frame = (struct trapframe *)((caddr_t)td->td_pcb) - 1;
}

void
cpu_set_upcall(struct thread *td, void *pcb)
{
	struct pcb *pcb2;

	td->td_flags |= TDF_UPCALLING;

	/* Point the pcb to the top of the stack. */
	pcb2 = td->td_pcb;

	/*
	 * Copy the upcall pcb.  This loads kernel regs.
	 * Those not loaded individually below get their default
	 * values here.
	 *
	 * XXXKSE It might be a good idea to simply skip this as
	 * the values of the other registers may be unimportant.
	 * This would remove any requirement for knowing the KSE
	 * at this time (see the matching comment below for
	 * more analysis) (need a good safe default).
	 */
	bcopy(pcb, pcb2, sizeof(*pcb2));

	/*
	 * Create a new fresh stack for the new thread.
	 * Don't forget to set this stack value into whatever supplies
	 * the address for the fault handlers.
	 * The contexts are filled in at the time we actually DO the
	 * upcall as only then do we know which KSE we got.
	 */
	td->td_frame = (struct trapframe *)((caddr_t)pcb2) - 1;

	/*
	 * Arrange for continuation at fork_return(), which
	 * will return to exception_return().  Note that the child
	 * process doesn't stay in the kernel for long!
	 */
	pcb2->pcb_hw.apcb_ksp = (u_int64_t)td->td_frame;
	pcb2->pcb_context[0] = (u_int64_t)fork_return;	 	/* s0: a0 */
	pcb2->pcb_context[1] = (u_int64_t)exception_return;	/* s1: ra */
	pcb2->pcb_context[2] = (u_long)td;			/* s2: a1 */
	pcb2->pcb_context[7] = (u_int64_t)fork_trampoline;	/* ra: magic*/
#ifdef SMP
	/*
	 * We start off at a nesting level of 1 within the kernel.
	 */
	td->td_md.md_kernnest = 1;
#endif
}

void
cpu_set_upcall_kse(struct thread *td, struct kse *ke)
{

	/* XXX */
}

void
cpu_wait(p)
	struct proc *p;
{
}

/*
 * Map an IO request into kernel virtual address space.
 *
 * All requests are (re)mapped into kernel VA space.
 * Notice that we use b_bufsize for the size of the buffer
 * to be mapped.  b_bcount might be modified by the driver.
 */
void
vmapbuf(bp)
	register struct buf *bp;
{
	register caddr_t addr, v, kva;
	vm_offset_t pa;

	GIANT_REQUIRED;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");

	for (v = bp->b_saveaddr, addr = (caddr_t)trunc_page(bp->b_data);
	    addr < bp->b_data + bp->b_bufsize;
	    addr += PAGE_SIZE, v += PAGE_SIZE) {
		/*
		 * Do the vm_fault if needed; do the copy-on-write thing
		 * when reading stuff off device into memory.
		 */
		vm_fault_quick((addr >= bp->b_data) ? addr : bp->b_data,
			(bp->b_iocmd == BIO_READ)?(VM_PROT_READ|VM_PROT_WRITE):VM_PROT_READ);
		pa = trunc_page(pmap_kextract((vm_offset_t) addr));
		if (pa == 0)
			panic("vmapbuf: page not present");
		vm_page_hold(PHYS_TO_VM_PAGE(pa));
		pmap_kenter((vm_offset_t) v, pa);
	}

	kva = bp->b_saveaddr;
	bp->b_saveaddr = bp->b_data;
	bp->b_data = kva + (((vm_offset_t) bp->b_data) & PAGE_MASK);
}

/*
 * Free the io map PTEs associated with this IO operation.
 * We also invalidate the TLB entries and restore the original b_addr.
 */
void
vunmapbuf(bp)
	register struct buf *bp;
{
	register caddr_t addr;
	vm_offset_t pa;

	GIANT_REQUIRED;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");

	vm_page_lock_queues();
	for (addr = (caddr_t)trunc_page(bp->b_data);
	    addr < bp->b_data + bp->b_bufsize;
	    addr += PAGE_SIZE) {
		pa = trunc_page(pmap_kextract((vm_offset_t) addr));
		pmap_kremove((vm_offset_t) addr);
		vm_page_unhold(PHYS_TO_VM_PAGE(pa));
	}
	vm_page_unlock_queues();

	bp->b_data = bp->b_saveaddr;
}

/*
 * Reset back to firmware.
 */
void
cpu_reset()
{
	prom_halt(0);
}

/*
 * Software interrupt handler for queued VM system processing.
 */   
void  
swi_vm(void *dummy) 
{     
	if (busdma_swi_pending != 0)
		busdma_swi();
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
