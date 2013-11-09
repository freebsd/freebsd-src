/*-
 * Copyright (c) 2012 Ganbold Tsagaankhuu <ganbold@gmail.com>
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: FreeBSD: //depot/projects/arm/src/sys/arm/ti/ti_machdep.c
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

#include <arm/allwinner/a10_wdog.h>

/* Start of address space used for bootstrap map */
#define DEVMAP_BOOTSTRAP_MAP_START      0xE0000000


vm_offset_t
initarm_lastaddr(void)
{

	return (DEVMAP_BOOTSTRAP_MAP_START);
}

void
initarm_early_init(void)
{

}

void
initarm_gpio_init(void)
{
}

void
initarm_late_init(void)
{
}

#define FDT_DEVMAP_MAX		(1 + 2 + 1 + 1)
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

	fdt_devmap[i].pd_va =   0xE1C00000;
	fdt_devmap[i].pd_pa =   0x01C00000;
	fdt_devmap[i].pd_size = 0x00400000;	/* 4 MB */
	fdt_devmap[i].pd_prot = VM_PROT_READ | VM_PROT_WRITE;
	fdt_devmap[i].pd_cache = PTE_DEVICE;

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

void
cpu_reset()
{
	a10wd_watchdog_reset();
	printf("Reset failed!\n");
	while (1);
}
