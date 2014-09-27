/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * Copyright (C) 2001 Benno Rice
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *	$NetBSD: machdep.c,v 1.74.2.1 2000/11/01 16:13:48 tv Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_ddb.h"
#include "opt_kstack_pages.h"
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/cpu.h>
#include <sys/eventhandler.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/ptrace.h>
#include <sys/reboot.h>
#include <sys/rwlock.h>
#include <sys/signalvar.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/ucontext.h>
#include <sys/uio.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <net/netisr.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>

#include <machine/altivec.h>
#ifndef __powerpc64__
#include <machine/bat.h>
#endif
#include <machine/cpu.h>
#include <machine/elf.h>
#include <machine/fpu.h>
#include <machine/hid.h>
#include <machine/kdb.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/mmuvar.h>
#include <machine/pcb.h>
#include <machine/reg.h>
#include <machine/sigframe.h>
#include <machine/spr.h>
#include <machine/trap.h>
#include <machine/vmparam.h>
#include <machine/ofw_machdep.h>

#include <ddb/ddb.h>

#include <dev/ofw/openfirm.h>

int cold = 1;
#ifdef __powerpc64__
extern int n_slbs;
int cacheline_size = 128;
#else
int cacheline_size = 32;
#endif
int hw_direct_map = 1;

extern void *ap_pcpu;

struct pcpu __pcpu[MAXCPU];

static struct trapframe frame0;

char		machine[] = "powerpc";
SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD, machine, 0, "");

static void	cpu_startup(void *);
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL);

SYSCTL_INT(_machdep, CPU_CACHELINE, cacheline_size,
	   CTLFLAG_RD, &cacheline_size, 0, "");

uintptr_t	powerpc_init(vm_offset_t, vm_offset_t, vm_offset_t, void *);

long		Maxmem = 0;
long		realmem = 0;

#ifndef __powerpc64__
struct bat	battable[16];
#endif

struct kva_md_info kmi;

