/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 2003 Peter Wemm.
 * Copyright (c) 1992 Terrence R. Lambert.
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	from: @(#)machdep.c	7.4 (Berkeley) 6/3/91
 */

#include <sys/cdefs.h>
#include "opt_atpic.h"
#include "opt_cpu.h"
#include "opt_ddb.h"
#include "opt_inet.h"
#include "opt_isa.h"
#include "opt_kstack_pages.h"
#include "opt_maxmem.h"
#include "opt_pci.h"
#include "opt_platform.h"
#include "opt_sched.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/asan.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/cons.h>
#include <sys/cpu.h>
#include <sys/csan.h>
#include <sys/efi.h>
#include <sys/eventhandler.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <sys/msan.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/ptrace.h>
#include <sys/reboot.h>
#include <sys/reg.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#ifdef SMP
#include <sys/smp.h>
#endif
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/ucontext.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>
#include <vm/vm_phys.h>
#include <vm/vm_dumpset.h>

#ifdef DDB
#ifndef KDB
#error KDB must be enabled in order for DDB to work!
#endif
#include <ddb/ddb.h>
#include <ddb/db_sym.h>
#endif

#include <net/netisr.h>

#include <dev/smbios/smbios.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/cputypes.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <x86/mca.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/pc/bios.h>
#include <machine/pcb.h>
#include <machine/proc.h>
#include <machine/sigframe.h>
#include <machine/specialreg.h>
#include <machine/trap.h>
#include <machine/tss.h>
#include <x86/ucode.h>
#include <x86/ifunc.h>
#ifdef SMP
#include <machine/smp.h>
#endif
#ifdef FDT
#include <x86/fdt.h>
#endif

#ifdef DEV_ATPIC
#include <x86/isa/icu.h>
#else
#include <x86/apicvar.h>
#endif

#include <isa/isareg.h>
#include <isa/rtc.h>
#include <x86/init.h>

/* Sanity check for __curthread() */
CTASSERT(offsetof(struct pcpu, pc_curthread) == 0);

/*
 * The PTI trampoline stack needs enough space for a hardware trapframe and a
 * couple of scratch registers, as well as the trapframe left behind after an
 * iret fault.
 */
CTASSERT(PC_PTI_STACK_SZ * sizeof(register_t) >= 2 * sizeof(struct pti_frame) -
    offsetof(struct pti_frame, pti_rip));

extern u_int64_t hammer_time(u_int64_t, u_int64_t);

static void cpu_startup(void *);
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL);

/* Probe 8254 PIT and TSC. */
static void native_clock_source_init(void);

/* Preload data parse function */
static caddr_t native_parse_preload_data(u_int64_t);

/* Native function to fetch and parse the e820 map */
static void native_parse_memmap(caddr_t, vm_paddr_t *, int *);

/* Default init_ops implementation. */
struct init_ops init_ops = {
	.parse_preload_data =		native_parse_preload_data,
	.early_clock_source_init =	native_clock_source_init,
	.early_delay =			i8254_delay,
	.parse_memmap =			native_parse_memmap,
};

/*
 * Physical address of the EFI System Table. Stashed from the metadata hints
 * passed into the kernel and used by the EFI code to call runtime services.
 */
vm_paddr_t efi_systbl_phys;

/* Intel ICH registers */
#define ICH_PMBASE	0x400
#define ICH_SMI_EN	ICH_PMBASE + 0x30

int	_udatasel, _ucodesel, _ucode32sel, _ufssel, _ugssel;

int cold = 1;

long Maxmem = 0;
long realmem = 0;
int late_console = 1;

struct kva_md_info kmi;

struct region_descriptor r_idt;

struct pcpu *__pcpu;
struct pcpu temp_bsp_pcpu;

struct mtx icu_lock;

struct mem_range_softc mem_range_softc;

struct mtx dt_lock;	/* lock for GDT and LDT */

void (*vmm_resume_p)(void);

bool efi_boot;

static void
cpu_startup(void *dummy)
{
	uintmax_t memsize;
	char *sysenv;

	/*
	 * On MacBooks, we need to disallow the legacy USB circuit to
	 * generate an SMI# because this can cause several problems,
	 * namely: incorrect CPU frequency detection and failure to
	 * start the APs.
	 * We do this by disabling a bit in the SMI_EN (SMI Control and
	 * Enable register) of the Intel ICH LPC Interface Bridge. 
	 */
	sysenv = kern_getenv("smbios.system.product");
	if (sysenv != NULL) {
		if (strncmp(sysenv, "MacBook1,1", 10) == 0 ||
		    strncmp(sysenv, "MacBook3,1", 10) == 0 ||
		    strncmp(sysenv, "MacBook4,1", 10) == 0 ||
		    strncmp(sysenv, "MacBookPro1,1", 13) == 0 ||
		    strncmp(sysenv, "MacBookPro1,2", 13) == 0 ||
		    strncmp(sysenv, "MacBookPro3,1", 13) == 0 ||
		    strncmp(sysenv, "MacBookPro4,1", 13) == 0 ||
		    strncmp(sysenv, "Macmini1,1", 10) == 0) {
			if (bootverbose)
				printf("Disabling LEGACY_USB_EN bit on "
				    "Intel ICH.\n");
			outl(ICH_SMI_EN, inl(ICH_SMI_EN) & ~0x8);
		}
		freeenv(sysenv);
	}

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	startrtclock();
	printcpuinfo();

	/*
	 * Display physical memory if SMBIOS reports reasonable amount.
	 */
	memsize = 0;
	sysenv = kern_getenv("smbios.memory.enabled");
	if (sysenv != NULL) {
		memsize = (uintmax_t)strtoul(sysenv, (char **)NULL, 10) << 10;
		freeenv(sysenv);
	}
	if (memsize < ptoa((uintmax_t)vm_free_count()))
		memsize = ptoa((uintmax_t)Maxmem);
	printf("real memory  = %ju (%ju MB)\n", memsize, memsize >> 20);
	realmem = atop(memsize);

	/*
	 * Display any holes after the first chunk of extended memory.
	 */
	if (bootverbose) {
		int indx;

		printf("Physical memory chunk(s):\n");
		for (indx = 0; phys_avail[indx + 1] != 0; indx += 2) {
			vm_paddr_t size;

			size = phys_avail[indx + 1] - phys_avail[indx];
			printf(
			    "0x%016jx - 0x%016jx, %ju bytes (%ju pages)\n",
			    (uintmax_t)phys_avail[indx],
			    (uintmax_t)phys_avail[indx + 1] - 1,
			    (uintmax_t)size, (uintmax_t)size / PAGE_SIZE);
		}
	}

	vm_ksubmap_init(&kmi);

	printf("avail memory = %ju (%ju MB)\n",
	    ptoa((uintmax_t)vm_free_count()),
	    ptoa((uintmax_t)vm_free_count()) / 1048576);
#ifdef DEV_PCI
	if (bootverbose && intel_graphics_stolen_base != 0)
		printf("intel stolen mem: base %#jx size %ju MB\n",
		    (uintmax_t)intel_graphics_stolen_base,
		    (uintmax_t)intel_graphics_stolen_size / 1024 / 1024);
#endif

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
	vm_pager_bufferinit();

	cpu_setregs();
}

static void
late_ifunc_resolve(void *dummy __unused)
{
	link_elf_late_ireloc();
}
SYSINIT(late_ifunc_resolve, SI_SUB_CPU, SI_ORDER_ANY, late_ifunc_resolve, NULL);


void
cpu_setregs(void)
{
	register_t cr0;

	TSENTER();
	cr0 = rcr0();
	cr0 |= CR0_MP | CR0_NE | CR0_TS | CR0_WP | CR0_AM;
	TSENTER2("load_cr0");
	load_cr0(cr0);
	TSEXIT2("load_cr0");
	TSEXIT();
}

