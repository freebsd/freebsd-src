/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
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
#include "opt_isa.h"
#include "opt_cpu.h"

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
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/procctl.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/unistd.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <sys/wait.h>

#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/smp.h>
#include <machine/specialreg.h>
#include <machine/tss.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_param.h>

_Static_assert(OFFSETOF_MONITORBUF == offsetof(struct pcpu, pc_monitorbuf),
    "OFFSETOF_MONITORBUF does not correspond with offset of pc_monitorbuf.");

void
set_top_of_stack_td(struct thread *td)
{
	td->td_md.md_stack_base = td->td_kstack +
	    td->td_kstack_pages * PAGE_SIZE;
}

struct savefpu *
get_pcb_user_save_td(struct thread *td)
{
	KASSERT(((vm_offset_t)td->td_md.md_usr_fpu_save %
	    XSAVE_AREA_ALIGN) == 0,
	    ("Unaligned pcb_user_save area ptr %p td %p",
	    td->td_md.md_usr_fpu_save, td));
	return (td->td_md.md_usr_fpu_save);
}

struct pcb *
get_pcb_td(struct thread *td)
{

	return (&td->td_md.md_pcb);
}

struct savefpu *
get_pcb_user_save_pcb(struct pcb *pcb)
{
	struct thread *td;

	td = __containerof(pcb, struct thread, td_md.md_pcb);
	return (get_pcb_user_save_td(td));
}

void *
alloc_fpusave(int flags)
{
	void *res;
	struct savefpu_ymm *sf;

	res = malloc(cpu_max_ext_state_size, M_DEVBUF, flags);
	if (use_xsave) {
		sf = (struct savefpu_ymm *)res;
		bzero(&sf->sv_xstate.sx_hd, sizeof(sf->sv_xstate.sx_hd));
		sf->sv_xstate.sx_hd.xstate_bv = xsave_mask;
	}
	return (res);
}

/*
 * Common code shared between cpu_fork() and cpu_copy_thread() for
 * initializing a thread.
 */
static void
copy_thread(struct thread *td1, struct thread *td2)
{
	struct pcb *pcb2;

	pcb2 = td2->td_pcb;

	/* Ensure that td1's pcb is up to date for user threads. */
	if ((td2->td_pflags & TDP_KTHREAD) == 0) {
		MPASS(td1 == curthread);
		fpuexit(td1);
		update_pcb_bases(td1->td_pcb);
	}

	/* Copy td1's pcb */
	bcopy(td1->td_pcb, pcb2, sizeof(*pcb2));

	/* Properly initialize pcb_save */
	pcb2->pcb_save = get_pcb_user_save_pcb(pcb2);

	/* Kernel threads start with clean FPU and segment bases. */
	if ((td2->td_pflags & TDP_KTHREAD) != 0) {
		pcb2->pcb_fsbase = 0;
		pcb2->pcb_gsbase = 0;
		clear_pcb_flags(pcb2, PCB_FPUINITDONE | PCB_USERFPUINITDONE |
		    PCB_KERNFPU | PCB_KERNFPU_THR);
	} else {
		MPASS((pcb2->pcb_flags & (PCB_KERNFPU | PCB_KERNFPU_THR)) == 0);
		bcopy(get_pcb_user_save_td(td1), get_pcb_user_save_pcb(pcb2),
		    cpu_max_ext_state_size);
	}

	td2->td_frame = (struct trapframe *)td2->td_md.md_stack_base - 1;

	/*
	 * Set registers for trampoline to user mode.  Leave space for the
	 * return address on stack.  These are the kernel mode register values.
	 */
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

	pcb2->pcb_tssp = NULL;

	/* Setup to release spin count in fork_exit(). */
	td2->td_md.md_spinlock_count = 1;
	td2->td_md.md_saved_flags = PSL_KERNEL | PSL_I;
	pmap_thread_init_invl_gen(td2);

	/*
	 * Copy the trap frame for the return to user mode as if from a syscall.
	 * This copies most of the user mode register values.  Some of these
	 * registers are rewritten by cpu_set_upcall() and linux_set_upcall().
	 */
	if ((td1->td_proc->p_flag & P_KPROC) == 0) {
		bcopy(td1->td_frame, td2->td_frame, sizeof(struct trapframe));

		/*
		 * If the current thread has the trap bit set (i.e. a debugger
		 * had single stepped the process to the system call), we need
		 * to clear the trap flag from the new frame. Otherwise, the new
		 * thread will receive a (likely unexpected) SIGTRAP when it
		 * executes the first instruction after returning to userland.
		 */
		td2->td_frame->tf_rflags &= ~PSL_T;
	}
}

