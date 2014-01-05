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

vm_offset_t
initarm_lastaddr(void)
{

	return (arm_devmap_lastaddr());
}

void
initarm_early_init(void)
{
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

/*
 * Add a single static device mapping.
 * The values used were taken from the ranges property of the SoC node in the
 * dts file when this code was converted to arm_devmap_add_entry().
 */
int
initarm_devmap_init(void)
{

	arm_devmap_add_entry(LPC_DEV_PHYS_BASE, LPC_DEV_SIZE);
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
	bus_space_tag_t bst;
	bus_space_handle_t bsh;

	bst = fdtbus_bs_tag;

	/* Enable WDT */
	bus_space_map(bst, LPC_CLKPWR_PHYS_BASE, LPC_CLKPWR_SIZE, 0, &bsh);
	bus_space_write_4(bst, bsh, LPC_CLKPWR_TIMCLK_CTRL,
	    LPC_CLKPWR_TIMCLK_CTRL_WATCHDOG);
	bus_space_unmap(bst, bsh, LPC_CLKPWR_SIZE);

	/* Instant assert of RESETOUT_N with pulse length 1ms */
	bus_space_map(bst, LPC_WDTIM_PHYS_BASE, LPC_WDTIM_SIZE, 0, &bsh);
	bus_space_write_4(bst, bsh, LPC_WDTIM_PULSE, 13000);
	bus_space_write_4(bst, bsh, LPC_WDTIM_MCTRL, 0x70);
	bus_space_unmap(bst, bsh, LPC_WDTIM_SIZE);

	for (;;)
		continue;
}

