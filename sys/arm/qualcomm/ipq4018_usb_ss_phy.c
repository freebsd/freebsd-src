/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Michal Meloun <mmel@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <dev/extres/hwreset/hwreset.h>
#include <dev/extres/phy/phy_usb.h>
#include <dev/extres/regulator/regulator.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/fdt/simple_mfd.h>
#include "phynode_if.h"
#include "phynode_usb_if.h"

static struct ofw_compat_data compat_data[] = {
	{"qcom,usb-ss-ipq4019-phy",	1},
	{NULL,				0},
};

struct ipq4018_usb_ss_phy_softc {
	device_t		dev;
};

struct ipq4018_usb_ss_phynode_sc {
	struct phynode_usb_sc	usb_sc;
	int			mode;
	hwreset_t		por_rst;
};

static int
ipq4018_usb_ss_phynode_phy_enable(struct phynode *phynode, bool enable)
{
	struct ipq4018_usb_ss_phynode_sc *sc;
	device_t dev;
	int rv;

	dev = phynode_get_device(phynode);
	sc = phynode_get_softc(phynode);

	/*
	 * For power-off - assert por, sleep for 10ms
	 */
	rv = hwreset_assert(sc->por_rst);
	if (rv != 0)
		goto done;
	DELAY(10*1000);

	/*
	 * For power-on - power off first, then deassert por.
	 */
	if (enable) {
		rv = hwreset_deassert(sc->por_rst);
		if (rv != 0)
			goto done;
		DELAY(10*1000);
	}

done:
	if (rv != 0) {
		device_printf(dev, "%s: failed, rv=%d\n", __func__, rv);
	}
	return (rv);
}

 /* Phy controller class and methods. */
static phynode_method_t ipq4018_usb_ss_phynode_methods[] = {
	PHYNODEUSBMETHOD(phynode_enable,	ipq4018_usb_ss_phynode_phy_enable),
	PHYNODEUSBMETHOD_END
};
DEFINE_CLASS_1(ipq4018_usb_ss_phynode, ipq4018_usb_ss_phynode_class,
    ipq4018_usb_ss_phynode_methods,
    sizeof(struct ipq4018_usb_ss_phynode_sc), phynode_usb_class);

static int
ipq4018_usb_ss_usbphy_init_phy(struct ipq4018_usb_ss_phy_softc *sc,
    phandle_t node)
{
	struct phynode *phynode;
	struct phynode_init_def phy_init;
	struct ipq4018_usb_ss_phynode_sc *phy_sc;
	int rv;
	hwreset_t por_rst = NULL;

	/* FDT resources */
	rv = hwreset_get_by_ofw_name(sc->dev, node, "por_rst", &por_rst);
	if (rv != 0 && rv != ENOENT) {
		device_printf(sc->dev, "Cannot get 'por_rst' reset\n");
		goto fail;
	}

	/* Create and register phy. */
	bzero(&phy_init, sizeof(phy_init));
	phy_init.id = 1;
	phy_init.ofw_node = node;
	phynode = phynode_create(sc->dev, &ipq4018_usb_ss_phynode_class,
	    &phy_init);
	if (phynode == NULL) {
		device_printf(sc->dev, "Cannot create phy.\n");
		return (ENXIO);
	}

	phy_sc = phynode_get_softc(phynode);
	phy_sc->por_rst = por_rst;

	if (phynode_register(phynode) == NULL) {
		device_printf(sc->dev, "Cannot register phy.\n");
		return (ENXIO);
	}

	(void) ipq4018_usb_ss_phynode_phy_enable(phynode, true);

	return (0);

fail:
	if (por_rst != NULL)
		 hwreset_release(por_rst);

	return (ENXIO);
}

static int
ipq4018_usb_ss_usbphy_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "IPQ4018/IPQ4019 USB SS PHY");
	return (BUS_PROBE_DEFAULT);
}

static int
ipq4018_usb_ss_usbphy_attach(device_t dev)
{
	struct ipq4018_usb_ss_phy_softc *sc;
	phandle_t node;
	int rv;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(sc->dev);

	rv = ipq4018_usb_ss_usbphy_init_phy(sc, node);
	if (rv != 0)
		goto fail;
	return (bus_generic_attach(dev));

fail:
	return (ENXIO);
}

static int
ipq4018_usb_ss_usbphy_detach(device_t dev)
{
	struct ipq4018_usb_ss_phy_softc *sc;
	sc = device_get_softc(dev);

	return (0);
}

static device_method_t ipq4018_usb_ss_usbphy_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			ipq4018_usb_ss_usbphy_probe),
	DEVMETHOD(device_attach,		ipq4018_usb_ss_usbphy_attach),
	DEVMETHOD(device_detach,		ipq4018_usb_ss_usbphy_detach),
	DEVMETHOD_END
};

static DEFINE_CLASS_0(ipq4018_usb_ss_usbphy, ipq4018_usb_ss_usbphy_driver,
    ipq4018_usb_ss_usbphy_methods,
    sizeof(struct ipq4018_usb_ss_phy_softc));
EARLY_DRIVER_MODULE(ipq4018_usb_ss_usbphy, simplebus,
    ipq4018_usb_ss_usbphy_driver, NULL, NULL,
    BUS_PASS_TIMER + BUS_PASS_ORDER_LAST);
