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
#include "opt_ddb.h"
#include "opt_platform.h"
#include "opt_sched.h"
#include "opt_timer.h"

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
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/ptrace.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

#include <machine/armreg.h>
#include <machine/atags.h>
#include <machine/cpu.h>
#include <machine/devmap.h>
#include <machine/frame.h>
#include <machine/intr.h>
#include <machine/machdep.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/pcb.h>
#include <machine/reg.h>
#include <machine/trap.h>
#include <machine/undefined.h>
#include <machine/vmparam.h>
#include <machine/sysarch.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#endif

#ifdef DEBUG
#define	debugf(fmt, args...) printf(fmt, ##args)
#else
#define	debugf(fmt, args...)
#endif

struct pcpu __pcpu[MAXCPU];
struct pcpu *pcpup = &__pcpu[0];

static struct trapframe proc0_tf;
uint32_t cpu_reset_address = 0;
int cold = 1;
vm_offset_t vector_page;

long realmem = 0;

int (*_arm_memcpy)(void *, void *, int, int) = NULL;
int (*_arm_bzero)(void *, int, int) = NULL;
int _min_memcpy_size = 0;
int _min_bzero_size = 0;

extern int *end;
#ifdef DDB
extern vm_offset_t ksym_start, ksym_end;
#endif

#ifdef FDT
/*
 * This is the number of L2 page tables required for covering max
 * (hypothetical) memsize of 4GB and all kernel mappings (vectors, msgbuf,
 * stacks etc.), uprounded to be divisible by 4.
 */
#define KERNEL_PT_MAX	78

static struct pv_addr kernel_pt_table[KERNEL_PT_MAX];

vm_paddr_t phys_avail[10];
vm_paddr_t dump_avail[4];

extern u_int data_abort_handler_address;
extern u_int prefetch_abort_handler_address;
extern u_int undefined_handler_address;

vm_paddr_t pmap_pa;

struct pv_addr systempage;
static struct pv_addr msgbufpv;
struct pv_addr irqstack;
struct pv_addr undstack;
struct pv_addr abtstack;
static struct pv_addr kernelstack;

#endif

#if defined(LINUX_BOOT_ABI)
#define LBABI_MAX_BANKS	10

uint32_t board_id;
struct arm_lbabi_tag *atag_list;
char linux_command_line[LBABI_MAX_COMMAND_LINE + 1];
char atags[LBABI_MAX_COMMAND_LINE * 2];
uint32_t memstart[LBABI_MAX_BANKS];
uint32_t memsize[LBABI_MAX_BANKS];
uint32_t membanks;
#endif

static uint32_t board_revision;
/* hex representation of uint64_t */
static char board_serial[32];

SYSCTL_NODE(_hw, OID_AUTO, board, CTLFLAG_RD, 0, "Board attributes");
SYSCTL_UINT(_hw_board, OID_AUTO, revision, CTLFLAG_RD,
    &board_revision, 0, "Board revision");
SYSCTL_STRING(_hw_board, OID_AUTO, serial, CTLFLAG_RD,
    board_serial, 0, "Board serial");

int vfp_exists;
SYSCTL_INT(_hw, HW_FLOATINGPT, floatingpoint, CTLFLAG_RD,
    &vfp_exists, 0, "Floating point support enabled");

void
board_set_serial(uint64_t serial)
{

	snprintf(board_serial, sizeof(board_serial)-1, 
		    "%016jx", serial);
}

void
board_set_revision(uint32_t revision)
{

	board_revision = revision;
}

