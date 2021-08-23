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
#include <vm/vm_phys.h>
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
#include <machine/ifunc.h>
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
#include <dev/ofw/ofw_subr.h>

int cold = 1;
#ifdef __powerpc64__
int cacheline_size = 128;
#else
int cacheline_size = 32;
#endif
#ifdef __powerpc64__
int hw_direct_map = -1;
#else
int hw_direct_map = 1;
#endif

#ifdef BOOKE
extern vm_paddr_t kernload;
#endif

extern void *ap_pcpu;

struct pcpu __pcpu[MAXCPU] __aligned(PAGE_SIZE);
static char init_kenv[2048];

static struct trapframe frame0;

char		machine[] = "powerpc";
SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD, machine, 0, "");

static void	cpu_startup(void *);
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL);

SYSCTL_INT(_machdep, CPU_CACHELINE, cacheline_size,
	   CTLFLAG_RD, &cacheline_size, 0, "");

uintptr_t	powerpc_init(vm_offset_t, vm_offset_t, vm_offset_t, void *,
		    uint32_t);

static void	fake_preload_metadata(void);

long		Maxmem = 0;
long		realmem = 0;

/* Default MSR values set in the AIM/Book-E early startup code */
register_t	psl_kernset;
register_t	psl_userset;
register_t	psl_userstatic;
#ifdef __powerpc64__
register_t	psl_userset32;
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
	printf("real memory  = %ju (%ju MB)\n", ptoa((uintmax_t)physmem),
	    ptoa((uintmax_t)physmem) / 1048576);
	realmem = physmem;

	if (bootverbose)
		printf("available KVA = %zu (%zu MB)\n",
		    virtual_end - virtual_avail,
		    (virtual_end - virtual_avail) / 1048576);

	/*
	 * Display any holes after the first chunk of extended memory.
	 */
	if (bootverbose) {
		int indx;

		printf("Physical memory chunk(s):\n");
		for (indx = 0; phys_avail[indx + 1] != 0; indx += 2) {
			vm_paddr_t size1 =
			    phys_avail[indx + 1] - phys_avail[indx];

			#ifdef __powerpc64__
			printf("0x%016jx - 0x%016jx, %ju bytes (%ju pages)\n",
			#else
			printf("0x%09jx - 0x%09jx, %ju bytes (%ju pages)\n",
			#endif
			    (uintmax_t)phys_avail[indx],
			    (uintmax_t)phys_avail[indx + 1] - 1,
			    (uintmax_t)size1, (uintmax_t)size1 / PAGE_SIZE);
		}
	}

	vm_ksubmap_init(&kmi);

	printf("avail memory = %ju (%ju MB)\n",
	    ptoa((uintmax_t)vm_free_count()),
	    ptoa((uintmax_t)vm_free_count()) / 1048576);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
	vm_pager_bufferinit();
}

extern vm_offset_t	__startkernel, __endkernel;
extern unsigned char	__bss_start[];
extern unsigned char	__sbss_start[];
extern unsigned char	__sbss_end[];
extern unsigned char	_end[];

void aim_early_init(vm_offset_t fdt, vm_offset_t toc, vm_offset_t ofentry,
    void *mdp, uint32_t mdp_cookie);
void aim_cpu_init(vm_offset_t toc);
void booke_cpu_init(void);

#ifdef DDB
static void	load_external_symtab(void);
#endif

