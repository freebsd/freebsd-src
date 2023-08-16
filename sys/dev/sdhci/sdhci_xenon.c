/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Rubicon Communications, LLC (Netgate)
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

/*
 * Marvell Xenon SDHCI controller driver.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/extres/regulator/regulator.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcbrvar.h>
#include <dev/mmc/mmcreg.h>

#include <dev/sdhci/sdhci.h>
#include <dev/sdhci/sdhci_xenon.h>

#include "mmcbr_if.h"
#include "sdhci_if.h"

#include "opt_mmccam.h"
#include "opt_soc.h"

#define	MAX_SLOTS		6

static uint8_t
sdhci_xenon_read_1(device_t dev, struct sdhci_slot *slot __unused,
    bus_size_t off)
{
	struct sdhci_xenon_softc *sc = device_get_softc(dev);

	return (bus_read_1(sc->mem_res, off));
}

static void
sdhci_xenon_write_1(device_t dev, struct sdhci_slot *slot __unused,
    bus_size_t off, uint8_t val)
{
	struct sdhci_xenon_softc *sc = device_get_softc(dev);

	bus_write_1(sc->mem_res, off, val);
}

static uint16_t
sdhci_xenon_read_2(device_t dev, struct sdhci_slot *slot __unused,
    bus_size_t off)
{
	struct sdhci_xenon_softc *sc = device_get_softc(dev);

	return (bus_read_2(sc->mem_res, off));
}

static void
sdhci_xenon_write_2(device_t dev, struct sdhci_slot *slot __unused,
    bus_size_t off, uint16_t val)
{
	struct sdhci_xenon_softc *sc = device_get_softc(dev);

	bus_write_2(sc->mem_res, off, val);
}

static uint32_t
sdhci_xenon_read_4(device_t dev, struct sdhci_slot *slot __unused,
    bus_size_t off)
{
	struct sdhci_xenon_softc *sc = device_get_softc(dev);

	return bus_read_4(sc->mem_res, off);
}

static void
sdhci_xenon_write_4(device_t dev, struct sdhci_slot *slot __unused,
    bus_size_t off, uint32_t val)
{
	struct sdhci_xenon_softc *sc = device_get_softc(dev);

	bus_write_4(sc->mem_res, off, val);
}

static void
sdhci_xenon_read_multi_4(device_t dev, struct sdhci_slot *slot __unused,
    bus_size_t off, uint32_t *data, bus_size_t count)
{
	struct sdhci_xenon_softc *sc = device_get_softc(dev);

	bus_read_multi_4(sc->mem_res, off, data, count);
}

static void
sdhci_xenon_write_multi_4(device_t dev, struct sdhci_slot *slot __unused,
    bus_size_t off, uint32_t *data, bus_size_t count)
{
	struct sdhci_xenon_softc *sc = device_get_softc(dev);

	bus_write_multi_4(sc->mem_res, off, data, count);
}

static void
sdhci_xenon_intr(void *arg)
{
	struct sdhci_xenon_softc *sc = (struct sdhci_xenon_softc *)arg;

	sdhci_generic_intr(sc->slot);
}

static int
sdhci_xenon_get_ro(device_t bus, device_t dev)
{
	struct sdhci_xenon_softc *sc = device_get_softc(bus);

	return (sdhci_generic_get_ro(bus, dev) ^ sc->wp_inverted);
}

static void
sdhci_xenon_set_uhs_timing(device_t brdev, struct sdhci_slot *slot)
{
	const struct mmc_ios *ios;
	uint16_t hostctrl2;

	if (slot->version < SDHCI_SPEC_300)
		return;

	mtx_assert(&slot->mtx, MA_OWNED);
	ios = &slot->host.ios;

	/* Update timing parameteres in SDHCI_HOST_CONTROL2 register. */
	hostctrl2 = sdhci_xenon_read_2(brdev, slot, SDHCI_HOST_CONTROL2);
	hostctrl2 &= ~SDHCI_CTRL2_UHS_MASK;
	if (ios->clock > SD_SDR50_MAX) {
		if (ios->timing == bus_timing_mmc_hs400 ||
		    ios->timing == bus_timing_mmc_hs400es)
			hostctrl2 |= XENON_CTRL2_MMC_HS400;
		else if (ios->timing == bus_timing_mmc_hs200)
			hostctrl2 |= XENON_CTRL2_MMC_HS200;
		else
			hostctrl2 |= SDHCI_CTRL2_UHS_SDR104;
	}
	else if (ios->clock > SD_SDR25_MAX)
		hostctrl2 |= SDHCI_CTRL2_UHS_SDR50;
	else if (ios->clock > SD_SDR12_MAX) {
		if (ios->timing == bus_timing_uhs_ddr50 ||
		    ios->timing == bus_timing_mmc_ddr52)
			hostctrl2 |= SDHCI_CTRL2_UHS_DDR50;
		else
			hostctrl2 |= SDHCI_CTRL2_UHS_SDR25;
	} else if (ios->clock > SD_MMC_CARD_ID_FREQUENCY)
		hostctrl2 |= SDHCI_CTRL2_UHS_SDR12;
	sdhci_xenon_write_2(brdev, slot, SDHCI_HOST_CONTROL2, hostctrl2);
}

