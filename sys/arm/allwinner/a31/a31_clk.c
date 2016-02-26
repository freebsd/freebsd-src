/*-
 * Copyright (c) 2013 Ganbold Tsagaankhuu <ganbold@freebsd.org>
 * Copyright (c) 2016 Emmanuel Vadot <manu@bidouilliste.com>
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

/*
 * Simple clock driver for Allwinner A31
 * Adapted from a10_clk.c
*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <machine/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/allwinner/a31/a31_clk.h>

struct a31_ccm_softc {
	struct resource		*res;
	int			pll6_enabled;
};

static struct a31_ccm_softc *a31_ccm_sc = NULL;

#define ccm_read_4(sc, reg)		\
	bus_read_4((sc)->res, (reg))
#define ccm_write_4(sc, reg, val)	\
	bus_write_4((sc)->res, (reg), (val))

#define PLL6_TIMEOUT	10

static int
a31_ccm_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "allwinner,sun6i-a31-ccm")) {
		device_set_desc(dev, "Allwinner Clock Control Module");
		return(BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
a31_ccm_attach(device_t dev)
{
	struct a31_ccm_softc *sc = device_get_softc(dev);
	int rid = 0;

	if (a31_ccm_sc)
		return (ENXIO);

	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (!sc->res) {
		device_printf(dev, "could not allocate resource\n");
		return (ENXIO);
	}

	a31_ccm_sc = sc;

	return (0);
}

static device_method_t a31_ccm_methods[] = {
	DEVMETHOD(device_probe,		a31_ccm_probe),
	DEVMETHOD(device_attach,	a31_ccm_attach),
	{ 0, 0 }
};

static driver_t a31_ccm_driver = {
	"a31_ccm",
	a31_ccm_methods,
	sizeof(struct a31_ccm_softc),
};

static devclass_t a31_ccm_devclass;

EARLY_DRIVER_MODULE(a31_ccm, simplebus, a31_ccm_driver, a31_ccm_devclass, 0, 0,
    BUS_PASS_TIMER + BUS_PASS_ORDER_MIDDLE);

static int
a31_clk_pll6_enable(void)
{
	struct a31_ccm_softc *sc;
	uint32_t reg_value;
	int i;

	/* Datasheet recommand to use the default 600Mhz value */
	sc = a31_ccm_sc;
	if (sc->pll6_enabled)
		return (0);
	reg_value = ccm_read_4(sc, A31_CCM_PLL6_CFG);
	reg_value |= A31_CCM_PLL_CFG_ENABLE;
	ccm_write_4(sc, A31_CCM_PLL6_CFG, reg_value);

	/* Wait for PLL to be stable */
	for (i = 0; i < PLL6_TIMEOUT; i++)
		if (!(ccm_read_4(sc, A31_CCM_PLL6_CFG) &
			A31_CCM_PLL6_CFG_REG_LOCK))
			break;
	if (i == PLL6_TIMEOUT)
		return (ENXIO);
	sc->pll6_enabled = 1;

	return (0);
}

static unsigned int
a31_clk_pll6_get_rate(void)
{
	struct a31_ccm_softc *sc;
	uint32_t k, n, reg_value;

	sc = a31_ccm_sc;
	reg_value = ccm_read_4(sc, A31_CCM_PLL6_CFG);
	n = ((reg_value & A31_CCM_PLL_CFG_FACTOR_N) >>
	        A31_CCM_PLL_CFG_FACTOR_N_SHIFT);
	k = ((reg_value & A31_CCM_PLL_CFG_FACTOR_K) >>
	        A31_CCM_PLL_CFG_FACTOR_K_SHIFT) + 1;

	return ((A31_CCM_CLK_REF_FREQ * n * k) / 2);
}

int
a31_clk_gmac_activate(phandle_t node)
{
	char *phy_type;
	struct a31_ccm_softc *sc;
	uint32_t reg_value;

	sc = a31_ccm_sc;
	if (sc == NULL)
		return (ENXIO);

	if (a31_clk_pll6_enable())
		return (ENXIO);

	/* Gating AHB clock for GMAC */
	reg_value = ccm_read_4(sc, A31_CCM_AHB_GATING0);
	reg_value |= A31_CCM_AHB_GATING_GMAC;
	ccm_write_4(sc, A31_CCM_AHB_GATING0, reg_value);

	/* Set GMAC mode. */
	reg_value = A31_CCM_GMAC_CLK_MII;
	if (OF_getprop_alloc(node, "phy-mode", 1, (void **)&phy_type) > 0) {
		if (strcasecmp(phy_type, "rgmii") == 0)
 			reg_value = A31_CCM_GMAC_CLK_RGMII |
				    A31_CCM_GMAC_MODE_RGMII;
		free(phy_type, M_OFWPROP);
	}
	ccm_write_4(sc, A31_CCM_GMAC_CLK, reg_value);

	/* Reset gmac */
	reg_value = ccm_read_4(sc, A31_CCM_AHB1_RST_REG0);
	reg_value |= A31_CCM_AHB1_RST_REG0_GMAC;
	ccm_write_4(sc, A31_CCM_AHB1_RST_REG0, reg_value);

	return (0);
}

