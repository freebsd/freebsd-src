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
#include <dev/hwreset/hwreset.h>
#include <dev/ofw/openfirm.h>
#include <dev/syscon/syscon.h>
#include <dev/phy/phy.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcbrvar.h>

#include <dev/sdhci/sdhci.h>
#include <dev/sdhci/sdhci_fdt.h>

#include "mmcbr_if.h"
#include "sdhci_if.h"

#include "opt_mmccam.h"

#include "clkdev_if.h"
#include "syscon_if.h"

#define	SDHCI_FDT_RK3399	1
#define	SDHCI_FDT_RK3568	2
#define	SDHCI_FDT_RK3588	3

/*
 * RK3588 dwcmshc ("revision 1" in Linux's sdhci-of-dwcmshc): register
 * values cross-checked against RK3588 TRM v1.0 Part2, chapter 4.4
 * (Host Control 2, EMMC_CTRL, DLL registers) and 4.6.1 Clock Adjustment.
 * Tap counts follow that driver.  The SDCLK divider bits are not functional on
 * Rockchip's dwcmshc; the card clock is cclk_emmc from the CRU, so the
 * divider is always left at 0 (base clock).
 */
#define	DWCMSHC_P_VENDOR_AREA1			0xe8
#define	 DWCMSHC_AREA1_MASK			0xfff
#define	DWCMSHC_HOST_CTRL3			0x08	/* in vendor area 1 */
#define	 DWCMSHC_HOST_CTRL3_CMD_CONFLICT	(1 << 0)
#define	 DWCMSHC_HOST_CTRL3_CLK_GATE_DIS	(1 << 4)
#define	DWCMSHC_EMMC_CONTROL			0x2c
#define	 DWCMSHC_CARD_IS_EMMC			(1 << 0)
#define	 DWCMSHC_ENHANCED_STROBE		(1 << 8)
#define	DWCMSHC_CTRL_HS400			0x7
#define	DWCMSHC_EMMC_ATCTRL			0x40	/* in vendor area 1 */
#define	DWCMSHC_EMMC_DLL_CTRL			0x800
#define	 DWCMSHC_DLL_START			(1 << 0)
#define	 DWCMSHC_DLL_SRST			(1 << 1)
#define	 DWCMSHC_DLL_INC_SHIFT			8
#define	 DWCMSHC_DLL_START_POINT_SHIFT		16
#define	 DWCMSHC_DLL_BYPASS			(1 << 24)
#define	DWCMSHC_EMMC_DLL_RXCLK			0x804
#define	DWCMSHC_EMMC_DLL_TXCLK			0x808
#define	DWCMSHC_EMMC_DLL_STRBIN			0x80c
#define	DWCMSHC_EMMC_DLL_CMDOUT			0x810
#define	DWCMSHC_EMMC_DLL_STATUS0		0x840
#define	 DWCMSHC_DLL_LOCKED			(1 << 8)
#define	 DWCMSHC_DLL_TIMEOUT			(1 << 9)
#define	DWCMSHC_DLL_DLYENA			(1 << 27)
#define	DWCMSHC_DLL_RXCLK_SRCSEL_SHIFT		29
#define	DWCMSHC_DLL_RXCLK_ORI_GATE		(1U << 31)
#define	DWCMSHC_DLL_TAPNUM_FROM_SW		(1 << 24)
#define	DWCMSHC_DLL_TXCLK_TAPNUM_DEFAULT	0x10
#define	DWCMSHC_DLL_TXCLK_TAPNUM_90		0x0a
#define	DWCMSHC_DLL_STRBIN_TAPNUM_DEFAULT	0x04
#define	DWCMSHC_DLL_STRBIN_DELAY_NUM_SEL	(1 << 26)
#define	DWCMSHC_DLL_STRBIN_DELAY_NUM_SHIFT	16
#define	DWCMSHC_DLL_STRBIN_DELAY_NUM_DEFAULT	0x16
#define	DWCMSHC_DLL_CMDOUT_TAPNUM_90		0x08
#define	DWCMSHC_DLL_CMDOUT_SRC_CLK_NEG		(1 << 28)
#define	DWCMSHC_DLL_CMDOUT_EN_SRC_CLK_NEG	(1 << 29)