static int
sdhci_xenon_phy_init(device_t brdev, struct mmc_ios *ios)
{
	int i;
	struct sdhci_xenon_softc *sc;
	uint32_t reg;

 	sc = device_get_softc(brdev);
	reg = bus_read_4(sc->mem_res, XENON_EMMC_PHY_TIMING_ADJUST);
	reg |= XENON_SAMPL_INV_QSP_PHASE_SELECT;
	switch (ios->timing) {
	case bus_timing_normal:
	case bus_timing_hs:
	case bus_timing_uhs_sdr12:
	case bus_timing_uhs_sdr25:
	case bus_timing_uhs_sdr50:
		reg |= XENON_TIMING_ADJUST_SLOW_MODE;
		break;
	default:
		reg &= ~XENON_TIMING_ADJUST_SLOW_MODE;
	}
	if (sc->slow_mode)
		reg |= XENON_TIMING_ADJUST_SLOW_MODE;
	bus_write_4(sc->mem_res, XENON_EMMC_PHY_TIMING_ADJUST, reg);

	reg = bus_read_4(sc->mem_res, XENON_EMMC_PHY_TIMING_ADJUST);
	reg |= XENON_PHY_INITIALIZATION;
	bus_write_4(sc->mem_res, XENON_EMMC_PHY_TIMING_ADJUST, reg);

	/* Wait for the eMMC PHY init. */
	for (i = 100; i > 0; i--) {
		DELAY(100);

		reg = bus_read_4(sc->mem_res, XENON_EMMC_PHY_TIMING_ADJUST);
		if ((reg & XENON_PHY_INITIALIZATION) == 0)
			break;
	}

	if (i == 0) {
		device_printf(brdev, "eMMC PHY failed to initialize\n");
		return (ETIMEDOUT);
	}

	return (0);
}

