/*	$NetBSD: arm32_machdep.c,v 1.44 2004/03/24 15:34:47 atatat Exp $	*/

/*-
 * Copyright (c) 2004 Olivier Houchard
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Machine dependant functions for kernel setup
 *
 * Created      : 17/09/94
 * Updated	: 18/04/01 updated for new wscons
 */

#include "opt_compat.h"
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/cpu.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/ptrace.h>
#include <sys/signalvar.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vnode_pager.h>

#include <machine/armreg.h>
#include <machine/cpu.h>
#include <machine/machdep.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/pcb.h>
#include <machine/pmap.h>
#include <machine/reg.h>
#include <machine/trap.h>
#include <machine/undefined.h>
#include <machine/vmparam.h>
#include <machine/sysarch.h>

uint32_t cpu_reset_address = 0;
int cold = 1;
vm_offset_t vector_page;

long realmem = 0;

int (*_arm_memcpy)(void *, void *, int, int) = NULL;
int (*_arm_bzero)(void *, int, int) = NULL;
int _min_memcpy_size = 0;
int _min_bzero_size = 0;

void
sendsig(catcher, ksi, mask)
	sig_t catcher;
	ksiginfo_t *ksi;
	sigset_t *mask;
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct trapframe *tf = td->td_frame;
	struct sigframe *fp, frame;
	struct sigacts *psp = td->td_proc->p_sigacts;
	int onstack;
	int sig;
	int code;

	onstack = sigonstack(td->td_frame->tf_usr_sp);

	sig = ksi->ksi_signo;
	code = ksi->ksi_code;
	if ((td->td_flags & TDP_ALTSTACK) &&
	    !(onstack) &&
	    SIGISMEMBER(td->td_proc->p_sigacts->ps_sigonstack, sig)) {
		fp = (void*)(td->td_sigstk.ss_sp + td->td_sigstk.ss_size);
		td->td_sigstk.ss_flags |= SS_ONSTACK;
	} else
		fp = (void*)td->td_frame->tf_usr_sp;
		 
	/* make room on the stack */
	fp--;
	
	/* make the stack aligned */
	fp = (struct sigframe *)STACKALIGN(fp);
	/* Populate the siginfo frame. */
	frame.sf_si = ksi->ksi_info;
	frame.sf_uc.uc_sigmask = *mask;
	frame.sf_uc.uc_link = NULL;
	frame.sf_uc.uc_flags = (td->td_pflags & TDP_ALTSTACK ) 
	    ? ((onstack) ? SS_ONSTACK : 0) : SS_DISABLE;
	frame.sf_uc.uc_stack = td->td_sigstk;
	memset(&frame.sf_uc.uc_stack, 0, sizeof(frame.sf_uc.uc_stack));
	get_mcontext(td, &frame.sf_uc.uc_mcontext, 0);
	PROC_UNLOCK(td->td_proc);
	mtx_unlock(&psp->ps_mtx);
	if (copyout(&frame, (void*)fp, sizeof(frame)) != 0)
		sigexit(td, SIGILL);
	/*
	 * Build context to run handler in.  We invoke the handler
	 * directly, only returning via the trampoline.  Note the
	 * trampoline version numbers are coordinated with machine-
	 * dependent code in libc.
	 */
	
	tf->tf_r0 = sig;
	tf->tf_r1 = (int)&fp->sf_si;
	tf->tf_r2 = (int)&fp->sf_uc;

	/* the trampoline uses r5 as the uc address */
	tf->tf_r5 = (int)&fp->sf_uc;
	tf->tf_pc = (int)catcher;
	tf->tf_usr_sp = (int)fp;
	tf->tf_usr_lr = (int)(PS_STRINGS - *(p->p_sysent->sv_szsigcode));
	PROC_LOCK(td->td_proc);
	mtx_lock(&psp->ps_mtx);
}

struct kva_md_info kmi;

/*
 * arm32_vector_init:
 *
 *	Initialize the vector page, and select whether or not to
 *	relocate the vectors.
 *
 *	NOTE: We expect the vector page to be mapped at its expected
 *	destination.
 */

