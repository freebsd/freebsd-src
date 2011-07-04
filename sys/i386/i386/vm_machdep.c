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
#include "opt_npx.h"
#include "opt_reset.h"
#include "opt_cpu.h"
#include "opt_xbox.h"

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
#include <sys/sysent.h>
#include <sys/sf_buf.h>
#include <sys/smp.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>

#include <machine/cpu.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/pcb_ext.h>
#include <machine/smp.h>
#include <machine/vm86.h>

#ifdef CPU_ELAN
#include <machine/elan_mmcr.h>
#endif

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>

#ifdef XEN
#include <xen/hypervisor.h>
#endif
#ifdef PC98
#include <pc98/cbus/cbus.h>
#else
#include <x86/isa/isa.h>
#endif

#ifdef XBOX
#include <machine/xbox.h>
#endif

#ifndef NSFBUFS
#define	NSFBUFS		(512 + maxusers * 16)
#endif

static void	cpu_reset_real(void);
#ifdef SMP
static void	cpu_reset_proxy(void);
static u_int	cpu_reset_proxyid;
static volatile u_int	cpu_reset_proxy_active;
#endif
static void	sf_buf_init(void *arg);
SYSINIT(sock_sf, SI_SUB_MBUF, SI_ORDER_ANY, sf_buf_init, NULL);

LIST_HEAD(sf_head, sf_buf);

/*
 * A hash table of active sendfile(2) buffers
 */
static struct sf_head *sf_buf_active;
static u_long sf_buf_hashmask;

#define	SF_BUF_HASH(m)	(((m) - vm_page_array) & sf_buf_hashmask)

static TAILQ_HEAD(, sf_buf) sf_buf_freelist;
static u_int	sf_buf_alloc_want;

/*
 * A lock used to synchronize access to the hash table and free list
 */
static struct mtx sf_buf_lock;

extern int	_ucodesel, _udatasel;

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
	if ((flags & RFPROC) == 0) {
		if ((flags & RFMEM) == 0) {
			/* unshare user LDT */
			struct mdproc *mdp1 = &p1->p_md;
			struct proc_ldt *pldt, *pldt1;

			mtx_lock_spin(&dt_lock);
			if ((pldt1 = mdp1->md_ldt) != NULL &&
			    pldt1->ldt_refcnt > 1) {
				pldt = user_ldt_alloc(mdp1, pldt1->ldt_len);
				if (pldt == NULL)
					panic("could not copy LDT");
				mdp1->md_ldt = pldt;
				set_user_ldt(mdp1);
				user_ldt_deref(pldt1);
			} else
				mtx_unlock_spin(&dt_lock);
		}
		return;
	}

	/* Ensure that td1's pcb is up to date. */
	if (td1 == curthread)
		td1->td_pcb->pcb_gs = rgs();
#ifdef DEV_NPX
	critical_enter();
	if (PCPU_GET(fpcurthread) == td1)
		npxsave(td1->td_pcb->pcb_save);
	critical_exit();
#endif

	/* Point the pcb to the top of the stack */
	pcb2 = (struct pcb *)(td2->td_kstack +
	    td2->td_kstack_pages * PAGE_SIZE) - 1;
	td2->td_pcb = pcb2;

	/* Copy td1's pcb */
	bcopy(td1->td_pcb, pcb2, sizeof(*pcb2));

	/* Properly initialize pcb_save */
	pcb2->pcb_save = &pcb2->pcb_user_save;

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
	 * If the parent process has the trap bit set (i.e. a debugger had
	 * single stepped the process to the system call), we need to clear
	 * the trap flag from the new frame unless the debugger had set PF_FORK
	 * on the parent.  Otherwise, the child will receive a (likely
	 * unexpected) SIGTRAP when it executes the first instruction after
	 * returning  to userland.
	 */
	if ((p1->p_pfsflags & PF_FORK) == 0)
		td2->td_frame->tf_eflags &= ~PSL_T;

	/*
	 * Set registers for trampoline to user mode.  Leave space for the
	 * return address on stack.  These are the kernel mode register values.
	 */
#ifdef PAE
	pcb2->pcb_cr3 = vtophys(vmspace_pmap(p2->p_vmspace)->pm_pdpt);
#else
	pcb2->pcb_cr3 = vtophys(vmspace_pmap(p2->p_vmspace)->pm_pdir);