static int
sdhci_xenon_phy_set(device_t brdev, struct mmc_ios *ios)
{
	struct sdhci_xenon_softc *sc;
	uint32_t reg;

 	sc = device_get_softc(brdev);
	/* Setup pad, set bit[28] and bits[26:24] */
	reg = bus_read_4(sc->mem_res, XENON_EMMC_PHY_PAD_CONTROL);
	reg |= (XENON_FC_DQ_RECEN | XENON_FC_CMD_RECEN |
		XENON_FC_QSP_RECEN | XENON_OEN_QSN);
	/* All FC_XX_RECEIVCE should be set as CMOS Type */
	reg |= XENON_FC_ALL_CMOS_RECEIVER;
	bus_write_4(sc->mem_res, XENON_EMMC_PHY_PAD_CONTROL, reg);

	/* Set CMD and DQ Pull Up */
	reg = bus_read_4(sc->mem_res, XENON_EMMC_PHY_PAD_CONTROL1);
	reg |= (XENON_EMMC_FC_CMD_PU | XENON_EMMC_FC_DQ_PU);
	reg &= ~(XENON_EMMC_FC_CMD_PD | XENON_EMMC_FC_DQ_PD);
	bus_write_4(sc->mem_res, XENON_EMMC_PHY_PAD_CONTROL1, reg);

	if (ios->timing == bus_timing_normal)
		return (sdhci_xenon_phy_init(brdev, ios));

	/* Clear SDIO mode, no SDIO support for now. */
	reg = bus_read_4(sc->mem_res, XENON_EMMC_PHY_TIMING_ADJUST);
	reg &= ~XENON_TIMING_ADJUST_SDIO_MODE;
	bus_write_4(sc->mem_res, XENON_EMMC_PHY_TIMING_ADJUST, reg);

	/*
	 * Set preferred ZNR and ZPR value.
	 * The ZNR and ZPR value vary between different boards.
	 * Define them both in the DTS for the board!
	 */
	reg = bus_read_4(sc->mem_res, XENON_EMMC_PHY_PAD_CONTROL2);
	reg &= ~((XENON_ZNR_MASK << XENON_ZNR_SHIFT) | XENON_ZPR_MASK);
	reg |= ((sc->znr << XENON_ZNR_SHIFT) | sc->zpr);
	bus_write_4(sc->mem_res, XENON_EMMC_PHY_PAD_CONTROL2, reg);

	/* Disable the SD clock to set EMMC_PHY_FUNC_CONTROL. */
	reg = bus_read_4(sc->mem_res, SDHCI_CLOCK_CONTROL);
	reg &= ~SDHCI_CLOCK_CARD_EN;
	bus_write_4(sc->mem_res, SDHCI_CLOCK_CONTROL, reg);

	reg = bus_read_4(sc->mem_res, XENON_EMMC_PHY_FUNC_CONTROL);
	switch (ios->timing) {
	case bus_timing_mmc_hs400:
		reg |= (XENON_DQ_DDR_MODE_MASK << XENON_DQ_DDR_MODE_SHIFT) |
		    XENON_CMD_DDR_MODE;
		reg &= ~XENON_DQ_ASYNC_MODE;
		break;
	case bus_timing_uhs_ddr50:
	case bus_timing_mmc_ddr52:
		reg |= (XENON_DQ_DDR_MODE_MASK << XENON_DQ_DDR_MODE_SHIFT) |
		    XENON_CMD_DDR_MODE | XENON_DQ_ASYNC_MODE;
		break;
	default:
		reg &= ~((XENON_DQ_DDR_MODE_MASK << XENON_DQ_DDR_MODE_SHIFT) |
		    XENON_CMD_DDR_MODE);
		reg |= XENON_DQ_ASYNC_MODE;
	}
	bus_write_4(sc->mem_res, XENON_EMMC_PHY_FUNC_CONTROL, reg);

	/* Enable SD clock. */
	reg = bus_read_4(sc->mem_res, SDHCI_CLOCK_CONTROL);
	reg |= SDHCI_CLOCK_CARD_EN;
	bus_write_4(sc->mem_res, SDHCI_CLOCK_CONTROL, reg);

	if (ios->timing == bus_timing_mmc_hs400)
		bus_write_4(sc->mem_res, XENON_EMMC_PHY_LOGIC_TIMING_ADJUST,
		    XENON_LOGIC_TIMING_VALUE);
	else {
		/* Disable both SDHC Data Strobe and Enhanced Strobe. */
		reg = bus_read_4(sc->mem_res, XENON_SLOT_EMMC_CTRL);
		reg &= ~(XENON_ENABLE_DATA_STROBE | XENON_ENABLE_RESP_STROBE);
		bus_write_4(sc->mem_res, XENON_SLOT_EMMC_CTRL, reg);

		/* Clear Strobe line Pull down or Pull up. */
		reg = bus_read_4(sc->mem_res, XENON_EMMC_PHY_PAD_CONTROL1);
		reg &= ~(XENON_EMMC_FC_QSP_PD | XENON_EMMC_FC_QSP_PU);
		bus_write_4(sc->mem_res, XENON_EMMC_PHY_PAD_CONTROL1, reg);
	}

	return (sdhci_xenon_phy_init(brdev, ios));
}

