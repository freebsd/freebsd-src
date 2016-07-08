/*-
 * Copyright (c) 2012 Ganbold Tsagaankhuu <ganbold@freebsd.org>
 * Copyright (c) 2015-2016 Emmanuel Vadot <manu@bidouilliste.com>
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
#include <sys/devmap.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/machdep.h>
#include <machine/platformvar.h>

#include <dev/fdt/fdt_common.h>

#include <arm/allwinner/aw_mp.h>
#include <arm/allwinner/aw_wdog.h>
#include <arm/allwinner/allwinner_machdep.h>

#include "platform_if.h"

static u_int soc_type;
static u_int soc_family;

static int
a10_attach(platform_t plat)
{
	soc_type = ALLWINNERSOC_A10;
	soc_family = ALLWINNERSOC_SUN4I;
	return (0);
}

static int
a13_attach(platform_t plat)
{
	soc_type = ALLWINNERSOC_A13;
	soc_family = ALLWINNERSOC_SUN5I;
	return (0);
}

static int
a20_attach(platform_t plat)
{
	soc_type = ALLWINNERSOC_A20;
	soc_family = ALLWINNERSOC_SUN7I;

	return (0);
}

static int
a31_attach(platform_t plat)
{
	soc_type = ALLWINNERSOC_A31;
	soc_family = ALLWINNERSOC_SUN6I;

	return (0);
}

static int
a31s_attach(platform_t plat)
{
	soc_type = ALLWINNERSOC_A31S;
	soc_family = ALLWINNERSOC_SUN6I;

	return (0);
}

static int
a83t_attach(platform_t plat)
{
	soc_type = ALLWINNERSOC_A83T;
	soc_family = ALLWINNERSOC_SUN8I;

	return (0);
}

static int
h3_attach(platform_t plat)
{
	soc_type = ALLWINNERSOC_H3;
	soc_family = ALLWINNERSOC_SUN8I;

	return (0);
}

static vm_offset_t
allwinner_lastaddr(platform_t plat)
{

	return (devmap_lastaddr());
}

/*
 * Set up static device mappings.
 *
 * This covers all the on-chip device with 1MB section mappings, which is good
 * for performance (uses fewer TLB entries for device access).
 *
 * XXX It also covers a block of SRAM and some GPU (mali400) stuff that maybe
 * shouldn't be device-mapped.  The original code mapped a 4MB block, but
 * perhaps a 1MB block would be more appropriate.
 */
static int
allwinner_devmap_init(platform_t plat)
{

	devmap_add_entry(0x01C00000, 0x00400000); /* 4MB */

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
	aw_wdog_watchdog_reset();
	printf("Reset failed!\n");
	while (1);
}

#if defined(SOC_ALLWINNER_A10)
static platform_method_t a10_methods[] = {
	PLATFORMMETHOD(platform_attach,         a10_attach),
	PLATFORMMETHOD(platform_lastaddr,       allwinner_lastaddr),
	PLATFORMMETHOD(platform_devmap_init,    allwinner_devmap_init),

	PLATFORMMETHOD_END,
};
FDT_PLATFORM_DEF(a10, "a10", 0, "allwinner,sun4i-a10", 200);
#endif

#if defined(SOC_ALLWINNER_A13)
static platform_method_t a13_methods[] = {
	PLATFORMMETHOD(platform_attach,         a13_attach),
	PLATFORMMETHOD(platform_lastaddr,       allwinner_lastaddr),
	PLATFORMMETHOD(platform_devmap_init,    allwinner_devmap_init),

	PLATFORMMETHOD_END,
};
FDT_PLATFORM_DEF(a13, "a13", 0, "allwinner,sun5i-a13", 200);
#endif

#if defined(SOC_ALLWINNER_A20)
static platform_method_t a20_methods[] = {
	PLATFORMMETHOD(platform_attach,         a20_attach),
	PLATFORMMETHOD(platform_lastaddr,       allwinner_lastaddr),
	PLATFORMMETHOD(platform_devmap_init,    allwinner_devmap_init),

#ifdef SMP
	PLATFORMMETHOD(platform_mp_start_ap,	aw_mp_start_ap),
	PLATFORMMETHOD(platform_mp_setmaxid,	aw_mp_setmaxid),
#endif
	PLATFORMMETHOD_END,
};
FDT_PLATFORM_DEF(a20, "a20", 0, "allwinner,sun7i-a20", 200);
#endif

#if defined(SOC_ALLWINNER_A31)
static platform_method_t a31_methods[] = {
	PLATFORMMETHOD(platform_attach,         a31_attach),
	PLATFORMMETHOD(platform_lastaddr,       allwinner_lastaddr),
	PLATFORMMETHOD(platform_devmap_init,    allwinner_devmap_init),

#ifdef SMP
	PLATFORMMETHOD(platform_mp_start_ap,	aw_mp_start_ap),
	PLATFORMMETHOD(platform_mp_setmaxid,	aw_mp_setmaxid),
#endif
	PLATFORMMETHOD_END,
};
FDT_PLATFORM_DEF(a31, "a31", 0, "allwinner,sun6i-a31", 200);
#endif

#if defined(SOC_ALLWINNER_A31S)
static platform_method_t a31s_methods[] = {
	PLATFORMMETHOD(platform_attach,         a31s_attach),
	PLATFORMMETHOD(platform_lastaddr,       allwinner_lastaddr),
	PLATFORMMETHOD(platform_devmap_init,    allwinner_devmap_init),

#ifdef SMP
	PLATFORMMETHOD(platform_mp_start_ap,	aw_mp_start_ap),
	PLATFORMMETHOD(platform_mp_setmaxid,	aw_mp_setmaxid),
#endif
	PLATFORMMETHOD_END,
};
FDT_PLATFORM_DEF(a31s, "a31s", 0, "allwinner,sun6i-a31s", 200);
#endif

#if defined(SOC_ALLWINNER_A83T)
static platform_method_t a83t_methods[] = {
	PLATFORMMETHOD(platform_attach,         a83t_attach),
	PLATFORMMETHOD(platform_lastaddr,       allwinner_lastaddr),
	PLATFORMMETHOD(platform_devmap_init,    allwinner_devmap_init),

#ifdef SMP
	PLATFORMMETHOD(platform_mp_start_ap,	a83t_mp_start_ap),
	PLATFORMMETHOD(platform_mp_setmaxid,	aw_mp_setmaxid),
#endif
	PLATFORMMETHOD_END,
};
FDT_PLATFORM_DEF(a83t, "a83t", 0, "allwinner,sun8i-a83t", 200);
#endif

#if defined(SOC_ALLWINNER_H3)
static platform_method_t h3_methods[] = {
	PLATFORMMETHOD(platform_attach,         h3_attach),
	PLATFORMMETHOD(platform_lastaddr,       allwinner_lastaddr),
	PLATFORMMETHOD(platform_devmap_init,    allwinner_devmap_init),

#ifdef SMP
	PLATFORMMETHOD(platform_mp_start_ap,	aw_mp_start_ap),
	PLATFORMMETHOD(platform_mp_setmaxid,	aw_mp_setmaxid),
#endif
	PLATFORMMETHOD_END,
};
FDT_PLATFORM_DEF(h3, "h3", 0, "allwinner,sun8i-h3", 200);
#endif

u_int
allwinner_soc_type(void)
{
	return (soc_type);
}

u_int
allwinner_soc_family(void)
{
	return (soc_family);
}