void
sendsig(catcher, ksi, mask)
	sig_t catcher;
	ksiginfo_t *ksi;
	sigset_t *mask;
{
	struct thread *td;
	struct proc *p;
	struct trapframe *tf;
	struct sigframe *fp, frame;
	struct sigacts *psp;
	int onstack;
	int sig;
	int code;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	sig = ksi->ksi_signo;
	code = ksi->ksi_code;
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);
	tf = td->td_frame;
	onstack = sigonstack(tf->tf_usr_sp);

	CTR4(KTR_SIG, "sendsig: td=%p (%s) catcher=%p sig=%d", td, p->p_comm,
	    catcher, sig);

	/* Allocate and validate space for the signal handler context. */
	if ((td->td_pflags & TDP_ALTSTACK) != 0 && !(onstack) &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		fp = (struct sigframe *)(td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size);
#if defined(COMPAT_43)
		td->td_sigstk.ss_flags |= SS_ONSTACK;
#endif
	} else
		fp = (struct sigframe *)td->td_frame->tf_usr_sp;

	/* make room on the stack */
	fp--;
	
	/* make the stack aligned */
	fp = (struct sigframe *)STACKALIGN(fp);
	/* Populate the siginfo frame. */
	get_mcontext(td, &frame.sf_uc.uc_mcontext, 0);
	frame.sf_si = ksi->ksi_info;
	frame.sf_uc.uc_sigmask = *mask;
	frame.sf_uc.uc_stack.ss_flags = (td->td_pflags & TDP_ALTSTACK )
	    ? ((onstack) ? SS_ONSTACK : 0) : SS_DISABLE;
	frame.sf_uc.uc_stack = td->td_sigstk;
	mtx_unlock(&psp->ps_mtx);
	PROC_UNLOCK(td->td_proc);

	/* Copy the sigframe out to the user's stack. */
	if (copyout(&frame, fp, sizeof(*fp)) != 0) {
		/* Process has trashed its stack. Kill it. */
		CTR2(KTR_SIG, "sendsig: sigexit td=%p fp=%p", td, fp);
		PROC_LOCK(p);
		sigexit(td, SIGILL);
	}

	/* Translate the signal if appropriate. */
	if (p->p_sysent->sv_sigtbl && sig <= p->p_sysent->sv_sigsize)
		sig = p->p_sysent->sv_sigtbl[_SIG_IDX(sig)];

	/*
	 * Build context to run handler in.  We invoke the handler
	 * directly, only returning via the trampoline.  Note the
	 * trampoline version numbers are coordinated with machine-
	 * dependent code in libc.
	 */
	
	tf->tf_r0 = sig;
	tf->tf_r1 = (register_t)&fp->sf_si;
	tf->tf_r2 = (register_t)&fp->sf_uc;

	/* the trampoline uses r5 as the uc address */
	tf->tf_r5 = (register_t)&fp->sf_uc;
	tf->tf_pc = (register_t)catcher;
	tf->tf_usr_sp = (register_t)fp;
	tf->tf_usr_lr = (register_t)(PS_STRINGS - *(p->p_sysent->sv_szsigcode));

	CTR3(KTR_SIG, "sendsig: return td=%p pc=%#x sp=%#x", td, tf->tf_usr_lr,
	    tf->tf_usr_sp);

	PROC_LOCK(p);
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
#ifdef ARM_TP_ADDRESS
#ifndef ARM_CACHE_LOCK_ENABLE
	vm_page_t m;
#endif
#endif

	identify_arm_cpu();

	printf("real memory  = %ju (%ju MB)\n", (uintmax_t)ptoa(physmem),
	    (uintmax_t)ptoa(physmem) / 1048576);
	realmem = physmem;

	/*
	 * Display the RAM layout.
	 */
	if (bootverbose) {
		int indx;

		printf("Physical memory chunk(s):\n");
		for (indx = 0; phys_avail[indx + 1] != 0; indx += 2) {
			vm_paddr_t size;

			size = phys_avail[indx + 1] - phys_avail[indx];
			printf("%#08jx - %#08jx, %ju bytes (%ju pages)\n",
			    (uintmax_t)phys_avail[indx],
			    (uintmax_t)phys_avail[indx + 1] - 1,
			    (uintmax_t)size, (uintmax_t)size / PAGE_SIZE);
		}
	}

	vm_ksubmap_init(&kmi);

	printf("avail memory = %ju (%ju MB)\n",
	    (uintmax_t)ptoa(cnt.v_free_count),
	    (uintmax_t)ptoa(cnt.v_free_count) / 1048576);

	bufinit();
	vm_pager_bufferinit();
	pcb->un_32.pcb32_und_sp = (u_int)thread0.td_kstack +
	    USPACE_UNDEF_STACK_TOP;
	pcb->un_32.pcb32_sp = (u_int)thread0.td_kstack +
	    USPACE_SVC_STACK_TOP;
	vector_page_setprot(VM_PROT_READ);
	pmap_set_pcb_pagedir(pmap_kernel(), pcb);
	pmap_postinit();
#ifdef ARM_TP_ADDRESS
#ifdef ARM_CACHE_LOCK_ENABLE
	pmap_kenter_user(ARM_TP_ADDRESS, ARM_TP_ADDRESS);
	arm_lock_cache_line(ARM_TP_ADDRESS);
#else
	m = vm_page_alloc(NULL, 0, VM_ALLOC_NOOBJ | VM_ALLOC_ZERO);
	pmap_kenter_user(ARM_TP_ADDRESS, VM_PAGE_TO_PHYS(m));
#endif
	*(uint32_t *)ARM_RAS_START = 0;
	*(uint32_t *)ARM_RAS_END = 0xffffffff;
#endif
}

SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL);

/*
 * Flush the D-cache for non-DMA I/O so that the I-cache can
 * be made coherent later.
 */
void
cpu_flush_dcache(void *ptr, size_t len)
{

	cpu_dcache_wb_range((uintptr_t)ptr, len);
	cpu_l2cache_wb_range((uintptr_t)ptr, len);
}

/* Get current clock frequency for the given cpu id. */
int
cpu_est_clockrate(int cpu_id, uint64_t *rate)
{

	return (ENXIO);
}

