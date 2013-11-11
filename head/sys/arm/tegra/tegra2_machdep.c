/*-
 * Copyright (c) 2011 Damjan Marion.
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 * from: FreeBSD: //depot/projects/arm/src/sys/arm/mv/mv_machdep.c
 */

#include "opt_ddb.h"
#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/devmap.h>
#include <machine/machdep.h>

#include <dev/fdt/fdt_common.h>

/* FIXME move to tegrareg.h */
#define TEGRA2_BASE			0xE0000000	/* KVM base for peripherials */
#define TEGRA2_UARTA_VA_BASE		0xE0006000
#define TEGRA2_UARTA_PA_BASE		0x70006000

#define TEGRA2_CLK_RST_PA_BASE		0x60006000

#define TEGRA2_CLK_RST_OSC_FREQ_DET_REG		0x58
#define TEGRA2_CLK_RST_OSC_FREQ_DET_STAT_REG	0x5C
#define OSC_FREQ_DET_TRIG			(1<<31)
#define OSC_FREQ_DET_BUSY               	(1<<31)

#if 0
static int
tegra2_osc_freq_detect(void)
{
	bus_space_handle_t	bsh;
	uint32_t		c;
	uint32_t		r=0;
	int			i=0;

	struct {
		uint32_t val;
		uint32_t freq;
	} freq_det_cnts[] = {
		{ 732,  12000000 },
		{ 794,  13000000 },
		{1172,  19200000 },
		{1587,  26000000 },
		{  -1,         0 },
	};

	printf("Measuring...\n");
	bus_space_map(fdtbus_bs_tag,TEGRA2_CLK_RST_PA_BASE, 0x1000, 0, &bsh);

	bus_space_write_4(fdtbus_bs_tag, bsh, TEGRA2_CLK_RST_OSC_FREQ_DET_REG,
			OSC_FREQ_DET_TRIG | 1 );
	do {} while (bus_space_read_4(fdtbus_bs_tag, bsh,
			TEGRA2_CLK_RST_OSC_FREQ_DET_STAT_REG) & OSC_FREQ_DET_BUSY);

	c = bus_space_read_4(fdtbus_bs_tag, bsh, TEGRA2_CLK_RST_OSC_FREQ_DET_STAT_REG);

	while (freq_det_cnts[i].val > 0) {
		if (((freq_det_cnts[i].val - 3) < c) && (c < (freq_det_cnts[i].val + 3)))
			r = freq_det_cnts[i].freq;
		i++;
	}
	printf("c=%u r=%u\n",c,r );
	bus_space_free(fdtbus_bs_tag, bsh, 0x1000);
	return r;
}
#endif

vm_offset_t
initarm_lastaddr(void)
{

	return (fdt_immr_va);
}

void
initarm_early_init(void)
{

	if (fdt_immr_addr(TEGRA2_BASE) != 0)				/* FIXME ???? */
		while (1);
}

void
initarm_gpio_init(void)
{
}

void
initarm_late_init(void)
{
}

#define FDT_DEVMAP_MAX	(1 + 2 + 1 + 1)	/* FIXME */
static struct arm_devmap_entry fdt_devmap[FDT_DEVMAP_MAX] = {
	{ 0, 0, 0, 0, 0, }
};

/*
 * Construct pmap_devmap[] with DT-derived config data.
 */
int
initarm_devmap_init(void)
{
	int i = 0;
	fdt_devmap[i].pd_va = 0xe0000000;
	fdt_devmap[i].pd_pa = 0x70000000;
	fdt_devmap[i].pd_size = 0x100000;
	fdt_devmap[i].pd_prot = VM_PROT_READ | VM_PROT_WRITE;
	fdt_devmap[i].pd_cache = PTE_NOCACHE;
	i++;

	arm_devmap_register_table(&fdt_devmap[0]);
	return (0);
}

struct arm32_dma_range *
bus_dma_get_range(void)
{

	return (NULL);
}

int
bus_dma_get_range_nb(void)
{

	return (0);
}

