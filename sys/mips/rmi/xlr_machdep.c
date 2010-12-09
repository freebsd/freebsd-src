/*-
 * Copyright (c) 2006-2009 RMI Corporation
 * Copyright (c) 2002-2004 Juli Mallett <jmallett@FreeBSD.org>
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

#include <mips/rmi/iomap.h>
#include <mips/rmi/msgring.h>
#include <mips/rmi/interrupt.h>
#include <mips/rmi/pic.h>
#include <mips/rmi/board.h>
#include <mips/rmi/rmi_mips_exts.h>
#include <mips/rmi/rmi_boot_info.h>

void mpwait(void);
unsigned long xlr_io_base = (unsigned long)(DEFAULT_XLR_IO_BASE);

/* 4KB static data aread to keep a copy of the bootload env until
   the dynamic kenv is setup */
char boot1_env[4096];
int rmi_spin_mutex_safe=0;
struct mtx xlr_pic_lock;

/*
 * Parameters from boot loader
 */
struct boot1_info xlr_boot1_info;
int xlr_run_mode;
int xlr_argc;
int32_t *xlr_argv, *xlr_envp;
uint64_t cpu_mask_info;
uint32_t xlr_online_cpumask;
uint32_t xlr_core_cpu_mask = 0x1;	/* Core 0 thread 0 is always there */

int xlr_shtlb_enabled;
int xlr_ncores;
int xlr_threads_per_core;
uint32_t xlr_hw_thread_mask;
int xlr_cpuid_to_hwtid[MAXCPU];
int xlr_hwtid_to_cpuid[MAXCPU];

static void 
xlr_setup_mmu_split(void)
{
	uint64_t mmu_setup;
	int val = 0;

	if (xlr_threads_per_core == 4 && xlr_shtlb_enabled == 0)
		return;   /* no change from boot setup */	

	switch (xlr_threads_per_core) {
	case 1: 
		val = 0; break;
	case 2: 
		val = 2; break;
	case 4: 
		val = 3; break;
	}
	
	mmu_setup = read_xlr_ctrl_register(4, 0);
	mmu_setup = mmu_setup & ~0x06;
	mmu_setup |= (val << 1);

	/* turn on global mode */
	if (xlr_shtlb_enabled)
		mmu_setup |= 0x01;

	write_xlr_ctrl_register(4, 0, mmu_setup);
}

static void
xlr_parse_mmu_options(void)
{
#ifdef notyet
	char *hw_env, *start, *end;
#endif
	uint32_t cpu_map;
	uint8_t core0_thr_mask, core_thr_mask;
	int i, j, k;

	/* First check for the shared TLB setup */
	xlr_shtlb_enabled = 0;
#ifdef notyet
	/* 
	 * We don't support sharing TLB per core - TODO
	 */
	xlr_shtlb_enabled = 0;
	if ((hw_env = getenv("xlr.shtlb")) != NULL) {
		start = hw_env;
		tmp = strtoul(start, &end, 0);
		if (start != end)
			xlr_shtlb_enabled = (tmp != 0);
		else
			printf("Bad value for xlr.shtlb [%s]\n", hw_env);
		freeenv(hw_env);
	}
#endif
	/*
	 * XLR supports splitting the 64 TLB entries across one, two or four
	 * threads (split mode).  XLR also allows the 64 TLB entries to be shared
         * across all threads in the core using a global flag (shared TLB mode).
         * We will support 1/2/4 threads in split mode or shared mode.
	 *
	 */
	xlr_ncores = 1;
	cpu_map = xlr_boot1_info.cpu_online_map;

#ifndef SMP /* Uniprocessor! */
	if (cpu_map != 0x1) {
		printf("WARNING: Starting uniprocessor kernel on cpumask [0x%lx]!\n"
		   "WARNING: Other CPUs will be unused.\n", (u_long)cpu_map);
		cpu_map = 0x1;
	}
#endif
	core0_thr_mask = cpu_map & 0xf;
	switch (core0_thr_mask) {
	case 1:
		xlr_threads_per_core = 1; break;
	case 3:
		xlr_threads_per_core = 2; break;
	case 0xf: 
		xlr_threads_per_core = 4; break;
	default:
		goto unsupp;
	}

	/* Verify other cores CPU masks */
	for (i = 1; i < XLR_MAX_CORES; i++) {
		core_thr_mask = (cpu_map >> (i*4)) & 0xf;
		if (core_thr_mask) {
			if (core_thr_mask != core0_thr_mask)
				goto unsupp; 
			xlr_ncores++;
		}
	}
	xlr_hw_thread_mask = cpu_map;

	/* setup hardware processor id to cpu id mapping */
	for (i = 0; i< MAXCPU; i++)
		xlr_cpuid_to_hwtid[i] = 
		    xlr_hwtid_to_cpuid [i] = -1;
	for (i = 0, k = 0; i < XLR_MAX_CORES; i++) {
		if (((cpu_map >> (i*4)) & 0xf) == 0)
			continue;
		for (j = 0; j < xlr_threads_per_core; j++) {
			xlr_cpuid_to_hwtid[k] = i*4 + j;
			xlr_hwtid_to_cpuid[i*4 + j] = k;
			k++;
		}
	}

	/* setup for the startup core */
	xlr_setup_mmu_split();
	return;

unsupp:
	printf("ERROR : Unsupported CPU mask [use 1,2 or 4 threads per core].\n"
	    "\tcore0 thread mask [%lx], boot cpu mask [%lx]\n"
	    "\tUsing default, 16 TLB entries per CPU, split mode\n", 
	    (u_long)core0_thr_mask, (u_long)cpu_map);
	panic("Invalid CPU mask - halting.\n");
	return;
}

