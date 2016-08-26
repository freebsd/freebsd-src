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
			printf("0x%016jx - 0x%016jx, %jd bytes (%jd pages)\n",
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
	    ptoa((uintmax_t)vm_cnt.v_free_count),
	    ptoa((uintmax_t)vm_cnt.v_free_count) / 1048576);

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

void aim_cpu_init(vm_offset_t toc);
void booke_cpu_init(void);

uintptr_t
powerpc_init(vm_offset_t fdt, vm_offset_t toc, vm_offset_t ofentry, void *mdp)
{
	struct		pcpu *pc;
	vm_offset_t	startkernel, endkernel;
	void		*kmdp;
        char		*env;
#ifdef DDB
	vm_offset_t ksym_start;
	vm_offset_t ksym_end;
#endif

	kmdp = NULL;

	/* First guess at start/end kernel positions */
	startkernel = __startkernel;
	endkernel = __endkernel;

	/* Check for ePAPR loader, which puts a magic value into r6 */
	if (mdp == (void *)0x65504150)
		mdp = NULL;

#ifdef AIM
	/*
	 * If running from an FDT, make sure we are in real mode to avoid
	 * tromping on firmware page tables. Everything in the kernel assumes
	 * 1:1 mappings out of firmware, so this won't break anything not
	 * already broken. This doesn't work if there is live OF, since OF
	 * may internally use non-1:1 mappings.
	 */
	if (ofentry == 0)
		mtmsr(mfmsr() & ~(PSL_IR | PSL_DR));
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
			init_static_kenv(MD_FETCH(kmdp, MODINFOMD_ENVP, char *),
			    0);
			endkernel = ulmax(endkernel, MD_FETCH(kmdp,
			    MODINFOMD_KERNEND, vm_offset_t));
#ifdef DDB
			ksym_start = MD_FETCH(kmdp, MODINFOMD_SSYM, uintptr_t);
			ksym_end = MD_FETCH(kmdp, MODINFOMD_ESYM, uintptr_t);
			db_fetch_ksymtab(ksym_start, ksym_end);
#endif
		}
	} else {
#if !defined(BOOKE)
		/*
		 * On BOOKE the BSS is already cleared and some variables
		 * initialized.  Do not wipe them out.
		 */
		bzero(__sbss_start, __sbss_end - __sbss_start);
		bzero(__bss_start, _end - __bss_start);
#endif
		init_static_kenv(NULL, 0);
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
	 * Bring up MMU
	 */
	pmap_bootstrap(startkernel, endkernel);
	mtmsr(PSL_KERNSET & ~PSL_EE);

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
		__asm __volatile("or 2,2,2"); /* Set high thread priority */
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
	if (td->td_md.md_spinlock_count == 0) {
		intr_restore(msr);
		__asm __volatile("or 6,6,6"); /* Set normal thread priority */
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
	*p = (*p & ~0x001ff800) | (sprno << 11);
	__syncicache(get_spr, cacheline_size);
	spr = get_spr(sprno);

	db_printf("SPR %d(%x): %lx\n", saved_sprno, saved_sprno,
	    (unsigned long)spr);
}
