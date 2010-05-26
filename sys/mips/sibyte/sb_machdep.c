/*-
 * Copyright (c) 2007 Bruce M. Simpson.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <machine/cpuregs.h>

#include "opt_ddb.h"
#include "opt_kdb.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/imgact.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/cons.h>
#include <sys/exec.h>
#include <sys/ucontext.h>
#include <sys/proc.h>
#include <sys/kdb.h>
#include <sys/ptrace.h>
#include <sys/reboot.h>
#include <sys/signalvar.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/user.h>
#include <sys/timetc.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

#include <machine/cache.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/cpuinfo.h>
#include <machine/cpufunc.h>
#include <machine/cpuregs.h>
#include <machine/hwfunc.h>
#include <machine/intr_machdep.h>
#include <machine/locore.h>
#include <machine/md_var.h>
#include <machine/pte.h>
#include <machine/sigframe.h>
#include <machine/trap.h>
#include <machine/vmparam.h>

#ifdef SMP
#include <sys/smp.h>
#include <machine/smp.h>
#endif

#ifdef CFE
#include <dev/cfe/cfe_api.h>
#endif

#include "sb_scd.h"

#ifdef DDB
#ifndef KDB
#error KDB must be enabled in order for DDB to work!
#endif
#endif

#ifdef CFE_ENV
extern void cfe_env_init(void);
#endif

extern int *edata;
extern int *end;

extern char MipsTLBMiss[], MipsTLBMissEnd[];

void
platform_cpu_init()
{
	/* Nothing special */
}

static void
sb_intr_init(int cpuid)
{
	int intrnum, intsrc;

	/*
	 * Disable all sources to the interrupt mapper and setup the mapping
	 * between an interrupt source and the mips hard interrupt number.
	 */
	for (intsrc = 0; intsrc < NUM_INTSRC; ++intsrc) {
		intrnum = sb_route_intsrc(intsrc);
		sb_disable_intsrc(cpuid, intsrc);
		sb_write_intmap(cpuid, intsrc, intrnum);
#ifdef SMP
		/*
		 * Set up the mailbox interrupt mapping.
		 *
		 * The mailbox interrupt is "special" in that it is not shared
		 * with any other interrupt source.
		 */
		if (intsrc == INTSRC_MAILBOX3) {
			intrnum = platform_ipi_intrnum();
			sb_write_intmap(cpuid, INTSRC_MAILBOX3, intrnum);
			sb_enable_intsrc(cpuid, INTSRC_MAILBOX3);
		}
#endif
	}
}

static void
mips_init(void)
{
	int i, cfe_mem_idx, tmp;
	uint64_t maxmem;

#ifdef CFE_ENV
	cfe_env_init();
#endif

	TUNABLE_INT_FETCH("boothowto", &boothowto);

	if (boothowto & RB_VERBOSE)
		bootverbose++;

#ifdef MAXMEM
	tmp = MAXMEM;
#else
	tmp = 0;
#endif
	TUNABLE_INT_FETCH("hw.physmem", &tmp);
	maxmem = (uint64_t)tmp * 1024;

	/*
	 * XXX
	 * If we used vm_paddr_t consistently in pmap, etc., we could
	 * use 64-bit page numbers on !n64 systems, too, like i386
	 * does with PAE.
	 */
#if !defined(__mips_n64)
	if (maxmem == 0 || maxmem > 0xffffffff)
		maxmem = 0xffffffff;
#endif

#ifdef CFE
	/*
	 * Query DRAM memory map from CFE.
	 */
	physmem = 0;
	cfe_mem_idx = 0;
	for (i = 0; i < 10; i += 2) {
		int result;
		uint64_t addr, len, type;

		result = cfe_enummem(cfe_mem_idx++, 0, &addr, &len, &type);
		if (result < 0) {
			phys_avail[i] = phys_avail[i + 1] = 0;
			break;
		}

		KASSERT(type == CFE_MI_AVAILABLE,
			("CFE DRAM region is not available?"));

		if (bootverbose)
			printf("cfe_enummem: 0x%016jx/%llu.\n", addr, len);

		if (maxmem != 0) {
			if (addr >= maxmem) {
				printf("Ignoring %llu bytes of memory at 0x%jx "
				       "that is above maxmem %dMB\n",
				       len, addr,
				       (int)(maxmem / (1024 * 1024)));
				continue;
			}

			if (addr + len > maxmem) {
				printf("Ignoring %llu bytes of memory "
				       "that is above maxmem %dMB\n",
				       (addr + len) - maxmem,
				       (int)(maxmem / (1024 * 1024)));
				len = maxmem - addr;
			}
		}

		phys_avail[i] = addr;
		if (i == 0 && addr == 0) {
			/*
			 * If this is the first physical memory segment probed
			 * from CFE, omit the region at the start of physical
			 * memory where the kernel has been loaded.
			 */
			phys_avail[i] += MIPS_KSEG0_TO_PHYS(kernel_kseg0_end);
		}
		phys_avail[i + 1] = addr + len;
		physmem += len;
	}

	realmem = btoc(physmem);
#endif

	physmem = realmem;

	init_param1();
	init_param2(physmem);
	mips_cpu_init();

	/*
	 * Sibyte has a L1 data cache coherent with DMA. This includes
	 * on-chip network interfaces as well as PCI/HyperTransport bus
	 * masters.
	 */
	cpuinfo.cache_coherent_dma = TRUE;

	/*
	 * XXX
	 * The kernel is running in 32-bit mode but the CFE is running in
	 * 64-bit mode. So the SR_KX bit in the status register is turned
	 * on by the CFE every time we call into it - for e.g. CFE_CONSOLE.
	 *
	 * This means that if get a TLB miss for any address above 0xc0000000
	 * and the SR_KX bit is set then we will end up in the XTLB exception
	 * vector.
	 *
	 * For now work around this by copying the TLB exception handling
	 * code to the XTLB exception vector.
	 */
	{
		bcopy(MipsTLBMiss, (void *)XTLB_MISS_EXC_VEC,
		      MipsTLBMissEnd - MipsTLBMiss);

		mips_icache_sync_all();
		mips_dcache_wbinv_all();
	}

	pmap_bootstrap();
	mips_proc0_init();
	mutex_init();

	kdb_init();
#ifdef KDB
	if (boothowto & RB_KDB)
		kdb_enter(KDB_WHY_BOOTFLAGS, "Boot flags requested debugger");
#endif
}

