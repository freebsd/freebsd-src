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
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/ktr.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/random.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/unistd.h>

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
#include <mips/rmi/rmi_mips_exts.h>

#include <mips/rmi/iomap.h>
#include <mips/rmi/clock.h>
#include <mips/rmi/msgring.h>
#include <mips/rmi/xlrconfig.h>
#include <mips/rmi/interrupt.h>
#include <mips/rmi/pic.h>

#ifdef XLR_PERFMON
#include <mips/rmi/perfmon.h>
#endif



void platform_prep_smp_launch(void);

unsigned long xlr_io_base = (unsigned long)(DEFAULT_XLR_IO_BASE);

/* 4KB static data aread to keep a copy of the bootload env until
   the dynamic kenv is setup */
char boot1_env[4096];
extern unsigned long _gp;
int rmi_spin_mutex_safe=0;
/*
 * Parameters from boot loader
 */
struct boot1_info xlr_boot1_info;
struct xlr_loader_info xlr_loader_info;	/* FIXME : Unused */
int xlr_run_mode;
int xlr_argc;
char **xlr_argv, **xlr_envp;
uint64_t cpu_mask_info;
uint32_t xlr_online_cpumask;

#ifdef SMP
static unsigned long xlr_secondary_gp[MAXCPU];
static unsigned long xlr_secondary_sp[MAXCPU];

#endif
extern int mips_cpu_online_mask;
extern int mips_cpu_logical_mask;
uint32_t cpu_ltop_map[MAXCPU];
uint32_t cpu_ptol_map[MAXCPU];
uint32_t xlr_core_cpu_mask = 0x1;	/* Core 0 thread 0 is always there */

void
platform_reset(void)
{
	/* FIXME : use proper define */
	u_int32_t *mmio = (u_int32_t *) 0xbef18000;

	printf("Rebooting the system now\n");
	mmio[8] = 0x1;
}

void 
platform_secondary_init(void)
{
#ifdef SMP
	xlr_msgring_cpu_init();

	/* Setup interrupts for secondary CPUs here */
	mips_mask_hard_irq(IPI_SMP_CALL_FUNCTION);
	mips_mask_hard_irq(IPI_STOP);
	mips_mask_hard_irq(IPI_RENDEZVOUS);
	mips_mask_hard_irq(IPI_AST);
	mips_mask_hard_irq(IRQ_TIMER);
#ifdef XLR_PERFMON
	mips_mask_hard_irq(IPI_PERFMON);
#endif

	return;
#endif
}


int xlr_asid_pcpu = 256;	/* This the default */
int xlr_shtlb_enabled = 0;

/* This function sets up the number of tlb entries available
   to the kernel based on the number of threads brought up.
   The ASID range also gets divided similarly.
   THE NUMBER OF THREADS BROUGHT UP IN EACH CORE MUST BE THE SAME
NOTE: This function will mark all 64TLB entries as available
to the threads brought up in the core. If kernel is brought with say mask
0x33333333, no TLBs will be available to the threads in each core.
*/
static void 
setup_tlb_resource(void)
{
	int mmu_setup;
	int value = 0;
	uint32_t cpu_map = xlr_boot1_info.cpu_online_map;
	uint32_t thr_mask = cpu_map >> (xlr_cpu_id() << 2);
	uint8_t core0 = xlr_boot1_info.cpu_online_map & 0xf;
	uint8_t core_thr_mask;
	int i = 0, count = 0;

	/* If CPU0 did not enable shared TLB, other cores need to follow */
	if ((xlr_cpu_id() != 0) && (xlr_shtlb_enabled == 0))
		return;
	/* First check if each core is brought up with the same mask */
	for (i = 1; i < 8; i++) {
		core_thr_mask = cpu_map >> (i << 2);
		core_thr_mask &= 0xf;
		if (core_thr_mask && core_thr_mask != core0) {
			printf
			    ("Each core must be brought with same cpu mask\n");
			printf("Cannot enabled shared TLB. ");
			printf("Falling back to split TLB mode\n");
			return;
		}
	}

	xlr_shtlb_enabled = 1;
	for (i = 0; i < 4; i++)
		if (thr_mask & (1 << i))
			count++;
	switch (count) {
	case 1:
		xlr_asid_pcpu = 256;
		break;
	case 2:
		xlr_asid_pcpu = 128;
		value = 0x2;
		break;
	default:
		xlr_asid_pcpu = 64;
		value = 0x3;
		break;
	}

	mmu_setup = read_32bit_phnx_ctrl_reg(4, 0);
	mmu_setup = mmu_setup & ~0x06;
	mmu_setup |= (value << 1);

	/* turn on global mode */
	mmu_setup |= 0x01;

	write_32bit_phnx_ctrl_reg(4, 0, mmu_setup);

}

