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

void
cpu_thread_exit(struct thread *td)
{
}

void
cpu_thread_setup(struct thread *td)
{
}

void
cpu_save_upcall(struct thread *td, struct kse *newkse)
{
}

void
cpu_set_upcall(struct thread *td, void *pcb)
{
}

void
cpu_set_args(struct thread *td, struct kse *ke)
{
}

void
cpu_free_kse_mdstorage(struct kse *ke)
{
}

int
cpu_export_context(struct thread *td)
{
       return (0);
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
	u_int64_t bspstore, *p1bs, *p2bs, rnatloc, rnat;

	KASSERT(td1 == curthread || td1 == &thread0,
	    ("cpu_fork: p1 not curproc and not proc0"));

	if ((flags & RFPROC) == 0)
		return;

	p1 = td1->td_proc;
	td2->td_pcb = (struct pcb *)
	    (td2->td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;
	td2->td_md.md_flags = td1->td_md.md_flags & (MDP_FPUSED | MDP_UAC_MASK);

	/*
	 * Copy floating point state from the FP chip to the PCB
	 * if this process has state stored there.
	 */
	ia64_fpstate_save(td1, 0);

	/*
	 * Copy pcb and stack from proc p1 to p2.  We do this as
	 * cheaply as possible, copying only the active part of the
	 * stack.  The stack and pcb need to agree. Make sure that the 
	 * new process has FEN disabled.
	 */
	bcopy(td1->td_pcb, td2->td_pcb, sizeof(struct pcb));

	/*
	 * Set the floating point state.
	 */
#if 0
	if ((td2->td_pcb->pcb_fp_control & IEEE_INHERIT) == 0) {
		td2->td_pcb->pcb_fp_control = 0;
		td2->td_pcb->pcb_fp.fpr_cr = (FPCR_DYN_NORMAL
						   | FPCR_INVD | FPCR_DZED
						   | FPCR_OVFD | FPCR_INED
						   | FPCR_UNFD);
	}
#endif

	/*
	 * Arrange for a non-local goto when the new process
	 * is started, to resume here, returning nonzero from setjmp.
	 */
#ifdef DIAGNOSTIC
	if (td1 == curthread)
		ia64_fpstate_check(td1);
#endif

	/*
	 * create the child's kernel stack, from scratch.
	 *
	 * Pick a stack pointer, leaving room for a trapframe;
	 * copy trapframe from parent so return to user mode
	 * will be to right address, with correct registers. Clear the
	 * high-fp enable for the new process so that it is forced to
	 * load its state from the pcb.
	 */
	td2->td_frame = (struct trapframe *)td2->td_pcb - 1;
	bcopy(td1->td_frame, td2->td_frame, sizeof(struct trapframe));
	td2->td_frame->tf_cr_ipsr |= IA64_PSR_DFH;

	/*
	 * Set up return-value registers as fork() libc stub expects.
	 */
	p2tf = td2->td_frame;
	if (p2tf->tf_cr_ipsr & IA64_PSR_IS) {
		p2tf->tf_r[FRAME_R8] = 0; /* child returns zero (eax) */
		p2tf->tf_r[FRAME_R10] = 1; /* is child (edx) */
		td2->td_pcb->pcb_eflag &= ~PSL_C; /* no error */
	} else {
		p2tf->tf_r[FRAME_R8] = 0; /* child's pid (linux) 	*/
		p2tf->tf_r[FRAME_R9] = 1; /* is child (FreeBSD) 	*/
		p2tf->tf_r[FRAME_R10] = 0; /* no error	 		*/
	}

	/*
	 * Turn off RSE for a moment and work out our current
	 * ar.bspstore. This assumes that td1==curthread. Also
	 * flush dirty regs to ensure that the user's stacked
	 * regs are written out to backing store.
	 *
	 * We could cope with td1!=curthread by digging values
	 * out of its PCB but I don't see the point since
	 * current usage only allows &thread0 when creating kernel
	 * threads and &thread0 doesn't have any dirty regs.
	 */

	p1bs = (u_int64_t *)td1->td_kstack;
	p2bs = (u_int64_t *)td2->td_kstack;

	if (td1 == curthread) {
		__asm __volatile("mov ar.rsc=0;;");
		__asm __volatile("flushrs;;" ::: "memory");
		__asm __volatile("mov %0=ar.bspstore" : "=r"(bspstore));
	} else {
		bspstore = (u_int64_t) p1bs;
	}

	/*
	 * Copy enough of td1's backing store to include all
	 * the user's stacked regs.
	 */
	bcopy(p1bs, p2bs, td1->td_frame->tf_ndirty);
	/*
	 * To calculate the ar.rnat for td2, we need to decide
	 * if td1's ar.bspstore has advanced past the place
	 * where the last ar.rnat which covers the user's
	 * saved registers would be placed. If so, we read
	 * that one from memory, otherwise we take td1's
	 * current ar.rnat. If we are simply spawning a new kthread
	 * from &thread0 we don't care about ar.rnat.
	 */
	if (td1 == curthread) {
		rnatloc = (u_int64_t)p1bs + td1->td_frame->tf_ndirty;
		rnatloc |= 0x1f8;
		if (bspstore > rnatloc)
			rnat = *(u_int64_t *) rnatloc;
		else
			__asm __volatile("mov %0=ar.rnat;;" : "=r"(rnat));

		/*
		 * Switch the RSE back on.
		 */
		__asm __volatile("mov ar.rsc=3;;");
	} else {
		rnat = 0;
	}
	
	/*
	 * Setup the child's pcb so that its ar.bspstore
	 * starts just above the region which we copied. This
	 * should work since the child will normally return
	 * straight into exception_restore. Also initialise its
	 * pmap to the containing proc's vmspace.
	 */
	td2->td_pcb->pcb_bspstore = (u_int64_t)p2bs + td1->td_frame->tf_ndirty;
	td2->td_pcb->pcb_rnat = rnat;
	td2->td_pcb->pcb_pfs = 0;
	td2->td_pcb->pcb_pmap = (u_int64_t)
		vmspace_pmap(td2->td_proc->p_vmspace);

	/*
	 * Arrange for continuation at fork_return(), which
	 * will return to exception_restore().  Note that the
	 * child process doesn't stay in the kernel for long!
	 *
	 * The extra 16 bytes subtracted from sp is part of the ia64
	 * ABI - a function can assume that the 16 bytes above sp are
	 * available as scratch space.
	 */
	td2->td_pcb->pcb_sp = (u_int64_t)p2tf - 16;	
	td2->td_pcb->pcb_r4 = (u_int64_t)fork_return;
	td2->td_pcb->pcb_r5 = FDESC_FUNC(exception_restore);
	td2->td_pcb->pcb_r6 = (u_int64_t)td2;
	td2->td_pcb->pcb_b0 = FDESC_FUNC(fork_trampoline);
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
	td->td_pcb->pcb_r4 = (u_int64_t) func;
	td->td_pcb->pcb_r6 = (u_int64_t) arg;
}

/*
 * cpu_exit is called as the last action during exit.
 * We drop the fp state (if we have it) and switch to a live one.
 * When the proc is reaped, cpu_wait() will gc the VM state.
 */
void
cpu_exit(td)
	register struct thread *td;
{

	ia64_fpstate_drop(td);
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

	for (addr = (caddr_t)trunc_page(bp->b_data);
	    addr < bp->b_data + bp->b_bufsize;
	    addr += PAGE_SIZE) {
		pa = trunc_page(pmap_kextract((vm_offset_t) addr));
		pmap_kremove((vm_offset_t) addr);
		vm_page_unhold(PHYS_TO_VM_PAGE(pa));
	}

	bp->b_data = bp->b_saveaddr;
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
