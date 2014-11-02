/*-
 * Copyright (C) 2006-2012 Semihalf
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
 * $NetBSD: machdep.c,v 1.74.2.1 2000/11/01 16:13:48 tv Exp $
 */
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"
#include "opt_ddb.h"
#include "opt_kstack_pages.h"
#include "opt_platform.h"

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/cpu.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/sysctl.h>
#include <sys/exec.h>
#include <sys/ktr.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/signalvar.h>
#include <sys/sysent.h>
#include <sys/imgact.h>
#include <sys/msgbuf.h>
#include <sys/ptrace.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>

#include <machine/cpu.h>
#include <machine/kdb.h>
#include <machine/reg.h>
#include <machine/vmparam.h>
#include <machine/spr.h>
#include <machine/hid.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/md_var.h>
#include <machine/mmuvar.h>
#include <machine/sigframe.h>
#include <machine/machdep.h>
#include <machine/metadata.h>
#include <machine/platform.h>

#include <sys/linker.h>
#include <sys/reboot.h>

#include <contrib/libfdt/libfdt.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#ifdef  DEBUG
#define debugf(fmt, args...) printf(fmt, ##args)
#else
#define debugf(fmt, args...)
#endif

extern unsigned char kernel_text[];
extern unsigned char _etext[];
extern unsigned char _edata[];
extern unsigned char __bss_start[];
extern unsigned char __sbss_start[];
extern unsigned char __sbss_end[];
extern unsigned char _end[];

/*
 * Bootinfo is passed to us by legacy loaders. Save the address of the
 * structure to handle backward compatibility.
 */
uint32_t *bootinfo;

struct kva_md_info kmi;
struct pcpu __pcpu[MAXCPU];
struct trapframe frame0;
int cold = 1;
long realmem = 0;
long Maxmem = 0;
char machine[] = "powerpc";
SYSCTL_STRING(_hw, HW_MACHINE, machine, CTLFLAG_RD, machine, 0, "");

int cacheline_size = 32;

SYSCTL_INT(_machdep, CPU_CACHELINE, cacheline_size,
	   CTLFLAG_RD, &cacheline_size, 0, "");

int hw_direct_map = 0;

static void cpu_booke_startup(void *);
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_booke_startup, NULL);

void print_kernel_section_addr(void);
void print_kenv(void);
u_int booke_init(uint32_t, uint32_t);

extern int elf32_nxstack;

static void
cpu_booke_startup(void *dummy)
{
	int indx;
	unsigned long size;

	/* Initialise the decrementer-based clock. */
	decr_init();

	/* Good {morning,afternoon,evening,night}. */
	cpu_setup(PCPU_GET(cpuid));

	printf("real memory  = %lu (%ld MB)\n", ptoa(physmem),
	    ptoa(physmem) / 1048576);
	realmem = physmem;

	/* Display any holes after the first chunk of extended memory. */
	if (bootverbose) {
		printf("Physical memory chunk(s):\n");
		for (indx = 0; phys_avail[indx + 1] != 0; indx += 2) {
			size = phys_avail[indx + 1] - phys_avail[indx];

			printf("0x%08x - 0x%08x, %lu bytes (%lu pages)\n",
			    phys_avail[indx], phys_avail[indx + 1] - 1,
			    size, size / PAGE_SIZE);
		}
	}

	vm_ksubmap_init(&kmi);

	printf("avail memory = %lu (%ld MB)\n", ptoa(vm_cnt.v_free_count),
	    ptoa(vm_cnt.v_free_count) / 1048576);

	/* Set up buffers, so they can be used to read disk labels. */
	bufinit();
	vm_pager_bufferinit();

	/* Cpu supports execution permissions on the pages. */
	elf32_nxstack = 1;
}

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

void
print_kenv(void)
{
	int len;
	char *cp;

	debugf("loader passed (static) kenv:\n");
	if (kern_envp == NULL) {
		debugf(" no env, null ptr\n");
		return;
	}
	debugf(" kern_envp = 0x%08x\n", (u_int32_t)kern_envp);

	len = 0;
	for (cp = kern_envp; cp != NULL; cp = kenv_next(cp))
		debugf(" %x %s\n", (u_int32_t)cp, cp);
}

void
print_kernel_section_addr(void)
{

	debugf("kernel image addresses:\n");
	debugf(" kernel_text    = 0x%08x\n", (uint32_t)kernel_text);
	debugf(" _etext (sdata) = 0x%08x\n", (uint32_t)_etext);
	debugf(" _edata         = 0x%08x\n", (uint32_t)_edata);
	debugf(" __sbss_start   = 0x%08x\n", (uint32_t)__sbss_start);
	debugf(" __sbss_end     = 0x%08x\n", (uint32_t)__sbss_end);
	debugf(" __sbss_start   = 0x%08x\n", (uint32_t)__bss_start);
	debugf(" _end           = 0x%08x\n", (uint32_t)_end);
}

