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

struct ia64_fdesc {
	u_int64_t	func;
	u_int64_t	gp;
};

#define FDESC_FUNC(fn)	(((struct ia64_fdesc *) fn)->func)
#define FDESC_GP(fn)	(((struct ia64_fdesc *) fn)->gp)

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb, set up the stack so that the child
 * ready to run and return to user mode.
 */
void
cpu_fork(p1, p2, flags)
	register struct proc *p1, *p2;
	int flags;
{
	if ((flags & RFPROC) == 0)
		return;

	p2->p_md.md_tf = p1->p_md.md_tf;
	p2->p_md.md_flags = p1->p_md.md_flags & (MDP_FPUSED | MDP_UAC_MASK);

	/*
	 * Copy floating point state from the FP chip to the PCB
	 * if this process has state stored there.
	 */
	ia64_fpstate_save(p1, 0);

	/*
	 * Copy pcb and stack from proc p1 to p2.  We do this as
	 * cheaply as possible, copying only the active part of the
	 * stack.  The stack and pcb need to agree. Make sure that the 
	 * new process has FEN disabled.
	 */
	p2->p_addr->u_pcb = p1->p_addr->u_pcb;

	/*
	 * Set the floating point state.
	 */
#if 0
	if ((p2->p_addr->u_pcb.pcb_fp_control & IEEE_INHERIT) == 0) {
		p2->p_addr->u_pcb.pcb_fp_control = 0;
		p2->p_addr->u_pcb.pcb_fp.fpr_cr = (FPCR_DYN_NORMAL
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
	if (p1 != curproc)
		panic("cpu_fork: curproc");
	ia64_fpstate_check(p1);
#endif

	/*
	 * create the child's kernel stack, from scratch.
	 */
	{
		struct user *up = p2->p_addr;
		struct trapframe *p2tf;
		u_int64_t bspstore, *p1bs, *p2bs, rnatloc, rnat;

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
		p2tf->tf_r[FRAME_R8] = 0; 	/* child's pid (linux) 	*/
		p2tf->tf_r[FRAME_R9] = 1;	/* is child (FreeBSD) 	*/
		p2tf->tf_r[FRAME_R10] = 0;	/* no error 		*/

		/*
		 * Turn off RSE for a moment and work out our current
		 * ar.bspstore. This assumes that p1==curproc. Also
		 * flush dirty regs to ensure that the user's stacked
		 * regs are written out to backing store.
		 *
		 * We could cope with p1!=curproc by digging values
		 * out of its PCB but I don't see the point since
		 * current usage never allows it.
		 */
		__asm __volatile("mov ar.rsc=0;;");
		__asm __volatile("flushrs;;" ::: "memory");
		__asm __volatile("mov %0=ar.bspstore" : "=r"(bspstore));

		p1bs = (u_int64_t *) (p1->p_addr + 1);
		p2bs = (u_int64_t *) (p2->p_addr + 1);

		/*
		 * Copy enough of p1's backing store to include all
		 * the user's stacked regs.
		 */
		bcopy(p1bs, p2bs, p1->p_md.md_tf->tf_ndirty);

		/*
		 * To calculate the ar.rnat for p2, we need to decide
		 * if p1's ar.bspstore has advanced past the place
		 * where the last ar.rnat which covers the user's
		 * saved registers would be placed. If so, we read
		 * that one from memory, otherwise we take p1's
		 * current ar.rnat.
		 */
		rnatloc = (u_int64_t)p1bs + p1->p_md.md_tf->tf_ndirty;
		rnatloc |= 0x1f8;
		if (bspstore > rnatloc)
			rnat = *(u_int64_t *) rnatloc;
		else
			__asm __volatile("mov %0=ar.rnat;;" : "=r"(rnat));
		
		/*
		 * Switch the RSE back on.
		 */
		__asm __volatile("mov ar.rsc=3;;");

		/*
		 * Setup the child's pcb so that its ar.bspstore
		 * starts just above the region which we copied. This
		 * should work since the child will normally return
		 * straight into exception_restore.
		 */
		up->u_pcb.pcb_bspstore =
			(u_int64_t)p2bs + p1->p_md.md_tf->tf_ndirty;
		up->u_pcb.pcb_rnat = rnat;
		up->u_pcb.pcb_pfs = 0;

		/*
		 * Arrange for continuation at fork_return(), which
		 * will return to exception_restore().  Note that the
		 * child process doesn't stay in the kernel for long!
		 */
		up->u_pcb.pcb_sp = (u_int64_t)p2tf - 16;	
		up->u_pcb.pcb_r4 = (u_int64_t)fork_return;
		up->u_pcb.pcb_r5 = FDESC_FUNC(exception_restore);
		up->u_pcb.pcb_r6 = (u_int64_t)p2;
		up->u_pcb.pcb_b0 = FDESC_FUNC(fork_trampoline);
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
	p->p_addr->u_pcb.pcb_r4 = (u_int64_t) func;
	p->p_addr->u_pcb.pcb_r6 = (u_int64_t) arg;
}

/*
 * cpu_exit is called as the last action during exit.
 * We drop the fp state (if we have it) and switch to a live one.
 * When the proc is reaped, cpu_wait() will gc the VM state.
 */
void
cpu_exit(p)
	register struct proc *p;
{
	ia64_fpstate_drop(p);

	PROC_LOCK(p);
	mtx_lock_spin(&sched_lock);
	mtx_unlock_flags(&Giant, MTX_NOSWITCH);
	mtx_assert(&Giant, MA_NOTOWNED);

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
	cpu_switch();
	panic("cpu_exit");
}

void
cpu_wait(p)
	struct proc *p;
{

	mtx_lock(&vm_mtx);
	/* drop per-process resources */
	pmap_dispose_proc(p);

	/* and clean-out the vmspace */
	vmspace_free(p->p_vmspace);
	mtx_unlock(&vm_mtx);
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

	mtx_lock(&vm_mtx);
	for (v = bp->b_saveaddr, addr = (caddr_t)trunc_page(bp->b_data);
	    addr < bp->b_data + bp->b_bufsize;
	    addr += PAGE_SIZE, v += PAGE_SIZE) {
		/*
		 * Do the vm_fault if needed; do the copy-on-write thing
		 * when reading stuff off device into memory.
		 */
		vm_fault_quick(addr,
			(bp->b_iocmd == BIO_READ)?(VM_PROT_READ|VM_PROT_WRITE):VM_PROT_READ);
		pa = trunc_page(pmap_kextract((vm_offset_t) addr));
		if (pa == 0)
			panic("vmapbuf: page not present");
		vm_page_hold(PHYS_TO_VM_PAGE(pa));
		pmap_kenter((vm_offset_t) v, pa);
	}
	mtx_unlock(&vm_mtx);

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

	mtx_lock(&vm_mtx);
	for (addr = (caddr_t)trunc_page(bp->b_data);
	    addr < bp->b_data + bp->b_bufsize;
	    addr += PAGE_SIZE) {
		pa = trunc_page(pmap_kextract((vm_offset_t) addr));
		pmap_kremove((vm_offset_t) addr);
		vm_page_unhold(PHYS_TO_VM_PAGE(pa));
	}
	mtx_unlock(&vm_mtx);

	bp->b_data = bp->b_saveaddr;
}

/*
 * Force reset the processor by invalidating the entire address space!
 */
void
cpu_reset()
{
	/* prom_halt(0); */
}

int
grow_stack(p, sp)
	struct proc *p;
	size_t sp;
{
	int rv;

	rv = vm_map_growstack (p, sp);
	if (rv != KERN_SUCCESS)
		return (0);

	return (1);
}


static int cnt_prezero;

SYSCTL_INT(_machdep, OID_AUTO, cnt_prezero, CTLFLAG_RD, &cnt_prezero, 0, "");

/*
 * Implement the pre-zeroed page mechanism.
 * This routine is called from the idle loop.
 */

#define ZIDLE_LO(v)    ((v) * 2 / 3)
#define ZIDLE_HI(v)    ((v) * 4 / 5)

int
vm_page_zero_idle()
{
	static int free_rover;
	static int zero_state;
	vm_page_t m;
	int s;

	/*
         * Attempt to maintain approximately 1/2 of our free pages in a
         * PG_ZERO'd state.   Add some hysteresis to (attempt to) avoid
         * generally zeroing a page when the system is near steady-state.
         * Otherwise we might get 'flutter' during disk I/O / IPC or
         * fast sleeps.  We also do not want to be continuously zeroing
         * pages because doing so may flush our L1 and L2 caches too much.
	 */

	if (mtx_trylock(&vm_mtx) == 0)
		return (0);
	if (zero_state && vm_page_zero_count >= ZIDLE_LO(cnt.v_free_count)) {
		mtx_unlock(&vm_mtx);
		return(0);
	}
	if (vm_page_zero_count >= ZIDLE_HI(cnt.v_free_count)) {
		mtx_unlock(&vm_mtx);
		return(0);
	}

		s = splvm();
		m = vm_page_list_find(PQ_FREE, free_rover, FALSE);
		zero_state = 0;
		if (m != NULL && (m->flags & PG_ZERO) == 0) {
			vm_page_queues[m->queue].lcnt--;
			TAILQ_REMOVE(&vm_page_queues[m->queue].pl, m, pageq);
			m->queue = PQ_NONE;
			splx(s);
			pmap_zero_page(VM_PAGE_TO_PHYS(m));
			(void)splvm();
			vm_page_flag_set(m, PG_ZERO);
			m->queue = PQ_FREE + m->pc;
			vm_page_queues[m->queue].lcnt++;
			TAILQ_INSERT_TAIL(&vm_page_queues[m->queue].pl, m,
			    pageq);
			++vm_page_zero_count;
			++cnt_prezero;
			if (vm_page_zero_count >= ZIDLE_HI(cnt.v_free_count))
				zero_state = 1;
		}
		free_rover = (free_rover + PQ_PRIME2) & PQ_L2_MASK;
		splx(s);
		mtx_unlock(&vm_mtx);
		return (1);
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
