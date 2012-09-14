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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * From: FreeBSD: src/sys/arm/mv/kirkwood/sheevaplug.c,v 1.2 2010/06/13 13:28:53
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/armreg.h>

#include <arm/mv/mvreg.h>
#include <arm/mv/mvvar.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>

#include <machine/fdt.h>

#define CPU_FREQ_FIELD(sar)	(((0x01 & (sar >> 52)) << 3) | \
				    (0x07 & (sar >> 21)))
#define FAB_FREQ_FIELD(sar)	(((0x01 & (sar >> 51)) << 4) | \
				    (0x0F & (sar >> 24)))

static uint32_t count_l2clk(void);

/* XXX Make gpio driver optional and remove it */
struct resource_spec mv_gpio_res[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

struct vco_freq_ratio {
	uint8_t	vco_cpu;	/* VCO to CLK0(CPU) clock ratio */
	uint8_t	vco_l2c;	/* VCO to NB(L2 cache) clock ratio */
	uint8_t	vco_hcl;	/* VCO to HCLK(DDR controller) clock ratio */
	uint8_t	vco_ddr;	/* VCO to DR(DDR memory) clock ratio */
};

static struct vco_freq_ratio freq_conf_table[] = {
/*00*/	{ 1, 1,	 4,  2 },
/*01*/	{ 1, 2,	 2,  2 },
/*02*/	{ 2, 2,	 6,  3 },
/*03*/	{ 2, 2,	 3,  3 },
/*04*/	{ 1, 2,	 3,  3 },
/*05*/	{ 1, 2,	 4,  2 },
/*06*/	{ 1, 1,	 2,  2 },
/*07*/	{ 2, 3,	 6,  6 },
/*08*/	{ 2, 3,	 5,  5 },
/*09*/	{ 1, 2,	 6,  3 },
/*10*/	{ 2, 4,	10,  5 },
/*11*/	{ 1, 3,	 6,  6 },
/*12*/	{ 1, 2,	 5,  5 },
/*13*/	{ 1, 3,	 6,  3 },
/*14*/	{ 1, 2,	 5,  5 },
/*15*/	{ 2, 2,	 5,  5 },
/*16*/	{ 1, 1,	 3,  3 },
/*17*/	{ 2, 5,	10, 10 },
/*18*/	{ 1, 3,	 8,  4 },
/*19*/	{ 1, 1,	 2,  1 },
/*20*/	{ 2, 3,	 6,  3 },
/*21*/	{ 1, 2,	 8,  4 },
/*22*/	{ 2, 5,	10,  5 }
};

static uint16_t	cpu_clock_table[] = {
    1000, 1066, 1200, 1333, 1500, 1666, 1800, 2000, 600,  667,  800,  1600,
    2133, 2200, 2400 };

uint32_t
get_tclk(void)
{
 	uint32_t cputype;

	cputype = cpufunc_id();
	cputype &= CPU_ID_CPU_MASK;

	if (cputype == CPU_ID_MV88SV584X_V7)
		return (TCLK_250MHZ);
	else
		return (TCLK_200MHZ);
}

static uint32_t
count_l2clk(void)
{
	uint64_t sar_reg;
	uint32_t freq_vco, freq_l2clk;
	uint8_t  sar_cpu_freq, sar_fab_freq, array_size;

	/* Get value of the SAR register and process it */
	sar_reg = get_sar_value();
	sar_cpu_freq = CPU_FREQ_FIELD(sar_reg);
	sar_fab_freq = FAB_FREQ_FIELD(sar_reg);

	/* Check if CPU frequency field has correct value */
	array_size = sizeof(cpu_clock_table) / sizeof(cpu_clock_table[0]);
	if (sar_cpu_freq >= array_size)
		panic("Reserved value in cpu frequency configuration field: "
		    "%d", sar_cpu_freq);

	/* Check if fabric frequency field has correct value */
	array_size = sizeof(freq_conf_table) / sizeof(freq_conf_table[0]);
	if (sar_fab_freq >= array_size)
		panic("Reserved value in fabric frequency configuration field: "
		    "%d", sar_fab_freq);

	/* Get CPU clock frequency */
	freq_vco = cpu_clock_table[sar_cpu_freq] *
	    freq_conf_table[sar_fab_freq].vco_cpu;

	/* Get L2CLK clock frequency */
	freq_l2clk = freq_vco / freq_conf_table[sar_fab_freq].vco_l2c;

	/* Round L2CLK value to integer MHz */
	if (((freq_vco % freq_conf_table[sar_fab_freq].vco_l2c) * 10 /
	    freq_conf_table[sar_fab_freq].vco_l2c) >= 5)
		freq_l2clk++;

	return (freq_l2clk * 1000000);
}

uint32_t
get_l2clk(void)
{
	static uint32_t	l2clk_freq = 0;

	/* If get_l2clk is called first time get L2CLK value from register */
	if (l2clk_freq == 0)
		l2clk_freq = count_l2clk();

	return (l2clk_freq);
}

int
fdt_pci_devmap(phandle_t node, struct pmap_devmap *devmap, vm_offset_t io_va,
    vm_offset_t mem_va)
{

	return (0);
}

