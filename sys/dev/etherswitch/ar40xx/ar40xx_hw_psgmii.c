/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Adrian Chadd <adrian@FreeBSD.org>.
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
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <machine/bus.h>
#include <dev/iicbus/iic.h>
#include <dev/iicbus/iiconf.h>
#include <dev/iicbus/iicbus.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mdio/mdio.h>
#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/etherswitch/etherswitch.h>

#include <dev/etherswitch/ar40xx/ar40xx_var.h>
#include <dev/etherswitch/ar40xx/ar40xx_reg.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw.h>
#include <dev/etherswitch/ar40xx/ar40xx_phy.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw_atu.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw_mdio.h>
#include <dev/etherswitch/ar40xx/ar40xx_hw_psgmii.h>

#include "mdio_if.h"
#include "miibus_if.h"
#include "etherswitch_if.h"

/*
 * Routines that control the ess-psgmii block - the interconnect
 * between the ess-switch and the external multi-port PHY
 * (eg Maple.)
 */

static void
ar40xx_hw_psgmii_reg_write(struct ar40xx_softc *sc, uint32_t reg,
    uint32_t val)
{
	bus_space_write_4(sc->sc_psgmii_mem_tag, sc->sc_psgmii_mem_handle,
	    reg, val);
	bus_space_barrier(sc->sc_psgmii_mem_tag, sc->sc_psgmii_mem_handle,
	    0, sc->sc_psgmii_mem_size, BUS_SPACE_BARRIER_WRITE);
}

static int
ar40xx_hw_psgmii_reg_read(struct ar40xx_softc *sc, uint32_t reg)
{
	int ret;

	bus_space_barrier(sc->sc_psgmii_mem_tag, sc->sc_psgmii_mem_handle,
	    0, sc->sc_psgmii_mem_size, BUS_SPACE_BARRIER_READ);
	ret = bus_space_read_4(sc->sc_psgmii_mem_tag, sc->sc_psgmii_mem_handle,
	    reg);

	return (ret);
}

int
ar40xx_hw_psgmii_set_mac_mode(struct ar40xx_softc *sc, uint32_t mac_mode)
{
	if (mac_mode == PORT_WRAPPER_PSGMII) {
		ar40xx_hw_psgmii_reg_write(sc, AR40XX_PSGMII_MODE_CONTROL,
		    0x2200);
		ar40xx_hw_psgmii_reg_write(sc, AR40XX_PSGMIIPHY_TX_CONTROL,
		    0x8380);
	} else {
		device_printf(sc->sc_dev, "WARNING: unknown MAC_MODE=%u\n",
		    mac_mode);
	}

	return (0);
}

