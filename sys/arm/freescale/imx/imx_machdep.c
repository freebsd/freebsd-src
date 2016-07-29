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
#include <sys/devmap.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/armreg.h>
#include <machine/bus.h>
#include <machine/machdep.h>

#include <arm/freescale/imx/imx_machdep.h>
#include <arm/freescale/imx/imx_wdogreg.h>

SYSCTL_NODE(_hw, OID_AUTO, imx, CTLFLAG_RW, NULL, "i.MX container");

static int last_reset_status;
SYSCTL_UINT(_hw_imx, OID_AUTO, last_reset_status, CTLFLAG_RD, 
    &last_reset_status, 0, "Last reset status register");

SYSCTL_STRING(_hw_imx, OID_AUTO, last_reset_reason, CTLFLAG_RD, 
    "unknown", 0, "Last reset reason");

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
	 * Trigger an immediate reset by clearing the SRS bit in the watchdog
	 * control register.  The reset happens on the next cycle of the wdog
	 * 32KHz clock, so hang out in a spin loop until the reset takes effect.
	 */
	if ((pcr = devmap_ptov(wdcr_physaddr, sizeof(*pcr))) == NULL) {
		printf("cpu_reset() can't find its control register... locking up now.");
	} else {
		*pcr &= ~WDOG_CR_SRS;
	}
	for (;;)
		continue;
}

void
imx_wdog_init_last_reset(vm_offset_t wdsr_phys)
{
	volatile uint16_t * psr;

	if ((psr = devmap_ptov(wdsr_phys, sizeof(*psr))) == NULL)
		return;
	last_reset_status = *psr;
	if (last_reset_status & WDOG_RSR_SFTW) {
		sysctl___hw_imx_last_reset_reason.oid_arg1 = "SoftwareReset";
	} else if (last_reset_status & WDOG_RSR_TOUT) {
		sysctl___hw_imx_last_reset_reason.oid_arg1 = "WatchdogTimeout";
	} else if (last_reset_status & WDOG_RSR_POR) {
		sysctl___hw_imx_last_reset_reason.oid_arg1 = "PowerOnReset";
	}
}

u_int
imx_soc_family(void)
{
	return (imx_soc_type() >> IMXSOC_FAMSHIFT);
}


