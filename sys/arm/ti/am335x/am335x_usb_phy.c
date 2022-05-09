/*-
 *
 * Copyright (c) 2020 Oskar Holmlund <oskar.holmlund@ohdata.se>
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/fbio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/resource.h>
#include <machine/bus.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#define TI_AM335X_USB_PHY		1
#define TI_AM335X_USB_PHY_END		0

static struct ofw_compat_data compat_data[] = {
	{ "ti,am335x-usb-phy",	TI_AM335X_USB_PHY },
	{ NULL,			TI_AM335X_USB_PHY_END }
};

struct ti_usb_phy_softc {
	device_t		dev;
};

static int ti_usb_phy_probe(device_t dev);
static int ti_usb_phy_attach(device_t dev);
static int ti_usb_phy_detach(device_t dev);

static int
ti_usb_phy_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "TI AM335x USB PHY");
	if (!bootverbose)
		device_quiet(dev);

	return (BUS_PROBE_DEFAULT);
}

static int
ti_usb_phy_attach(device_t dev)
{
	struct ti_usb_phy_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	/* FIXME: Add dev/extres/phy/ interface */

	return (bus_generic_attach(dev));
}

static int
ti_usb_phy_detach(device_t dev)
{
	return (EBUSY);
}

static device_method_t ti_usb_phy_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ti_usb_phy_probe),
	DEVMETHOD(device_attach,	ti_usb_phy_attach),
	DEVMETHOD(device_detach,	ti_usb_phy_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(ti_usb_phy, ti_usb_phy_driver, ti_usb_phy_methods,
    sizeof(struct ti_usb_phy_softc), simplebus_driver);

EARLY_DRIVER_MODULE(ti_usb_phy, simplebus, ti_usb_phy_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_FIRST);
MODULE_VERSION(ti_usb_phy, 1);
MODULE_DEPEND(ti_usb_phy, ti_sysc, 1, 1, 1);