#endif
	pcb2->pcb_edi = 0;
	pcb2->pcb_esi = (int)fork_return;	/* fork_trampoline argument */
	pcb2->pcb_ebp = 0;
	pcb2->pcb_esp = (int)td2->td_frame - sizeof(void *);
	pcb2->pcb_ebx = (int)td2;		/* fork_trampoline argument */
	pcb2->pcb_eip = (int)fork_trampoline;
	pcb2->pcb_psl = PSL_KERNEL;		/* ints disabled */
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
	mtx_lock_spin(&dt_lock);
	if (mdp2->md_ldt != NULL) {
		if (flags & RFMEM) {
			mdp2->md_ldt->ldt_refcnt++;
		} else {
			mdp2->md_ldt = user_ldt_alloc(mdp2,
			    mdp2->md_ldt->ldt_len);
			if (mdp2->md_ldt == NULL)
				panic("could not copy LDT");
		}
	}
	mtx_unlock_spin(&dt_lock);

	/* Setup to release spin count in fork_exit(). */
	td2->td_md.md_spinlock_count = 1;
	/*
	 * XXX XEN need to check on PSL_USER is handled
	 */
	td2->td_md.md_saved_flags = PSL_KERNEL | PSL_I;
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

	/*
	 * If this process has a custom LDT, release it.  Reset pc->pcb_gs
	 * and %gs before we free it in case they refer to an LDT entry.
	 */
	mtx_lock_spin(&dt_lock);
	if (td->td_proc->p_md.md_ldt) {
		td->td_pcb->pcb_gs = _udatasel;
		load_gs(_udatasel);
		user_ldt_free(td);
	} else
		mtx_unlock_spin(&dt_lock);
}

void
cpu_thread_exit(struct thread *td)
{

#ifdef DEV_NPX
	critical_enter();
	if (td == PCPU_GET(fpcurthread))
		npxdrop();
	critical_exit();
#endif

	/* Disable any hardware breakpoints. */
	if (td->td_pcb->pcb_flags & PCB_DBREGS) {
		reset_dbregs();
		td->td_pcb->pcb_flags &= ~PCB_DBREGS;
	}
}

void
cpu_thread_clean(struct thread *td)
{
	struct pcb *pcb;

	pcb = td->td_pcb; 
	if (pcb->pcb_ext != NULL) {
		/* if (pcb->pcb_ext->ext_refcount-- == 1) ?? */
		/*
		 * XXX do we need to move the TSS off the allocated pages
		 * before freeing them?  (not done here)
		 */
		kmem_free(kernel_map, (vm_offset_t)pcb->pcb_ext,
		    ctob(IOPAGES + 1));
		pcb->pcb_ext = NULL;
	}
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
	td->td_frame = (struct trapframe *)((caddr_t)td->td_pcb - 16) - 1;
	td->td_pcb->pcb_ext = NULL; 
	td->td_pcb->pcb_save = &td->td_pcb->pcb_user_save;
}

void
cpu_thread_free(struct thread *td)
{

	cpu_thread_clean(td);
}

void
cpu_set_syscall_retval(struct thread *td, int error)
{

	switch (error) {
	case 0:
		td->td_frame->tf_eax = td->td_retval[0];
		td->td_frame->tf_edx = td->td_retval[1];
		td->td_frame->tf_eflags &= ~PSL_C;
		break;

	case ERESTART:
		/*
		 * Reconstruct pc, assuming lcall $X,y is 7 bytes, int
		 * 0x80 is 2 bytes. We saved this in tf_err.
		 */
		td->td_frame->tf_eip -= td->td_frame->tf_err;
		break;

	case EJUSTRETURN:
		break;

	default:
		if (td->td_proc->p_sysent->sv_errsize) {
			if (error >= td->td_proc->p_sysent->sv_errsize)
				error = -1;	/* XXX */
			else
				error = td->td_proc->p_sysent->sv_errtbl[error];
		}
		td->td_frame->tf_eax = error;
		td->td_frame->tf_eflags |= PSL_C;
		break;
	}
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
	pcb2->pcb_flags &= ~(PCB_NPXINITDONE | PCB_NPXUSERINITDONE);
	pcb2->pcb_save = &pcb2->pcb_user_save;

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
	td->td_frame->tf_eflags &= ~PSL_T;

	/*
	 * Set registers for trampoline to user mode.  Leave space for the
	 * return address on stack.  These are the kernel mode register values.
	 */
	pcb2->pcb_edi = 0;
	pcb2->pcb_esi = (int)fork_return;		    /* trampoline arg */
	pcb2->pcb_ebp = 0;
	pcb2->pcb_esp = (int)td->td_frame - sizeof(void *); /* trampoline arg */
	pcb2->pcb_ebx = (int)td;			    /* trampoline arg */
	pcb2->pcb_eip = (int)fork_trampoline;
	pcb2->pcb_psl &= ~(PSL_I);	/* interrupts must be disabled */
	pcb2->pcb_gs = rgs();
	/*
	 * If we didn't copy the pcb, we'd need to do the following registers:
	 * pcb2->pcb_cr3:	cloned above.
	 * pcb2->pcb_dr*:	cloned above.
	 * pcb2->pcb_savefpu:	cloned above.
	 * pcb2->pcb_flags:	cloned above.
	 * pcb2->pcb_onfault:	cloned above (always NULL here?).
	 * pcb2->pcb_gs:	cloned above.
	 * pcb2->pcb_ext:	cleared below.
	 */
	pcb2->pcb_ext = NULL;

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

	/*
	 * Set the trap frame to point at the beginning of the uts
	 * function.
	 */
	td->td_frame->tf_ebp = 0; 
	td->td_frame->tf_esp =
	    (((int)stack->ss_sp + stack->ss_size - 4) & ~0x0f) - 4;
	td->td_frame->tf_eip = (int)entry;

	/*
	 * Pass the address of the mailbox for this kse to the uts
	 * function as a parameter on the stack.
	 */
	suword((void *)(td->td_frame->tf_esp + sizeof(void *)),
	    (int)arg);
}