static int
sdhci_xenon_update_ios(device_t brdev, device_t reqdev)
{
	int err;
	struct sdhci_xenon_softc *sc;
	struct mmc_ios *ios;
	struct sdhci_slot *slot;
	uint32_t reg;

	err = sdhci_generic_update_ios(brdev, reqdev);
	if (err != 0)
		return (err);

 	sc = device_get_softc(brdev);
	slot = device_get_ivars(reqdev);
 	ios = &slot->host.ios;

	switch (ios->power_mode) {
	case power_on:
		break;
	case power_off:
		if (bootverbose)
			device_printf(sc->dev, "Powering down sd/mmc\n");

		if (sc->vmmc_supply)
			regulator_disable(sc->vmmc_supply);
		if (sc->vqmmc_supply)
			regulator_disable(sc->vqmmc_supply);
		break;
	case power_up:
		if (bootverbose)
			device_printf(sc->dev, "Powering up sd/mmc\n");

		if (sc->vmmc_supply)
			regulator_enable(sc->vmmc_supply);
		if (sc->vqmmc_supply)
			regulator_enable(sc->vqmmc_supply);
		break;
	};

	/* Update the PHY settings. */
	if (ios->clock != 0)
		sdhci_xenon_phy_set(brdev, ios);

	if (ios->clock > SD_MMC_CARD_ID_FREQUENCY) {
		/* Enable SDCLK_IDLEOFF. */
		reg = bus_read_4(sc->mem_res, XENON_SYS_OP_CTRL);
		reg |= 1 << (XENON_SDCLK_IDLEOFF_ENABLE_SHIFT + sc->slot_id);
		bus_write_4(sc->mem_res, XENON_SYS_OP_CTRL, reg);
	}

	return (0);
}

static int
sdhci_xenon_switch_vccq(device_t brdev, device_t reqdev)
{
	struct sdhci_xenon_softc *sc;
	struct sdhci_slot *slot;
	uint16_t hostctrl2;
	int uvolt, err;

	slot = device_get_ivars(reqdev);

	if (slot->version < SDHCI_SPEC_300)
		return (0);

	sc = device_get_softc(brdev);

	if (sc->vqmmc_supply == NULL && !sc->skip_regulators)
		return (EOPNOTSUPP);

	err = 0;

	hostctrl2 = bus_read_2(sc->mem_res, SDHCI_HOST_CONTROL2);
	switch (slot->host.ios.vccq) {
	case vccq_330:
		if (!(hostctrl2 & SDHCI_CTRL2_S18_ENABLE))
			return (0);
		hostctrl2 &= ~SDHCI_CTRL2_S18_ENABLE;
		bus_write_2(sc->mem_res, SDHCI_HOST_CONTROL2, hostctrl2);

		if (!sc->skip_regulators) {
			uvolt = 3300000;
			err = regulator_set_voltage(sc->vqmmc_supply,
			    uvolt, uvolt);
			if (err != 0) {
				device_printf(sc->dev,
				    "Cannot set vqmmc to %d<->%d\n",
				    uvolt,
				    uvolt);
				return (err);
			}
		}

		/*
		 * According to the 'SD Host Controller Simplified
		 * Specification 4.20 the host driver should take more
		 * than 5ms for stable time of host voltage regulator
		 * from changing 1.8V Signaling Enable.
		 */
		DELAY(5000);
		hostctrl2 = bus_read_2(sc->mem_res, SDHCI_HOST_CONTROL2);
		if (!(hostctrl2 & SDHCI_CTRL2_S18_ENABLE))
			return (0);
		return (EAGAIN);
	case vccq_180:
		if (!(slot->host.caps & MMC_CAP_SIGNALING_180)) {
			return (EINVAL);
		}
		if (hostctrl2 & SDHCI_CTRL2_S18_ENABLE)
			return (0);
		hostctrl2 |= SDHCI_CTRL2_S18_ENABLE;
		bus_write_2(sc->mem_res, SDHCI_HOST_CONTROL2, hostctrl2);

		if (!sc->skip_regulators) {
			uvolt = 1800000;
			err = regulator_set_voltage(sc->vqmmc_supply,
				uvolt, uvolt);
			if (err != 0) {
				device_printf(sc->dev,
					"Cannot set vqmmc to %d<->%d\n",
					uvolt,
					uvolt);
				return (err);
			}
		}

		/*
		 * According to the 'SD Host Controller Simplified
		 * Specification 4.20 the host driver should take more
		 * than 5ms for stable time of host voltage regulator
		 * from changing 1.8V Signaling Enable.
		 */
		DELAY(5000);
		hostctrl2 = bus_read_2(sc->mem_res, SDHCI_HOST_CONTROL2);
		if (hostctrl2 & SDHCI_CTRL2_S18_ENABLE)
			return (0);
		return (EAGAIN);
	default:
		device_printf(brdev,
		    "Attempt to set unsupported signaling voltage\n");
		return (EINVAL);
	}
}