static void 
xlr_set_boot_flags(void)
{
	char *p;

	p = getenv("bootflags");
	if (p == NULL)
		p = getenv("boot_flags");  /* old style */
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
extern uint32_t _end;

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

u_int
platform_get_timecount(struct timecounter *tc __unused)
{

	return (0xffffffffU - pic_timer_count32(PIC_CLOCK_TIMER));
}

static void 
xlr_pic_init(void)
{
	struct timecounter pic_timecounter = {
		platform_get_timecount, /* get_timecount */
		0,                      /* no poll_pps */
		~0U,                    /* counter_mask */
		PIC_TIMER_HZ,           /* frequency */
		"XLRPIC",               /* name */
		2000,                   /* quality (adjusted in code) */
	};
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_PIC_OFFSET);
	int i, irq;

	write_c0_eimr64(0ULL);
	mtx_init(&xlr_pic_lock, "pic", NULL, MTX_SPIN);
	xlr_write_reg(mmio, PIC_CTRL, 0);

	/* Initialize all IRT entries */
	for (i = 0; i < PIC_NUM_IRTS; i++) {
		irq = PIC_INTR_TO_IRQ(i);

		/*
		 * Disable all IRTs. Set defaults (local scheduling, high
		 * polarity, level * triggered, and CPU irq)
		 */
		xlr_write_reg(mmio, PIC_IRT_1(i), (1 << 30) | (1 << 6) | irq);
		/* Bind all PIC irqs to cpu 0 */
		xlr_write_reg(mmio, PIC_IRT_0(i), 0x01);
	}

	/* Setup timer 7 of PIC as a timestamp, no interrupts */
	pic_init_timer(PIC_CLOCK_TIMER);
	pic_set_timer(PIC_CLOCK_TIMER, ~UINT64_C(0));
	platform_timecounter = &pic_timecounter;
}

