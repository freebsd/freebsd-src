/*-
 * Copyright (c) 2013 Ruslan Bukin <br@bsdpad.com>
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
#include <machine/frame.h> /* For trapframe_t, used in <machine/machdep.h> */
#include <machine/machdep.h>
#include <machine/pmap.h>

#include <dev/fdt/fdt_common.h>

#define	DEVMAP_BOOTSTRAP_MAP_START 0xE0000000

extern int unmapped_buf_allowed;

vm_offset_t
initarm_lastaddr(void)
{

	return (DEVMAP_BOOTSTRAP_MAP_START - ARM_NOCACHE_KVA_SIZE);
}

void
initarm_gpio_init(void)
{
}

void
initarm_late_init(void)
{

	/* XXX: PR arm/180080 */
	unmapped_buf_allowed = 0;
}

#define FDT_DEVMAP_MAX	(1 + 2 + 1 + 1)	/* FIXME */
static struct pmap_devmap fdt_devmap[FDT_DEVMAP_MAX] = {
	{ 0, 0, 0, 0, 0, }
};

/*
 * Construct pmap_devmap[] with DT-derived config data.
 */
int
platform_devmap_init(void)
{
	int i;

	i = 0;
	fdt_devmap[i].pd_va = 0xe2C00000;
	fdt_devmap[i].pd_pa = 0x12C00000;
	fdt_devmap[i].pd_size = 0x100000;
	fdt_devmap[i].pd_prot = VM_PROT_READ | VM_PROT_WRITE;
	fdt_devmap[i].pd_cache = PTE_NOCACHE;
	i++;

	pmap_devmap_bootstrap_table = &fdt_devmap[0];
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
