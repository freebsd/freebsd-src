/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * NETLOGIC_BSD */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/rtprio.h>
#include <sys/systm.h>
#include <sys/interrupt.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/random.h>

#include <sys/cons.h>		/* cinit() */
#include <sys/kdb.h>
#include <sys/reboot.h>
#include <sys/queue.h>
#include <sys/smp.h>
#include <sys/timetc.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/cpuinfo.h>
#include <machine/tlb.h>
#include <machine/cpuregs.h>
#include <machine/frame.h>
#include <machine/hwfunc.h>
#include <machine/md_var.h>
#include <machine/asm.h>
#include <machine/pmap.h>
#include <machine/trap.h>
#include <machine/clock.h>
#include <machine/fls64.h>
#include <machine/intr_machdep.h>
#include <machine/smp.h>

#include <mips/nlm/hal/mips-extns.h>
#include <mips/nlm/hal/mmio.h>
#include <mips/nlm/hal/iomap.h>
#include <mips/nlm/hal/cop0.h>
#include <mips/nlm/hal/sys.h>
#include <mips/nlm/hal/pic.h>
#include <mips/nlm/hal/uart.h>
#include <mips/nlm/hal/mmu.h>
#include <mips/nlm/hal/bridge.h>
#include <mips/nlm/hal/cpucontrol.h>

#include <mips/nlm/clock.h>
#include <mips/nlm/interrupt.h>
#include <mips/nlm/board.h>
#include <mips/nlm/xlp.h>

/* 4KB static data aread to keep a copy of the bootload env until
   the dynamic kenv is setup */
char boot1_env[4096];
int xlp_argc;
char **xlp_argv, **xlp_envp;

uint64_t xlp_cpu_frequency;
uint64_t nlm_pcicfg_baseaddr = MIPS_PHYS_TO_KSEG1(XLP_DEFAULT_IO_BASE);

int xlp_ncores;
int xlp_threads_per_core;
uint32_t xlp_hw_thread_mask;
int xlp_cpuid_to_hwtid[MAXCPU];
int xlp_hwtid_to_cpuid[MAXCPU];
uint64_t xlp_pic_base;

static int xlp_mmuval;

extern uint32_t _end;
extern char XLPResetEntry[], XLPResetEntryEnd[];

static void
xlp_setup_core(void)
{
	uint64_t reg;

	reg = nlm_mfcr(XLP_LSU_DEFEATURE);
	/* Enable Unaligned and L2HPE */
	reg |= (1 << 30) | (1 << 23);
	/*
	 * Experimental : Enable SUE
	 * Speculative Unmap Enable. Enable speculative L2 cache request for
	 * unmapped access.
	 */
	reg |= (1ull << 31);
	/* Clear S1RCM  - A0 errata */
	reg &= ~0xeull;
	nlm_mtcr(XLP_LSU_DEFEATURE, reg);

	reg = nlm_mfcr(XLP_SCHED_DEFEATURE);
	/* Experimental: Disable BRU accepting ALU ops - A0 errata */
	reg |= (1 << 24);
	nlm_mtcr(XLP_SCHED_DEFEATURE, reg);
}

static void 
xlp_setup_mmu(void)
{

	nlm_setup_extended_pagemask(0); /* pagemask = 0 for 4K pages */
	nlm_large_variable_tlb_en(0);
	nlm_extended_tlb_en(1);
	nlm_mmu_setup(0, 0, 0);
}