/*
 * Platform specific register setup for CPUs
 * XLR has control registers accessible with MFCR/MTCR instructions, this
 * code initialized them from the environment variable xlr.cr of form:
 *  xlr.cr=reg:val[,reg:val]*, all values in hex.
 * To enable shared TLB option use xlr.shtlb=1
 */
void 
platform_cpu_init()
{
	char *hw_env;
	char *start, *end;
	uint32_t reg, val;
	int thr_id = xlr_thr_id();

	if (thr_id == 0) {
		if ((hw_env = getenv("xlr.shtlb")) != NULL) {
			start = hw_env;
			reg = strtoul(start, &end, 16);
			if (start != end && reg != 0)
				setup_tlb_resource();
		} else {
			/* By default TLB entries are shared in a core */
			setup_tlb_resource();
		}
	}
	if ((hw_env = getenv("xlr.cr")) == NULL)
		return;

	start = hw_env;
	while (*start != '\0') {
		reg = strtoul(start, &end, 16);
		if (start == end) {
			printf("Invalid value in xlr.cr %s, cannot read a hex value at %d\n",
			    hw_env, start - hw_env);
			goto err_return;
		}
		if (*end != ':') {
			printf("Invalid format in xlr.cr %s, ':' expected at pos %d\n",
			    hw_env, end - hw_env);
			goto err_return;
		}
		start = end + 1;/* step over ':' */
		val = strtoul(start, &end, 16);
		if (start == end) {
			printf("Invalid value in xlr.cr %s, cannot read a hex value at pos %d\n",
			    hw_env, start - hw_env);
			goto err_return;
		}
		if (*end != ',' && *end != '\0') {
			printf("Invalid format in xlr.cr %s, ',' expected at pos %d\n",
			    hw_env, end - hw_env);
			goto err_return;
		}
		xlr_mtcr(reg, val);
		if (*end == ',')
			start = end + 1;	/* skip over ',' */
		else
			start = end;
	}
	freeenv(hw_env);
	return;

err_return:
	panic("Invalid xlr.cr setting!");
	return;
}


#ifdef SMP
extern void xlr_secondary_start(unsigned long, unsigned long, unsigned long);
static void 
xlr_secondary_entry(void *data)
{
	unsigned long sp, gp;
	unsigned int cpu = (xlr_cpu_id() << 2) + xlr_thr_id();

	sp = xlr_secondary_sp[cpu];
	gp = xlr_secondary_gp[cpu];

	xlr_secondary_start((unsigned long)mips_secondary_wait, sp, gp);
}

#endif

