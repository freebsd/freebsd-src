/*-
 * Copyright (c) 2006 Wojciech A. Koszek <wkoszek@FreeBSD.org>
 * Copyright (c) 2012 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include "opt_ddb.h"
#include "opt_platform.h"

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
#include <sys/linker.h>
#include <sys/ucontext.h>
#include <sys/proc.h>
#include <sys/kdb.h>
#include <sys/ptrace.h>
#include <sys/reboot.h>
#include <sys/signalvar.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/user.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#endif

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/cpuregs.h>
#include <machine/hwfunc.h>
#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/pmap.h>
#include <machine/trap.h>

extern int	*edata;
extern int	*end;

void
platform_cpu_init()
{
	/* Nothing special */
}

static void
mips_init(void)
{
	int i;

	for (i = 0; i < 10; i++) {
		phys_avail[i] = 0;
	}

	/* phys_avail regions are in bytes */
	phys_avail[0] = MIPS_KSEG0_TO_PHYS(kernel_kseg0_end);
	phys_avail[1] = ctob(realmem);

	dump_avail[0] = phys_avail[0];
	dump_avail[1] = phys_avail[1];

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

/*
 * Perform a board-level soft-reset.
 */
void
platform_reset(void)
{

	/* XXX SMP will likely require us to do more. */
	__asm__ __volatile__(
		"mfc0 $k0, $12\n\t"
		"li $k1, 0x00100000\n\t"
		"or $k0, $k0, $k1\n\t"
		"mtc0 $k0, $12\n");
	for( ; ; )
		__asm__ __volatile("wait");
}

void
platform_start(__register_t a0, __register_t a1,  __register_t a2, 
    __register_t a3)
{
	vm_offset_t kernend;
	uint64_t platform_counter_freq;
	int argc = a0;
	char **argv = (char **)a1;
	char **envp = (char **)a2;
	unsigned int memsize = a3;
#ifdef FDT
	vm_offset_t dtbp;
	void *kmdp;
#endif
	int i;

	/* clear the BSS and SBSS segments */
	kernend = (vm_offset_t)&end;
	memset(&edata, 0, kernend - (vm_offset_t)(&edata));

	mips_postboot_fixup();

	mips_pcpu0_init();

#ifdef FDT
	/*
	 * Find the dtb passed in by the boot loader (currently fictional).
	 */
	kmdp = preload_search_by_type("elf kernel");
	if (kmdp != NULL)
		dtbp = MD_FETCH(kmdp, MODINFOMD_DTBP, vm_offset_t);
	else
		dtbp = (vm_offset_t)NULL;

#if defined(FDT_DTB_STATIC)
	/*
	 * In case the device tree blob was not retrieved (from metadata) try
	 * to use the statically embedded one.
	 */
	if (dtbp == (vm_offset_t)NULL)
		dtbp = (vm_offset_t)&fdt_static_dtb;
#else
#error	"Non-static FDT not yet supported on BERI"
#endif

	if (OF_install(OFW_FDT, 0) == FALSE)
		while (1);
	if (OF_init(&fdt_static_dtb) != 0)
		while (1);
#endif

	/*
	 * XXXRW: We have no way to compare wallclock time to cycle rate on
	 * BERI, so for now assume we run at the MALTA default (100MHz).
	 */
	platform_counter_freq = MIPS_DEFAULT_HZ;
	mips_timer_early_init(platform_counter_freq);

	cninit();
	printf("entry: platform_start()\n");

	bootverbose = 1;
	if (bootverbose) {
		printf("cmd line: ");
		for (i = 0; i < argc; i++)
			printf("%s ", argv[i]);
		printf("\n");

		printf("envp:\n");
		for (i = 0; envp[i]; i += 2)
			printf("\t%s = %s\n", envp[i], envp[i+1]);

		printf("memsize = %08x\n", memsize);
	}

	realmem = btoc(memsize);
	mips_init();

	mips_timer_init_params(platform_counter_freq, 0);
}
