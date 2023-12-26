/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Soren Schmidt <sos@deepcore.dk>
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
 *
 * $Id: eqos_fdt.c 1049 2022-12-03 14:25:46Z sos $
 */

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/hash.h>
#include <sys/gpio.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <machine/bus.h>

#include <net/if.h>
#include <net/if_media.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>
#include <dev/regulator/regulator.h>
#include <dev/syscon/syscon.h>

#include <dev/eqos/if_eqos_var.h>

#include "if_eqos_if.h"
#include "syscon_if.h"
#include "gpio_if.h"
#include "rk_otp_if.h"

#define	RK356XGMAC0	0xfe2a0000
#define	RK356XGMAC1	0xfe010000
#define	RK3588GMAC0	0xfe1b0000
#define	RK3588GMAC1	0xfe1c0000

#define	EQOS_GRF_GMAC0				0x0380
#define	EQOS_GRF_GMAC1				0x0388
#define	EQOS_CON0_OFFSET			0
#define	EQOS_CON1_OFFSET			4

#define	EQOS_GMAC_PHY_INTF_SEL_RGMII		0x00fc0010
#define	EQOS_GMAC_PHY_INTF_SEL_RMII		0x00fc0040
#define	EQOS_GMAC_RXCLK_DLY_ENABLE		0x00020002
#define	EQOS_GMAC_RXCLK_DLY_DISABLE		0x00020000
#define	EQOS_GMAC_TXCLK_DLY_ENABLE		0x00010001
#define	EQOS_GMAC_TXCLK_DLY_DISABLE		0x00010000
#define	EQOS_GMAC_CLK_RX_DL_CFG(val)		(0x7f000000 | val << 8)
#define	EQOS_GMAC_CLK_TX_DL_CFG(val)		(0x007f0000 | val)

#define	WR4(sc, o, v)		bus_write_4(sc->res[EQOS_RES_MEM], (o), (v))

static const struct ofw_compat_data compat_data[] = {
	{"snps,dwmac-4.20a",	1},
	{ NULL, 0 }
};


static int
eqos_phy_reset(device_t dev)
{
	pcell_t gpio_prop[4];
	pcell_t delay_prop[3];
	phandle_t node, gpio_node;
	device_t gpio;
	uint32_t pin, flags;
	uint32_t pin_value;

	node = ofw_bus_get_node(dev);
	if (OF_getencprop(node, "snps,reset-gpio",
	    gpio_prop, sizeof(gpio_prop)) <= 0)
		return (0);

	if (OF_getencprop(node, "snps,reset-delays-us",
	    delay_prop, sizeof(delay_prop)) <= 0) {
		device_printf(dev,
		    "Wrong property for snps,reset-delays-us");
		return (ENXIO);
	}

	gpio_node = OF_node_from_xref(gpio_prop[0]);
	if ((gpio = OF_device_from_xref(gpio_prop[0])) == NULL) {
		device_printf(dev,
		    "Can't find gpio controller for phy reset\n");
		return (ENXIO);
	}

	if (GPIO_MAP_GPIOS(gpio, node, gpio_node,
	    nitems(gpio_prop) - 1,
	    gpio_prop + 1, &pin, &flags) != 0) {
		device_printf(dev, "Can't map gpio for phy reset\n");
		return (ENXIO);
	}

	pin_value = GPIO_PIN_LOW;
	if (OF_hasprop(node, "snps,reset-active-low"))
		pin_value = GPIO_PIN_HIGH;

	GPIO_PIN_SETFLAGS(gpio, pin, GPIO_PIN_OUTPUT);
	GPIO_PIN_SET(gpio, pin, pin_value);
	DELAY(delay_prop[0]);
	GPIO_PIN_SET(gpio, pin, !pin_value);
	DELAY(delay_prop[1]);
	GPIO_PIN_SET(gpio, pin, pin_value);
	DELAY(delay_prop[2]);

	return (0);
}