extern unsigned int page0[], page0_data[];
void
arm_vector_init(vm_offset_t va, int which)
{
	unsigned int *vectors = (int *) va;
	unsigned int *vectors_data = vectors + (page0_data - page0);
	int vec;

	/*
	 * Loop through the vectors we're taking over, and copy the
	 * vector's insn and data word.
	 */
	for (vec = 0; vec < ARM_NVEC; vec++) {
		if ((which & (1 << vec)) == 0) {
			/* Don't want to take over this vector. */
			continue;
		}
		vectors[vec] = page0[vec];
		vectors_data[vec] = page0_data[vec];
	}

	/* Now sync the vectors. */
	cpu_icache_sync_range(va, (ARM_NVEC * 2) * sizeof(u_int));

	vector_page = va;

	if (va == ARM_VECTORS_HIGH) {
		/*
		 * Assume the MD caller knows what it's doing here, and
		 * really does want the vector page relocated.
		 *
		 * Note: This has to be done here (and not just in
		 * cpu_setup()) because the vector page needs to be
		 * accessible *before* cpu_startup() is called.
		 * Think ddb(9) ...
		 *
		 * NOTE: If the CPU control register is not readable,
		 * this will totally fail!  We'll just assume that
		 * any system that has high vector support has a
		 * readable CPU control register, for now.  If we
		 * ever encounter one that does not, we'll have to
		 * rethink this.
		 */
		cpu_control(CPU_CONTROL_VECRELOC, CPU_CONTROL_VECRELOC);
	}
}

static void
cpu_startup(void *dummy)
{
	struct pcb *pcb = thread0.td_pcb;
#ifndef ARM_CACHE_LOCK_ENABLE
	vm_page_t m;
#endif

	vm_ksubmap_init(&kmi);
	bufinit();
	vm_pager_bufferinit();
	pcb->un_32.pcb32_und_sp = (u_int)thread0.td_kstack +
	    USPACE_UNDEF_STACK_TOP;
	pcb->un_32.pcb32_sp = (u_int)thread0.td_kstack +
	    USPACE_SVC_STACK_TOP;
	vector_page_setprot(VM_PROT_READ);
	pmap_set_pcb_pagedir(pmap_kernel(), pcb);
	cpu_setup("");
	identify_arm_cpu();
	thread0.td_frame = (struct trapframe *)pcb->un_32.pcb32_sp - 1;
	pmap_postinit();
#ifdef ARM_CACHE_LOCK_ENABLE
	pmap_kenter_user(ARM_TP_ADDRESS, ARM_TP_ADDRESS);
	arm_lock_cache_line(ARM_TP_ADDRESS);
#else
	m = vm_page_alloc(NULL, 0, VM_ALLOC_NOOBJ | VM_ALLOC_ZERO);
	pmap_kenter_user(ARM_TP_ADDRESS, VM_PAGE_TO_PHYS(m));
#endif
	realmem = physmem;
	
}

SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL)

/* Get current clock frequency for the given cpu id. */
int
cpu_est_clockrate(int cpu_id, uint64_t *rate)
{

	return (ENXIO);
}

void
cpu_idle(void)
{
	cpu_sleep(0);
}

int
fill_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *tf = td->td_frame;
	bcopy(&tf->tf_r0, regs->r, sizeof(regs->r));
	regs->r_sp = tf->tf_usr_sp;
	regs->r_lr = tf->tf_usr_lr;
	regs->r_pc = tf->tf_pc;
	regs->r_cpsr = tf->tf_spsr;
	return (0);
}
int
fill_fpregs(struct thread *td, struct fpreg *regs)
{
	bzero(regs, sizeof(*regs));
	return (0);
}

int
set_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *tf = td->td_frame;
	
	bcopy(regs->r, &tf->tf_r0, sizeof(regs->r));
	tf->tf_usr_sp = regs->r_sp;
	tf->tf_usr_lr = regs->r_lr;
	tf->tf_pc = regs->r_pc;
	tf->tf_spsr &=  ~PSR_FLAGS;
	tf->tf_spsr |= regs->r_cpsr & PSR_FLAGS;
	return (0);								
}

