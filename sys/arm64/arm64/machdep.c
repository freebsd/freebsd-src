/*-
 * Copyright (c) 2014 Andrew Turner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "opt_platform.h"
#include "opt_ddb.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/cpu.h>
#include <sys/efi.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/kdb.h> 
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/linker.h>
#include <sys/msgbuf.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/reboot.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/ucontext.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_pager.h>

#include <machine/armreg.h>
#include <machine/cpu.h>
#include <machine/debug_monitor.h>
#include <machine/kdb.h>
#include <machine/devmap.h>
#include <machine/machdep.h>
#include <machine/metadata.h>
#include <machine/pcb.h>
#include <machine/reg.h>
#include <machine/vmparam.h>

#ifdef VFP
#include <machine/vfp.h>
#endif

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#endif

struct pcpu __pcpu[MAXCPU];

static struct trapframe proc0_tf;

vm_paddr_t phys_avail[PHYS_AVAIL_SIZE + 2];
vm_paddr_t dump_avail[PHYS_AVAIL_SIZE + 2];

int early_boot = 1;
int cold = 1;
long realmem = 0;
long Maxmem = 0;

#define	PHYSMAP_SIZE	(2 * (VM_PHYSSEG_MAX - 1))
vm_paddr_t physmap[PHYSMAP_SIZE];
u_int physmap_idx;

struct kva_md_info kmi;

int64_t dcache_line_size;	/* The minimum D cache line size */
int64_t icache_line_size;	/* The minimum I cache line size */
int64_t idcache_line_size;	/* The minimum cache line size */

static void
cpu_startup(void *dummy)
{

	identify_cpu();

	vm_ksubmap_init(&kmi);
	bufinit();
	vm_pager_bufferinit();
}

SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL);

int
cpu_idle_wakeup(int cpu)
{

	return (0);
}

void
bzero(void *buf, size_t len)
{
	uint8_t *p;

	p = buf;
	while(len-- > 0)
		*p++ = 0;
}

int
fill_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *frame;

	frame = td->td_frame;
	regs->sp = frame->tf_sp;
	regs->lr = frame->tf_lr;
	regs->elr = frame->tf_elr;
	regs->spsr = frame->tf_spsr;

	memcpy(regs->x, frame->tf_x, sizeof(regs->x));

	return (0);
}

int
set_regs(struct thread *td, struct reg *regs)
{
	struct trapframe *frame;

	frame = td->td_frame;
	frame->tf_sp = regs->sp;
	frame->tf_lr = regs->lr;
	frame->tf_elr = regs->elr;
	frame->tf_spsr = regs->spsr;

	memcpy(frame->tf_x, regs->x, sizeof(frame->tf_x));

	return (0);
}

int
fill_fpregs(struct thread *td, struct fpreg *regs)
{
#ifdef VFP
	struct pcb *pcb;

	pcb = td->td_pcb;
	if ((pcb->pcb_fpflags & PCB_FP_STARTED) != 0) {
		/*
		 * If we have just been running VFP instructions we will
		 * need to save the state to memcpy it below.
		 */
		vfp_save_state(td, pcb);

		memcpy(regs->fp_q, pcb->pcb_vfp, sizeof(regs->fp_q));
		regs->fp_cr = pcb->pcb_fpcr;
		regs->fp_sr = pcb->pcb_fpsr;
	} else
#endif
		memset(regs->fp_q, 0, sizeof(regs->fp_q));
	return (0);
}

int
set_fpregs(struct thread *td, struct fpreg *regs)
{
#ifdef VFP
	struct pcb *pcb;

	pcb = td->td_pcb;
	memcpy(pcb->pcb_vfp, regs->fp_q, sizeof(regs->fp_q));
	pcb->pcb_fpcr = regs->fp_cr;
	pcb->pcb_fpsr = regs->fp_sr;
#endif
	return (0);
}

int
fill_dbregs(struct thread *td, struct dbreg *regs)
{

	panic("ARM64TODO: fill_dbregs");
}

int
set_dbregs(struct thread *td, struct dbreg *regs)
{

	panic("ARM64TODO: set_dbregs");
}

int
ptrace_set_pc(struct thread *td, u_long addr)
{

	panic("ARM64TODO: ptrace_set_pc");
	return (0);
}