/*
 * Keep fast modes opt-in until the board's DT and electrical support
 * have been verified; otherwise retain MMC high-speed at <= 52 MHz.
 */
static int sdhci_rockchip_hs200 = 0;
SYSCTL_NODE(_hw, OID_AUTO, sdhci_rockchip, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Rockchip SDHCI");
SYSCTL_INT(_hw_sdhci_rockchip, OID_AUTO, hs200, CTLFLAG_RDTUN,
    &sdhci_rockchip_hs200, 0, "Allow HS200 on RK3588 eMMC");
static int sdhci_rockchip_hs400es = 0;
SYSCTL_INT(_hw_sdhci_rockchip, OID_AUTO, hs400es, CTLFLAG_RDTUN,
    &sdhci_rockchip_hs400es, 0, "Allow HS400 enhanced strobe on RK3588 eMMC");

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
	{ "rockchip,rk3588-dwcmshc",	SDHCI_FDT_RK3588 },
	{ NULL, 0 }
};

static int
sdhci_fdt_rockchip_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	switch (ofw_bus_search_compatible(dev, compat_data)->ocd_data) {
	case SDHCI_FDT_RK3399:
		device_set_desc(dev, "Rockchip RK3399 fdt SDHCI controller");
		break;
	case SDHCI_FDT_RK3568:
		device_set_desc(dev, "Rockchip RK3568 fdt SDHCI controller");
		break;
	case SDHCI_FDT_RK3588:
		/* The RK3588 class supplies its timing and transfer methods. */
		return (ENXIO);
	default:
		return (ENXIO);
	}

	return (BUS_PROBE_DEFAULT);
}

