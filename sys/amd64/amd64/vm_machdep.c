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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_isa.h"
#include "opt_cpu.h"
#include "opt_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/pioctl.h>
#include <sys/proc.h>
#include <sys/sf_buf.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>

#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/specialreg.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>

#include <amd64/isa/isa.h>

#ifdef COMPAT_IA32

extern struct sysentvec ia32_freebsd_sysvec;

#endif

static void	cpu_reset_real(void);
#ifdef SMP
static void	cpu_reset_proxy(void);
static u_int	cpu_reset_proxyid;
static volatile u_int	cpu_reset_proxy_active;
#endif

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

	p1 = td1->td_proc;
	if ((flags & RFPROC) == 0)
		return;

	/* Ensure that p1's pcb is up to date. */
	fpuexit(td1);

	/* Point the pcb to the top of the stack */
	pcb2 = (struct pcb *)(td2->td_kstack +
	    td2->td_kstack_pages * PAGE_SIZE) - 1;
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
	 */
	td2->td_frame = (struct trapframe *)td2->td_pcb - 1;
	bcopy(td1->td_frame, td2->td_frame, sizeof(struct trapframe));

	td2->td_frame->tf_rax = 0;		/* Child returns zero */
	td2->td_frame->tf_rflags &= ~PSL_C;	/* success */
	td2->td_frame->tf_rdx = 1;

	/*
	 * If the parent process has the trap bit set (i.e. a debugger had
	 * single stepped the process to the system call), we need to clear
	 * the trap flag from the new frame unless the debugger had set PF_FORK
	 * on the parent.  Otherwise, the child will receive a (likely
	 * unexpected) SIGTRAP when it executes the first instruction after
	 * returning  to userland.
	 */
	if ((p1->p_pfsflags & PF_FORK) == 0)
		td2->td_frame->tf_rflags &= ~PSL_T;

	/*
	 * Set registers for trampoline to user mode.  Leave space for the
	 * return address on stack.  These are the kernel mode register values.
	 */
	pcb2->pcb_cr3 = vtophys(vmspace_pmap(p2->p_vmspace)->pm_pml4);
	pcb2->pcb_r12 = (register_t)fork_return;	/* fork_trampoline argument */
	pcb2->pcb_rbp = 0;
	pcb2->pcb_rsp = (register_t)td2->td_frame - sizeof(void *);
	pcb2->pcb_rbx = (register_t)td2;		/* fork_trampoline argument */
	pcb2->pcb_rip = (register_t)fork_trampoline;
	/*-
	 * pcb2->pcb_dr*:	cloned above.
	 * pcb2->pcb_savefpu:	cloned above.
	 * pcb2->pcb_flags:	cloned above.
	 * pcb2->pcb_onfault:	cloned above (always NULL here?).
	 * pcb2->pcb_[fg]sbase:	cloned above
	 */

	/* Setup to release spin count in fork_exit(). */
	td2->td_md.md_spinlock_count = 1;
	td2->td_md.md_saved_flags = PSL_KERNEL | PSL_I;

	/*
	 * Now, cpu_switch() can schedule the new process.
	 * pcb_rsp is loaded pointing to the cpu_switch() stack frame
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
	td->td_pcb->pcb_r12 = (long) func;	/* function */
	td->td_pcb->pcb_rbx = (long) arg;	/* first arg */
}

void
cpu_exit(struct thread *td)
{
}

void
cpu_thread_exit(struct thread *td)
{

	if (td == PCPU_GET(fpcurthread))
		fpudrop();

	/* Disable any hardware breakpoints. */
	if (td->td_pcb->pcb_flags & PCB_DBREGS) {
		reset_dbregs();
		td->td_pcb->pcb_flags &= ~PCB_DBREGS;
	}
}

