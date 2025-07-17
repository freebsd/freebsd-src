/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 2018 The FreeBSD Foundation
 * Copyright (c) 1992 Terrence R. Lambert.
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Portions of this software were developed by A. Joseph Koshy under
 * sponsorship from the FreeBSD Foundation and Google, Inc.
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
 */

#include <sys/cdefs.h>
#include "opt_apic.h"
#include "opt_atpic.h"
#include "opt_cpu.h"
#include "opt_ddb.h"
#include "opt_inet.h"
#include "opt_isa.h"
#include "opt_kstack_pages.h"
#include "opt_maxmem.h"
#include "opt_perfmon.h"
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/callout.h>
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
#include <sys/memrange.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/ptrace.h>
#include <sys/reboot.h>
#include <sys/reg.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#include <sys/smp.h>
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

#include <isa/rtc.h>

#include <net/netisr.h>

#include <dev/smbios/smbios.h>

#include <machine/bootinfo.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/cputypes.h>
#include <machine/intr_machdep.h>
#include <x86/mca.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/pc/bios.h>
#include <machine/pcb.h>
#include <machine/pcb_ext.h>
#include <machine/proc.h>
#include <machine/sigframe.h>
#include <machine/specialreg.h>
#include <machine/sysarch.h>
#include <machine/trap.h>
#include <x86/ucode.h>
#include <machine/vm86.h>
#include <x86/init.h>
#ifdef PERFMON
#include <machine/perfmon.h>
#endif
#ifdef SMP
#include <machine/smp.h>
#endif
#ifdef FDT
#include <x86/fdt.h>
#endif

#ifdef DEV_APIC
#include <x86/apicvar.h>
#endif

#ifdef DEV_ISA
#include <x86/isa/icu.h>
#endif

/* Sanity check for __curthread() */
CTASSERT(offsetof(struct pcpu, pc_curthread) == 0);

register_t init386(int first);
void dblfault_handler(void);
void identify_cpu(void);

static void cpu_startup(void *);
SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL);

/* Intel ICH registers */
#define ICH_PMBASE	0x400
#define ICH_SMI_EN	ICH_PMBASE + 0x30

int	_udatasel, _ucodesel;
u_int	basemem;
static int above4g_allow = 1;
static int above24g_allow = 0;

int cold = 1;

long Maxmem = 0;
long realmem = 0;
int late_console = 1;

#ifdef PAE
FEATURE(pae, "Physical Address Extensions");
#endif

struct kva_md_info kmi;

static struct trapframe proc0_tf;
struct pcpu __pcpu[MAXCPU];

static void i386_clock_source_init(void);

struct mtx icu_lock;

struct mem_range_softc mem_range_softc;

extern char start_exceptions[], end_exceptions[];

extern struct sysentvec elf32_freebsd_sysvec;

/* Default init_ops implementation. */
struct init_ops init_ops = {
	.early_clock_source_init =	i386_clock_source_init,
	.early_delay =			i8254_delay,
};

static void
i386_clock_source_init(void)
{
	i8254_init();
}

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
	panicifcpuunsupported();
#ifdef PERFMON
	perfmon_init();
#endif

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

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
	vm_pager_bufferinit();
	cpu_setregs();
}

void
cpu_setregs(void)
{
	unsigned int cr0;

	cr0 = rcr0();

	/*
	 * CR0_MP, CR0_NE and CR0_TS are set for NPX (FPU) support:
	 *
	 * Prepare to trap all ESC (i.e., NPX) instructions and all WAIT
	 * instructions.  We must set the CR0_MP bit and use the CR0_TS
	 * bit to control the trap, because setting the CR0_EM bit does
	 * not cause WAIT instructions to trap.  It's important to trap
	 * WAIT instructions - otherwise the "wait" variants of no-wait
	 * control instructions would degenerate to the "no-wait" variants
	 * after FP context switches but work correctly otherwise.  It's
	 * particularly important to trap WAITs when there is no NPX -
	 * otherwise the "wait" variants would always degenerate.
	 *
	 * Try setting CR0_NE to get correct error reporting on 486DX's.
	 * Setting it should fail or do nothing on lesser processors.
	 */
	cr0 |= CR0_MP | CR0_NE | CR0_TS | CR0_WP | CR0_AM;
	load_cr0(cr0);
	load_gs(_udatasel);
}

u_long bootdev;		/* not a struct cdev *- encoding is different */
SYSCTL_ULONG(_machdep, OID_AUTO, guessed_bootdev,
	CTLFLAG_RD, &bootdev, 0, "Maybe the Boot device (not in struct cdev *format)");

/*
 * Initialize 386 and configure to run kernel
 */

/*
 * Initialize segments & interrupt table
 */

int _default_ldt;

struct mtx dt_lock;			/* lock for GDT and LDT */

union descriptor gdt0[NGDT];	/* initial global descriptor table */
union descriptor *gdt = gdt0;	/* global descriptor table */

union descriptor *ldt;		/* local descriptor table */

static struct gate_descriptor idt0[NIDT];
struct gate_descriptor *idt = &idt0[0];	/* interrupt descriptor table */

static struct i386tss *dblfault_tss;
static char *dblfault_stack;

static struct i386tss common_tss0;

vm_offset_t proc0kstack;

/*
 * software prototypes -- in more palatable form.
 *
 * GCODE_SEL through GUDATA_SEL must be in this order for syscall/sysret
 * GUFS_SEL and GUGS_SEL must be in this order (swtch.s knows it)
 */
