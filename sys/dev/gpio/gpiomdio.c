/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Rubicon Communications, LLC (Netgate)
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

#include <sys/cdefs.h>
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <dev/fdt/fdt_common.h>
#include <dev/gpio/gpiobusvar.h>
#include <dev/mii/mii_bitbang.h>
#include <dev/mii/miivar.h>
#include <dev/ofw/ofw_bus.h>

#include "gpiobus_if.h"
#include "miibus_if.h"

#define	GPIOMDIO_MDC_DFLT	0
#define	GPIOMDIO_MDIO_DFLT	1
#define	GPIOMDIO_MIN_PINS	2

#define	MDO_BIT			0x01
#define	MDI_BIT			0x02
#define	MDC_BIT			0x04
#define	MDIRPHY_BIT		0x08
#define	MDIRHOST_BIT		0x10
#define	MDO			sc->miibb_ops.mbo_bits[MII_BIT_MDO]
#define	MDI			sc->miibb_ops.mbo_bits[MII_BIT_MDI]
#define	MDC			sc->miibb_ops.mbo_bits[MII_BIT_MDC]
#define	MDIRPHY			sc->miibb_ops.mbo_bits[MII_BIT_DIR_HOST_PHY]
#define	MDIRHOST		sc->miibb_ops.mbo_bits[MII_BIT_DIR_PHY_HOST]

static uint32_t gpiomdio_bb_read(device_t);
static void gpiomdio_bb_write(device_t, uint32_t);

struct gpiomdio_softc
{
	device_t		sc_dev;
	device_t		sc_busdev;
	int			mdc_pin;
	int			mdio_pin;
	struct mii_bitbang_ops	miibb_ops;
};


static int
gpiomdio_probe(device_t dev)
{
	struct gpiobus_ivar *devi;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);
	if (!ofw_bus_is_compatible(dev, "freebsd,gpiomdio"))
		return (ENXIO);
	devi = GPIOBUS_IVAR(dev);
	if (devi->npins < GPIOMDIO_MIN_PINS) {
		device_printf(dev,
		    "gpiomdio needs at least %d GPIO pins (only %d given).\n",
		    GPIOMDIO_MIN_PINS, devi->npins);
		return (ENXIO);
	}
	device_set_desc(dev, "GPIO MDIO bit-banging Bus driver");

	return (BUS_PROBE_DEFAULT);
}

static int
gpiomdio_attach(device_t dev)
{
	phandle_t		node;
	pcell_t			pin;
	struct gpiobus_ivar	*devi;
	struct gpiomdio_softc	*sc;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_busdev = device_get_parent(dev);

	if ((node = ofw_bus_get_node(dev)) == -1)
		return (ENXIO);
	if (OF_getencprop(node, "mdc", &pin, sizeof(pin)) > 0)
		sc->mdc_pin = (int)pin;
	if (OF_getencprop(node, "mdio", &pin, sizeof(pin)) > 0)
		sc->mdio_pin = (int)pin;

	if (sc->mdc_pin < 0 || sc->mdc_pin > 1)
		sc->mdc_pin = GPIOMDIO_MDC_DFLT;
	if (sc->mdio_pin < 0 || sc->mdio_pin > 1)
		sc->mdio_pin = GPIOMDIO_MDIO_DFLT;

	devi = GPIOBUS_IVAR(dev);
	device_printf(dev, "MDC pin: %d, MDIO pin: %d\n",
	    devi->pins[sc->mdc_pin], devi->pins[sc->mdio_pin]);

	/* Initialize mii_bitbang_ops. */
	MDO = MDO_BIT;
	MDI = MDI_BIT;
	MDC = MDC_BIT;
	MDIRPHY = MDIRPHY_BIT;
	MDIRHOST = MDIRHOST_BIT;
	sc->miibb_ops.mbo_read = gpiomdio_bb_read;
	sc->miibb_ops.mbo_write = gpiomdio_bb_write;

	/* Register our MDIO Bus device. */
	OF_device_register_xref(OF_xref_from_node(node), dev);

	return (0);
}