uintptr_t
powerpc_init(vm_offset_t fdt, vm_offset_t toc, vm_offset_t ofentry, void *mdp,
    uint32_t mdp_cookie)
{
	struct		pcpu *pc;
	struct cpuref	bsp;
	vm_offset_t	startkernel, endkernel;
	char		*env;
	void		*kmdp = NULL;
        bool		ofw_bootargs = false;
#ifdef DDB
	bool		symbols_provided = false;
	vm_offset_t ksym_start;
	vm_offset_t ksym_end;
	vm_offset_t ksym_sz;
#endif

	/* First guess at start/end kernel positions */
	startkernel = __startkernel;
	endkernel = __endkernel;

	/*
	 * If the metadata pointer cookie is not set to the magic value,
	 * the number in mdp should be treated as nonsense.
	 */
	if (mdp_cookie != 0xfb5d104d)
		mdp = NULL;

#if !defined(BOOKE)
	/*
	 * On BOOKE the BSS is already cleared and some variables
	 * initialized.  Do not wipe them out.
	 */
	bzero(__sbss_start, __sbss_end - __sbss_start);
	bzero(__bss_start, _end - __bss_start);
#endif

	cpu_feature_setup();

#ifdef AIM
	aim_early_init(fdt, toc, ofentry, mdp, mdp_cookie);
#endif

	/*
	 * At this point, we are executing in our correct memory space.
	 * Book-E started there, and AIM has done an rfi and restarted
	 * execution from _start.
	 *
	 * We may still be in real mode, however. If we are running out of
	 * the direct map on 64 bit, this is possible to do.
	 */

	/*
	 * Parse metadata if present and fetch parameters.  Must be done
	 * before console is inited so cninit gets the right value of
	 * boothowto.
	 */
	if (mdp != NULL) {
		/*
		 * Starting up from loader.
		 *
		 * Full metadata has been provided, but we need to figure
		 * out the correct address to relocate it to.
		 */
		char *envp = NULL;
		uintptr_t md_offset = 0;
		vm_paddr_t kernelstartphys, kernelendphys;

#ifdef AIM
		if ((uintptr_t)&powerpc_init > DMAP_BASE_ADDRESS)
			md_offset = DMAP_BASE_ADDRESS;
#else /* BOOKE */
		md_offset = VM_MIN_KERNEL_ADDRESS - kernload;
#endif

		preload_metadata = mdp;
		if (md_offset > 0) {
			/* Translate phys offset into DMAP offset. */
			preload_metadata += md_offset;
			preload_bootstrap_relocate(md_offset);
		}
		kmdp = preload_search_by_type("elf kernel");
		if (kmdp != NULL) {
			boothowto = MD_FETCH(kmdp, MODINFOMD_HOWTO, int);
			envp = MD_FETCH(kmdp, MODINFOMD_ENVP, char *);
			if (envp != NULL)
				envp += md_offset;
			init_static_kenv(envp, 0);
			if (fdt == 0) {
				fdt = MD_FETCH(kmdp, MODINFOMD_DTBP, uintptr_t);
				if (fdt != 0)
					fdt += md_offset;
			}
			kernelstartphys = MD_FETCH(kmdp, MODINFO_ADDR,
			    vm_offset_t);
			/* kernelstartphys is already relocated. */
			kernelendphys = MD_FETCH(kmdp, MODINFOMD_KERNEND,
			    vm_offset_t);
			if (kernelendphys != 0)
				kernelendphys += md_offset;
			endkernel = ulmax(endkernel, kernelendphys);
#ifdef DDB
			ksym_start = MD_FETCH(kmdp, MODINFOMD_SSYM, uintptr_t);
			ksym_end = MD_FETCH(kmdp, MODINFOMD_ESYM, uintptr_t);
			ksym_sz = *(Elf_Size*)ksym_start;

			db_fetch_ksymtab(ksym_start, ksym_end, md_offset);
			/* Symbols provided by loader. */
			symbols_provided = true;
#endif
		}
	} else {
		/*
		 * Self-loading kernel, we have to fake up metadata.
		 *
		 * Since we are creating the metadata from the final
		 * memory space, we don't need to call
		 * preload_boostrap_relocate().
		 */
		fake_preload_metadata();
		kmdp = preload_search_by_type("elf kernel");
		init_static_kenv(init_kenv, sizeof(init_kenv));
		ofw_bootargs = true;
	}

	/* Store boot environment state */
	OF_initial_setup((void *)fdt, NULL, (int (*)(void *))ofentry);

	/*
	 * Init params/tunables that can be overridden by the loader
	 */
	init_param1();

	/*
	 * Start initializing proc0 and thread0.
	 */
	proc_linkup0(&proc0, &thread0);
	thread0.td_frame = &frame0;
#ifdef __powerpc64__
	__asm __volatile("mr 13,%0" :: "r"(&thread0));
#else
	__asm __volatile("mr 2,%0" :: "r"(&thread0));
#endif

	/*
	 * Init mutexes, which we use heavily in PMAP
	 */
	mutex_init();

	/*
	 * Install the OF client interface
	 */
	OF_bootstrap();

#ifdef DDB
	if (!symbols_provided && hw_direct_map)
		load_external_symtab();
#endif

	if (ofw_bootargs)
		ofw_parse_bootargs();

	/*
	 * Initialize the console before printing anything.
	 */
	cninit();

#ifdef AIM
	aim_cpu_init(toc);
#else /* BOOKE */
	booke_cpu_init();

	/* Make sure the kernel icache is valid before we go too much further */
	__syncicache((caddr_t)startkernel, endkernel - startkernel);
#endif

	/*
	 * Choose a platform module so we can get the physical memory map.
	 */

	platform_probe_and_attach();

	/*
	 * Set up per-cpu data for the BSP now that the platform can tell
	 * us which that is.
	 */
	if (platform_smp_get_bsp(&bsp) != 0)
		bsp.cr_cpuid = 0;
	pc = &__pcpu[bsp.cr_cpuid];
	__asm __volatile("mtsprg 0, %0" :: "r"(pc));
	pcpu_init(pc, bsp.cr_cpuid, sizeof(struct pcpu));
	pc->pc_curthread = &thread0;
	thread0.td_oncpu = bsp.cr_cpuid;
	pc->pc_cpuid = bsp.cr_cpuid;
	pc->pc_hwref = bsp.cr_hwref;

	/*
	 * Init KDB
	 */
	kdb_init();

	/*
	 * Bring up MMU
	 */
	pmap_mmu_init();
	link_elf_ireloc(kmdp);
	pmap_bootstrap(startkernel, endkernel);
	mtmsr(psl_kernset & ~PSL_EE);

	/*
	 * Initialize params/tunables that are derived from memsize
	 */
	init_param2(physmem);

	/*
	 * Grab booted kernel's name
	 */
        env = kern_getenv("kernelname");
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

#ifdef DDB
/*
 * On powernv and some booke systems, we might not have symbols loaded via
 * loader. However, if the user passed the kernel in as the initrd as well,
 * we can manually load it via reinterpreting the initrd copy of the kernel.
 *
 * In the BOOKE case, we don't actually have a DMAP yet, so we have to use
 * temporary maps to inspect the memory, but write DMAP addresses to the
 * configuration variables.
 */
static void
load_external_symtab(void) {
	phandle_t chosen;
	vm_paddr_t start, end;
	pcell_t cell[2];
	ssize_t size;
	u_char *kernelimg;		/* Temporary map */
	u_char *kernelimg_final;	/* Final location */

	int i;

	Elf_Ehdr *ehdr;
	Elf_Phdr *phdr;
	Elf_Shdr *shdr;

	vm_offset_t ksym_start, ksym_sz, kstr_start, kstr_sz,
	    ksym_start_final, kstr_start_final;

	if (!hw_direct_map)
		return;

	chosen = OF_finddevice("/chosen");
	if (chosen <= 0)
		return;

	if (!OF_hasprop(chosen, "linux,initrd-start") ||
	    !OF_hasprop(chosen, "linux,initrd-end"))
		return;

	size = OF_getencprop(chosen, "linux,initrd-start", cell, sizeof(cell));
	if (size == 4)
		start = cell[0];
	else if (size == 8)
		start = (uint64_t)cell[0] << 32 | cell[1];
	else
		return;

	size = OF_getencprop(chosen, "linux,initrd-end", cell, sizeof(cell));
	if (size == 4)
		end = cell[0];
	else if (size == 8)
		end = (uint64_t)cell[0] << 32 | cell[1];
	else
		return;

	if (!(end - start > 0))
		return;

	kernelimg_final = (u_char *) PHYS_TO_DMAP(start);
#ifdef	AIM
	kernelimg = kernelimg_final;
#else	/* BOOKE */
	kernelimg = (u_char *)pmap_early_io_map(start, PAGE_SIZE);
#endif
	ehdr = (Elf_Ehdr *)kernelimg;

	if (!IS_ELF(*ehdr)) {
#ifdef	BOOKE
		pmap_early_io_unmap(start, PAGE_SIZE);
#endif
		return;
	}

#ifdef	BOOKE
	pmap_early_io_unmap(start, PAGE_SIZE);
	kernelimg = (u_char *)pmap_early_io_map(start, (end - start));
#endif

	phdr = (Elf_Phdr *)(kernelimg + ehdr->e_phoff);
	shdr = (Elf_Shdr *)(kernelimg + ehdr->e_shoff);

	ksym_start = 0;
	ksym_sz = 0;
	ksym_start_final = 0;
	kstr_start = 0;
	kstr_sz = 0;
	kstr_start_final = 0;
	for (i = 0; i < ehdr->e_shnum; i++) {
		if (shdr[i].sh_type == SHT_SYMTAB) {
			ksym_start = (vm_offset_t)(kernelimg +
			    shdr[i].sh_offset);
			ksym_start_final = (vm_offset_t)
			    (kernelimg_final + shdr[i].sh_offset);
			ksym_sz = (vm_offset_t)(shdr[i].sh_size);
			kstr_start = (vm_offset_t)(kernelimg +
			    shdr[shdr[i].sh_link].sh_offset);
			kstr_start_final = (vm_offset_t)
			    (kernelimg_final +
			    shdr[shdr[i].sh_link].sh_offset);

			kstr_sz = (vm_offset_t)
			    (shdr[shdr[i].sh_link].sh_size);
		}
	}

	if (ksym_start != 0 && kstr_start != 0 && ksym_sz != 0 &&
	    kstr_sz != 0 && ksym_start < kstr_start) {
		/*
		 * We can't use db_fetch_ksymtab() here, because we need to
		 * feed in DMAP addresses that are not mapped yet on booke.
		 *
		 * Write the variables directly, where db_init() will pick
		 * them up later, after the DMAP is up.
		 */
		ksymtab = ksym_start_final;
		ksymtab_size = ksym_sz;
		kstrtab = kstr_start_final;
		ksymtab_relbase = (__startkernel - KERNBASE);
	}

#ifdef	BOOKE
	pmap_early_io_unmap(start, (end - start));
#endif

};
#endif

/*
 * When not being loaded from loader, we need to create our own metadata
 * so we can interact with the kernel linker.
 */
static void
fake_preload_metadata(void) {
	/* We depend on dword alignment here. */
	static uint32_t fake_preload[36] __aligned(8);
	int i = 0;

	fake_preload[i++] = MODINFO_NAME;
	fake_preload[i++] = strlen("kernel") + 1;
	strcpy((char*)&fake_preload[i], "kernel");
	/* ['k' 'e' 'r' 'n'] ['e' 'l' '\0' ..] */
	i += 2;

	fake_preload[i++] = MODINFO_TYPE;
	fake_preload[i++] = strlen("elf kernel") + 1;
	strcpy((char*)&fake_preload[i], "elf kernel");
	/* ['e' 'l' 'f' ' '] ['k' 'e' 'r' 'n'] ['e' 'l' '\0' ..] */
	i += 3;

#ifdef __powerpc64__
	/* Padding -- Fields start on u_long boundaries */
	fake_preload[i++] = 0;
#endif

	fake_preload[i++] = MODINFO_ADDR;
	fake_preload[i++] = sizeof(vm_offset_t);
	*(vm_offset_t *)&fake_preload[i] =
	    (vm_offset_t)(__startkernel);
	i += (sizeof(vm_offset_t) / 4);

	fake_preload[i++] = MODINFO_SIZE;
	fake_preload[i++] = sizeof(vm_offset_t);
	*(vm_offset_t *)&fake_preload[i] =
	    (vm_offset_t)(__endkernel) - (vm_offset_t)(__startkernel);
	i += (sizeof(vm_offset_t) / 4);

	/*
	 * MODINFOMD_SSYM and MODINFOMD_ESYM cannot be provided here,
	 * as the memory comes from outside the loaded ELF sections.
	 *
	 * If the symbols are being provided by other means (MFS), the
	 * tables will be loaded into the debugger directly.
	 */

	/* Null field at end to mark end of data. */
	fake_preload[i++] = 0;
	fake_preload[i] = 0;
	preload_metadata = (void*)fake_preload;
}

/*
 * Flush the D-cache for non-DMA I/O so that the I-cache can
 * be made coherent later.
 */
void
cpu_flush_dcache(void *ptr, size_t len)
{
	register_t addr, off;

	/*
	 * Align the address to a cacheline and adjust the length
	 * accordingly. Then round the length to a multiple of the
	 * cacheline for easy looping.
	 */
	addr = (uintptr_t)ptr;
	off = addr & (cacheline_size - 1);
	addr -= off;
	len = roundup2(len + off, cacheline_size);

	while (len > 0) {
		__asm __volatile ("dcbf 0,%0" :: "r"(addr));
		__asm __volatile ("sync");
		addr += cacheline_size;
		len -= cacheline_size;
	}
}

int
ptrace_set_pc(struct thread *td, unsigned long addr)
{
	struct trapframe *tf;

	tf = td->td_frame;
	tf->srr0 = (register_t)addr;

	return (0);
}

void
spinlock_enter(void)
{
	struct thread *td;
	register_t msr;

	td = curthread;
	if (td->td_md.md_spinlock_count == 0) {
		nop_prio_mhigh();
		msr = intr_disable();
		td->td_md.md_spinlock_count = 1;
		td->td_md.md_saved_msr = msr;
		critical_enter();
	} else
		td->td_md.md_spinlock_count++;
}

void
spinlock_exit(void)
{
	struct thread *td;
	register_t msr;

	td = curthread;
	msr = td->td_md.md_saved_msr;
	td->td_md.md_spinlock_count--;
	if (td->td_md.md_spinlock_count == 0) {
		critical_exit();
		intr_restore(msr);
		nop_prio_medium();
	}
}

/*
 * Simple ddb(4) command/hack to view any SPR on the running CPU.
 * Uses a trivial asm function to perform the mfspr, and rewrites the mfspr
 * instruction each time.
 * XXX: Since it uses code modification, it won't work if the kernel code pages
 * are marked RO.
 */
extern register_t get_spr(int);

#ifdef DDB
DB_SHOW_COMMAND(spr, db_show_spr)
{
	register_t spr;
	volatile uint32_t *p;
	int sprno, saved_sprno;

	if (!have_addr)
		return;

	saved_sprno = sprno = (intptr_t) addr;
	sprno = ((sprno & 0x3e0) >> 5) | ((sprno & 0x1f) << 5);
	p = (uint32_t *)(void *)&get_spr;
#ifdef __powerpc64__
#if defined(_CALL_ELF) && _CALL_ELF == 2
	/* Account for ELFv2 function prologue. */
	p += 2;
#else
	p = *(volatile uint32_t * volatile *)p;
#endif
#endif
	*p = (*p & ~0x001ff800) | (sprno << 11);
	__syncicache(__DEVOLATILE(uint32_t *, p), cacheline_size);
	spr = get_spr(sprno);

	db_printf("SPR %d(%x): %lx\n", saved_sprno, saved_sprno,
	    (unsigned long)spr);
}

DB_SHOW_COMMAND(frame, db_show_frame)
{
	struct trapframe *tf;
	long reg;
	int i;

	tf = have_addr ? (struct trapframe *)addr : curthread->td_frame;

	/*
	 * Everything casts through long to simplify the printing.
	 * 'long' is native register size anyway.
	 */
	db_printf("trap frame %p\n", tf);
	for (i = 0; i < nitems(tf->fixreg); i++) {
		reg = tf->fixreg[i];
		db_printf("  r%d:\t%#lx (%ld)\n", i, reg, reg);
	}
	reg = tf->lr;
	db_printf("  lr:\t%#lx\n", reg);
	reg = tf->cr;
	db_printf("  cr:\t%#lx\n", reg);
	reg = tf->xer;
	db_printf("  xer:\t%#lx\n", reg);
	reg = tf->ctr;
	db_printf("  ctr:\t%#lx (%ld)\n", reg, reg);
	reg = tf->srr0;
	db_printf("  srr0:\t%#lx\n", reg);
	reg = tf->srr1;
	db_printf("  srr1:\t%#lx\n", reg);
	reg = tf->exc;
	db_printf("  exc:\t%#lx\n", reg);
	reg = tf->dar;
	db_printf("  dar:\t%#lx\n", reg);
#ifdef AIM
	reg = tf->cpu.aim.dsisr;
	db_printf("  dsisr:\t%#lx\n", reg);
#else
	reg = tf->cpu.booke.esr;
	db_printf("  esr:\t%#lx\n", reg);
	reg = tf->cpu.booke.dbcr0;
	db_printf("  dbcr0:\t%#lx\n", reg);
#endif
}
#endif

/* __stack_chk_fail_local() is called in secure-plt (32-bit). */
#if !defined(__powerpc64__)
extern void __stack_chk_fail(void);
void __stack_chk_fail_local(void);

void
__stack_chk_fail_local(void)
{

	__stack_chk_fail();
}
#endif
