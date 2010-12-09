/*-
 * Copyright (C) 2007 by Oleksandr Tymoshenko. All rights reserved.
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
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: $
 * 
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <machine/trap.h>
#include <machine/vmparam.h>

extern int	*edata;
extern int	*end;

void
platform_cpu_init()
{
	/* Nothing special */
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
	volatile unsigned int * p = (void *)0xb8008000;
	/* 
	 * TODO: we should take care of TLB stuff here. Otherwise
	 * board does not boots properly next time
	 */

	/* Write 0x8000_0001 to the Reset register */
	*p = 0x80000001;

	__asm __volatile("li	$25, 0xbfc00000");
	__asm __volatile("j	$25");
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
platform_start(__register_t a0, __register_t a1,
    __register_t a2 __unused, __register_t a3 __unused)
{
	uint64_t platform_counter_freq;
	vm_offset_t kernend;
	int argc = a0;
	char **argv = (char **)a1;
	int i, mem;


	/* clear the BSS and SBSS segments */
	kernend = (vm_offset_t)&end;
	memset(&edata, 0, kernend - (vm_offset_t)(&edata));

	mips_postboot_fixup();

	/* Initialize pcpu stuff */
	mips_pcpu0_init();

	/*
	 * Looking for mem=XXM argument
	 */
	mem = 0; /* Just something to start with */
	for (i=0; i < argc; i++) {
		if (strncmp(argv[i], "mem=", 4) == 0) {
			mem = strtol(argv[i] + 4, NULL, 0);
			break;
		}
	}

	bootverbose = 1;
	if (mem > 0)
		realmem = btoc(mem << 20);
	else
		realmem = btoc(32 << 20);

	for (i = 0; i < 10; i++) {
		phys_avail[i] = 0;
	}

	/* phys_avail regions are in bytes */
	phys_avail[0] = MIPS_KSEG0_TO_PHYS(kernel_kseg0_end);
	phys_avail[1] = ctob(realmem);

	dump_avail[0] = phys_avail[0];
	dump_avail[1] = phys_avail[1] - phys_avail[0];

	physmem = realmem;

	/* 
	 * ns8250 uart code uses DELAY so ticker should be inititalized 
	 * before cninit. And tick_init_params refers to hz, so * init_param1 
	 * should be called first.
	 */
	init_param1();
	/* TODO: parse argc,argv */
	platform_counter_freq = 330000000UL;
	mips_timer_init_params(platform_counter_freq, 1);
	cninit();
	/* Panic here, after cninit */ 
	if (mem == 0)
		panic("No mem=XX parameter in arguments");

	printf("cmd line: ");
	for (i=0; i < argc; i++)
		printf("%s ", argv[i]);
	printf("\n");

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