int
ptrace_single_step(struct thread *td)
{

	/* TODO; */
	return (0);
}

int
ptrace_clear_single_step(struct thread *td)
{

	/* TODO; */
	return (0);
}

void
exec_setregs(struct thread *td, struct image_params *imgp, u_long stack)
{
	struct trapframe *tf = td->td_frame;

	memset(tf, 0, sizeof(struct trapframe));

	tf->tf_sp = stack;
	tf->tf_lr = imgp->entry_addr;
	tf->tf_elr = imgp->entry_addr;
}

/* Sanity check these are the same size, they will be memcpy'd to and fro */
CTASSERT(sizeof(((struct trapframe *)0)->tf_x) ==
    sizeof((struct gpregs *)0)->gp_x);
CTASSERT(sizeof(((struct trapframe *)0)->tf_x) ==
    sizeof((struct reg *)0)->x);

int
get_mcontext(struct thread *td, mcontext_t *mcp, int clear_ret)
{
	struct trapframe *tf = td->td_frame;

	if (clear_ret & GET_MC_CLEAR_RET) {
		mcp->mc_gpregs.gp_x[0] = 0;
		mcp->mc_gpregs.gp_spsr = tf->tf_spsr & ~PSR_C;
	} else {
		mcp->mc_gpregs.gp_x[0] = tf->tf_x[0];
		mcp->mc_gpregs.gp_spsr = tf->tf_spsr;
	}

	memcpy(&mcp->mc_gpregs.gp_x[1], &tf->tf_x[1],
	    sizeof(mcp->mc_gpregs.gp_x[1]) * (nitems(mcp->mc_gpregs.gp_x) - 1));

	mcp->mc_gpregs.gp_sp = tf->tf_sp;
	mcp->mc_gpregs.gp_lr = tf->tf_lr;
	mcp->mc_gpregs.gp_elr = tf->tf_elr;

	return (0);
}

int
set_mcontext(struct thread *td, mcontext_t *mcp)
{
	struct trapframe *tf = td->td_frame;

	memcpy(tf->tf_x, mcp->mc_gpregs.gp_x, sizeof(tf->tf_x));

	tf->tf_sp = mcp->mc_gpregs.gp_sp;
	tf->tf_lr = mcp->mc_gpregs.gp_lr;
	tf->tf_elr = mcp->mc_gpregs.gp_elr;
	tf->tf_spsr = mcp->mc_gpregs.gp_spsr;

	return (0);
}

static void
get_fpcontext(struct thread *td, mcontext_t *mcp)
{
#ifdef VFP
	struct pcb *curpcb;

	critical_enter();

	curpcb = curthread->td_pcb;

	if ((curpcb->pcb_fpflags & PCB_FP_STARTED) != 0) {
		/*
		 * If we have just been running VFP instructions we will
		 * need to save the state to memcpy it below.
		 */
		vfp_save_state(td, curpcb);

		memcpy(mcp->mc_fpregs.fp_q, curpcb->pcb_vfp,
		    sizeof(mcp->mc_fpregs));
		mcp->mc_fpregs.fp_cr = curpcb->pcb_fpcr;
		mcp->mc_fpregs.fp_sr = curpcb->pcb_fpsr;
		mcp->mc_fpregs.fp_flags = curpcb->pcb_fpflags;
		mcp->mc_flags |= _MC_FP_VALID;
	}

	critical_exit();
#endif
}

static void
set_fpcontext(struct thread *td, mcontext_t *mcp)
{
#ifdef VFP
	struct pcb *curpcb;

	critical_enter();

	if ((mcp->mc_flags & _MC_FP_VALID) != 0) {
		curpcb = curthread->td_pcb;

		/*
		 * Discard any vfp state for the current thread, we
		 * are about to override it.
		 */
		vfp_discard(td);

		memcpy(curpcb->pcb_vfp, mcp->mc_fpregs.fp_q,
		    sizeof(mcp->mc_fpregs));
		curpcb->pcb_fpcr = mcp->mc_fpregs.fp_cr;
		curpcb->pcb_fpsr = mcp->mc_fpregs.fp_sr;
		curpcb->pcb_fpflags = mcp->mc_fpregs.fp_flags;
	}

	critical_exit();
#endif
}