static void
xlr_mem_init(void)
{
	struct xlr_boot1_mem_map *boot_map;
	vm_size_t physsz = 0;
	int i, j;

	/* get physical memory info from boot loader */
	boot_map = (struct xlr_boot1_mem_map *)
	    (unsigned long)xlr_boot1_info.psb_mem_map;
	for (i = 0, j = 0; i < boot_map->num_entries; i++, j += 2) {
		if (boot_map->physmem_map[i].type == BOOT1_MEM_RAM) {
			if (j == 14) {
				printf("*** ERROR *** memory map too large ***\n");
				break;
			}
			if (j == 0) {
				/* TODO FIXME  */
				/* start after kernel end */
				phys_avail[0] = (vm_paddr_t)
				    MIPS_KSEG0_TO_PHYS(&_end) + 0x20000;
				/* boot loader start */
				/* HACK to Use bootloaders memory region */
				/* TODO FIXME  */
				if (boot_map->physmem_map[0].size == 0x0c000000) {
					boot_map->physmem_map[0].size = 0x0ff00000;
				}
				phys_avail[1] = boot_map->physmem_map[0].addr +
				    boot_map->physmem_map[0].size;
				printf("First segment: addr:%p -> %p \n",
				       (void *)phys_avail[0], 
				       (void *)phys_avail[1]);

				dump_avail[0] = boot_map->physmem_map[0].addr;
				dump_avail[1] = boot_map->physmem_map[0].size;

			} else {
/*
 * Can't use this code yet, because most of the fixed allocations happen from
 * the biggest physical area. If we have more than 512M memory the kernel will try
 * to map from the second are which is not in KSEG0 and not mapped
 */
				phys_avail[j] = (vm_paddr_t)
				    boot_map->physmem_map[i].addr;
				phys_avail[j + 1] = phys_avail[j] +
				    boot_map->physmem_map[i].size;
				if (phys_avail[j + 1] < phys_avail[j] ) {
					/* Houston we have an issue. Memory is
					 * larger than possible. Its probably in
					 * 64 bit > 4Gig and we are in 32 bit mode.
					 */
					phys_avail[j + 1] = 0xfffff000;
					printf("boot map size was %jx\n",
					    (intmax_t)boot_map->physmem_map[i].size);
					boot_map->physmem_map[i].size = phys_avail[j + 1]
					    - phys_avail[j];
					printf("reduced to %jx\n", 
					    (intmax_t)boot_map->physmem_map[i].size);
				}
				printf("Next segment : addr:%p -> %p \n",
				       (void *)phys_avail[j], 
				       (void *)phys_avail[j+1]);
			}

			dump_avail[j] = boot_map->physmem_map[j].addr;
			dump_avail[j+1] = boot_map->physmem_map[j].size;

			physsz += boot_map->physmem_map[i].size;
		}
	}

	/* FIXME XLR TODO */
	phys_avail[j] = phys_avail[j + 1] = 0;
	realmem = physmem = btoc(physsz);
}

void
platform_start(__register_t a0 __unused,
    __register_t a1 __unused,
    __register_t a2 __unused,
    __register_t a3 __unused)
{
	int i;
#ifdef SMP
	uint32_t tmp;
	void (*wakeup) (void *, void *, unsigned int);
#endif

	/* XXX FIXME the code below is not 64 bit clean */
	/* Save boot loader and other stuff from scratch regs */
	xlr_boot1_info = *(struct boot1_info *)(intptr_t)(int)read_c0_register32(MIPS_COP_0_OSSCRATCH, 0);
	cpu_mask_info = read_c0_register64(MIPS_COP_0_OSSCRATCH, 1);
	xlr_online_cpumask = read_c0_register32(MIPS_COP_0_OSSCRATCH, 2);
	xlr_run_mode = read_c0_register32(MIPS_COP_0_OSSCRATCH, 3);
	xlr_argc = read_c0_register32(MIPS_COP_0_OSSCRATCH, 4);
	/*
	 * argv and envp are passed in array of 32bit pointers
	 */
	xlr_argv = (int32_t *)(intptr_t)(int)read_c0_register32(MIPS_COP_0_OSSCRATCH, 5);
	xlr_envp = (int32_t *)(intptr_t)(int)read_c0_register32(MIPS_COP_0_OSSCRATCH, 6);

	/* TODO: Verify the magic number here */
	/* FIXMELATER: xlr_boot1_info.magic_number */

	/* Initialize pcpu stuff */
	mips_pcpu0_init();

	/* initialize console so that we have printf */
	boothowto |= (RB_SERIAL | RB_MULTIPLE);	/* Use multiple consoles */

	/* clockrate used by delay, so initialize it here */
	cpu_clock = xlr_boot1_info.cpu_frequency / 1000000;

	/*
	 * Note the time counter on CPU0 runs not at system clock speed, but
	 * at PIC time counter speed (which is returned by
	 * platform_get_frequency(). Thus we do not use
	 * xlr_boot1_info.cpu_frequency here.
	 */
	mips_timer_early_init(xlr_boot1_info.cpu_frequency);

	/* Init console please */
	cninit();
	init_static_kenv(boot1_env, sizeof(boot1_env));
	printf("Environment (from %d args):\n", xlr_argc - 1);
	if (xlr_argc == 1)
		printf("\tNone\n");
	for (i = 1; i < xlr_argc; i++) {
		char *n, *arg;

		arg = (char *)(intptr_t)xlr_argv[i];
		printf("\t%s\n", arg);
		n = strsep(&arg, "=");
		if (arg == NULL)
			setenv(n, "1");
		else
			setenv(n, arg);
	}

	xlr_set_boot_flags();
	xlr_parse_mmu_options();

	xlr_mem_init();
	/* Set up hz, among others. */
	mips_init();

#ifdef SMP
	/*
	 * If thread 0 of any core is not available then mark whole core as
	 * not available
	 */
	tmp = xlr_boot1_info.cpu_online_map;
	for (i = 4; i < MAXCPU; i += 4) {
		if ((tmp & (0xf << i)) && !(tmp & (0x1 << i))) {
			/*
			 * Oops.. thread 0 is not available. Disable whole
			 * core
			 */
			tmp = tmp & ~(0xf << i);
			printf("WARNING: Core %d is disabled because thread 0"
			    " of this core is not enabled.\n", i / 4);
		}
	}
	xlr_boot1_info.cpu_online_map = tmp;

	/* Wakeup Other cpus, and put them in bsd park code. */
	wakeup = ((void (*) (void *, void *, unsigned int))
	    (unsigned long)(xlr_boot1_info.wakeup));
	printf("Waking up CPUs 0x%jx.\n", 
	    (intmax_t)xlr_boot1_info.cpu_online_map & ~(0x1U));
	if (xlr_boot1_info.cpu_online_map & ~(0x1U))
		wakeup(mpwait, 0,
		    (unsigned int)xlr_boot1_info.cpu_online_map);
#endif

	/* xlr specific post initialization */
	/* initialize other on chip stuff */
	xlr_board_info_setup();
	xlr_msgring_config();
	xlr_pic_init();
	xlr_msgring_cpu_init();

	mips_timer_init_params(xlr_boot1_info.cpu_frequency, 0);

	printf("Platform specific startup now completes\n");
}

