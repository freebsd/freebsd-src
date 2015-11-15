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
#include "opt_kstack_pages.h"
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
#include <sys/efi.h>
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
#include <sys/reboot.h>
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

#include <machine/acle-compat.h>
#include <machine/armreg.h>
#include <machine/atags.h>
#include <machine/cpu.h>
#include <machine/cpuinfo.h>
#include <machine/db_machdep.h>
#include <machine/devmap.h>
#include <machine/frame.h>
#include <machine/intr.h>
#include <machine/machdep.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/pcb.h>
#include <machine/physmem.h>
#include <machine/platform.h>
#include <machine/reg.h>
#include <machine/trap.h>
#include <machine/undefined.h>
#include <machine/vfp.h>
#include <machine/vmparam.h>
#include <machine/sysarch.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#endif

#ifdef DDB
#include <ddb/ddb.h>

#if __ARM_ARCH >= 6
#include <machine/cpu-v6.h>

DB_SHOW_COMMAND(cp15, db_show_cp15)
{
	u_int reg;

	reg = cp15_midr_get();
	db_printf("Cpu ID: 0x%08x\n", reg);
	reg = cp15_ctr_get();
	db_printf("Current Cache Lvl ID: 0x%08x\n",reg);

	reg = cp15_sctlr_get();
	db_printf("Ctrl: 0x%08x\n",reg);
	reg = cp15_actlr_get();
	db_printf("Aux Ctrl: 0x%08x\n",reg);

	reg = cp15_id_pfr0_get();
	db_printf("Processor Feat 0: 0x%08x\n", reg);
	reg = cp15_id_pfr1_get();
	db_printf("Processor Feat 1: 0x%08x\n", reg);
	reg = cp15_id_dfr0_get();
	db_printf("Debug Feat 0: 0x%08x\n", reg);
	reg = cp15_id_afr0_get();
	db_printf("Auxiliary Feat 0: 0x%08x\n", reg);
	reg = cp15_id_mmfr0_get();
	db_printf("Memory Model Feat 0: 0x%08x\n", reg);
	reg = cp15_id_mmfr1_get();
	db_printf("Memory Model Feat 1: 0x%08x\n", reg);
	reg = cp15_id_mmfr2_get();
	db_printf("Memory Model Feat 2: 0x%08x\n", reg);
	reg = cp15_id_mmfr3_get();
	db_printf("Memory Model Feat 3: 0x%08x\n", reg);
	reg = cp15_ttbr_get();
	db_printf("TTB0: 0x%08x\n", reg);
}

DB_SHOW_COMMAND(vtop, db_show_vtop)
{
	u_int reg;

	if (have_addr) {
		cp15_ats1cpr_set(addr);
		reg = cp15_par_get();
		db_printf("Physical address reg: 0x%08x\n",reg);
	} else
		db_printf("show vtop <virt_addr>\n");
}
#endif /* __ARM_ARCH >= 6 */
#endif /* DDB */

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

int (*_arm_memcpy)(void *, void *, int, int) = NULL;
int (*_arm_bzero)(void *, int, int) = NULL;
int _min_memcpy_size = 0;
int _min_bzero_size = 0;

extern int *end;

#ifdef FDT
vm_paddr_t pmap_pa;

#ifdef ARM_NEW_PMAP
vm_offset_t systempage;
vm_offset_t irqstack;
vm_offset_t undstack;
vm_offset_t abtstack;
#else
/*
 * This is the number of L2 page tables required for covering max
 * (hypothetical) memsize of 4GB and all kernel mappings (vectors, msgbuf,
 * stacks etc.), uprounded to be divisible by 4.
 */
#define KERNEL_PT_MAX	78

static struct pv_addr kernel_pt_table[KERNEL_PT_MAX];

struct pv_addr systempage;
static struct pv_addr msgbufpv;
struct pv_addr irqstack;
struct pv_addr undstack;
struct pv_addr abtstack;
static struct pv_addr kernelstack;
#endif
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
	/* Set the mode to enter in the signal handler */
#if __ARM_ARCH >= 7
	if ((register_t)catcher & 1)
		tf->tf_spsr |= PSR_T;
	else
		tf->tf_spsr &= ~PSR_T;
#endif

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
	const unsigned int mbyte = 1024 * 1024;
#ifdef ARM_TP_ADDRESS
#ifndef ARM_CACHE_LOCK_ENABLE
	vm_page_t m;