static void
xlp_parse_mmu_options(void)
{
	int i, j, k;
	uint32_t cpu_map = xlp_hw_thread_mask;
	uint32_t core0_thr_mask, core_thr_mask;

#ifndef SMP /* Uniprocessor! */
	if (cpu_map != 0x1) {
		printf("WARNING: Starting uniprocessor kernel on cpumask [0x%lx]!\n"
		    "WARNING: Other CPUs will be unused.\n", (u_long)cpu_map);
		cpu_map = 0x1;
	}
#endif

	xlp_ncores = 1;
	core0_thr_mask = cpu_map & 0xf;
	switch (core0_thr_mask) {
	case 1:
		xlp_threads_per_core = 1;
		xlp_mmuval = 0;
	       	break;
	case 3:
		xlp_threads_per_core = 2;
		xlp_mmuval = 2;
	       	break;
	case 0xf: 
		xlp_threads_per_core = 4;
		xlp_mmuval = 3;
	       	break;
	default:
		goto unsupp;
	}

	/* Verify other cores CPU masks */
	for (i = 1; i < XLP_MAX_CORES; i++) {
		core_thr_mask = (cpu_map >> (i*4)) & 0xf;
		if (core_thr_mask) {
			if (core_thr_mask != core0_thr_mask)
				goto unsupp; 
			xlp_ncores++;
		}
	}

	xlp_hw_thread_mask = cpu_map;
	/* setup hardware processor id to cpu id mapping */
	for (i = 0; i< MAXCPU; i++)
		xlp_cpuid_to_hwtid[i] = 
		    xlp_hwtid_to_cpuid [i] = -1;
	for (i = 0, k = 0; i < XLP_MAX_CORES; i++) {
		if (((cpu_map >> (i*4)) & 0xf) == 0)
			continue;
		for (j = 0; j < xlp_threads_per_core; j++) {
			xlp_cpuid_to_hwtid[k] = i*4 + j;
			xlp_hwtid_to_cpuid[i*4 + j] = k;
			k++;
		}
	}

#ifdef SMP
	/* 
	 * We will enable the other threads in core 0 here
	 * so that the TLB and cache info is correct when
	 * mips_init runs
	 */
	xlp_enable_threads(xlp_mmuval);
#endif
	/* setup for the startup core */
	xlp_setup_mmu();
	return;

unsupp:
	printf("ERROR : Unsupported CPU mask [use 1,2 or 4 threads per core].\n"
	    "\tcore0 thread mask [%lx], boot cpu mask [%lx].\n",
	    (u_long)core0_thr_mask, (u_long)cpu_map);
	panic("Invalid CPU mask - halting.\n");
	return;
}

static void 
xlp_set_boot_flags(void)
{
	char *p;

	p = getenv("bootflags");
	if (p == NULL)
		return;

	for (; p && *p != '\0'; p++) {
		switch (*p) {
		case 'd':
		case 'D':
			boothowto |= RB_KDB;
			break;
		case 'g':
		case 'G':
			boothowto |= RB_GDB;
			break;
		case 'v':
		case 'V':
			boothowto |= RB_VERBOSE;
			break;

		case 's':	/* single-user (default, supported for sanity) */
		case 'S':
			boothowto |= RB_SINGLE;
			break;

		default:
			printf("Unrecognized boot flag '%c'.\n", *p);
			break;
		}
	}

	freeenv(p);
	return;
}

static void
mips_init(void)
{
	init_param1();
	init_param2(physmem);

	mips_cpu_init();
	cpuinfo.cache_coherent_dma = TRUE;
	pmap_bootstrap();
#ifdef DDB
	kdb_init();
	if (boothowto & RB_KDB) {
		kdb_enter("Boot flags requested debugger", NULL);
	}
#endif
	mips_proc0_init();
	mutex_init();
}

unsigned int
platform_get_timecount(struct timecounter *tc __unused)
{

	return ((unsigned int)~nlm_pic_read_systimer(xlp_pic_base, 7));
}

