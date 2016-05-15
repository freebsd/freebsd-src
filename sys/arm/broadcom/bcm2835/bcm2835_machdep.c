/*-
 * Copyright (c) 2012 Oleksandr Tymoshenko.
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
#include <sys/devmap.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/machdep.h>
#include <machine/platform.h>
#include <machine/platformvar.h>

#include <dev/fdt/fdt_common.h>

#include <arm/broadcom/bcm2835/bcm2835_wdog.h>

#include "platform_if.h"

static vm_offset_t
bcm2835_lastaddr(platform_t plat)
{

	return (devmap_lastaddr());
}

static void
bcm2835_late_init(platform_t plat)
{
	phandle_t system;
	pcell_t cells[2];
	int len;

	system = OF_finddevice("/system");
	if (system != 0) {
		len = OF_getprop(system, "linux,serial", &cells, sizeof(cells));
		if (len > 0)
			board_set_serial(fdt64_to_cpu(*((uint64_t *)cells)));

		len = OF_getprop(system, "linux,revision", &cells, sizeof(cells));
		if (len > 0)
			board_set_revision(fdt32_to_cpu(*((uint32_t *)cells)));
	}
}

#ifdef SOC_BCM2835
/*
 * Set up static device mappings.
 * All on-chip peripherals exist in a 16MB range starting at 0x20000000.
 * Map the entire range using 1MB section mappings.
 */
static int
bcm2835_devmap_init(platform_t plat)
{

	devmap_add_entry(0x20000000, 0x01000000);
	return (0);
}
#endif

#ifdef SOC_BCM2836
static int
bcm2836_devmap_init(platform_t plat)
{

	devmap_add_entry(0x3f000000, 0x01000000);
	return (0);
}
#endif

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
	bcmwd_watchdog_reset();
	while (1);
}

#ifdef SOC_BCM2835
static platform_method_t bcm2835_methods[] = {
	PLATFORMMETHOD(platform_devmap_init,	bcm2835_devmap_init),
	PLATFORMMETHOD(platform_lastaddr,	bcm2835_lastaddr),
	PLATFORMMETHOD(platform_late_init,	bcm2835_late_init),

	PLATFORMMETHOD_END,
};
FDT_PLATFORM_DEF(bcm2835, "bcm2835", 0, "raspberrypi,model-b", 0);
#endif

#ifdef SOC_BCM2836
static platform_method_t bcm2836_methods[] = {
	PLATFORMMETHOD(platform_devmap_init,	bcm2836_devmap_init),
	PLATFORMMETHOD(platform_lastaddr,	bcm2835_lastaddr),
	PLATFORMMETHOD(platform_late_init,	bcm2835_late_init),

	PLATFORMMETHOD_END,
};
FDT_PLATFORM_DEF(bcm2836, "bcm2836", 0, "brcm,bcm2709", 0);
#endif