static uint32_t
gpiomdio_bb_read(device_t dev)
{
	struct gpiomdio_softc *sc;
	unsigned int val;

	sc = device_get_softc(dev);
	GPIOBUS_PIN_GET(sc->sc_busdev, sc->sc_dev, sc->mdio_pin, &val);

	return (val != 0 ? MDI_BIT : 0);
}

static void
gpiomdio_bb_write(device_t dev, uint32_t val)
{
	struct gpiomdio_softc *sc;

	sc = device_get_softc(dev);

	/* Set the data pin state. */
	if ((val & (MDIRPHY_BIT | MDO_BIT)) == (MDIRPHY_BIT | MDO_BIT))
		GPIOBUS_PIN_SET(sc->sc_busdev, sc->sc_dev, sc->mdio_pin, 1);
	else if ((val & (MDIRPHY_BIT | MDO_BIT)) == MDIRPHY_BIT)
		GPIOBUS_PIN_SET(sc->sc_busdev, sc->sc_dev, sc->mdio_pin, 0);
	if (val & MDIRPHY_BIT)
		GPIOBUS_PIN_SETFLAGS(sc->sc_busdev, sc->sc_dev, sc->mdio_pin,
		    GPIO_PIN_OUTPUT);
	else if (val & MDIRHOST_BIT)
		GPIOBUS_PIN_SETFLAGS(sc->sc_busdev, sc->sc_dev, sc->mdio_pin,
		    GPIO_PIN_INPUT);

	/* And now the clock state. */
	if (val & MDC_BIT)
		GPIOBUS_PIN_SET(sc->sc_busdev, sc->sc_dev, sc->mdc_pin, 1);
	else
		GPIOBUS_PIN_SET(sc->sc_busdev, sc->sc_dev, sc->mdc_pin, 0);
	GPIOBUS_PIN_SETFLAGS(sc->sc_busdev, sc->sc_dev, sc->mdc_pin,
	    GPIO_PIN_OUTPUT);
}

static int
gpiomdio_readreg(device_t dev, int phy, int reg)
{
	struct gpiomdio_softc	*sc;

	sc = device_get_softc(dev);

	return (mii_bitbang_readreg(dev, &sc->miibb_ops, phy, reg));
}

static int
gpiomdio_writereg(device_t dev, int phy, int reg, int val)
{
	struct gpiomdio_softc	*sc;

	sc = device_get_softc(dev);
	mii_bitbang_writereg(dev, &sc->miibb_ops, phy, reg, val);

	return (0);
}

static phandle_t
gpiomdio_get_node(device_t bus, device_t dev)
{

	return (ofw_bus_get_node(bus));
}

static device_method_t gpiomdio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gpiomdio_probe),
	DEVMETHOD(device_attach,	gpiomdio_attach),

	/* MDIO interface */
	DEVMETHOD(miibus_readreg,	gpiomdio_readreg),
	DEVMETHOD(miibus_writereg,	gpiomdio_writereg),

	/* OFW bus interface */
	DEVMETHOD(ofw_bus_get_node,	gpiomdio_get_node),

	DEVMETHOD_END
};

static driver_t gpiomdio_driver = {
	"gpiomdio",
	gpiomdio_methods,
	sizeof(struct gpiomdio_softc),
};

EARLY_DRIVER_MODULE(gpiomdio, gpiobus, gpiomdio_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
DRIVER_MODULE(miibus, gpiomdio, miibus_driver, 0, 0);
MODULE_DEPEND(gpiomdio, gpiobus, 1, 1, 1);
MODULE_DEPEND(gpiomdio, miibus, 1, 1, 1);
MODULE_DEPEND(gpiomdio, mii_bitbang, 1, 1, 1);
