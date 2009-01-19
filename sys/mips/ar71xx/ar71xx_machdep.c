/*-
 * Copyright (c) 2009 Oleksandr Tymoshenko
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
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/kdb.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/hwfunc.h>
#include <machine/md_var.h>
#include <machine/trap.h>
#include <machine/vmparam.h>

#include <mips/ar71xx/ar71xxreg.h>

extern int *edata;
extern int *end;

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
        volatile unsigned int * p = 
	    (void *)MIPS_PHYS_TO_KSEG1(AR71XX_RST_RESET);

        *p = RST_RESET_CPU_COLD_RESET;
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

	/* TODO: Get available memory from RedBoot. Is it possible? */
	realmem = btoc(64*1024*1024);
        /* phys_avail regions are in bytes */
        phys_avail[0] = MIPS_KSEG0_TO_PHYS((vm_offset_t)&end);
        phys_avail[1] = ctob(realmem);

        physmem = realmem;

	/*
         * ns8250 uart code uses DELAY so ticker should be inititalized 
         * before cninit. And tick_init_params refers to hz, so * init_param1 
         * should be called first.
         */
        init_param1();
	/* TODO: Get CPU freq from RedBoot. Is it possible? */
        platform_counter_freq = 680000000UL;
        mips_timer_init_params(platform_counter_freq, 0);
        cninit();

        printf("arguments: \n");
	printf("  a0 = %08x\n", a0);
	printf("  a1 = %08x\n", a1);
	printf("  a2 = %08x\n", a2);
	printf("  a3 = %08x\n", a3);

        init_param2(physmem);
        mips_cpu_init();
        pmap_bootstrap();
        mips_proc0_init();
        mutex_init();

#ifdef DDB
        kdb_init();
#endif
}