#endif
#endif

	identify_arm_cpu();

	vm_ksubmap_init(&kmi);

	/*
	 * Display the RAM layout.
	 */
	printf("real memory  = %ju (%ju MB)\n",
	    (uintmax_t)arm32_ptob(realmem),
	    (uintmax_t)arm32_ptob(realmem) / mbyte);
	printf("avail memory = %ju (%ju MB)\n",
	    (uintmax_t)arm32_ptob(vm_cnt.v_free_count),
	    (uintmax_t)arm32_ptob(vm_cnt.v_free_count) / mbyte);
	if (bootverbose) {
		arm_physmem_print_tables();
		arm_devmap_print_table();
	}

	bufinit();
	vm_pager_bufferinit();
	pcb->pcb_regs.sf_sp = (u_int)thread0.td_kstack +
	    USPACE_SVC_STACK_TOP;
	pmap_set_pcb_pagedir(pmap_kernel(), pcb);
#ifndef ARM_NEW_PMAP
	vector_page_setprot(VM_PROT_READ);
	pmap_postinit();
#endif
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
#ifdef ARM_L2_PIPT
	cpu_l2cache_wb_range((uintptr_t)vtophys(ptr), len);
#else
	cpu_l2cache_wb_range((uintptr_t)ptr, len);
#endif
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

	CTR2(KTR_SPARE2, "cpu_idle(%d) at %d", busy, curcpu);
	spinlock_enter();
#ifndef NO_EVENTTIMERS
	if (!busy)
		cpu_idleclock();
#endif
	if (!sched_runnable())
		cpu_sleep(0);
#ifndef NO_EVENTTIMERS
	if (!busy)
		cpu_activeclock();
#endif
	spinlock_exit();
	CTR2(KTR_SPARE2, "cpu_idle(%d) at %d done", busy, curcpu);
}

int
cpu_idle_wakeup(int cpu)
{

	return (0);
}

/*
 * Most ARM platforms don't need to do anything special to init their clocks
 * (they get intialized during normal device attachment), and by not defining a
 * cpu_initclocks() function they get this generic one.  Any platform that needs
 * to do something special can just provide their own implementation, which will
 * override this one due to the weak linkage.
 */
void
arm_generic_initclocks(void)
{

#ifndef NO_EVENTTIMERS
#ifdef SMP
	if (PCPU_GET(cpuid) == 0)
		cpu_initclocks_bsp();
	else
		cpu_initclocks_ap();
#else
	cpu_initclocks_bsp();
#endif
#endif
}
__weak_reference(arm_generic_initclocks, cpu_initclocks);

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

static u_int
ptrace_get_usr_reg(void *cookie, int reg)
{
	int ret;
	struct thread *td = cookie;

	KASSERT(((reg >= 0) && (reg <= ARM_REG_NUM_PC)),
	 ("reg is outside range"));

	switch(reg) {
	case ARM_REG_NUM_PC:
		ret = td->td_frame->tf_pc;
		break;
	case ARM_REG_NUM_LR:
		ret = td->td_frame->tf_usr_lr;
		break;
	case ARM_REG_NUM_SP:
		ret = td->td_frame->tf_usr_sp;
		break;
	default:
		ret = *((register_t*)&td->td_frame->tf_r0 + reg);
		break;
	}

	return (ret);
}

static u_int
ptrace_get_usr_int(void* cookie, vm_offset_t offset, u_int* val)
{
	struct thread *td = cookie;
	u_int error;

	error = ptrace_read_int(td, offset, val);

	return (error);
}

/**
 * This function parses current instruction opcode and decodes
 * any possible jump (change in PC) which might occur after
 * the instruction is executed.
 *
 * @param     td                Thread structure of analysed task
 * @param     cur_instr         Currently executed instruction
 * @param     alt_next_address  Pointer to the variable where
 *                              the destination address of the
 *                              jump instruction shall be stored.
 *
 * @return    <0>               when jump is possible
 *            <EINVAL>          otherwise
 */
static int
ptrace_get_alternative_next(struct thread *td, uint32_t cur_instr,
    uint32_t *alt_next_address)
{
	int error;

	if (inst_branch(cur_instr) || inst_call(cur_instr) ||
	    inst_return(cur_instr)) {
		error = arm_predict_branch(td, cur_instr, td->td_frame->tf_pc,
		    alt_next_address, ptrace_get_usr_reg, ptrace_get_usr_int);

		return (error);
	}

	return (EINVAL);
}