static void 
xlp_pic_init(void)
{
	struct timecounter pic_timecounter = {
		platform_get_timecount, /* get_timecount */
		0,                      /* no poll_pps */
		~0U,                    /* counter_mask */
		XLP_PIC_TIMER_FREQ,           /* frequency */
		"XLRPIC",               /* name */
		2000,                   /* quality (adjusted in code) */
	};
        int i;

	xlp_pic_base = nlm_regbase_pic(0);  /* TOOD: Add other nodes */
        printf("Initializing PIC...@%jx\n", (uintmax_t)xlp_pic_base);
	/* Bind all PIC irqs to cpu 0 */
        for(i = 0; i < XLP_PIC_MAX_IRT; i++) {
                nlm_pic_write_irt_raw(xlp_pic_base, i, 0, 0, 1, 0,
		    1, 0, 0x1);
        }

	nlm_pic_set_systimer(xlp_pic_base, 7, ~0ULL, 0, 0, 0, 0);
	platform_timecounter = &pic_timecounter;
}

#if defined(__mips_n32) || defined(__mips_n64) /* PHYSADDR_64_BIT */
#ifdef XLP_SIM
#define	XLP_MEM_LIM	0x200000000ULL
#else
#define	XLP_MEM_LIM	0x10000000000ULL
#endif
#else
#define	XLP_MEM_LIM	0xfffff000UL
#endif
static void
xlp_mem_init(void)
{
	uint64_t bridgebase = nlm_regbase_bridge(0);  /* TOOD: Add other nodes */
	vm_size_t physsz = 0;
        uint64_t base, lim, val;
	int i, j;

        for (i = 0, j = 0; i < 8; i++) {
		val = nlm_rdreg_bridge(bridgebase, XLP_BRIDGE_DRAM_BAR_REG(i));
		base = ((val >>  12) & 0xfffff) << 20;
		val = nlm_rdreg_bridge(bridgebase, XLP_BRIDGE_DRAM_LIMIT_REG(i));
                lim = ((val >>  12) & 0xfffff) << 20;

		/* BAR not enabled */
		if (lim == 0)
			continue;

		/* first bar, start a bit after end */
		if (base == 0) {
			base = (vm_paddr_t)MIPS_KSEG0_TO_PHYS(&_end) + 0x20000;
			lim  = 0x0c000000;  /* TODO : hack to avoid uboot packet mem */
		}
		if (base >= XLP_MEM_LIM) {
			printf("Mem [%d]: Ignore %#jx - %#jx\n", i,
			   (intmax_t)base, (intmax_t)lim);
			continue;
		}
		if (lim > XLP_MEM_LIM) {
			printf("Mem [%d]: Restrict %#jx -> %#jx\n", i,
			    (intmax_t)lim, (intmax_t)XLP_MEM_LIM);
			lim = XLP_MEM_LIM;
		}
		if (lim <= base) {
			printf("Mem[%d]: Malformed %#jx -> %#jx\n", i,
			    (intmax_t)base, (intmax_t)lim);
			continue;
		}

		/*
		 * Exclude reset entry memory range 0x1fc00000 - 0x20000000
		 * from free memory
		 */
		if (base <= 0x1fc00000 && (base + lim) > 0x1fc00000) {
			uint64_t base0, lim0, base1, lim1;

			base0 = base;
			lim0 = 0x1fc00000;
			base1 = 0x20000000;
			lim1 = lim;

			if (lim0 > base0) {
				phys_avail[j++] = (vm_paddr_t)base0;
				phys_avail[j++] = (vm_paddr_t)lim0;
				physsz += lim0 - base0;
				printf("Mem[%d]: %#jx - %#jx (excl reset)\n", i,
				    (intmax_t)base0, (intmax_t)lim0);
			}
			if (lim1 > base1) {
				phys_avail[j++] = (vm_paddr_t)base1;
				phys_avail[j++] = (vm_paddr_t)lim1;
				physsz += lim1 - base1;
				printf("Mem[%d]: %#jx - %#jx (excl reset)\n", i,
				    (intmax_t)base1, (intmax_t)lim1);
			}
		} else {
			phys_avail[j++] = (vm_paddr_t)base;
			phys_avail[j++] = (vm_paddr_t)lim;
			physsz += lim - base;
			printf("Mem[%d]: %#jx - %#jx\n", i,
			    (intmax_t)base, (intmax_t)lim);
		}

        }

	/* setup final entry with 0 */
	phys_avail[j] = phys_avail[j + 1] = 0;
	realmem = physmem = btoc(physsz);
}