int
cpu_set_user_tls(struct thread *td, void *tls_base)
{
	struct segment_descriptor sd;
	uint32_t base;

	/*
	 * Construct a descriptor and store it in the pcb for
	 * the next context switch.  Also store it in the gdt
	 * so that the load of tf_fs into %fs will activate it
	 * at return to userland.
	 */
	base = (uint32_t)tls_base;
	sd.sd_lobase = base & 0xffffff;
	sd.sd_hibase = (base >> 24) & 0xff;
	sd.sd_lolimit = 0xffff;	/* 4GB limit, wraps around */
	sd.sd_hilimit = 0xf;
	sd.sd_type  = SDT_MEMRWA;
	sd.sd_dpl   = SEL_UPL;
	sd.sd_p     = 1;
	sd.sd_xx    = 0;
	sd.sd_def32 = 1;
	sd.sd_gran  = 1;
	critical_enter();
	/* set %gs */
	td->td_pcb->pcb_gsd = sd;
	if (td == curthread) {
		PCPU_GET(fsgs_gdt)[1] = sd;
		load_gs(GSEL(GUGS_SEL, SEL_UPL));
	}
	critical_exit();
	return (0);
}

/*
 * Convert kernel VA to physical address
 */
vm_paddr_t
kvtop(void *addr)
{
	vm_paddr_t pa;

	pa = pmap_kextract((vm_offset_t)addr);
	if (pa == 0)
		panic("kvtop: zero page frame");
	return (pa);
}

#ifdef SMP
static void
cpu_reset_proxy()
{
	cpuset_t tcrp;

	cpu_reset_proxy_active = 1;
	while (cpu_reset_proxy_active == 1)
		;	/* Wait for other cpu to see that we've started */
	CPU_SETOF(cpu_reset_proxyid, &tcrp);
	stop_cpus(tcrp);
	printf("cpu_reset_proxy: Stopped CPU %d\n", cpu_reset_proxyid);
	DELAY(1000000);
	cpu_reset_real();
}
#endif

