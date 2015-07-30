/*-
 * Copyright (c) 2014 Ganbold Tsagaankhuu <ganbold@freebsd.org>
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

#include <machine/smp.h>
#include <machine/fdt.h>
#include <machine/intr.h>

#define	CPUCFG_BASE		0x01c25c00
#define	CPUCFG_SIZE		0x400

#define	CPU0_RST_CTL		0x40
#define	CPU0_CTL		0x44
#define	CPU0_STATUS		0x48
#define	CPU1_RST_CTL		0x80
#define	CPU1_CTL		0x84
#define	CPU1_STATUS		0x88
#define	CPUCFG_GENCTL		0x184
#define	CPUCFG_P_REG0		0x1a4
#define	CPU1_PWR_CLAMP		0x1b0
#define	CPU1_PWROFF_REG		0x1b4
#define	CPUCFG_DBGCTL0		0x1e0
#define	CPUCFG_DBGCTL1		0x1e4

void
platform_mp_init_secondary(void)
{

	gic_init_secondary();
}

void
platform_mp_setmaxid(void)
{
	int ncpu;

	if (mp_ncpus != 0)
		return;

	/* Read the number of cores from the CP15 L2 Control Register. */
	__asm __volatile("mrc p15, 1, %0, c9, c0, 2" : "=r" (ncpu));
	ncpu = ((ncpu >> 24) & 0x3) + 1;

	mp_ncpus = ncpu;
	mp_maxid = ncpu - 1;
}

int
platform_mp_probe(void)
{

	if (mp_ncpus == 0)
		platform_mp_setmaxid();

	return (mp_ncpus > 1);
}

void
platform_mp_start_ap(void)
{
	bus_space_handle_t cpucfg;

	uint32_t val;

	if (bus_space_map(fdtbus_bs_tag, CPUCFG_BASE, CPUCFG_SIZE, 0,
	    &cpucfg) != 0)
		panic("Couldn't map the CPUCFG\n");

	cpu_idcache_wbinv_all();
	cpu_l2cache_wbinv_all();

	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPUCFG_P_REG0,
	    pmap_kextract((vm_offset_t)mpentry));

	/*
	 * Assert nCOREPORESET low and set L1RSTDISABLE low.
	 * Ensure DBGPWRDUP is set to LOW to prevent any external
	 * debug access to the processor.
	 */
	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPU1_RST_CTL, 0);

	/* Set L1RSTDISABLE low */
	val = bus_space_read_4(fdtbus_bs_tag, cpucfg, CPUCFG_GENCTL);
	val &= ~(1 << 1);
	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPUCFG_GENCTL, val);

	/* Set DBGPWRDUP low */
	val = bus_space_read_4(fdtbus_bs_tag, cpucfg, CPUCFG_DBGCTL1);
	val &= ~(1 << 1);
	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPUCFG_DBGCTL1, val);

	/* Release power clamp */
	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPU1_PWR_CLAMP, 0xff);
	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPU1_PWR_CLAMP, 0x7f);
	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPU1_PWR_CLAMP, 0x3f);
	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPU1_PWR_CLAMP, 0x1f);
	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPU1_PWR_CLAMP, 0x0f);
	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPU1_PWR_CLAMP, 0x07);
	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPU1_PWR_CLAMP, 0x03);
	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPU1_PWR_CLAMP, 0x01);
	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPU1_PWR_CLAMP, 0x00);
	DELAY(10000);

	/* Clear power-off gating */
	val = bus_space_read_4(fdtbus_bs_tag, cpucfg, CPU1_PWROFF_REG);
	val &= ~(1 << 0);
	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPU1_PWROFF_REG, val);
	DELAY(1000);

	/* De-assert cpu core reset */
	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPU1_RST_CTL, 3);

	/* Assert DBGPWRDUP signal */
	val = bus_space_read_4(fdtbus_bs_tag, cpucfg, CPUCFG_DBGCTL1);
	val |= (1 << 1);
	bus_space_write_4(fdtbus_bs_tag, cpucfg, CPUCFG_DBGCTL1, val);

	armv7_sev();
	bus_space_unmap(fdtbus_bs_tag, cpucfg, CPUCFG_SIZE);
}

void
platform_ipi_send(cpuset_t cpus, u_int ipi)
{

	pic_ipi_send(cpus, ipi);
}