/*
 * Initialize amd64 and configure to run kernel
 */

/*
 * Initialize segments & interrupt table
 */
static struct gate_descriptor idt0[NIDT];
struct gate_descriptor *idt = &idt0[0];	/* interrupt descriptor table */

static char dblfault_stack[DBLFAULT_STACK_SIZE] __aligned(16);
static char mce0_stack[MCE_STACK_SIZE] __aligned(16);
static char nmi0_stack[NMI_STACK_SIZE] __aligned(16);
static char dbg0_stack[DBG_STACK_SIZE] __aligned(16);
CTASSERT(sizeof(struct nmi_pcpu) == 16);

/*
 * Software prototypes -- in more palatable form.
 *
 * Keep GUFS32, GUGS32, GUCODE32 and GUDATA at the same
 * slots as corresponding segments for i386 kernel.
 */
struct soft_segment_descriptor gdt_segs[] = {
[GNULL_SEL] = { /* 0 Null Descriptor */
	.ssd_base = 0x0,
	.ssd_limit = 0x0,
	.ssd_type = 0,
	.ssd_dpl = 0,
	.ssd_p = 0,
	.ssd_long = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
[GNULL2_SEL] = { /*	1 Null Descriptor */
	.ssd_base = 0x0,
	.ssd_limit = 0x0,
	.ssd_type = 0,
	.ssd_dpl = 0,
	.ssd_p = 0,
	.ssd_long = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
[GUFS32_SEL] = { /* 2 32 bit %gs Descriptor for user */
	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = SEL_UPL,
	.ssd_p = 1,
	.ssd_long = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
[GUGS32_SEL] = { /* 3 32 bit %fs Descriptor for user */
	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = SEL_UPL,
	.ssd_p = 1,
	.ssd_long = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
[GCODE_SEL] = { /* 4 Code Descriptor for kernel */
	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMERA,
	.ssd_dpl = SEL_KPL,
	.ssd_p = 1,
	.ssd_long = 1,
	.ssd_def32 = 0,
	.ssd_gran = 1		},
[GDATA_SEL] = { /* 5 Data Descriptor for kernel */
	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = SEL_KPL,
	.ssd_p = 1,
	.ssd_long = 1,
	.ssd_def32 = 0,
	.ssd_gran = 1		},
[GUCODE32_SEL] = { /* 6 32 bit Code Descriptor for user */
	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMERA,
	.ssd_dpl = SEL_UPL,
	.ssd_p = 1,
	.ssd_long = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
[GUDATA_SEL] = { /* 7 32/64 bit Data Descriptor for user */
	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = SEL_UPL,
	.ssd_p = 1,
	.ssd_long = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
[GUCODE_SEL] = { /* 8 64 bit Code Descriptor for user */
	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMERA,
	.ssd_dpl = SEL_UPL,
	.ssd_p = 1,
	.ssd_long = 1,
	.ssd_def32 = 0,
	.ssd_gran = 1		},
[GPROC0_SEL] = { /* 9 Proc 0 TSS Descriptor */
	.ssd_base = 0x0,
	.ssd_limit = sizeof(struct amd64tss) + IOPERM_BITMAP_SIZE - 1,
	.ssd_type = SDT_SYSTSS,
	.ssd_dpl = SEL_KPL,
	.ssd_p = 1,
	.ssd_long = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
[GPROC0_SEL + 1] = { /* 10 Proc 0 TSS descriptor, double size */
	.ssd_base = 0x0,
	.ssd_limit = 0x0,
	.ssd_type = 0,
	.ssd_dpl = 0,
	.ssd_p = 0,
	.ssd_long = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
[GUSERLDT_SEL] = { /* 11 LDT Descriptor */
	.ssd_base = 0x0,
	.ssd_limit = 0x0,
	.ssd_type = 0,
	.ssd_dpl = 0,
	.ssd_p = 0,
	.ssd_long = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
[GUSERLDT_SEL + 1] = { /* 12 LDT Descriptor, double size */
	.ssd_base = 0x0,
	.ssd_limit = 0x0,
	.ssd_type = 0,
	.ssd_dpl = 0,
	.ssd_p = 0,
	.ssd_long = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
};
_Static_assert(nitems(gdt_segs) == NGDT, "Stale NGDT");

void
setidt(int idx, inthand_t *func, int typ, int dpl, int ist)
{
	struct gate_descriptor *ip;

	ip = idt + idx;
	ip->gd_looffset = (uintptr_t)func;
	ip->gd_selector = GSEL(GCODE_SEL, SEL_KPL);
	ip->gd_ist = ist;
	ip->gd_xx = 0;
	ip->gd_type = typ;
	ip->gd_dpl = dpl;
	ip->gd_p = 1;
	ip->gd_hioffset = ((uintptr_t)func)>>16 ;
}

extern inthand_t
	IDTVEC(div), IDTVEC(dbg), IDTVEC(nmi), IDTVEC(bpt), IDTVEC(ofl),
	IDTVEC(bnd), IDTVEC(ill), IDTVEC(dna), IDTVEC(fpusegm),
	IDTVEC(tss), IDTVEC(missing), IDTVEC(stk), IDTVEC(prot),
	IDTVEC(page), IDTVEC(mchk), IDTVEC(rsvd), IDTVEC(fpu), IDTVEC(align),
	IDTVEC(xmm), IDTVEC(dblfault),
	IDTVEC(div_pti), IDTVEC(bpt_pti),
	IDTVEC(ofl_pti), IDTVEC(bnd_pti), IDTVEC(ill_pti), IDTVEC(dna_pti),
	IDTVEC(fpusegm_pti), IDTVEC(tss_pti), IDTVEC(missing_pti),
	IDTVEC(stk_pti), IDTVEC(prot_pti), IDTVEC(page_pti),
	IDTVEC(rsvd_pti), IDTVEC(fpu_pti), IDTVEC(align_pti),
	IDTVEC(xmm_pti),
#ifdef KDTRACE_HOOKS
	IDTVEC(dtrace_ret), IDTVEC(dtrace_ret_pti),
#endif
#ifdef XENHVM
	IDTVEC(xen_intr_upcall), IDTVEC(xen_intr_upcall_pti),
#endif
	IDTVEC(fast_syscall), IDTVEC(fast_syscall32),
	IDTVEC(fast_syscall_pti);

#ifdef DDB
/*
 * Display the index and function name of any IDT entries that don't use
 * the default 'rsvd' entry point.
 */
DB_SHOW_COMMAND_FLAGS(idt, db_show_idt, DB_CMD_MEMSAFE)
{
	struct gate_descriptor *ip;
	int idx;
	uintptr_t func;

	ip = idt;
	for (idx = 0; idx < NIDT && !db_pager_quit; idx++) {
		func = ((long)ip->gd_hioffset << 16 | ip->gd_looffset);
		if (func != (uintptr_t)&IDTVEC(rsvd)) {
			db_printf("%3d\t", idx);
			db_printsym(func, DB_STGY_PROC);
			db_printf("\n");
		}
		ip++;
	}
}

/* Show privileged registers. */
DB_SHOW_COMMAND_FLAGS(sysregs, db_show_sysregs, DB_CMD_MEMSAFE)
{
	struct {
		uint16_t limit;
		uint64_t base;
	} __packed idtr, gdtr;
	uint16_t ldt, tr;

	__asm __volatile("sidt %0" : "=m" (idtr));
	db_printf("idtr\t0x%016lx/%04x\n",
	    (u_long)idtr.base, (u_int)idtr.limit);
	__asm __volatile("sgdt %0" : "=m" (gdtr));
	db_printf("gdtr\t0x%016lx/%04x\n",
	    (u_long)gdtr.base, (u_int)gdtr.limit);
	__asm __volatile("sldt %0" : "=r" (ldt));
	db_printf("ldtr\t0x%04x\n", ldt);
	__asm __volatile("str %0" : "=r" (tr));
	db_printf("tr\t0x%04x\n", tr);
	db_printf("cr0\t0x%016lx\n", rcr0());
	db_printf("cr2\t0x%016lx\n", rcr2());
	db_printf("cr3\t0x%016lx\n", rcr3());
	db_printf("cr4\t0x%016lx\n", rcr4());
	if (rcr4() & CR4_XSAVE)
		db_printf("xcr0\t0x%016lx\n", rxcr(0));
	db_printf("EFER\t0x%016lx\n", rdmsr(MSR_EFER));
	if (cpu_feature2 & (CPUID2_VMX | CPUID2_SMX))
		db_printf("FEATURES_CTL\t%016lx\n",
		    rdmsr(MSR_IA32_FEATURE_CONTROL));
	db_printf("DEBUG_CTL\t0x%016lx\n", rdmsr(MSR_DEBUGCTLMSR));
	db_printf("PAT\t0x%016lx\n", rdmsr(MSR_PAT));
	db_printf("GSBASE\t0x%016lx\n", rdmsr(MSR_GSBASE));
}

DB_SHOW_COMMAND_FLAGS(dbregs, db_show_dbregs, DB_CMD_MEMSAFE)
{

	db_printf("dr0\t0x%016lx\n", rdr0());
	db_printf("dr1\t0x%016lx\n", rdr1());
	db_printf("dr2\t0x%016lx\n", rdr2());
	db_printf("dr3\t0x%016lx\n", rdr3());
	db_printf("dr6\t0x%016lx\n", rdr6());
	db_printf("dr7\t0x%016lx\n", rdr7());
}
#endif

void
sdtossd(struct user_segment_descriptor *sd, struct soft_segment_descriptor *ssd)
{

	ssd->ssd_base  = (sd->sd_hibase << 24) | sd->sd_lobase;
	ssd->ssd_limit = (sd->sd_hilimit << 16) | sd->sd_lolimit;
	ssd->ssd_type  = sd->sd_type;
	ssd->ssd_dpl   = sd->sd_dpl;
	ssd->ssd_p     = sd->sd_p;
	ssd->ssd_long  = sd->sd_long;
	ssd->ssd_def32 = sd->sd_def32;
	ssd->ssd_gran  = sd->sd_gran;
}

void
ssdtosd(struct soft_segment_descriptor *ssd, struct user_segment_descriptor *sd)
{

	sd->sd_lobase = (ssd->ssd_base) & 0xffffff;
	sd->sd_hibase = (ssd->ssd_base >> 24) & 0xff;
	sd->sd_lolimit = (ssd->ssd_limit) & 0xffff;
	sd->sd_hilimit = (ssd->ssd_limit >> 16) & 0xf;
	sd->sd_type  = ssd->ssd_type;
	sd->sd_dpl   = ssd->ssd_dpl;
	sd->sd_p     = ssd->ssd_p;
	sd->sd_long  = ssd->ssd_long;
	sd->sd_def32 = ssd->ssd_def32;
	sd->sd_gran  = ssd->ssd_gran;
}

void
ssdtosyssd(struct soft_segment_descriptor *ssd, struct system_segment_descriptor *sd)
{

	sd->sd_lobase = (ssd->ssd_base) & 0xffffff;
	sd->sd_hibase = (ssd->ssd_base >> 24) & 0xfffffffffful;
	sd->sd_lolimit = (ssd->ssd_limit) & 0xffff;
	sd->sd_hilimit = (ssd->ssd_limit >> 16) & 0xf;
	sd->sd_type  = ssd->ssd_type;
	sd->sd_dpl   = ssd->ssd_dpl;
	sd->sd_p     = ssd->ssd_p;
	sd->sd_gran  = ssd->ssd_gran;
}

u_int basemem;

static int
add_physmap_entry(uint64_t base, uint64_t length, vm_paddr_t *physmap,
    int *physmap_idxp)
{
	int i, insert_idx, physmap_idx;

	physmap_idx = *physmap_idxp;

	if (length == 0)
		return (1);

	/*
	 * Find insertion point while checking for overlap.  Start off by
	 * assuming the new entry will be added to the end.
	 *
	 * NB: physmap_idx points to the next free slot.
	 */
	insert_idx = physmap_idx;
	for (i = 0; i <= physmap_idx; i += 2) {
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
	if (insert_idx <= physmap_idx && base + length == physmap[insert_idx]) {
		physmap[insert_idx] = base;
		return (1);
	}

	/* See if we can append to the previous entry. */
	if (insert_idx > 0 && base == physmap[insert_idx - 1]) {
		physmap[insert_idx - 1] += length;
		return (1);
	}

	physmap_idx += 2;
	*physmap_idxp = physmap_idx;
	if (physmap_idx == PHYS_AVAIL_ENTRIES) {
		printf(
		"Too many segments in the physical address map, giving up\n");
		return (0);
	}

	/*
	 * Move the last 'N' entries down to make room for the new
	 * entry if needed.
	 */
	for (i = (physmap_idx - 2); i > insert_idx; i -= 2) {
		physmap[i] = physmap[i - 2];
		physmap[i + 1] = physmap[i - 1];
	}

	/* Insert the new entry. */
	physmap[insert_idx] = base;
	physmap[insert_idx + 1] = base + length;
	return (1);
}

void
bios_add_smap_entries(struct bios_smap *smapbase, u_int32_t smapsize,
                      vm_paddr_t *physmap, int *physmap_idx)
{
	struct bios_smap *smap, *smapend;

	smapend = (struct bios_smap *)((uintptr_t)smapbase + smapsize);

	for (smap = smapbase; smap < smapend; smap++) {
		if (boothowto & RB_VERBOSE)
			printf("SMAP type=%02x base=%016lx len=%016lx\n",
			    smap->type, smap->base, smap->length);

		if (smap->type != SMAP_TYPE_MEMORY)
			continue;

		if (!add_physmap_entry(smap->base, smap->length, physmap,
		    physmap_idx))
			break;
	}
}

static void
add_efi_map_entries(struct efi_map_header *efihdr, vm_paddr_t *physmap,
    int *physmap_idx)
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
		"PalCode",
		"PersistentMemory"
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
			if (p->md_type < nitems(types))
				type = types[p->md_type];
			else
				type = "<INVALID>";
			printf("%23s %012lx %012lx %08lx ", type, p->md_phys,
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
			if (p->md_attr & EFI_MD_ATTR_NV)
				printf("NV ");
			if (p->md_attr & EFI_MD_ATTR_MORE_RELIABLE)
				printf("MORE_RELIABLE ");
			if (p->md_attr & EFI_MD_ATTR_RO)
				printf("RO ");
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

		if (!add_physmap_entry(p->md_phys, p->md_pages * EFI_PAGE_SIZE,
		    physmap, physmap_idx))
			break;
	}
}

static void
native_parse_memmap(caddr_t kmdp, vm_paddr_t *physmap, int *physmap_idx)
{
	struct bios_smap *smap;
	struct efi_map_header *efihdr;
	u_int32_t size;

	/*
	 * Memory map from INT 15:E820.
	 *
	 * subr_module.c says:
	 * "Consumer may safely assume that size value precedes data."
	 * ie: an int32_t immediately precedes smap.
	 */

	efihdr = (struct efi_map_header *)preload_search_info(kmdp,
	    MODINFO_METADATA | MODINFOMD_EFI_MAP);
	smap = (struct bios_smap *)preload_search_info(kmdp,
	    MODINFO_METADATA | MODINFOMD_SMAP);
	if (efihdr == NULL && smap == NULL)
		panic("No BIOS smap or EFI map info from loader!");

	if (efihdr != NULL) {
		add_efi_map_entries(efihdr, physmap, physmap_idx);
		strlcpy(bootmethod, "UEFI", sizeof(bootmethod));
	} else {
		size = *((u_int32_t *)smap - 1);
		bios_add_smap_entries(smap, size, physmap, physmap_idx);
		strlcpy(bootmethod, "BIOS", sizeof(bootmethod));
	}
}

#define	PAGES_PER_GB	(1024 * 1024 * 1024 / PAGE_SIZE)

/*
 * Populate the (physmap) array with base/bound pairs describing the
 * available physical memory in the system, then test this memory and
 * build the phys_avail array describing the actually-available memory.
 *
 * Total memory size may be set by the kernel environment variable
 * hw.physmem or the compile-time define MAXMEM.
 *
 * XXX first should be vm_paddr_t.
 */
static void
getmemsize(caddr_t kmdp, u_int64_t first)
{
	int i, physmap_idx, pa_indx, da_indx;
	vm_paddr_t pa, physmap[PHYS_AVAIL_ENTRIES];
	u_long physmem_start, physmem_tunable, memtest;
	pt_entry_t *pte;
	quad_t dcons_addr, dcons_size;
	int page_counter;

	TSENTER();
	/*
	 * Tell the physical memory allocator about pages used to store
	 * the kernel and preloaded data.  See kmem_bootstrap_free().
	 */
	vm_phys_early_add_seg((vm_paddr_t)kernphys, trunc_page(first));

	bzero(physmap, sizeof(physmap));
	physmap_idx = 0;

	init_ops.parse_memmap(kmdp, physmap, &physmap_idx);
	physmap_idx -= 2;

	/*
	 * Find the 'base memory' segment for SMP
	 */
	basemem = 0;
	for (i = 0; i <= physmap_idx; i += 2) {
		if (physmap[i] <= 0xA0000) {
			basemem = physmap[i + 1] / 1024;
			break;
		}
	}
	if (basemem == 0 || basemem > 640) {
		if (bootverbose)
			printf(
		"Memory map doesn't contain a basemem segment, faking it");
		basemem = 640;
	}

	/*
	 * Maxmem isn't the "maximum memory", it's one larger than the
	 * highest page of the physical address space.  It should be
	 * called something like "Maxphyspage".  We may adjust this
	 * based on ``hw.physmem'' and the results of the memory test.
	 */
	Maxmem = atop(physmap[physmap_idx + 1]);

#ifdef MAXMEM
	Maxmem = MAXMEM / 4;
#endif

	if (TUNABLE_ULONG_FETCH("hw.physmem", &physmem_tunable))
		Maxmem = atop(physmem_tunable);

	/*
	 * The boot memory test is disabled by default, as it takes a
	 * significant amount of time on large-memory systems, and is
	 * unfriendly to virtual machines as it unnecessarily touches all
	 * pages.
	 *
	 * A general name is used as the code may be extended to support
	 * additional tests beyond the current "page present" test.
	 */
	memtest = 0;
	TUNABLE_ULONG_FETCH("hw.memtest.tests", &memtest);

	/*
	 * Don't allow MAXMEM or hw.physmem to extend the amount of memory
	 * in the system.
	 */
	if (Maxmem > atop(physmap[physmap_idx + 1]))
		Maxmem = atop(physmap[physmap_idx + 1]);

	if (atop(physmap[physmap_idx + 1]) != Maxmem &&
	    (boothowto & RB_VERBOSE))
		printf("Physical memory use set to %ldK\n", Maxmem * 4);

	/* call pmap initialization to make new kernel address space */
	pmap_bootstrap(&first);

	/*
	 * Size up each available chunk of physical memory.
	 *
	 * XXX Some BIOSes corrupt low 64KB between suspend and resume.
	 * By default, mask off the first 16 pages unless we appear to be
	 * running in a VM.
	 */
	physmem_start = (vm_guest > VM_GUEST_NO ? 1 : 16) << PAGE_SHIFT;
	TUNABLE_ULONG_FETCH("hw.physmem.start", &physmem_start);
	if (physmap[0] < physmem_start) {
		if (physmem_start < PAGE_SIZE)
			physmap[0] = PAGE_SIZE;
		else if (physmem_start >= physmap[1])
			physmap[0] = round_page(physmap[1] - PAGE_SIZE);
		else
			physmap[0] = round_page(physmem_start);
	}
	pa_indx = 0;
	da_indx = 1;
	phys_avail[pa_indx++] = physmap[0];
	phys_avail[pa_indx] = physmap[0];
	dump_avail[da_indx] = physmap[0];
	pte = CMAP1;

	/*
	 * Get dcons buffer address
	 */
	if (getenv_quad("dcons.addr", &dcons_addr) == 0 ||
	    getenv_quad("dcons.size", &dcons_size) == 0)
		dcons_addr = 0;

	/*
	 * physmap is in bytes, so when converting to page boundaries,
	 * round up the start address and round down the end address.
	 */
	page_counter = 0;
	if (memtest != 0)
		printf("Testing system memory");
	for (i = 0; i <= physmap_idx; i += 2) {
		vm_paddr_t end;

		end = ptoa((vm_paddr_t)Maxmem);
		if (physmap[i + 1] < end)
			end = trunc_page(physmap[i + 1]);
		for (pa = round_page(physmap[i]); pa < end; pa += PAGE_SIZE) {
			int tmp, page_bad, full;
			int *ptr = (int *)CADDR1;

			full = FALSE;
			/*
			 * block out kernel memory as not available.
			 */
			if (pa >= (vm_paddr_t)kernphys && pa < first)
				goto do_dump_avail;

			/*
			 * block out dcons buffer
			 */
			if (dcons_addr > 0
			    && pa >= trunc_page(dcons_addr)
			    && pa < dcons_addr + dcons_size)
				goto do_dump_avail;

			page_bad = FALSE;
			if (memtest == 0)
				goto skip_memtest;

			/*
			 * Print a "." every GB to show we're making
			 * progress.
			 */
			page_counter++;
			if ((page_counter % PAGES_PER_GB) == 0)
				printf(".");

			/*
			 * map page into kernel: valid, read/write,non-cacheable
			 */
			*pte = pa | PG_V | PG_RW | PG_NC_PWT | PG_NC_PCD;
			invltlb();

			tmp = *(int *)ptr;
			/*
			 * Test for alternating 1's and 0's
			 */
			*(volatile int *)ptr = 0xaaaaaaaa;
			if (*(volatile int *)ptr != 0xaaaaaaaa)
				page_bad = TRUE;
			/*
			 * Test for alternating 0's and 1's
			 */
			*(volatile int *)ptr = 0x55555555;
			if (*(volatile int *)ptr != 0x55555555)
				page_bad = TRUE;
			/*
			 * Test for all 1's
			 */
			*(volatile int *)ptr = 0xffffffff;
			if (*(volatile int *)ptr != 0xffffffff)
				page_bad = TRUE;
			/*
			 * Test for all 0's
			 */
			*(volatile int *)ptr = 0x0;
			if (*(volatile int *)ptr != 0x0)
				page_bad = TRUE;
			/*
			 * Restore original value.
			 */
			*(int *)ptr = tmp;

skip_memtest:
			/*
			 * Adjust array of valid/good pages.
			 */
			if (page_bad == TRUE)
				continue;
			/*
			 * If this good page is a continuation of the
			 * previous set of good pages, then just increase
			 * the end pointer. Otherwise start a new chunk.
			 * Note that "end" points one higher than end,
			 * making the range >= start and < end.
			 * If we're also doing a speculative memory
			 * test and we at or past the end, bump up Maxmem
			 * so that we keep going. The first bad page
			 * will terminate the loop.
			 */
			if (phys_avail[pa_indx] == pa) {
				phys_avail[pa_indx] += PAGE_SIZE;
			} else {
				pa_indx++;
				if (pa_indx == PHYS_AVAIL_ENTRIES) {
					printf(
		"Too many holes in the physical address space, giving up\n");
					pa_indx--;
					full = TRUE;
					goto do_dump_avail;
				}
				phys_avail[pa_indx++] = pa;	/* start */
				phys_avail[pa_indx] = pa + PAGE_SIZE; /* end */
			}
			physmem++;
do_dump_avail:
			if (dump_avail[da_indx] == pa) {
				dump_avail[da_indx] += PAGE_SIZE;
			} else {
				da_indx++;
				if (da_indx == PHYS_AVAIL_ENTRIES) {
					da_indx--;
					goto do_next;
				}
				dump_avail[da_indx++] = pa; /* start */
				dump_avail[da_indx] = pa + PAGE_SIZE; /* end */
			}
do_next:
			if (full)
				break;
		}
	}
	*pte = 0;
	invltlb();
	if (memtest != 0)
		printf("\n");

	/*
	 * XXX
	 * The last chunk must contain at least one page plus the message
	 * buffer to avoid complicating other code (message buffer address
	 * calculation, etc.).
	 */
	while (phys_avail[pa_indx - 1] + PAGE_SIZE +
	    round_page(msgbufsize) >= phys_avail[pa_indx]) {
		physmem -= atop(phys_avail[pa_indx] - phys_avail[pa_indx - 1]);
		phys_avail[pa_indx--] = 0;
		phys_avail[pa_indx--] = 0;
	}

	Maxmem = atop(phys_avail[pa_indx]);

	/* Trim off space for the message buffer. */
	phys_avail[pa_indx] -= round_page(msgbufsize);

	/* Map the message buffer. */
	msgbufp = (struct msgbuf *)PHYS_TO_DMAP(phys_avail[pa_indx]);
	TSEXIT();
}

static caddr_t
native_parse_preload_data(u_int64_t modulep)
{
	caddr_t kmdp;
	char *envp;
#ifdef DDB
	vm_offset_t ksym_start;
	vm_offset_t ksym_end;
#endif

	preload_metadata = (caddr_t)(uintptr_t)(modulep + KERNBASE);
	preload_bootstrap_relocate(KERNBASE);
	kmdp = preload_search_by_type("elf kernel");
	if (kmdp == NULL)
		kmdp = preload_search_by_type("elf64 kernel");
	boothowto = MD_FETCH(kmdp, MODINFOMD_HOWTO, int);
	envp = MD_FETCH(kmdp, MODINFOMD_ENVP, char *);
	if (envp != NULL)
		envp += KERNBASE;
	init_static_kenv(envp, 0);
#ifdef DDB
	ksym_start = MD_FETCH(kmdp, MODINFOMD_SSYM, uintptr_t);
	ksym_end = MD_FETCH(kmdp, MODINFOMD_ESYM, uintptr_t);
	db_fetch_ksymtab(ksym_start, ksym_end, 0);
#endif
	efi_systbl_phys = MD_FETCH(kmdp, MODINFOMD_FW_HANDLE, vm_paddr_t);

	return (kmdp);
}

static void
native_clock_source_init(void)
{
	i8254_init();
}

static void
amd64_kdb_init(void)
{
	kdb_init();
#ifdef KDB
	if (boothowto & RB_KDB)
		kdb_enter(KDB_WHY_BOOTFLAGS, "Boot flags requested debugger");
#endif
}

/* Set up the fast syscall stuff */
void
amd64_conf_fast_syscall(void)
{
	uint64_t msr;

	msr = rdmsr(MSR_EFER) | EFER_SCE;
	wrmsr(MSR_EFER, msr);
	wrmsr(MSR_LSTAR, pti ? (u_int64_t)IDTVEC(fast_syscall_pti) :
	    (u_int64_t)IDTVEC(fast_syscall));
	wrmsr(MSR_CSTAR, (u_int64_t)IDTVEC(fast_syscall32));
	msr = ((u_int64_t)GSEL(GCODE_SEL, SEL_KPL) << 32) |
	    ((u_int64_t)GSEL(GUCODE32_SEL, SEL_UPL) << 48);
	wrmsr(MSR_STAR, msr);
	wrmsr(MSR_SF_MASK, PSL_NT | PSL_T | PSL_I | PSL_C | PSL_D | PSL_AC);
}

void
amd64_bsp_pcpu_init1(struct pcpu *pc)
{
	struct user_segment_descriptor *gdt;

	PCPU_SET(prvspace, pc);
	gdt = *PCPU_PTR(gdt);
	PCPU_SET(curthread, &thread0);
	PCPU_SET(tssp, PCPU_PTR(common_tss));
	PCPU_SET(tss, (struct system_segment_descriptor *)&gdt[GPROC0_SEL]);
	PCPU_SET(ldt, (struct system_segment_descriptor *)&gdt[GUSERLDT_SEL]);
	PCPU_SET(fs32p, &gdt[GUFS32_SEL]);
	PCPU_SET(gs32p, &gdt[GUGS32_SEL]);
	PCPU_SET(ucr3_load_mask, PMAP_UCR3_NOMASK);
	PCPU_SET(smp_tlb_gen, 1);
}

void
amd64_bsp_pcpu_init2(uint64_t rsp0)
{

	PCPU_SET(rsp0, rsp0);
	PCPU_SET(pti_rsp0, ((vm_offset_t)PCPU_PTR(pti_stack) +
	    PC_PTI_STACK_SZ * sizeof(uint64_t)) & ~0xful);
	PCPU_SET(curpcb, thread0.td_pcb);
}

void
amd64_bsp_ist_init(struct pcpu *pc)
{
	struct nmi_pcpu *np;
	struct amd64tss *tssp;

	tssp = &pc->pc_common_tss;

	/* doublefault stack space, runs on ist1 */
	np = ((struct nmi_pcpu *)&dblfault_stack[sizeof(dblfault_stack)]) - 1;
	np->np_pcpu = (register_t)pc;
	tssp->tss_ist1 = (long)np;

	/*
	 * NMI stack, runs on ist2.  The pcpu pointer is stored just
	 * above the start of the ist2 stack.
	 */
	np = ((struct nmi_pcpu *)&nmi0_stack[sizeof(nmi0_stack)]) - 1;
	np->np_pcpu = (register_t)pc;
	tssp->tss_ist2 = (long)np;

	/*
	 * MC# stack, runs on ist3.  The pcpu pointer is stored just
	 * above the start of the ist3 stack.
	 */
	np = ((struct nmi_pcpu *)&mce0_stack[sizeof(mce0_stack)]) - 1;
	np->np_pcpu = (register_t)pc;
	tssp->tss_ist3 = (long)np;

	/*
	 * DB# stack, runs on ist4.
	 */
	np = ((struct nmi_pcpu *)&dbg0_stack[sizeof(dbg0_stack)]) - 1;
	np->np_pcpu = (register_t)pc;
	tssp->tss_ist4 = (long)np;
}

/*
 * Calculate the kernel load address by inspecting page table created by loader.
 * The assumptions:
 * - kernel is mapped at KERNBASE, backed by contiguous phys memory
 *   aligned at 2M, below 4G (the latter is important for AP startup)
 * - there is a 2M hole at KERNBASE (KERNSTART = KERNBASE + 2M)
 * - kernel is mapped with 2M superpages
 * - all participating memory, i.e. kernel, modules, metadata,
 *   page table is accessible by pre-created 1:1 mapping
 *   (right now loader creates 1:1 mapping for lower 4G, and all
 *   memory is from there)
 * - there is a usable memory block right after the end of the
 *   mapped kernel and all modules/metadata, pointed to by
 *   physfree, for early allocations
 */
vm_paddr_t __nosanitizeaddress __nosanitizememory
amd64_loadaddr(void)
{
	pml4_entry_t *pml4e;
	pdp_entry_t *pdpe;
	pd_entry_t *pde;
	uint64_t cr3;

	cr3 = rcr3();
	pml4e = (pml4_entry_t *)cr3 + pmap_pml4e_index(KERNSTART);
	pdpe = (pdp_entry_t *)(*pml4e & PG_FRAME) + pmap_pdpe_index(KERNSTART);
	pde = (pd_entry_t *)(*pdpe & PG_FRAME) + pmap_pde_index(KERNSTART);
	return (*pde & PG_FRAME);
}

u_int64_t
hammer_time(u_int64_t modulep, u_int64_t physfree)
{
	caddr_t kmdp;
	int gsel_tss, x;
	struct pcpu *pc;
	uint64_t rsp0;
	char *env;
	struct user_segment_descriptor *gdt;
	struct region_descriptor r_gdt;
	size_t kstack0_sz;

	TSRAW(&thread0, TS_ENTER, __func__, NULL);

	kernphys = amd64_loadaddr();

	physfree += kernphys;

	kmdp = init_ops.parse_preload_data(modulep);

	efi_boot = preload_search_info(kmdp, MODINFO_METADATA |
	    MODINFOMD_EFI_MAP) != NULL;

	if (!efi_boot) {
		/* Tell the bios to warmboot next time */
		atomic_store_short((u_short *)0x472, 0x1234);
	}

	physfree += ucode_load_bsp(physfree - kernphys + KERNSTART);
	physfree = roundup2(physfree, PAGE_SIZE);

	identify_cpu1();
	identify_hypervisor();
	identify_hypervisor_smbios();
	identify_cpu_fixup_bsp();
	identify_cpu2();
	initializecpucache();

	/*
	 * Check for pti, pcid, and invpcid before ifuncs are
	 * resolved, to correctly select the implementation for
	 * pmap_activate_sw_mode().
	 */
	pti = pti_get_default();
	TUNABLE_INT_FETCH("vm.pmap.pti", &pti);
	TUNABLE_INT_FETCH("vm.pmap.pcid_enabled", &pmap_pcid_enabled);
	if ((cpu_feature2 & CPUID2_PCID) != 0 && pmap_pcid_enabled) {
		invpcid_works = (cpu_stdext_feature &
		    CPUID_STDEXT_INVPCID) != 0;
	} else {
		pmap_pcid_enabled = 0;
	}

	/*
	 * Now we can do small core initialization, after the PCID
	 * CPU features and user knobs are evaluated.
	 */
	TUNABLE_INT_FETCH("vm.pmap.pcid_invlpg_workaround",
	    &pmap_pcid_invlpg_workaround_uena);
	cpu_init_small_core();

	if ((cpu_feature2 & CPUID2_XSAVE) != 0) {
		use_xsave = 1;
		TUNABLE_INT_FETCH("hw.use_xsave", &use_xsave);
	}

	link_elf_ireloc(kmdp);

	/*
	 * This may be done better later if it gets more high level
	 * components in it. If so just link td->td_proc here.
	 */
	proc_linkup0(&proc0, &thread0);

	/* Init basic tunables, hz etc */
	init_param1();

	thread0.td_kstack = physfree - kernphys + KERNSTART;
	thread0.td_kstack_pages = kstack_pages;
	kstack0_sz = thread0.td_kstack_pages * PAGE_SIZE;
	bzero((void *)thread0.td_kstack, kstack0_sz);
	physfree += kstack0_sz;

	/*
	 * Initialize enough of thread0 for delayed invalidation to
	 * work very early.  Rely on thread0.td_base_pri
	 * zero-initialization, it is reset to PVM at proc0_init().
	 */
	pmap_thread_init_invl_gen(&thread0);

	pc = &temp_bsp_pcpu;
	pcpu_init(pc, 0, sizeof(struct pcpu));
	gdt = &temp_bsp_pcpu.pc_gdt[0];

	/*
	 * make gdt memory segments
	 */
	for (x = 0; x < NGDT; x++) {
		if (x != GPROC0_SEL && x != (GPROC0_SEL + 1) &&
		    x != GUSERLDT_SEL && x != (GUSERLDT_SEL) + 1)
			ssdtosd(&gdt_segs[x], &gdt[x]);
	}
	gdt_segs[GPROC0_SEL].ssd_base = (uintptr_t)&pc->pc_common_tss;
	ssdtosyssd(&gdt_segs[GPROC0_SEL],
	    (struct system_segment_descriptor *)&gdt[GPROC0_SEL]);

	r_gdt.rd_limit = NGDT * sizeof(gdt[0]) - 1;
	r_gdt.rd_base = (long)gdt;
	lgdt(&r_gdt);

	wrmsr(MSR_FSBASE, 0);		/* User value */
	wrmsr(MSR_GSBASE, (u_int64_t)pc);
	wrmsr(MSR_KGSBASE, 0);		/* User value while in the kernel */

	dpcpu_init((void *)(physfree - kernphys + KERNSTART), 0);
	physfree += DPCPU_SIZE;
	amd64_bsp_pcpu_init1(pc);
	/* Non-late cninit() and printf() can be moved up to here. */

	/*
	 * Initialize mutexes.
	 *
	 * icu_lock: in order to allow an interrupt to occur in a critical
	 * 	     section, to set pcpu->ipending (etc...) properly, we
	 *	     must be able to get the icu lock, so it can't be
	 *	     under witness.
	 */
	mutex_init();
	mtx_init(&icu_lock, "icu", NULL, MTX_SPIN | MTX_NOWITNESS);
	mtx_init(&dt_lock, "descriptor tables", NULL, MTX_DEF);

	/* exceptions */
	for (x = 0; x < NIDT; x++)
		setidt(x, pti ? &IDTVEC(rsvd_pti) : &IDTVEC(rsvd), SDT_SYSIGT,
		    SEL_KPL, 0);
	setidt(IDT_DE, pti ? &IDTVEC(div_pti) : &IDTVEC(div), SDT_SYSIGT,
	    SEL_KPL, 0);
	setidt(IDT_DB, &IDTVEC(dbg), SDT_SYSIGT, SEL_KPL, 4);
	setidt(IDT_NMI, &IDTVEC(nmi),  SDT_SYSIGT, SEL_KPL, 2);
	setidt(IDT_BP, pti ? &IDTVEC(bpt_pti) : &IDTVEC(bpt), SDT_SYSIGT,
	    SEL_UPL, 0);
	setidt(IDT_OF, pti ? &IDTVEC(ofl_pti) : &IDTVEC(ofl), SDT_SYSIGT,
	    SEL_UPL, 0);
	setidt(IDT_BR, pti ? &IDTVEC(bnd_pti) : &IDTVEC(bnd), SDT_SYSIGT,
	    SEL_KPL, 0);
	setidt(IDT_UD, pti ? &IDTVEC(ill_pti) : &IDTVEC(ill), SDT_SYSIGT,
	    SEL_KPL, 0);
	setidt(IDT_NM, pti ? &IDTVEC(dna_pti) : &IDTVEC(dna), SDT_SYSIGT,
	    SEL_KPL, 0);
	setidt(IDT_DF, &IDTVEC(dblfault), SDT_SYSIGT, SEL_KPL, 1);
	setidt(IDT_FPUGP, pti ? &IDTVEC(fpusegm_pti) : &IDTVEC(fpusegm),
	    SDT_SYSIGT, SEL_KPL, 0);
	setidt(IDT_TS, pti ? &IDTVEC(tss_pti) : &IDTVEC(tss), SDT_SYSIGT,
	    SEL_KPL, 0);
	setidt(IDT_NP, pti ? &IDTVEC(missing_pti) : &IDTVEC(missing),
	    SDT_SYSIGT, SEL_KPL, 0);
	setidt(IDT_SS, pti ? &IDTVEC(stk_pti) : &IDTVEC(stk), SDT_SYSIGT,
	    SEL_KPL, 0);
	setidt(IDT_GP, pti ? &IDTVEC(prot_pti) : &IDTVEC(prot), SDT_SYSIGT,
	    SEL_KPL, 0);
	setidt(IDT_PF, pti ? &IDTVEC(page_pti) : &IDTVEC(page), SDT_SYSIGT,
	    SEL_KPL, 0);
	setidt(IDT_MF, pti ? &IDTVEC(fpu_pti) : &IDTVEC(fpu), SDT_SYSIGT,
	    SEL_KPL, 0);
	setidt(IDT_AC, pti ? &IDTVEC(align_pti) : &IDTVEC(align), SDT_SYSIGT,
	    SEL_KPL, 0);
	setidt(IDT_MC, &IDTVEC(mchk), SDT_SYSIGT, SEL_KPL, 3);
	setidt(IDT_XF, pti ? &IDTVEC(xmm_pti) : &IDTVEC(xmm), SDT_SYSIGT,
	    SEL_KPL, 0);
#ifdef KDTRACE_HOOKS
	setidt(IDT_DTRACE_RET, pti ? &IDTVEC(dtrace_ret_pti) :
	    &IDTVEC(dtrace_ret), SDT_SYSIGT, SEL_UPL, 0);
#endif
#ifdef XENHVM
	setidt(IDT_EVTCHN, pti ? &IDTVEC(xen_intr_upcall_pti) :
	    &IDTVEC(xen_intr_upcall), SDT_SYSIGT, SEL_KPL, 0);
#endif
	r_idt.rd_limit = sizeof(idt0) - 1;
	r_idt.rd_base = (long) idt;
	lidt(&r_idt);

	/*
	 * Use vt(4) by default for UEFI boot (during the sc(4)/vt(4)
	 * transition).
	 * Once bootblocks have updated, we can test directly for
	 * efi_systbl != NULL here...
	 */
	if (efi_boot)
		vty_set_preferred(VTY_VT);

	TUNABLE_INT_FETCH("hw.ibrs_disable", &hw_ibrs_disable);
	TUNABLE_INT_FETCH("machdep.mitigations.ibrs.disable", &hw_ibrs_disable);

	TUNABLE_INT_FETCH("hw.spec_store_bypass_disable", &hw_ssb_disable);
	TUNABLE_INT_FETCH("machdep.mitigations.ssb.disable", &hw_ssb_disable);

	TUNABLE_INT_FETCH("machdep.syscall_ret_flush_l1d",
	    &syscall_ret_l1d_flush_mode);

	TUNABLE_INT_FETCH("hw.mds_disable", &hw_mds_disable);
	TUNABLE_INT_FETCH("machdep.mitigations.mds.disable", &hw_mds_disable);

	TUNABLE_INT_FETCH("machdep.mitigations.taa.enable", &x86_taa_enable);

	TUNABLE_INT_FETCH("machdep.mitigations.rngds.enable",
	    &x86_rngds_mitg_enable);

	TUNABLE_INT_FETCH("machdep.mitigations.zenbleed.enable",
	    &zenbleed_enable);
	zenbleed_sanitize_enable();

	finishidentcpu();	/* Final stage of CPU initialization */

	/*
	 * Initialize the clock before the console so that console
	 * initialization can use DELAY().
	 */
	clock_init();

	initializecpu();	/* Initialize CPU registers */

	amd64_bsp_ist_init(pc);

	/* Set the IO permission bitmap (empty due to tss seg limit) */
	pc->pc_common_tss.tss_iobase = sizeof(struct amd64tss) +
	    IOPERM_BITMAP_SIZE;

	gsel_tss = GSEL(GPROC0_SEL, SEL_KPL);
	ltr(gsel_tss);

	amd64_conf_fast_syscall();

	/*
	 * We initialize the PCB pointer early so that exception
	 * handlers will work.  Also set up td_critnest to short-cut
	 * the page fault handler.
	 */
	cpu_max_ext_state_size = sizeof(struct savefpu);
	set_top_of_stack_td(&thread0);
	thread0.td_pcb = get_pcb_td(&thread0);
	thread0.td_critnest = 1;

	/*
	 * The console and kdb should be initialized even earlier than here,
	 * but some console drivers don't work until after getmemsize().
	 * Default to late console initialization to support these drivers.
	 * This loses mainly printf()s in getmemsize() and early debugging.
	 */
	TUNABLE_INT_FETCH("debug.late_console", &late_console);
	if (!late_console) {
		cninit();
		amd64_kdb_init();
	}

	getmemsize(kmdp, physfree);
	init_param2(physmem);

	/* now running on new page tables, configured,and u/iom is accessible */

#ifdef DEV_PCI
        /* This call might adjust phys_avail[]. */
        pci_early_quirks();
#endif

	if (late_console)
		cninit();

	/*
	 * Dump the boot metadata. We have to wait for cninit() since console
	 * output is required. If it's grossly incorrect the kernel will never
	 * make it this far.
	 */
	if (getenv_is_true("debug.dump_modinfo_at_boot"))
		preload_dump();

#ifdef DEV_ISA
#ifdef DEV_ATPIC
	elcr_probe();
	atpic_startup();
#else
	/* Reset and mask the atpics and leave them shut down. */
	atpic_reset();

	/*
	 * Point the ICU spurious interrupt vectors at the APIC spurious
	 * interrupt handler.
	 */
	setidt(IDT_IO_INTS + 7, IDTVEC(spuriousint), SDT_SYSIGT, SEL_KPL, 0);
	setidt(IDT_IO_INTS + 15, IDTVEC(spuriousint), SDT_SYSIGT, SEL_KPL, 0);
#endif
#else
#error "have you forgotten the isa device?"
#endif

	if (late_console)
		amd64_kdb_init();

	msgbufinit(msgbufp, msgbufsize);
	fpuinit();

	/* make an initial tss so cpu can get interrupt stack on syscall! */
	rsp0 = thread0.td_md.md_stack_base;
	/* Ensure the stack is aligned to 16 bytes */
	rsp0 &= ~0xFul;
	PCPU_PTR(common_tss)->tss_rsp0 = rsp0;
	amd64_bsp_pcpu_init2(rsp0);

	/* transfer to user mode */

	_ucodesel = GSEL(GUCODE_SEL, SEL_UPL);
	_udatasel = GSEL(GUDATA_SEL, SEL_UPL);
	_ucode32sel = GSEL(GUCODE32_SEL, SEL_UPL);
	_ufssel = GSEL(GUFS32_SEL, SEL_UPL);
	_ugssel = GSEL(GUGS32_SEL, SEL_UPL);

	load_ds(_udatasel);
	load_es(_udatasel);
	load_fs(_ufssel);

	/* setup proc 0's pcb */
	thread0.td_pcb->pcb_flags = 0;

        env = kern_getenv("kernelname");
	if (env != NULL)
		strlcpy(kernelname, env, sizeof(kernelname));

	kcsan_cpu_init(0);

#ifdef FDT
	x86_init_fdt();
#endif
	thread0.td_critnest = 0;

	kasan_init();
	kmsan_init();

	TSEXIT();

	/* Location of kernel stack for locore */
	return (thread0.td_md.md_stack_base);
}

void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t size)
{

	pcpu->pc_acpi_id = 0xffffffff;
}

static int
smap_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	struct bios_smap *smapbase;
	struct bios_smap_xattr smap;
	caddr_t kmdp;
	uint32_t *smapattr;
	int count, error, i;

	/* Retrieve the system memory map from the loader. */
	kmdp = preload_search_by_type("elf kernel");
	if (kmdp == NULL)
		kmdp = preload_search_by_type("elf64 kernel");
	smapbase = (struct bios_smap *)preload_search_info(kmdp,
	    MODINFO_METADATA | MODINFOMD_SMAP);
	if (smapbase == NULL)
		return (0);
	smapattr = (uint32_t *)preload_search_info(kmdp,
	    MODINFO_METADATA | MODINFOMD_SMAP_XATTR);
	count = *((uint32_t *)smapbase - 1) / sizeof(*smapbase);
	error = 0;
	for (i = 0; i < count; i++) {
		smap.base = smapbase[i].base;
		smap.length = smapbase[i].length;
		smap.type = smapbase[i].type;
		if (smapattr != NULL)
			smap.xattr = smapattr[i];
		else
			smap.xattr = 0;
		error = SYSCTL_OUT(req, &smap, sizeof(smap));
	}
	return (error);
}
SYSCTL_PROC(_machdep, OID_AUTO, smap,
    CTLTYPE_OPAQUE | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    smap_sysctl_handler, "S,bios_smap_xattr",
    "Raw BIOS SMAP data");

static int
efi_map_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	struct efi_map_header *efihdr;
	caddr_t kmdp;
	uint32_t efisize;

	kmdp = preload_search_by_type("elf kernel");
	if (kmdp == NULL)
		kmdp = preload_search_by_type("elf64 kernel");
	efihdr = (struct efi_map_header *)preload_search_info(kmdp,
	    MODINFO_METADATA | MODINFOMD_EFI_MAP);
	if (efihdr == NULL)
		return (0);
	efisize = *((uint32_t *)efihdr - 1);
	return (SYSCTL_OUT(req, efihdr, efisize));
}
SYSCTL_PROC(_machdep, OID_AUTO, efi_map,
    CTLTYPE_OPAQUE | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    efi_map_sysctl_handler, "S,efi_map_header",
    "Raw EFI Memory Map");

void
spinlock_enter(void)
{
	struct thread *td;
	register_t flags;

	td = curthread;
	if (td->td_md.md_spinlock_count == 0) {
		flags = intr_disable();
		td->td_md.md_spinlock_count = 1;
		td->td_md.md_saved_flags = flags;
		critical_enter();
	} else
		td->td_md.md_spinlock_count++;
}

void
spinlock_exit(void)
{
	struct thread *td;
	register_t flags;

	td = curthread;
	flags = td->td_md.md_saved_flags;
	td->td_md.md_spinlock_count--;
	if (td->td_md.md_spinlock_count == 0) {
		critical_exit();
		intr_restore(flags);
	}
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

	pcb->pcb_r12 = tf->tf_r12;
	pcb->pcb_r13 = tf->tf_r13;
	pcb->pcb_r14 = tf->tf_r14;
	pcb->pcb_r15 = tf->tf_r15;
	pcb->pcb_rbp = tf->tf_rbp;
	pcb->pcb_rbx = tf->tf_rbx;
	pcb->pcb_rip = tf->tf_rip;
	pcb->pcb_rsp = tf->tf_rsp;
}

/*
 * The pcb_flags is only modified by current thread, or by other threads
 * when current thread is stopped.  However, current thread may change it
 * from the interrupt context in cpu_switch(), or in the trap handler.
 * When we read-modify-write pcb_flags from C sources, compiler may generate
 * code that is not atomic regarding the interrupt handler.  If a trap or
 * interrupt happens and any flag is modified from the handler, it can be
 * clobbered with the cached value later.  Therefore, we implement setting
 * and clearing flags with single-instruction functions, which do not race
 * with possible modification of the flags from the trap or interrupt context,
 * because traps and interrupts are executed only on instruction boundary.
 */
void
set_pcb_flags_raw(struct pcb *pcb, const u_int flags)
{

	__asm __volatile("orl %1,%0"
	    : "=m" (pcb->pcb_flags) : "ir" (flags), "m" (pcb->pcb_flags)
	    : "cc", "memory");

}

/*
 * The support for RDFSBASE, WRFSBASE and similar instructions for %gs
 * base requires that kernel saves MSR_FSBASE and MSR_{K,}GSBASE into
 * pcb if user space modified the bases.  We must save on the context
 * switch or if the return to usermode happens through the doreti.
 *
 * Tracking of both events is performed by the pcb flag PCB_FULL_IRET,
 * which have a consequence that the base MSRs must be saved each time
 * the PCB_FULL_IRET flag is set.  We disable interrupts to sync with
 * context switches.
 */
static void
set_pcb_flags_fsgsbase(struct pcb *pcb, const u_int flags)
{
	register_t r;

	if (curpcb == pcb &&
	    (flags & PCB_FULL_IRET) != 0 &&
	    (pcb->pcb_flags & PCB_FULL_IRET) == 0) {
		r = intr_disable();
		if ((pcb->pcb_flags & PCB_FULL_IRET) == 0) {
			if (rfs() == _ufssel)
				pcb->pcb_fsbase = rdfsbase();
			if (rgs() == _ugssel)
				pcb->pcb_gsbase = rdmsr(MSR_KGSBASE);
		}
		set_pcb_flags_raw(pcb, flags);
		intr_restore(r);
	} else {
		set_pcb_flags_raw(pcb, flags);
	}
}

DEFINE_IFUNC(, void, set_pcb_flags, (struct pcb *, const u_int))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_FSGSBASE) != 0 ?
	    set_pcb_flags_fsgsbase : set_pcb_flags_raw);
}

void
clear_pcb_flags(struct pcb *pcb, const u_int flags)
{

	__asm __volatile("andl %1,%0"
	    : "=m" (pcb->pcb_flags) : "ir" (~flags), "m" (pcb->pcb_flags)
	    : "cc", "memory");
}

#ifdef KDB

/*
 * Provide inb() and outb() as functions.  They are normally only available as
 * inline functions, thus cannot be called from the debugger.
 */

/* silence compiler warnings */
u_char inb_(u_short);
void outb_(u_short, u_char);

u_char
inb_(u_short port)
{
	return inb(port);
}

void
outb_(u_short port, u_char data)
{
	outb(port, data);
}

#endif /* KDB */

#undef memset
#undef memmove
#undef memcpy

void	*memset_std(void *buf, int c, size_t len);
void	*memset_erms(void *buf, int c, size_t len);
void    *memmove_std(void * _Nonnull dst, const void * _Nonnull src,
	    size_t len);
void    *memmove_erms(void * _Nonnull dst, const void * _Nonnull src,
	    size_t len);
void    *memcpy_std(void * _Nonnull dst, const void * _Nonnull src,
	    size_t len);
void    *memcpy_erms(void * _Nonnull dst, const void * _Nonnull src,
	    size_t len);

#ifdef KCSAN
/*
 * These fail to build as ifuncs when used with KCSAN.
 */
void *
memset(void *buf, int c, size_t len)
{

	return (memset_std(buf, c, len));
}

void *
memmove(void * _Nonnull dst, const void * _Nonnull src, size_t len)
{

	return (memmove_std(dst, src, len));
}

void *
memcpy(void * _Nonnull dst, const void * _Nonnull src, size_t len)
{

	return (memcpy_std(dst, src, len));
}
#else
DEFINE_IFUNC(, void *, memset, (void *, int, size_t))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_ERMS) != 0 ?
	    memset_erms : memset_std);
}

DEFINE_IFUNC(, void *, memmove, (void * _Nonnull, const void * _Nonnull,
    size_t))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_ERMS) != 0 ?
	    memmove_erms : memmove_std);
}

DEFINE_IFUNC(, void *, memcpy, (void * _Nonnull, const void * _Nonnull,size_t))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_ERMS) != 0 ?
	    memcpy_erms : memcpy_std);
}
#endif

void	pagezero_std(void *addr);
void	pagezero_erms(void *addr);
DEFINE_IFUNC(, void , pagezero, (void *))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_ERMS) != 0 ?
	    pagezero_erms : pagezero_std);
}
