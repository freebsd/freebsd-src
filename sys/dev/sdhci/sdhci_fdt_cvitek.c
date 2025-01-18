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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/taskqueue.h>
#include <sys/module.h>

#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_subr.h>
#include <dev/clk/clk.h>
#include <dev/clk/clk_fixed.h>
#include <dev/ofw/openfirm.h>
#include <dev/syscon/syscon.h>

#include <dev/phy/phy.h>
#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcbrvar.h>
#include <dev/mmc/mmcreg.h>

#include <dev/fdt/fdt_common.h>
#include <dev/mmc/mmc_fdt_helpers.h>

#include <dev/sdhci/sdhci.h>
#include <dev/sdhci/sdhci_fdt_gpio.h>
#include <dev/sdhci/sdhci_fdt.h>

#include "syscon_if.h"
#include "mmcbr_if.h"
#include "sdhci_if.h"

#include "opt_mmccam.h"
#include "opt_soc.h"

#define CV181X_SYSCTRL_SD_PWRSW_CTRL	0x1F4
#define  SD_PWRSW_CTRL_RESET_MASK	0x9
#define CVI_CV181X_SDHCI_VENDOR_OFFSET	0x200
#define CVI_CV181X_SDHCI_EMMC_CTRL	(CVI_CV181X_SDHCI_VENDOR_OFFSET + 0x0)
#define  EMMC_CTRL_RESET_MASK		 0x302
#define CVI_CV181X_SDHCI_PHY_TX_RX_DLY	(CVI_CV181X_SDHCI_VENDOR_OFFSET + 0x40)
#define  PHY_TX_RX_DLY_RESET_MASK	 0x1000100
#define CVI_CV181X_SDHCI_PHY_CONFIG	(CVI_CV181X_SDHCI_VENDOR_OFFSET + 0x4C)
#define  PHY_CONFIG_RESET_MASK		 0x1

#define	SDHCI_FDT_CVITEK_CV181X_SD	1

static struct ofw_compat_data compat_data[] = {
	{ "cvitek,cv181x-sd", SDHCI_FDT_CVITEK_CV181X_SD },
	{ NULL, 0 }
};

static int
sdhci_fdt_cvitek_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Cvitek CV181x SDHCI controller");
	return (BUS_PROBE_SPECIFIC);
}

static int
sdhci_fdt_cvitek_attach(device_t dev)
{
	int error;
	uint32_t reg;
	phandle_t node;
	struct resource *res;
	struct syscon	*syscon;
	struct sdhci_fdt_softc *sc = device_get_softc(dev);

	if (sdhci_fdt_attach(dev))
		return (ENXIO);

	res = sc->mem_res[0];
	node = ofw_bus_find_compatible(OF_finddevice("/"), "syscon");
	error = syscon_get_by_ofw_node(dev, node, &syscon);
	if (error != 0) {
		device_printf(dev, "Couldn't get syscon handle\n");
		return (error);
	}

	SYSCON_WRITE_4(syscon, CV181X_SYSCTRL_SD_PWRSW_CTRL,
	    SD_PWRSW_CTRL_RESET_MASK);
	DELAY(1000);

	reg = bus_read_4(res, CVI_CV181X_SDHCI_EMMC_CTRL);
	reg |= EMMC_CTRL_RESET_MASK;
	bus_write_4(res, CVI_CV181X_SDHCI_EMMC_CTRL, reg);
	bus_write_4(res, CVI_CV181X_SDHCI_PHY_TX_RX_DLY,
	    PHY_TX_RX_DLY_RESET_MASK);
	bus_write_4(res, CVI_CV181X_SDHCI_PHY_CONFIG,
	    PHY_CONFIG_RESET_MASK);

	return (0);
}

static device_method_t sdhci_fdt_cvitek_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe,		sdhci_fdt_cvitek_probe),
	DEVMETHOD(device_attach,	sdhci_fdt_cvitek_attach),

	DEVMETHOD_END
};
extern driver_t sdhci_fdt_driver;

DEFINE_CLASS_1(sdhci_cvitek, sdhci_fdt_cvitek_driver, sdhci_fdt_cvitek_methods,
    sizeof(struct sdhci_fdt_softc), sdhci_fdt_driver);
DRIVER_MODULE(sdhci_cvitek, simplebus, sdhci_fdt_cvitek_driver, NULL, NULL);

#ifndef MMCCAM
MMC_DECLARE_BRIDGE(sdhci_fdt_cvitek);
#endif