int
set_fpregs(struct thread *td, struct fpreg *regs)
{
	return (0);
}

int
fill_dbregs(struct thread *td, struct dbreg *regs)
{
	return (0);
}
int
set_dbregs(struct thread *td, struct dbreg *regs)
{
	return (0);
}


static int
ptrace_read_int(struct thread *td, vm_offset_t addr, u_int32_t *v)
{
	struct iovec iov;
	struct uio uio;
	iov.iov_base = (caddr_t) v;
	iov.iov_len = sizeof(u_int32_t);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)addr;
	uio.uio_resid = sizeof(u_int32_t);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_td = td;
	return proc_rwmem(td->td_proc, &uio);
}

static int
ptrace_write_int(struct thread *td, vm_offset_t addr, u_int32_t v)
{
	struct iovec iov;
	struct uio uio;
	iov.iov_base = (caddr_t) &v;
	iov.iov_len = sizeof(u_int32_t);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)addr;
	uio.uio_resid = sizeof(u_int32_t);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	uio.uio_td = td;
	return proc_rwmem(td->td_proc, &uio);
}

int
ptrace_single_step(struct thread *td)
{
	int error;
	
	KASSERT(td->td_md.md_ptrace_instr == 0,
	 ("Didn't clear single step"));
	error = ptrace_read_int(td, td->td_frame->tf_pc + 4, 
	    &td->td_md.md_ptrace_instr);
	if (error)
		return (error);
	error = ptrace_write_int(td, td->td_frame->tf_pc + 4,
	    PTRACE_BREAKPOINT);
	if (error)
		td->td_md.md_ptrace_instr = 0;
	td->td_md.md_ptrace_addr = td->td_frame->tf_pc + 4;
	return (error);
}

int
ptrace_clear_single_step(struct thread *td)
{
	if (td->td_md.md_ptrace_instr) {
		ptrace_write_int(td, td->td_md.md_ptrace_addr,
		    td->td_md.md_ptrace_instr);
		td->td_md.md_ptrace_instr = 0;
	}
	return (0);
}

int
ptrace_set_pc(struct thread *td, unsigned long addr)
{
	td->td_frame->tf_pc = addr;
	return (0);
}

void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t size)
{
}

void
spinlock_enter(void)
{
	struct thread *td;

	td = curthread;
	if (td->td_md.md_spinlock_count == 0)
		td->td_md.md_saved_cspr = disable_interrupts(I32_bit | F32_bit);
	td->td_md.md_spinlock_count++;
	critical_enter();
}

void
spinlock_exit(void)
{
	struct thread *td;

	td = curthread;
	critical_exit();
	td->td_md.md_spinlock_count--;
	if (td->td_md.md_spinlock_count == 0)
		restore_interrupts(td->td_md.md_saved_cspr);
}

/*
 * Clear registers on exec
 */
void
exec_setregs(struct thread *td, u_long entry, u_long stack, u_long ps_strings)
{
	struct trapframe *tf = td->td_frame;

	memset(tf, 0, sizeof(*tf));
	tf->tf_usr_sp = stack;
	tf->tf_usr_lr = entry;
	tf->tf_svc_lr = 0x77777777;
	tf->tf_pc = entry;
	tf->tf_spsr = PSR_USR32_MODE;
}

/*
 * Get machine context.
 */
int
get_mcontext(struct thread *td, mcontext_t *mcp, int clear_ret)
{
	struct trapframe *tf = td->td_frame;
	__greg_t *gr = mcp->__gregs;

	if (clear_ret & GET_MC_CLEAR_RET)
		gr[_REG_R0] = 0;
	else
		gr[_REG_R0]   = tf->tf_r0;
	gr[_REG_R1]   = tf->tf_r1;
	gr[_REG_R2]   = tf->tf_r2;
	gr[_REG_R3]   = tf->tf_r3;
	gr[_REG_R4]   = tf->tf_r4;
	gr[_REG_R5]   = tf->tf_r5;
	gr[_REG_R6]   = tf->tf_r6;
	gr[_REG_R7]   = tf->tf_r7;
	gr[_REG_R8]   = tf->tf_r8;
	gr[_REG_R9]   = tf->tf_r9;
	gr[_REG_R10]  = tf->tf_r10;
	gr[_REG_R11]  = tf->tf_r11;
	gr[_REG_R12]  = tf->tf_r12;
	gr[_REG_SP]   = tf->tf_usr_sp;
	gr[_REG_LR]   = tf->tf_usr_lr;
	gr[_REG_PC]   = tf->tf_pc;
	gr[_REG_CPSR] = tf->tf_spsr;

	return (0);
}

