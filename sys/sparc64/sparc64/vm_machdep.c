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
#include <sys/malloc.h>
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
#include <vm/vm_map.h>
#include <vm/vm_page.h>
#include <vm/vm_param.h>

#include <machine/cache.h>
#include <machine/cpu.h>
#include <machine/fsr.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/ofw_machdep.h>
#include <machine/tstate.h>

void
cpu_exit(struct thread *td)
{
	struct md_utrap *ut;
	struct proc *p;

	p = td->td_proc;
	p->p_md.md_sigtramp = NULL;
	if ((ut = p->p_md.md_utrap) != NULL) {
		ut->ut_refcnt--;
		if (ut->ut_refcnt == 0)
			free(ut, M_SUBPROC);
		p->p_md.md_utrap = NULL;
	}
}

void
cpu_sched_exit(struct thread *td)
{
	struct vmspace *vm;
	struct pcpu *pc;
	struct proc *p;

	mtx_assert(&sched_lock, MA_OWNED);

	p = td->td_proc;
	vm = p->p_vmspace;
	if (vm->vm_refcnt > 1)
		return;
	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		if (pc->pc_vmspace == vm) {
			vm->vm_pmap.pm_active &= ~pc->pc_cpumask;
			vm->vm_pmap.pm_context[pc->pc_cpuid] = -1;
			pc->pc_vmspace = NULL;
		}
	}
}

void
cpu_thread_exit(struct thread *td)
{
}

void
cpu_thread_dtor(struct thread *td)
{
}

void
cpu_thread_setup(struct thread *td)
{
}

void
cpu_set_upcall(struct thread *td, void *pcb)
{
}

void
cpu_set_upcall_kse(struct thread *td, struct kse *ke)
{
}

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb, set up the stack so that the child
 * ready to run and return to user mode.
 */
void
cpu_fork(struct thread *td1, struct proc *p2, struct thread *td2, int flags)
{
	struct md_utrap *ut;
	struct trapframe *tf;
	struct frame *fp;
	struct pcb *pcb1;
	struct pcb *pcb2;
	vm_offset_t sp;
	int error;
	int i;

	KASSERT(td1 == curthread || td1 == &thread0,
	    ("cpu_fork: p1 not curproc and not proc0"));

	if ((flags & RFPROC) == 0)
		return;

	p2->p_md.md_sigtramp = td1->td_proc->p_md.md_sigtramp;
	if ((ut = td1->td_proc->p_md.md_utrap) != NULL)
		ut->ut_refcnt++;
	p2->p_md.md_utrap = ut;

	/* The pcb must be aligned on a 64-byte boundary. */
	pcb1 = td1->td_pcb;
	pcb2 = (struct pcb *)((td2->td_kstack + KSTACK_PAGES * PAGE_SIZE -
	    sizeof(struct pcb)) & ~0x3fUL);
	td2->td_pcb = pcb2;

	/*
	 * Ensure that p1's pcb is up to date.
	 */
	if ((td1->td_frame->tf_fprs & FPRS_FEF) != 0) {
		mtx_lock_spin(&sched_lock);
		savefpctx(&pcb1->pcb_fpstate);
		mtx_unlock_spin(&sched_lock);
	}
	/* Make sure the copied windows are spilled. */
	flushw();
	/* Copy the pcb (this will copy the windows saved in the pcb, too). */
	bcopy(pcb1, pcb2, sizeof(*pcb1));

	/*
	 * If we're creating a new user process and we're sharing the address
	 * space, the parent's top most frame must be saved in the pcb.  The
	 * child will pop the frame when it returns to user mode, and may
	 * overwrite it with its own data causing much suffering for the
	 * parent.  We check if its already in the pcb, and if not copy it
	 * in.  Its unlikely that the copyin will fail, but if so there's not
	 * much we can do.  The parent will likely crash soon anyway in that
	 * case.
	 */
	if ((flags & RFMEM) != 0 && td1 != &thread0) {
		sp = td1->td_frame->tf_sp;
		for (i = 0; i < pcb1->pcb_nsaved; i++) {
			if (pcb1->pcb_rwsp[i] == sp)
				break;
		}
		if (i == pcb1->pcb_nsaved) {
			error = copyin((caddr_t)sp + SPOFF, &pcb1->pcb_rw[i],
			    sizeof(struct rwindow));
			if (error == 0) {
				pcb1->pcb_rwsp[i] = sp;
				pcb1->pcb_nsaved++;
			}
		}
	}

	/*
	 * Create a new fresh stack for the new process.
	 * Copy the trap frame for the return to user mode as if from a
	 * syscall.  This copies most of the user mode register values.
	 */
	tf = (struct trapframe *)pcb2 - 1;
	bcopy(td1->td_frame, tf, sizeof(*tf));

	tf->tf_out[0] = 0;			/* Child returns zero */
	tf->tf_out[1] = 0;
	tf->tf_tstate &= ~TSTATE_XCC_C;		/* success */
	tf->tf_fprs = 0;

	td2->td_frame = tf;
	fp = (struct frame *)tf - 1;
	fp->fr_local[0] = (u_long)fork_return;
	fp->fr_local[1] = (u_long)td2;
	fp->fr_local[2] = (u_long)tf;
	pcb2->pcb_fp = (u_long)fp - SPOFF;
	pcb2->pcb_pc = (u_long)fork_trampoline - 8;

	/*
	 * Now, cpu_switch() can schedule the new process.
	 */
}