int
ptrace_single_step(struct thread *td)
{
	struct proc *p;
	int error, error_alt;
	uint32_t cur_instr, alt_next = 0;

	/* TODO: This needs to be updated for Thumb-2 */
	if ((td->td_frame->tf_spsr & PSR_T) != 0)
		return (EINVAL);

	KASSERT(td->td_md.md_ptrace_instr == 0,
	 ("Didn't clear single step"));
	KASSERT(td->td_md.md_ptrace_instr_alt == 0,
	 ("Didn't clear alternative single step"));
	p = td->td_proc;
	PROC_UNLOCK(p);

	error = ptrace_read_int(td, td->td_frame->tf_pc,
	    &cur_instr);
	if (error)
		goto out;

	error = ptrace_read_int(td, td->td_frame->tf_pc + INSN_SIZE,
	    &td->td_md.md_ptrace_instr);
	if (error == 0) {
		error = ptrace_write_int(td, td->td_frame->tf_pc + INSN_SIZE,
		    PTRACE_BREAKPOINT);
		if (error) {
			td->td_md.md_ptrace_instr = 0;
		} else {
			td->td_md.md_ptrace_addr = td->td_frame->tf_pc +
			    INSN_SIZE;
		}
	}

	error_alt = ptrace_get_alternative_next(td, cur_instr, &alt_next);
	if (error_alt == 0) {
		error_alt = ptrace_read_int(td, alt_next,
		    &td->td_md.md_ptrace_instr_alt);
		if (error_alt) {
			td->td_md.md_ptrace_instr_alt = 0;
		} else {
			error_alt = ptrace_write_int(td, alt_next,
			    PTRACE_BREAKPOINT);
			if (error_alt)
				td->td_md.md_ptrace_instr_alt = 0;
			else
				td->td_md.md_ptrace_addr_alt = alt_next;
		}
	}

out:
	PROC_LOCK(p);
	return ((error != 0) && (error_alt != 0));
}