static int
booke_check_for_fdt(uint32_t arg1, vm_offset_t *dtbp)
{
	void *ptr;

	if (arg1 % 8 != 0)
		return (-1);

	ptr = (void *)pmap_early_io_map(arg1, PAGE_SIZE);
	if (fdt_check_header(ptr) != 0)
		return (-1);

	*dtbp = (vm_offset_t)ptr;

	return (0);
}

u_int
booke_init(uint32_t arg1, uint32_t arg2)
{
	struct pcpu *pc;
	void *kmdp, *mdp;
	vm_offset_t dtbp, end;
#ifdef DDB
	vm_offset_t ksym_start;
	vm_offset_t ksym_end;
#endif

	kmdp = NULL;

	end = (uintptr_t)_end;
	dtbp = (vm_offset_t)NULL;

	/* Set up TLB initially */
	bootinfo = NULL;
	tlb1_init();

	/*
	 * Handle the various ways we can get loaded and started:
	 *  -	FreeBSD's loader passes the pointer to the metadata
	 *	in arg1, with arg2 undefined. arg1 has a value that's
	 *	relative to the kernel's link address (i.e. larger
	 *	than 0xc0000000).
	 *  -	Juniper's loader passes the metadata pointer in arg2
	 *	and sets arg1 to zero. This is to signal that the
	 *	loader maps the kernel and starts it at its link
	 *	address (unlike the FreeBSD loader).
	 *  -	U-Boot passes the standard argc and argv parameters
	 *	in arg1 and arg2 (resp). arg1 is between 1 and some
	 *	relatively small number, such as 64K. arg2 is the
	 *	physical address of the argv vector.
	 *  -   ePAPR loaders pass an FDT blob in r3 (arg1) and the magic hex
	 *      string 0x45504150 ('ePAP') in r6 (which has been lost by now).
	 *      r4 (arg2) is supposed to be set to zero, but is not always.
	 */
	
	if (arg1 == 0)				/* Juniper loader */
		mdp = (void *)arg2;
	else if (booke_check_for_fdt(arg1, &dtbp) == 0) { /* ePAPR */
		end = roundup(end, 8);
		memmove((void *)end, (void *)dtbp, fdt_totalsize((void *)dtbp));
		dtbp = end;
		end += fdt_totalsize((void *)dtbp);
		mdp = NULL;
	} else if (arg1 > (uintptr_t)kernel_text)	/* FreeBSD loader */
		mdp = (void *)arg1;
	else					/* U-Boot */
		mdp = NULL;

	/*
	 * Parse metadata and fetch parameters.
	 */
	if (mdp != NULL) {
		preload_metadata = mdp;
		kmdp = preload_search_by_type("elf kernel");
		if (kmdp != NULL) {
			boothowto = MD_FETCH(kmdp, MODINFOMD_HOWTO, int);
			kern_envp = MD_FETCH(kmdp, MODINFOMD_ENVP, char *);
			dtbp = MD_FETCH(kmdp, MODINFOMD_DTBP, vm_offset_t);
			end = MD_FETCH(kmdp, MODINFOMD_KERNEND, vm_offset_t);

			bootinfo = (uint32_t *)preload_search_info(kmdp,
			    MODINFO_METADATA | MODINFOMD_BOOTINFO);

#ifdef DDB
			ksym_start = MD_FETCH(kmdp, MODINFOMD_SSYM, uintptr_t);
			ksym_end = MD_FETCH(kmdp, MODINFOMD_ESYM, uintptr_t);
			db_fetch_ksymtab(ksym_start, ksym_end);
#endif
		}
	} else {
		bzero(__sbss_start, __sbss_end - __sbss_start);
		bzero(__bss_start, _end - __bss_start);
	}

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

	OF_interpret("perform-fixup", 0);
	
	/* Reset TLB1 to get rid of temporary mappings */
	tlb1_init();

	/* Reset Time Base */
	mttb(0);

	/* Init params/tunables that can be overridden by the loader. */
	init_param1();

	/* Start initializing proc0 and thread0. */
	proc_linkup0(&proc0, &thread0);
	thread0.td_frame = &frame0;

	/* Set up per-cpu data and store the pointer in SPR general 0. */
	pc = &__pcpu[0];
	pcpu_init(pc, 0, sizeof(struct pcpu));
	pc->pc_curthread = &thread0;
#ifdef __powerpc64__
	__asm __volatile("mr 13,%0" :: "r"(pc->pc_curthread));
#else
	__asm __volatile("mr 2,%0" :: "r"(pc->pc_curthread));
#endif
	__asm __volatile("mtsprg 0, %0" :: "r"(pc));

	/* Initialize system mutexes. */
	mutex_init();

	/* Initialize the console before printing anything. */
	cninit();

	/* Print out some debug info... */
	debugf("%s: console initialized\n", __func__);
	debugf(" arg3 mdp = 0x%08x\n", (u_int32_t)mdp);
	debugf(" end = 0x%08x\n", (u_int32_t)end);
	debugf(" boothowto = 0x%08x\n", boothowto);
	debugf(" kernel ccsrbar = 0x%08x\n", CCSRBAR_VA);
	debugf(" MSR = 0x%08x\n", mfmsr());
#if defined(BOOKE_E500)
	debugf(" HID0 = 0x%08x\n", mfspr(SPR_HID0));
	debugf(" HID1 = 0x%08x\n", mfspr(SPR_HID1));
	debugf(" BUCSR = 0x%08x\n", mfspr(SPR_BUCSR));
#endif

	debugf(" dtbp = 0x%08x\n", (uint32_t)dtbp);

	print_kernel_section_addr();
	print_kenv();
#if defined(BOOKE_E500)
	//tlb1_print_entries();
	//tlb1_print_tlbentries();
#endif

	kdb_init();

#ifdef KDB
	if (boothowto & RB_KDB)
		kdb_enter(KDB_WHY_BOOTFLAGS, "Boot flags requested debugger");
#endif

	/* Initialise platform module */
	platform_probe_and_attach();

	/* Initialise virtual memory. */
	pmap_mmu_install(MMU_TYPE_BOOKE, 0);
	pmap_bootstrap((uintptr_t)kernel_text, end);
	pmap_bootstrapped = 1;
	debugf("MSR = 0x%08x\n", mfmsr());
#if defined(BOOKE_E500)
	//tlb1_print_entries();
	//tlb1_print_tlbentries();
#endif

	/* Initialize params/tunables that are derived from memsize. */
	init_param2(physmem);

	/* Finish setting up thread0. */
	thread0.td_pcb = (struct pcb *)
	    ((thread0.td_kstack + thread0.td_kstack_pages * PAGE_SIZE -
	    sizeof(struct pcb)) & ~15);
	bzero((void *)thread0.td_pcb, sizeof(struct pcb));
	pc->pc_curpcb = thread0.td_pcb;

	/* Initialise the message buffer. */
	msgbufinit(msgbufp, msgbufsize);

	/* Enable Machine Check interrupt. */
	mtmsr(mfmsr() | PSL_ME);
	isync();

	/* Enable L1 caches */
	booke_enable_l1_cache();

	debugf("%s: SP = 0x%08x\n", __func__,
	    ((uintptr_t)thread0.td_pcb - 16) & ~15);

	return (((uintptr_t)thread0.td_pcb - 16) & ~15);
}

