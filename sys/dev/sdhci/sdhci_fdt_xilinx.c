/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Emmanuel Vadot <manu@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/ofw/ofw_subr.h>
#include <dev/clk/clk.h>
#include <dev/clk/clk_fixed.h>
#include <dev/ofw/openfirm.h>
#include <dev/syscon/syscon.h>
#include <dev/phy/phy.h>

#include <dev/mmc/bridge.h>

#include <dev/sdhci/sdhci.h>
#include <dev/sdhci/sdhci_fdt.h>

#include "mmcbr_if.h"
#include "sdhci_if.h"

#include "opt_mmccam.h"

#include "clkdev_if.h"
#include "syscon_if.h"

static int
sdhci_fdt_xilinx_probe(device_t dev)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);

	if (!ofw_bus_is_compatible(dev, "xlnx,zynqmp-8.9a"))
		return (ENXIO);

	sc->quirks = 0;
	device_set_desc(dev, "ZynqMP generic fdt SDHCI controller");

	return (0);
}

static int
sdhci_fdt_xilinx_attach(device_t dev)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);
	int err;

	err = sdhci_init_clocks(dev);
	if (err != 0) {
		device_printf(dev, "Cannot init clocks\n");
		return (err);
	}
	sdhci_export_clocks(sc);
	if ((err = sdhci_init_phy(sc)) != 0) {
		device_printf(dev, "Cannot init phy\n");
		return (err);
	}
	if ((err = sdhci_get_syscon(sc)) != 0) {
		device_printf(dev, "Cannot get syscon handle\n");
		return (err);
	}

	return (sdhci_fdt_attach(dev));
}

static device_method_t sdhci_fdt_xilinx_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe,		sdhci_fdt_xilinx_probe),
	DEVMETHOD(device_attach,	sdhci_fdt_xilinx_attach),

	DEVMETHOD_END
};
extern driver_t sdhci_fdt_driver;

DEFINE_CLASS_1(sdhci_xilinx, sdhci_fdt_xilinx_driver, sdhci_fdt_xilinx_methods,
    sizeof(struct sdhci_fdt_softc), sdhci_fdt_driver);
DRIVER_MODULE(sdhci_xilinx, simplebus, sdhci_fdt_xilinx_driver, NULL, NULL);