void
cpu_idle(int busy)
{

	spinlock_enter();
	if (!busy)
		cpu_idleclock();
	if (!sched_runnable())
		__asm __volatile(
		    "dsb sy \n"
		    "wfi    \n");
	if (!busy)
		cpu_activeclock();
	spinlock_exit();
}

void
cpu_halt(void)
{

	/* We should have shutdown by now, if not enter a low power sleep */
	intr_disable();
	while (1) {
		__asm __volatile("wfi");
	}
}

/*
 * Flush the D-cache for non-DMA I/O so that the I-cache can
 * be made coherent later.
 */
void
cpu_flush_dcache(void *ptr, size_t len)
{

	/* ARM64TODO TBD */
}

/* Get current clock frequency for the given CPU ID. */
int
cpu_est_clockrate(int cpu_id, uint64_t *rate)
{

	panic("ARM64TODO: cpu_est_clockrate");
}

void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t size)
{

	pcpu->pc_acpi_id = 0xffffffff;
}

void
spinlock_enter(void)
{
	struct thread *td;
	register_t daif;

	td = curthread;
	if (td->td_md.md_spinlock_count == 0) {
		daif = intr_disable();
		td->td_md.md_spinlock_count = 1;
		td->td_md.md_saved_daif = daif;
	} else
		td->td_md.md_spinlock_count++;
	critical_enter();
}

void
spinlock_exit(void)
{
	struct thread *td;
	register_t daif;

	td = curthread;
	critical_exit();
	daif = td->td_md.md_saved_daif;
	td->td_md.md_spinlock_count--;
	if (td->td_md.md_spinlock_count == 0)
		intr_restore(daif);
}

#ifndef	_SYS_SYSPROTO_H_
struct sigreturn_args {
	ucontext_t *ucp;
};
#endif

int
sys_sigreturn(struct thread *td, struct sigreturn_args *uap)
{
	ucontext_t uc;
	uint32_t spsr;

	if (uap == NULL)
		return (EFAULT);
	if (copyin(uap->sigcntxp, &uc, sizeof(uc)))
		return (EFAULT);

	spsr = uc.uc_mcontext.mc_gpregs.gp_spsr;
	if ((spsr & PSR_M_MASK) != PSR_M_EL0t ||
	    (spsr & (PSR_F | PSR_I | PSR_A | PSR_D)) != 0)
		return (EINVAL); 

	set_mcontext(td, &uc.uc_mcontext);
	set_fpcontext(td, &uc.uc_mcontext);

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
	int i;

	for (i = 0; i < PCB_LR; i++)
		pcb->pcb_x[i] = tf->tf_x[i];

	pcb->pcb_x[PCB_LR] = tf->tf_lr;
	pcb->pcb_pc = tf->tf_elr;
	pcb->pcb_sp = tf->tf_sp;
}

void
sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{
	struct thread *td;
	struct proc *p;
	struct trapframe *tf;
	struct sigframe *fp, frame;
	struct sigacts *psp;
	int code, onstack, sig;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);

	sig = ksi->ksi_signo;
	code = ksi->ksi_code;
	psp = p->p_sigacts;
	mtx_assert(&psp->ps_mtx, MA_OWNED);

	tf = td->td_frame;
	onstack = sigonstack(tf->tf_sp);

	CTR4(KTR_SIG, "sendsig: td=%p (%s) catcher=%p sig=%d", td, p->p_comm,
	    catcher, sig);

	/* Allocate and validate space for the signal handler context. */
	if ((td->td_pflags & TDP_ALTSTACK) != 0 && !onstack &&
	    SIGISMEMBER(psp->ps_sigonstack, sig)) {
		fp = (struct sigframe *)(td->td_sigstk.ss_sp +
		    td->td_sigstk.ss_size);
#if defined(COMPAT_43)
		td->td_sigstk.ss_flags |= SS_ONSTACK;
#endif
	} else {
		fp = (struct sigframe *)td->td_frame->tf_sp;
	}

	/* Make room, keeping the stack aligned */
	fp--;
	fp = (struct sigframe *)STACKALIGN(fp);

	/* Fill in the frame to copy out */
	get_mcontext(td, &frame.sf_uc.uc_mcontext, 0);
	get_fpcontext(td, &frame.sf_uc.uc_mcontext);
	frame.sf_si = ksi->ksi_info;
	frame.sf_uc.uc_sigmask = *mask;
	frame.sf_uc.uc_stack.ss_flags = (td->td_pflags & TDP_ALTSTACK) ?
	    ((onstack) ? SS_ONSTACK : 0) : SS_DISABLE;
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

	tf->tf_x[0]= sig;
	tf->tf_x[1] = (register_t)&fp->sf_si;
	tf->tf_x[2] = (register_t)&fp->sf_uc;

	tf->tf_elr = (register_t)catcher;
	tf->tf_sp = (register_t)fp;
	tf->tf_lr = (register_t)(PS_STRINGS - *(p->p_sysent->sv_szsigcode));

	CTR3(KTR_SIG, "sendsig: return td=%p pc=%#x sp=%#x", td, tf->tf_elr,
	    tf->tf_sp);

	PROC_LOCK(p);
	mtx_lock(&psp->ps_mtx);
}

