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
 *	$Id: vm_machdep.c,v 1.2 1998/06/10 19:59:41 dfr Exp $
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
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/prom.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <sys/lock.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <sys/user.h>

/*
 * quick version of vm_fault
 */
void
vm_fault_quick(v, prot)
	caddr_t v;
	int prot;
{
	if (prot & VM_PROT_WRITE)
		subyte(v, fubyte(v));
	else
		fubyte(v);
}

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb, set up the stack so that the child
 * ready to run and return to user mode.
 */
void
cpu_fork(p1, p2)
	register struct proc *p1, *p2;
{
	struct user *up = p2->p_addr;
	int i;

	p2->p_md.md_tf = p1->p_md.md_tf;
	p2->p_md.md_flags = p1->p_md.md_flags & MDP_FPUSED;

	/*
	 * Cache the physical address of the pcb, so we can
	 * swap to it easily.
	 */
	p2->p_md.md_pcbpaddr = (void*) vtophys((vm_offset_t) &up->u_pcb);

	/*
	 * Copy floating point state from the FP chip to the PCB
	 * if this process has state stored there.
	 */
	if (p1 == fpcurproc) {
		alpha_pal_wrfen(1);
		savefpstate(&fpcurproc->p_addr->u_pcb.pcb_fp);
		alpha_pal_wrfen(0);
	}

	/*
	 * Copy pcb and stack from proc p1 to p2.
	 * We do this as cheaply as possible, copying only the active
	 * part of the stack.  The stack and pcb need to agree;
	 */
	p2->p_addr->u_pcb = p1->p_addr->u_pcb;
	p2->p_addr->u_pcb.pcb_hw.apcb_usp = alpha_pal_rdusp();

	/*
	 * Arrange for a non-local goto when the new process
	 * is started, to resume here, returning nonzero from setjmp.
	 */
#ifdef DIAGNOSTIC
	if (p1 != curproc)
		panic("cpu_fork: curproc");
	if ((up->u_pcb.pcb_hw.apcb_flags & ALPHA_PCB_FLAGS_FEN) != 0)
		printf("DANGER WILL ROBINSON: FEN SET IN cpu_fork!\n");
#endif

	/*
	 * create the child's kernel stack, from scratch.
	 */
	{
		struct trapframe *p2tf;

		/*
		 * Pick a stack pointer, leaving room for a trapframe;
		 * copy trapframe from parent so return to user mode
		 * will be to right address, with correct registers.
		 */
		p2tf = p2->p_md.md_tf = (struct trapframe *)
		    ((char *)p2->p_addr + USPACE - sizeof(struct trapframe));
		bcopy(p1->p_md.md_tf, p2->p_md.md_tf,
		    sizeof(struct trapframe));

		/*
		 * Set up return-value registers as fork() libc stub expects.
		 */
		p2tf->tf_regs[FRAME_V0] = p1->p_pid;	/* parent's pid */
		p2tf->tf_regs[FRAME_A3] = 0;		/* no error */
		p2tf->tf_regs[FRAME_A4] = 1;		/* is child */

		/*
		 * Arrange for continuation at child_return(), which
		 * will return to exception_return().  Note that the child
		 * process doesn't stay in the kernel for long!
		 * 
		 * This is an inlined version of cpu_set_kpc.
		 */
		up->u_pcb.pcb_hw.apcb_ksp = (u_int64_t)p2tf;	
		up->u_pcb.pcb_context[0] =
		    (u_int64_t)child_return;		/* s0: pc */
		up->u_pcb.pcb_context[1] =
		    (u_int64_t)exception_return;	/* s1: ra */
		up->u_pcb.pcb_context[2] = (u_long) p2;	/* s2: a0 */
		up->u_pcb.pcb_context[7] =
		    (u_int64_t)switch_trampoline;	/* ra: assembly magic */
	}
}

/*
 * Intercept the return address from a freshly forked process that has NOT
 * been scheduled yet.
 *
 * This is needed to make kernel threads stay in kernel mode.
 */
void
cpu_set_fork_handler(p, func, arg)
	struct proc *p;
	void (*func) __P((void *));
	void *arg;
{
	/*
	 * Note that the trap frame follows the args, so the function
	 * is really called like this:  func(arg, frame);
	 */
	p->p_addr->u_pcb.pcb_context[0] = (u_long) func;
	p->p_addr->u_pcb.pcb_context[2] = (u_long) arg;
}

/*
 * cpu_exit is called as the last action during exit.
 * We release the address space of the process, block interrupts,
 * and call switch_exit.  switch_exit switches to proc0's PCB and stack,
 * then jumps into the middle of cpu_switch, as if it were switching
 * from proc0.
 */
void
cpu_exit(p)
	register struct proc *p;
{
	if (p == fpcurproc)
		fpcurproc = NULL;

	(void) splhigh();
	cnt.v_swtch++;
	cpu_switch(p);
	panic("cpu_exit");
}

void
cpu_wait(p)
	struct proc *p;
{
	/* drop per-process resources */
	pmap_dispose_proc(p);

	/* and clean-out the vmspace */
	vmspace_free(p->p_vmspace);
}

/*
 * Dump the machine specific header information at the start of a core dump.
 */
int
cpu_coredump(p, vp, cred)
	struct proc *p;
	struct vnode *vp;
	struct ucred *cred;
{

	return (vn_rdwr(UIO_WRITE, vp, (caddr_t) p->p_addr, ctob(UPAGES),
	    (off_t)0, UIO_SYSSPACE, IO_NODELOCKED|IO_UNIT, cred, (int *)NULL,
	    p));
}