int
ar40xx_hw_psgmii_single_phy_testing(struct ar40xx_softc *sc, int phy)
{
	int j;
	uint32_t tx_ok, tx_error;
	uint32_t rx_ok, rx_error;
	uint32_t tx_ok_high16;
	uint32_t rx_ok_high16;
	uint32_t tx_all_ok, rx_all_ok;

	MDIO_WRITEREG(sc->sc_mdio_dev, phy, 0x0, 0x9000);
	MDIO_WRITEREG(sc->sc_mdio_dev, phy, 0x0, 0x4140);

	for (j = 0; j < AR40XX_PSGMII_CALB_NUM; j++) {
		uint16_t status;

	status = MDIO_READREG(sc->sc_mdio_dev, phy, 0x11);
		if (status & AR40XX_PHY_SPEC_STATUS_LINK)
			break;
			/*
			 * the polling interval to check if the PHY link up
			 * or not
			 * maxwait_timer: 750 ms +/-10 ms
			 * minwait_timer : 1 us +/- 0.1us
			 * time resides in minwait_timer ~ maxwait_timer
			 * see IEEE 802.3 section 40.4.5.2
			 */
		DELAY(8 * 1000);
	}

	/* enable check */
	ar40xx_hw_phy_mmd_write(sc, phy, 7, 0x8029, 0x0000);
	ar40xx_hw_phy_mmd_write(sc, phy, 7, 0x8029, 0x0003);

	/* start traffic */
	ar40xx_hw_phy_mmd_write(sc, phy, 7, 0x8020, 0xa000);
	/*
	 *wait for all traffic end
	 * 4096(pkt num)*1524(size)*8ns(125MHz)=49.9ms
	 */
	DELAY(60 * 1000);

	/* check counter */
	tx_ok = ar40xx_hw_phy_mmd_read(sc, phy, 7, 0x802e);
	tx_ok_high16 = ar40xx_hw_phy_mmd_read(sc, phy, 7, 0x802d);
	tx_error = ar40xx_hw_phy_mmd_read(sc, phy, 7, 0x802f);
	rx_ok = ar40xx_hw_phy_mmd_read(sc, phy, 7, 0x802b);
	rx_ok_high16 = ar40xx_hw_phy_mmd_read(sc, phy, 7, 0x802a);
	rx_error = ar40xx_hw_phy_mmd_read(sc, phy, 7, 0x802c);
	tx_all_ok = tx_ok + (tx_ok_high16 << 16);
	rx_all_ok = rx_ok + (rx_ok_high16 << 16);

	if (tx_all_ok == 0x1000 && tx_error == 0) {
		/* success */
		sc->sc_psgmii.phy_t_status &= ~(1U << phy);
	} else {
		device_printf(sc->sc_dev, "TX_OK=%d, tx_error=%d RX_OK=%d"
		    " rx_error=%d\n",
		    tx_all_ok, tx_error, rx_all_ok, rx_error);
		device_printf(sc->sc_dev,
		    "PHY %d single test PSGMII issue happen!\n", phy);
		sc->sc_psgmii.phy_t_status |= BIT(phy);
	}

	MDIO_WRITEREG(sc->sc_mdio_dev, phy, 0x0, 0x1840);
	return (0);
}

int
ar40xx_hw_psgmii_all_phy_testing(struct ar40xx_softc *sc)
{
	int phy, j;

	MDIO_WRITEREG(sc->sc_mdio_dev, 0x1f, 0x0, 0x9000);
	MDIO_WRITEREG(sc->sc_mdio_dev, 0x1f, 0x0, 0x4140);

	for (j = 0; j < AR40XX_PSGMII_CALB_NUM; j++) {
		for (phy = 0; phy < AR40XX_NUM_PORTS - 1; phy++) {
			uint16_t status;

			status = MDIO_READREG(sc->sc_mdio_dev, phy, 0x11);
			if (!(status & (1U << 10)))
				break;
		}

		if (phy >= (AR40XX_NUM_PORTS - 1))
			break;
		/* The polling interval to check if the PHY link up or not */
		DELAY(8*1000);
	}

	/* enable check */
	ar40xx_hw_phy_mmd_write(sc, 0x1f, 7, 0x8029, 0x0000);
	ar40xx_hw_phy_mmd_write(sc, 0x1f, 7, 0x8029, 0x0003);

	/* start traffic */
	ar40xx_hw_phy_mmd_write(sc, 0x1f, 7, 0x8020, 0xa000);
	/*
	 * wait for all traffic end
	 * 4096(pkt num)*1524(size)*8ns(125MHz)=49.9ms
	 */
	DELAY(60*1000); /* was 50ms */

	for (phy = 0; phy < AR40XX_NUM_PORTS - 1; phy++) {
		uint32_t tx_ok, tx_error;
		uint32_t rx_ok, rx_error;
		uint32_t tx_ok_high16;
		uint32_t rx_ok_high16;
		uint32_t tx_all_ok, rx_all_ok;

		/* check counter */
		tx_ok = ar40xx_hw_phy_mmd_read(sc, phy, 7, 0x802e);
		tx_ok_high16 = ar40xx_hw_phy_mmd_read(sc, phy, 7, 0x802d);
		tx_error = ar40xx_hw_phy_mmd_read(sc, phy, 7, 0x802f);
		rx_ok = ar40xx_hw_phy_mmd_read(sc, phy, 7, 0x802b);
		rx_ok_high16 = ar40xx_hw_phy_mmd_read(sc, phy, 7, 0x802a);
		rx_error = ar40xx_hw_phy_mmd_read(sc, phy, 7, 0x802c);

		tx_all_ok = tx_ok + (tx_ok_high16<<16);
		rx_all_ok = rx_ok + (rx_ok_high16<<16);
		if (tx_all_ok == 0x1000 && tx_error == 0) {
			/* success */
			sc->sc_psgmii.phy_t_status &= ~(1U << (phy + 8));
		} else {
			device_printf(sc->sc_dev,
			    "PHY%d test see issue! (tx_all_ok=%u,"
			    " rx_all_ok=%u, tx_error=%u, rx_error=%u)\n",
			    phy, tx_all_ok, rx_all_ok, tx_error, rx_error);
			sc->sc_psgmii.phy_t_status |= (1U << (phy + 8));
		}
	}

	device_printf(sc->sc_dev, "PHY all test 0x%x\n",
	    sc->sc_psgmii.phy_t_status);
	return (0);
}

