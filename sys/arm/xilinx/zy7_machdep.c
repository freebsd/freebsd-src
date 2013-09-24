/*-
 * Copyright (c) 2013 Thomas Skibo
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
 *
 * $FreeBSD$
 */

/*
 * Machine dependent code for Xilinx Zynq-7000 Soc.
 *
 * Reference: Zynq-7000 All Programmable SoC Technical Reference Manual.
 * (v1.4) November 16, 2012.  Xilinx doc UG585.
 */

#include "opt_global.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/fdt/fdt_common.h>

#include <machine/bus.h>
#include <machine/pmap.h>
#include <machine/frame.h>
#include <machine/machdep.h>

#include <arm/xilinx/zy7_reg.h>

void (*zynq7_cpu_reset)(void);

vm_offset_t
initarm_lastaddr(void)
{

	return (ZYNQ7_PSIO_VBASE - ARM_NOCACHE_KVA_SIZE);
}

void
initarm_gpio_init(void)
{
}

void
initarm_late_init(void)
{
}

#define FDT_DEVMAP_SIZE 3
static struct pmap_devmap fdt_devmap[FDT_DEVMAP_SIZE];

/*
 * Construct pmap_devmap[] with DT-derived config data.
 */
int
platform_devmap_init(void)
{
	int i = 0;

	fdt_devmap[i].pd_va =	ZYNQ7_PSIO_VBASE;
	fdt_devmap[i].pd_pa =	ZYNQ7_PSIO_HWBASE;
	fdt_devmap[i].pd_size = ZYNQ7_PSIO_SIZE;
	fdt_devmap[i].pd_prot = VM_PROT_READ | VM_PROT_WRITE;
	fdt_devmap[i].pd_cache = PTE_DEVICE;
	i++;

	fdt_devmap[i].pd_va =	ZYNQ7_PSCTL_VBASE;
	fdt_devmap[i].pd_pa = 	ZYNQ7_PSCTL_HWBASE;
	fdt_devmap[i].pd_size = ZYNQ7_PSCTL_SIZE;
	fdt_devmap[i].pd_prot = VM_PROT_READ | VM_PROT_WRITE;
	fdt_devmap[i].pd_cache = PTE_DEVICE;
	i++;

	/* end of table */
	fdt_devmap[i].pd_va = 0;
	fdt_devmap[i].pd_pa = 0;
	fdt_devmap[i].pd_size = 0;
	fdt_devmap[i].pd_prot = 0;
	fdt_devmap[i].pd_cache = 0;

	pmap_devmap_bootstrap_table = &fdt_devmap[0];
	return (0);
}


struct fdt_fixup_entry fdt_fixup_table[] = {
	{ NULL, NULL }
};

static int
fdt_gic_decode_ic(phandle_t node, pcell_t *intr, int *interrupt, int *trig,
    int *pol)
{

	if (!fdt_is_compatible(node, "arm,gic"))
		return (ENXIO);

	*interrupt = fdt32_to_cpu(intr[0]);
	*trig = INTR_TRIGGER_CONFORM;
	*pol = INTR_POLARITY_CONFORM;

	return (0);
}

fdt_pic_decode_t fdt_pic_table[] = {
	&fdt_gic_decode_ic,
	NULL
};


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

void
cpu_reset()
{
	if (zynq7_cpu_reset != NULL)
		(*zynq7_cpu_reset)();

	printf("cpu_reset: no platform cpu_reset.  hanging.\n");
	for (;;)
		;
}