static uint32_t
xlp_get_cpu_frequency(void)
{
	uint64_t  sysbase = nlm_regbase_sys(0);
	unsigned int pll_divf, pll_divr, dfs_div, num, denom;
	uint32_t val;
	       
	val = nlm_rdreg_sys(sysbase, XLP_SYS_POWER_ON_RESET_REG);
	pll_divf = (val >> 10) & 0x7f;
	pll_divr = (val >> 8)  & 0x3;
	dfs_div  = (val >> 17) & 0x3;

	num = pll_divf + 1;
	denom = 3 * (pll_divr + 1) * (1<< (dfs_div + 1));
	val = 800000000ULL * num / denom;
	return (val);
}

void
platform_start(__register_t a0 __unused,
    __register_t a1 __unused,
    __register_t a2 __unused,
    __register_t a3 __unused)
{
	int i;

	xlp_argc = 1;
	/*
	 * argv and envp are passed in array of 32bit pointers
	 */
	xlp_argv = NULL;
	xlp_envp = NULL;

	/* Initialize pcpu stuff */
	mips_pcpu0_init();

	/* initialize console so that we have printf */
	boothowto |= (RB_SERIAL | RB_MULTIPLE);	/* Use multiple consoles */

	/* For now */
	boothowto |= RB_VERBOSE;
	boothowto |= RB_SINGLE;
	bootverbose++;

	/* clockrate used by delay, so initialize it here */
	xlp_cpu_frequency = xlp_get_cpu_frequency();
	cpu_clock = xlp_cpu_frequency / 1000000;
	mips_timer_early_init(xlp_cpu_frequency);

	/* Init console please */
	cninit();

	/* Environment */
	printf("Args %#jx %#jx %#jx %#jx:\n", (intmax_t)a0,
	    (intmax_t)a1, (intmax_t)a2, (intmax_t)a3);
	xlp_hw_thread_mask = a0;
	init_static_kenv(boot1_env, sizeof(boot1_env));
	printf("Environment (from %d args):\n", xlp_argc - 1);
	if (xlp_argc == 1)
		printf("\tNone\n");
	for (i = 1; i < xlp_argc; i++) {
		char *n, *arg;

		arg = (char *)(intptr_t)xlp_argv[i];
		printf("\t%s\n", arg);
		n = strsep(&arg, "=");
		if (arg == NULL)
			setenv(n, "1");
		else
			setenv(n, arg);
	}

	/* Early core init and fixes for errata */
	xlp_setup_core();

	xlp_set_boot_flags();
	xlp_parse_mmu_options();
	
	xlp_mem_init();

	bcopy(XLPResetEntry, (void *)MIPS_RESET_EXC_VEC,
              XLPResetEntryEnd - XLPResetEntry);

	/*
	 * MIPS generic init 
	 */
	mips_init();
	/*
	 * XLP specific post initialization
 	 * initialize other on chip stuff
	 */
	nlm_board_info_setup();
	xlp_pic_init();

	mips_timer_init_params(xlp_cpu_frequency, 0);
}

void 
platform_cpu_init()
{
}

void
platform_identify(void)
{

	printf("XLP Eval Board\n");
}

/*
 * XXX Maybe return the state of the watchdog in enter, and pass it to
 * exit?  Like spl().
 */
void
platform_trap_enter(void)
{
}

void
platform_reset(void)
{
	uint64_t sysbase = nlm_regbase_sys(0);

	nlm_wreg_sys(sysbase, XLP_SYS_CHIP_RESET_REG, 1);
	for(;;)
		__asm __volatile("wait");
}

