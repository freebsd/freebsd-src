/*-
 * Copyright (c) 2013 Ganbold Tsagaankhuu <ganbold@freebsd.org>
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

/* Simple clock driver for Allwinner A10 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "a10_clk.h"

struct a10_ccm_softc {
	struct resource		*res;
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	int			pll6_enabled;
};

static struct a10_ccm_softc *a10_ccm_sc = NULL;

#define ccm_read_4(sc, reg)		\
	bus_space_read_4((sc)->bst, (sc)->bsh, (reg))
#define ccm_write_4(sc, reg, val)	\
	bus_space_write_4((sc)->bst, (sc)->bsh, (reg), (val))

static int
a10_ccm_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "allwinner,sun4i-ccm")) {
		device_set_desc(dev, "Allwinner Clock Control Module");
		return(BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
a10_ccm_attach(device_t dev)
{
	struct a10_ccm_softc *sc = device_get_softc(dev);
	int rid = 0;

	if (a10_ccm_sc)
		return (ENXIO);

	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (!sc->res) {
		device_printf(dev, "could not allocate resource\n");
		return (ENXIO);
	}

	sc->bst = rman_get_bustag(sc->res);
	sc->bsh = rman_get_bushandle(sc->res);

	a10_ccm_sc = sc;

	return (0);
}

static device_method_t a10_ccm_methods[] = {
	DEVMETHOD(device_probe,		a10_ccm_probe),
	DEVMETHOD(device_attach,	a10_ccm_attach),
	{ 0, 0 }
};

static driver_t a10_ccm_driver = {
	"a10_ccm",
	a10_ccm_methods,
	sizeof(struct a10_ccm_softc),
};

static devclass_t a10_ccm_devclass;

DRIVER_MODULE(a10_ccm, simplebus, a10_ccm_driver, a10_ccm_devclass, 0, 0);

int
a10_clk_usb_activate(void)
{
	struct a10_ccm_softc *sc = a10_ccm_sc;
	uint32_t reg_value;

	if (sc == NULL)
		return (ENXIO);

	/* Gating AHB clock for USB */
	reg_value = ccm_read_4(sc, CCM_AHB_GATING0);
	reg_value |= CCM_AHB_GATING_USB0; /* AHB clock gate usb0 */
	reg_value |= CCM_AHB_GATING_EHCI0; /* AHB clock gate ehci0 */
	reg_value |= CCM_AHB_GATING_EHCI1; /* AHB clock gate ehci1 */
	ccm_write_4(sc, CCM_AHB_GATING0, reg_value);

	/* Enable clock for USB */
	reg_value = ccm_read_4(sc, CCM_USB_CLK);
	reg_value |= CCM_USB_PHY; /* USBPHY */
	reg_value |= CCM_USB0_RESET; /* disable reset for USB0 */
	reg_value |= CCM_USB1_RESET; /* disable reset for USB1 */
	reg_value |= CCM_USB2_RESET; /* disable reset for USB2 */
	ccm_write_4(sc, CCM_USB_CLK, reg_value);

	return (0);
}

int
a10_clk_usb_deactivate(void)
{
	struct a10_ccm_softc *sc = a10_ccm_sc;
	uint32_t reg_value;

	if (sc == NULL)
		return (ENXIO);

	/* Disable clock for USB */
	reg_value = ccm_read_4(sc, CCM_USB_CLK);
	reg_value &= ~CCM_USB_PHY; /* USBPHY */
	reg_value &= ~CCM_USB0_RESET; /* reset for USB0 */
	reg_value &= ~CCM_USB1_RESET; /* reset for USB1 */
	reg_value &= ~CCM_USB2_RESET; /* reset for USB2 */
	ccm_write_4(sc, CCM_USB_CLK, reg_value);

	/* Disable gating AHB clock for USB */
	reg_value = ccm_read_4(sc, CCM_AHB_GATING0);
	reg_value &= ~CCM_AHB_GATING_USB0; /* disable AHB clock gate usb0 */
	reg_value &= ~CCM_AHB_GATING_EHCI0; /* disable AHB clock gate ehci0 */
	reg_value &= ~CCM_AHB_GATING_EHCI1; /* disable AHB clock gate ehci1 */
	ccm_write_4(sc, CCM_AHB_GATING0, reg_value);

	return (0);
}

int
a10_clk_emac_activate(void)
{
	struct a10_ccm_softc *sc = a10_ccm_sc;
	uint32_t reg_value;

	if (sc == NULL)
		return (ENXIO);

	/* Gating AHB clock for EMAC */
	reg_value = ccm_read_4(sc, CCM_AHB_GATING0);
	reg_value |= CCM_AHB_GATING_EMAC;
	ccm_write_4(sc, CCM_AHB_GATING0, reg_value);

	return (0);
}

int
a10_clk_gmac_activate(phandle_t node)
{
	char *phy_type;
	struct a10_ccm_softc *sc;
	uint32_t reg_value;

	sc = a10_ccm_sc;
	if (sc == NULL)
		return (ENXIO);

	/* Gating AHB clock for GMAC */
	reg_value = ccm_read_4(sc, CCM_AHB_GATING1);
	reg_value |= CCM_AHB_GATING_GMAC;
	ccm_write_4(sc, CCM_AHB_GATING1, reg_value);

	/* Set GMAC mode. */
	reg_value = CCM_GMAC_CLK_MII;
	if (OF_getprop_alloc(node, "phy-type", 1, (void **)&phy_type) > 0) {
		if (strcasecmp(phy_type, "rgmii") == 0)
			reg_value = CCM_GMAC_CLK_RGMII | CCM_GMAC_MODE_RGMII;
		else if (strcasecmp(phy_type, "rgmii-bpi") == 0) {
			reg_value = CCM_GMAC_CLK_RGMII | CCM_GMAC_MODE_RGMII;
			reg_value |= (3 << CCM_GMAC_CLK_DELAY_SHIFT);
		}
		free(phy_type, M_OFWPROP);
	}
	ccm_write_4(sc, CCM_GMAC_CLK, reg_value);

	return (0);
}