static void
sdhci_xenon_parse_prop(device_t dev)
{
	struct sdhci_xenon_softc *sc;
	uint32_t val;

	sc = device_get_softc(dev);
	val = 0;

	if (device_get_property(dev, "quirks",
	    &val, sizeof(val), DEVICE_PROP_UINT32) > 0)
		sc->slot->quirks = val;
	sc->znr = XENON_ZNR_DEF_VALUE;
	if (device_get_property(dev, "marvell,xenon-phy-znr",
	    &val, sizeof(val), DEVICE_PROP_UINT32) > 0)
		sc->znr = val & XENON_ZNR_MASK;
	sc->zpr = XENON_ZPR_DEF_VALUE;
	if (device_get_property(dev, "marvell,xenon-phy-zpr",
	    &val, sizeof(val), DEVICE_PROP_UINT32) > 0)
		sc->zpr = val & XENON_ZPR_MASK;
	if (device_has_property(dev, "marvell,xenon-phy-slow-mode"))
		sc->slow_mode = true;
}

int
sdhci_xenon_attach(device_t dev)
{
	struct sdhci_xenon_softc *sc = device_get_softc(dev);
	int err, rid;
	uint32_t reg;

	sc->dev = dev;
	sc->slot_id = 0;

	/* Allocate IRQ. */
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "Can't allocate IRQ\n");
		return (ENOMEM);
	}

	/* Allocate memory. */
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);
	if (sc->mem_res == NULL) {
		bus_release_resource(dev, SYS_RES_IRQ,
		    rman_get_rid(sc->irq_res), sc->irq_res);
		device_printf(dev, "Can't allocate memory for slot\n");
		return (ENOMEM);
	}

	sdhci_xenon_parse_prop(dev);

	sc->slot->max_clk = XENON_MMC_MAX_CLK;
	if (sc->slot->host.f_max > 0)
		sc->slot->max_clk = sc->slot->host.f_max;

	if (sdhci_init_slot(dev, sc->slot, 0))
		goto fail;

	/* 1.2V signaling is not supported. */
	sc->slot->host.caps &= ~MMC_CAP_SIGNALING_120;

	/* Disable UHS in case of the PHY slow mode. */
	if (sc->slow_mode)
		sc->slot->host.caps &= ~MMC_CAP_SIGNALING_180;

	/* Activate the interrupt */
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, sdhci_xenon_intr, sc, &sc->intrhand);
	if (err) {
		device_printf(dev, "Cannot setup IRQ\n");
		goto fail;
	}

	/* Disable Auto Clock Gating. */
	reg = bus_read_4(sc->mem_res, XENON_SYS_OP_CTRL);
	reg |= XENON_AUTO_CLKGATE_DISABLE;
	bus_write_4(sc->mem_res, XENON_SYS_OP_CTRL, reg);

	/* Enable this SD controller. */
	reg |= (1 << sc->slot_id);
	bus_write_4(sc->mem_res, XENON_SYS_OP_CTRL, reg);

	/* Enable Parallel Transfer. */
	reg = bus_read_4(sc->mem_res, XENON_SYS_EXT_OP_CTRL);
	reg |= (1 << sc->slot_id);
	bus_write_4(sc->mem_res, XENON_SYS_EXT_OP_CTRL, reg);

	/* Enable Auto Clock Gating. */
	reg &= ~XENON_AUTO_CLKGATE_DISABLE;
	bus_write_4(sc->mem_res, XENON_SYS_OP_CTRL, reg);

	/* Disable SDCLK_IDLEOFF before the card initialization. */
	reg = bus_read_4(sc->mem_res, XENON_SYS_OP_CTRL);
	reg &= ~(1 << (XENON_SDCLK_IDLEOFF_ENABLE_SHIFT + sc->slot_id));
	bus_write_4(sc->mem_res, XENON_SYS_OP_CTRL, reg);

	/* Mask command conflict errors. */
	reg = bus_read_4(sc->mem_res, XENON_SYS_EXT_OP_CTRL);
	reg |= XENON_MASK_CMD_CONFLICT_ERR;
	bus_write_4(sc->mem_res, XENON_SYS_EXT_OP_CTRL, reg);

	/* Process cards detection. */
	sdhci_start_slot(sc->slot);

	return (0);

