/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011-2012 Semihalf.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/eventhandler.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/machdep.h>

#include <dev/fdt/fdt_common.h>

#include <powerpc/mpc85xx/mpc85xx.h>

extern void dcache_enable(void);
extern void dcache_inval(void);
extern void icache_enable(void);
extern void icache_inval(void);
extern void l2cache_enable(void);
extern void l2cache_inval(void);
extern void bpred_enable(void);

void
booke_enable_l1_cache(void)
{
	uint32_t csr;

	/* Enable D-cache if applicable */
	csr = mfspr(SPR_L1CSR0);
	if ((csr & L1CSR0_DCE) == 0) {
		dcache_inval();
		dcache_enable();
	}

	csr = mfspr(SPR_L1CSR0);
	if ((boothowto & RB_VERBOSE) != 0 || (csr & L1CSR0_DCE) == 0)
		printf("L1 D-cache %sabled\n",
		    (csr & L1CSR0_DCE) ? "en" : "dis");

	/* Enable L1 I-cache if applicable. */
	csr = mfspr(SPR_L1CSR1);
	if ((csr & L1CSR1_ICE) == 0) {
		icache_inval();
		icache_enable();
	}

	csr = mfspr(SPR_L1CSR1);
	if ((boothowto & RB_VERBOSE) != 0 || (csr & L1CSR1_ICE) == 0)
		printf("L1 I-cache %sabled\n",
		    (csr & L1CSR1_ICE) ? "en" : "dis");
}

void
booke_enable_l2_cache(void)
{
	uint32_t csr;

	/* Enable L2 cache on E500mc */
	if ((((mfpvr() >> 16) & 0xFFFF) == FSL_E500mc) ||
	    (((mfpvr() >> 16) & 0xFFFF) == FSL_E5500)) {
		csr = mfspr(SPR_L2CSR0);
		/*
		 * Don't actually attempt to manipulate the L2 cache if
		 * L2CFG0 is zero.
		 *
		 * Any chip with a working L2 cache will have a nonzero
		 * L2CFG0, as it will have a nonzero L2CSIZE field.
		 *
		 * This fixes waiting forever for cache enable in qemu,
		 * which does not implement the L2 cache.
		 */
		if (mfspr(SPR_L2CFG0) != 0 && (csr & L2CSR0_L2E) == 0) {
			l2cache_inval();
			l2cache_enable();
		}

		csr = mfspr(SPR_L2CSR0);
		if ((boothowto & RB_VERBOSE) != 0 || (csr & L2CSR0_L2E) == 0)
			printf("L2 cache %sabled\n",
			    (csr & L2CSR0_L2E) ? "en" : "dis");
	}
}

void
booke_enable_bpred(void)
{
	uint32_t csr;

	bpred_enable();
	csr = mfspr(SPR_BUCSR);
	if ((boothowto & RB_VERBOSE) != 0 || (csr & BUCSR_BPEN) == 0)
		printf("Branch Predictor %sabled\n",
		    (csr & BUCSR_BPEN) ? "en" : "dis");
}

void
booke_disable_l2_cache(void)
{
}

/* Return 0 on handled success, otherwise signal number. */
int
cpu_machine_check(struct thread *td, struct trapframe *frame, int *ucode)
{

	*ucode = BUS_OBJERR;
	return (SIGBUS);
}

/*
 * Book-E watchdog timer is a simple check of a single bit in the timebase
 * register.  When that bit rolls over from 0 to 1 the state machine activates.
 * In our case, we want it to trigger an interrupt to the core first, then
 * reboot on the second interrupt.
 *
 * With all PowerPC numbering, 0 is the MSB, and 63 is LSB.
 */
/* Arg is the timebase bit number 1-based (flsll result) */
static void
booke_watchdog_cpu(void *arg)
{
	uint32_t tcr;
	int bitno = (uintptr_t)arg;

	/* First pet the watchdog */
	mtspr(SPR_TSR, TSR_ENW | TSR_WIS);

	tcr = mfspr(SPR_TCR);
	tcr &= ~(TCR_WP_MASK | TCR_WPEXT_MASK);
	tcr |= TCR_MAKE_WP(bitno);

	tcr |= TCR_WRC_CHIP | TCR_WIE;

	mtspr(SPR_TCR, tcr);
}

static void
booke_watchdog_fn(void *priv, sbintime_t sbt, sbintime_t *esbt, int *err)
{
	struct cpuref cpuref;
	uintptr_t tb_bit;
	uint64_t freq, tb, ticks;

	/* Once enabled it cannot be disabled */
	if (sbt == 0) {
		*err = EOPNOTSUPP;
		return;
	}
	cpuref.cr_hwref = 0;
	cpuref.cr_cpuid = 0;
	freq = platform_timebase_freq(&cpuref);
	ticks = 1000000000 / freq;	/* Ticks/s -> ns/tick */
	ticks = sbttons(sbt) / ticks;

	/*
	 * To get the next rollover bit add the current timbase to the tick
	 * count, using only a mask of the current timebase matching the tick
	 * size.  This will give us the next rollover bit *beyond* the timeout.
	 */
	tb = mftb() & ((1 << flsll(ticks)) - 1);
	tb += ticks;

	tb_bit = 64 - flsll(tb);

	smp_rendezvous(NULL, booke_watchdog_cpu, NULL, (void *)tb_bit);
	*err = 0;
}

static void
booke_watchdog_register(void *arg)
{
	printf("Registering booke watchdog timer\n");
	EVENTHANDLER_REGISTER(watchdog_sbt_list, booke_watchdog_fn, NULL, 0);
}

SYSINIT(booke_watchdog, SI_SUB_LAST, SI_ORDER_ANY, booke_watchdog_register, NULL);