/*
 * Set machine context.
 *
 * However, we don't set any but the user modifiable flags, and we won't
 * touch the cs selector.
 */
int
set_mcontext(struct thread *td, const mcontext_t *mcp)
{
	struct trapframe *tf = td->td_frame;
	__greg_t *gr = mcp->__gregs;

	tf->tf_r0 = gr[_REG_R0];
	tf->tf_r1 = gr[_REG_R1];
	tf->tf_r2 = gr[_REG_R2];
	tf->tf_r3 = gr[_REG_R3];
	tf->tf_r4 = gr[_REG_R4];
	tf->tf_r5 = gr[_REG_R5];
	tf->tf_r6 = gr[_REG_R6];
	tf->tf_r7 = gr[_REG_R7];
	tf->tf_r8 = gr[_REG_R8];
	tf->tf_r9 = gr[_REG_R9];
	tf->tf_r10 = gr[_REG_R10];
	tf->tf_r11 = gr[_REG_R11];
	tf->tf_r12 = gr[_REG_R12];
	tf->tf_usr_sp = gr[_REG_SP];
	tf->tf_usr_lr = gr[_REG_LR];
	tf->tf_pc = gr[_REG_PC];
	tf->tf_spsr = gr[_REG_CPSR];

	return (0);
}

/*
 * MPSAFE
 */
int
sigreturn(td, uap)
	struct thread *td;
	struct sigreturn_args /* {
		const struct __ucontext *sigcntxp;
	} */ *uap;
{
	struct proc *p = td->td_proc;
	struct sigframe sf;
	struct trapframe *tf;
	int spsr;
	
	if (uap == NULL)
		return (EFAULT);
	if (copyin(uap->sigcntxp, &sf, sizeof(sf)))
		return (EFAULT);
	/*
	 * Make sure the processor mode has not been tampered with and
	 * interrupts have not been disabled.
	 */
	spsr = sf.sf_uc.uc_mcontext.__gregs[_REG_CPSR];
	if ((spsr & PSR_MODE) != PSR_USR32_MODE ||
	    (spsr & (I32_bit | F32_bit)) != 0)
		return (EINVAL);
		/* Restore register context. */
	tf = td->td_frame;
	set_mcontext(td, &sf.sf_uc.uc_mcontext);

	/* Restore signal mask. */
	PROC_LOCK(p);
	td->td_sigmask = sf.sf_uc.uc_sigmask;
	SIG_CANTMASK(td->td_sigmask);
	signotify(td);
	PROC_UNLOCK(p);

	return (EJUSTRETURN);
}


/*
 * Construct a PCB from a trapframe. This is called from kdb_trap() where
 * we want to start a backtrace from the function that caused us to enter
 * the debugger. We have the context in the trapframe, but base the trace
 * on the PCB. The PCB doesn't have to be perfect, as long as it contains
 * enough for a backtrace.
 */
void
makectx(struct trapframe *tf, struct pcb *pcb)
{
	pcb->un_32.pcb32_r8 = tf->tf_r8;
	pcb->un_32.pcb32_r9 = tf->tf_r9;
	pcb->un_32.pcb32_r10 = tf->tf_r10;
	pcb->un_32.pcb32_r11 = tf->tf_r11;
	pcb->un_32.pcb32_r12 = tf->tf_r12;
	pcb->un_32.pcb32_pc = tf->tf_pc;
	pcb->un_32.pcb32_lr = tf->tf_usr_lr;
	pcb->un_32.pcb32_sp = tf->tf_usr_sp;
}
