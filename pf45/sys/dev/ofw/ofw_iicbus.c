/*-
 * Copyright (c) 2009, Nathan Whitehorn <nwhitehorn@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include "iicbus_if.h"

/* Methods */
static device_probe_t ofw_iicbus_probe;
static device_attach_t ofw_iicbus_attach;
static device_t ofw_iicbus_add_child(device_t dev, u_int order,
    const char *name, int unit);
static const struct ofw_bus_devinfo *ofw_iicbus_get_devinfo(device_t bus,
    device_t dev);

static device_method_t ofw_iicbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ofw_iicbus_probe),
	DEVMETHOD(device_attach,	ofw_iicbus_attach),

	/* Bus interface */
	DEVMETHOD(bus_child_pnpinfo_str, ofw_bus_gen_child_pnpinfo_str),
	DEVMETHOD(bus_add_child,	ofw_iicbus_add_child),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	ofw_iicbus_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	KOBJMETHOD_END
};

struct ofw_iicbus_devinfo {
	struct iicbus_ivar	opd_dinfo;
	struct ofw_bus_devinfo	opd_obdinfo;
};

static devclass_t ofwiicbus_devclass;

DEFINE_CLASS_1(iicbus, ofw_iicbus_driver, ofw_iicbus_methods,
    sizeof(struct iicbus_softc), iicbus_driver);
DRIVER_MODULE(ofw_iicbus, iichb, ofw_iicbus_driver, ofwiicbus_devclass, 0, 0);
MODULE_VERSION(ofw_iicbus, 1);
MODULE_DEPEND(ofw_iicbus, iicbus, 1, 1, 1);

static int
ofw_iicbus_probe(device_t dev)
{

	if (ofw_bus_get_node(dev) == 0)
		return (ENXIO);
	device_set_desc(dev, "OFW I2C bus");

	return (0);
}

static int
ofw_iicbus_attach(device_t dev)
{
	struct iicbus_softc *sc = IICBUS_SOFTC(dev);
	struct ofw_iicbus_devinfo *dinfo;
	phandle_t child;
	device_t childdev;
	uint32_t addr;

	sc->dev = dev;
	mtx_init(&sc->lock, "iicbus", NULL, MTX_DEF);
	iicbus_reset(dev, IIC_FASTEST, 0, NULL);

	bus_generic_probe(dev);
	bus_enumerate_hinted_children(dev);

	/*
	 * Attach those children represented in the device tree.
	 */
	for (child = OF_child(ofw_bus_get_node(dev)); child != 0;
	    child = OF_peer(child)) {
		/*
		 * Try to get the I2C address first from the i2c-address
		 * property, then try the reg property.  It moves around
		 * on different systems.
		 */
		if (OF_getprop(child, "i2c-address", &addr, sizeof(addr)) == -1)
			if (OF_getprop(child, "reg", &addr, sizeof(addr)) == -1)
				continue;

		/*
		 * Now set up the I2C and OFW bus layer devinfo and add it
		 * to the bus.
		 */
		dinfo = malloc(sizeof(struct ofw_iicbus_devinfo), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (dinfo == NULL)
			continue;
		dinfo->opd_dinfo.addr = addr;
		if (ofw_bus_gen_setup_devinfo(&dinfo->opd_obdinfo, child) !=
		    0) {
			free(dinfo, M_DEVBUF);
			continue;
		}
		childdev = device_add_child(dev, NULL, -1);
		device_set_ivars(childdev, dinfo);
	}

	return (bus_generic_attach(dev));
}

static device_t
ofw_iicbus_add_child(device_t dev, u_int order, const char *name, int unit)
{
	device_t child;
	struct ofw_iicbus_devinfo *devi;

	child = device_add_child_ordered(dev, order, name, unit);
	if (child == NULL)
		return (child);
	devi = malloc(sizeof(struct ofw_iicbus_devinfo), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (devi == NULL) {
		device_delete_child(dev, child);
		return (0);
	}

	/*
	 * NULL all the OFW-related parts of the ivars for non-OFW
	 * children.
	 */
	devi->opd_obdinfo.obd_node = -1;
	devi->opd_obdinfo.obd_name = NULL;
	devi->opd_obdinfo.obd_compat = NULL;
	devi->opd_obdinfo.obd_type = NULL;
	devi->opd_obdinfo.obd_model = NULL;

	device_set_ivars(child, devi);

	return (child);
}

static const struct ofw_bus_devinfo *
ofw_iicbus_get_devinfo(device_t bus, device_t dev)
{
	struct ofw_iicbus_devinfo *dinfo;

	dinfo = device_get_ivars(dev);
	return (&dinfo->opd_obdinfo);
}