void
cpu_idle(int busy)
{
	
	CTR2(KTR_SPARE2, "cpu_idle(%d) at %d",
	    busy, curcpu);
#ifndef NO_EVENTTIMERS
	if (!busy) {
		critical_enter();
		cpu_idleclock();
	}
#endif
	if (!sched_runnable())
		cpu_sleep(0);
#ifndef NO_EVENTTIMERS
	if (!busy) {
		cpu_activeclock();
		critical_exit();
	}
#endif
	CTR2(KTR_SPARE2, "cpu_idle(%d) at %d done",
	    busy, curcpu);
}

int
cpu_idle_wakeup(int cpu)
{

	return (0);
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

	PROC_LOCK_ASSERT(td->td_proc, MA_NOTOWNED);
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

	PROC_LOCK_ASSERT(td->td_proc, MA_NOTOWNED);
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
	struct proc *p;
	int error;
	
	KASSERT(td->td_md.md_ptrace_instr == 0,
	 ("Didn't clear single step"));
	p = td->td_proc;
	PROC_UNLOCK(p);
	error = ptrace_read_int(td, td->td_frame->tf_pc + 4,
	    &td->td_md.md_ptrace_instr);
	if (error)
		goto out;
	error = ptrace_write_int(td, td->td_frame->tf_pc + 4,
	    PTRACE_BREAKPOINT);
	if (error)
		td->td_md.md_ptrace_instr = 0;
	td->td_md.md_ptrace_addr = td->td_frame->tf_pc + 4;
out:
	PROC_LOCK(p);
	return (error);
}