/*
 * Finish a fork operation, with process p2 nearly set up.
 * Copy and update the pcb, set up the stack so that the child
 * ready to run and return to user mode.
 */
void
cpu_fork(struct thread *td1, struct proc *p2, struct thread *td2, int flags)
{
	struct proc *p1;
	struct pcb *pcb2;
	struct mdproc *mdp1, *mdp2;
	struct proc_ldt *pldt;

	p1 = td1->td_proc;
	if ((flags & RFPROC) == 0) {
		if ((flags & RFMEM) == 0) {
			/* unshare user LDT */
			mdp1 = &p1->p_md;
			mtx_lock(&dt_lock);
			if ((pldt = mdp1->md_ldt) != NULL &&
			    pldt->ldt_refcnt > 1 &&
			    user_ldt_alloc(p1, 1) == NULL)
				panic("could not copy LDT");
			mtx_unlock(&dt_lock);
		}
		return;
	}

	/* Point the stack and pcb to the actual location */
	set_top_of_stack_td(td2);
	td2->td_pcb = pcb2 = get_pcb_td(td2);

	copy_thread(td1, td2);

	/* Reset debug registers in the new process */
	x86_clear_dbregs(pcb2);

	/* Point mdproc and then copy over p1's contents */
	mdp2 = &p2->p_md;
	bcopy(&p1->p_md, mdp2, sizeof(*mdp2));

	/* Set child return values. */
	p2->p_sysent->sv_set_fork_retval(td2);

	/* As on i386, do not copy io permission bitmap. */
	pcb2->pcb_tssp = NULL;

	/* New segment registers. */
	set_pcb_flags_raw(pcb2, PCB_FULL_IRET);

	/* Copy the LDT, if necessary. */
	mdp1 = &td1->td_proc->p_md;
	mdp2 = &p2->p_md;
	if (mdp1->md_ldt == NULL) {
		mdp2->md_ldt = NULL;
		return;
	}
	mtx_lock(&dt_lock);
	if (mdp1->md_ldt != NULL) {
		if (flags & RFMEM) {
			mdp1->md_ldt->ldt_refcnt++;
			mdp2->md_ldt = mdp1->md_ldt;
			bcopy(&mdp1->md_ldt_sd, &mdp2->md_ldt_sd, sizeof(struct
			    system_segment_descriptor));
		} else {
			mdp2->md_ldt = NULL;
			mdp2->md_ldt = user_ldt_alloc(p2, 0);
			if (mdp2->md_ldt == NULL)
				panic("could not copy LDT");
			amd64_set_ldt_data(td2, 0, max_ldt_segment,
			    (struct user_segment_descriptor *)
			    mdp1->md_ldt->ldt_base);
		}
	} else
		mdp2->md_ldt = NULL;
	mtx_unlock(&dt_lock);

	/*
	 * Now, cpu_switch() can schedule the new process.
	 * pcb_rsp is loaded pointing to the cpu_switch() stack frame
	 * containing the return address when exiting cpu_switch.
	 * This will normally be to fork_trampoline(), which will have
	 * %rbx loaded with the new proc's pointer.  fork_trampoline()
	 * will set up a stack to call fork_return(p, frame); to complete
	 * the return to user-mode.
	 */
}

void
x86_set_fork_retval(struct thread *td)
{
	struct trapframe *frame = td->td_frame;

	frame->tf_rax = 0;		/* Child returns zero */
	frame->tf_rflags &= ~PSL_C;	/* success */
	frame->tf_rdx = 1;		/* System V emulation */
}

/*
 * Intercept the return address from a freshly forked process that has NOT
 * been scheduled yet.
 *
 * This is needed to make kernel threads stay in kernel mode.
 */
void
cpu_fork_kthread_handler(struct thread *td, void (*func)(void *), void *arg)
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

	/*
	 * If this process has a custom LDT, release it.
	 */
	if (td->td_proc->p_md.md_ldt != NULL)
		user_ldt_free(td);
}

void
cpu_thread_exit(struct thread *td)
{
	struct pcb *pcb;

	critical_enter();
	if (td == PCPU_GET(fpcurthread))
		fpudrop();
	critical_exit();

	pcb = td->td_pcb;

	/* Disable any hardware breakpoints. */
	if (pcb->pcb_flags & PCB_DBREGS) {
		reset_dbregs();
		clear_pcb_flags(pcb, PCB_DBREGS);
	}
}

