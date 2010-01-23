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

void
platform_cpu_init()
{
	/* Nothing special */
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

void
platform_start(__register_t a0, __register_t a1, __register_t a2,
	       __register_t a3)
{
	vm_offset_t kernend;

	/*
	 * Make sure that kseg0 is mapped cacheable-coherent
	 */
	kseg0_map_coherent();

	/* clear the BSS and SBSS segments */
	memset(&edata, 0, (vm_offset_t)&end - (vm_offset_t)&edata);
	kernend = round_page((vm_offset_t)&end);

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
}
