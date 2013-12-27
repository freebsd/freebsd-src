/*-
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: FreeBSD: //depot/projects/arm/src/sys/arm/at91/kb920x_machdep.c, rev 45
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

#include <arm/lpc/lpcreg.h>
#include <arm/lpc/lpcvar.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ic/ns16550.h>

vm_offset_t
initarm_lastaddr(void)
{

	return (fdt_immr_va);
}

void
initarm_early_init(void)
{

	if (fdt_immr_addr(LPC_DEV_BASE) != 0)
		while (1);
}

void
initarm_gpio_init(void)
{

	/*
	 * Set initial values of GPIO output ports
	 */
	platform_gpio_init();
}

void
initarm_late_init(void)
{
}

#define FDT_DEVMAP_MAX	(1 + 2 + 1 + 1)
static struct arm_devmap_entry fdt_devmap[FDT_DEVMAP_MAX] = {
	{ 0, 0, 0, 0, 0, }
};

/*
 * Construct pmap_devmap[] with DT-derived config data.
 */
int
initarm_devmap_init(void)
{

	/*
	 * IMMR range.
	 */
	fdt_devmap[0].pd_va = fdt_immr_va;
	fdt_devmap[0].pd_pa = fdt_immr_pa;
	fdt_devmap[0].pd_size = fdt_immr_size;
	fdt_devmap[0].pd_prot = VM_PROT_READ | VM_PROT_WRITE;
	fdt_devmap[0].pd_cache = PTE_NOCACHE;
	
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
cpu_reset(void)
{
	/* Enable WDT */
	bus_space_write_4(fdtbus_bs_tag, 
	    LPC_CLKPWR_BASE, LPC_CLKPWR_TIMCLK_CTRL,
	    LPC_CLKPWR_TIMCLK_CTRL_WATCHDOG);

	/* Instant assert of RESETOUT_N with pulse length 1ms */
	bus_space_write_4(fdtbus_bs_tag, LPC_WDTIM_BASE, LPC_WDTIM_PULSE, 13000);
	bus_space_write_4(fdtbus_bs_tag, LPC_WDTIM_BASE, LPC_WDTIM_MCTRL, 0x70);

	for (;;);
}

