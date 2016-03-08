/*-
 * Copyright (c) 2014 Ganbold Tsagaankhuu <ganbold@freebsd.org>
 * Copyright (c) 2016 Emmanuel Vadot <manu@bidouilliste.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/smp.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/cpu.h>
#include <machine/cpu-v6.h>
#include <machine/smp.h>
#include <machine/fdt.h>
#include <machine/intr.h>
#include <machine/platformvar.h>

#include <arm/allwinner/aw_mp.h>
#include <arm/allwinner/allwinner_machdep.h>

/* Register for all dual-core SoC */
#define	A20_CPUCFG_BASE		0x01c25c00
/* Register for all quad-core SoC */
#define	CPUCFG_BASE		0x01f01c00
#define	CPUCFG_SIZE		0x400
#define	PRCM_BASE		0x01f01400
#define	PRCM_SIZE		0x800

#define	CPU_OFFSET		0x40
#define	CPU_OFFSET_CTL		0x04
#define	CPU_OFFSET_STATUS	0x08
#define	CPU_RST_CTL(cpuid)	((cpuid + 1) * CPU_OFFSET)
#define	CPU_CTL(cpuid)		(((cpuid + 1) * CPU_OFFSET) + CPU_OFFSET_CTL)
#define	CPU_STATUS(cpuid)	(((cpuid + 1) * CPU_OFFSET) + CPU_OFFSET_STATUS)

#define	CPU_RESET		(1 << 0)
#define	CPU_CORE_RESET		(1 << 1)

#define	CPUCFG_GENCTL		0x184
#define	CPUCFG_P_REG0		0x1a4

#define	A20_CPU1_PWR_CLAMP	0x1b0
#define	CPU_PWR_CLAMP_REG	0x140
#define	CPU_PWR_CLAMP(cpu)	((cpu * 4) + CPU_PWR_CLAMP_REG)
#define	CPU_PWR_CLAMP_STEPS	8

#define	A20_CPU1_PWROFF_REG	0x1b4
#define	CPU_PWROFF		0x100

#define	CPUCFG_DBGCTL0		0x1e0
#define	CPUCFG_DBGCTL1		0x1e4

void
aw_mp_setmaxid(platform_t plat)
{
	int ncpu;
	uint32_t reg;

	if (mp_ncpus != 0)
		return;

	reg = cp15_l2ctlr_get();
	ncpu = CPUV7_L2CTLR_NPROC(reg);

	mp_ncpus = ncpu;
	mp_maxid = ncpu - 1;
}

static void
aw_common_mp_start_ap(bus_space_handle_t cpucfg, bus_space_handle_t prcm)
{
	int i, j;
	uint32_t val;

	dcache_wbinv_poc_all();

	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPUCFG_P_REG0,
	    pmap_kextract((vm_offset_t)mpentry));

	/*
	 * Assert nCOREPORESET low and set L1RSTDISABLE low.
	 * Ensure DBGPWRDUP is set to LOW to prevent any external
	 * debug access to the processor.
	 */
	for (i = 1; i < mp_ncpus; i++)
		bus_space_write_4(fdtbus_bs_tag, cpucfg, CPU_RST_CTL(i), 0);

	/* Set L1RSTDISABLE low */
	val = bus_space_read_4(fdtbus_bs_tag, cpucfg, CPUCFG_GENCTL);
	for (i = 1; i < mp_ncpus; i++)
		val &= ~(1 << i);
	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPUCFG_GENCTL, val);

	/* Set DBGPWRDUP low */
	val = bus_space_read_4(fdtbus_bs_tag, cpucfg, CPUCFG_DBGCTL1);
	for (i = 1; i < mp_ncpus; i++)
		val &= ~(1 << i);
	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPUCFG_DBGCTL1, val);

	/* Release power clamp */
	for (i = 1; i < mp_ncpus; i++)
		for (j = 0; j <= CPU_PWR_CLAMP_STEPS; j++) {
			if (prcm) {
				bus_space_write_4(fdtbus_bs_tag, prcm,
				    CPU_PWR_CLAMP(i), 0xff >> j);
			} else {
				bus_space_write_4(fdtbus_bs_tag,
				    cpucfg, A20_CPU1_PWR_CLAMP, 0xff >> j);
			}
		}
	DELAY(10000);

	/* Clear power-off gating */
	if (prcm) {
		val = bus_space_read_4(fdtbus_bs_tag, prcm, CPU_PWROFF);
		for (i = 0; i < mp_ncpus; i++)
			val &= ~(1 << i);
		bus_space_write_4(fdtbus_bs_tag, prcm, CPU_PWROFF, val);
	} else {
		val = bus_space_read_4(fdtbus_bs_tag,
		    cpucfg, A20_CPU1_PWROFF_REG);
		val &= ~(1 << 0);
		bus_space_write_4(fdtbus_bs_tag, cpucfg,
		    A20_CPU1_PWROFF_REG, val);
	}
	DELAY(1000);

	/* De-assert cpu core reset */
	for (i = 1; i < mp_ncpus; i++)
		bus_space_write_4(fdtbus_bs_tag, cpucfg, CPU_RST_CTL(i),
		    CPU_RESET | CPU_CORE_RESET);

	/* Assert DBGPWRDUP signal */
	val = bus_space_read_4(fdtbus_bs_tag, cpucfg, CPUCFG_DBGCTL1);
	for (i = 1; i < mp_ncpus; i++)
		val |= (1 << i);
	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPUCFG_DBGCTL1, val);

	armv7_sev();
	bus_space_unmap(fdtbus_bs_tag, cpucfg, CPUCFG_SIZE);
}

void
a20_mp_start_ap(platform_t plat)
{
	bus_space_handle_t cpucfg;

	if (bus_space_map(fdtbus_bs_tag, A20_CPUCFG_BASE, CPUCFG_SIZE,
	    0, &cpucfg) != 0)
		panic("Couldn't map the CPUCFG\n");

	aw_common_mp_start_ap(cpucfg, 0);
	armv7_sev();
	bus_space_unmap(fdtbus_bs_tag, cpucfg, CPUCFG_SIZE);
}

void
a31_mp_start_ap(platform_t plat)
{
	bus_space_handle_t cpucfg;
	bus_space_handle_t prcm;

	if (bus_space_map(fdtbus_bs_tag, CPUCFG_BASE, CPUCFG_SIZE,
	    0, &cpucfg) != 0)
		panic("Couldn't map the CPUCFG\n");
	if (bus_space_map(fdtbus_bs_tag, PRCM_BASE, PRCM_SIZE, 0,
	    &prcm) != 0)
		panic("Couldn't map the PRCM\n");

	aw_common_mp_start_ap(cpucfg, prcm);
	armv7_sev();
	bus_space_unmap(fdtbus_bs_tag, cpucfg, CPUCFG_SIZE);
	bus_space_unmap(fdtbus_bs_tag, prcm, PRCM_SIZE);
}
