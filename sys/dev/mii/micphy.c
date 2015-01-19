/*-
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
__FBSDID("$FreeBSD$");

/*
 * Micrel KSZ9021 Gigabit Ethernet Transceiver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_media.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include "miidevs.h"

#include "miibus_if.h"

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#define	MII_KSZPHY_EXTREG			0x0b
#define	 KSZPHY_EXTREG_WRITE			(1 << 15)
#define	MII_KSZPHY_EXTREG_WRITE			0x0c
#define	MII_KSZPHY_EXTREG_READ			0x0d
#define	MII_KSZPHY_CLK_CONTROL_PAD_SKEW		0x104
#define	MII_KSZPHY_RX_DATA_PAD_SKEW		0x105
#define	MII_KSZPHY_TX_DATA_PAD_SKEW		0x106

#define	PS_TO_REG(p)	((p) / 200)

static int micphy_probe(device_t);
static int micphy_attach(device_t);
static int micphy_service(struct mii_softc *, struct mii_data *, int);

static device_method_t micphy_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		micphy_probe),
	DEVMETHOD(device_attach,	micphy_attach),
	DEVMETHOD(device_detach,	mii_phy_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD_END
};

static devclass_t micphy_devclass;

static driver_t micphy_driver = {
	"micphy",
	micphy_methods,
	sizeof(struct mii_softc)
};

DRIVER_MODULE(micphy, miibus, micphy_driver, micphy_devclass, 0, 0);

static const struct mii_phydesc micphys[] = {
	MII_PHY_DESC(MICREL, KSZ9021),
	MII_PHY_END
};

static const struct mii_phy_funcs micphy_funcs = {
	micphy_service,
	ukphy_status,
	mii_phy_reset
};

static void
micphy_write(struct mii_softc *sc, uint32_t reg, uint32_t val)
{

	PHY_WRITE(sc, MII_KSZPHY_EXTREG, KSZPHY_EXTREG_WRITE | reg);
	PHY_WRITE(sc, MII_KSZPHY_EXTREG_WRITE, val);
}

static int
ksz9021_load_values(struct mii_softc *sc, phandle_t node, uint32_t reg,
			char *field1, char *field2,
			char *field3, char *field4)
{
	pcell_t dts_value[1];
	int len;
	int val;

	val = 0;

	if ((len = OF_getproplen(node, field1)) > 0) {
		OF_getencprop(node, field1, dts_value, len);
		val = PS_TO_REG(dts_value[0]);
	}

	if ((len = OF_getproplen(node, field2)) > 0) {
		OF_getencprop(node, field2, dts_value, len);
		val |= PS_TO_REG(dts_value[0]) << 4;
	}

	if ((len = OF_getproplen(node, field3)) > 0) {
		OF_getencprop(node, field3, dts_value, len);
		val |= PS_TO_REG(dts_value[0]) << 8;
	}

	if ((len = OF_getproplen(node, field4)) > 0) {
		OF_getencprop(node, field4, dts_value, len);
		val |= PS_TO_REG(dts_value[0]) << 12;
	}

	micphy_write(sc, reg, val);

	return (0);
}

static int
micphy_probe(device_t dev)
{

	return (mii_phy_dev_probe(dev, micphys, BUS_PROBE_DEFAULT));
}

static int
micphy_attach(device_t dev)
{
	struct mii_softc *sc;
	phandle_t node;
	device_t miibus;
	device_t parent;

	sc = device_get_softc(dev);

	mii_phy_dev_attach(dev, MIIF_NOMANPAUSE, &micphy_funcs, 1);
	mii_phy_setmedia(sc);

	miibus = device_get_parent(dev);
	parent = device_get_parent(miibus);

	if ((node = ofw_bus_get_node(parent)) == -1)
		return (ENXIO);

	ksz9021_load_values(sc, node, MII_KSZPHY_CLK_CONTROL_PAD_SKEW,
			"txen-skew-ps", "txc-skew-ps",
			"rxdv-skew-ps", "rxc-skew-ps");

	ksz9021_load_values(sc, node, MII_KSZPHY_RX_DATA_PAD_SKEW,
			"rxd0-skew-ps", "rxd1-skew-ps",
			"rxd2-skew-ps", "rxd3-skew-ps");

	ksz9021_load_values(sc, node, MII_KSZPHY_TX_DATA_PAD_SKEW,
			"txd0-skew-ps", "txd1-skew-ps",
			"txd2-skew-ps", "txd3-skew-ps");

	return (0);
}

static int
micphy_service(struct mii_softc *sc, struct mii_data *mii, int cmd)
{

	switch (cmd) {
	case MII_POLLSTAT:
		break;

	case MII_MEDIACHG:
		mii_phy_setmedia(sc);
		break;

	case MII_TICK:
		if (mii_phy_tick(sc) == EJUSTRETURN)
			return (0);
		break;
	}

	/* Update the media status. */
	PHY_STATUS(sc);

	/* Callback if something changed. */
	mii_phy_update(sc, cmd);
	return (0);
}