#ifdef notyet
static void
setredzone(pte, vaddr)
	u_short *pte;
	caddr_t vaddr;
{
/* eventually do this by setting up an expand-down stack segment
   for ss0: selector, allowing stack access down to top of u.
   this means though that protection violations need to be handled
   thru a double fault exception that must do an integral task
   switch to a known good context, within which a dump can be
   taken. a sensible scheme might be to save the initial context
   used by sched (that has physical memory mapped 1:1 at bottom)
   and take the dump while still in mapped mode */
}
#endif

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

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");

	for (v = bp->b_saveaddr, addr = (caddr_t)trunc_page(bp->b_data);
	    addr < bp->b_data + bp->b_bufsize;
	    addr += PAGE_SIZE, v += PAGE_SIZE) {
		/*
		 * Do the vm_fault if needed; do the copy-on-write thing
		 * when reading stuff off device into memory.
		 */
		vm_fault_quick(addr,
			(bp->b_flags&B_READ)?(VM_PROT_READ|VM_PROT_WRITE):VM_PROT_READ);
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
	prom_halt(0);
}

/*
 * Grow the user stack to allow for 'sp'. This version grows the stack in
 *	chunks of SGROWSIZ.
 */
int
grow(p, sp)
	struct proc *p;
	size_t sp;
{
	unsigned int nss;
	caddr_t v;
	struct vmspace *vm = p->p_vmspace;

	if ((caddr_t)sp <= vm->vm_maxsaddr || sp >= (size_t) USRSTACK)
	    return (1);

	nss = roundup(USRSTACK - (unsigned)sp, PAGE_SIZE);

	if (nss > p->p_rlimit[RLIMIT_STACK].rlim_cur)
		return (0);

	if (vm->vm_ssize && roundup(vm->vm_ssize << PAGE_SHIFT,
	    SGROWSIZ) < nss) {
		int grow_amount;
		/*
		 * If necessary, grow the VM that the stack occupies
		 * to allow for the rlimit. This allows us to not have
		 * to allocate all of the VM up-front in execve (which
		 * is expensive).
		 * Grow the VM by the amount requested rounded up to
		 * the nearest SGROWSIZ to provide for some hysteresis.
		 */
		grow_amount = roundup((nss - (vm->vm_ssize << PAGE_SHIFT)), SGROWSIZ);
		v = (char *)USRSTACK - roundup(vm->vm_ssize << PAGE_SHIFT,
		    SGROWSIZ) - grow_amount;
		/*
		 * If there isn't enough room to extend by SGROWSIZ, then
		 * just extend to the maximum size
		 */
		if (v < vm->vm_maxsaddr) {
			v = vm->vm_maxsaddr;
			grow_amount = MAXSSIZ - (vm->vm_ssize << PAGE_SHIFT);
		}
		if ((grow_amount == 0) || (vm_map_find(&vm->vm_map, NULL, 0, (vm_offset_t *)&v,
		    grow_amount, FALSE, VM_PROT_ALL, VM_PROT_ALL, 0) != KERN_SUCCESS)) {
			return (0);
		}
		vm->vm_ssize += grow_amount >> PAGE_SHIFT;
	}

	return (1);
}

static int cnt_prezero;

SYSCTL_INT(_machdep, OID_AUTO, cnt_prezero, CTLFLAG_RD, &cnt_prezero, 0, "");

/*
 * Implement the pre-zeroed page mechanism.
 * This routine is called from the idle loop.
 */
int
vm_page_zero_idle()
{
	static int free_rover;
	vm_page_t m;
	int s;

	/*
	 * XXX
	 * We stop zeroing pages when there are sufficent prezeroed pages.
	 * This threshold isn't really needed, except we want to
	 * bypass unneeded calls to vm_page_list_find, and the
	 * associated cache flush and latency.  The pre-zero will
	 * still be called when there are significantly more
	 * non-prezeroed pages than zeroed pages.  The threshold
	 * of half the number of reserved pages is arbitrary, but
	 * approximately the right amount.  Eventually, we should
	 * perhaps interrupt the zero operation when a process
	 * is found to be ready to run.
	 */
	if (cnt.v_free_count - vm_page_zero_count <= cnt.v_free_reserved / 2)
		return (0);
#ifdef SMP
	if (try_mplock()) {
#endif
		s = splvm();
		m = vm_page_list_find(PQ_FREE, free_rover);
		if (m != NULL) {
			--(*vm_page_queues[m->queue].lcnt);
			TAILQ_REMOVE(vm_page_queues[m->queue].pl, m, pageq);
			m->queue = PQ_NONE;
			splx(s);
#if 0
			rel_mplock();
#endif
			pmap_zero_page(VM_PAGE_TO_PHYS(m));
#if 0
			get_mplock();
#endif
			(void)splvm();
			m->queue = PQ_ZERO + m->pc;
			++(*vm_page_queues[m->queue].lcnt);
			TAILQ_INSERT_HEAD(vm_page_queues[m->queue].pl, m,
			    pageq);
			free_rover = (free_rover + PQ_PRIME3) & PQ_L2_MASK;
			++vm_page_zero_count;
			++cnt_prezero;
		}
		splx(s);
#ifdef SMP
		rel_mplock();
#endif
		return (1);
#ifdef SMP
	}
#endif
	return (0);
}

/*
 * Software interrupt handler for queued VM system processing.
 */   
void  
swi_vm() 
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