int
ptrace_clear_single_step(struct thread *td)
{
	struct proc *p;

	/* TODO: This needs to be updated for Thumb-2 */
	if ((td->td_frame->tf_spsr & PSR_T) != 0)
		return (EINVAL);

	if (td->td_md.md_ptrace_instr != 0) {
		p = td->td_proc;
		PROC_UNLOCK(p);
		ptrace_write_int(td, td->td_md.md_ptrace_addr,
		    td->td_md.md_ptrace_instr);
		PROC_LOCK(p);
		td->td_md.md_ptrace_instr = 0;
	}

	if (td->td_md.md_ptrace_instr_alt != 0) {
		p = td->td_proc;
		PROC_UNLOCK(p);
		ptrace_write_int(td, td->td_md.md_ptrace_addr_alt,
		    td->td_md.md_ptrace_instr_alt);
		PROC_LOCK(p);
		td->td_md.md_ptrace_instr_alt = 0;
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
		cspr = disable_interrupts(PSR_I | PSR_F);
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

	if (clear_ret & GET_MC_CLEAR_RET) {
		gr[_REG_R0] = 0;
		gr[_REG_CPSR] = tf->tf_spsr & ~PSR_C;
	} else {
		gr[_REG_R0]   = tf->tf_r0;
		gr[_REG_CPSR] = tf->tf_spsr;
	}
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

	return (0);
}

/*
 * Set machine context.
 *
 * However, we don't set any but the user modifiable flags, and we won't
 * touch the cs selector.
 */
int
set_mcontext(struct thread *td, mcontext_t *mcp)
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
	ucontext_t uc;
	int spsr;

	if (uap == NULL)
		return (EFAULT);
	if (copyin(uap->sigcntxp, &uc, sizeof(uc)))
		return (EFAULT);
	/*
	 * Make sure the processor mode has not been tampered with and
	 * interrupts have not been disabled.
	 */
	spsr = uc.uc_mcontext.__gregs[_REG_CPSR];
	if ((spsr & PSR_MODE) != PSR_USR32_MODE ||
	    (spsr & (PSR_I | PSR_F)) != 0)
		return (EINVAL);
		/* Restore register context. */
	set_mcontext(td, &uc.uc_mcontext);

	/* Restore signal mask. */
	kern_sigprocmask(td, SIG_SETMASK, &uc.uc_sigmask, NULL, 0);

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
	pcb->pcb_regs.sf_r4 = tf->tf_r4;
	pcb->pcb_regs.sf_r5 = tf->tf_r5;
	pcb->pcb_regs.sf_r6 = tf->tf_r6;
	pcb->pcb_regs.sf_r7 = tf->tf_r7;
	pcb->pcb_regs.sf_r8 = tf->tf_r8;
	pcb->pcb_regs.sf_r9 = tf->tf_r9;
	pcb->pcb_regs.sf_r10 = tf->tf_r10;
	pcb->pcb_regs.sf_r11 = tf->tf_r11;
	pcb->pcb_regs.sf_r12 = tf->tf_r12;
	pcb->pcb_regs.sf_pc = tf->tf_pc;
	pcb->pcb_regs.sf_lr = tf->tf_usr_lr;
	pcb->pcb_regs.sf_sp = tf->tf_usr_sp;
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
		db_fetch_ksymtab(zstart, zend);
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
#if __ARM_ARCH >= 6
	set_curthread(&thread0);
#endif
	pcpu_init(pcpup, 0, sizeof(struct pcpu));
	PCPU_SET(curthread, &thread0);
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
	    (abp->abp_r2 + KERNVIRTADDR - abp->abp_physaddr);

	/* xxx - Need to also look for binary device tree */
	if (ATAG_TAG(walker) != ATAG_CORE)
		return 0;

	atag_list = walker;
	while (ATAG_TAG(walker) != ATAG_NONE) {
		switch (ATAG_TAG(walker)) {
		case ATAG_CORE:
			break;
		case ATAG_MEM:
			arm_physmem_hardware_region(walker->u.tag_mem.start,
			    walker->u.tag_mem.size);
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
#ifdef DDB
	vm_offset_t ksym_start;
	vm_offset_t ksym_end;
#endif

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
	db_fetch_ksymtab(ksym_start, ksym_end);
#endif
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
		(thread0.td_kstack + kstack_pages * PAGE_SIZE) - 1;
	thread0.td_pcb->pcb_flags = 0;
	thread0.td_pcb->pcb_vfpcpu = -1;
	thread0.td_pcb->pcb_vfpstate.fpscr = VFPSCR_DN;
	thread0.td_frame = &proc0_tf;
	pcpup->pc_curpcb = thread0.td_pcb;
}

int
arm_predict_branch(void *cookie, u_int insn, register_t pc, register_t *new_pc,
    u_int (*fetch_reg)(void*, int), u_int (*read_int)(void*, vm_offset_t, u_int*))
{
	u_int addr, nregs, offset = 0;
	int error = 0;

	switch ((insn >> 24) & 0xf) {
	case 0x2:	/* add pc, reg1, #value */
	case 0x0:	/* add pc, reg1, reg2, lsl #offset */
		addr = fetch_reg(cookie, (insn >> 16) & 0xf);
		if (((insn >> 16) & 0xf) == 15)
			addr += 8;
		if (insn & 0x0200000) {
			offset = (insn >> 7) & 0x1e;
			offset = (insn & 0xff) << (32 - offset) |
			    (insn & 0xff) >> offset;
		} else {

			offset = fetch_reg(cookie, insn & 0x0f);
			if ((insn & 0x0000ff0) != 0x00000000) {
				if (insn & 0x10)
					nregs = fetch_reg(cookie,
					    (insn >> 8) & 0xf);
				else
					nregs = (insn >> 7) & 0x1f;
				switch ((insn >> 5) & 3) {
				case 0:
					/* lsl */
					offset = offset << nregs;
					break;
				case 1:
					/* lsr */
					offset = offset >> nregs;
					break;
				default:
					break; /* XXX */
				}

			}
			*new_pc = addr + offset;
			return (0);

		}

	case 0xa:	/* b ... */
	case 0xb:	/* bl ... */
		addr = ((insn << 2) & 0x03ffffff);
		if (addr & 0x02000000)
			addr |= 0xfc000000;
		*new_pc = (pc + 8 + addr);
		return (0);
	case 0x7:	/* ldr pc, [pc, reg, lsl #2] */
		addr = fetch_reg(cookie, insn & 0xf);
		addr = pc + 8 + (addr << 2);
		error = read_int(cookie, addr, &addr);
		*new_pc = addr;
		return (error);
	case 0x1:	/* mov pc, reg */
		*new_pc = fetch_reg(cookie, insn & 0xf);
		return (0);
	case 0x4:
	case 0x5:	/* ldr pc, [reg] */
		addr = fetch_reg(cookie, (insn >> 16) & 0xf);
		/* ldr pc, [reg, #offset] */
		if (insn & (1 << 24))
			offset = insn & 0xfff;
		if (insn & 0x00800000)
			addr += offset;
		else
			addr -= offset;
		error = read_int(cookie, addr, &addr);
		*new_pc = addr;

		return (error);
	case 0x8:	/* ldmxx reg, {..., pc} */
	case 0x9:
		addr = fetch_reg(cookie, (insn >> 16) & 0xf);
		nregs = (insn  & 0x5555) + ((insn  >> 1) & 0x5555);
		nregs = (nregs & 0x3333) + ((nregs >> 2) & 0x3333);
		nregs = (nregs + (nregs >> 4)) & 0x0f0f;
		nregs = (nregs + (nregs >> 8)) & 0x001f;
		switch ((insn >> 23) & 0x3) {
		case 0x0:	/* ldmda */
			addr = addr - 0;
			break;
		case 0x1:	/* ldmia */
			addr = addr + 0 + ((nregs - 1) << 2);
			break;
		case 0x2:	/* ldmdb */
			addr = addr - 4;
			break;
		case 0x3:	/* ldmib */
			addr = addr + 4 + ((nregs - 1) << 2);
			break;
		}
		error = read_int(cookie, addr, &addr);
		*new_pc = addr;

		return (error);
	default:
		return (EINVAL);
	}
}

#ifdef ARM_NEW_PMAP
void
set_stackptrs(int cpu)
{

	set_stackptr(PSR_IRQ32_MODE,
	    irqstack + ((IRQ_STACK_SIZE * PAGE_SIZE) * (cpu + 1)));
	set_stackptr(PSR_ABT32_MODE,
	    abtstack + ((ABT_STACK_SIZE * PAGE_SIZE) * (cpu + 1)));
	set_stackptr(PSR_UND32_MODE,
	    undstack + ((UND_STACK_SIZE * PAGE_SIZE) * (cpu + 1)));
}
#else
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
#endif

#ifdef EFI
#define efi_next_descriptor(ptr, size) \
	((struct efi_md *)(((uint8_t *) ptr) + size))

static void
add_efi_map_entries(struct efi_map_header *efihdr, struct mem_region *mr,
    int *mrcnt, uint32_t *memsize)
{
	struct efi_md *map, *p;
	const char *type;
	size_t efisz, memory_size;
	int ndesc, i, j;

	static const char *types[] = {
		"Reserved",
		"LoaderCode",
		"LoaderData",
		"BootServicesCode",
		"BootServicesData",
		"RuntimeServicesCode",
		"RuntimeServicesData",
		"ConventionalMemory",
		"UnusableMemory",
		"ACPIReclaimMemory",
		"ACPIMemoryNVS",
		"MemoryMappedIO",
		"MemoryMappedIOPortSpace",
		"PalCode"
	};

	*mrcnt = 0;
	*memsize = 0;

	/*
	 * Memory map data provided by UEFI via the GetMemoryMap
	 * Boot Services API.
	 */
	efisz = roundup2(sizeof(struct efi_map_header), 0x10);
	map = (struct efi_md *)((uint8_t *)efihdr + efisz);

	if (efihdr->descriptor_size == 0)
		return;
	ndesc = efihdr->memory_size / efihdr->descriptor_size;

	if (boothowto & RB_VERBOSE)
		printf("%23s %12s %12s %8s %4s\n",
		    "Type", "Physical", "Virtual", "#Pages", "Attr");

	memory_size = 0;
	for (i = 0, j = 0, p = map; i < ndesc; i++,
	    p = efi_next_descriptor(p, efihdr->descriptor_size)) {
		if (boothowto & RB_VERBOSE) {
			if (p->md_type <= EFI_MD_TYPE_PALCODE)
				type = types[p->md_type];
			else
				type = "<INVALID>";
			printf("%23s %012llx %12p %08llx ", type, p->md_phys,
			    p->md_virt, p->md_pages);
			if (p->md_attr & EFI_MD_ATTR_UC)
				printf("UC ");
			if (p->md_attr & EFI_MD_ATTR_WC)
				printf("WC ");
			if (p->md_attr & EFI_MD_ATTR_WT)
				printf("WT ");
			if (p->md_attr & EFI_MD_ATTR_WB)
				printf("WB ");
			if (p->md_attr & EFI_MD_ATTR_UCE)
				printf("UCE ");
			if (p->md_attr & EFI_MD_ATTR_WP)
				printf("WP ");
			if (p->md_attr & EFI_MD_ATTR_RP)
				printf("RP ");
			if (p->md_attr & EFI_MD_ATTR_XP)
				printf("XP ");
			if (p->md_attr & EFI_MD_ATTR_RT)
				printf("RUNTIME");
			printf("\n");
		}

		switch (p->md_type) {
		case EFI_MD_TYPE_CODE:
		case EFI_MD_TYPE_DATA:
		case EFI_MD_TYPE_BS_CODE:
		case EFI_MD_TYPE_BS_DATA:
		case EFI_MD_TYPE_FREE:
			/*
			 * We're allowed to use any entry with these types.
			 */
			break;
		default:
			continue;
		}

		j++;
		if (j >= FDT_MEM_REGIONS)
			break;

		mr[j].mr_start = p->md_phys;
		mr[j].mr_size = p->md_pages * PAGE_SIZE;
		memory_size += mr[j].mr_size;
	}

	*mrcnt = j;
	*memsize = memory_size;
}
#endif /* EFI */

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
	char *cp;

	debugf("loader passed (static) kenv:\n");
	if (kern_envp == NULL) {
		debugf(" no env, null ptr\n");
		return;
	}
	debugf(" kern_envp = 0x%08x\n", (uint32_t)kern_envp);

	for (cp = kern_envp; cp != NULL; cp = kenv_next(cp))
		debugf(" %x %s\n", (uint32_t)cp, cp);
}

#ifndef ARM_NEW_PMAP
void *
initarm(struct arm_boot_params *abp)
{
	struct mem_region mem_regions[FDT_MEM_REGIONS];
	struct pv_addr kernel_l1pt;
	struct pv_addr dpcpu;
	vm_offset_t dtbp, freemempos, l2_start, lastaddr;
	uint32_t memsize, l2size;
	char *env;
	void *kmdp;
	u_int l1pagetable;
	int i, j, err_devmap, mem_regions_sz;

	lastaddr = parse_boot_param(abp);
	arm_physmem_kernaddr = abp->abp_physaddr;

	memsize = 0;

	cpuinfo_init();
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
		panic("Cannot install FDT");

	if (OF_init((void *)dtbp) != 0)
		panic("OF_init failed with the found device tree");

	/* Grab physical memory regions information from device tree. */
	if (fdt_get_mem_regions(mem_regions, &mem_regions_sz, &memsize) != 0)
		panic("Cannot get physical memory regions");
	arm_physmem_hardware_regions(mem_regions, mem_regions_sz);

	/* Grab reserved memory regions information from device tree. */
	if (fdt_get_reserved_regions(mem_regions, &mem_regions_sz) == 0)
		arm_physmem_exclude_regions(mem_regions, mem_regions_sz,
		    EXFLAG_NODUMP | EXFLAG_NOALLOC);

	/* Platform-specific initialisation */
	platform_probe_and_attach();

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
	(var).pv_pa = (var).pv_va + (abp->abp_physaddr - KERNVIRTADDR);

#define alloc_pages(var, np)						\
	(var) = freemempos;						\
	freemempos += (np * PAGE_SIZE);					\
	memset((char *)(var), 0, ((np) * PAGE_SIZE));

	while (((freemempos - L1_TABLE_SIZE) & (L1_TABLE_SIZE - 1)) != 0)
		freemempos += PAGE_SIZE;
	valloc_pages(kernel_l1pt, L1_TABLE_SIZE / PAGE_SIZE);

	for (i = 0, j = 0; i < l2size; ++i) {
		if (!(i % (PAGE_SIZE / L2_TABLE_SIZE_REAL))) {
			valloc_pages(kernel_pt_table[i],
			    L2_TABLE_SIZE / PAGE_SIZE);
			j = i;
		} else {
			kernel_pt_table[i].pv_va = kernel_pt_table[j].pv_va +
			    L2_TABLE_SIZE_REAL * (i - j);
			kernel_pt_table[i].pv_pa =
			    kernel_pt_table[i].pv_va - KERNVIRTADDR +
			    abp->abp_physaddr;

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
	valloc_pages(kernelstack, kstack_pages * MAXCPU);
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
	pmap_map_chunk(l1pagetable, KERNVIRTADDR, abp->abp_physaddr,
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
	err_devmap = platform_devmap_init();
	arm_devmap_bootstrap(l1pagetable, NULL);
	vm_max_kernel_address = platform_lastaddr();

	cpu_domains((DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL * 2)) | DOMAIN_CLIENT);
	pmap_pa = kernel_l1pt.pv_pa;
	setttb(kernel_l1pt.pv_pa);
	cpu_tlb_flushID();
	cpu_domains(DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL * 2));

	/*
	 * Now that proper page tables are installed, call cpu_setup() to enable
	 * instruction and data caches and other chip-specific features.
	 */
	cpu_setup();

	/*
	 * Only after the SOC registers block is mapped we can perform device
	 * tree fixups, as they may attempt to read parameters from hardware.
	 */
	OF_interpret("perform-fixup", 0);

	platform_gpio_init();

	cninit();

	debugf("initarm: console initialized\n");
	debugf(" arg1 kmdp = 0x%08x\n", (uint32_t)kmdp);
	debugf(" boothowto = 0x%08x\n", boothowto);
	debugf(" dtbp = 0x%08x\n", (uint32_t)dtbp);
	print_kenv();

	env = kern_getenv("kernelname");
	if (env != NULL) {
		strlcpy(kernelname, env, sizeof(kernelname));
		freeenv(env);
	}

	if (err_devmap != 0)
		printf("WARNING: could not fully configure devmap, error=%d\n",
		    err_devmap);

	platform_late_init();

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

	undefined_init();

	init_proc0(kernelstack.pv_va);

	arm_vector_init(ARM_VECTORS_HIGH, ARM_VEC_ALL);
	pmap_bootstrap(freemempos, &kernel_l1pt);
	msgbufp = (void *)msgbufpv.pv_va;
	msgbufinit(msgbufp, msgbufsize);
	mutex_init();

	/*
	 * Exclude the kernel (and all the things we allocated which immediately
	 * follow the kernel) from the VM allocation pool but not from crash
	 * dumps.  virtual_avail is a global variable which tracks the kva we've
	 * "allocated" while setting up pmaps.
	 *
	 * Prepare the list of physical memory available to the vm subsystem.
	 */
	arm_physmem_exclude_region(abp->abp_physaddr,
	    (virtual_avail - KERNVIRTADDR), EXFLAG_NOALLOC);
	arm_physmem_init_kernel_globals();

	init_param2(physmem);
	kdb_init();

	return ((void *)(kernelstack.pv_va + USPACE_SVC_STACK_TOP -
	    sizeof(struct pcb)));
}
#else /* !ARM_NEW_PMAP */
void *
initarm(struct arm_boot_params *abp)
{
	struct mem_region mem_regions[FDT_MEM_REGIONS];
	vm_paddr_t lastaddr;
	vm_offset_t dtbp, kernelstack, dpcpu;
	uint32_t memsize;
	char *env;
	void *kmdp;
	int err_devmap, mem_regions_sz;
#ifdef EFI
	struct efi_map_header *efihdr;
#endif

	/* get last allocated physical address */
	arm_physmem_kernaddr = abp->abp_physaddr;
	lastaddr = parse_boot_param(abp) - KERNVIRTADDR + arm_physmem_kernaddr;

	memsize = 0;
	set_cpufuncs();
	cpuinfo_init();

	/*
	 * Find the dtb passed in by the boot loader.
	 */
	kmdp = preload_search_by_type("elf kernel");
	dtbp = MD_FETCH(kmdp, MODINFOMD_DTBP, vm_offset_t);
#if defined(FDT_DTB_STATIC)
	/*
	 * In case the device tree blob was not retrieved (from metadata) try
	 * to use the statically embedded one.
	 */
	if (dtbp == (vm_offset_t)NULL)
		dtbp = (vm_offset_t)&fdt_static_dtb;
#endif

	if (OF_install(OFW_FDT, 0) == FALSE)
		panic("Cannot install FDT");

	if (OF_init((void *)dtbp) != 0)
		panic("OF_init failed with the found device tree");

#ifdef EFI
	efihdr = (struct efi_map_header *)preload_search_info(kmdp,
	    MODINFO_METADATA | MODINFOMD_EFI_MAP);
	if (efihdr != NULL) {
		add_efi_map_entries(efihdr, mem_regions, &mem_regions_sz,
		   &memsize);
	} else
#endif
	{
		/* Grab physical memory regions information from device tree. */
		if (fdt_get_mem_regions(mem_regions, &mem_regions_sz,
		    &memsize) != 0)
			panic("Cannot get physical memory regions");
	}
	arm_physmem_hardware_regions(mem_regions, mem_regions_sz);

	/* Grab reserved memory regions information from device tree. */
	if (fdt_get_reserved_regions(mem_regions, &mem_regions_sz) == 0)
		arm_physmem_exclude_regions(mem_regions, mem_regions_sz,
		    EXFLAG_NODUMP | EXFLAG_NOALLOC);

	/*
	 * Set TEX remapping registers.
	 * Setup kernel page tables and switch to kernel L1 page table.
	 */
	pmap_set_tex();
	pmap_bootstrap_prepare(lastaddr);

	/*
	 * Now that proper page tables are installed, call cpu_setup() to enable
	 * instruction and data caches and other chip-specific features.
	 */
	cpu_setup();

	/* Platform-specific initialisation */
	platform_probe_and_attach();
	pcpu0_init();

	/* Do basic tuning, hz etc */
	init_param1();

	/*
	 * Allocate a page for the system page mapped to 0xffff0000
	 * This page will just contain the system vectors and can be
	 * shared by all processes.
	 */
	systempage = pmap_preboot_get_pages(1);

	/* Map the vector page. */
	pmap_preboot_map_pages(systempage, ARM_VECTORS_HIGH,  1);
	if (virtual_end >= ARM_VECTORS_HIGH)
		virtual_end = ARM_VECTORS_HIGH - 1;

	/* Allocate dynamic per-cpu area. */
	dpcpu = pmap_preboot_get_vpages(DPCPU_SIZE / PAGE_SIZE);
	dpcpu_init((void *)dpcpu, 0);

	/* Allocate stacks for all modes */
	irqstack    = pmap_preboot_get_vpages(IRQ_STACK_SIZE * MAXCPU);
	abtstack    = pmap_preboot_get_vpages(ABT_STACK_SIZE * MAXCPU);
	undstack    = pmap_preboot_get_vpages(UND_STACK_SIZE * MAXCPU );
	kernelstack = pmap_preboot_get_vpages(kstack_pages * MAXCPU);

	/* Allocate message buffer. */
	msgbufp = (void *)pmap_preboot_get_vpages(
	    round_page(msgbufsize) / PAGE_SIZE);

	/*
	 * Pages were allocated during the secondary bootstrap for the
	 * stacks for different CPU modes.
	 * We must now set the r13 registers in the different CPU modes to
	 * point to these stacks.
	 * Since the ARM stacks use STMFD etc. we must set r13 to the top end
	 * of the stack memory.
	 */
	set_stackptrs(0);
	mutex_init();

	/* Establish static device mappings. */
	err_devmap = platform_devmap_init();
	arm_devmap_bootstrap(0, NULL);
	vm_max_kernel_address = platform_lastaddr();

	/*
	 * Only after the SOC registers block is mapped we can perform device
	 * tree fixups, as they may attempt to read parameters from hardware.
	 */
	OF_interpret("perform-fixup", 0);
	platform_gpio_init();
	cninit();

	debugf("initarm: console initialized\n");
	debugf(" arg1 kmdp = 0x%08x\n", (uint32_t)kmdp);
	debugf(" boothowto = 0x%08x\n", boothowto);
	debugf(" dtbp = 0x%08x\n", (uint32_t)dtbp);
	debugf(" lastaddr1: 0x%08x\n", lastaddr);
	print_kenv();

	env = kern_getenv("kernelname");
	if (env != NULL)
		strlcpy(kernelname, env, sizeof(kernelname));

	if (err_devmap != 0)
		printf("WARNING: could not fully configure devmap, error=%d\n",
		    err_devmap);

	platform_late_init();

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
	/* Set stack for exception handlers */
	undefined_init();
	init_proc0(kernelstack);
	arm_vector_init(ARM_VECTORS_HIGH, ARM_VEC_ALL);
	enable_interrupts(PSR_A);
	pmap_bootstrap(0);

	/* Exclude the kernel (and all the things we allocated which immediately
	 * follow the kernel) from the VM allocation pool but not from crash
	 * dumps.  virtual_avail is a global variable which tracks the kva we've
	 * "allocated" while setting up pmaps.
	 *
	 * Prepare the list of physical memory available to the vm subsystem.
	 */
	arm_physmem_exclude_region(abp->abp_physaddr,
		pmap_preboot_get_pages(0) - abp->abp_physaddr, EXFLAG_NOALLOC);
	arm_physmem_init_kernel_globals();

	init_param2(physmem);
	/* Init message buffer. */
	msgbufinit(msgbufp, msgbufsize);
	kdb_init();
	return ((void *)STACKALIGN(thread0.td_pcb));

}

#endif /* !ARM_NEW_PMAP */
#endif /* FDT */