/*
 * Reset PSGMII in the Malibu PHY.
 */
int
ar40xx_hw_malibu_psgmii_ess_reset(struct ar40xx_softc *sc)
{
	device_printf(sc->sc_dev, "%s: called\n", __func__);
	uint32_t i;

	/* reset phy psgmii */
	/* fix phy psgmii RX 20bit */
	MDIO_WRITEREG(sc->sc_mdio_dev, 5, 0x0, 0x005b);
	/* reset phy psgmii */
	MDIO_WRITEREG(sc->sc_mdio_dev, 5, 0x0, 0x001b);
	/* release reset phy psgmii */
	MDIO_WRITEREG(sc->sc_mdio_dev, 5, 0x0, 0x005b);

	for (i = 0; i < AR40XX_PSGMII_CALB_NUM; i++) {
		uint32_t status;

		status = ar40xx_hw_phy_mmd_read(sc, 5, 1, 0x28);
		if (status & (1U << 0))
			break;
		/*
		 * Polling interval to check PSGMII PLL in malibu is ready
		 * the worst time is 8.67ms
		 * for 25MHz reference clock
		 * [512+(128+2048)*49]*80ns+100us
		 */
		DELAY(2000);
	}
	/* XXX TODO ;see if it timed out? */

	/*check malibu psgmii calibration done end..*/

	/*freeze phy psgmii RX CDR*/
	MDIO_WRITEREG(sc->sc_mdio_dev, 5, 0x1a, 0x2230);

	ar40xx_hw_ess_reset(sc);

	/*check psgmii calibration done start*/
	for (i = 0; i < AR40XX_PSGMII_CALB_NUM; i++) {
		uint32_t status;

		status = ar40xx_hw_psgmii_reg_read(sc, 0xa0);
		if (status & (1U << 0))
			break;
		/* Polling interval to check PSGMII PLL in ESS is ready */
		DELAY(2000);
	}
	/* XXX TODO ;see if it timed out? */

	/* check dakota psgmii calibration done end..*/

	/* release phy psgmii RX CDR */
	MDIO_WRITEREG(sc->sc_mdio_dev, 5, 0x1a, 0x3230);
	/* release phy psgmii RX 20bit */
	MDIO_WRITEREG(sc->sc_mdio_dev, 5, 0x0, 0x005f);

	return (0);
}