static void
cpu_startup(void *dummy)
{

	/*
	 * Initialise the decrementer-based clock.
	 */
	decr_init();

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	cpu_setup(PCPU_GET(cpuid));

#ifdef PERFMON
	perfmon_init();
#endif
	printf("real memory  = %ld (%ld MB)\n", ptoa(physmem),
	    ptoa(physmem) / 1048576);
	realmem = physmem;

	if (bootverbose)
		printf("available KVA = %zd (%zd MB)\n",
		    virtual_end - virtual_avail,
		    (virtual_end - virtual_avail) / 1048576);

	/*
	 * Display any holes after the first chunk of extended memory.
	 */
	if (bootverbose) {
		int indx;

		printf("Physical memory chunk(s):\n");
		for (indx = 0; phys_avail[indx + 1] != 0; indx += 2) {
			vm_offset_t size1 =
			    phys_avail[indx + 1] - phys_avail[indx];

			#ifdef __powerpc64__
			printf("0x%016lx - 0x%016lx, %ld bytes (%ld pages)\n",
			#else
			printf("0x%08x - 0x%08x, %d bytes (%ld pages)\n",
			#endif
			    phys_avail[indx], phys_avail[indx + 1] - 1, size1,
			    size1 / PAGE_SIZE);
		}
	}

	vm_ksubmap_init(&kmi);

	printf("avail memory = %ld (%ld MB)\n", ptoa(vm_cnt.v_free_count),
	    ptoa(vm_cnt.v_free_count) / 1048576);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
	vm_pager_bufferinit();
}

extern char	kernel_text[], _end[];

#ifndef __powerpc64__
/* Bits for running on 64-bit systems in 32-bit mode. */
extern void	*testppc64, *testppc64size;
extern void	*restorebridge, *restorebridgesize;
extern void	*rfid_patch, *rfi_patch1, *rfi_patch2;
extern void	*trapcode64;
#endif

extern void	*rstcode, *rstsize;
extern void	*trapcode, *trapsize;
extern void	*slbtrap, *slbtrapsize;
extern void	*alitrap, *alisize;
extern void	*dsitrap, *dsisize;
extern void	*decrint, *decrsize;
extern void     *extint, *extsize;
extern void	*dblow, *dbsize;
extern void	*imisstrap, *imisssize;
extern void	*dlmisstrap, *dlmisssize;
extern void	*dsmisstrap, *dsmisssize;
char 		save_trap_init[0x2f00];		/* EXC_LAST */

uintptr_t
powerpc_init(vm_offset_t startkernel, vm_offset_t endkernel,
    vm_offset_t basekernel, void *mdp)
{
	struct		pcpu *pc;
	void		*generictrap;
	size_t		trap_offset;
	void		*kmdp;
        char		*env;
	register_t	msr, scratch;
#ifdef WII
	register_t 	vers;
#endif
	uint8_t		*cache_check;
	int		cacheline_warn;
	#ifndef __powerpc64__
	int		ppc64;
	#endif
#ifdef DDB
	vm_offset_t ksym_start;
	vm_offset_t ksym_end;
#endif

	kmdp = NULL;
	trap_offset = 0;
	cacheline_warn = 0;

	/* Save trap vectors. */
	ofw_save_trap_vec(save_trap_init);

#ifdef WII
	/*
	 * The Wii loader doesn't pass us any environment so, mdp
	 * points to garbage at this point. The Wii CPU is a 750CL.
	 */
	vers = mfpvr();
	if ((vers & 0xfffff0e0) == (MPC750 << 16 | MPC750CL)) 
		mdp = NULL;
#endif

	/*
	 * Parse metadata if present and fetch parameters.  Must be done
	 * before console is inited so cninit gets the right value of
	 * boothowto.
	 */
	if (mdp != NULL) {
		preload_metadata = mdp;
		kmdp = preload_search_by_type("elf kernel");
		if (kmdp != NULL) {
			boothowto = MD_FETCH(kmdp, MODINFOMD_HOWTO, int);
			kern_envp = MD_FETCH(kmdp, MODINFOMD_ENVP, char *);
			endkernel = ulmax(endkernel, MD_FETCH(kmdp,
			    MODINFOMD_KERNEND, vm_offset_t));
#ifdef DDB
			ksym_start = MD_FETCH(kmdp, MODINFOMD_SSYM, uintptr_t);
			ksym_end = MD_FETCH(kmdp, MODINFOMD_ESYM, uintptr_t);
			db_fetch_ksymtab(ksym_start, ksym_end);
#endif
		}
	}

	/*
	 * Init params/tunables that can be overridden by the loader
	 */
	init_param1();

	/*
	 * Start initializing proc0 and thread0.
	 */
	proc_linkup0(&proc0, &thread0);
	thread0.td_frame = &frame0;

	/*
	 * Set up per-cpu data.
	 */
	pc = __pcpu;
	pcpu_init(pc, 0, sizeof(struct pcpu));
	pc->pc_curthread = &thread0;
#ifdef __powerpc64__
	__asm __volatile("mr 13,%0" :: "r"(pc->pc_curthread));
#else
	__asm __volatile("mr 2,%0" :: "r"(pc->pc_curthread));
#endif
	pc->pc_cpuid = 0;

	__asm __volatile("mtsprg 0, %0" :: "r"(pc));

	/*
	 * Init mutexes, which we use heavily in PMAP
	 */

	mutex_init();

	/*
	 * Install the OF client interface
	 */

	OF_bootstrap();

	/*
	 * Initialize the console before printing anything.
	 */
	cninit();

	/*
	 * Complain if there is no metadata.
	 */
	if (mdp == NULL || kmdp == NULL) {
		printf("powerpc_init: no loader metadata.\n");
	}

	/*
	 * Init KDB
	 */

	kdb_init();

	/* Various very early CPU fix ups */
	switch (mfpvr() >> 16) {
		/*
		 * PowerPC 970 CPUs have a misfeature requested by Apple that
		 * makes them pretend they have a 32-byte cacheline. Turn this
		 * off before we measure the cacheline size.
		 */
		case IBM970:
		case IBM970FX:
		case IBM970MP:
		case IBM970GX:
			scratch = mfspr(SPR_HID5);
			scratch &= ~HID5_970_DCBZ_SIZE_HI;
			mtspr(SPR_HID5, scratch);
			break;
	#ifdef __powerpc64__
		case IBMPOWER7:
			/* XXX: get from ibm,slb-size in device tree */
			n_slbs = 32;
			break;
	#endif
	}

	/*
	 * Initialize the interrupt tables and figure out our cache line
	 * size and whether or not we need the 64-bit bridge code.
	 */

	/*
	 * Disable translation in case the vector area hasn't been
	 * mapped (G5). Note that no OFW calls can be made until
	 * translation is re-enabled.
	 */

	msr = mfmsr();
	mtmsr((msr & ~(PSL_IR | PSL_DR)) | PSL_RI);

	/*
	 * Measure the cacheline size using dcbz
	 *
	 * Use EXC_PGM as a playground. We are about to overwrite it
	 * anyway, we know it exists, and we know it is cache-aligned.
	 */

	cache_check = (void *)EXC_PGM;

	for (cacheline_size = 0; cacheline_size < 0x100; cacheline_size++)
		cache_check[cacheline_size] = 0xff;

	__asm __volatile("dcbz 0,%0":: "r" (cache_check) : "memory");

	/* Find the first byte dcbz did not zero to get the cache line size */
	for (cacheline_size = 0; cacheline_size < 0x100 &&
	    cache_check[cacheline_size] == 0; cacheline_size++);

	/* Work around psim bug */
	if (cacheline_size == 0) {
		cacheline_warn = 1;
		cacheline_size = 32;
	}

	/* Make sure the kernel icache is valid before we go too much further */
	__syncicache((caddr_t)startkernel, endkernel - startkernel);

	#ifndef __powerpc64__
	/*
	 * Figure out whether we need to use the 64 bit PMAP. This works by
	 * executing an instruction that is only legal on 64-bit PPC (mtmsrd),
	 * and setting ppc64 = 0 if that causes a trap.
	 */

	ppc64 = 1;

	bcopy(&testppc64, (void *)EXC_PGM,  (size_t)&testppc64size);
	__syncicache((void *)EXC_PGM, (size_t)&testppc64size);

	__asm __volatile("\
		mfmsr %0;	\
		mtsprg2 %1;	\
				\
		mtmsrd %0;	\
		mfsprg2 %1;"
	    : "=r"(scratch), "=r"(ppc64));

	if (ppc64)
		cpu_features |= PPC_FEATURE_64;

	/*
	 * Now copy restorebridge into all the handlers, if necessary,
	 * and set up the trap tables.
	 */

	if (cpu_features & PPC_FEATURE_64) {
		/* Patch the two instances of rfi -> rfid */
		bcopy(&rfid_patch,&rfi_patch1,4);
	#ifdef KDB
		/* rfi_patch2 is at the end of dbleave */
		bcopy(&rfid_patch,&rfi_patch2,4);
	#endif

		/*
		 * Copy a code snippet to restore 32-bit bridge mode
		 * to the top of every non-generic trap handler
		 */

		trap_offset += (size_t)&restorebridgesize;
		bcopy(&restorebridge, (void *)EXC_RST, trap_offset); 
		bcopy(&restorebridge, (void *)EXC_DSI, trap_offset); 
		bcopy(&restorebridge, (void *)EXC_ALI, trap_offset); 
		bcopy(&restorebridge, (void *)EXC_PGM, trap_offset); 
		bcopy(&restorebridge, (void *)EXC_MCHK, trap_offset); 
		bcopy(&restorebridge, (void *)EXC_TRC, trap_offset); 
		bcopy(&restorebridge, (void *)EXC_BPT, trap_offset); 

		/*
		 * Set the common trap entry point to the one that
		 * knows to restore 32-bit operation on execution.
		 */

		generictrap = &trapcode64;
	} else {
		generictrap = &trapcode;
	}

	#else /* powerpc64 */
	cpu_features |= PPC_FEATURE_64;
	generictrap = &trapcode;
	#endif

	bcopy(&rstcode, (void *)(EXC_RST + trap_offset),  (size_t)&rstsize);

#ifdef KDB
	bcopy(&dblow,	(void *)(EXC_MCHK + trap_offset), (size_t)&dbsize);
	bcopy(&dblow,   (void *)(EXC_PGM + trap_offset),  (size_t)&dbsize);
	bcopy(&dblow,   (void *)(EXC_TRC + trap_offset),  (size_t)&dbsize);
	bcopy(&dblow,   (void *)(EXC_BPT + trap_offset),  (size_t)&dbsize);
#else
	bcopy(generictrap, (void *)EXC_MCHK, (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_PGM,  (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_TRC,  (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_BPT,  (size_t)&trapsize);
#endif
	bcopy(&alitrap,  (void *)(EXC_ALI + trap_offset),  (size_t)&alisize);
	bcopy(&dsitrap,  (void *)(EXC_DSI + trap_offset),  (size_t)&dsisize);
	bcopy(generictrap, (void *)EXC_ISI,  (size_t)&trapsize);
	#ifdef __powerpc64__
	bcopy(&slbtrap, (void *)EXC_DSE,  (size_t)&slbtrapsize);
	bcopy(&slbtrap, (void *)EXC_ISE,  (size_t)&slbtrapsize);
	#endif
	bcopy(generictrap, (void *)EXC_EXI,  (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_FPU,  (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_DECR, (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_SC,   (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_FPA,  (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_VEC,  (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_PERF,  (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_VECAST_G4, (size_t)&trapsize);
	bcopy(generictrap, (void *)EXC_VECAST_G5, (size_t)&trapsize);
	#ifndef __powerpc64__
	/* G2-specific TLB miss helper handlers */
	bcopy(&imisstrap, (void *)EXC_IMISS,  (size_t)&imisssize);
	bcopy(&dlmisstrap, (void *)EXC_DLMISS,  (size_t)&dlmisssize);
	bcopy(&dsmisstrap, (void *)EXC_DSMISS,  (size_t)&dsmisssize);
	#endif
	__syncicache(EXC_RSVD, EXC_LAST - EXC_RSVD);

	/*
	 * Restore MSR
	 */
	mtmsr(msr);
	
	/* Warn if cachline size was not determined */
	if (cacheline_warn == 1) {
		printf("WARNING: cacheline size undetermined, setting to 32\n");
	}

	/*
	 * Choose a platform module so we can get the physical memory map.
	 */
	
	platform_probe_and_attach();

	/*
	 * Initialise virtual memory. Use BUS_PROBE_GENERIC priority
	 * in case the platform module had a better idea of what we
	 * should do.
	 */
	if (cpu_features & PPC_FEATURE_64)
		pmap_mmu_install(MMU_TYPE_G5, BUS_PROBE_GENERIC);
	else
		pmap_mmu_install(MMU_TYPE_OEA, BUS_PROBE_GENERIC);

	pmap_bootstrap(startkernel, endkernel);
	mtmsr(PSL_KERNSET & ~PSL_EE);

	/*
	 * Initialize params/tunables that are derived from memsize
	 */
	init_param2(physmem);

	/*
	 * Grab booted kernel's name
	 */
        env = getenv("kernelname");
        if (env != NULL) {
		strlcpy(kernelname, env, sizeof(kernelname));
		freeenv(env);
	}

	/*
	 * Finish setting up thread0.
	 */
	thread0.td_pcb = (struct pcb *)
	    ((thread0.td_kstack + thread0.td_kstack_pages * PAGE_SIZE -
	    sizeof(struct pcb)) & ~15UL);
	bzero((void *)thread0.td_pcb, sizeof(struct pcb));
	pc->pc_curpcb = thread0.td_pcb;

	/* Initialise the message buffer. */
	msgbufinit(msgbufp, msgbufsize);

#ifdef KDB
	if (boothowto & RB_KDB)
		kdb_enter(KDB_WHY_BOOTFLAGS,
		    "Boot flags requested debugger");
#endif

	return (((uintptr_t)thread0.td_pcb -
	    (sizeof(struct callframe) - 3*sizeof(register_t))) & ~15UL);
}

void
bzero(void *buf, size_t len)
{
	caddr_t	p;

	p = buf;

	while (((vm_offset_t) p & (sizeof(u_long) - 1)) && len) {
		*p++ = 0;
		len--;
	}

	while (len >= sizeof(u_long) * 8) {
		*(u_long*) p = 0;
		*((u_long*) p + 1) = 0;
		*((u_long*) p + 2) = 0;
		*((u_long*) p + 3) = 0;
		len -= sizeof(u_long) * 8;
		*((u_long*) p + 4) = 0;
		*((u_long*) p + 5) = 0;
		*((u_long*) p + 6) = 0;
		*((u_long*) p + 7) = 0;
		p += sizeof(u_long) * 8;
	}

	while (len >= sizeof(u_long)) {
		*(u_long*) p = 0;
		len -= sizeof(u_long);
		p += sizeof(u_long);
	}

	while (len) {
		*p++ = 0;
		len--;
	}
}

void
cpu_boot(int howto)
{
}

/*
 * Flush the D-cache for non-DMA I/O so that the I-cache can
 * be made coherent later.
 */
void
cpu_flush_dcache(void *ptr, size_t len)
{
	/* TBD */
}

/*
 * Shutdown the CPU as much as possible.
 */
void
cpu_halt(void)
{

	OF_exit();
}

int
ptrace_set_pc(struct thread *td, unsigned long addr)
{
	struct trapframe *tf;

	tf = td->td_frame;
	tf->srr0 = (register_t)addr;

	return (0);
}

int
ptrace_single_step(struct thread *td)
{
	struct trapframe *tf;
	
	tf = td->td_frame;
	tf->srr1 |= PSL_SE;

	return (0);
}

int
ptrace_clear_single_step(struct thread *td)
{
	struct trapframe *tf;

	tf = td->td_frame;
	tf->srr1 &= ~PSL_SE;

	return (0);
}

void
kdb_cpu_clear_singlestep(void)
{

	kdb_frame->srr1 &= ~PSL_SE;
}

void
kdb_cpu_set_singlestep(void)
{

	kdb_frame->srr1 |= PSL_SE;
}

/*
 * Initialise a struct pcpu.
 */
void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t sz)
{
#ifdef __powerpc64__
/* Copy the SLB contents from the current CPU */
memcpy(pcpu->pc_slb, PCPU_GET(slb), sizeof(pcpu->pc_slb));
#endif
}

void
spinlock_enter(void)
{
	struct thread *td;
	register_t msr;

	td = curthread;
	if (td->td_md.md_spinlock_count == 0) {
		msr = intr_disable();
		td->td_md.md_spinlock_count = 1;
		td->td_md.md_saved_msr = msr;
	} else
		td->td_md.md_spinlock_count++;
	critical_enter();
}

void
spinlock_exit(void)
{
	struct thread *td;
	register_t msr;

	td = curthread;
	critical_exit();
	msr = td->td_md.md_saved_msr;
	td->td_md.md_spinlock_count--;
	if (td->td_md.md_spinlock_count == 0)
		intr_restore(msr);
}

int db_trap_glue(struct trapframe *);		/* Called from trap_subr.S */

int
db_trap_glue(struct trapframe *frame)
{
	if (!(frame->srr1 & PSL_PR)
	    && (frame->exc == EXC_TRC || frame->exc == EXC_RUNMODETRC
		|| (frame->exc == EXC_PGM
		    && (frame->srr1 & 0x20000))
		|| frame->exc == EXC_BPT
		|| frame->exc == EXC_DSI)) {
		int type = frame->exc;
		if (type == EXC_PGM && (frame->srr1 & 0x20000)) {
			type = T_BREAKPOINT;
		}
		return (kdb_trap(type, 0, frame));
	}

	return (0);
}

#ifndef __powerpc64__

uint64_t
va_to_vsid(pmap_t pm, vm_offset_t va)
{
	return ((pm->pm_sr[(uintptr_t)va >> ADDR_SR_SHFT]) & SR_VSID_MASK);
}

#endif

vm_offset_t
pmap_early_io_map(vm_paddr_t pa, vm_size_t size)
{

	return (pa);
}

/* From p3-53 of the MPC7450 RISC Microprocessor Family Reference Manual */
void
flush_disable_caches(void)
{
	register_t msr;
	register_t msscr0;
	register_t cache_reg;
	volatile uint32_t *memp;
	uint32_t temp;
	int i;
	int x;

	msr = mfmsr();
	powerpc_sync();
	mtmsr(msr & ~(PSL_EE | PSL_DR));
	msscr0 = mfspr(SPR_MSSCR0);
	msscr0 &= ~MSSCR0_L2PFE;
	mtspr(SPR_MSSCR0, msscr0);
	powerpc_sync();
	isync();
	__asm__ __volatile__("dssall; sync");
	powerpc_sync();
	isync();
	__asm__ __volatile__("dcbf 0,%0" :: "r"(0));
	__asm__ __volatile__("dcbf 0,%0" :: "r"(0));
	__asm__ __volatile__("dcbf 0,%0" :: "r"(0));

	/* Lock the L1 Data cache. */
	mtspr(SPR_LDSTCR, mfspr(SPR_LDSTCR) | 0xFF);
	powerpc_sync();
	isync();

	mtspr(SPR_LDSTCR, 0);

	/*
	 * Perform this in two stages: Flush the cache starting in RAM, then do it
	 * from ROM.
	 */
	memp = (volatile uint32_t *)0x00000000;
	for (i = 0; i < 128 * 1024; i++) {
		temp = *memp;
		__asm__ __volatile__("dcbf 0,%0" :: "r"(memp));
		memp += 32/sizeof(*memp);
	}

	memp = (volatile uint32_t *)0xfff00000;
	x = 0xfe;

	for (; x != 0xff;) {
		mtspr(SPR_LDSTCR, x);
		for (i = 0; i < 128; i++) {
			temp = *memp;
			__asm__ __volatile__("dcbf 0,%0" :: "r"(memp));
			memp += 32/sizeof(*memp);
		}
		x = ((x << 1) | 1) & 0xff;
	}
	mtspr(SPR_LDSTCR, 0);

	cache_reg = mfspr(SPR_L2CR);
	if (cache_reg & L2CR_L2E) {
		cache_reg &= ~(L2CR_L2IO_7450 | L2CR_L2DO_7450);
		mtspr(SPR_L2CR, cache_reg);
		powerpc_sync();
		mtspr(SPR_L2CR, cache_reg | L2CR_L2HWF);
		while (mfspr(SPR_L2CR) & L2CR_L2HWF)
			; /* Busy wait for cache to flush */
		powerpc_sync();
		cache_reg &= ~L2CR_L2E;
		mtspr(SPR_L2CR, cache_reg);
		powerpc_sync();
		mtspr(SPR_L2CR, cache_reg | L2CR_L2I);
		powerpc_sync();
		while (mfspr(SPR_L2CR) & L2CR_L2I)
			; /* Busy wait for L2 cache invalidate */
		powerpc_sync();
	}

	cache_reg = mfspr(SPR_L3CR);
	if (cache_reg & L3CR_L3E) {
		cache_reg &= ~(L3CR_L3IO | L3CR_L3DO);
		mtspr(SPR_L3CR, cache_reg);
		powerpc_sync();
		mtspr(SPR_L3CR, cache_reg | L3CR_L3HWF);
		while (mfspr(SPR_L3CR) & L3CR_L3HWF)
			; /* Busy wait for cache to flush */
		powerpc_sync();
		cache_reg &= ~L3CR_L3E;
		mtspr(SPR_L3CR, cache_reg);
		powerpc_sync();
		mtspr(SPR_L3CR, cache_reg | L3CR_L3I);
		powerpc_sync();
		while (mfspr(SPR_L3CR) & L3CR_L3I)
			; /* Busy wait for L3 cache invalidate */
		powerpc_sync();
	}

	mtspr(SPR_HID0, mfspr(SPR_HID0) & ~HID0_DCE);
	powerpc_sync();
	isync();

	mtmsr(msr);
}

void
cpu_sleep()
{
	static u_quad_t timebase = 0;
	static register_t sprgs[4];
	static register_t srrs[2];

	jmp_buf resetjb;
	struct thread *fputd;
	struct thread *vectd;
	register_t hid0;
	register_t msr;
	register_t saved_msr;

	ap_pcpu = pcpup;

	PCPU_SET(restore, &resetjb);

	saved_msr = mfmsr();
	fputd = PCPU_GET(fputhread);
	vectd = PCPU_GET(vecthread);
	if (fputd != NULL)
		save_fpu(fputd);
	if (vectd != NULL)
		save_vec(vectd);
	if (setjmp(resetjb) == 0) {
		sprgs[0] = mfspr(SPR_SPRG0);
		sprgs[1] = mfspr(SPR_SPRG1);
		sprgs[2] = mfspr(SPR_SPRG2);
		sprgs[3] = mfspr(SPR_SPRG3);
		srrs[0] = mfspr(SPR_SRR0);
		srrs[1] = mfspr(SPR_SRR1);
		timebase = mftb();
		powerpc_sync();
		flush_disable_caches();
		hid0 = mfspr(SPR_HID0);
		hid0 = (hid0 & ~(HID0_DOZE | HID0_NAP)) | HID0_SLEEP;
		powerpc_sync();
		isync();
		msr = mfmsr() | PSL_POW;
		mtspr(SPR_HID0, hid0);
		powerpc_sync();

		while (1)
			mtmsr(msr);
	}
	mttb(timebase);
	PCPU_SET(curthread, curthread);
	PCPU_SET(curpcb, curthread->td_pcb);
	pmap_activate(curthread);
	powerpc_sync();
	mtspr(SPR_SPRG0, sprgs[0]);
	mtspr(SPR_SPRG1, sprgs[1]);
	mtspr(SPR_SPRG2, sprgs[2]);
	mtspr(SPR_SPRG3, sprgs[3]);
	mtspr(SPR_SRR0, srrs[0]);
	mtspr(SPR_SRR1, srrs[1]);
	mtmsr(saved_msr);
	if (fputd == curthread)
		enable_fpu(curthread);
	if (vectd == curthread)
		enable_vec(curthread);
	powerpc_sync();
}