static int
sdhci_fdt_rk3588_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev) ||
	    ofw_bus_search_compatible(dev, compat_data)->ocd_data !=
	    SDHCI_FDT_RK3588)
		return (ENXIO);
	device_set_desc(dev, "Rockchip RK3588 fdt SDHCI controller");
	return (BUS_PROBE_DEFAULT);
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
sdhci_rk3588_set_clock(device_t dev, struct sdhci_slot *slot, int clock)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);
	struct resource *res = sc->mem_res[slot->num];
	uint32_t area1, extra, txclk;
	int i;

	if (clock == 0)
		return (0);
	/* Rockchip only supports 375 kHz for identification. */
	if (clock <= 400000)
		clock = 375000;
	/* Keep SDCLK stopped while changing its CRU parent and DLL. */
	bus_write_2(res, SDHCI_CLOCK_CONTROL, 0);
	if (clk_set_freq(sc->clk_core, clock, CLK_SET_ROUND_DOWN) != 0)
		device_printf(dev, "cannot set core clock to %d\n", clock);

	area1 = bus_read_2(res, DWCMSHC_P_VENDOR_AREA1) & DWCMSHC_AREA1_MASK;

	/* No command-conflict check; no internal clock gating. */
	extra = bus_read_4(res, area1 + DWCMSHC_HOST_CTRL3);
	extra &= ~DWCMSHC_HOST_CTRL3_CMD_CONFLICT;
	extra |= DWCMSHC_HOST_CTRL3_CLK_GATE_DIS;
	bus_write_4(res, area1 + DWCMSHC_HOST_CTRL3, extra);

	/* Stop the card clock while the DLL is reconfigured. */
	bus_write_2(res, SDHCI_CLOCK_CONTROL, 0);

	if (clock <= 52000000) {
		/* DLL bypassed; strobe input set up for HS400ES ahead of time. */
		bus_write_4(res, DWCMSHC_EMMC_DLL_CTRL,
		    DWCMSHC_DLL_BYPASS | DWCMSHC_DLL_START);
		bus_write_4(res, DWCMSHC_EMMC_DLL_RXCLK,
		    DWCMSHC_DLL_RXCLK_ORI_GATE);
		bus_write_4(res, DWCMSHC_EMMC_DLL_TXCLK, 0);
		bus_write_4(res, DWCMSHC_EMMC_DLL_CMDOUT, 0);
		bus_write_4(res, DWCMSHC_EMMC_DLL_STRBIN, DWCMSHC_DLL_DLYENA |
		    DWCMSHC_DLL_STRBIN_DELAY_NUM_SEL |
		    (DWCMSHC_DLL_STRBIN_DELAY_NUM_DEFAULT <<
		    DWCMSHC_DLL_STRBIN_DELAY_NUM_SHIFT));
		goto out;
	}

	bus_write_4(res, DWCMSHC_EMMC_DLL_CTRL, DWCMSHC_DLL_SRST);
	DELAY(1);
	bus_write_4(res, DWCMSHC_EMMC_DLL_CTRL, 0);
	bus_write_4(res, DWCMSHC_EMMC_DLL_RXCLK, DWCMSHC_DLL_DLYENA);
	bus_write_4(res, DWCMSHC_EMMC_DLL_CTRL,
	    (0x5 << DWCMSHC_DLL_START_POINT_SHIFT) |
	    (0x2 << DWCMSHC_DLL_INC_SHIFT) | DWCMSHC_DLL_START);
	for (i = 0; i < 500; i++) {
		extra = bus_read_4(res, DWCMSHC_EMMC_DLL_STATUS0);
		if ((extra & DWCMSHC_DLL_LOCKED) != 0 &&
		    (extra & DWCMSHC_DLL_TIMEOUT) == 0)
			break;
		DELAY(1000);
	}
	if (i == 500) {
		device_printf(dev, "DLL lock timeout\n");
		goto out;
	}

	bus_write_4(res, area1 + DWCMSHC_EMMC_ATCTRL,
	    (0x1 << 16) | (0x3 << 17) | (0x3 << 19));

	txclk = DWCMSHC_DLL_TXCLK_TAPNUM_DEFAULT;
	if (slot->host.ios.timing == bus_timing_mmc_hs400 ||
	    slot->host.ios.timing == bus_timing_mmc_hs400es) {
		txclk = DWCMSHC_DLL_TXCLK_TAPNUM_90;
		bus_write_4(res, DWCMSHC_EMMC_DLL_CMDOUT,
		    DWCMSHC_DLL_CMDOUT_SRC_CLK_NEG |
		    DWCMSHC_DLL_CMDOUT_EN_SRC_CLK_NEG | DWCMSHC_DLL_DLYENA |
		    DWCMSHC_DLL_CMDOUT_TAPNUM_90 | DWCMSHC_DLL_TAPNUM_FROM_SW);
	}
	bus_write_4(res, DWCMSHC_EMMC_DLL_TXCLK, DWCMSHC_DLL_DLYENA |
	    DWCMSHC_DLL_TAPNUM_FROM_SW |
	    (1 << DWCMSHC_DLL_RXCLK_SRCSEL_SHIFT) | txclk);
	bus_write_4(res, DWCMSHC_EMMC_DLL_STRBIN, DWCMSHC_DLL_DLYENA |
	    DWCMSHC_DLL_STRBIN_TAPNUM_DEFAULT | DWCMSHC_DLL_TAPNUM_FROM_SW);

out:
	/* Divider 0: the sdhci core must not divide the CRU clock again. */
	slot->max_clk = clock;
	return (clock);
}