void
platform_halt(void)
{

}


void
platform_identify(void)
{

}

void
platform_reset(void)
{
	
	/*
	 * XXX SMP
	 * XXX flush data caches
	 */
	sb_system_reset();
}

void
platform_trap_enter(void)
{

}

void
platform_trap_exit(void)
{

}

static void
kseg0_map_coherent(void)
{
	uint32_t config;
	const int CFG_K0_COHERENT = 5;

	config = mips_rd_config();
	config &= ~CFG_K0_MASK;
	config |= CFG_K0_COHERENT;
	mips_wr_config(config);
}

#ifdef SMP
void
platform_ipi_send(int cpuid)
{
	KASSERT(cpuid == 0 || cpuid == 1,
		("platform_ipi_send: invalid cpuid %d", cpuid));

	sb_set_mailbox(cpuid, 1ULL);
}

void
platform_ipi_clear(void)
{
	int cpuid;

	cpuid = PCPU_GET(cpuid);
	sb_clear_mailbox(cpuid, 1ULL);
}

int
platform_ipi_intrnum(void)
{

	return (4);
}

struct cpu_group *
platform_smp_topo(void)
{

	return (smp_topo_none());
}

void
platform_init_ap(int cpuid)
{
	int ipi_int_mask, clock_int_mask;

	KASSERT(cpuid == 1, ("AP has an invalid cpu id %d", cpuid));

	/*
	 * Make sure that kseg0 is mapped cacheable-coherent
	 */
	kseg0_map_coherent();

	sb_intr_init(cpuid);

	/*
	 * Unmask the clock and ipi interrupts.
	 */
	clock_int_mask = hard_int_mask(5);
	ipi_int_mask = hard_int_mask(platform_ipi_intrnum());
	set_intr_mask(ALL_INT_MASK & ~(ipi_int_mask | clock_int_mask));
}

int
platform_start_ap(int cpuid)
{
#ifdef CFE
	int error;

	if ((error = cfe_cpu_start(cpuid, mpentry, 0, 0, 0))) {
		printf("cfe_cpu_start error: %d\n", error);
		return (-1);
	} else {
		return (0);
	}
#else
	return (-1);
#endif	/* CFE */
}
#endif	/* SMP */

static u_int
sb_get_timecount(struct timecounter *tc)
{

	return ((u_int)sb_zbbus_cycle_count());
}

static void
sb_timecounter_init(void)
{
	static struct timecounter sb_timecounter = {
		sb_get_timecount,
		NULL,
		~0u,
		0,
		"sibyte_zbbus_counter",
		2000
	};

	/*
	 * The ZBbus cycle counter runs at half the cpu frequency.
	 */
	sb_timecounter.tc_frequency = sb_cpu_speed() / 2;
	platform_timecounter = &sb_timecounter;
}

void
platform_start(__register_t a0, __register_t a1, __register_t a2,
	       __register_t a3)
{
	/*
	 * Make sure that kseg0 is mapped cacheable-coherent
	 */
	kseg0_map_coherent();

	/* clear the BSS and SBSS segments */
	memset(&edata, 0, (vm_offset_t)&end - (vm_offset_t)&edata);
	mips_postboot_fixup();

	sb_intr_init(0);
	sb_timecounter_init();

	/* Initialize pcpu stuff */
	mips_pcpu0_init();

#ifdef CFE
	/*
	 * Initialize CFE firmware trampolines before
	 * we initialize the low-level console.
	 *
	 * CFE passes the following values in registers:
	 * a0: firmware handle
	 * a2: firmware entry point
	 * a3: entry point seal
	 */
	if (a3 == CFE_EPTSEAL)
		cfe_init(a0, a2);
#endif
	cninit();

	mips_init();

	mips_timer_init_params(sb_cpu_speed(), 0);

	set_cputicker(sb_zbbus_cycle_count, sb_cpu_speed() / 2, 1);
}
