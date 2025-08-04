/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Bojan Novković <bnovkov@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/module.h>

#include <net/if.h>

#include <machine/bus.h>
#include <machine/param.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>

#include <dev/dwc/if_dwcvar.h>
#include <dev/dwc/dwc1000_reg.h>

#include "if_dwc_if.h"

static int
if_dwc_cvitek_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "cvitek,ethernet"))
		return (ENXIO);
	device_set_desc(dev, "CVITEK Ethernet Controller");

	return (BUS_PROBE_DEFAULT);
}

static int
if_dwc_cvitek_mii_clk(device_t dev)
{
	/*
	 * XXX: This is a hack to get the driver working on
	 * the Milk-V platform. For reference, the u-boot designware
	 * driver uses the same '150_250M' clock value, but if_dwc
	 * will not work on Milk-V hardware unless the lowest
	 * bit of the PHY register address is always set.
	 */
	return (0x10 | GMAC_MII_CLK_150_250M_DIV102);
}

static device_method_t dwc_cvitek_methods[] = {
	DEVMETHOD(device_probe,		if_dwc_cvitek_probe),

	DEVMETHOD(if_dwc_mii_clk,	if_dwc_cvitek_mii_clk),
	DEVMETHOD_END
};

extern driver_t dwc_driver;

DEFINE_CLASS_1(dwc, dwc_cvitek_driver, dwc_cvitek_methods,
    sizeof(struct dwc_softc), dwc_driver);
DRIVER_MODULE(dwc_cvitek, simplebus, dwc_cvitek_driver, 0, 0);
MODULE_DEPEND(dwc_cvitek, dwc, 1, 1, 1);