static void
a10_clk_pll6_enable(void)
{
	struct a10_ccm_softc *sc;
	uint32_t reg_value;

	/*
	 * SATA needs PLL6 to be a 100MHz clock.
	 * The SATA output frequency is 24MHz * n * k / m / 6.
	 * To get to 100MHz, k & m must be equal and n must be 25.
	 * For other uses the output frequency is 24MHz * n * k / 2.
	 */
	sc = a10_ccm_sc;
	if (sc->pll6_enabled)
		return;
	reg_value = ccm_read_4(sc, CCM_PLL6_CFG);
	reg_value &= ~CCM_PLL_CFG_BYPASS;
	reg_value &= ~(CCM_PLL_CFG_FACTOR_K | CCM_PLL_CFG_FACTOR_M |
	    CCM_PLL_CFG_FACTOR_N);
	reg_value |= (25 << CCM_PLL_CFG_FACTOR_N_SHIFT);
	reg_value |= CCM_PLL6_CFG_SATA_CLKEN;
	reg_value |= CCM_PLL_CFG_ENABLE;
	ccm_write_4(sc, CCM_PLL6_CFG, reg_value);
	sc->pll6_enabled = 1;
}

static unsigned int
a10_clk_pll6_get_rate(void)
{
	struct a10_ccm_softc *sc;
	uint32_t k, n, reg_value;

	sc = a10_ccm_sc;
	reg_value = ccm_read_4(sc, CCM_PLL6_CFG);
	n = ((reg_value & CCM_PLL_CFG_FACTOR_N) >> CCM_PLL_CFG_FACTOR_N_SHIFT);
	k = ((reg_value & CCM_PLL_CFG_FACTOR_K) >> CCM_PLL_CFG_FACTOR_K_SHIFT) +
	    1;

	return ((CCM_CLK_REF_FREQ * n * k) / 2);
}

int
a10_clk_ahci_activate(void)
{
	struct a10_ccm_softc *sc;
	uint32_t reg_value;

	sc = a10_ccm_sc;
	if (sc == NULL)
		return (ENXIO);

	a10_clk_pll6_enable();

	/* Gating AHB clock for SATA */
	reg_value = ccm_read_4(sc, CCM_AHB_GATING0);
	reg_value |= CCM_AHB_GATING_SATA;
	ccm_write_4(sc, CCM_AHB_GATING0, reg_value);
	DELAY(1000);

	ccm_write_4(sc, CCM_SATA_CLK, CCM_PLL_CFG_ENABLE);

	return (0);
}

int
a10_clk_mmc_activate(int devid)
{
	struct a10_ccm_softc *sc;
	uint32_t reg_value;

	sc = a10_ccm_sc;
	if (sc == NULL)
		return (ENXIO);

	a10_clk_pll6_enable();

	/* Gating AHB clock for SD/MMC */
	reg_value = ccm_read_4(sc, CCM_AHB_GATING0);
	reg_value |= CCM_AHB_GATING_SDMMC0 << devid;
	ccm_write_4(sc, CCM_AHB_GATING0, reg_value);

	return (0);
}

int
a10_clk_mmc_cfg(int devid, int freq)
{
	struct a10_ccm_softc *sc;
	uint32_t clksrc, m, n, ophase, phase, reg_value;
	unsigned int pll_freq;

	sc = a10_ccm_sc;
	if (sc == NULL)
		return (ENXIO);

	freq /= 1000;
	if (freq <= 400) {
		pll_freq = CCM_CLK_REF_FREQ / 1000;
		clksrc = CCM_SD_CLK_SRC_SEL_OSC24M;
		ophase = 0;
		phase = 0;
		n = 2;
	} else if (freq <= 25000) {
		pll_freq = a10_clk_pll6_get_rate() / 1000;
		clksrc = CCM_SD_CLK_SRC_SEL_PLL6;
		ophase = 0;
		phase = 5;
		n = 2;
	} else if (freq <= 50000) {
		pll_freq = a10_clk_pll6_get_rate() / 1000;
		clksrc = CCM_SD_CLK_SRC_SEL_PLL6;
		ophase = 3;
		phase = 5;
		n = 0;
	} else
		return (EINVAL);
	m = ((pll_freq / (1 << n)) / (freq)) - 1;
	reg_value = ccm_read_4(sc, CCM_MMC0_SCLK_CFG + (devid * 4));
	reg_value &= ~CCM_SD_CLK_SRC_SEL;
	reg_value |= (clksrc << CCM_SD_CLK_SRC_SEL_SHIFT);
	reg_value &= ~CCM_SD_CLK_PHASE_CTR;
	reg_value |= (phase << CCM_SD_CLK_PHASE_CTR_SHIFT);
	reg_value &= ~CCM_SD_CLK_DIV_RATIO_N;
	reg_value |= (n << CCM_SD_CLK_DIV_RATIO_N_SHIFT);
	reg_value &= ~CCM_SD_CLK_OPHASE_CTR;
	reg_value |= (ophase << CCM_SD_CLK_OPHASE_CTR_SHIFT);
	reg_value &= ~CCM_SD_CLK_DIV_RATIO_M;
	reg_value |= m;
	reg_value |= CCM_PLL_CFG_ENABLE;
	ccm_write_4(sc, CCM_MMC0_SCLK_CFG + (devid * 4), reg_value);

	return (0);
}