fail:
	bus_release_resource(dev, SYS_RES_IRQ, rman_get_rid(sc->irq_res),
	    sc->irq_res);
	bus_release_resource(dev, SYS_RES_MEMORY, rman_get_rid(sc->mem_res),
	    sc->mem_res);
	free(sc->slot, M_DEVBUF);
	sc->slot = NULL;

	return (ENXIO);
}

int
sdhci_xenon_detach(device_t dev)
{
	struct sdhci_xenon_softc *sc = device_get_softc(dev);

	bus_generic_detach(dev);
	bus_teardown_intr(dev, sc->irq_res, sc->intrhand);
	bus_release_resource(dev, SYS_RES_IRQ, rman_get_rid(sc->irq_res),
	    sc->irq_res);
	sdhci_cleanup_slot(sc->slot);
	bus_release_resource(dev, SYS_RES_MEMORY, rman_get_rid(sc->mem_res),
	    sc->mem_res);
	free(sc->slot, M_DEVBUF);
	sc->slot = NULL;

	return (0);
}

static device_method_t sdhci_xenon_methods[] = {
	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	sdhci_generic_read_ivar),
	DEVMETHOD(bus_write_ivar,	sdhci_generic_write_ivar),

	/* mmcbr_if */
	DEVMETHOD(mmcbr_update_ios,	sdhci_xenon_update_ios),
	DEVMETHOD(mmcbr_request,	sdhci_generic_request),
	DEVMETHOD(mmcbr_get_ro,		sdhci_xenon_get_ro),
	DEVMETHOD(mmcbr_acquire_host,	sdhci_generic_acquire_host),
	DEVMETHOD(mmcbr_release_host,	sdhci_generic_release_host),
	DEVMETHOD(mmcbr_switch_vccq,	sdhci_xenon_switch_vccq),
	DEVMETHOD(mmcbr_tune,		sdhci_generic_tune),
	DEVMETHOD(mmcbr_retune,		sdhci_generic_retune),

	/* SDHCI registers accessors */
	DEVMETHOD(sdhci_read_1,		sdhci_xenon_read_1),
	DEVMETHOD(sdhci_read_2,		sdhci_xenon_read_2),
	DEVMETHOD(sdhci_read_4,		sdhci_xenon_read_4),
	DEVMETHOD(sdhci_read_multi_4,	sdhci_xenon_read_multi_4),
	DEVMETHOD(sdhci_write_1,	sdhci_xenon_write_1),
	DEVMETHOD(sdhci_write_2,	sdhci_xenon_write_2),
	DEVMETHOD(sdhci_write_4,	sdhci_xenon_write_4),
	DEVMETHOD(sdhci_write_multi_4,	sdhci_xenon_write_multi_4),
	DEVMETHOD(sdhci_set_uhs_timing, sdhci_xenon_set_uhs_timing),

	DEVMETHOD_END
};

DEFINE_CLASS_0(sdhci_xenon, sdhci_xenon_driver, sdhci_xenon_methods,
    sizeof(struct sdhci_xenon_softc));

SDHCI_DEPEND(sdhci_xenon);
#ifndef MMCCAM
MMC_DECLARE_BRIDGE(sdhci_xenon);
#endif
