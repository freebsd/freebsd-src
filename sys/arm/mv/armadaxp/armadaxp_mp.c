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

/* XXX move to separate header files*/
#define IPI_SELF		0
#define IPI_ALL			1
#define IPI_ALL_BUT_SELF	2
void mpic_ipi_send(int cpus, u_int ipi);
static int platform_get_ncpus(void);

#define MV_AXP_CPU_DIVCLK_BASE		(MV_BASE + 0x18700)
#define MV_AXP_CPU_DIVCLK_CTRL0			0x00
#define MV_AXP_CPU_DIVCLK_CTRL2_RATIO_FULL0	0x08
#define MV_AXP_CPU_DIVCLK_CTRL2_RATIO_FULL1	0x0c

#define MV_COHERENCY_FABRIC_BASE	(MV_MBUS_BRIDGE_BASE + 0x200)
#define MV_COHER_FABRIC_CTRL		0x00
#define MV_COHER_FABRIC_CONF		0x04

/* Coherency Fabric registers */
static uint32_t
read_coher_fabric(uint32_t reg)
{

	return (bus_space_read_4(fdtbus_bs_tag, MV_COHERENCY_FABRIC_BASE, reg));
}

#ifdef not_yet
static void
write_coher_fabric(uint32_t reg, uint32_t val)
{

	bus_space_write_4(fdtbus_bs_tag, MV_COHERENCY_FABRIC_BASE, reg, val);
}
#endif

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

#if 0
static void
hello_message(void)
{
	uint32_t cpuid;

	__asm __volatile("mrc p15, 0, %0, c0, c0, 5" : "=r" (cpuid));
	printf("CPU AP #%d is ready to serve you my sire.\n", cpuid);
}
#endif

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

int
platform_mp_start_ap(int cpuid)
{
	uint32_t reg;

	if (cpuid == 1) {
		if (mp_ncpus > 1) {
			reg = read_cpu_clkdiv(MV_AXP_CPU_DIVCLK_CTRL2_RATIO_FULL0);
			reg &= 0x00ffffff;
			reg |= 0x01000000;
			write_cpu_clkdiv(MV_AXP_CPU_DIVCLK_CTRL2_RATIO_FULL0, reg);
		}
		if (mp_ncpus > 2) {
			reg = read_cpu_clkdiv(MV_AXP_CPU_DIVCLK_CTRL2_RATIO_FULL1);
			reg &= 0xff00ffff;
			reg |= 0x00010000;
			write_cpu_clkdiv(MV_AXP_CPU_DIVCLK_CTRL2_RATIO_FULL1, reg);
		}
		if (mp_ncpus > 3) {
			reg = read_cpu_clkdiv(MV_AXP_CPU_DIVCLK_CTRL2_RATIO_FULL1);
			reg &= 0x00ffffff;
			reg |= 0x01000000;
			write_cpu_clkdiv(MV_AXP_CPU_DIVCLK_CTRL2_RATIO_FULL1, reg);
		}

		reg = read_cpu_clkdiv(MV_AXP_CPU_DIVCLK_CTRL0);
		reg |= ((0x1 << (mp_ncpus - 1)) - 1) << 21;
		write_cpu_clkdiv(MV_AXP_CPU_DIVCLK_CTRL0, reg);
		reg = read_cpu_clkdiv(MV_AXP_CPU_DIVCLK_CTRL0);
		reg |= 0x01000000;
		write_cpu_clkdiv(MV_AXP_CPU_DIVCLK_CTRL0, reg);

		DELAY(100);
		reg &= ~(0xf << 21);
		write_cpu_clkdiv(MV_AXP_CPU_DIVCLK_CTRL0, reg);
		DELAY(100);

	}

	return (0);
}

static int
platform_get_ncpus(void)
{

	return ((read_coher_fabric(MV_COHER_FABRIC_CONF) & 0xf) + 1);
}

#ifdef not_yet
static void
platform_ipi_send(int cpu, u_int ipi)
{

	mpic_ipi_send(cpu, ipi);
}
#endif
