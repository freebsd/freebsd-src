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

#include "opt_npx.h"
#ifdef PC98
#include "opt_pc98.h"
#endif
#include "opt_reset.h"
#include "opt_isa.h"
#include "opt_kstack_pages.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/kse.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/mutex.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>

#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/pcb_ext.h>
#include <machine/vm86.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>

#include <sys/user.h>

#ifdef PC98
#include <pc98/pc98/pc98.h>
#else
#include <i386/isa/isa.h>
#endif

static void	cpu_reset_real(void);
#ifdef SMP
static void	cpu_reset_proxy(void);
static u_int	cpu_reset_proxyid;
static volatile u_int	cpu_reset_proxy_active;
#endif
extern int	_ucodesel, _udatasel;

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
	struct thread *td2;
	int flags;
{
	register struct proc *p1;
	struct pcb *pcb2;
	struct mdproc *mdp2;
#ifdef DEV_NPX
	register_t savecrit;
#endif

	p1 = td1->td_proc;
	if ((flags & RFPROC) == 0) {
		if ((flags & RFMEM) == 0) {
			/* unshare user LDT */
			struct mdproc *mdp1 = &p1->p_md;
			struct proc_ldt *pldt = mdp1->md_ldt;
			if (pldt && pldt->ldt_refcnt > 1) {
				pldt = user_ldt_alloc(mdp1, pldt->ldt_len);
				if (pldt == NULL)
					panic("could not copy LDT");
				mdp1->md_ldt = pldt;
				set_user_ldt(mdp1);
				user_ldt_free(td1);
			}
		}
		return;
	}

	/* Ensure that p1's pcb is up to date. */
#ifdef DEV_NPX
	if (td1 == curthread)
		td1->td_pcb->pcb_gs = rgs();
	savecrit = intr_disable();
	if (PCPU_GET(fpcurthread) == td1)
		npxsave(&td1->td_pcb->pcb_save);
	intr_restore(savecrit);
#endif

	/* Point the pcb to the top of the stack */
	pcb2 = (struct pcb *)(td2->td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;
	td2->td_pcb = pcb2;

	/* Copy p1's pcb */
	bcopy(td1->td_pcb, pcb2, sizeof(*pcb2));

	/* Point mdproc and then copy over td1's contents */
	mdp2 = &p2->p_md;
	bcopy(&p1->p_md, mdp2, sizeof(*mdp2));

	/*
	 * Create a new fresh stack for the new process.
	 * Copy the trap frame for the return to user mode as if from a
	 * syscall.  This copies most of the user mode register values.
	 * The -16 is so we can expand the trapframe if we go to vm86.
	 */
	td2->td_frame = (struct trapframe *)((caddr_t)td2->td_pcb - 16) - 1;
	bcopy(td1->td_frame, td2->td_frame, sizeof(struct trapframe));

	td2->td_frame->tf_eax = 0;		/* Child returns zero */
	td2->td_frame->tf_eflags &= ~PSL_C;	/* success */
	td2->td_frame->tf_edx = 1;

	/*
	 * Set registers for trampoline to user mode.  Leave space for the
	 * return address on stack.  These are the kernel mode register values.
	 */
	pcb2->pcb_cr3 = vtophys(vmspace_pmap(p2->p_vmspace)->pm_pdir);
	pcb2->pcb_edi = 0;
	pcb2->pcb_esi = (int)fork_return;	/* fork_trampoline argument */
	pcb2->pcb_ebp = 0;
	pcb2->pcb_esp = (int)td2->td_frame - sizeof(void *);
	pcb2->pcb_ebx = (int)td2;		/* fork_trampoline argument */
	pcb2->pcb_eip = (int)fork_trampoline;
	pcb2->pcb_psl = td2->td_frame->tf_eflags & ~PSL_I; /* ints disabled */
	/*-
	 * pcb2->pcb_dr*:	cloned above.
	 * pcb2->pcb_savefpu:	cloned above.
	 * pcb2->pcb_flags:	cloned above.
	 * pcb2->pcb_onfault:	cloned above (always NULL here?).
	 * pcb2->pcb_gs:	cloned above.
	 * pcb2->pcb_ext:	cleared below.
	 */

	/*
	 * XXX don't copy the i/o pages.  this should probably be fixed.
	 */
	pcb2->pcb_ext = 0;

        /* Copy the LDT, if necessary. */
	mtx_lock_spin(&sched_lock);
        if (mdp2->md_ldt != 0) {
		if (flags & RFMEM) {
			mdp2->md_ldt->ldt_refcnt++;
		} else {
			mdp2->md_ldt = user_ldt_alloc(mdp2,
			    mdp2->md_ldt->ldt_len);
			if (mdp2->md_ldt == NULL)
				panic("could not copy LDT");
		}
        }
	mtx_unlock_spin(&sched_lock);

	/*
	 * Now, cpu_switch() can schedule the new process.
	 * pcb_esp is loaded pointing to the cpu_switch() stack frame
	 * containing the return address when exiting cpu_switch.
	 * This will normally be to fork_trampoline(), which will have
	 * %ebx loaded with the new proc's pointer.  fork_trampoline()
	 * will set up a stack to call fork_return(p, frame); to complete
	 * the return to user-mode.
	 */
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
	td->td_pcb->pcb_esi = (int) func;	/* function */
	td->td_pcb->pcb_ebx = (int) arg;	/* first arg */
}

void
cpu_exit(struct thread *td)
{
	struct mdproc *mdp;

	mdp = &td->td_proc->p_md;
	if (mdp->md_ldt)
		user_ldt_free(td);
	reset_dbregs();
}

void
cpu_thread_exit(struct thread *td)
{
	struct pcb *pcb = td->td_pcb; 
#ifdef DEV_NPX
	npxexit(td);
#endif
	if (pcb->pcb_ext != 0) {
		/* XXXKSE  XXXSMP  not SMP SAFE.. what locks do we have? */
		/* if (pcb->pcb_ext->ext_refcount-- == 1) ?? */
	        /* 
		 * XXX do we need to move the TSS off the allocated pages 
		 * before freeing them?  (not done here)
		 */
		kmem_free(kernel_map, (vm_offset_t)pcb->pcb_ext,
		    ctob(IOPAGES + 1));
		pcb->pcb_ext = 0;
	}
        if (pcb->pcb_flags & PCB_DBREGS) {
                /*
                 * disable all hardware breakpoints
                 */
                reset_dbregs();
                pcb->pcb_flags &= ~PCB_DBREGS;
        }
}

void
cpu_sched_exit(td)
	register struct thread *td;
{
}

void
cpu_thread_setup(struct thread *td)
{

	td->td_pcb =
	     (struct pcb *)(td->td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;
	td->td_frame = (struct trapframe *)((caddr_t)td->td_pcb - 16) - 1;
}

/*
 * Initialize machine state (pcb and trap frame) for a new thread about to
 * upcall. Pu t enough state in the new thread's PCB to get it to go back 
 * userret(), where we can intercept it again to set the return (upcall)
 * Address and stack, along with those from upcals that are from other sources
 * such as those generated in thread_userret() itself.
 */
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
	 * The -16 is so we can expand the trapframe if we go to vm86.
	 * Don't forget to set this stack value into whatever supplies
	 * the address for the fault handlers.
	 * The contexts are filled in at the time we actually DO the
	 * upcall as only then do we know which KSE we got.
	 */
	td->td_frame = (struct trapframe *)((caddr_t)pcb2 - 16) - 1;

	/*
	 * Set registers for trampoline to user mode.  Leave space for the
	 * return address on stack.  These are the kernel mode register values.
	 */
	pcb2->pcb_cr3 = vtophys(vmspace_pmap(td->td_proc->p_vmspace)->pm_pdir);
	pcb2->pcb_edi = 0;
	pcb2->pcb_esi = (int)fork_return;		    /* trampoline arg */
	pcb2->pcb_ebp = 0;
	pcb2->pcb_esp = (int)td->td_frame - sizeof(void *); /* trampoline arg */
	pcb2->pcb_ebx = (int)td;			    /* trampoline arg */
	pcb2->pcb_eip = (int)fork_trampoline;
	pcb2->pcb_psl &= ~(PSL_I);	/* interrupts must be disabled */
	/*
	 * If we didn't copy the pcb, we'd need to do the following registers:
	 * pcb2->pcb_dr*:	cloned above.
	 * pcb2->pcb_savefpu:	cloned above.
	 * pcb2->pcb_flags:	cloned above.
	 * pcb2->pcb_onfault:	cloned above (always NULL here?).
	 * pcb2->pcb_gs:	cloned above.  XXXKSE ???
	 * pcb2->pcb_ext:	cleared below.
	 */
	 pcb2->pcb_ext = NULL;
}

/*
 * Set that machine state for performing an upcall that has to
 * be done in thread_userret() so that those upcalls generated
 * in thread_userret() itself can be done as well.
 */
void
cpu_set_upcall_kse(struct thread *td, struct kse *ke)
{

	/*
	 * Set the trap frame to point at the beginning of the uts
	 * function.
	 */
	td->td_frame->tf_esp =
	    (int)ke->ke_stack.ss_sp + ke->ke_stack.ss_size - 16;
	td->td_frame->tf_eip = (int)ke->ke_upcall;

	/*
	 * Pass the address of the mailbox for this kse to the uts
	 * function as a parameter on the stack.
	 */
	suword((void *)(td->td_frame->tf_esp + sizeof(void *)),
	    (int)ke->ke_mailbox);
}

void
cpu_wait(p)
	struct proc *p;
{
}

/*
 * Convert kernel VA to physical address
 */
u_long
kvtop(void *addr)
{
	vm_offset_t va;

	va = pmap_kextract((vm_offset_t)addr);
	if (va == 0)
		panic("kvtop: zero page frame");
	return((int)va);
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
	register caddr_t addr, kva;
	vm_offset_t pa;
	int pidx;
	struct vm_page *m;

	GIANT_REQUIRED;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vmapbuf");

	for (addr = (caddr_t)trunc_page((vm_offset_t)bp->b_data), pidx = 0;
	     addr < bp->b_data + bp->b_bufsize;
	     addr += PAGE_SIZE, pidx++) {
		/*
		 * Do the vm_fault if needed; do the copy-on-write thing
		 * when reading stuff off device into memory.
		 */
		vm_fault_quick((addr >= bp->b_data) ? addr : bp->b_data,
			(bp->b_iocmd == BIO_READ)?(VM_PROT_READ|VM_PROT_WRITE):VM_PROT_READ);
		pa = trunc_page(pmap_kextract((vm_offset_t) addr));
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
	int pidx;
	int npages;
	vm_page_t *m;

	GIANT_REQUIRED;

	if ((bp->b_flags & B_PHYS) == 0)
		panic("vunmapbuf");

	npages = bp->b_npages;
	pmap_qremove(trunc_page((vm_offset_t)bp->b_data),
		     npages);
	m = bp->b_pages;
	for (pidx = 0; pidx < npages; pidx++)
		vm_page_unhold(*m++);

	bp->b_data = bp->b_saveaddr;
}

/*
 * Force reset the processor by invalidating the entire address space!
 */

#ifdef SMP
static void
cpu_reset_proxy()
{

	cpu_reset_proxy_active = 1;
	while (cpu_reset_proxy_active == 1)
		;	 /* Wait for other cpu to see that we've started */
	stop_cpus((1<<cpu_reset_proxyid));
	printf("cpu_reset_proxy: Stopped CPU %d\n", cpu_reset_proxyid);
	DELAY(1000000);
	cpu_reset_real();
}
#endif

void
cpu_reset()
{
#ifdef SMP
	if (smp_active == 0) {
		cpu_reset_real();
		/* NOTREACHED */
	} else {

		u_int map;
		int cnt;
		printf("cpu_reset called on cpu#%d\n", PCPU_GET(cpuid));

		map = PCPU_GET(other_cpus) & ~ stopped_cpus;

		if (map != 0) {
			printf("cpu_reset: Stopping other CPUs\n");
			stop_cpus(map);		/* Stop all other CPUs */
		}

		if (PCPU_GET(cpuid) == 0) {
			DELAY(1000000);
			cpu_reset_real();
			/* NOTREACHED */
		} else {
			/* We are not BSP (CPU #0) */

			cpu_reset_proxyid = PCPU_GET(cpuid);
			cpustop_restartfunc = cpu_reset_proxy;
			cpu_reset_proxy_active = 0;
			printf("cpu_reset: Restarting BSP\n");
			started_cpus = (1<<0);		/* Restart CPU #0 */

			cnt = 0;
			while (cpu_reset_proxy_active == 0 && cnt < 10000000)
				cnt++;	/* Wait for BSP to announce restart */
			if (cpu_reset_proxy_active == 0)
				printf("cpu_reset: Failed to restart BSP\n");
			enable_intr();
			cpu_reset_proxy_active = 2;

			while (1);
			/* NOTREACHED */
		}
	}
#else
	cpu_reset_real();
#endif
}

static void
cpu_reset_real()
{

#ifdef PC98
	/*
	 * Attempt to do a CPU reset via CPU reset port.
	 */
	disable_intr();
	if ((inb(0x35) & 0xa0) != 0xa0) {
		outb(0x37, 0x0f);		/* SHUT0 = 0. */
		outb(0x37, 0x0b);		/* SHUT1 = 0. */
	}
	outb(0xf0, 0x00);		/* Reset. */
#else
	/*
	 * Attempt to do a CPU reset via the keyboard controller,
	 * do not turn of the GateA20, as any machine that fails
	 * to do the reset here would then end up in no man's land.
	 */

#if !defined(BROKEN_KEYBOARD_RESET)
	outb(IO_KBD + 4, 0xFE);
	DELAY(500000);	/* wait 0.5 sec to see if that did it */
	printf("Keyboard reset did not work, attempting CPU shutdown\n");
	DELAY(1000000);	/* wait 1 sec for printf to complete */
#endif
#endif /* PC98 */
	/* force a shutdown by unmapping entire address space ! */
	bzero((caddr_t) PTD, PAGE_SIZE);

	/* "good night, sweet prince .... <THUNK!>" */
	invltlb();
	/* NOTREACHED */
	while(1);
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

#ifdef DEV_ISA
	/* The ISA ``memory hole''. */
	if (addr >= 0xa0000 && addr < 0x100000)
		return 0;
#endif

	/*
	 * stuff other tests for known memory-mapped devices (PCI?)
	 * here
	 */

	return 1;
}