int
a31_clk_mmc_activate(int devid)
{
	struct a31_ccm_softc *sc;
	uint32_t reg_value;

	sc = a31_ccm_sc;
	if (sc == NULL)
		return (ENXIO);

	if (a31_clk_pll6_enable())
		return (ENXIO);

	/* Gating AHB clock for SD/MMC */
	reg_value = ccm_read_4(sc, A31_CCM_AHB_GATING0);
	reg_value |= A31_CCM_AHB_GATING_SDMMC0 << devid;
	ccm_write_4(sc, A31_CCM_AHB_GATING0, reg_value);

	/* Soft reset */
	reg_value = ccm_read_4(sc, A31_CCM_AHB1_RST_REG0);
	reg_value |= A31_CCM_AHB1_RST_REG0_SDMMC << devid;
	ccm_write_4(sc, A31_CCM_AHB1_RST_REG0, reg_value);

	return (0);
}

int
a31_clk_mmc_cfg(int devid, int freq)
{
	struct a31_ccm_softc *sc;
	uint32_t clksrc, m, n, ophase, phase, reg_value;
	unsigned int pll_freq;

	sc = a31_ccm_sc;
	if (sc == NULL)
		return (ENXIO);

	freq /= 1000;
	if (freq <= 400) {
		pll_freq = A31_CCM_CLK_REF_FREQ / 1000;
		clksrc = A31_CCM_SD_CLK_SRC_SEL_OSC24M;
		ophase = 0;
		phase = 0;
		n = 2;
	} else if (freq <= 25000) {
		pll_freq = a31_clk_pll6_get_rate() / 1000;
		clksrc = A31_CCM_SD_CLK_SRC_SEL_PLL6;
		ophase = 0;
		phase = 5;
		n = 2;
	} else if (freq <= 50000) {
		pll_freq = a31_clk_pll6_get_rate() / 1000;
		clksrc = A31_CCM_SD_CLK_SRC_SEL_PLL6;
		ophase = 3;
		phase = 5;
		n = 0;
	} else
		return (EINVAL);
	m = ((pll_freq / (1 << n)) / (freq)) - 1;
	reg_value = ccm_read_4(sc, A31_CCM_MMC0_SCLK_CFG + (devid * 4));
	reg_value &= ~A31_CCM_SD_CLK_SRC_SEL;
	reg_value |= (clksrc << A31_CCM_SD_CLK_SRC_SEL_SHIFT);
	reg_value &= ~A31_CCM_SD_CLK_PHASE_CTR;
	reg_value |= (phase << A31_CCM_SD_CLK_PHASE_CTR_SHIFT);
	reg_value &= ~A31_CCM_SD_CLK_DIV_RATIO_N;
	reg_value |= (n << A31_CCM_SD_CLK_DIV_RATIO_N_SHIFT);
	reg_value &= ~A31_CCM_SD_CLK_OPHASE_CTR;
	reg_value |= (ophase << A31_CCM_SD_CLK_OPHASE_CTR_SHIFT);
	reg_value &= ~A31_CCM_SD_CLK_DIV_RATIO_M;
	reg_value |= m;
	reg_value |= A31_CCM_PLL_CFG_ENABLE;
	ccm_write_4(sc, A31_CCM_MMC0_SCLK_CFG + (devid * 4), reg_value);

	return (0);
}

int
a31_clk_i2c_activate(int devid)
{
	struct a31_ccm_softc *sc;
	uint32_t reg_value;

	sc = a31_ccm_sc;
	if (sc == NULL)
		return (ENXIO);

	if (a31_clk_pll6_enable())
		return (ENXIO);

	/* Gating APB clock for I2C/TWI */
	reg_value = ccm_read_4(sc, A31_CCM_APB2_GATING);
	reg_value |= A31_CCM_APB2_GATING_TWI << devid;
	ccm_write_4(sc, A31_CCM_APB2_GATING, reg_value);

	/* Soft reset */
	reg_value = ccm_read_4(sc, A31_CCM_APB2_RST);
	reg_value |= A31_CCM_APB2_RST_TWI << devid;
	ccm_write_4(sc, A31_CCM_APB2_RST, reg_value);

	return (0);
}
