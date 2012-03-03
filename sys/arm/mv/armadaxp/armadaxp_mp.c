/*-
 * Copyright (c) 2011 Semihalf.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/smp.h>

#include <machine/smp.h>
#include <machine/fdt.h>

#include <arm/mv/mvwin.h>

static int platform_get_ncpus(void);

#define MV_AXP_CPU_DIVCLK_BASE		(MV_BASE + 0x18700)
#define CPU_DIVCLK_CTRL0		0x00
#define CPU_DIVCLK_CTRL2_RATIO_FULL0	0x08
#define CPU_DIVCLK_CTRL2_RATIO_FULL1	0x0c

#define MV_COHERENCY_FABRIC_BASE	(MV_MBUS_BRIDGE_BASE + 0x200)
#define COHER_FABRIC_CTRL		0x00
#define COHER_FABRIC_CONF		0x04

#define CPU_PMU(x)			(MV_BASE + 0x22100 + (0x100 * (x)))
#define CPU_PMU_BOOT			0x24

#define MP				(MV_BASE + 0x20800)
#define MP_SW_RESET(x)			((x) * 8)

#define CPU_RESUME_CONTROL		(0x20988)

/* Coherency Fabric registers */
static uint32_t
read_coher_fabric(uint32_t reg)
{

	return (bus_space_read_4(fdtbus_bs_tag, MV_COHERENCY_FABRIC_BASE, reg));
}

static void
write_coher_fabric(uint32_t reg, uint32_t val)
{

	bus_space_write_4(fdtbus_bs_tag, MV_COHERENCY_FABRIC_BASE, reg, val);
}

/* Coherency Fabric registers */
static uint32_t
read_cpu_clkdiv(uint32_t reg)
{

	return (bus_space_read_4(fdtbus_bs_tag, MV_AXP_CPU_DIVCLK_BASE, reg));
}

static void
write_cpu_clkdiv(uint32_t reg, uint32_t val)
{

	bus_space_write_4(fdtbus_bs_tag, MV_AXP_CPU_DIVCLK_BASE, reg, val);
}

void
platform_mp_setmaxid(void)
{

	mp_maxid = 3;
}

int
platform_mp_probe(void)
{

	mp_ncpus = platform_get_ncpus();

	return (mp_ncpus > 1);
}

void
platform_mp_init_secondary(void)
{
}

void mpentry(void);
void mptramp(void);

static void
initialize_coherency_fabric(void)
{
	uint32_t val, cpus, mask;

	cpus = platform_get_ncpus();
	mask = (1 << cpus) - 1;
	val = read_coher_fabric(COHER_FABRIC_CTRL);
	val |= (mask << 24);
	write_coher_fabric(COHER_FABRIC_CTRL, val);

	val = read_coher_fabric(COHER_FABRIC_CONF);
	val |= (mask << 24);
	write_coher_fabric(COHER_FABRIC_CONF, val);
}


void
platform_mp_start_ap(void)
{
	uint32_t reg, *ptr, cpu_num;

	/* Copy boot code to SRAM */
	*((unsigned int*)(0xf1020240)) = 0xffff0101;
	*((unsigned int*)(0xf1008500)) = 0xffff0003;

	pmap_kenter_nocache(0x880f0000, 0xffff0000);
	reg = 0x880f0000;

	for (ptr = (uint32_t *)mptramp; ptr < (uint32_t *)mpentry;
	    ptr++, reg += 4)
		*((uint32_t *)reg) = *ptr;

	if (mp_ncpus > 1) {
		reg = read_cpu_clkdiv(CPU_DIVCLK_CTRL2_RATIO_FULL0);
		reg &= 0x00ffffff;
		reg |= 0x01000000;
		write_cpu_clkdiv(CPU_DIVCLK_CTRL2_RATIO_FULL0, reg);
	}
	if (mp_ncpus > 2) {
		reg = read_cpu_clkdiv(CPU_DIVCLK_CTRL2_RATIO_FULL1);
		reg &= 0xff00ffff;
		reg |= 0x00010000;
		write_cpu_clkdiv(CPU_DIVCLK_CTRL2_RATIO_FULL1, reg);
	}
	if (mp_ncpus > 3) {
		reg = read_cpu_clkdiv(CPU_DIVCLK_CTRL2_RATIO_FULL1);
		reg &= 0x00ffffff;
		reg |= 0x01000000;
		write_cpu_clkdiv(CPU_DIVCLK_CTRL2_RATIO_FULL1, reg);
	}

	reg = read_cpu_clkdiv(CPU_DIVCLK_CTRL0);
	reg |= ((0x1 << (mp_ncpus - 1)) - 1) << 21;
	write_cpu_clkdiv(CPU_DIVCLK_CTRL0, reg);
	reg = read_cpu_clkdiv(CPU_DIVCLK_CTRL0);
	reg |= 0x01000000;
	write_cpu_clkdiv(CPU_DIVCLK_CTRL0, reg);

	DELAY(100);
	reg &= ~(0xf << 21);
	write_cpu_clkdiv(CPU_DIVCLK_CTRL0, reg);
	DELAY(100);

	bus_space_write_4(fdtbus_bs_tag, MV_BASE, CPU_RESUME_CONTROL, 0);

	for (cpu_num = 1; cpu_num < mp_ncpus; cpu_num++ )
		bus_space_write_4(fdtbus_bs_tag, CPU_PMU(cpu_num), CPU_PMU_BOOT,
		    pmap_kextract(mpentry));

	cpu_idcache_wbinv_all();

	for (cpu_num = 1; cpu_num < mp_ncpus; cpu_num++ )
		bus_space_write_4(fdtbus_bs_tag, MP, MP_SW_RESET(cpu_num), 0);

	/* XXX: Temporary workaround for hangup after releasing AP's */
	wmb();
	DELAY(10);

	initialize_coherency_fabric();
}

static int
platform_get_ncpus(void)
{

	return ((read_coher_fabric(COHER_FABRIC_CONF) & 0xf) + 1);
}

void
platform_ipi_send(cpuset_t cpus, u_int ipi)
{

	pic_ipi_send(cpus, ipi);
}
