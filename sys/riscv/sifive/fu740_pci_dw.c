/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2021 Jessica Clarke <jrtc27@FreeBSD.org>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* SiFive FU740 DesignWare PCIe driver */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/resource.h>

#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>
#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/ofwpci.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>
#include <dev/pci/pci_dw.h>

#include "pcib_if.h"
#include "pci_dw_if.h"

#define	FUDW_PHYS		2
#define	FUDW_LANES_PER_PHY	4

#define	FUDW_MGMT_PERST_N			0x0
#define	FUDW_MGMT_LTSSM_EN			0x10
#define	FUDW_MGMT_HOLD_PHY_RST			0x18
#define	FUDW_MGMT_DEVICE_TYPE			0x708
#define	 FUDW_MGMT_DEVICE_TYPE_RC			0x4
#define	FUDW_MGMT_PHY_CR_PARA_REG(_n, _r)	\
    (0x860 + (_n) * 0x40 + FUDW_MGMT_PHY_CR_PARA_##_r)
#define	 FUDW_MGMT_PHY_CR_PARA_ADDR			0x0
#define	 FUDW_MGMT_PHY_CR_PARA_READ_EN			0x10
#define	 FUDW_MGMT_PHY_CR_PARA_READ_DATA		0x18
#define	 FUDW_MGMT_PHY_CR_PARA_SEL			0x20
#define	 FUDW_MGMT_PHY_CR_PARA_WRITE_DATA		0x28
#define	 FUDW_MGMT_PHY_CR_PARA_WRITE_EN			0x30
#define	 FUDW_MGMT_PHY_CR_PARA_ACK			0x38

#define	FUDW_MGMT_PHY_LANE(_n)			(0x1008 + (_n) * 0x100)
#define	 FUDW_MGMT_PHY_LANE_CDR_TRACK_EN		(1 << 0)
#define	 FUDW_MGMT_PHY_LANE_LOS_THRESH			(1 << 5)
#define	 FUDW_MGMT_PHY_LANE_TERM_EN			(1 << 9)
#define	 FUDW_MGMT_PHY_LANE_TERM_ACDC			(1 << 10)
#define	 FUDW_MGMT_PHY_LANE_EN				(1 << 11)
#define	 FUDW_MGMT_PHY_LANE_INIT					\
    (FUDW_MGMT_PHY_LANE_CDR_TRACK_EN | FUDW_MGMT_PHY_LANE_LOS_THRESH |	\
     FUDW_MGMT_PHY_LANE_TERM_EN | FUDW_MGMT_PHY_LANE_TERM_ACDC |	\
     FUDW_MGMT_PHY_LANE_EN)

#define	FUDW_DBI_PORT_DBG1			0x72c
#define	 FUDW_DBI_PORT_DBG1_LINK_UP			(1 << 4)
#define	 FUDW_DBI_PORT_DBG1_LINK_IN_TRAINING		(1 << 29)

struct fupci_softc {
	struct pci_dw_softc	dw_sc;
	device_t		dev;
	struct resource		*mgmt_res;
	gpio_pin_t		porst_pin;
	gpio_pin_t		pwren_pin;
	clk_t			pcie_aux_clk;
	hwreset_t		pcie_aux_rst;
};

#define	FUDW_MGMT_READ(_sc, _o)		bus_read_4((_sc)->mgmt_res, (_o))
#define	FUDW_MGMT_WRITE(_sc, _o, _v)	bus_write_4((_sc)->mgmt_res, (_o), (_v))

static struct ofw_compat_data compat_data[] = {
	{ "sifive,fu740-pcie",	1 },
	{ NULL,			0 },
};

/* Currently unused; included for completeness */
static int __unused
fupci_phy_read(struct fupci_softc *sc, int phy, uint32_t reg, uint32_t *val)
{
	unsigned timeout;
	uint32_t ack;

	FUDW_MGMT_WRITE(sc, FUDW_MGMT_PHY_CR_PARA_REG(phy, ADDR), reg);
	FUDW_MGMT_WRITE(sc, FUDW_MGMT_PHY_CR_PARA_REG(phy, READ_EN), 1);

	timeout = 10;
	do {
		ack = FUDW_MGMT_READ(sc, FUDW_MGMT_PHY_CR_PARA_REG(phy, ACK));
		if (ack != 0)
			break;
		DELAY(10);
	} while (--timeout > 0);

	if (timeout == 0) {
		device_printf(sc->dev, "Timeout waiting for read ACK\n");
		return (ETIMEDOUT);
	}

	*val = FUDW_MGMT_READ(sc, FUDW_MGMT_PHY_CR_PARA_REG(phy, READ_DATA));
	FUDW_MGMT_WRITE(sc, FUDW_MGMT_PHY_CR_PARA_REG(phy, READ_EN), 0);

	timeout = 10;
	do {
		ack = FUDW_MGMT_READ(sc, FUDW_MGMT_PHY_CR_PARA_REG(phy, ACK));
		if (ack == 0)
			break;
		DELAY(10);
	} while (--timeout > 0);

	if (timeout == 0) {
		device_printf(sc->dev, "Timeout waiting for read un-ACK\n");
		return (ETIMEDOUT);
	}

	return (0);
}

static int
fupci_phy_write(struct fupci_softc *sc, int phy, uint32_t reg, uint32_t val)
{
	unsigned timeout;
	uint32_t ack;

	FUDW_MGMT_WRITE(sc, FUDW_MGMT_PHY_CR_PARA_REG(phy, ADDR), reg);
	FUDW_MGMT_WRITE(sc, FUDW_MGMT_PHY_CR_PARA_REG(phy, WRITE_DATA), val);
	FUDW_MGMT_WRITE(sc, FUDW_MGMT_PHY_CR_PARA_REG(phy, WRITE_EN), 1);

	timeout = 10;
	do {
		ack = FUDW_MGMT_READ(sc, FUDW_MGMT_PHY_CR_PARA_REG(phy, ACK));
		if (ack != 0)
			break;
		DELAY(10);
	} while (--timeout > 0);

	if (timeout == 0) {
		device_printf(sc->dev, "Timeout waiting for write ACK\n");
		return (ETIMEDOUT);
	}

	FUDW_MGMT_WRITE(sc, FUDW_MGMT_PHY_CR_PARA_REG(phy, WRITE_EN), 0);

	timeout = 10;
	do {
		ack = FUDW_MGMT_READ(sc, FUDW_MGMT_PHY_CR_PARA_REG(phy, ACK));
		if (ack == 0)
			break;
		DELAY(10);
	} while (--timeout > 0);

	if (timeout == 0) {
		device_printf(sc->dev, "Timeout waiting for write un-ACK\n");
		return (ETIMEDOUT);
	}

	return (0);
}

static int
fupci_phy_init(struct fupci_softc *sc)
{
	device_t dev;
	int error, phy, lane;

	dev = sc->dev;

	/* Assert core power-on reset (active low) */
	error = gpio_pin_set_active(sc->porst_pin, false);
	if (error != 0) {
		device_printf(dev, "Cannot assert power-on reset: %d\n",
		    error);
		return (error);
	}

	/* Assert PERST_N */
	FUDW_MGMT_WRITE(sc, FUDW_MGMT_PERST_N, 0);

	/* Enable power */
	error = gpio_pin_set_active(sc->pwren_pin, true);
	if (error != 0) {
		device_printf(dev, "Cannot enable power: %d\n", error);
		return (error);
	}

	/* Hold PERST for 100ms as per the PCIe spec */
	DELAY(100);

	/* Deassert PERST_N */
	FUDW_MGMT_WRITE(sc, FUDW_MGMT_PERST_N, 1);

	/* Deassert core power-on reset (active low) */
	error = gpio_pin_set_active(sc->porst_pin, true);
	if (error != 0) {
		device_printf(dev, "Cannot deassert power-on reset: %d\n",
		    error);
		return (error);
	}

	/* Enable the aux clock */
	error = clk_enable(sc->pcie_aux_clk);
	if (error != 0) {
		device_printf(dev, "Cannot enable aux clock: %d\n", error);
		return (error);
	}

	/* Hold LTSSM in reset whilst initialising the PHYs */
	FUDW_MGMT_WRITE(sc, FUDW_MGMT_HOLD_PHY_RST, 1);

	/* Deassert the aux reset */
	error = hwreset_deassert(sc->pcie_aux_rst);
	if (error != 0) {
		device_printf(dev, "Cannot deassert aux reset: %d\n", error);
		return (error);
	}

	/* Enable control register interface */
	for (phy = 0; phy < FUDW_PHYS; ++phy)
		FUDW_MGMT_WRITE(sc, FUDW_MGMT_PHY_CR_PARA_REG(phy, SEL), 1);

	/* Wait for enable to take effect */
	DELAY(1);

	/* Initialise lane configuration */
	for (phy = 0; phy < FUDW_PHYS; ++phy) {
		for (lane = 0; lane < FUDW_LANES_PER_PHY; ++lane)
			fupci_phy_write(sc, phy, FUDW_MGMT_PHY_LANE(lane),
			    FUDW_MGMT_PHY_LANE_INIT);
	}

	/* Disable the aux clock whilst taking the LTSSM out of reset */
	error = clk_disable(sc->pcie_aux_clk);
	if (error != 0) {
		device_printf(dev, "Cannot disable aux clock: %d\n", error);
		return (error);
	}

	/* Take LTSSM out of reset */
	FUDW_MGMT_WRITE(sc, FUDW_MGMT_HOLD_PHY_RST, 0);

	/* Enable the aux clock again */
	error = clk_enable(sc->pcie_aux_clk);
	if (error != 0) {
		device_printf(dev, "Cannot re-enable aux clock: %d\n", error);
		return (error);
	}

	/* Put the controller in Root Complex mode */
	FUDW_MGMT_WRITE(sc, FUDW_MGMT_DEVICE_TYPE, FUDW_MGMT_DEVICE_TYPE_RC);

	return (0);
}

static void
fupci_dbi_protect(struct fupci_softc *sc, bool protect)
{
	uint32_t reg;

	reg = pci_dw_dbi_rd4(sc->dev, DW_MISC_CONTROL_1);
	if (protect)
		reg &= ~DBI_RO_WR_EN;
	else
		reg |= DBI_RO_WR_EN;
	pci_dw_dbi_wr4(sc->dev, DW_MISC_CONTROL_1, reg);
}

static int
fupci_init(struct fupci_softc *sc)
{
	/* Enable 32-bit I/O window */
	fupci_dbi_protect(sc, false);
	pci_dw_dbi_wr2(sc->dev, PCIR_IOBASEL_1,
	    (PCIM_BRIO_32 << 8) | PCIM_BRIO_32);
	fupci_dbi_protect(sc, true);

	/* Enable LTSSM */
	FUDW_MGMT_WRITE(sc, FUDW_MGMT_LTSSM_EN, 1);

	return (0);
}

static int
fupci_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "SiFive FU740 PCIe Controller");

	return (BUS_PROBE_DEFAULT);
}