void
cpu_thread_clean(struct thread *td)
{
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
cpu_thread_alloc(struct thread *td)
{

	td->td_pcb = (struct pcb *)(td->td_kstack +
	    td->td_kstack_pages * PAGE_SIZE) - 1;
	td->td_frame = (struct trapframe *)td->td_pcb - 1;
}

void
cpu_thread_free(struct thread *td)
{
}

/*
 * Initialize machine state (pcb and trap frame) for a new thread about to
 * upcall. Put enough state in the new thread's PCB to get it to go back 
 * userret(), where we can intercept it again to set the return (upcall)
 * Address and stack, along with those from upcals that are from other sources
 * such as those generated in thread_userret() itself.
 */
void
cpu_set_upcall(struct thread *td, struct thread *td0)
{
	struct pcb *pcb2;

	/* Point the pcb to the top of the stack. */
	pcb2 = td->td_pcb;

	/*
	 * Copy the upcall pcb.  This loads kernel regs.
	 * Those not loaded individually below get their default
	 * values here.
	 */
	bcopy(td0->td_pcb, pcb2, sizeof(*pcb2));
	pcb2->pcb_flags &= ~PCB_FPUINITDONE;

	/*
	 * Create a new fresh stack for the new thread.
	 */
	bcopy(td0->td_frame, td->td_frame, sizeof(struct trapframe));

	/* If the current thread has the trap bit set (i.e. a debugger had
	 * single stepped the process to the system call), we need to clear
	 * the trap flag from the new frame. Otherwise, the new thread will
	 * receive a (likely unexpected) SIGTRAP when it executes the first
	 * instruction after returning to userland.
	 */
	td->td_frame->tf_rflags &= ~PSL_T;

	/*
	 * Set registers for trampoline to user mode.  Leave space for the
	 * return address on stack.  These are the kernel mode register values.
	 */
	pcb2->pcb_cr3 = vtophys(vmspace_pmap(td->td_proc->p_vmspace)->pm_pml4);
	pcb2->pcb_r12 = (register_t)fork_return;	    /* trampoline arg */
	pcb2->pcb_rbp = 0;
	pcb2->pcb_rsp = (register_t)td->td_frame - sizeof(void *);	/* trampoline arg */
	pcb2->pcb_rbx = (register_t)td;			    /* trampoline arg */
	pcb2->pcb_rip = (register_t)fork_trampoline;
	/*
	 * If we didn't copy the pcb, we'd need to do the following registers:
	 * pcb2->pcb_dr*:	cloned above.
	 * pcb2->pcb_savefpu:	cloned above.
	 * pcb2->pcb_onfault:	cloned above (always NULL here?).
	 * pcb2->pcb_[fg]sbase: cloned above
	 */

	/* Setup to release spin count in fork_exit(). */
	td->td_md.md_spinlock_count = 1;
	td->td_md.md_saved_flags = PSL_KERNEL | PSL_I;
}

/*
 * Set that machine state for performing an upcall that has to
 * be done in thread_userret() so that those upcalls generated
 * in thread_userret() itself can be done as well.
 */
void
cpu_set_upcall_kse(struct thread *td, void (*entry)(void *), void *arg,
	stack_t *stack)
{

	/* 
	 * Do any extra cleaning that needs to be done.
	 * The thread may have optional components
	 * that are not present in a fresh thread.
	 * This may be a recycled thread so make it look
	 * as though it's newly allocated.
	 */
	cpu_thread_clean(td);

#ifdef COMPAT_IA32
	if (td->td_proc->p_sysent == &ia32_freebsd_sysvec) {
		/*
	 	 * Set the trap frame to point at the beginning of the uts
		 * function.
		 */
		td->td_frame->tf_rbp = 0;
		td->td_frame->tf_rsp =
		   (((uintptr_t)stack->ss_sp + stack->ss_size - 4) & ~0x0f) - 4;
		td->td_frame->tf_rip = (uintptr_t)entry;

		/*
		 * Pass the address of the mailbox for this kse to the uts
		 * function as a parameter on the stack.
		 */
		suword32((void *)(td->td_frame->tf_rsp + sizeof(int32_t)),
		    (uint32_t)(uintptr_t)arg);

		return;
	}
#endif

	/*
	 * Set the trap frame to point at the beginning of the uts
	 * function.
	 */
	td->td_frame->tf_rbp = 0;
	td->td_frame->tf_rsp =
	    ((register_t)stack->ss_sp + stack->ss_size) & ~0x0f;
	td->td_frame->tf_rsp -= 8;
	td->td_frame->tf_rip = (register_t)entry;

	/*
	 * Pass the address of the mailbox for this kse to the uts
	 * function as a parameter on the stack.
	 */
	td->td_frame->tf_rdi = (register_t)arg;
}

int
cpu_set_user_tls(struct thread *td, void *tls_base)
{

	if ((u_int64_t)tls_base >= VM_MAXUSER_ADDRESS)
		return (EINVAL);

#ifdef COMPAT_IA32
	if (td->td_proc->p_sysent == &ia32_freebsd_sysvec) {
		if (td == curthread) {
			critical_enter();
			td->td_pcb->pcb_gsbase = (register_t)tls_base;
			wrmsr(MSR_KGSBASE, td->td_pcb->pcb_gsbase);
			critical_exit();
		} else {
			td->td_pcb->pcb_gsbase = (register_t)tls_base;
		}
		return (0);
	}
#endif
	if (td == curthread) {
		critical_enter();
		td->td_pcb->pcb_fsbase = (register_t)tls_base;
		wrmsr(MSR_FSBASE, td->td_pcb->pcb_fsbase);
		critical_exit();
	} else {
		td->td_pcb->pcb_fsbase = (register_t)tls_base;
	}
	return (0);
}

#ifdef SMP
static void
cpu_reset_proxy()
{

	cpu_reset_proxy_active = 1;
	while (cpu_reset_proxy_active == 1)
		;	/* Wait for other cpu to see that we've started */
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
	u_int cnt, map;

	if (smp_active) {
		map = PCPU_GET(other_cpus) & ~stopped_cpus;
		if (map != 0) {
			printf("cpu_reset: Stopping other CPUs\n");
			stop_cpus(map);
		}

		if (PCPU_GET(cpuid) != 0) {
			cpu_reset_proxyid = PCPU_GET(cpuid);
			cpustop_restartfunc = cpu_reset_proxy;
			cpu_reset_proxy_active = 0;
			printf("cpu_reset: Restarting BSP\n");

			/* Restart CPU #0. */
			atomic_store_rel_int(&started_cpus, 1 << 0);

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

		DELAY(1000000);
	}
#endif
	cpu_reset_real();
	/* NOTREACHED */
}

static void
cpu_reset_real()
{
	struct region_descriptor null_idt;
	int b;

	disable_intr();

	/*
	 * Attempt to do a CPU reset via the keyboard controller,
	 * do not turn off GateA20, as any machine that fails
	 * to do the reset here would then end up in no man's land.
	 */
	outb(IO_KBD + 4, 0xFE);
	DELAY(500000);	/* wait 0.5 sec to see if that did it */

	/*
	 * Attempt to force a reset via the Reset Control register at
	 * I/O port 0xcf9.  Bit 2 forces a system reset when it
	 * transitions from 0 to 1.  Bit 1 selects the type of reset
	 * to attempt: 0 selects a "soft" reset, and 1 selects a
	 * "hard" reset.  We try a "hard" reset.  The first write sets
	 * bit 1 to select a "hard" reset and clears bit 2.  The
	 * second write forces a 0 -> 1 transition in bit 2 to trigger
	 * a reset.
	 */
	outb(0xcf9, 0x2);
	outb(0xcf9, 0x6);
	DELAY(500000);  /* wait 0.5 sec to see if that did it */

	/*
	 * Attempt to force a reset via the Fast A20 and Init register
	 * at I/O port 0x92.  Bit 1 serves as an alternate A20 gate.
	 * Bit 0 asserts INIT# when set to 1.  We are careful to only
	 * preserve bit 1 while setting bit 0.  We also must clear bit
	 * 0 before setting it if it isn't already clear.
	 */
	b = inb(0x92);
	if (b != 0xff) {
		if ((b & 0x1) != 0)
			outb(0x92, b & 0xfe);
		outb(0x92, b | 0x1);
		DELAY(500000);  /* wait 0.5 sec to see if that did it */
	}

	printf("No known reset method worked, attempting CPU shutdown\n");
	DELAY(1000000);	/* wait 1 sec for printf to complete */

	/* Wipe the IDT. */
	null_idt.rd_limit = 0;
	null_idt.rd_base = 0;
	lidt(&null_idt);

	/* "good night, sweet prince .... <THUNK!>" */
	breakpoint();

	/* NOTREACHED */
	while(1);
}

/*
 * Allocate an sf_buf for the given vm_page.  On this machine, however, there
 * is no sf_buf object.  Instead, an opaque pointer to the given vm_page is
 * returned.
 */
struct sf_buf *
sf_buf_alloc(struct vm_page *m, int pri)
{

	return ((struct sf_buf *)m);
}

/*
 * Free the sf_buf.  In fact, do nothing because there are no resources
 * associated with the sf_buf.
 */
void
sf_buf_free(struct sf_buf *sf)
{
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
is_physical_memory(vm_paddr_t addr)
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