int
ptrace_clear_single_step(struct thread *td)
{
	struct proc *p;

	if (td->td_md.md_ptrace_instr) {
		p = td->td_proc;
		PROC_UNLOCK(p);
		ptrace_write_int(td, td->td_md.md_ptrace_addr,
		    td->td_md.md_ptrace_instr);
		PROC_LOCK(p);
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
	register_t cspr;

	td = curthread;
	if (td->td_md.md_spinlock_count == 0) {
		cspr = disable_interrupts(I32_bit | F32_bit);
		td->td_md.md_spinlock_count = 1;
		td->td_md.md_saved_cspr = cspr;
	} else
		td->td_md.md_spinlock_count++;
	critical_enter();
}

void
spinlock_exit(void)
{
	struct thread *td;
	register_t cspr;

	td = curthread;
	critical_exit();
	cspr = td->td_md.md_saved_cspr;
	td->td_md.md_spinlock_count--;
	if (td->td_md.md_spinlock_count == 0)
		restore_interrupts(cspr);
}

/*
 * Clear registers on exec
 */
void
exec_setregs(struct thread *td, struct image_params *imgp, u_long stack)
{
	struct trapframe *tf = td->td_frame;

	memset(tf, 0, sizeof(*tf));
	tf->tf_usr_sp = stack;
	tf->tf_usr_lr = imgp->entry_addr;
	tf->tf_svc_lr = 0x77777777;
	tf->tf_pc = imgp->entry_addr;
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
	const __greg_t *gr = mcp->__gregs;

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
sys_sigreturn(td, uap)
	struct thread *td;
	struct sigreturn_args /* {
		const struct __ucontext *sigcntxp;
	} */ *uap;
{
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
	kern_sigprocmask(td, SIG_SETMASK, &sf.sf_uc.uc_sigmask, NULL, 0);

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

/*
 * Make a standard dump_avail array.  Can't make the phys_avail
 * since we need to do that after we call pmap_bootstrap, but this
 * is needed before pmap_boostrap.
 *
 * ARM_USE_SMALL_ALLOC uses dump_avail, so it must be filled before
 * calling pmap_bootstrap.
 */
void
arm_dump_avail_init(vm_offset_t ramsize, size_t max)
{
#ifdef LINUX_BOOT_ABI
	/*
	 * Linux boot loader passes us the actual banks of memory, so use them
	 * to construct the dump_avail array.
	 */
	if (membanks > 0) 
	{
		int i, j;

		if (max < (membanks + 1) * 2)
			panic("dump_avail[%d] too small for %d banks\n",
			    max, membanks);
		for (j = 0, i = 0; i < membanks; i++) {
			dump_avail[j++] = round_page(memstart[i]);
			dump_avail[j++] = trunc_page(memstart[i] + memsize[i]);
		}
		dump_avail[j++] = 0;
		dump_avail[j++] = 0;
		return;
	}
#endif
	if (max < 4)
		panic("dump_avail too small\n");

	dump_avail[0] = round_page(PHYSADDR);
	dump_avail[1] = trunc_page(PHYSADDR + ramsize);
	dump_avail[2] = 0;
	dump_avail[3] = 0;
}

/*
 * Fake up a boot descriptor table
 */
vm_offset_t
fake_preload_metadata(struct arm_boot_params *abp __unused)
{
#ifdef DDB
	vm_offset_t zstart = 0, zend = 0;
#endif
	vm_offset_t lastaddr;
	int i = 0;
	static uint32_t fake_preload[35];

	fake_preload[i++] = MODINFO_NAME;
	fake_preload[i++] = strlen("kernel") + 1;
	strcpy((char*)&fake_preload[i++], "kernel");
	i += 1;
	fake_preload[i++] = MODINFO_TYPE;
	fake_preload[i++] = strlen("elf kernel") + 1;
	strcpy((char*)&fake_preload[i++], "elf kernel");
	i += 2;
	fake_preload[i++] = MODINFO_ADDR;
	fake_preload[i++] = sizeof(vm_offset_t);
	fake_preload[i++] = KERNVIRTADDR;
	fake_preload[i++] = MODINFO_SIZE;
	fake_preload[i++] = sizeof(uint32_t);
	fake_preload[i++] = (uint32_t)&end - KERNVIRTADDR;
#ifdef DDB
	if (*(uint32_t *)KERNVIRTADDR == MAGIC_TRAMP_NUMBER) {
		fake_preload[i++] = MODINFO_METADATA|MODINFOMD_SSYM;
		fake_preload[i++] = sizeof(vm_offset_t);
		fake_preload[i++] = *(uint32_t *)(KERNVIRTADDR + 4);
		fake_preload[i++] = MODINFO_METADATA|MODINFOMD_ESYM;
		fake_preload[i++] = sizeof(vm_offset_t);
		fake_preload[i++] = *(uint32_t *)(KERNVIRTADDR + 8);
		lastaddr = *(uint32_t *)(KERNVIRTADDR + 8);
		zend = lastaddr;
		zstart = *(uint32_t *)(KERNVIRTADDR + 4);
		ksym_start = zstart;
		ksym_end = zend;
	} else
#endif
		lastaddr = (vm_offset_t)&end;
	fake_preload[i++] = 0;
	fake_preload[i] = 0;
	preload_metadata = (void *)fake_preload;

	return (lastaddr);
}

void
pcpu0_init(void)
{
#if ARM_ARCH_6 || ARM_ARCH_7A || defined(CPU_MV_PJ4B)
	set_pcpu(pcpup);
#endif
	pcpu_init(pcpup, 0, sizeof(struct pcpu));
	PCPU_SET(curthread, &thread0);
#ifdef VFP
	PCPU_SET(cpu, 0);
#endif
}

#if defined(LINUX_BOOT_ABI)
vm_offset_t
linux_parse_boot_param(struct arm_boot_params *abp)
{
	struct arm_lbabi_tag *walker;
	uint32_t revision;
	uint64_t serial;

	/*
	 * Linux boot ABI: r0 = 0, r1 is the board type (!= 0) and r2
	 * is atags or dtb pointer.  If all of these aren't satisfied,
	 * then punt.
	 */
	if (!(abp->abp_r0 == 0 && abp->abp_r1 != 0 && abp->abp_r2 != 0))
		return 0;

	board_id = abp->abp_r1;
	walker = (struct arm_lbabi_tag *)
	    (abp->abp_r2 + KERNVIRTADDR - KERNPHYSADDR);

	/* xxx - Need to also look for binary device tree */
	if (ATAG_TAG(walker) != ATAG_CORE)
		return 0;

	atag_list = walker;
	while (ATAG_TAG(walker) != ATAG_NONE) {
		switch (ATAG_TAG(walker)) {
		case ATAG_CORE:
			break;
		case ATAG_MEM:
			if (membanks < LBABI_MAX_BANKS) {
				memstart[membanks] = walker->u.tag_mem.start;
				memsize[membanks] = walker->u.tag_mem.size;
			}
			membanks++;
			break;
		case ATAG_INITRD2:
			break;
		case ATAG_SERIAL:
			serial = walker->u.tag_sn.low |
			    ((uint64_t)walker->u.tag_sn.high << 32);
			board_set_serial(serial);
			break;
		case ATAG_REVISION:
			revision = walker->u.tag_rev.rev;
			board_set_revision(revision);
			break;
		case ATAG_CMDLINE:
			/* XXX open question: Parse this for boothowto? */
			bcopy(walker->u.tag_cmd.command, linux_command_line,
			      ATAG_SIZE(walker));
			break;
		default:
			break;
		}
		walker = ATAG_NEXT(walker);
	}

	/* Save a copy for later */
	bcopy(atag_list, atags,
	    (char *)walker - (char *)atag_list + ATAG_SIZE(walker));

	return fake_preload_metadata(abp);
}
#endif

#if defined(FREEBSD_BOOT_LOADER)
vm_offset_t
freebsd_parse_boot_param(struct arm_boot_params *abp)
{
	vm_offset_t lastaddr = 0;
	void *mdp;
	void *kmdp;

	/*
	 * Mask metadata pointer: it is supposed to be on page boundary. If
	 * the first argument (mdp) doesn't point to a valid address the
	 * bootloader must have passed us something else than the metadata
	 * ptr, so we give up.  Also give up if we cannot find metadta section
	 * the loader creates that we get all this data out of.
	 */

	if ((mdp = (void *)(abp->abp_r0 & ~PAGE_MASK)) == NULL)
		return 0;
	preload_metadata = mdp;
	kmdp = preload_search_by_type("elf kernel");
	if (kmdp == NULL)
		return 0;

	boothowto = MD_FETCH(kmdp, MODINFOMD_HOWTO, int);
	kern_envp = MD_FETCH(kmdp, MODINFOMD_ENVP, char *);
	lastaddr = MD_FETCH(kmdp, MODINFOMD_KERNEND, vm_offset_t);
#ifdef DDB
	ksym_start = MD_FETCH(kmdp, MODINFOMD_SSYM, uintptr_t);
	ksym_end = MD_FETCH(kmdp, MODINFOMD_ESYM, uintptr_t);
#endif
	preload_addr_relocate = KERNVIRTADDR - KERNPHYSADDR;
	return lastaddr;
}
#endif

vm_offset_t
default_parse_boot_param(struct arm_boot_params *abp)
{
	vm_offset_t lastaddr;

#if defined(LINUX_BOOT_ABI)
	if ((lastaddr = linux_parse_boot_param(abp)) != 0)
		return lastaddr;
#endif
#if defined(FREEBSD_BOOT_LOADER)
	if ((lastaddr = freebsd_parse_boot_param(abp)) != 0)
		return lastaddr;
#endif
	/* Fall back to hardcoded metadata. */
	lastaddr = fake_preload_metadata(abp);

	return lastaddr;
}

/*
 * Stub version of the boot parameter parsing routine.  We are
 * called early in initarm, before even VM has been initialized.
 * This routine needs to preserve any data that the boot loader
 * has passed in before the kernel starts to grow past the end
 * of the BSS, traditionally the place boot-loaders put this data.
 *
 * Since this is called so early, things that depend on the vm system
 * being setup (including access to some SoC's serial ports), about
 * all that can be done in this routine is to copy the arguments.
 *
 * This is the default boot parameter parsing routine.  Individual
 * kernels/boards can override this weak function with one of their
 * own.  We just fake metadata...
 */
__weak_reference(default_parse_boot_param, parse_boot_param);

/*
 * Initialize proc0
 */
void
init_proc0(vm_offset_t kstack)
{
	proc_linkup0(&proc0, &thread0);
	thread0.td_kstack = kstack;
	thread0.td_pcb = (struct pcb *)
		(thread0.td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;
	thread0.td_pcb->pcb_flags = 0;
	thread0.td_frame = &proc0_tf;
	pcpup->pc_curpcb = thread0.td_pcb;
}

void
set_stackptrs(int cpu)
{

	set_stackptr(PSR_IRQ32_MODE,
	    irqstack.pv_va + ((IRQ_STACK_SIZE * PAGE_SIZE) * (cpu + 1)));
	set_stackptr(PSR_ABT32_MODE,
	    abtstack.pv_va + ((ABT_STACK_SIZE * PAGE_SIZE) * (cpu + 1)));
	set_stackptr(PSR_UND32_MODE,
	    undstack.pv_va + ((UND_STACK_SIZE * PAGE_SIZE) * (cpu + 1)));
}

#ifdef FDT
static char *
kenv_next(char *cp)
{

	if (cp != NULL) {
		while (*cp != 0)
			cp++;
		cp++;
		if (*cp == 0)
			cp = NULL;
	}
	return (cp);
}

static void
print_kenv(void)
{
	int len;
	char *cp;

	debugf("loader passed (static) kenv:\n");
	if (kern_envp == NULL) {
		debugf(" no env, null ptr\n");
		return;
	}
	debugf(" kern_envp = 0x%08x\n", (uint32_t)kern_envp);

	len = 0;
	for (cp = kern_envp; cp != NULL; cp = kenv_next(cp))
		debugf(" %x %s\n", (uint32_t)cp, cp);
}

static void
physmap_init(struct mem_region *availmem_regions, int availmem_regions_sz)
{
	int i, j, cnt;
	vm_offset_t phys_kernelend, kernload;
	uint32_t s, e, sz;
	struct mem_region *mp, *mp1;

	phys_kernelend = KERNPHYSADDR + (virtual_avail - KERNVIRTADDR);
	kernload = KERNPHYSADDR;

	/*
	 * Remove kernel physical address range from avail
	 * regions list. Page align all regions.
	 * Non-page aligned memory isn't very interesting to us.
	 * Also, sort the entries for ascending addresses.
	 */
	sz = 0;
	cnt = availmem_regions_sz;
	debugf("processing avail regions:\n");
	for (mp = availmem_regions; mp->mr_size; mp++) {
		s = mp->mr_start;
		e = mp->mr_start + mp->mr_size;
		debugf(" %08x-%08x -> ", s, e);
		/* Check whether this region holds all of the kernel. */
		if (s < kernload && e > phys_kernelend) {
			availmem_regions[cnt].mr_start = phys_kernelend;
			availmem_regions[cnt++].mr_size = e - phys_kernelend;
			e = kernload;
		}
		/* Look whether this regions starts within the kernel. */
		if (s >= kernload && s < phys_kernelend) {
			if (e <= phys_kernelend)
				goto empty;
			s = phys_kernelend;
		}
		/* Now look whether this region ends within the kernel. */
		if (e > kernload && e <= phys_kernelend) {
			if (s >= kernload) {
				goto empty;
			}
			e = kernload;
		}
		/* Now page align the start and size of the region. */
		s = round_page(s);
		e = trunc_page(e);
		if (e < s)
			e = s;
		sz = e - s;
		debugf("%08x-%08x = %x\n", s, e, sz);

		/* Check whether some memory is left here. */
		if (sz == 0) {
		empty:
			printf("skipping\n");
			bcopy(mp + 1, mp,
			    (cnt - (mp - availmem_regions)) * sizeof(*mp));
			cnt--;
			mp--;
			continue;
		}

		/* Do an insertion sort. */
		for (mp1 = availmem_regions; mp1 < mp; mp1++)
			if (s < mp1->mr_start)
				break;
		if (mp1 < mp) {
			bcopy(mp1, mp1 + 1, (char *)mp - (char *)mp1);
			mp1->mr_start = s;
			mp1->mr_size = sz;
		} else {
			mp->mr_start = s;
			mp->mr_size = sz;
		}
	}
	availmem_regions_sz = cnt;

	/* Fill in phys_avail table, based on availmem_regions */
	debugf("fill in phys_avail:\n");
	for (i = 0, j = 0; i < availmem_regions_sz; i++, j += 2) {

		debugf(" region: 0x%08x - 0x%08x (0x%08x)\n",
		    availmem_regions[i].mr_start,
		    availmem_regions[i].mr_start + availmem_regions[i].mr_size,
		    availmem_regions[i].mr_size);

		/*
		 * We should not map the page at PA 0x0000000, the VM can't
		 * handle it, as pmap_extract() == 0 means failure.
		 */
		if (availmem_regions[i].mr_start > 0 ||
		    availmem_regions[i].mr_size > PAGE_SIZE) {
			vm_size_t size;
			phys_avail[j] = availmem_regions[i].mr_start;

			size = availmem_regions[i].mr_size;
			if (phys_avail[j] == 0) {
				phys_avail[j] += PAGE_SIZE;
				size -= PAGE_SIZE;
			}
			phys_avail[j + 1] = availmem_regions[i].mr_start + size;
		} else
			j -= 2;
	}
	phys_avail[j] = 0;
	phys_avail[j + 1] = 0;
}

void *
initarm(struct arm_boot_params *abp)
{
	struct mem_region memory_regions[FDT_MEM_REGIONS];
	struct mem_region availmem_regions[FDT_MEM_REGIONS];
	struct mem_region reserved_regions[FDT_MEM_REGIONS];
	struct pv_addr kernel_l1pt;
	struct pv_addr dpcpu;
	vm_offset_t dtbp, freemempos, l2_start, lastaddr;
	uint32_t memsize, l2size;
	char *env;
	void *kmdp;
	u_int l1pagetable;
	int i = 0, j = 0, err_devmap = 0;
	int memory_regions_sz;
	int availmem_regions_sz;
	int reserved_regions_sz;
	vm_offset_t start, end;
	vm_offset_t rstart, rend;
	int curr;

	lastaddr = parse_boot_param(abp);
	memsize = 0;
	set_cpufuncs();

	/*
	 * Find the dtb passed in by the boot loader.
	 */
	kmdp = preload_search_by_type("elf kernel");
	if (kmdp != NULL)
		dtbp = MD_FETCH(kmdp, MODINFOMD_DTBP, vm_offset_t);
	else
		dtbp = (vm_offset_t)NULL;

#if defined(FDT_DTB_STATIC)
	/*
	 * In case the device tree blob was not retrieved (from metadata) try
	 * to use the statically embedded one.
	 */
	if (dtbp == (vm_offset_t)NULL)
		dtbp = (vm_offset_t)&fdt_static_dtb;
#endif

	if (OF_install(OFW_FDT, 0) == FALSE)
		while (1);

	if (OF_init((void *)dtbp) != 0)
		while (1);

	/* Grab physical memory regions information from device tree. */
	if (fdt_get_mem_regions(memory_regions, &memory_regions_sz,
	    &memsize) != 0)
		while(1);

	/* Grab physical memory regions information from device tree. */
	if (fdt_get_reserved_regions(reserved_regions, &reserved_regions_sz) != 0)
		reserved_regions_sz = 0;
		
	/*
	 * Now exclude all the reserved regions
	 */
	curr = 0;
	for (i = 0; i < memory_regions_sz; i++) {
		start = memory_regions[i].mr_start;
		end = start + memory_regions[i].mr_size;
		for (j = 0; j < reserved_regions_sz; j++) {
			rstart = reserved_regions[j].mr_start;
			rend = rstart + reserved_regions[j].mr_size;
			/* 
			 * Restricted region is before available
			 * Skip restricted region
			 */
			if (rend <= start)
				continue;
			/* 
			 * Restricted region is behind available
			 * No  further processing required
			 */
			if (rstart >= end)
				break;
			/*
			 * Restricted region includes memory region
			 * skip available region
			 */
			if ((start >= rstart) && (rend >= end)) {
				start = rend;
				end = rend;
				break;
			}
			/*
			 * Memory region includes restricted region
			 */
			if ((rstart > start) && (end > rend)) {
				availmem_regions[curr].mr_start = start;
				availmem_regions[curr++].mr_size = rstart - start;
				start = rend;
				break;
			}
			/*
			 * Memory region partially overlaps with restricted
			 */
			if ((rstart >= start) && (rstart <= end)) {
				end = rstart;
			}
			else if ((rend >= start) && (rend <= end)) {
				start = rend;
			}
		}

		if (end > start) {
			availmem_regions[curr].mr_start = start;
			availmem_regions[curr++].mr_size = end - start;
		}
	}

	availmem_regions_sz = curr;

	/* Platform-specific initialisation */
	initarm_early_init();

	pcpu0_init();

	/* Do basic tuning, hz etc */
	init_param1();

	/* Calculate number of L2 tables needed for mapping vm_page_array */
	l2size = (memsize / PAGE_SIZE) * sizeof(struct vm_page);
	l2size = (l2size >> L1_S_SHIFT) + 1;

	/*
	 * Add one table for end of kernel map, one for stacks, msgbuf and
	 * L1 and L2 tables map and one for vectors map.
	 */
	l2size += 3;

	/* Make it divisible by 4 */
	l2size = (l2size + 3) & ~3;

	freemempos = (lastaddr + PAGE_MASK) & ~PAGE_MASK;

	/* Define a macro to simplify memory allocation */
#define valloc_pages(var, np)						\
	alloc_pages((var).pv_va, (np));					\
	(var).pv_pa = (var).pv_va + (KERNPHYSADDR - KERNVIRTADDR);

#define alloc_pages(var, np)						\
	(var) = freemempos;						\
	freemempos += (np * PAGE_SIZE);					\
	memset((char *)(var), 0, ((np) * PAGE_SIZE));

	while (((freemempos - L1_TABLE_SIZE) & (L1_TABLE_SIZE - 1)) != 0)
		freemempos += PAGE_SIZE;
	valloc_pages(kernel_l1pt, L1_TABLE_SIZE / PAGE_SIZE);

	for (i = 0; i < l2size; ++i) {
		if (!(i % (PAGE_SIZE / L2_TABLE_SIZE_REAL))) {
			valloc_pages(kernel_pt_table[i],
			    L2_TABLE_SIZE / PAGE_SIZE);
			j = i;
		} else {
			kernel_pt_table[i].pv_va = kernel_pt_table[j].pv_va +
			    L2_TABLE_SIZE_REAL * (i - j);
			kernel_pt_table[i].pv_pa =
			    kernel_pt_table[i].pv_va - KERNVIRTADDR +
			    KERNPHYSADDR;

		}
	}
	/*
	 * Allocate a page for the system page mapped to 0x00000000
	 * or 0xffff0000. This page will just contain the system vectors
	 * and can be shared by all processes.
	 */
	valloc_pages(systempage, 1);

	/* Allocate dynamic per-cpu area. */
	valloc_pages(dpcpu, DPCPU_SIZE / PAGE_SIZE);
	dpcpu_init((void *)dpcpu.pv_va, 0);

	/* Allocate stacks for all modes */
	valloc_pages(irqstack, IRQ_STACK_SIZE * MAXCPU);
	valloc_pages(abtstack, ABT_STACK_SIZE * MAXCPU);
	valloc_pages(undstack, UND_STACK_SIZE * MAXCPU);
	valloc_pages(kernelstack, KSTACK_PAGES * MAXCPU);
	valloc_pages(msgbufpv, round_page(msgbufsize) / PAGE_SIZE);

	/*
	 * Now we start construction of the L1 page table
	 * We start by mapping the L2 page tables into the L1.
	 * This means that we can replace L1 mappings later on if necessary
	 */
	l1pagetable = kernel_l1pt.pv_va;

	/*
	 * Try to map as much as possible of kernel text and data using
	 * 1MB section mapping and for the rest of initial kernel address
	 * space use L2 coarse tables.
	 *
	 * Link L2 tables for mapping remainder of kernel (modulo 1MB)
	 * and kernel structures
	 */
	l2_start = lastaddr & ~(L1_S_OFFSET);
	for (i = 0 ; i < l2size - 1; i++)
		pmap_link_l2pt(l1pagetable, l2_start + i * L1_S_SIZE,
		    &kernel_pt_table[i]);

	pmap_curmaxkvaddr = l2_start + (l2size - 1) * L1_S_SIZE;

	/* Map kernel code and data */
	pmap_map_chunk(l1pagetable, KERNVIRTADDR, KERNPHYSADDR,
	   (((uint32_t)(lastaddr) - KERNVIRTADDR) + PAGE_MASK) & ~PAGE_MASK,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);


	/* Map L1 directory and allocated L2 page tables */
	pmap_map_chunk(l1pagetable, kernel_l1pt.pv_va, kernel_l1pt.pv_pa,
	    L1_TABLE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);

	pmap_map_chunk(l1pagetable, kernel_pt_table[0].pv_va,
	    kernel_pt_table[0].pv_pa,
	    L2_TABLE_SIZE_REAL * l2size,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);

	/* Map allocated DPCPU, stacks and msgbuf */
	pmap_map_chunk(l1pagetable, dpcpu.pv_va, dpcpu.pv_pa,
	    freemempos - dpcpu.pv_va,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);

	/* Link and map the vector page */
	pmap_link_l2pt(l1pagetable, ARM_VECTORS_HIGH,
	    &kernel_pt_table[l2size - 1]);
	pmap_map_entry(l1pagetable, ARM_VECTORS_HIGH, systempage.pv_pa,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE, PTE_CACHE);

	/* Establish static device mappings. */
	err_devmap = initarm_devmap_init();
	arm_devmap_bootstrap(l1pagetable, NULL);
	vm_max_kernel_address = initarm_lastaddr();

	cpu_domains((DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL * 2)) | DOMAIN_CLIENT);
	pmap_pa = kernel_l1pt.pv_pa;
	setttb(kernel_l1pt.pv_pa);
	cpu_tlb_flushID();
	cpu_domains(DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL * 2));

	/*
	 * Now that proper page tables are installed, call cpu_setup() to enable
	 * instruction and data caches and other chip-specific features.
	 */
	cpu_setup("");

	/*
	 * Only after the SOC registers block is mapped we can perform device
	 * tree fixups, as they may attempt to read parameters from hardware.
	 */
	OF_interpret("perform-fixup", 0);

	initarm_gpio_init();

	cninit();

	physmem = memsize / PAGE_SIZE;

	debugf("initarm: console initialized\n");
	debugf(" arg1 kmdp = 0x%08x\n", (uint32_t)kmdp);
	debugf(" boothowto = 0x%08x\n", boothowto);
	debugf(" dtbp = 0x%08x\n", (uint32_t)dtbp);
	print_kenv();

	env = getenv("kernelname");
	if (env != NULL)
		strlcpy(kernelname, env, sizeof(kernelname));

	if (err_devmap != 0)
		printf("WARNING: could not fully configure devmap, error=%d\n",
		    err_devmap);

	initarm_late_init();

	/*
	 * Pages were allocated during the secondary bootstrap for the
	 * stacks for different CPU modes.
	 * We must now set the r13 registers in the different CPU modes to
	 * point to these stacks.
	 * Since the ARM stacks use STMFD etc. we must set r13 to the top end
	 * of the stack memory.
	 */
	cpu_control(CPU_CONTROL_MMU_ENABLE, CPU_CONTROL_MMU_ENABLE);

	set_stackptrs(0);

	/*
	 * We must now clean the cache again....
	 * Cleaning may be done by reading new data to displace any
	 * dirty data in the cache. This will have happened in setttb()
	 * but since we are boot strapping the addresses used for the read
	 * may have just been remapped and thus the cache could be out
	 * of sync. A re-clean after the switch will cure this.
	 * After booting there are no gross relocations of the kernel thus
	 * this problem will not occur after initarm().
	 */
	cpu_idcache_wbinv_all();

	/* Set stack for exception handlers */
	data_abort_handler_address = (u_int)data_abort_handler;
	prefetch_abort_handler_address = (u_int)prefetch_abort_handler;
	undefined_handler_address = (u_int)undefinedinstruction_bounce;
	undefined_init();

	init_proc0(kernelstack.pv_va);

	arm_intrnames_init();
	arm_vector_init(ARM_VECTORS_HIGH, ARM_VEC_ALL);
	arm_dump_avail_init(memsize, sizeof(dump_avail) / sizeof(dump_avail[0]));
	pmap_bootstrap(freemempos, &kernel_l1pt);
	msgbufp = (void *)msgbufpv.pv_va;
	msgbufinit(msgbufp, msgbufsize);
	mutex_init();

	/*
	 * Prepare map of physical memory regions available to vm subsystem.
	 */
	physmap_init(availmem_regions, availmem_regions_sz);

	init_param2(physmem);
	kdb_init();

	return ((void *)(kernelstack.pv_va + USPACE_SVC_STACK_TOP -
	    sizeof(struct pcb)));
}
#endif