static int
fupci_attach(device_t dev)
{
	struct fupci_softc *sc;
	phandle_t node;
	int error, rid;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);
	sc->dev = dev;

	rid = 0;
	error = ofw_bus_find_string_index(node, "reg-names", "dbi", &rid);
	if (error != 0) {
		device_printf(dev, "Cannot get DBI memory: %d\n", error);
		goto fail;
	}
	sc->dw_sc.dbi_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->dw_sc.dbi_res == NULL) {
		device_printf(dev, "Cannot allocate DBI memory\n");
		error = ENXIO;
		goto fail;
	}

	rid = 0;
	error = ofw_bus_find_string_index(node, "reg-names", "mgmt", &rid);
	if (error != 0) {
		device_printf(dev, "Cannot get management space memory: %d\n",
		    error);
		goto fail;
	}
	sc->mgmt_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mgmt_res == NULL) {
		device_printf(dev, "Cannot allocate management space memory\n");
		error = ENXIO;
		goto fail;
	}

	error = gpio_pin_get_by_ofw_property(dev, node, "reset-gpios",
	    &sc->porst_pin);
	/* Old U-Boot device tree uses perstn-gpios */
	if (error == ENOENT)
		error = gpio_pin_get_by_ofw_property(dev, node, "perstn-gpios",
		    &sc->porst_pin);
	if (error != 0) {
		device_printf(dev, "Cannot get power-on reset GPIO: %d\n",
		    error);
		goto fail;
	}
	error = gpio_pin_setflags(sc->porst_pin, GPIO_PIN_OUTPUT);
	if (error != 0) {
		device_printf(dev, "Cannot configure power-on reset GPIO: %d\n",
		    error);
		goto fail;
	}

	error = gpio_pin_get_by_ofw_property(dev, node, "pwren-gpios",
	    &sc->pwren_pin);
	if (error != 0) {
		device_printf(dev, "Cannot get power enable GPIO: %d\n",
		    error);
		goto fail;
	}
	error = gpio_pin_setflags(sc->pwren_pin, GPIO_PIN_OUTPUT);
	if (error != 0) {
		device_printf(dev, "Cannot configure power enable GPIO: %d\n",
		    error);
		goto fail;
	}

	error = clk_get_by_ofw_name(dev, node, "pcie_aux", &sc->pcie_aux_clk);
	/* Old U-Boot device tree uses pcieaux */
	if (error == ENOENT)
		error = clk_get_by_ofw_name(dev, node, "pcieaux",
		    &sc->pcie_aux_clk);
	if (error != 0) {
		device_printf(dev, "Cannot get aux clock: %d\n", error);
		goto fail;
	}

	error = hwreset_get_by_ofw_idx(dev, node, 0, &sc->pcie_aux_rst);
	if (error != 0) {
		device_printf(dev, "Cannot get aux reset: %d\n", error);
		goto fail;
	}

	error = fupci_phy_init(sc);
	if (error != 0)
		goto fail;

	error = pci_dw_init(dev);
	if (error != 0)
		goto fail;

	error = fupci_init(sc);
	if (error != 0)
		goto fail;

	return (bus_generic_attach(dev));

fail:
	/* XXX Cleanup */
	return (error);
}

static int
fupci_get_link(device_t dev, bool *status)
{
	uint32_t reg;

	reg = pci_dw_dbi_rd4(dev, FUDW_DBI_PORT_DBG1);
	*status = (reg & FUDW_DBI_PORT_DBG1_LINK_UP) != 0 &&
	    (reg & FUDW_DBI_PORT_DBG1_LINK_IN_TRAINING) == 0;

	return (0);
}

static device_method_t fupci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			fupci_probe),
	DEVMETHOD(device_attach,		fupci_attach),

	/* PCI DW interface  */
	DEVMETHOD(pci_dw_get_link,		fupci_get_link),

	DEVMETHOD_END
};

DEFINE_CLASS_1(pcib, fupci_driver, fupci_methods,
    sizeof(struct fupci_softc), pci_dw_driver);
DRIVER_MODULE(fu740_pci_dw, simplebus, fupci_driver, NULL, NULL);