void
cpu_reset()
{
#ifdef XBOX
	if (arch_i386_is_xbox) {
		/* Kick the PIC16L, it can reboot the box */
		pic16l_reboot();
		for (;;);
	}
#endif

#ifdef SMP
	cpuset_t map;
	u_int cnt;

	if (smp_active) {
		map = all_cpus;
		CPU_CLR(PCPU_GET(cpuid), &map);
		CPU_NAND(&map, &stopped_cpus);
		if (!CPU_EMPTY(&map)) {
			printf("cpu_reset: Stopping other CPUs\n");
			stop_cpus(map);
		}

		if (PCPU_GET(cpuid) != 0) {
			cpu_reset_proxyid = PCPU_GET(cpuid);
			cpustop_restartfunc = cpu_reset_proxy;
			cpu_reset_proxy_active = 0;
			printf("cpu_reset: Restarting BSP\n");

			/* Restart CPU #0. */
			/* XXX: restart_cpus(1 << 0); */
			CPU_SETOF(0, &started_cpus);
			wmb();

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
#ifndef PC98
	int b;
#endif

	disable_intr();
#ifdef XEN
	if (smp_processor_id() == 0)
		HYPERVISOR_shutdown(SHUTDOWN_reboot);
	else
		HYPERVISOR_shutdown(SHUTDOWN_poweroff);
#endif 
#ifdef CPU_ELAN
	if (elan_mmcr != NULL)
		elan_mmcr->RESCFG = 1;
#endif

	if (cpu == CPU_GEODE1100) {
		/* Attempt Geode's own reset */
		outl(0xcf8, 0x80009044ul);
		outl(0xcfc, 0xf);
	}

#ifdef PC98
	/*
	 * Attempt to do a CPU reset via CPU reset port.
	 */
	if ((inb(0x35) & 0xa0) != 0xa0) {
		outb(0x37, 0x0f);		/* SHUT0 = 0. */
		outb(0x37, 0x0b);		/* SHUT1 = 0. */
	}
	outb(0xf0, 0x00);		/* Reset. */
#else
#if !defined(BROKEN_KEYBOARD_RESET)
	/*
	 * Attempt to do a CPU reset via the keyboard controller,
	 * do not turn off GateA20, as any machine that fails
	 * to do the reset here would then end up in no man's land.
	 */
	outb(IO_KBD + 4, 0xFE);
	DELAY(500000);	/* wait 0.5 sec to see if that did it */
#endif

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
#endif /* PC98 */

	printf("No known reset method worked, attempting CPU shutdown\n");
	DELAY(1000000); /* wait 1 sec for printf to complete */

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
 * Allocate a pool of sf_bufs (sendfile(2) or "super-fast" if you prefer. :-))
 */
static void
sf_buf_init(void *arg)
{
	struct sf_buf *sf_bufs;
	vm_offset_t sf_base;
	int i;

	nsfbufs = NSFBUFS;
	TUNABLE_INT_FETCH("kern.ipc.nsfbufs", &nsfbufs);

	sf_buf_active = hashinit(nsfbufs, M_TEMP, &sf_buf_hashmask);
	TAILQ_INIT(&sf_buf_freelist);
	sf_base = kmem_alloc_nofault(kernel_map, nsfbufs * PAGE_SIZE);
	sf_bufs = malloc(nsfbufs * sizeof(struct sf_buf), M_TEMP,
	    M_NOWAIT | M_ZERO);
	for (i = 0; i < nsfbufs; i++) {
		sf_bufs[i].kva = sf_base + i * PAGE_SIZE;
		TAILQ_INSERT_TAIL(&sf_buf_freelist, &sf_bufs[i], free_entry);
	}
	sf_buf_alloc_want = 0;
	mtx_init(&sf_buf_lock, "sf_buf", NULL, MTX_DEF);
}

/*
 * Invalidate the cache lines that may belong to the page, if
 * (possibly old) mapping of the page by sf buffer exists.  Returns
 * TRUE when mapping was found and cache invalidated.
 */
boolean_t
sf_buf_invalidate_cache(vm_page_t m)
{
	struct sf_head *hash_list;
	struct sf_buf *sf;
	boolean_t ret;

	hash_list = &sf_buf_active[SF_BUF_HASH(m)];
	ret = FALSE;
	mtx_lock(&sf_buf_lock);
	LIST_FOREACH(sf, hash_list, list_entry) {
		if (sf->m == m) {
			/*
			 * Use pmap_qenter to update the pte for
			 * existing mapping, in particular, the PAT
			 * settings are recalculated.
			 */
			pmap_qenter(sf->kva, &m, 1);
			pmap_invalidate_cache_range(sf->kva, sf->kva +
			    PAGE_SIZE);
			ret = TRUE;
			break;
		}
	}
	mtx_unlock(&sf_buf_lock);
	return (ret);
}

/*
 * Get an sf_buf from the freelist.  May block if none are available.
 */
struct sf_buf *
sf_buf_alloc(struct vm_page *m, int flags)
{
	pt_entry_t opte, *ptep;
	struct sf_head *hash_list;
	struct sf_buf *sf;
#ifdef SMP
	cpuset_t other_cpus;
	u_int cpuid;
#endif
	int error;

	KASSERT(curthread->td_pinned > 0 || (flags & SFB_CPUPRIVATE) == 0,
	    ("sf_buf_alloc(SFB_CPUPRIVATE): curthread not pinned"));
	hash_list = &sf_buf_active[SF_BUF_HASH(m)];
	mtx_lock(&sf_buf_lock);
	LIST_FOREACH(sf, hash_list, list_entry) {
		if (sf->m == m) {
			sf->ref_count++;
			if (sf->ref_count == 1) {
				TAILQ_REMOVE(&sf_buf_freelist, sf, free_entry);
				nsfbufsused++;
				nsfbufspeak = imax(nsfbufspeak, nsfbufsused);
			}
#ifdef SMP
			goto shootdown;	
#else
			goto done;
#endif
		}
	}
	while ((sf = TAILQ_FIRST(&sf_buf_freelist)) == NULL) {
		if (flags & SFB_NOWAIT)
			goto done;
		sf_buf_alloc_want++;
		mbstat.sf_allocwait++;
		error = msleep(&sf_buf_freelist, &sf_buf_lock,
		    (flags & SFB_CATCH) ? PCATCH | PVM : PVM, "sfbufa", 0);
		sf_buf_alloc_want--;

		/*
		 * If we got a signal, don't risk going back to sleep. 
		 */
		if (error)
			goto done;
	}
	TAILQ_REMOVE(&sf_buf_freelist, sf, free_entry);
	if (sf->m != NULL)
		LIST_REMOVE(sf, list_entry);
	LIST_INSERT_HEAD(hash_list, sf, list_entry);
	sf->ref_count = 1;
	sf->m = m;
	nsfbufsused++;
	nsfbufspeak = imax(nsfbufspeak, nsfbufsused);

	/*
	 * Update the sf_buf's virtual-to-physical mapping, flushing the
	 * virtual address from the TLB.  Since the reference count for 
	 * the sf_buf's old mapping was zero, that mapping is not 
	 * currently in use.  Consequently, there is no need to exchange 
	 * the old and new PTEs atomically, even under PAE.
	 */
	ptep = vtopte(sf->kva);
	opte = *ptep;
#ifdef XEN
       PT_SET_MA(sf->kva, xpmap_ptom(VM_PAGE_TO_PHYS(m)) | pgeflag
	   | PG_RW | PG_V | pmap_cache_bits(m->md.pat_mode, 0));
#else
	*ptep = VM_PAGE_TO_PHYS(m) | pgeflag | PG_RW | PG_V |
	    pmap_cache_bits(m->md.pat_mode, 0);
#endif

	/*
	 * Avoid unnecessary TLB invalidations: If the sf_buf's old
	 * virtual-to-physical mapping was not used, then any processor
	 * that has invalidated the sf_buf's virtual address from its TLB
	 * since the last used mapping need not invalidate again.
	 */
#ifdef SMP
	if ((opte & (PG_V | PG_A)) ==  (PG_V | PG_A))
		CPU_ZERO(&sf->cpumask);
shootdown:
	sched_pin();
	cpuid = PCPU_GET(cpuid);
	if (!CPU_ISSET(cpuid, &sf->cpumask)) {
		CPU_SET(cpuid, &sf->cpumask);
		invlpg(sf->kva);
	}
	if ((flags & SFB_CPUPRIVATE) == 0) {
		other_cpus = all_cpus;
		CPU_CLR(cpuid, &other_cpus);
		CPU_NAND(&other_cpus, &sf->cpumask);
		if (!CPU_EMPTY(&other_cpus)) {
			CPU_OR(&sf->cpumask, &other_cpus);
			smp_masked_invlpg(other_cpus, sf->kva);
		}
	}
	sched_unpin();
#else
	if ((opte & (PG_V | PG_A)) ==  (PG_V | PG_A))
		pmap_invalidate_page(kernel_pmap, sf->kva);
#endif
done:
	mtx_unlock(&sf_buf_lock);
	return (sf);
}

/*
 * Remove a reference from the given sf_buf, adding it to the free
 * list when its reference count reaches zero.  A freed sf_buf still,
 * however, retains its virtual-to-physical mapping until it is
 * recycled or reactivated by sf_buf_alloc(9).
 */
void
sf_buf_free(struct sf_buf *sf)
{

	mtx_lock(&sf_buf_lock);
	sf->ref_count--;
	if (sf->ref_count == 0) {
		TAILQ_INSERT_TAIL(&sf_buf_freelist, sf, free_entry);
		nsfbufsused--;
#ifdef XEN
/*
 * Xen doesn't like having dangling R/W mappings
 */
		pmap_qremove(sf->kva, 1);
		sf->m = NULL;
		LIST_REMOVE(sf, list_entry);
#endif
		if (sf_buf_alloc_want > 0)
			wakeup(&sf_buf_freelist);
	}
	mtx_unlock(&sf_buf_lock);
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