void
cpu_thread_clean(struct thread *td)
{
	struct pcb *pcb;

	pcb = td->td_pcb;

	/*
	 * Clean TSS/iomap
	 */
	if (pcb->pcb_tssp != NULL) {
		pmap_pti_remove_kva((vm_offset_t)pcb->pcb_tssp,
		    (vm_offset_t)pcb->pcb_tssp + ctob(IOPAGES + 1));
		kmem_free(pcb->pcb_tssp, ctob(IOPAGES + 1));
		pcb->pcb_tssp = NULL;
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
	struct pcb *pcb;
	struct xstate_hdr *xhdr;

	set_top_of_stack_td(td);
	td->td_pcb = pcb = get_pcb_td(td);
	td->td_frame = (struct trapframe *)td->td_md.md_stack_base - 1;
	td->td_md.md_usr_fpu_save = fpu_save_area_alloc();
	pcb->pcb_save = get_pcb_user_save_pcb(pcb);
	if (use_xsave) {
		xhdr = (struct xstate_hdr *)(pcb->pcb_save + 1);
		bzero(xhdr, sizeof(*xhdr));
		xhdr->xstate_bv = xsave_mask;
	}
}

void
cpu_thread_free(struct thread *td)
{
	cpu_thread_clean(td);

	fpu_save_area_free(td->td_md.md_usr_fpu_save);
	td->td_md.md_usr_fpu_save = NULL;
}

bool
cpu_exec_vmspace_reuse(struct proc *p, vm_map_t map)
{

	return (((curproc->p_md.md_flags & P_MD_KPTI) != 0) ==
	    (vm_map_pmap(map)->pm_ucr3 != PMAP_NO_CR3));
}

static void
cpu_procctl_kpti_ctl(struct proc *p, int val)
{

	if (pti && val == PROC_KPTI_CTL_ENABLE_ON_EXEC)
		p->p_md.md_flags |= P_MD_KPTI;
	if (val == PROC_KPTI_CTL_DISABLE_ON_EXEC)
		p->p_md.md_flags &= ~P_MD_KPTI;
}

static void
cpu_procctl_kpti_status(struct proc *p, int *val)
{
	*val = (p->p_md.md_flags & P_MD_KPTI) != 0 ?
	    PROC_KPTI_CTL_ENABLE_ON_EXEC:
	    PROC_KPTI_CTL_DISABLE_ON_EXEC;
	if (vmspace_pmap(p->p_vmspace)->pm_ucr3 != PMAP_NO_CR3)
		*val |= PROC_KPTI_STATUS_ACTIVE;
}

static int
cpu_procctl_la_ctl(struct proc *p, int val)
{
	int error;

	error = 0;
	switch (val) {
	case PROC_LA_CTL_LA48_ON_EXEC:
		p->p_md.md_flags |= P_MD_LA48;
		p->p_md.md_flags &= ~P_MD_LA57;
		break;
	case PROC_LA_CTL_LA57_ON_EXEC:
		if (la57) {
			p->p_md.md_flags &= ~P_MD_LA48;
			p->p_md.md_flags |= P_MD_LA57;
		} else {
			error = ENOTSUP;
		}
		break;
	case PROC_LA_CTL_DEFAULT_ON_EXEC:
		p->p_md.md_flags &= ~(P_MD_LA48 | P_MD_LA57);
		break;
	}
	return (error);
}

static void
cpu_procctl_la_status(struct proc *p, int *val)
{
	int res;

	if ((p->p_md.md_flags & P_MD_LA48) != 0)
		res = PROC_LA_CTL_LA48_ON_EXEC;
	else if ((p->p_md.md_flags & P_MD_LA57) != 0)
		res = PROC_LA_CTL_LA57_ON_EXEC;
	else
		res = PROC_LA_CTL_DEFAULT_ON_EXEC;
	if (p->p_sysent->sv_maxuser == VM_MAXUSER_ADDRESS_LA48)
		res |= PROC_LA_STATUS_LA48;
	else
		res |= PROC_LA_STATUS_LA57;
	*val = res;
}

int
cpu_procctl(struct thread *td, int idtype, id_t id, int com, void *data)
{
	struct proc *p;
	int error, val;

	switch (com) {
	case PROC_KPTI_CTL:
	case PROC_KPTI_STATUS:
	case PROC_LA_CTL:
	case PROC_LA_STATUS:
		if (idtype != P_PID) {
			error = EINVAL;
			break;
		}
		if (com == PROC_KPTI_CTL) {
			/* sad but true and not a joke */
			error = priv_check(td, PRIV_IO);
			if (error != 0)
				break;
		}
		if (com == PROC_KPTI_CTL || com == PROC_LA_CTL) {
			error = copyin(data, &val, sizeof(val));
			if (error != 0)
				break;
		}
		if (com == PROC_KPTI_CTL &&
		    val != PROC_KPTI_CTL_ENABLE_ON_EXEC &&
		    val != PROC_KPTI_CTL_DISABLE_ON_EXEC) {
			error = EINVAL;
			break;
		}
		if (com == PROC_LA_CTL &&
		    val != PROC_LA_CTL_LA48_ON_EXEC &&
		    val != PROC_LA_CTL_LA57_ON_EXEC &&
		    val != PROC_LA_CTL_DEFAULT_ON_EXEC) {
			error = EINVAL;
			break;
		}
		error = pget(id, PGET_CANSEE | PGET_NOTWEXIT | PGET_NOTID, &p);
		if (error != 0)
			break;
		switch (com) {
		case PROC_KPTI_CTL:
			cpu_procctl_kpti_ctl(p, val);
			break;
		case PROC_KPTI_STATUS:
			cpu_procctl_kpti_status(p, &val);
			break;
		case PROC_LA_CTL:
			error = cpu_procctl_la_ctl(p, val);
			break;
		case PROC_LA_STATUS:
			cpu_procctl_la_status(p, &val);
			break;
		}
		PROC_UNLOCK(p);
		if (com == PROC_KPTI_STATUS || com == PROC_LA_STATUS)
			error = copyout(&val, data, sizeof(val));
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}

void
cpu_set_syscall_retval(struct thread *td, int error)
{
	struct trapframe *frame;

	frame = td->td_frame;
	if (__predict_true(error == 0)) {
		frame->tf_rax = td->td_retval[0];
		frame->tf_rdx = td->td_retval[1];
		frame->tf_rflags &= ~PSL_C;
		return;
	}

	switch (error) {
	case ERESTART:
		/*
		 * Reconstruct pc, we know that 'syscall' is 2 bytes,
		 * lcall $X,y is 7 bytes, int 0x80 is 2 bytes.
		 * We saved this in tf_err.
		 * %r10 (which was holding the value of %rcx) is restored
		 * for the next iteration.
		 * %r10 restore is only required for freebsd/amd64 processes,
		 * but shall be innocent for any ia32 ABI.
		 *
		 * Require full context restore to get the arguments
		 * in the registers reloaded at return to usermode.
		 */
		frame->tf_rip -= frame->tf_err;
		frame->tf_r10 = frame->tf_rcx;
		set_pcb_flags(td->td_pcb, PCB_FULL_IRET);
		break;

	case EJUSTRETURN:
		break;

	default:
		frame->tf_rax = error;
		frame->tf_rflags |= PSL_C;
		break;
	}
}

/*
 * Initialize machine state, mostly pcb and trap frame for a new
 * thread, about to return to userspace.  Put enough state in the new
 * thread's PCB to get it to go back to the fork_return(), which
 * finalizes the thread state and handles peculiarities of the first
 * return to userspace for the new thread.
 */
void
cpu_copy_thread(struct thread *td, struct thread *td0)
{
	copy_thread(td0, td);

	set_pcb_flags_raw(td->td_pcb, PCB_FULL_IRET);
}

/*
 * Set that machine state for performing an upcall that starts
 * the entry function with the given argument.
 */
void
cpu_set_upcall(struct thread *td, void (*entry)(void *), void *arg,
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

#ifdef COMPAT_FREEBSD32
	if (SV_PROC_FLAG(td->td_proc, SV_ILP32)) {
		/*
		 * Set the trap frame to point at the beginning of the entry
		 * function.
		 */
		td->td_frame->tf_rbp = 0;
		td->td_frame->tf_rsp =
		   (((uintptr_t)stack->ss_sp + stack->ss_size - 4) & ~0x0f) - 4;
		td->td_frame->tf_rip = (uintptr_t)entry;

		/* Return address sentinel value to stop stack unwinding. */
		suword32((void *)td->td_frame->tf_rsp, 0);

		/* Pass the argument to the entry point. */
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
	td->td_frame->tf_ds = _udatasel;
	td->td_frame->tf_es = _udatasel;
	td->td_frame->tf_fs = _ufssel;
	td->td_frame->tf_gs = _ugssel;
	td->td_frame->tf_flags = TF_HASSEGS;

	/* Return address sentinel value to stop stack unwinding. */
	suword((void *)td->td_frame->tf_rsp, 0);

	/* Pass the argument to the entry point. */
	td->td_frame->tf_rdi = (register_t)arg;
}

int
cpu_set_user_tls(struct thread *td, void *tls_base)
{
	struct pcb *pcb;

	if ((u_int64_t)tls_base >= VM_MAXUSER_ADDRESS)
		return (EINVAL);

	pcb = td->td_pcb;
	set_pcb_flags(pcb, PCB_FULL_IRET);
#ifdef COMPAT_FREEBSD32
	if (SV_PROC_FLAG(td->td_proc, SV_ILP32)) {
		pcb->pcb_gsbase = (register_t)tls_base;
		return (0);
	}
#endif
	pcb->pcb_fsbase = (register_t)tls_base;
	return (0);
}
