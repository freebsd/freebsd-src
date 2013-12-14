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
#include <machine/intr.h>
#include <machine/machdep.h>
#include <machine/platformvar.h>

#include <arm/ti/omap4/omap4_reg.h>

#include "platform_if.h"

/* Start of address space used for bootstrap map */
#define DEVMAP_BOOTSTRAP_MAP_START	0xF0000000

#if !defined(SOC_OMAP4) && !defined(SOC_TI_AM335X)
#error "Unknown SoC"
#endif

#if defined(SOC_OMAP4) && defined(SOC_TI_AM335X)
#error Not yet able to use both OMAP4 and AM335X in the same kernel
#endif

static int
ti_attach(platform_t plat)
{

	return (0);
}

static vm_offset_t
ti_lastaddr(platform_t plat)
{

	return (DEVMAP_BOOTSTRAP_MAP_START);
}

#define FDT_DEVMAP_MAX	(2)		// FIXME
static struct arm_devmap_entry fdt_devmap[FDT_DEVMAP_MAX] = {
	{ 0, 0, 0, 0, 0, }
};


/*
 * Construct pmap_devmap[] with DT-derived config data.
 */
#if defined(SOC_OMAP4)
static int
ti_omap4_devmap_init(platform_t plat)
{

	fdt_devmap[0].pd_va = 0xF8000000;
	fdt_devmap[0].pd_pa = 0x48000000;
	fdt_devmap[0].pd_size = 0x1000000;
	fdt_devmap[0].pd_prot = VM_PROT_READ | VM_PROT_WRITE;
	fdt_devmap[0].pd_cache = PTE_DEVICE;

	arm_devmap_register_table(&fdt_devmap[0]);
	return (0);
}
#endif

#if defined(SOC_TI_AM335X)
static int
ti_am335x_devmap_init(platform_t plat)
{

	fdt_devmap[0].pd_va = 0xF4C00000;
	fdt_devmap[0].pd_pa = 0x44C00000;       /* L4_WKUP */
	fdt_devmap[0].pd_size = 0x400000;       /* 4 MB */
	fdt_devmap[0].pd_prot = VM_PROT_READ | VM_PROT_WRITE;
	fdt_devmap[0].pd_cache = PTE_DEVICE;

	arm_devmap_register_table(&fdt_devmap[0]);
	return (0);
}
#endif

#if defined(SOC_OMAP4)
void omap4_prcm_reset(platform_t);

static platform_method_t omap4_methods[] = {
	PLATFORMMETHOD(platform_attach,		ti_attach),
	PLATFORMMETHOD(platform_devmap_init,	ti_omap4_devmap_init),
	PLATFORMMETHOD(platform_lastaddr,	ti_lastaddr),

	PLATFORMMETHOD(platform_cpu_reset,	omap4_prcm_reset),

	PLATFORMMETHOD(platform_get_next_irq,	gic_get_next_irq),
	PLATFORMMETHOD(platform_mask_irq,	gic_mask_irq),
	PLATFORMMETHOD(platform_unmask_irq,	gic_unmask_irq),

	PLATFORMMETHOD(platform_cpu_initclocks, arm_tmr_cpu_initclocks),
	PLATFORMMETHOD(platform_delay,		arm_tmr_delay),

	PLATFORMMETHOD_END,
};

FDT_PLATFORM_DEF(omap4, "omap4", 0, "ti,omap4430");
#endif

#if defined(SOC_TI_AM335X)
static platform_method_t am335x_methods[] = {
	PLATFORMMETHOD(platform_attach,		ti_attach),
	PLATFORMMETHOD(platform_devmap_init,	ti_am335x_devmap_init),
	PLATFORMMETHOD(platform_lastaddr,	ti_lastaddr),

	PLATFORMMETHOD_END,
};

FDT_PLATFORM_DEF(am335x, "am335x", 0, "ti,am335x");
#endif