void
platform_trap_exit(void)
{
}

#ifdef SMP
/*
 * XLP threads are started simultaneously when we enable threads, this will
 * ensure that the threads are blocked in platform_init_ap, until they are 
 * ready to proceed to smp_init_secondary()
 */
static volatile int thr_unblock[4];

int
platform_start_ap(int cpuid)
{
	uint32_t coremask, val;
	uint64_t sysbase = nlm_regbase_sys(0);
	int hwtid = xlp_cpuid_to_hwtid[cpuid];
	int core, thr;

	core = hwtid / 4;
	thr = hwtid % 4;
	if (thr == 0) {
		/* First thread in core, do core wake up */
		coremask = 1u << core;

		/* Enable core clock */
		val = nlm_rdreg_sys(sysbase, XLP_SYS_CORE_DFS_DIS_CTRL_REG);
		val &= ~coremask;
		nlm_wreg_sys(sysbase, XLP_SYS_CORE_DFS_DIS_CTRL_REG, val);

		/* Remove CPU Reset */
		val = nlm_rdreg_sys(sysbase, XLP_SYS_CPU_RESET_REG);
		val &=  ~coremask & 0xff;
		nlm_wreg_sys(sysbase, XLP_SYS_CPU_RESET_REG, val);

		if (bootverbose)
			printf("Waking up core %d ...", core);

		/* Poll for CPU to mark itself coherent */
		do {
			val = nlm_rdreg_sys(sysbase, XLP_SYS_CPU_NONCOHERENT_MODE_REG);
       		} while ((val & coremask) != 0);
		if (bootverbose)
			printf("Done\n");
        } else {
		/* otherwise release the threads stuck in platform_init_ap */
		thr_unblock[thr] = 1;
	}

	return (0);
}

void
platform_init_ap(int cpuid)
{
	uint32_t stat;
	int thr;

	/* The first thread has to setup the MMU and enable other threads */
	thr = nlm_threadid();
	if (thr == 0) {
		xlp_setup_core();
		xlp_enable_threads(xlp_mmuval);
		xlp_setup_mmu();
	} else {
		/*
		 * FIXME busy wait here eats too many cycles, especially 
		 * in the core 0 while bootup
		 */
		while (thr_unblock[thr] == 0)
			__asm__ __volatile__ ("nop;nop;nop;nop");
		thr_unblock[thr] = 0;
	}

	stat = mips_rd_status();
	KASSERT((stat & MIPS_SR_INT_IE) == 0,
	    ("Interrupts enabled in %s!", __func__));
	stat |= MIPS_SR_COP_2_BIT | MIPS_SR_COP_0_BIT;
	mips_wr_status(stat);

	nlm_write_c0_eimr(0ull);
	xlp_enable_irq(IRQ_IPI);
	xlp_enable_irq(IRQ_TIMER);
	xlp_enable_irq(IRQ_MSGRING);

	return;
}

int
platform_ipi_intrnum(void) 
{

	return (IRQ_IPI);
}

void
platform_ipi_send(int cpuid)
{

	nlm_pic_send_ipi(xlp_pic_base, 0, xlp_cpuid_to_hwtid[cpuid],
	    platform_ipi_intrnum(), 0);
}

void
platform_ipi_clear(void)
{
}

int
platform_processor_id(void)
{

	return (xlp_hwtid_to_cpuid[nlm_cpuid()]);
}

void
platform_cpu_mask(cpuset_t *mask)
{
	int i, s;

	CPU_ZERO(mask);
	s = xlp_ncores * xlp_threads_per_core;
	for (i = 0; i < s; i++)
		CPU_SET(i, mask);
}

struct cpu_group *
platform_smp_topo()
{

	return (smp_topo_2level(CG_SHARE_L2, xlp_ncores, CG_SHARE_L1,
		xlp_threads_per_core, CG_FLAG_THREAD));
}
#endif
