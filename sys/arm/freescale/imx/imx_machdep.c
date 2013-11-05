/*-
 * Copyright (c) 2013 Ian Lepore <ian@freebsd.org>
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

#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/armreg.h>
#include <machine/bus.h>
#include <machine/devmap.h>
#include <machine/machdep.h>

#include <arm/freescale/imx/imx_machdep.h>
#include <arm/freescale/imx/imx_wdogreg.h>

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

/*
 * This code which manipulates the watchdog hardware is here to implement
 * cpu_reset() because the watchdog is the only way for software to reset the
 * chip.  Why here and not in imx_wdog.c?  Because there's no requirement that
 * the watchdog driver be compiled in, but it's nice to be able to reboot even
 * if it's not.
 */
void
imx_wdog_cpu_reset(vm_offset_t wdcr_physaddr)
{
	volatile uint16_t * pcr;

	/*
	 * The deceptively simple write of WDOG_CR_WDE enables the watchdog,
	 * sets the timeout to its minimum value (half a second), and also
	 * clears the SRS bit which results in the SFTW (software-requested
	 * reset) bit being set in the watchdog status register after the reset.
	 * This is how software can distinguish a reset from a wdog timeout.
	 */
	if ((pcr = arm_devmap_ptov(wdcr_physaddr, sizeof(*pcr))) == NULL) {
		printf("cpu_reset() can't find its control register... locking up now.");
	} else {
		*pcr = WDOG_CR_WDE;
	}
	for (;;)
		continue;
}

u_int
imx_soc_family(void)
{
	return (imx_soc_type() >> IMXSOC_FAMSHIFT);
}