static void 
xlr_set_boot_flags(void)
{
	char *p;

	for (p = getenv("boot_flags"); p && *p != '\0'; p++) {
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

	if (p)
		freeenv(p);

	return;
}
extern uint32_t _end;


static void
mips_init(void)
{
	init_param1();
	init_param2(physmem);

	/* XXX: Catch 22. Something touches the tlb. */

	mips_cpu_init();
	pmap_bootstrap();

	mips_proc0_init();
	write_c0_register32(MIPS_COP_0_OSSCRATCH, 7, pcpup->pc_curthread);

	mutex_init();

	PMAP_LOCK_INIT(kernel_pmap);

#ifdef DDB
#ifdef SMP
	setup_nmi();
#endif				/* SMP */
	kdb_init();
	if (boothowto & RB_KDB) {
		kdb_enter("Boot flags requested debugger", NULL);
	}
#endif
}

void
platform_start(__register_t a0 __unused,
    __register_t a1 __unused,
    __register_t a2 __unused,
    __register_t a3 __unused)
{
	vm_size_t physsz = 0;
	int i, j;
	struct xlr_boot1_mem_map *boot_map;
#ifdef SMP
	uint32_t tmp;
	void (*wakeup) (void *, void *, unsigned int);

#endif
	/* XXX no zeroing of BSS? */

	/* Initialize pcpu stuff */
	mips_pcpu0_init();

	/* XXX FIXME the code below is not 64 bit clean */
	/* Save boot loader and other stuff from scratch regs */
	xlr_boot1_info = *(struct boot1_info *)read_c0_register32(MIPS_COP_0_OSSCRATCH, 0);
	cpu_mask_info = read_c0_register64(MIPS_COP_0_OSSCRATCH, 1);
	xlr_online_cpumask = read_c0_register32(MIPS_COP_0_OSSCRATCH, 2);
	xlr_run_mode = read_c0_register32(MIPS_COP_0_OSSCRATCH, 3);
	xlr_argc = read_c0_register32(MIPS_COP_0_OSSCRATCH, 4);
	xlr_argv = (char **)read_c0_register32(MIPS_COP_0_OSSCRATCH, 5);
	xlr_envp = (char **)read_c0_register32(MIPS_COP_0_OSSCRATCH, 6);

	/* TODO: Verify the magic number here */
	/* FIXMELATER: xlr_boot1_info.magic_number */

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
	mips_timer_early_init(platform_get_frequency());

	/* Init the time counter in the PIC and local putc routine*/
	rmi_early_counter_init();
	
	/* Init console please */
	cninit();
	init_static_kenv(boot1_env, sizeof(boot1_env));
	printf("Environment (from %d args):\n", xlr_argc - 1);
	if (xlr_argc == 1)
		printf("\tNone\n");
	for (i = 1; i < xlr_argc; i++) {
		char *n;

		printf("\t%s\n", xlr_argv[i]);
		n = strsep(&xlr_argv[i], "=");
		if (xlr_argv[i] == NULL)
			setenv(n, "1");
		else
			setenv(n, xlr_argv[i]);
	}

	xlr_set_boot_flags();

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
#if 0				/* FIXME TOD0 */
				phys_avail[j] = phys_avail[j + 1] = 0;
#endif
			}
			physsz += boot_map->physmem_map[i].size;
		}
	}

	/* FIXME XLR TODO */
	phys_avail[j] = phys_avail[j + 1] = 0;
	realmem = physmem = btoc(physsz);

	/* Store pcpu in scratch 5 */
	write_c0_register32(MIPS_COP_0_OSSCRATCH, 5, pcpup);

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
			 * Oopps.. thread 0 is not available. Disable whole
			 * core
			 */
			tmp = tmp & ~(0xf << i);
			printf("WARNING: Core %d is disabled because thread 0"
			    " of this core is not enabled.\n", i / 4);
		}
	}
	xlr_boot1_info.cpu_online_map = tmp;

	/* Wakeup Other cpus, and put them in bsd park code. */
	for (i = 1, j = 1; i < 32; i++) {
		/* Allocate stack for all other cpus from fbsd kseg0 memory. */
		if ((1U << i) & xlr_boot1_info.cpu_online_map) {
			xlr_secondary_gp[i] =
			    pmap_steal_memory(PAGE_SIZE);
			if (!xlr_secondary_gp[i])
				panic("Allocation failed for secondary cpu stacks");
			xlr_secondary_sp[i] =
			    xlr_secondary_gp[i] + PAGE_SIZE - CALLFRAME_SIZ;
			xlr_secondary_gp[i] = (unsigned long)&_gp;
			/* Build ltop and ptol cpu map. */
			cpu_ltop_map[j] = i;
			cpu_ptol_map[i] = j;
			if ((i & 0x3) == 0)	/* store thread0 of each core */
				xlr_core_cpu_mask |= (1 << j);
			mips_cpu_logical_mask |= (1 << j);
			j++;
		}
	}

	mips_cpu_online_mask |= xlr_boot1_info.cpu_online_map;
	wakeup = ((void (*) (void *, void *, unsigned int))
	    (unsigned long)(xlr_boot1_info.wakeup));
	printf("Waking up CPUs 0x%llx.\n", xlr_boot1_info.cpu_online_map & ~(0x1U));
	if (xlr_boot1_info.cpu_online_map & ~(0x1U))
		wakeup(xlr_secondary_entry, 0,
		    (unsigned int)xlr_boot1_info.cpu_online_map);
#endif

	/* xlr specific post initialization */
	/*
	 * The expectation is that mutex_init() is already done in
	 * mips_init() XXX NOTE: We may need to move this to SMP based init
	 * code for each CPU, later.
	 */
	rmi_spin_mutex_safe = 1;
	on_chip_init();
	mips_timer_init_params(platform_get_frequency(), 0);
	printf("Platform specific startup now completes\n");
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
platform_trap_exit(void)
{
}


/*
 void
 platform_update_intrmask(int intr)
 {
   write_c0_eimr64(read_c0_eimr64() | (1ULL<<intr));
 }
*/