void
cpu_reset(void)
{
	static char bspec[64] = "";
	phandle_t chosen;
	static struct {
		cell_t	name;
		cell_t	nargs;
		cell_t	nreturns;
		cell_t	bootspec;
	} args = {
		(cell_t)"boot",
		1,
		0,
		(cell_t)bspec
	};
	if ((chosen = OF_finddevice("/chosen")) != 0) {
		if (OF_getprop(chosen, "bootpath", bspec, sizeof(bspec)) == -1)
			bspec[0] = '\0';
		bspec[sizeof(bspec) - 1] = '\0';
	}

	openfirmware_exit(&args);
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
	fp->fr_local[0] = (u_long)func;
	fp->fr_local[1] = (u_long)arg;
}

void
cpu_wait(struct proc *p)
{
}

int
is_physical_memory(vm_offset_t addr)
{

	/* There is no device memory in the midst of the normal RAM. */
	return (1);
}

void
swi_vm(void *v)
{

	/*
	 * Nothing to do here yet - busdma bounce buffers are not yet
	 * implemented.
	 */
}

/*
 * quick version of vm_fault
 */
int
vm_fault_quick(caddr_t v, int prot)
{
	int r;

	if (prot & VM_PROT_WRITE)
		r = subyte(v, fubyte(v));
	else
		r = fubyte(v);
	return(r);
}

/*
 * Map an IO request into kernel virtual address space.
 *
 * All requests are (re)mapped into kernel VA space.
 * Notice that we use b_bufsize for the size of the buffer
 * to be mapped.  b_bcount might be modified by the driver.
 */
void
vmapbuf(struct buf *bp)
{
	caddr_t addr, kva;
	vm_offset_t pa;
	int pidx;
	struct vm_page *m;
	pmap_t pmap;

	GIANT_REQUIRED;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");

	pmap = &curproc->p_vmspace->vm_pmap;
	for (addr = (caddr_t)trunc_page((vm_offset_t)bp->b_data), pidx = 0;
	     addr < bp->b_data + bp->b_bufsize; addr += PAGE_SIZE,  pidx++) {
		/*
		 * Do the vm_fault if needed; do the copy-on-write thing
		 * when reading stuff off device into memory.
		 */
		vm_fault_quick((addr >= bp->b_data) ? addr : bp->b_data,
		    (bp->b_iocmd == BIO_READ) ? (VM_PROT_READ | VM_PROT_WRITE) :
		    VM_PROT_READ);
		pa = trunc_page(pmap_extract(pmap, (vm_offset_t)addr));
		if (pa == 0)
			panic("vmapbuf: page not present");
		m = PHYS_TO_VM_PAGE(pa);
		vm_page_hold(m);
		bp->b_pages[pidx] = m;
	}
	if (pidx > btoc(MAXPHYS))
		panic("vmapbuf: mapped more than MAXPHYS");
	pmap_qenter((vm_offset_t)bp->b_saveaddr, bp->b_pages, pidx);

	kva = bp->b_saveaddr;
	bp->b_npages = pidx;
	bp->b_saveaddr = bp->b_data;
	bp->b_data = kva + (((vm_offset_t)bp->b_data) & PAGE_MASK);
}

/*
 * Free the io map PTEs associated with this IO operation.
 * We also invalidate the TLB entries and restore the original b_addr.
 */
void
vunmapbuf(struct buf *bp)
{
	int pidx;
	int npages;

	GIANT_REQUIRED;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");

	npages = bp->b_npages;
	pmap_qremove(trunc_page((vm_offset_t)bp->b_data),
	    npages);
	for (pidx = 0; pidx < npages; pidx++)
		vm_page_unhold(bp->b_pages[pidx]);

	bp->b_data = bp->b_saveaddr;
}