static int
sdhci_fdt_rockchip_set_clock(device_t dev, struct sdhci_slot *slot, int clock)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);
	int32_t val;
	int i;

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data ==
	    SDHCI_FDT_RK3588)
		return (sdhci_rk3588_set_clock(dev, slot, clock));

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
sdhci_fdt_rockchip_read_ivar(device_t bus, device_t child, int which,
    uintptr_t *result)
{
	struct sdhci_slot *slot = device_get_ivars(child);
	int error;

	error = sdhci_generic_read_ivar(bus, child, which, result);
	if (error == 0 && which == MMCBR_IVAR_MAX_DATA &&
	    ofw_bus_search_compatible(bus, compat_data)->ocd_data ==
	    SDHCI_FDT_RK3588 && (slot->opt & SDHCI_HAVE_DMA) != 0) {
		/*
		 * Resuming SDMA after recycling its bounce buffer loses data on
		 * this controller. Keep each command within one buffer; mmcsd
		 * splits larger IO using the bridge's advertised block limit.
		 */
		*result = MIN(*result, slot->sdma_bbufsz / MMC_SECTOR_SIZE);
	}
	return (error);
}

/*
 * RK3588 takes its card clock directly from the CRU.  max_clk follows the
 * current rate so the SDHCI divider remains zero.  Generic ivar rounding
 * against that rate otherwise traps every request at the 375 kHz
 * identification clock.  Preserve the request until set_clock programs
 * the CRU; other Rockchip controllers keep the generic divider policy.
 */
static int
sdhci_fdt_rockchip_write_ivar(device_t bus, device_t child, int which,
    uintptr_t value)
{
	struct sdhci_slot *slot = device_get_ivars(child);

	if (which == MMCBR_IVAR_CLOCK && value > 0 &&
	    ofw_bus_search_compatible(bus, compat_data)->ocd_data ==
	    SDHCI_FDT_RK3588) {
		slot->host.ios.clock = MIN(value, slot->host.f_max);
		return (0);
	}
	return (sdhci_generic_write_ivar(bus, child, which, value));
}

/*
 * dwcmshc encodes MMC high-speed timing as SDR25, even at 52 MHz.
 * Leaving mode SDR12 or deriving SDR50 from the numeric clock is wrong.
 * SDHCI Host Control 2 (0x3e), Bus Speed Select bits [2:0].
 */
static void
sdhci_fdt_rockchip_set_uhs_timing(device_t dev, struct sdhci_slot *slot)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);
	struct resource *res = sc->mem_res[slot->num];
	uint32_t area1, emmc;
	uint16_t clk, ctrl2, mode;

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data !=
	    SDHCI_FDT_RK3588)
		return;
	switch (slot->host.ios.timing) {
	case bus_timing_normal:
		mode = SDHCI_CTRL2_UHS_SDR12;
		break;
	case bus_timing_hs:
		mode = SDHCI_CTRL2_UHS_SDR25;
		break;
	case bus_timing_mmc_ddr52:
		mode = SDHCI_CTRL2_UHS_DDR50;
		break;
	case bus_timing_mmc_hs200:
		mode = SDHCI_CTRL2_UHS_SDR104;
		break;
	case bus_timing_mmc_hs400:
	case bus_timing_mmc_hs400es:
		mode = DWCMSHC_CTRL_HS400;
		break;
	default:
		return;
	}
	clk = bus_read_2(res, SDHCI_CLOCK_CONTROL);
	bus_write_2(res, SDHCI_CLOCK_CONTROL, clk & ~SDHCI_CLOCK_CARD_EN);
	ctrl2 = bus_read_2(res, SDHCI_HOST_CONTROL2);
	ctrl2 = (ctrl2 & ~SDHCI_CTRL2_UHS_MASK) | mode;
	bus_write_2(res, SDHCI_HOST_CONTROL2, ctrl2);
	area1 = bus_read_2(res, DWCMSHC_P_VENDOR_AREA1) & DWCMSHC_AREA1_MASK;
	emmc = bus_read_4(res, area1 + DWCMSHC_EMMC_CONTROL);
	emmc |= DWCMSHC_CARD_IS_EMMC;
	if (slot->host.ios.timing == bus_timing_mmc_hs400es)
		emmc |= DWCMSHC_ENHANCED_STROBE;
	else
		emmc &= ~DWCMSHC_ENHANCED_STROBE;
	bus_write_4(res, area1 + DWCMSHC_EMMC_CONTROL, emmc);
	bus_write_2(res, SDHCI_CLOCK_CONTROL, clk);
}

