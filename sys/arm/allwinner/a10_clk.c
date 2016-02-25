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

#define	TCON_PLL_WORST		1000000
#define	TCON_PLL_N_MIN		1
#define	TCON_PLL_N_MAX		15
#define	TCON_PLL_M_MIN		9
#define	TCON_PLL_M_MAX		127
#define	TCON_PLLREF_SINGLE	3000	/* kHz */
#define	TCON_PLLREF_DOUBLE	6000	/* kHz */
#define	TCON_RATE_KHZ(rate_hz)	((rate_hz) / 1000)
#define	TCON_RATE_HZ(rate_khz)	((rate_khz) * 1000)
#define	HDMI_DEFAULT_RATE	297000000
#define	DEBE_DEFAULT_RATE	300000000

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

EARLY_DRIVER_MODULE(a10_ccm, simplebus, a10_ccm_driver, a10_ccm_devclass, 0, 0,
    BUS_PASS_TIMER + BUS_PASS_ORDER_MIDDLE);

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
	if (OF_getprop_alloc(node, "phy-mode", 1, (void **)&phy_type) > 0) {
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

static int
a10_clk_pll2_set_rate(unsigned int freq)
{
	struct a10_ccm_softc *sc;
	uint32_t reg_value;
	unsigned int prediv, postdiv, n;

	sc = a10_ccm_sc;
	if (sc == NULL)
		return (ENXIO);

	reg_value = ccm_read_4(sc, CCM_PLL2_CFG);
	reg_value &= ~(CCM_PLL2_CFG_PREDIV | CCM_PLL2_CFG_POSTDIV |
	    CCM_PLL_CFG_FACTOR_N);

	/*
	 * Audio Codec needs PLL2 to be either 24576000 Hz or 22579200 Hz
	 *
	 * PLL2 output frequency is 24MHz * n / prediv / postdiv.
	 * To get as close as possible to the desired rate, we use a
	 * pre-divider of 21 and a post-divider of 4. With these values,
	 * a multiplier of 86 or 79 gets us close to the target rates.
	 */
	prediv = 21;
	postdiv = 4;

	switch (freq) {
	case 24576000:
		n = 86;
		reg_value |= CCM_PLL_CFG_ENABLE;
		break;
	case 22579200:
		n = 79;
		reg_value |= CCM_PLL_CFG_ENABLE;
		break;
	case 0:
		n = 1;
		reg_value &= ~CCM_PLL_CFG_ENABLE;
		break;
	default:
		return (EINVAL);
	}

	reg_value |= (prediv << CCM_PLL2_CFG_PREDIV_SHIFT);
	reg_value |= (postdiv << CCM_PLL2_CFG_POSTDIV_SHIFT);
	reg_value |= (n << CCM_PLL_CFG_FACTOR_N_SHIFT);
	ccm_write_4(sc, CCM_PLL2_CFG, reg_value);

	return (0);
}

static int
a10_clk_pll3_set_rate(unsigned int freq)
{
	struct a10_ccm_softc *sc;
	uint32_t reg_value;
	int m;

	sc = a10_ccm_sc;
	if (sc == NULL)
		return (ENXIO);

	if (freq == 0) {
		/* Disable PLL3 */
		ccm_write_4(sc, CCM_PLL3_CFG, 0);
		return (0);
	}

	m = freq / TCON_RATE_HZ(TCON_PLLREF_SINGLE);

	reg_value = CCM_PLL_CFG_ENABLE | CCM_PLL3_CFG_MODE_SEL_INT | m;
	ccm_write_4(sc, CCM_PLL3_CFG, reg_value);

	return (0);
}

static unsigned int
a10_clk_pll5x_get_rate(void)
{
	struct a10_ccm_softc *sc;
	uint32_t k, n, p, reg_value;

	sc = a10_ccm_sc;
	reg_value = ccm_read_4(sc, CCM_PLL5_CFG);
	n = ((reg_value & CCM_PLL_CFG_FACTOR_N) >> CCM_PLL_CFG_FACTOR_N_SHIFT);
	k = ((reg_value & CCM_PLL_CFG_FACTOR_K) >> CCM_PLL_CFG_FACTOR_K_SHIFT) +
	    1;
	p = ((reg_value & CCM_PLL5_CFG_OUT_EXT_DIV_P) >> CCM_PLL5_CFG_OUT_EXT_DIV_P_SHIFT);

	return ((CCM_CLK_REF_FREQ * n * k) >> p);
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

int
a10_clk_i2c_activate(int devid)
{
	struct a10_ccm_softc *sc;
	uint32_t reg_value;

	sc = a10_ccm_sc;
	if (sc == NULL)
		return (ENXIO);

	a10_clk_pll6_enable();

	/* Gating APB clock for I2C/TWI */
	reg_value = ccm_read_4(sc, CCM_APB1_GATING);
	if (devid == 4)
		reg_value |= CCM_APB1_GATING_TWI << 15;
	else
		reg_value |= CCM_APB1_GATING_TWI << devid;
	ccm_write_4(sc, CCM_APB1_GATING, reg_value);

	return (0);
}

int
a10_clk_dmac_activate(void)
{
	struct a10_ccm_softc *sc;
	uint32_t reg_value;

	sc = a10_ccm_sc;
	if (sc == NULL)
		return (ENXIO);

	/* Gating AHB clock for DMA controller */
	reg_value = ccm_read_4(sc, CCM_AHB_GATING0);
	reg_value |= CCM_AHB_GATING_DMA;
	ccm_write_4(sc, CCM_AHB_GATING0, reg_value);

	return (0);
}

int
a10_clk_codec_activate(unsigned int freq)
{
	struct a10_ccm_softc *sc;
	uint32_t reg_value;

	sc = a10_ccm_sc;
	if (sc == NULL)
		return (ENXIO);

	a10_clk_pll2_set_rate(freq);

	/* Gating APB clock for ADDA */
	reg_value = ccm_read_4(sc, CCM_APB0_GATING);
	reg_value |= CCM_APB0_GATING_ADDA;
	ccm_write_4(sc, CCM_APB0_GATING, reg_value);

	/* Enable audio codec clock */
	reg_value = ccm_read_4(sc, CCM_AUDIO_CODEC_CLK);
	reg_value |= CCM_AUDIO_CODEC_ENABLE;
	ccm_write_4(sc, CCM_AUDIO_CODEC_CLK, reg_value);

	return (0);
}

static void
calc_tcon_pll(int f_ref, int f_out, int *pm, int *pn)
{
	int best, m, n, f_cur, diff;

	best = TCON_PLL_WORST;
	for (n = TCON_PLL_N_MIN; n <= TCON_PLL_N_MAX; n++) {
		for (m = TCON_PLL_M_MIN; m <= TCON_PLL_M_MAX; m++) {
			f_cur = (m * f_ref) / n;
			diff = f_out - f_cur;
			if (diff > 0 && diff < best) {
				best = diff;
				*pm = m;
				*pn = n;
			}
		}
	}
}

int
a10_clk_debe_activate(void)
{
	struct a10_ccm_softc *sc;
	int pll_rate, clk_div;
	uint32_t reg_value;

	sc = a10_ccm_sc;
	if (sc == NULL)
		return (ENXIO);

	/* Leave reset */
	reg_value = ccm_read_4(sc, CCM_BE0_SCLK);
	reg_value |= CCM_BE_CLK_RESET;
	ccm_write_4(sc, CCM_BE0_SCLK, reg_value);

	pll_rate = a10_clk_pll5x_get_rate();

	clk_div = howmany(pll_rate, DEBE_DEFAULT_RATE);

	/* Set BE0 source to PLL5 (DDR external peripheral clock) */
	reg_value = CCM_BE_CLK_RESET;
	reg_value |= (CCM_BE_CLK_SRC_SEL_PLL5 << CCM_BE_CLK_SRC_SEL_SHIFT);
	reg_value |= (clk_div - 1);
	ccm_write_4(sc, CCM_BE0_SCLK, reg_value);

	/* Gating AHB clock for BE0 */
	reg_value = ccm_read_4(sc, CCM_AHB_GATING1);
	reg_value |= CCM_AHB_GATING_DE_BE0;
	ccm_write_4(sc, CCM_AHB_GATING1, reg_value);

	/* Enable DRAM clock to BE0 */
	reg_value = ccm_read_4(sc, CCM_DRAM_CLK);
	reg_value |= CCM_DRAM_CLK_BE0_CLK_ENABLE;
	ccm_write_4(sc, CCM_DRAM_CLK, reg_value);

	/* Enable BE0 clock */
	reg_value = ccm_read_4(sc, CCM_BE0_SCLK);
	reg_value |= CCM_BE_CLK_SCLK_GATING;
	ccm_write_4(sc, CCM_BE0_SCLK, reg_value);

	return (0);
}

int
a10_clk_lcd_activate(void)
{
	struct a10_ccm_softc *sc;
	uint32_t reg_value;

	sc = a10_ccm_sc;
	if (sc == NULL)
		return (ENXIO);

	/* Clear LCD0 reset */
	reg_value = ccm_read_4(sc, CCM_LCD0_CH0_CLK);
	reg_value |= CCM_LCD_CH0_RESET;
	ccm_write_4(sc, CCM_LCD0_CH0_CLK, reg_value);

	/* Gating AHB clock for LCD0 */
	reg_value = ccm_read_4(sc, CCM_AHB_GATING1);
	reg_value |= CCM_AHB_GATING_LCD0;
	ccm_write_4(sc, CCM_AHB_GATING1, reg_value);

	return (0);
}

int
a10_clk_tcon_activate(unsigned int freq)
{
	struct a10_ccm_softc *sc;
	int m, n, m2, n2, f_single, f_double, dbl, src_sel;

	sc = a10_ccm_sc;
	if (sc == NULL)
		return (ENXIO);

	m = n = m2 = n2 = 0;
	dbl = 0;

	calc_tcon_pll(TCON_PLLREF_SINGLE, TCON_RATE_KHZ(freq), &m, &n);
	calc_tcon_pll(TCON_PLLREF_DOUBLE, TCON_RATE_KHZ(freq), &m2, &n2);

	f_single = n ? (m * TCON_PLLREF_SINGLE) / n : 0;
	f_double = n2 ? (m2 * TCON_PLLREF_DOUBLE) / n2 : 0;

	if (f_double > f_single) {
		dbl = 1;
		m = m2;
		n = n2;
	}
	src_sel = dbl ? CCM_LCD_CH1_SRC_SEL_PLL3_2X : CCM_LCD_CH1_SRC_SEL_PLL3;

	if (n == 0 || m == 0)
		return (EINVAL);

	/* Set PLL3 to the closest possible rate */
	a10_clk_pll3_set_rate(TCON_RATE_HZ(m * TCON_PLLREF_SINGLE));

	/* Enable LCD0 CH1 clock */
	ccm_write_4(sc, CCM_LCD0_CH1_CLK,
	    CCM_LCD_CH1_SCLK2_GATING | CCM_LCD_CH1_SCLK1_GATING |
	    (src_sel << CCM_LCD_CH1_SRC_SEL_SHIFT) | (n - 1));

	return (0);
}

int
a10_clk_tcon_get_config(int *pdiv, int *pdbl)
{
	struct a10_ccm_softc *sc;
	uint32_t reg_value;
	int src;

	sc = a10_ccm_sc;
	if (sc == NULL)
		return (ENXIO);

	reg_value = ccm_read_4(sc, CCM_LCD0_CH1_CLK);

	*pdiv = (reg_value & CCM_LCD_CH1_CLK_DIV_RATIO_M) + 1;

	src = (reg_value & CCM_LCD_CH1_SRC_SEL) >> CCM_LCD_CH1_SRC_SEL_SHIFT;
	switch (src) {
	case CCM_LCD_CH1_SRC_SEL_PLL3:
	case CCM_LCD_CH1_SRC_SEL_PLL7:
		*pdbl = 0;
		break;
	case CCM_LCD_CH1_SRC_SEL_PLL3_2X:
	case CCM_LCD_CH1_SRC_SEL_PLL7_2X:
		*pdbl = 1;
		break;
	}

	return (0);
}

int
a10_clk_hdmi_activate(void)
{
	struct a10_ccm_softc *sc;
	uint32_t reg_value;
	int error;

	sc = a10_ccm_sc;
	if (sc == NULL)
		return (ENXIO);

	/* Set PLL3 to 297MHz */
	error = a10_clk_pll3_set_rate(HDMI_DEFAULT_RATE);
	if (error != 0)
		return (error);

	/* Enable HDMI clock, source PLL3 */
	reg_value = ccm_read_4(sc, CCM_HDMI_CLK);
	reg_value |= CCM_HDMI_CLK_SCLK_GATING;
	reg_value &= ~CCM_HDMI_CLK_SRC_SEL;
	reg_value |= (CCM_HDMI_CLK_SRC_SEL_PLL3 << CCM_HDMI_CLK_SRC_SEL_SHIFT);
	ccm_write_4(sc, CCM_HDMI_CLK, reg_value);

	/* Gating AHB clock for HDMI */
	reg_value = ccm_read_4(sc, CCM_AHB_GATING1);
	reg_value |= CCM_AHB_GATING_HDMI;
	ccm_write_4(sc, CCM_AHB_GATING1, reg_value);

	return (0);
}
