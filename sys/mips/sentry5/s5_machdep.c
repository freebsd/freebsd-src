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

#include <mips/sentry5/s5reg.h>

#include "opt_ddb.h"

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
#include <machine/tlb.h>
#include <machine/trap.h>
#include <machine/vmparam.h>

#ifdef CFE
#include <dev/cfe/cfe_api.h>
#endif

#ifdef CFE
extern uint32_t cfe_handle;
extern uint32_t cfe_vector;
#endif

extern int *edata;
extern int *end;

static void
mips_init(void)
{
	int i;

	printf("entry: mips_init()\n");

#ifdef CFE
	/*
	 * Query DRAM memory map from CFE.
	 */
	physmem = 0;
	for (i = 0; i < 10; i += 2) {
		int result;
		uint64_t addr, len, type;

		result = cfe_enummem(i, 0, &addr, &len, &type);
		if (result < 0) {
			phys_avail[i] = phys_avail[i + 1] = 0;
			break;
		}
		if (type != CFE_MI_AVAILABLE)
			continue;

		phys_avail[i] = addr;
		if (i == 0 && addr == 0) {
			/*
			 * If this is the first physical memory segment probed
			 * from CFE, omit the region at the start of physical
			 * memory where the kernel has been loaded.
			 */
			phys_avail[i] += MIPS_KSEG0_TO_PHYS((vm_offset_t)&end);
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
	pmap_bootstrap();
	mips_proc0_init();
	mutex_init();
#ifdef DDB
	kdb_init();
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

#if defined(CFE)
	cfe_exit(0, 0);
#else
	*((volatile uint8_t *)MIPS_PHYS_TO_KSEG1(SENTRY5_EXTIFADR)) = 0x80;
#endif
}

void
platform_trap_enter(void)
{

}

void
platform_trap_exit(void)
{

}

void
platform_start(__register_t a0 __unused, __register_t a1 __unused, 
    __register_t a2 __unused, __register_t a3 __unused)
{
	vm_offset_t kernend;
	uint64_t platform_counter_freq;

	/* clear the BSS and SBSS segments */
	kernend = round_page((vm_offset_t)&end);
	memset(&edata, 0, kernend - (vm_offset_t)(&edata));

#ifdef CFE
	/*
	 * Initialize CFE firmware trampolines before
	 * we initialize the low-level console.
	 */
	if (cfe_handle != 0)
		cfe_init(cfe_handle, cfe_vector);
#endif
	cninit();

#ifdef CFE
	if (cfe_handle == 0)
		panic("CFE was not detected by locore.\n");
#endif
	mips_init();

# if 0
	/*
	 * Probe the Broadcom Sentry5's on-chip PLL clock registers
	 * and discover the CPU pipeline clock and bus clock
	 * multipliers from this.
	 * XXX: Wrong place. You have to ask the ChipCommon
	 * or External Interface cores on the SiBa.
	 */
	uint32_t busmult, cpumult, refclock, clkcfg1;
#define S5_CLKCFG1_REFCLOCK_MASK	0x0000001F
#define S5_CLKCFG1_BUSMULT_MASK		0x000003E0
#define S5_CLKCFG1_BUSMULT_SHIFT	5
#define S5_CLKCFG1_CPUMULT_MASK		0xFFFFFC00
#define S5_CLKCFG1_CPUMULT_SHIFT	10

	counter_freq = 100000000;	/* XXX */

	clkcfg1 = s5_rd_clkcfg1();
	printf("clkcfg1 = 0x%08x\n", clkcfg1);

	refclock = clkcfg1 & 0x1F;
	busmult = ((clkcfg1 & 0x000003E0) >> 5) + 1;
	cpumult = ((clkcfg1 & 0xFFFFFC00) >> 10) + 1;

	printf("refclock = %u\n", refclock);
	printf("busmult = %u\n", busmult);
	printf("cpumult = %u\n", cpumult);

	counter_freq = cpumult * refclock;
# else
	platform_counter_freq = 200 * 1000 * 1000; /* Sentry5 is 200MHz */
# endif

	mips_timer_init_params(platform_counter_freq, 0);
}
