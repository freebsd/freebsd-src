/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ganbold Tsagaankhuu <ganbold@freebsd.org>
 * Copyright (c) 2022 Søren Schmidt <sos@FreeBSD.org>
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

#define	SDHCI_FDT_RK3399	1
#define	SDHCI_FDT_RK3568	2

#define	RK3399_GRF_EMMCCORE_CON0		0xf000
#define	 RK3399_CORECFG_BASECLKFREQ		0xff00
#define	 RK3399_CORECFG_TIMEOUTCLKUNIT		(1 << 7)
#define	 RK3399_CORECFG_TUNINGCOUNT		0x3f
#define	RK3399_GRF_EMMCCORE_CON11		0xf02c
#define	 RK3399_CORECFG_CLOCKMULTIPLIER		0xff

#define	RK3568_EMMC_HOST_CTRL			0x0508
#define	RK3568_EMMC_EMMC_CTRL			0x052c
#define	RK3568_EMMC_ATCTRL			0x0540
#define	RK3568_EMMC_DLL_CTRL			0x0800
#define	 DLL_CTRL_SRST				0x00000001
#define	 DLL_CTRL_START				0x00000002
#define	 DLL_CTRL_START_POINT_DEFAULT		0x00050000
#define	 DLL_CTRL_INCREMENT_DEFAULT		0x00000200

#define	RK3568_EMMC_DLL_RXCLK			0x0804
#define	 DLL_RXCLK_DELAY_ENABLE			0x08000000
#define	 DLL_RXCLK_NO_INV			0x20000000

#define	RK3568_EMMC_DLL_TXCLK			0x0808
#define	 DLL_TXCLK_DELAY_ENABLE			0x08000000
#define	 DLL_TXCLK_TAPNUM_DEFAULT		0x00000008
#define	 DLL_TXCLK_TAPNUM_FROM_SW		0x01000000

#define	RK3568_EMMC_DLL_STRBIN			0x080c
#define	 DLL_STRBIN_DELAY_ENABLE		0x08000000
#define	 DLL_STRBIN_TAPNUM_DEFAULT		0x00000008
#define	DLL_STRBIN_TAPNUM_FROM_SW		0x01000000

#define	RK3568_EMMC_DLL_STATUS0			0x0840
#define	 DLL_STATUS0_DLL_LOCK			0x00000100
#define	 DLL_STATUS0_DLL_TIMEOUT		0x00000200

#define	LOWEST_SET_BIT(mask)	((((mask) - 1) & (mask)) ^ (mask))
#define	SHIFTIN(x, mask)	((x) * LOWEST_SET_BIT(mask))

static struct ofw_compat_data compat_data[] = {
	{ "rockchip,rk3399-sdhci-5.1",	SDHCI_FDT_RK3399 },
	{ "rockchip,rk3568-dwcmshc",	SDHCI_FDT_RK3568 },
	{ NULL, 0 }
};

static int
sdhci_fdt_rockchip_probe(device_t dev)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);

	sc->quirks = 0;
	switch (ofw_bus_search_compatible(dev, compat_data)->ocd_data) {
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	case SDHCI_FDT_RK3399:
		device_set_desc(dev, "Rockchip RK3399 fdt SDHCI controller");
		break;
	case SDHCI_FDT_RK3568:
		device_set_desc(dev, "Rockchip RK3568 fdt SDHCI controller");
		break;
	default:
		return (ENXIO);
	}

	return (0);
}

static int
sdhci_init_rk3399(device_t dev)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);
	uint64_t freq;
	uint32_t mask, val;
	int error;

	error = clk_get_freq(sc->clk_xin, &freq);
	if (error != 0) {
		device_printf(dev, "cannot get xin clock frequency\n");
		return (ENXIO);
	}

	/* Disable clock multiplier */
	mask = RK3399_CORECFG_CLOCKMULTIPLIER;
	val = 0;
	SYSCON_WRITE_4(sc->syscon, RK3399_GRF_EMMCCORE_CON11, (mask << 16) | val);

	/* Set base clock frequency */
	mask = RK3399_CORECFG_BASECLKFREQ;
	val = SHIFTIN((freq + (1000000 / 2)) / 1000000,
	    RK3399_CORECFG_BASECLKFREQ);
	SYSCON_WRITE_4(sc->syscon, RK3399_GRF_EMMCCORE_CON0, (mask << 16) | val);

	return (0);
}