static int
sdhci_rk3588_init(device_t dev, struct sdhci_fdt_softc *sc)
{
	static const char *names[] = { "core", "bus", "axi", "block", "timer" };
	hwreset_t rst;
	clk_t clk;
	int i;

	if (clk_get_by_ofw_name(dev, 0, "core", &sc->clk_core) != 0) {
		device_printf(dev, "cannot get core clock\n");
		return (ENXIO);
	}
	for (i = 0; i < nitems(names); i++) {
		if (clk_get_by_ofw_name(dev, 0, names[i], &clk) != 0 ||
		    clk_enable(clk) != 0)
			device_printf(dev, "cannot enable %s clock\n", names[i]);
		if (hwreset_get_by_ofw_name(dev, 0, __DECONST(char *, names[i]),
		    &rst) == 0)
			hwreset_deassert(rst);
	}

	sc->quirks |= SDHCI_QUIRK_BROKEN_TIMEOUT_VAL |
	    SDHCI_QUIRK_PRESET_VALUE_BROKEN;
	/* Fixed 1.8 V eMMC: expose only modes authorized by DT and tunables. */
	sc->caps_clear = MMC_CAP_MMC_HS200 | MMC_CAP_MMC_HS400 |
	    MMC_CAP_MMC_ENH_STROBE | MMC_CAP_SIGNALING_120;
	if (sdhci_rockchip_hs200 != 0 || sdhci_rockchip_hs400es != 0) {
		if (OF_hasprop(ofw_bus_get_node(dev), "mmc-hs200-1_8v") ||
		    OF_hasprop(ofw_bus_get_node(dev), "mmc-hs400-1_8v")) {
			/* CAPABILITIES reports 52 MHz, not the CRU rate ceiling. */
			sc->f_max_override = 200000000;
			sc->caps_set |= MMC_CAP_MMC_HS200_180;
			sc->caps_clear &= ~MMC_CAP_MMC_HS200_180;
		}
	}
	if (sdhci_rockchip_hs400es != 0 &&
	    OF_hasprop(ofw_bus_get_node(dev), "mmc-hs400-1_8v") &&
	    OF_hasprop(ofw_bus_get_node(dev), "mmc-hs400-enhanced-strobe")) {
		sc->caps_set |= MMC_CAP_MMC_HS400_180 | MMC_CAP_MMC_ENH_STROBE;
		sc->caps_clear &= ~(MMC_CAP_MMC_HS400_180 | MMC_CAP_MMC_ENH_STROBE);
	}
	return (0);
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
	case SDHCI_FDT_RK3588:
		err = sdhci_rk3588_init(dev, sc);
		if (err != 0)
			return (err);
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

/*
 * Method presence controls capability discovery in sdhci_init_slot().
 * Keep RK3588 timing methods out of the RK3399/RK3568 class: even a callback
 * that returns without writing would advertise previously disabled modes.
 */
static device_method_t sdhci_fdt_rk3588_methods[] = {
	DEVMETHOD(device_probe,		sdhci_fdt_rk3588_probe),
	DEVMETHOD(bus_read_ivar,		sdhci_fdt_rockchip_read_ivar),
	DEVMETHOD(bus_write_ivar,	sdhci_fdt_rockchip_write_ivar),
	DEVMETHOD(sdhci_set_uhs_timing,	sdhci_fdt_rockchip_set_uhs_timing),
	DEVMETHOD(mmcbr_switch_vccq,	sdhci_generic_switch_vccq),
	DEVMETHOD(mmcbr_tune,		sdhci_generic_tune),
	DEVMETHOD(mmcbr_retune,		sdhci_generic_retune),

	DEVMETHOD_END
};
DEFINE_CLASS_1(sdhci_rk3588, sdhci_fdt_rk3588_driver, sdhci_fdt_rk3588_methods,
    sizeof(struct sdhci_fdt_softc), sdhci_fdt_rockchip_driver);
DRIVER_MODULE(sdhci_rk3588, simplebus, sdhci_fdt_rk3588_driver, NULL, NULL);