int
ar40xx_hw_psgmii_self_test(struct ar40xx_softc *sc)
{
	uint32_t i, phy, reg;

	device_printf(sc->sc_dev, "%s: called\n", __func__);

	ar40xx_hw_malibu_psgmii_ess_reset(sc);

	/* switch to access MII reg for copper */
	MDIO_WRITEREG(sc->sc_mdio_dev, 4, 0x1f, 0x8500);
	for (phy = 0; phy < AR40XX_NUM_PORTS - 1; phy++) {
		/*enable phy mdio broadcast write*/
		ar40xx_hw_phy_mmd_write(sc, phy, 7, 0x8028, 0x801f);
	}

	/* force no link by power down */
	MDIO_WRITEREG(sc->sc_mdio_dev, 0x1f, 0x0, 0x1840);

	/* packet number*/
	ar40xx_hw_phy_mmd_write(sc, 0x1f, 7, 0x8021, 0x1000);
	ar40xx_hw_phy_mmd_write(sc, 0x1f, 7, 0x8062, 0x05e0);

	/* fix mdi status */
	MDIO_WRITEREG(sc->sc_mdio_dev, 0x1f, 0x10, 0x6800);
	for (i = 0; i < AR40XX_PSGMII_CALB_NUM; i++) {
		sc->sc_psgmii.phy_t_status = 0;

		for (phy = 0; phy < AR40XX_NUM_PORTS - 1; phy++) {
			/* Enable port loopback for testing */
			AR40XX_REG_BARRIER_READ(sc);
			reg = AR40XX_REG_READ(sc,
			    AR40XX_REG_PORT_LOOKUP(phy + 1));
			reg |= AR40XX_PORT_LOOKUP_LOOPBACK;
			AR40XX_REG_WRITE(sc,
			    AR40XX_REG_PORT_LOOKUP(phy + 1), reg);
			AR40XX_REG_BARRIER_WRITE(sc);
		}

		for (phy = 0; phy < AR40XX_NUM_PORTS - 1; phy++)
			ar40xx_hw_psgmii_single_phy_testing(sc, phy);

		ar40xx_hw_psgmii_all_phy_testing(sc);

		if (sc->sc_psgmii.phy_t_status)
			ar40xx_hw_malibu_psgmii_ess_reset(sc);
		else
			break;
	}

	if (i >= AR40XX_PSGMII_CALB_NUM)
		device_printf(sc->sc_dev, "PSGMII cannot recover\n");
	else
		device_printf(sc->sc_dev,
		    "PSGMII recovered after %d times reset\n", i);

	/* configuration recover */
	/* packet number */
	ar40xx_hw_phy_mmd_write(sc, 0x1f, 7, 0x8021, 0x0);
	/* disable check */
	ar40xx_hw_phy_mmd_write(sc, 0x1f, 7, 0x8029, 0x0);
	/* disable traffic */
	ar40xx_hw_phy_mmd_write(sc, 0x1f, 7, 0x8020, 0x0);

	return (0);
}

int
ar40xx_hw_psgmii_self_test_clean(struct ar40xx_softc *sc)
{
	uint32_t reg;
	int phy;

	device_printf(sc->sc_dev, "%s: called\n", __func__);

	/* disable phy internal loopback */
	MDIO_WRITEREG(sc->sc_mdio_dev, 0x1f, 0x10, 0x6860);
	MDIO_WRITEREG(sc->sc_mdio_dev, 0x1f, 0x0, 0x9040);

        for (phy = 0; phy < AR40XX_NUM_PORTS - 1; phy++) {
		/* disable mac loop back */
		reg = AR40XX_REG_READ(sc, AR40XX_REG_PORT_LOOKUP(phy + 1));
		reg &= ~AR40XX_PORT_LOOKUP_LOOPBACK;
		AR40XX_REG_WRITE(sc, AR40XX_REG_PORT_LOOKUP(phy + 1), reg);
		AR40XX_REG_BARRIER_WRITE(sc);

		/* disable phy mdio broadcast write */
		ar40xx_hw_phy_mmd_write(sc, phy, 7, 0x8028, 0x001f);
	}

	/* clear fdb entry */
	ar40xx_hw_atu_flush_all(sc);

	return (0);
}

int
ar40xx_hw_psgmii_init_config(struct ar40xx_softc *sc)
{
	uint32_t reg;

	/*
	 * This is based on what I found in uboot - it configures
	 * the initial ESS interconnect to either be PSGMII
	 * or RGMII.
	 */

	/* For now, just assume PSGMII and fix it in post. */
	/* PSGMIIPHY_PLL_VCO_RELATED_CTRL */
	reg = ar40xx_hw_psgmii_reg_read(sc, 0x78c);
	device_printf(sc->sc_dev,
	    "%s: PSGMIIPHY_PLL_VCO_RELATED_CTRL=0x%08x\n", __func__, reg);
	/* PSGMIIPHY_VCO_CALIBRATION_CTRL */
	reg = ar40xx_hw_psgmii_reg_read(sc, 0x09c);
	device_printf(sc->sc_dev,
	    "%s: PSGMIIPHY_VCO_CALIBRATION_CTRL=0x%08x\n", __func__, reg);

	return (0);
}