void 
platform_cpu_init()
{
}

void
platform_identify(void)
{

	printf("Board [%d:%d], processor 0x%08x\n", (int)xlr_boot1_info.board_major_version,
	    (int)xlr_boot1_info.board_minor_version, mips_rd_prid());
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
	xlr_reg_t *mmio = xlr_io_mmio(XLR_IO_GPIO_OFFSET);

	/* write 1 to GPIO software reset register */
	xlr_write_reg(mmio, 8, 1);
}

void
platform_trap_exit(void)
{
}

#ifdef SMP
int xlr_ap_release[MAXCPU];

int
platform_start_ap(int cpuid)
{
	int hwid = xlr_cpuid_to_hwtid[cpuid];

	if (xlr_boot1_info.cpu_online_map & (1<<hwid)) {
		/*
		 * other cpus are enabled by the boot loader and they will be 
		 * already looping in mpwait, release them
		 */
		atomic_store_rel_int(&xlr_ap_release[hwid], 1);
		return (0);
	} else
		return (-1);
}

void
platform_init_ap(int cpuid)
{
	uint32_t stat;

	/* The first thread has to setup the core MMU split  */
	if (xlr_thr_id() == 0)
		xlr_setup_mmu_split();

	/* Setup interrupts for secondary CPUs here */
	stat = mips_rd_status();
	KASSERT((stat & MIPS_SR_INT_IE) == 0,
	    ("Interrupts enabled in %s!", __func__));
	stat |= MIPS_SR_COP_2_BIT | MIPS_SR_COP_0_BIT;
	mips_wr_status(stat);

	write_c0_eimr64(0ULL);
	xlr_enable_irq(IRQ_IPI);
	xlr_enable_irq(IRQ_TIMER);
	if (xlr_thr_id() == 0) {
		xlr_msgring_cpu_init(); 
	 	xlr_enable_irq(IRQ_MSGRING);
	}

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

	pic_send_ipi(xlr_cpuid_to_hwtid[cpuid], platform_ipi_intrnum());
}

void
platform_ipi_clear(void)
{
}

int
platform_processor_id(void)
{

	return (xlr_hwtid_to_cpuid[xlr_cpu_id()]);
}

int
platform_num_processors(void)
{

	return (xlr_ncores * xlr_threads_per_core);
}

struct cpu_group *
platform_smp_topo()
{

	return (smp_topo_2level(CG_SHARE_L2, xlr_ncores, CG_SHARE_L1,
		xlr_threads_per_core, CG_FLAG_THREAD));
}
#endif