void 
disable_msgring_int(void *arg);
void 
enable_msgring_int(void *arg);
void xlr_msgring_handler(struct trapframe *tf);
void msgring_process_fast_intr(void *arg);

struct msgring_ithread {
	struct thread *i_thread;
	u_int i_pending;
	u_int i_flags;
	int i_cpu;
};
struct msgring_ithread msgring_ithreads[MAXCPU];
char ithd_name[MAXCPU][32];

void
msgring_process_fast_intr(void *arg)
{
	int cpu = PCPU_GET(cpuid);
	volatile struct msgring_ithread *it;
	struct proc *p;
	struct thread *td;

	/* wakeup an appropriate intr_thread for processing this interrupt */
	it = (volatile struct msgring_ithread *)&msgring_ithreads[cpu];
	td = it->i_thread;
	p = td->td_proc;

	/*
	 * Interrupt thread will enable the interrupts after processing all
	 * messages
	 */
	disable_msgring_int(NULL);
	it->i_pending = 1;
	if (TD_AWAITING_INTR(td)) {
		thread_lock(td);
		CTR3(KTR_INTR, "%s: schedule pid %d (%s)", __func__, p->p_pid,
		    p->p_comm);
		TD_CLR_IWAIT(td);
		sched_add(td, SRQ_INTR);
		thread_unlock(td);
	} else {
		CTR4(KTR_INTR, "%s: pid %d (%s): state %d",
		    __func__, p->p_pid, p->p_comm, td->td_state);
	}

}

#define MIT_DEAD 4
static void
msgring_process(void *arg)
{
	volatile struct msgring_ithread *ithd;
	struct thread *td;
	struct proc *p;

	td = curthread;
	p = td->td_proc;
	ithd = (volatile struct msgring_ithread *)arg;
	KASSERT(ithd->i_thread == td,
	    ("%s:msg_ithread and proc linkage out of sync", __func__));

	/* First bind this thread to the right CPU */
	thread_lock(td);
	sched_bind(td, ithd->i_cpu);
	thread_unlock(td);

	//printf("Started %s on CPU %d\n", __FUNCTION__, ithd->i_cpu);

	while (1) {
		if (ithd->i_flags & MIT_DEAD) {
			CTR3(KTR_INTR, "%s: pid %d (%s) exiting", __func__,
			    p->p_pid, p->p_comm);
			kthread_exit();
		}
		while (ithd->i_pending) {
			/*
			 * This might need a full read and write barrier to
			 * make sure that this write posts before any of the
			 * memory or device accesses in the handlers.
			 */
			atomic_store_rel_int(&ithd->i_pending, 0);
			xlr_msgring_handler(NULL);
		}
		if (!ithd->i_pending && !(ithd->i_flags & MIT_DEAD)) {
			thread_lock(td);
			sched_class(td, PRI_ITHD);
			TD_SET_IWAIT(td);
			thread_unlock(td);
			enable_msgring_int(NULL);
			mi_switch(SW_VOL, NULL);
		}
	}

}
void 
platform_prep_smp_launch(void)
{
	int cpu;
	uint32_t cpu_mask;
	struct msgring_ithread *ithd;
	struct thread *td;
	struct proc *p;
	int error;

	cpu_mask = PCPU_GET(cpumask) | PCPU_GET(other_cpus);

	/* Create kernel threads for message ring interrupt processing */
	/* Currently create one task for thread 0 of each core */
	for (cpu = 0; cpu < MAXCPU; cpu += 1) {

		if (!((1 << cpu) & cpu_mask))
			continue;

		if ((cpu_ltop_map[cpu] % 4) != 0)
			continue;

		ithd = &msgring_ithreads[cpu];
		sprintf(ithd_name[cpu], "msg_intr%d", cpu);
		error = kproc_create(msgring_process,
		    (void *)ithd,
		    &p,
		    (RFSTOPPED | RFHIGHPID),
		    2,
		    ithd_name[cpu]);

		if (error)
			panic("kproc_create() failed with %d", error);
		td = FIRST_THREAD_IN_PROC(p);	/* XXXKSE */

		thread_lock(td);
		sched_class(td, PRI_ITHD);
		TD_SET_IWAIT(td);
		thread_unlock(td);
		ithd->i_thread = td;
		ithd->i_pending = 0;
		ithd->i_cpu = cpu;
		CTR2(KTR_INTR, "%s: created %s", __func__, ithd_name[cpu]);
	}
}