#define RES_GRANULE 32
extern uint32_t tlb0_miss_locks[];

/* Initialise a struct pcpu. */
void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t sz)
{

	pcpu->pc_tid_next = TID_MIN;

#ifdef SMP
	uint32_t *ptr;
	int words_per_gran = RES_GRANULE / sizeof(uint32_t);

	ptr = &tlb0_miss_locks[cpuid * words_per_gran];
	pcpu->pc_booke_tlb_lock = ptr;
	*ptr = TLB_UNLOCKED;
	*(ptr + 1) = 0;		/* recurse counter */
#endif
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
	len = (len + off + cacheline_size - 1) & ~(cacheline_size - 1);

	while (len > 0) {
		__asm __volatile ("dcbf 0,%0" :: "r"(addr));
		__asm __volatile ("sync");
		addr += cacheline_size;
		len -= cacheline_size;
	}
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

/* Shutdown the CPU as much as possible. */
void
cpu_halt(void)
{

	mtmsr(mfmsr() & ~(PSL_CE | PSL_EE | PSL_ME | PSL_DE));
	while (1)
		;
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
	tf->srr1 |= PSL_DE;
	tf->cpu.booke.dbcr0 |= (DBCR0_IDM | DBCR0_IC);
	return (0);
}

int
ptrace_clear_single_step(struct thread *td)
{
	struct trapframe *tf;

	tf = td->td_frame;
	tf->srr1 &= ~PSL_DE;
	tf->cpu.booke.dbcr0 &= ~(DBCR0_IDM | DBCR0_IC);
	return (0);
}

void
kdb_cpu_clear_singlestep(void)
{
	register_t r;

	r = mfspr(SPR_DBCR0);
	mtspr(SPR_DBCR0, r & ~DBCR0_IC);
	kdb_frame->srr1 &= ~PSL_DE;
}

void
kdb_cpu_set_singlestep(void)
{
	register_t r;

	r = mfspr(SPR_DBCR0);
	mtspr(SPR_DBCR0, r | DBCR0_IC | DBCR0_IDM);
	kdb_frame->srr1 |= PSL_DE;
}

void
bzero(void *buf, size_t len)
{
	caddr_t p;

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