static void
init_proc0(vm_offset_t kstack)
{
	struct pcpu *pcpup = &__pcpu[0];

	proc_linkup0(&proc0, &thread0);
	thread0.td_kstack = kstack;
	thread0.td_pcb = (struct pcb *)(thread0.td_kstack) - 1;
	thread0.td_pcb->pcb_fpflags = 0;
	thread0.td_pcb->pcb_vfpcpu = UINT_MAX;
	thread0.td_frame = &proc0_tf;
	pcpup->pc_curpcb = thread0.td_pcb;
}

typedef struct {
	uint32_t type;
	uint64_t phys_start;
	uint64_t virt_start;
	uint64_t num_pages;
	uint64_t attr;
} EFI_MEMORY_DESCRIPTOR;

static int
add_physmap_entry(uint64_t base, uint64_t length, vm_paddr_t *physmap,
    u_int *physmap_idxp)
{
	u_int i, insert_idx, _physmap_idx;

	_physmap_idx = *physmap_idxp;

	if (length == 0)
		return (1);

	/*
	 * Find insertion point while checking for overlap.  Start off by
	 * assuming the new entry will be added to the end.
	 */
	insert_idx = _physmap_idx;
	for (i = 0; i <= _physmap_idx; i += 2) {
		if (base < physmap[i + 1]) {
			if (base + length <= physmap[i]) {
				insert_idx = i;
				break;
			}
			if (boothowto & RB_VERBOSE)
				printf(
		    "Overlapping memory regions, ignoring second region\n");
			return (1);
		}
	}

	/* See if we can prepend to the next entry. */
	if (insert_idx <= _physmap_idx &&
	    base + length == physmap[insert_idx]) {
		physmap[insert_idx] = base;
		return (1);
	}

	/* See if we can append to the previous entry. */
	if (insert_idx > 0 && base == physmap[insert_idx - 1]) {
		physmap[insert_idx - 1] += length;
		return (1);
	}

	_physmap_idx += 2;
	*physmap_idxp = _physmap_idx;
	if (_physmap_idx == PHYSMAP_SIZE) {
		printf(
		"Too many segments in the physical address map, giving up\n");
		return (0);
	}

	/*
	 * Move the last 'N' entries down to make room for the new
	 * entry if needed.
	 */
	for (i = _physmap_idx; i > insert_idx; i -= 2) {
		physmap[i] = physmap[i - 2];
		physmap[i + 1] = physmap[i - 1];
	}

	/* Insert the new entry. */
	physmap[insert_idx] = base;
	physmap[insert_idx + 1] = base + length;
	return (1);
}

#define efi_next_descriptor(ptr, size) \
	((struct efi_md *)(((uint8_t *) ptr) + size))

