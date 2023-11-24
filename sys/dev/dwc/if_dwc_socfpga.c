/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>

#include <dev/dwc/if_dwcvar.h>
#include <dev/dwc/dwc1000_reg.h>

#include "if_dwc_if.h"

static int
if_dwc_socfpga_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "altr,socfpga-stmmac"))
		return (ENXIO);

	device_set_desc(dev, "Altera SOCFPGA Ethernet MAC");

	return (BUS_PROBE_DEFAULT);
}

static int
if_dwc_socfpga_init(device_t dev)
{

	return (0);
}

static int
if_dwc_socfpga_mii_clk(device_t dev)
{
	phandle_t root;

	root = OF_finddevice("/");

	if (ofw_bus_node_is_compatible(root, "altr,socfpga-stratix10"))
		return (GMAC_MII_CLK_35_60M_DIV26);

	/* Default value. */
	return (GMAC_MII_CLK_25_35M_DIV16);
}

static device_method_t dwc_socfpga_methods[] = {
	DEVMETHOD(device_probe,		if_dwc_socfpga_probe),

	DEVMETHOD(if_dwc_init,		if_dwc_socfpga_init),
	DEVMETHOD(if_dwc_mii_clk,	if_dwc_socfpga_mii_clk),

	DEVMETHOD_END
};

extern driver_t dwc_driver;

DEFINE_CLASS_1(dwc, dwc_socfpga_driver, dwc_socfpga_methods,
    sizeof(struct dwc_softc), dwc_driver);
EARLY_DRIVER_MODULE(dwc_socfpga, simplebus, dwc_socfpga_driver, 0, 0,
    BUS_PASS_SUPPORTDEV + BUS_PASS_ORDER_MIDDLE);

MODULE_DEPEND(dwc_socfpga, dwc, 1, 1, 1);