static int
eqos_fdt_init(device_t dev)
{
	struct eqos_softc *sc = device_get_softc(dev);
	phandle_t node = ofw_bus_get_node(dev);
	hwreset_t eqos_reset;
	regulator_t eqos_supply;
	uint32_t rx_delay, tx_delay;
	uint8_t buffer[16];
	clk_t stmmaceth, mac_clk_rx, mac_clk_tx, aclk_mac, pclk_mac;
	uint64_t freq;
	int error;

	if (OF_hasprop(node, "rockchip,grf") &&
	    syscon_get_by_ofw_property(dev, node, "rockchip,grf", &sc->grf)) {
		device_printf(dev, "cannot get grf driver handle\n");
		return (ENXIO);
	}

	/* figure out if gmac0 or gmac1 offset */
	switch (rman_get_start(sc->res[EQOS_RES_MEM])) {
	case RK356XGMAC0:	/* RK356X gmac0 */
		sc->grf_offset = EQOS_GRF_GMAC0;
		break;
	case RK356XGMAC1:	/* RK356X gmac1 */
		sc->grf_offset = EQOS_GRF_GMAC1;
		break;
	case RK3588GMAC0:	/* RK3588 gmac0 */
	case RK3588GMAC1:	/* RK3588 gmac1 */
	default:
		device_printf(dev, "Unknown eqos address\n");
		return (ENXIO);
	}

	if (hwreset_get_by_ofw_idx(dev, node, 0, &eqos_reset)) {
		device_printf(dev, "cannot get reset\n");
		return (ENXIO);
	}
	hwreset_assert(eqos_reset);

	error = clk_set_assigned(dev, ofw_bus_get_node(dev));
	if (error != 0) {
		device_printf(dev, "clk_set_assigned failed\n");
		return (error);
	}

	if (clk_get_by_ofw_name(dev, 0, "stmmaceth", &stmmaceth) == 0) {
		error = clk_enable(stmmaceth);
		if (error != 0) {
			device_printf(dev, "could not enable main clock\n");
			return (error);
		}
		if (bootverbose) {
			clk_get_freq(stmmaceth, &freq);
			device_printf(dev, "MAC clock(%s) freq: %jd\n",
					clk_get_name(stmmaceth), (intmax_t)freq);
		}
	}
	else {
		device_printf(dev, "could not find clock stmmaceth\n");
	}

	if (clk_get_by_ofw_name(dev, 0, "mac_clk_rx", &mac_clk_rx) != 0) {
		device_printf(dev, "could not get mac_clk_rx clock\n");
		mac_clk_rx = NULL;
	}

	if (clk_get_by_ofw_name(dev, 0, "mac_clk_tx", &mac_clk_tx) != 0) {
		device_printf(dev, "could not get mac_clk_tx clock\n");
		mac_clk_tx = NULL;
	}

	if (clk_get_by_ofw_name(dev, 0, "aclk_mac", &aclk_mac) != 0) {
		device_printf(dev, "could not get aclk_mac clock\n");
		aclk_mac = NULL;
	}

	if (clk_get_by_ofw_name(dev, 0, "pclk_mac", &pclk_mac) != 0) {
		device_printf(dev, "could not get pclk_mac clock\n");
		pclk_mac = NULL;
	}

	if (aclk_mac)
		clk_enable(aclk_mac);
	if (pclk_mac)
		clk_enable(pclk_mac);
	if (mac_clk_tx)
		clk_enable(mac_clk_tx);

	sc->csr_clock = 125000000;
	sc->csr_clock_range = GMAC_MAC_MDIO_ADDRESS_CR_100_150;

	if (OF_getencprop(node, "tx_delay", &tx_delay, sizeof(tx_delay)) <= 0)
		tx_delay = 0x30;
	if (OF_getencprop(node, "rx_delay", &rx_delay, sizeof(rx_delay)) <= 0)
		rx_delay = 0x10;

	SYSCON_WRITE_4(sc->grf, sc->grf_offset + EQOS_CON0_OFFSET,
	    EQOS_GMAC_CLK_RX_DL_CFG(rx_delay) |
	    EQOS_GMAC_CLK_TX_DL_CFG(tx_delay));
	SYSCON_WRITE_4(sc->grf, sc->grf_offset + EQOS_CON1_OFFSET,
	    EQOS_GMAC_PHY_INTF_SEL_RGMII |
	    EQOS_GMAC_RXCLK_DLY_ENABLE |
	    EQOS_GMAC_TXCLK_DLY_ENABLE);

	if (!regulator_get_by_ofw_property(dev, 0, "phy-supply",
	    &eqos_supply)) {
		if (regulator_enable(eqos_supply))
			device_printf(dev, "cannot enable 'phy' regulator\n");
	}
	else
		device_printf(dev, "no phy-supply property\n");

	if (eqos_phy_reset(dev))
		return (ENXIO);

	if (eqos_reset)
		hwreset_deassert(eqos_reset);

	/* set the MAC address if we have OTP data handy */
	if (!RK_OTP_READ(dev, buffer, 0, sizeof(buffer))) {
		uint32_t mac;

		mac = hash32_buf(buffer, sizeof(buffer), HASHINIT);
		WR4(sc, GMAC_MAC_ADDRESS0_LOW,
		    htobe32((mac & 0xffffff00) | 0x22));

		mac = hash32_buf(buffer, sizeof(buffer), mac);
		WR4(sc, GMAC_MAC_ADDRESS0_HIGH,
		    htobe16((mac & 0x0000ffff) + (device_get_unit(dev) << 8)));
	}

	return (0);
}

static int
eqos_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
        if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "DesignWare EQOS Gigabit ethernet");

	return (BUS_PROBE_DEFAULT);
}


static device_method_t eqos_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		eqos_fdt_probe),

	/* EQOS interface */
	DEVMETHOD(if_eqos_init,		eqos_fdt_init),

	DEVMETHOD_END
};

DEFINE_CLASS_1(eqos, eqos_fdt_driver, eqos_fdt_methods,
    sizeof(struct eqos_softc), eqos_driver);
DRIVER_MODULE(eqos, simplebus, eqos_fdt_driver, 0, 0);
MODULE_DEPEND(eqos, ether, 1, 1, 1);
MODULE_DEPEND(eqos, miibus, 1, 1, 1);