static void
add_efi_map_entries(struct efi_map_header *efihdr, vm_paddr_t *physmap,
    u_int *physmap_idxp)
{
	struct efi_md *map, *p;
	const char *type;
	size_t efisz;
	int ndesc, i;

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

	/*
	 * Memory map data provided by UEFI via the GetMemoryMap
	 * Boot Services API.
	 */
	efisz = (sizeof(struct efi_map_header) + 0xf) & ~0xf;
	map = (struct efi_md *)((uint8_t *)efihdr + efisz); 

	if (efihdr->descriptor_size == 0)
		return;
	ndesc = efihdr->memory_size / efihdr->descriptor_size;

	if (boothowto & RB_VERBOSE)
		printf("%23s %12s %12s %8s %4s\n",
		    "Type", "Physical", "Virtual", "#Pages", "Attr");

	for (i = 0, p = map; i < ndesc; i++,
	    p = efi_next_descriptor(p, efihdr->descriptor_size)) {
		if (boothowto & RB_VERBOSE) {
			if (p->md_type <= EFI_MD_TYPE_PALCODE)
				type = types[p->md_type];
			else
				type = "<INVALID>";
			printf("%23s %012lx %12p %08lx ", type, p->md_phys,
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

		if (!add_physmap_entry(p->md_phys, (p->md_pages * PAGE_SIZE),
		    physmap, physmap_idxp))
			break;
	}
}

#ifdef FDT
static void
try_load_dtb(caddr_t kmdp)
{
	vm_offset_t dtbp;

	dtbp = MD_FETCH(kmdp, MODINFOMD_DTBP, vm_offset_t);
	if (dtbp == (vm_offset_t)NULL) {
		printf("ERROR loading DTB\n");
		return;
	}

	if (OF_install(OFW_FDT, 0) == FALSE)
		panic("Cannot install FDT");

	if (OF_init((void *)dtbp) != 0)
		panic("OF_init failed with the found device tree");
}
#endif

static void
cache_setup(void)
{
	int dcache_line_shift, icache_line_shift;
	uint32_t ctr_el0;

	ctr_el0 = READ_SPECIALREG(ctr_el0);

	/* Read the log2 words in each D cache line */
	dcache_line_shift = CTR_DLINE_SIZE(ctr_el0);
	/* Get the D cache line size */
	dcache_line_size = sizeof(int) << dcache_line_shift;

	/* And the same for the I cache */
	icache_line_shift = CTR_ILINE_SIZE(ctr_el0);
	icache_line_size = sizeof(int) << icache_line_shift;

	idcache_line_size = MIN(dcache_line_size, icache_line_size);
}

void
initarm(struct arm64_bootparams *abp)
{
	struct efi_map_header *efihdr;
	struct pcpu *pcpup;
	vm_offset_t lastaddr;
	caddr_t kmdp;
	vm_paddr_t mem_len;
	int i;

	/* Set the module data location */
	preload_metadata = (caddr_t)(uintptr_t)(abp->modulep);

	/* Find the kernel address */
	kmdp = preload_search_by_type("elf kernel");
	if (kmdp == NULL)
		kmdp = preload_search_by_type("elf64 kernel");

	boothowto = MD_FETCH(kmdp, MODINFOMD_HOWTO, int);
	kern_envp = MD_FETCH(kmdp, MODINFOMD_ENVP, char *);

#ifdef FDT
	try_load_dtb(kmdp);
#endif

	/* Find the address to start allocating from */
	lastaddr = MD_FETCH(kmdp, MODINFOMD_KERNEND, vm_offset_t);

	/* Load the physical memory ranges */
	physmap_idx = 0;
	efihdr = (struct efi_map_header *)preload_search_info(kmdp,
	    MODINFO_METADATA | MODINFOMD_EFI_MAP);
	add_efi_map_entries(efihdr, physmap, &physmap_idx);

	/* Print the memory map */
	mem_len = 0;
	for (i = 0; i < physmap_idx; i += 2) {
		dump_avail[i] = physmap[i];
		dump_avail[i + 1] = physmap[i + 1];
		mem_len += physmap[i + 1] - physmap[i];
	}
	dump_avail[i] = 0;
	dump_avail[i + 1] = 0;

	/* Set the pcpu data, this is needed by pmap_bootstrap */
	pcpup = &__pcpu[0];
	pcpu_init(pcpup, 0, sizeof(struct pcpu));

	/*
	 * Set the pcpu pointer with a backup in tpidr_el1 to be
	 * loaded when entering the kernel from userland.
	 */
	__asm __volatile(
	    "mov x18, %0 \n"
	    "msr tpidr_el1, %0" :: "r"(pcpup));

	PCPU_SET(curthread, &thread0);

	/* Do basic tuning, hz etc */
	init_param1();

	cache_setup();

	/* Bootstrap enough of pmap  to enter the kernel proper */
	pmap_bootstrap(abp->kern_l1pt, KERNBASE - abp->kern_delta,
	    lastaddr - KERNBASE);

	arm_devmap_bootstrap(0, NULL);

	cninit();

	init_proc0(abp->kern_stack);
	msgbufinit(msgbufp, msgbufsize);
	mutex_init();
	init_param2(physmem);

	dbg_monitor_init();
	kdb_init();

	early_boot = 0;
}

#ifdef DDB
#include <ddb/ddb.h>

DB_SHOW_COMMAND(specialregs, db_show_spregs)
{
#define	PRINT_REG(reg)	\
    db_printf(__STRING(reg) " = %#016lx\n", READ_SPECIALREG(reg))

	PRINT_REG(actlr_el1);
	PRINT_REG(afsr0_el1);
	PRINT_REG(afsr1_el1);
	PRINT_REG(aidr_el1);
	PRINT_REG(amair_el1);
	PRINT_REG(ccsidr_el1);
	PRINT_REG(clidr_el1);
	PRINT_REG(contextidr_el1);
	PRINT_REG(cpacr_el1);
	PRINT_REG(csselr_el1);
	PRINT_REG(ctr_el0);
	PRINT_REG(currentel);
	PRINT_REG(daif);
	PRINT_REG(dczid_el0);
	PRINT_REG(elr_el1);
	PRINT_REG(esr_el1);
	PRINT_REG(far_el1);
	PRINT_REG(fpcr);
	PRINT_REG(fpsr);
	PRINT_REG(id_aa64afr0_el1);
	PRINT_REG(id_aa64afr1_el1);
	PRINT_REG(id_aa64dfr0_el1);
	PRINT_REG(id_aa64dfr1_el1);
	PRINT_REG(id_aa64isar0_el1);
	PRINT_REG(id_aa64isar1_el1);
	PRINT_REG(id_aa64pfr0_el1);
	PRINT_REG(id_aa64pfr1_el1);
	PRINT_REG(id_afr0_el1);
	PRINT_REG(id_dfr0_el1);
	PRINT_REG(id_isar0_el1);
	PRINT_REG(id_isar1_el1);
	PRINT_REG(id_isar2_el1);
	PRINT_REG(id_isar3_el1);
	PRINT_REG(id_isar4_el1);
	PRINT_REG(id_isar5_el1);
	PRINT_REG(id_mmfr0_el1);
	PRINT_REG(id_mmfr1_el1);
	PRINT_REG(id_mmfr2_el1);
	PRINT_REG(id_mmfr3_el1);
#if 0
	/* Missing from llvm */
	PRINT_REG(id_mmfr4_el1);
#endif
	PRINT_REG(id_pfr0_el1);
	PRINT_REG(id_pfr1_el1);
	PRINT_REG(isr_el1);
	PRINT_REG(mair_el1);
	PRINT_REG(midr_el1);
	PRINT_REG(mpidr_el1);
	PRINT_REG(mvfr0_el1);
	PRINT_REG(mvfr1_el1);
	PRINT_REG(mvfr2_el1);
	PRINT_REG(revidr_el1);
	PRINT_REG(sctlr_el1);
	PRINT_REG(sp_el0);
	PRINT_REG(spsel);
	PRINT_REG(spsr_el1);
	PRINT_REG(tcr_el1);
	PRINT_REG(tpidr_el0);
	PRINT_REG(tpidr_el1);
	PRINT_REG(tpidrro_el0);
	PRINT_REG(ttbr0_el1);
	PRINT_REG(ttbr1_el1);
	PRINT_REG(vbar_el1);
#undef PRINT_REG
}

DB_SHOW_COMMAND(vtop, db_show_vtop)
{
	uint64_t phys;

	if (have_addr) {
		phys = arm64_address_translate_s1e1r(addr);
		db_printf("Physical address reg: 0x%016lx\n", phys);
	} else
		db_printf("show vtop <virt_addr>\n");
}
#endif