static int
sdhci_fdt_rockchip_set_clock(device_t dev, struct sdhci_slot *slot, int clock)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);
	int32_t val;
	int i;

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data ==
	    SDHCI_FDT_RK3568) {
		if (clock == 400000)
			clock = 375000;

		if (clock) {
			clk_set_freq(sc->clk_core, clock, 0);

			if (clock <= 52000000) {
				bus_write_4(sc->mem_res[slot->num],
				    RK3568_EMMC_DLL_CTRL, 0x0);
				bus_write_4(sc->mem_res[slot->num],
				    RK3568_EMMC_DLL_RXCLK, DLL_RXCLK_NO_INV);
				bus_write_4(sc->mem_res[slot->num],
				    RK3568_EMMC_DLL_TXCLK, 0x0);
				bus_write_4(sc->mem_res[slot->num],
				    RK3568_EMMC_DLL_STRBIN, 0x0);
				return (clock);
			}

			bus_write_4(sc->mem_res[slot->num],
			    RK3568_EMMC_DLL_CTRL, DLL_CTRL_START);
			DELAY(1000);
			bus_write_4(sc->mem_res[slot->num],
			    RK3568_EMMC_DLL_CTRL, 0);
			bus_write_4(sc->mem_res[slot->num],
			    RK3568_EMMC_DLL_CTRL, DLL_CTRL_START_POINT_DEFAULT |
			    DLL_CTRL_INCREMENT_DEFAULT | DLL_CTRL_START);
			for (i = 0; i < 500; i++) {
				val = bus_read_4(sc->mem_res[slot->num],
				    RK3568_EMMC_DLL_STATUS0);
				if (val & DLL_STATUS0_DLL_LOCK &&
				    !(val & DLL_STATUS0_DLL_TIMEOUT))
					break;
				DELAY(1000);
			}
			bus_write_4(sc->mem_res[slot->num], RK3568_EMMC_ATCTRL,
			    (0x1 << 16 | 0x2 << 17 | 0x3 << 19));
			bus_write_4(sc->mem_res[slot->num],
			    RK3568_EMMC_DLL_RXCLK,
			    DLL_RXCLK_DELAY_ENABLE | DLL_RXCLK_NO_INV);
			bus_write_4(sc->mem_res[slot->num],
			    RK3568_EMMC_DLL_TXCLK, DLL_TXCLK_DELAY_ENABLE |
			    DLL_TXCLK_TAPNUM_DEFAULT|DLL_TXCLK_TAPNUM_FROM_SW);
			bus_write_4(sc->mem_res[slot->num],
			    RK3568_EMMC_DLL_STRBIN, DLL_STRBIN_DELAY_ENABLE |
			    DLL_STRBIN_TAPNUM_DEFAULT |
			    DLL_STRBIN_TAPNUM_FROM_SW);
		}
	}
	return (sdhci_fdt_set_clock(dev, slot, clock));
}

static int
sdhci_fdt_rockchip_attach(device_t dev)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);
	int err, compat;

	sc->dev = dev;
	compat = ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	switch (compat) {
	case SDHCI_FDT_RK3399:
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
		err = sdhci_init_rk3399(dev);
		if (err != 0) {
			device_printf(dev, "Cannot init RK3399 SDHCI\n");
			return (err);
		}
		break;
	case SDHCI_FDT_RK3568:
		/* setup & enable clocks */
		if (clk_get_by_ofw_name(dev, 0, "core", &sc->clk_core)) {
			device_printf(dev, "cannot get core clock\n");
			return (ENXIO);
		}
		clk_enable(sc->clk_core);
		break;
	default:
		break;
	}

	return (sdhci_fdt_attach(dev));
}

static device_method_t sdhci_fdt_rockchip_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe,		sdhci_fdt_rockchip_probe),
	DEVMETHOD(device_attach,	sdhci_fdt_rockchip_attach),

	/* SDHCI methods */
	DEVMETHOD(sdhci_set_clock,	sdhci_fdt_rockchip_set_clock),

	DEVMETHOD_END
};
extern driver_t sdhci_fdt_driver;

DEFINE_CLASS_1(sdhci_rockchip, sdhci_fdt_rockchip_driver, sdhci_fdt_rockchip_methods,
    sizeof(struct sdhci_fdt_softc), sdhci_fdt_driver);
DRIVER_MODULE(sdhci_rockchip, simplebus, sdhci_fdt_rockchip_driver, NULL, NULL);