struct soft_segment_descriptor gdt_segs[] = {
/* GNULL_SEL	0 Null Descriptor */
{	.ssd_base = 0x0,
	.ssd_limit = 0x0,
	.ssd_type = 0,
	.ssd_dpl = SEL_KPL,
	.ssd_p = 0,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
/* GPRIV_SEL	1 SMP Per-Processor Private Data Descriptor */
{	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = SEL_KPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
/* GUFS_SEL	2 %fs Descriptor for user */
{	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = SEL_UPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
/* GUGS_SEL	3 %gs Descriptor for user */
{	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = SEL_UPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
/* GCODE_SEL	4 Code Descriptor for kernel */
{	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMERA,
	.ssd_dpl = SEL_KPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
/* GDATA_SEL	5 Data Descriptor for kernel */
{	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = SEL_KPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
/* GUCODE_SEL	6 Code Descriptor for user */
{	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMERA,
	.ssd_dpl = SEL_UPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
/* GUDATA_SEL	7 Data Descriptor for user */
{	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = SEL_UPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
/* GBIOSLOWMEM_SEL 8 BIOS access to realmode segment 0x40, must be #8 in GDT */
{	.ssd_base = 0x400,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = SEL_KPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
/* GPROC0_SEL	9 Proc 0 Tss Descriptor */
{
	.ssd_base = 0x0,
	.ssd_limit = sizeof(struct i386tss)-1,
	.ssd_type = SDT_SYS386TSS,
	.ssd_dpl = 0,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
/* GLDT_SEL	10 LDT Descriptor */
{	.ssd_base = 0,
	.ssd_limit = sizeof(union descriptor) * NLDT - 1,
	.ssd_type = SDT_SYSLDT,
	.ssd_dpl = SEL_UPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
/* GUSERLDT_SEL	11 User LDT Descriptor per process */
{	.ssd_base = 0,
	.ssd_limit = (512 * sizeof(union descriptor)-1),
	.ssd_type = SDT_SYSLDT,
	.ssd_dpl = 0,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
/* GPANIC_SEL	12 Panic Tss Descriptor */
{	.ssd_base = 0,
	.ssd_limit = sizeof(struct i386tss)-1,
	.ssd_type = SDT_SYS386TSS,
	.ssd_dpl = 0,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
/* GBIOSCODE32_SEL 13 BIOS 32-bit interface (32bit Code) */
{	.ssd_base = 0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMERA,
	.ssd_dpl = 0,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 1		},
/* GBIOSCODE16_SEL 14 BIOS 32-bit interface (16bit Code) */
{	.ssd_base = 0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMERA,
	.ssd_dpl = 0,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 1		},
/* GBIOSDATA_SEL 15 BIOS 32-bit interface (Data) */
{	.ssd_base = 0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = 0,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
/* GBIOSUTIL_SEL 16 BIOS 16-bit interface (Utility) */
{	.ssd_base = 0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = 0,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 1		},
/* GBIOSARGS_SEL 17 BIOS 16-bit interface (Arguments) */
{	.ssd_base = 0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = 0,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 1		},
/* GNDIS_SEL	18 NDIS Descriptor */
{	.ssd_base = 0x0,
	.ssd_limit = 0x0,
	.ssd_type = 0,
	.ssd_dpl = 0,
	.ssd_p = 0,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
};

static struct soft_segment_descriptor ldt_segs[] = {
	/* Null Descriptor - overwritten by call gate */
{	.ssd_base = 0x0,
	.ssd_limit = 0x0,
	.ssd_type = 0,
	.ssd_dpl = 0,
	.ssd_p = 0,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
	/* Null Descriptor - overwritten by call gate */
{	.ssd_base = 0x0,
	.ssd_limit = 0x0,
	.ssd_type = 0,
	.ssd_dpl = 0,
	.ssd_p = 0,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
	/* Null Descriptor - overwritten by call gate */
{	.ssd_base = 0x0,
	.ssd_limit = 0x0,
	.ssd_type = 0,
	.ssd_dpl = 0,
	.ssd_p = 0,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
	/* Code Descriptor for user */
{	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMERA,
	.ssd_dpl = SEL_UPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
	/* Null Descriptor - overwritten by call gate */
{	.ssd_base = 0x0,
	.ssd_limit = 0x0,
	.ssd_type = 0,
	.ssd_dpl = 0,
	.ssd_p = 0,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 0,
	.ssd_gran = 0		},
	/* Data Descriptor for user */
{	.ssd_base = 0x0,
	.ssd_limit = 0xfffff,
	.ssd_type = SDT_MEMRWA,
	.ssd_dpl = SEL_UPL,
	.ssd_p = 1,
	.ssd_xx = 0, .ssd_xx1 = 0,
	.ssd_def32 = 1,
	.ssd_gran = 1		},
};

size_t setidt_disp;

void
setidt(int idx, inthand_t *func, int typ, int dpl, int selec)
{
	uintptr_t off;

	off = func != NULL ? (uintptr_t)func + setidt_disp : 0;
	setidt_nodisp(idx, off, typ, dpl, selec);
}

void
setidt_nodisp(int idx, uintptr_t off, int typ, int dpl, int selec)
{
	struct gate_descriptor *ip;

	ip = idt + idx;
	ip->gd_looffset = off;
	ip->gd_selector = selec;
	ip->gd_stkcpy = 0;
	ip->gd_xx = 0;
	ip->gd_type = typ;
	ip->gd_dpl = dpl;
	ip->gd_p = 1;
	ip->gd_hioffset = ((u_int)off) >> 16 ;
}

extern inthand_t
	IDTVEC(div), IDTVEC(dbg), IDTVEC(nmi), IDTVEC(bpt), IDTVEC(ofl),
	IDTVEC(bnd), IDTVEC(ill), IDTVEC(dna), IDTVEC(fpusegm),
	IDTVEC(tss), IDTVEC(missing), IDTVEC(stk), IDTVEC(prot),
	IDTVEC(page), IDTVEC(mchk), IDTVEC(rsvd), IDTVEC(fpu), IDTVEC(align),
	IDTVEC(xmm),
#ifdef KDTRACE_HOOKS
	IDTVEC(dtrace_ret),
#endif
#ifdef XENHVM
	IDTVEC(xen_intr_upcall),
#endif
	IDTVEC(int0x80_syscall);

#ifdef DDB
/*
 * Display the index and function name of any IDT entries that don't use
 * the default 'rsvd' entry point.
 */
DB_SHOW_COMMAND_FLAGS(idt, db_show_idt, DB_CMD_MEMSAFE)
{
	struct gate_descriptor *ip;
	int idx;
	uintptr_t func, func_trm;
	bool trm;

	ip = idt;
	for (idx = 0; idx < NIDT && !db_pager_quit; idx++) {
		if (ip->gd_type == SDT_SYSTASKGT) {
			db_printf("%3d\t<TASK>\n", idx);
		} else {
			func = (ip->gd_hioffset << 16 | ip->gd_looffset);
			if (func >= PMAP_TRM_MIN_ADDRESS) {
				func_trm = func;
				func -= setidt_disp;
				trm = true;
			} else
				trm = false;
			if (func != (uintptr_t)&IDTVEC(rsvd)) {
				db_printf("%3d\t", idx);
				db_printsym(func, DB_STGY_PROC);
				if (trm)
					db_printf(" (trampoline %#x)",
					    func_trm);
				db_printf("\n");
			}
		}
		ip++;
	}
}

/* Show privileged registers. */
DB_SHOW_COMMAND_FLAGS(sysregs, db_show_sysregs, DB_CMD_MEMSAFE)
{
	uint64_t idtr, gdtr;

	idtr = ridt();
	db_printf("idtr\t0x%08x/%04x\n",
	    (u_int)(idtr >> 16), (u_int)idtr & 0xffff);
	gdtr = rgdt();
	db_printf("gdtr\t0x%08x/%04x\n",
	    (u_int)(gdtr >> 16), (u_int)gdtr & 0xffff);
	db_printf("ldtr\t0x%04x\n", rldt());
	db_printf("tr\t0x%04x\n", rtr());
	db_printf("cr0\t0x%08x\n", rcr0());
	db_printf("cr2\t0x%08x\n", rcr2());
	db_printf("cr3\t0x%08x\n", rcr3());
	db_printf("cr4\t0x%08x\n", rcr4());
	if (rcr4() & CR4_XSAVE)
		db_printf("xcr0\t0x%016llx\n", rxcr(0));
	if (amd_feature & (AMDID_NX | AMDID_LM))
		db_printf("EFER\t0x%016llx\n", rdmsr(MSR_EFER));
	if (cpu_feature2 & (CPUID2_VMX | CPUID2_SMX))
		db_printf("FEATURES_CTL\t0x%016llx\n",
		    rdmsr(MSR_IA32_FEATURE_CONTROL));
	if (((cpu_vendor_id == CPU_VENDOR_INTEL ||
	    cpu_vendor_id == CPU_VENDOR_AMD) && CPUID_TO_FAMILY(cpu_id) >= 6) ||
	    cpu_vendor_id == CPU_VENDOR_HYGON)
		db_printf("DEBUG_CTL\t0x%016llx\n", rdmsr(MSR_DEBUGCTLMSR));
	if (cpu_feature & CPUID_PAT)
		db_printf("PAT\t0x%016llx\n", rdmsr(MSR_PAT));
}

DB_SHOW_COMMAND_FLAGS(dbregs, db_show_dbregs, DB_CMD_MEMSAFE)
{

	db_printf("dr0\t0x%08x\n", rdr0());
	db_printf("dr1\t0x%08x\n", rdr1());
	db_printf("dr2\t0x%08x\n", rdr2());
	db_printf("dr3\t0x%08x\n", rdr3());
	db_printf("dr6\t0x%08x\n", rdr6());
	db_printf("dr7\t0x%08x\n", rdr7());
}

DB_SHOW_COMMAND(frame, db_show_frame)
{
	struct trapframe *frame;

	frame = have_addr ? (struct trapframe *)addr : curthread->td_frame;
	printf("ss %#x esp %#x efl %#x cs %#x eip %#x\n",
	    frame->tf_ss, frame->tf_esp, frame->tf_eflags, frame->tf_cs,
	    frame->tf_eip);
	printf("err %#x trapno %d\n", frame->tf_err, frame->tf_trapno);
	printf("ds %#x es %#x fs %#x\n",
	    frame->tf_ds, frame->tf_es, frame->tf_fs);
	printf("eax %#x ecx %#x edx %#x ebx %#x\n",
	    frame->tf_eax, frame->tf_ecx, frame->tf_edx, frame->tf_ebx);
	printf("ebp %#x esi %#x edi %#x\n",
	    frame->tf_ebp, frame->tf_esi, frame->tf_edi);

}
#endif

void
sdtossd(struct segment_descriptor *sd, struct soft_segment_descriptor *ssd)
{
	ssd->ssd_base  = (sd->sd_hibase << 24) | sd->sd_lobase;
	ssd->ssd_limit = (sd->sd_hilimit << 16) | sd->sd_lolimit;
	ssd->ssd_type  = sd->sd_type;
	ssd->ssd_dpl   = sd->sd_dpl;
	ssd->ssd_p     = sd->sd_p;
	ssd->ssd_def32 = sd->sd_def32;
	ssd->ssd_gran  = sd->sd_gran;
}

static int
add_physmap_entry(uint64_t base, uint64_t length, vm_paddr_t *physmap,
    int *physmap_idxp)
{
	uint64_t lim, ign;
	int i, insert_idx, physmap_idx;

	physmap_idx = *physmap_idxp;

	if (length == 0)
		return (1);

	lim = 0x100000000;					/*  4G */
	if (pae_mode && above4g_allow)
		lim = above24g_allow ? -1ULL : 0x600000000;	/* 24G */
	if (base >= lim) {
		printf("%uK of memory above %uGB ignored, pae %d "
		    "above4g_allow %d above24g_allow %d\n",
		    (u_int)(length / 1024), (u_int)(lim >> 30), pae_mode,
		    above4g_allow, above24g_allow);
		return (1);
	}
	if (base + length >= lim) {
		ign = base + length - lim;
		length -= ign;
		printf("%uK of memory above %uGB ignored, pae %d "
		    "above4g_allow %d above24g_allow %d\n",
		    (u_int)(ign / 1024), (u_int)(lim >> 30), pae_mode,
		    above4g_allow, above24g_allow);
	}

	/*
	 * Find insertion point while checking for overlap.  Start off by
	 * assuming the new entry will be added to the end.
	 */
	insert_idx = physmap_idx + 2;
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
	for (i = physmap_idx; i > insert_idx; i -= 2) {
		physmap[i] = physmap[i - 2];
		physmap[i + 1] = physmap[i - 1];
	}

	/* Insert the new entry. */
	physmap[insert_idx] = base;
	physmap[insert_idx + 1] = base + length;
	return (1);
}

static int
add_smap_entry(struct bios_smap *smap, vm_paddr_t *physmap, int *physmap_idxp)
{
	if (boothowto & RB_VERBOSE)
		printf("SMAP type=%02x base=%016llx len=%016llx\n",
		    smap->type, smap->base, smap->length);

	if (smap->type != SMAP_TYPE_MEMORY)
		return (1);

	return (add_physmap_entry(smap->base, smap->length, physmap,
	    physmap_idxp));
}

static void
add_smap_entries(struct bios_smap *smapbase, vm_paddr_t *physmap,
    int *physmap_idxp)
{
	struct bios_smap *smap, *smapend;
	u_int32_t smapsize;
	/*
	 * Memory map from INT 15:E820.
	 *
	 * subr_module.c says:
	 * "Consumer may safely assume that size value precedes data."
	 * ie: an int32_t immediately precedes SMAP.
	 */
	smapsize = *((u_int32_t *)smapbase - 1);
	smapend = (struct bios_smap *)((uintptr_t)smapbase + smapsize);

	for (smap = smapbase; smap < smapend; smap++)
		if (!add_smap_entry(smap, physmap, physmap_idxp))
			break;
}

static void
basemem_setup(void)
{

	if (basemem > 640) {
		printf("Preposterous BIOS basemem of %uK, truncating to 640K\n",
			basemem);
		basemem = 640;
	}

	pmap_basemem_setup(basemem);
}

/*
 * Populate the (physmap) array with base/bound pairs describing the
 * available physical memory in the system, then test this memory and
 * build the phys_avail array describing the actually-available memory.
 *
 * If we cannot accurately determine the physical memory map, then use
 * value from the 0xE801 call, and failing that, the RTC.
 *
 * Total memory size may be set by the kernel environment variable
 * hw.physmem or the compile-time define MAXMEM.
 *
 * XXX first should be vm_paddr_t.
 */
static void
getmemsize(int first)
{
	int has_smap, off, physmap_idx, pa_indx, da_indx;
	u_long memtest;
	vm_paddr_t physmap[PHYS_AVAIL_ENTRIES];
	quad_t dcons_addr, dcons_size, physmem_tunable;
	int hasbrokenint12, i, res __diagused;
	u_int extmem;
	struct vm86frame vmf;
	struct vm86context vmc;
	vm_paddr_t pa;
	struct bios_smap *smap, *smapbase;

	has_smap = 0;
	bzero(&vmf, sizeof(vmf));
	bzero(physmap, sizeof(physmap));
	basemem = 0;

	/*
	 * Tell the physical memory allocator about pages used to store
	 * the kernel and preloaded data.  See kmem_bootstrap_free().
	 */
	vm_phys_early_add_seg((vm_paddr_t)KERNLOAD, trunc_page(first));

	TUNABLE_INT_FETCH("hw.above4g_allow", &above4g_allow);
	TUNABLE_INT_FETCH("hw.above24g_allow", &above24g_allow);

	/*
	 * Check if the loader supplied an SMAP memory map.  If so,
	 * use that and do not make any VM86 calls.
	 */
	physmap_idx = 0;
	smapbase = (struct bios_smap *)preload_search_info(preload_kmdp,
	    MODINFO_METADATA | MODINFOMD_SMAP);
	if (smapbase != NULL) {
		add_smap_entries(smapbase, physmap, &physmap_idx);
		has_smap = 1;
		goto have_smap;
	}

	/*
	 * Some newer BIOSes have a broken INT 12H implementation
	 * which causes a kernel panic immediately.  In this case, we
	 * need use the SMAP to determine the base memory size.
	 */
	hasbrokenint12 = 0;
	TUNABLE_INT_FETCH("hw.hasbrokenint12", &hasbrokenint12);
	if (hasbrokenint12 == 0) {
		/* Use INT12 to determine base memory size. */
		vm86_intcall(0x12, &vmf);
		basemem = vmf.vmf_ax;
		basemem_setup();
	}

	/*
	 * Fetch the memory map with INT 15:E820.  Map page 1 R/W into
	 * the kernel page table so we can use it as a buffer.  The
	 * kernel will unmap this page later.
	 */
	vmc.npages = 0;
	smap = (void *)vm86_addpage(&vmc, 1, PMAP_MAP_LOW + ptoa(1));
	res = vm86_getptr(&vmc, (vm_offset_t)smap, &vmf.vmf_es, &vmf.vmf_di);
	KASSERT(res != 0, ("vm86_getptr() failed: address not found"));

	vmf.vmf_ebx = 0;
	do {
		vmf.vmf_eax = 0xE820;
		vmf.vmf_edx = SMAP_SIG;
		vmf.vmf_ecx = sizeof(struct bios_smap);
		i = vm86_datacall(0x15, &vmf, &vmc);
		if (i || vmf.vmf_eax != SMAP_SIG)
			break;
		has_smap = 1;
		if (!add_smap_entry(smap, physmap, &physmap_idx))
			break;
	} while (vmf.vmf_ebx != 0);

have_smap:
	/*
	 * If we didn't fetch the "base memory" size from INT12,
	 * figure it out from the SMAP (or just guess).
	 */
	if (basemem == 0) {
		for (i = 0; i <= physmap_idx; i += 2) {
			if (physmap[i] == 0x00000000) {
				basemem = physmap[i + 1] / 1024;
				break;
			}
		}

		/* XXX: If we couldn't find basemem from SMAP, just guess. */
		if (basemem == 0)
			basemem = 640;
		basemem_setup();
	}

	if (physmap[1] != 0)
		goto physmap_done;

	/*
	 * If we failed to find an SMAP, figure out the extended
	 * memory size.  We will then build a simple memory map with
	 * two segments, one for "base memory" and the second for
	 * "extended memory".  Note that "extended memory" starts at a
	 * physical address of 1MB and that both basemem and extmem
	 * are in units of 1KB.
	 *
	 * First, try to fetch the extended memory size via INT 15:E801.
	 */
	vmf.vmf_ax = 0xE801;
	if (vm86_intcall(0x15, &vmf) == 0) {
		extmem = vmf.vmf_cx + vmf.vmf_dx * 64;
	} else {
		/*
		 * If INT15:E801 fails, this is our last ditch effort
		 * to determine the extended memory size.  Currently
		 * we prefer the RTC value over INT15:88.
		 */
#if 0
		vmf.vmf_ah = 0x88;
		vm86_intcall(0x15, &vmf);
		extmem = vmf.vmf_ax;
#else
		extmem = rtcin(RTC_EXTLO) + (rtcin(RTC_EXTHI) << 8);
#endif
	}

	/*
	 * Special hack for chipsets that still remap the 384k hole when
	 * there's 16MB of memory - this really confuses people that
	 * are trying to use bus mastering ISA controllers with the
	 * "16MB limit"; they only have 16MB, but the remapping puts
	 * them beyond the limit.
	 *
	 * If extended memory is between 15-16MB (16-17MB phys address range),
	 *	chop it to 15MB.
	 */
	if ((extmem > 15 * 1024) && (extmem < 16 * 1024))
		extmem = 15 * 1024;

	physmap[0] = 0;
	physmap[1] = basemem * 1024;
	physmap_idx = 2;
	physmap[physmap_idx] = 0x100000;
	physmap[physmap_idx + 1] = physmap[physmap_idx] + extmem * 1024;

physmap_done:
	/*
	 * Now, physmap contains a map of physical memory.
	 */

#ifdef SMP
	/* make hole for AP bootstrap code */
	alloc_ap_trampoline(physmap, &physmap_idx);
#endif

	/*
	 * Maxmem isn't the "maximum memory", it's one larger than the
	 * highest page of the physical address space.  It should be
	 * called something like "Maxphyspage".  We may adjust this 
	 * based on ``hw.physmem'' and the results of the memory test.
	 *
	 * This is especially confusing when it is much larger than the
	 * memory size and is displayed as "realmem".
	 */
	Maxmem = atop(physmap[physmap_idx + 1]);

#ifdef MAXMEM
	Maxmem = MAXMEM / 4;
#endif

	if (TUNABLE_QUAD_FETCH("hw.physmem", &physmem_tunable))
		Maxmem = atop(physmem_tunable);

	/*
	 * If we have an SMAP, don't allow MAXMEM or hw.physmem to extend
	 * the amount of memory in the system.
	 */
	if (has_smap && Maxmem > atop(physmap[physmap_idx + 1]))
		Maxmem = atop(physmap[physmap_idx + 1]);

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

	if (atop(physmap[physmap_idx + 1]) != Maxmem &&
	    (boothowto & RB_VERBOSE))
		printf("Physical memory use set to %ldK\n", Maxmem * 4);

	/*
	 * If Maxmem has been increased beyond what the system has detected,
	 * extend the last memory segment to the new limit.
	 */ 
	if (atop(physmap[physmap_idx + 1]) < Maxmem)
		physmap[physmap_idx + 1] = ptoa((vm_paddr_t)Maxmem);

	/* call pmap initialization to make new kernel address space */
	pmap_bootstrap(first);

	/*
	 * Size up each available chunk of physical memory.
	 */
	physmap[0] = PAGE_SIZE;		/* mask off page 0 */
	pa_indx = 0;
	da_indx = 1;
	phys_avail[pa_indx++] = physmap[0];
	phys_avail[pa_indx] = physmap[0];
	dump_avail[da_indx] = physmap[0];

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
	for (i = 0; i <= physmap_idx; i += 2) {
		vm_paddr_t end;

		end = ptoa((vm_paddr_t)Maxmem);
		if (physmap[i + 1] < end)
			end = trunc_page(physmap[i + 1]);
		for (pa = round_page(physmap[i]); pa < end; pa += PAGE_SIZE) {
			int *ptr;
			int tmp;
			bool full, page_bad;

			full = false;
			/*
			 * block out kernel memory as not available.
			 */
			if (pa >= KERNLOAD && pa < first)
				goto do_dump_avail;

			/*
			 * block out dcons buffer
			 */
			if (dcons_addr > 0
			    && pa >= trunc_page(dcons_addr)
			    && pa < dcons_addr + dcons_size)
				goto do_dump_avail;

			page_bad = false;
			if (memtest == 0)
				goto skip_memtest;

			/*
			 * map page into kernel: valid, read/write,non-cacheable
			 */
			ptr = (int *)pmap_cmap3(pa, PG_V | PG_RW | PG_N);

			tmp = *(int *)ptr;
			/*
			 * Test for alternating 1's and 0's
			 */
			*(volatile int *)ptr = 0xaaaaaaaa;
			if (*(volatile int *)ptr != 0xaaaaaaaa)
				page_bad = true;
			/*
			 * Test for alternating 0's and 1's
			 */
			*(volatile int *)ptr = 0x55555555;
			if (*(volatile int *)ptr != 0x55555555)
				page_bad = true;
			/*
			 * Test for all 1's
			 */
			*(volatile int *)ptr = 0xffffffff;
			if (*(volatile int *)ptr != 0xffffffff)
				page_bad = true;
			/*
			 * Test for all 0's
			 */
			*(volatile int *)ptr = 0x0;
			if (*(volatile int *)ptr != 0x0)
				page_bad = true;
			/*
			 * Restore original value.
			 */
			*(int *)ptr = tmp;

skip_memtest:
			/*
			 * Adjust array of valid/good pages.
			 */
			if (page_bad == true)
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
					full = true;
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
				dump_avail[da_indx++] = pa;	/* start */
				dump_avail[da_indx] = pa + PAGE_SIZE; /* end */
			}
do_next:
			if (full)
				break;
		}
	}
	pmap_cmap3(0, 0);

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
	for (off = 0; off < round_page(msgbufsize); off += PAGE_SIZE)
		pmap_kenter((vm_offset_t)msgbufp + off, phys_avail[pa_indx] +
		    off);
}

static void
i386_kdb_init(void)
{
#ifdef DDB
	db_fetch_ksymtab(bootinfo.bi_symtab, bootinfo.bi_esymtab, 0);
#endif
	kdb_init();
#ifdef KDB
	if (boothowto & RB_KDB)
		kdb_enter(KDB_WHY_BOOTFLAGS, "Boot flags requested debugger");
#endif
}

static void
fixup_idt(void)
{
	struct gate_descriptor *ip;
	uintptr_t off;
	int x;

	for (x = 0; x < NIDT; x++) {
		ip = &idt[x];
		if (ip->gd_type != SDT_SYS386IGT &&
		    ip->gd_type != SDT_SYS386TGT)
			continue;
		off = ip->gd_looffset + (((u_int)ip->gd_hioffset) << 16);
		KASSERT(off >= (uintptr_t)start_exceptions &&
		    off < (uintptr_t)end_exceptions,
		    ("IDT[%d] type %d off %#x", x, ip->gd_type, off));
		off += setidt_disp;
		MPASS(off >= PMAP_TRM_MIN_ADDRESS &&
		    off < PMAP_TRM_MAX_ADDRESS);
		ip->gd_looffset = off;
		ip->gd_hioffset = off >> 16;
	}
}

static void
i386_setidt1(void)
{
	int x;

	/* exceptions */
	for (x = 0; x < NIDT; x++)
		setidt(x, &IDTVEC(rsvd), SDT_SYS386IGT, SEL_KPL,
		    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_DE, &IDTVEC(div), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_DB, &IDTVEC(dbg), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_NMI, &IDTVEC(nmi), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_BP, &IDTVEC(bpt), SDT_SYS386IGT, SEL_UPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_OF, &IDTVEC(ofl), SDT_SYS386IGT, SEL_UPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_BR, &IDTVEC(bnd), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_UD, &IDTVEC(ill), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_NM, &IDTVEC(dna), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_DF, 0, SDT_SYSTASKGT, SEL_KPL, GSEL(GPANIC_SEL,
	    SEL_KPL));
	setidt(IDT_FPUGP, &IDTVEC(fpusegm), SDT_SYS386IGT,
	    SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_TS, &IDTVEC(tss), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_NP, &IDTVEC(missing), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_SS, &IDTVEC(stk), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_GP, &IDTVEC(prot), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_PF, &IDTVEC(page), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_MF, &IDTVEC(fpu), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_AC, &IDTVEC(align), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_MC, &IDTVEC(mchk), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_XF, &IDTVEC(xmm), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_SYSCALL, &IDTVEC(int0x80_syscall),
	    SDT_SYS386IGT, SEL_UPL, GSEL(GCODE_SEL, SEL_KPL));
#ifdef KDTRACE_HOOKS
	setidt(IDT_DTRACE_RET, &IDTVEC(dtrace_ret),
	    SDT_SYS386IGT, SEL_UPL, GSEL(GCODE_SEL, SEL_KPL));
#endif
#ifdef XENHVM
	setidt(IDT_EVTCHN, &IDTVEC(xen_intr_upcall),
	    SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
#endif
}

static void
i386_setidt2(void)
{

	setidt(IDT_UD, &IDTVEC(ill), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_GP, &IDTVEC(prot), SDT_SYS386IGT, SEL_KPL,
	    GSEL(GCODE_SEL, SEL_KPL));
}

#if defined(DEV_ISA) && !defined(DEV_ATPIC)
static void
i386_setidt3(void)
{

	setidt(IDT_IO_INTS + 7, IDTVEC(spuriousint),
	    SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(IDT_IO_INTS + 15, IDTVEC(spuriousint),
	    SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
}
#endif

register_t
init386(int first)
{
	struct region_descriptor r_gdt, r_idt;	/* table descriptors */
	int gsel_tss, metadata_missing, x, pa;
	struct pcpu *pc;
	struct xstate_hdr *xhdr;
	vm_offset_t addend;
	size_t ucode_len;

	thread0.td_kstack = proc0kstack;
	thread0.td_kstack_pages = TD0_KSTACK_PAGES;

	/*
 	 * This may be done better later if it gets more high level
 	 * components in it. If so just link td->td_proc here.
	 */
	proc_linkup0(&proc0, &thread0);

	if (bootinfo.bi_modulep) {
		metadata_missing = 0;
		addend = (vm_paddr_t)bootinfo.bi_modulep < KERNBASE ?
		    PMAP_MAP_LOW : 0;
		preload_metadata = (caddr_t)bootinfo.bi_modulep + addend;
		preload_bootstrap_relocate(addend);
	} else {
		metadata_missing = 1;
	}

	if (bootinfo.bi_envp != 0) {
		addend = (vm_paddr_t)bootinfo.bi_envp < KERNBASE ?
		    PMAP_MAP_LOW : 0;
		init_static_kenv((char *)bootinfo.bi_envp + addend, 0);
	} else {
		init_static_kenv(NULL, 0);
	}

	/*
	 * Re-evaluate CPU features if we loaded a microcode update.
	 */
	ucode_len = ucode_load_bsp(first);
	if (ucode_len != 0) {
		identify_cpu();
		first = roundup2(first + ucode_len, PAGE_SIZE);
	}

	identify_hypervisor();
	identify_hypervisor_smbios();

	/* Init basic tunables, hz etc */
	init_param1();

	/* Set bootmethod to BIOS: it's the only supported on i386. */
	strlcpy(bootmethod, "BIOS", sizeof(bootmethod));

	/*
	 * Make gdt memory segments.  All segments cover the full 4GB
	 * of address space and permissions are enforced at page level.
	 */
	gdt_segs[GCODE_SEL].ssd_limit = atop(0 - 1);
	gdt_segs[GDATA_SEL].ssd_limit = atop(0 - 1);
	gdt_segs[GUCODE_SEL].ssd_limit = atop(0 - 1);
	gdt_segs[GUDATA_SEL].ssd_limit = atop(0 - 1);
	gdt_segs[GUFS_SEL].ssd_limit = atop(0 - 1);
	gdt_segs[GUGS_SEL].ssd_limit = atop(0 - 1);

	pc = &__pcpu[0];
	gdt_segs[GPRIV_SEL].ssd_limit = atop(0 - 1);
	gdt_segs[GPRIV_SEL].ssd_base = (int)pc;
	gdt_segs[GPROC0_SEL].ssd_base = (int)&common_tss0;

	for (x = 0; x < NGDT; x++)
		ssdtosd(&gdt_segs[x], &gdt0[x].sd);

	r_gdt.rd_limit = NGDT * sizeof(gdt0[0]) - 1;
	r_gdt.rd_base =  (int)gdt0;
	mtx_init(&dt_lock, "descriptor tables", NULL, MTX_SPIN);
	lgdt(&r_gdt);

	pcpu_init(pc, 0, sizeof(struct pcpu));
	for (pa = first; pa < first + DPCPU_SIZE; pa += PAGE_SIZE)
		pmap_kenter(pa, pa);
	dpcpu_init((void *)first, 0);
	first += DPCPU_SIZE;
	PCPU_SET(prvspace, pc);
	PCPU_SET(curthread, &thread0);
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
	mtx_init(&icu_lock, "icu", NULL, MTX_SPIN | MTX_NOWITNESS | MTX_NOPROFILE);

	i386_setidt1();

	r_idt.rd_limit = sizeof(idt0) - 1;
	r_idt.rd_base = (int) idt;
	lidt(&r_idt);

	finishidentcpu();	/* Final stage of CPU initialization */

	/*
	 * Initialize the clock before the console so that console
	 * initialization can use DELAY().
	 */
	clock_init();

	i386_setidt2();
	pmap_set_nx();
	initializecpu();	/* Initialize CPU registers */
	initializecpucache();

	/* pointer to selector slot for %fs/%gs */
	PCPU_SET(fsgs_gdt, &gdt[GUFS_SEL].sd);

	/* Initialize the tss (except for the final esp0) early for vm86. */
	common_tss0.tss_esp0 = thread0.td_kstack + thread0.td_kstack_pages *
	    PAGE_SIZE - VM86_STACK_SPACE;
	common_tss0.tss_ss0 = GSEL(GDATA_SEL, SEL_KPL);
	common_tss0.tss_ioopt = sizeof(struct i386tss) << 16;
	gsel_tss = GSEL(GPROC0_SEL, SEL_KPL);
	PCPU_SET(tss_gdt, &gdt[GPROC0_SEL].sd);
	PCPU_SET(common_tssd, *PCPU_GET(tss_gdt));
	ltr(gsel_tss);

	/* Initialize the PIC early for vm86 calls. */
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
	i386_setidt3();
#endif
#endif

	/*
	 * The console and kdb should be initialized even earlier than here,
	 * but some console drivers don't work until after getmemsize().
	 * Default to late console initialization to support these drivers.
	 * This loses mainly printf()s in getmemsize() and early debugging.
	 */
	TUNABLE_INT_FETCH("debug.late_console", &late_console);
	if (!late_console) {
		cninit();
		i386_kdb_init();
	}

	if (cpu_fxsr && (cpu_feature2 & CPUID2_XSAVE) != 0) {
		use_xsave = 1;
		TUNABLE_INT_FETCH("hw.use_xsave", &use_xsave);
	}

	/* Initialize preload_kmdp */
	preload_initkmdp(!metadata_missing);
	link_elf_ireloc();

	vm86_initialize();
	getmemsize(first);
	init_param2(physmem);

	/* now running on new page tables, configured,and u/iom is accessible */

	if (late_console)
		cninit();

	if (metadata_missing)
		printf("WARNING: loader(8) metadata is missing!\n");

	if (late_console)
		i386_kdb_init();

	msgbufinit(msgbufp, msgbufsize);
	npxinit(true);

	/*
	 * Set up thread0 pcb after npxinit calculated pcb + fpu save
	 * area size.  Zero out the extended state header in fpu save
	 * area.
	 */
	thread0.td_pcb = get_pcb_td(&thread0);
	thread0.td_pcb->pcb_save = get_pcb_user_save_td(&thread0);
	bzero(get_pcb_user_save_td(&thread0), cpu_max_ext_state_size);
	if (use_xsave) {
		xhdr = (struct xstate_hdr *)(get_pcb_user_save_td(&thread0) +
		    1);
		xhdr->xstate_bv = xsave_mask;
	}
	PCPU_SET(curpcb, thread0.td_pcb);
	/* Move esp0 in the tss to its final place. */
	/* Note: -16 is so we can grow the trapframe if we came from vm86 */
	common_tss0.tss_esp0 = (vm_offset_t)thread0.td_pcb - VM86_STACK_SPACE;
	PCPU_SET(kesp0, common_tss0.tss_esp0);
	gdt[GPROC0_SEL].sd.sd_type = SDT_SYS386TSS;	/* clear busy bit */
	ltr(gsel_tss);

	/* transfer to user mode */

	_ucodesel = GSEL(GUCODE_SEL, SEL_UPL);
	_udatasel = GSEL(GUDATA_SEL, SEL_UPL);

	/* setup proc 0's pcb */
	thread0.td_pcb->pcb_flags = 0;
	thread0.td_pcb->pcb_cr3 = pmap_get_kcr3();
	thread0.td_pcb->pcb_ext = 0;
	thread0.td_frame = &proc0_tf;

#ifdef FDT
	x86_init_fdt();
#endif

	/* Location of kernel stack for locore */
	return ((register_t)thread0.td_pcb);
}

static void
machdep_init_trampoline(void)
{
	struct region_descriptor r_gdt, r_idt;
	struct i386tss *tss;
	char *copyout_buf, *trampoline, *tramp_stack_base;
	int x;

	gdt = pmap_trm_alloc(sizeof(union descriptor) * NGDT * mp_ncpus,
	    M_NOWAIT | M_ZERO);
	bcopy(gdt0, gdt, sizeof(union descriptor) * NGDT);
	r_gdt.rd_limit = NGDT * sizeof(gdt[0]) - 1;
	r_gdt.rd_base = (int)gdt;
	lgdt(&r_gdt);

	tss = pmap_trm_alloc(sizeof(struct i386tss) * mp_ncpus,
	    M_NOWAIT | M_ZERO);
	bcopy(&common_tss0, tss, sizeof(struct i386tss));
	gdt[GPROC0_SEL].sd.sd_lobase = (int)tss;
	gdt[GPROC0_SEL].sd.sd_hibase = (u_int)tss >> 24;
	gdt[GPROC0_SEL].sd.sd_type = SDT_SYS386TSS;

	PCPU_SET(fsgs_gdt, &gdt[GUFS_SEL].sd);
	PCPU_SET(tss_gdt, &gdt[GPROC0_SEL].sd);
	PCPU_SET(common_tssd, *PCPU_GET(tss_gdt));
	PCPU_SET(common_tssp, tss);
	ltr(GSEL(GPROC0_SEL, SEL_KPL));

	trampoline = pmap_trm_alloc(end_exceptions - start_exceptions,
	    M_NOWAIT);
	bcopy(start_exceptions, trampoline, end_exceptions - start_exceptions);
	tramp_stack_base = pmap_trm_alloc(TRAMP_STACK_SZ, M_NOWAIT);
	PCPU_SET(trampstk, (uintptr_t)tramp_stack_base + TRAMP_STACK_SZ -
	    VM86_STACK_SPACE);
	tss[0].tss_esp0 = PCPU_GET(trampstk);

	idt = pmap_trm_alloc(sizeof(idt0), M_NOWAIT | M_ZERO);
	bcopy(idt0, idt, sizeof(idt0));

	/* Re-initialize new IDT since the handlers were relocated */
	setidt_disp = trampoline - start_exceptions;
	if (bootverbose)
		printf("Trampoline disposition %#zx\n", setidt_disp);
	fixup_idt();

	r_idt.rd_limit = sizeof(struct gate_descriptor) * NIDT - 1;
	r_idt.rd_base = (int)idt;
	lidt(&r_idt);

	/* dblfault TSS */
	dblfault_tss = pmap_trm_alloc(sizeof(struct i386tss), M_NOWAIT | M_ZERO);
	dblfault_stack = pmap_trm_alloc(PAGE_SIZE, M_NOWAIT);
	dblfault_tss->tss_esp = dblfault_tss->tss_esp0 =
	    dblfault_tss->tss_esp1 = dblfault_tss->tss_esp2 =
	    (int)dblfault_stack + PAGE_SIZE;
	dblfault_tss->tss_ss = dblfault_tss->tss_ss0 = dblfault_tss->tss_ss1 =
	    dblfault_tss->tss_ss2 = GSEL(GDATA_SEL, SEL_KPL);
	dblfault_tss->tss_cr3 = pmap_get_kcr3();
	dblfault_tss->tss_eip = (int)dblfault_handler;
	dblfault_tss->tss_eflags = PSL_KERNEL;
	dblfault_tss->tss_ds = dblfault_tss->tss_es =
	    dblfault_tss->tss_gs = GSEL(GDATA_SEL, SEL_KPL);
	dblfault_tss->tss_fs = GSEL(GPRIV_SEL, SEL_KPL);
	dblfault_tss->tss_cs = GSEL(GCODE_SEL, SEL_KPL);
	dblfault_tss->tss_ldt = GSEL(GLDT_SEL, SEL_KPL);
	gdt[GPANIC_SEL].sd.sd_lobase = (int)dblfault_tss;
	gdt[GPANIC_SEL].sd.sd_hibase = (u_int)dblfault_tss >> 24;

	/* make ldt memory segments */
	ldt = pmap_trm_alloc(sizeof(union descriptor) * NLDT,
	    M_NOWAIT | M_ZERO);
	gdt[GLDT_SEL].sd.sd_lobase = (int)ldt;
	gdt[GLDT_SEL].sd.sd_hibase = (u_int)ldt >> 24;
	ldt_segs[LUCODE_SEL].ssd_limit = atop(0 - 1);
	ldt_segs[LUDATA_SEL].ssd_limit = atop(0 - 1);
	for (x = 0; x < nitems(ldt_segs); x++)
		ssdtosd(&ldt_segs[x], &ldt[x].sd);

	_default_ldt = GSEL(GLDT_SEL, SEL_KPL);
	lldt(_default_ldt);
	PCPU_SET(currentldt, _default_ldt);

	copyout_buf = pmap_trm_alloc(TRAMP_COPYOUT_SZ, M_NOWAIT);
	PCPU_SET(copyout_buf, copyout_buf);
	copyout_init_tramp();
}
SYSINIT(vm_mem, SI_SUB_VM, SI_ORDER_SECOND, machdep_init_trampoline, NULL);

#ifdef COMPAT_43
static void
i386_setup_lcall_gate(void)
{
	struct sysentvec *sv;
	struct user_segment_descriptor desc;
	u_int lcall_addr;

	sv = &elf32_freebsd_sysvec;
	lcall_addr = (uintptr_t)sv->sv_psstrings - sz_lcall_tramp;

	bzero(&desc, sizeof(desc));
	desc.sd_type = SDT_MEMERA;
	desc.sd_dpl = SEL_UPL;
	desc.sd_p = 1;
	desc.sd_def32 = 1;
	desc.sd_gran = 1;
	desc.sd_lolimit = 0xffff;
	desc.sd_hilimit = 0xf;
	desc.sd_lobase = lcall_addr;
	desc.sd_hibase = lcall_addr >> 24;
	bcopy(&desc, &ldt[LSYS5CALLS_SEL], sizeof(desc));
}
SYSINIT(elf32, SI_SUB_EXEC, SI_ORDER_ANY, i386_setup_lcall_gate, NULL);
#endif

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
	uint32_t *smapattr;
	int count, error, i;

	/* Retrieve the system memory map from the loader. */
	smapbase = (struct bios_smap *)preload_search_info(preload_kmdp,
	    MODINFO_METADATA | MODINFOMD_SMAP);
	if (smapbase == NULL)
		return (0);
	smapattr = (uint32_t *)preload_search_info(preload_kmdp,
	    MODINFO_METADATA | MODINFOMD_SMAP_XATTR);
	count = *((u_int32_t *)smapbase - 1) / sizeof(*smapbase);
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

#if defined(I586_CPU) && !defined(NO_F00F_HACK)
static void f00f_hack(void *unused);
SYSINIT(f00f_hack, SI_SUB_INTRINSIC, SI_ORDER_FIRST, f00f_hack, NULL);

static void
f00f_hack(void *unused)
{
	struct region_descriptor r_idt;
	struct gate_descriptor *new_idt;
	vm_offset_t tmp;

	if (!has_f00f_bug)
		return;

	printf("Intel Pentium detected, installing workaround for F00F bug\n");

	tmp = (vm_offset_t)pmap_trm_alloc(PAGE_SIZE * 3, M_NOWAIT | M_ZERO);
	if (tmp == 0)
		panic("kmem_malloc returned 0");
	tmp = round_page(tmp);

	/* Put the problematic entry (#6) at the end of the lower page. */
	new_idt = (struct gate_descriptor *)
	    (tmp + PAGE_SIZE - 7 * sizeof(struct gate_descriptor));
	bcopy(idt, new_idt, sizeof(idt0));
	r_idt.rd_base = (u_int)new_idt;
	r_idt.rd_limit = sizeof(idt0) - 1;
	lidt(&r_idt);
	/* SMP machines do not need the F00F hack. */
	idt = new_idt;
	pmap_protect(kernel_pmap, tmp, tmp + PAGE_SIZE, VM_PROT_READ);
}
#endif /* defined(I586_CPU) && !NO_F00F_HACK */

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

	pcb->pcb_edi = tf->tf_edi;
	pcb->pcb_esi = tf->tf_esi;
	pcb->pcb_ebp = tf->tf_ebp;
	pcb->pcb_ebx = tf->tf_ebx;
	pcb->pcb_eip = tf->tf_eip;
	pcb->pcb_esp = (ISPL(tf->tf_cs)) ? tf->tf_esp : (int)(tf + 1) - 8;
	pcb->pcb_gs = rgs();
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
