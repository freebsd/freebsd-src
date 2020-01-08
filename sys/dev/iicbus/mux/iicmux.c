/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ian Lepore <ian@freebsd.org>
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

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sysctl.h>

#ifdef FDT
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>
#endif

#include <dev/iicbus/iiconf.h>
#include "iicbus_if.h"
#include "iicmux_if.h"
#include "iicmux.h"

/*------------------------------------------------------------------------------
 * iicbus methods, called by the iicbus functions in iiconf.c.
 *
 * All these functions return an IIC adapter-layer error code (because we are
 * pretending to be a host bridge/i2c controller).  Standard errno values
 * returned from these must be encoded using iic2errno().
 *----------------------------------------------------------------------------*/

static int
iicmux_callback(device_t dev, int index, caddr_t data)
{
	struct iicmux_softc *sc = device_get_softc(dev);
	struct iic_reqbus_data *rd;
	int err, i;

	/* If it's not one of the operations we know about, bail early. */
	if (index != IIC_REQUEST_BUS && index != IIC_RELEASE_BUS)
		return (iic2errno(EOPNOTSUPP));

	/*
	 * Ensure that the data passed to us includes the device_t of the child
	 * bus and device.  If missing, someone bypassed iicbus_request_bus()
	 * and called this method directly using the old calling standard.  If
	 * present, find the index of the child bus that called us.
	 */
	rd = (struct iic_reqbus_data *)data;
	if (!(rd->flags & IIC_REQBUS_DEV))
		return (iic2errno(EINVAL));

	for (i = 0; i <= sc->maxbus && sc->childdevs[i] != rd->bus; ++i)
		continue;
	if (i > sc->maxbus)
		return (iic2errno(ENOENT));

	/*
	 * If the operation is a release it "cannot fail".  Idle the downstream
	 * bus, then release exclusive use of the upstream bus, and we're done.
	 */
	if (index == IIC_RELEASE_BUS) {
		if (sc->debugmux > 0) {
			device_printf(dev, "idle the bus for %s on bus %s\n",
			    device_get_nameunit(rd->dev),
			    device_get_nameunit(rd->bus));
		}
		IICMUX_BUS_SELECT(dev, IICMUX_SELECT_IDLE, rd);
		iicbus_release_bus(sc->busdev, dev);
		return (IIC_NOERR);
	}

	if (sc->debugmux > 0) {
		device_printf(dev, "select bus idx %d for %s on bus %s\n", i,
		    device_get_nameunit(rd->dev), device_get_nameunit(rd->bus));
	}

	/*
	 * The operation is a request for exclusive use.  First we have to
	 * request exclusive use of our upstream bus.  If multiple slave devices
	 * from our different child buses attempt to do IO at the same time,
	 * this is what ensures that they don't switch the bus out from under
	 * each other. The first one in proceeds and others wait here (or get an
	 * EWOULDBLOCK return if they're using IIC_DONTWAIT).
	 */
	if ((err = iicbus_request_bus(sc->busdev, dev, rd->flags)) != 0)
		return (err); /* Already an IIC error code. */

	/*
	 * Now that we own exclusive use of the upstream bus, connect it to the
	 * downstream bus where the request came from.
	 */
	if ((err = IICMUX_BUS_SELECT(dev, i, rd)) != 0)
		iicbus_release_bus(sc->busdev, dev);

	return (err);
}

static u_int
iicmux_get_frequency(device_t dev, u_char speed)
{
	struct iicmux_softc *sc = device_get_softc(dev);

	return (IICBUS_GET_FREQUENCY(sc->busdev, speed));
}

#ifdef FDT
static phandle_t
iicmux_get_node(device_t dev, device_t child)
{
	struct iicmux_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i <= sc->maxbus; ++i) {
		if (sc->childdevs[i] == child)
			return (sc->childnodes[i]);
	}
	return (0); /* null handle */
}
#endif

static int
iicmux_intr(device_t dev, int event, char *buf)
{
	struct iicmux_softc *sc = device_get_softc(dev);

	/* XXX iicbus_intr() in iiconf.c should return status. */

	iicbus_intr(sc->busdev, event, buf);
	return (0); 
}

static int
iicmux_read(device_t dev, char *buf, int len, int *bytes, int last, int delay)
{
	struct iicmux_softc *sc = device_get_softc(dev);

	return (iicbus_read(sc->busdev, buf, len, bytes, last, delay));
}

static int
iicmux_repeated_start(device_t dev, u_char slave, int timeout)
{
	struct iicmux_softc *sc = device_get_softc(dev);

	return (iicbus_repeated_start(sc->busdev, slave, timeout));
}

static int
iicmux_reset(device_t dev, u_char speed, u_char addr, u_char *oldaddr)
{
	struct iicmux_softc *sc = device_get_softc(dev);

	return (iicbus_reset(sc->busdev, speed, addr, oldaddr));
}

static int
iicmux_start(device_t dev, u_char slave, int timeout)
{
	struct iicmux_softc *sc = device_get_softc(dev);

	return (iicbus_start(sc->busdev, slave, timeout));
}

static int
iicmux_stop(device_t dev)
{
	struct iicmux_softc *sc = device_get_softc(dev);

	return (iicbus_stop(sc->busdev));
}

static int
iicmux_transfer( device_t dev, struct iic_msg *msgs, uint32_t nmsgs)
{
	struct iicmux_softc *sc = device_get_softc(dev);

	return (iicbus_transfer(sc->busdev, msgs, nmsgs));
}

static int
iicmux_write(device_t dev, const char *buf, int len, int *bytes, int timeout)
{
	struct iicmux_softc *sc = device_get_softc(dev);

	return (iicbus_write(sc->busdev, buf, len, bytes, timeout));
}

/*------------------------------------------------------------------------------
 * iicmux helper functions, called by hardware-specific drivers.                
 * All these functions return a standard errno value.
 *----------------------------------------------------------------------------*/

int
iicmux_add_child(device_t dev, device_t child, int busidx)
{
	struct iicmux_softc *sc = device_get_softc(dev);

	if (busidx >= sc->numbuses) {
		device_printf(dev,
		    "iicmux_add_child: bus idx %d too big", busidx);
		return (EINVAL);
	}
	if (sc->childdevs[busidx] != NULL) {
		device_printf(dev, "iicmux_add_child: bus idx %d already added",
		    busidx);
		return (EINVAL);
	}

	sc->childdevs[busidx] = child;
	if (sc->maxbus < busidx)
		sc->maxbus = busidx;

	return (0);
}

int
iicmux_attach(device_t dev, device_t busdev, int numbuses)
{
	struct iicmux_softc *sc = device_get_softc(dev);
	int i, numadded;

	if (numbuses >= IICMUX_MAX_BUSES) {
		device_printf(dev, "iicmux_attach: numbuses %d > max %d\n",
		    numbuses, IICMUX_MAX_BUSES);
		return (EINVAL);
	}

	sc->dev = dev;
	sc->busdev = busdev;
	sc->maxbus = -1;
	sc->numbuses = numbuses;

	SYSCTL_ADD_UINT(device_get_sysctl_ctx(sc->dev), 
	    SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev)), OID_AUTO,
	    "debugmux", CTLFLAG_RWTUN, &sc->debugmux, 0, "debug mux operations");

        /*
         * Add children...
         */
	numadded = 0;

#ifdef FDT
	phandle_t child, node, parent;
	pcell_t reg;
	int idx;

	/*
	 * Find our FDT node.  Child nodes within our node will become our
	 * iicbus children.
	 */
	if((node = ofw_bus_get_node(sc->dev)) == 0) {
		device_printf(sc->dev, "cannot find FDT node\n");
		return (ENOENT);
	}

	/*
	 * First we have to see if there is a child node named "i2c-mux".  If it
	 * exists, all children of that node are buses, else all children of the
	 * device node are buses.
	 */
	if ((parent = ofw_bus_find_child(node, "i2c-mux")) == 0)
		parent = node;

	/*
	 * Attach the children represented in the device tree.
	 */
	for (child = OF_child(parent); child != 0; child = OF_peer(child)) {
		if (OF_getencprop(child, "reg", &reg, sizeof(reg)) == -1) {
			device_printf(dev,
			    "child bus missing required 'reg' property\n");
			continue;
		}
		idx = (int)reg;
		if (idx >= sc->numbuses) {
			device_printf(dev,
			    "child bus 'reg' property %d exceeds the number "
			    "of buses supported by the device (%d)\n",
			    idx, sc->numbuses);
			continue;
		}
		sc->childdevs[idx] = device_add_child(sc->dev, "iicbus", -1);
		sc->childnodes[idx] = child;
		if (sc->maxbus < idx)
			sc->maxbus = idx;
		++numadded;
	}
#endif /* FDT */

	/*
	 * If we configured anything using FDT data, we're done.  Otherwise add
	 * an iicbus child for every downstream bus supported by the mux chip.
	 */
	if (numadded > 0)
		return (0);

	for (i = 0; i < sc->numbuses; ++i) {
		sc->childdevs[i] = device_add_child(sc->dev, "iicbus", -1);
	}
	sc->maxbus = sc->numbuses - 1;

	return (0);
}

int
iicmux_detach(device_t dev)
{
	struct iicmux_softc *sc = device_get_softc(dev);
	int err, i;

	/* Delete only the children we added in iicmux_add* functions. */
	for (i = 0; i <= sc->maxbus; ++i) {
		if (sc->childdevs[i] == NULL)
			continue;
		if ((err = device_delete_child(dev, sc->childdevs[i])) != 0)
			return (err);
		sc->childdevs[i] = NULL;
	}

	return (0);
}

static device_method_t iicmux_methods [] = {
	/* iicbus_if methods */
	DEVMETHOD(iicbus_intr,			iicmux_intr),
	DEVMETHOD(iicbus_callback,		iicmux_callback),
	DEVMETHOD(iicbus_repeated_start,	iicmux_repeated_start),
	DEVMETHOD(iicbus_start,			iicmux_start),
	DEVMETHOD(iicbus_stop,			iicmux_stop),
	DEVMETHOD(iicbus_read,			iicmux_read),
	DEVMETHOD(iicbus_write,			iicmux_write),
	DEVMETHOD(iicbus_reset,			iicmux_reset),
	DEVMETHOD(iicbus_transfer,		iicmux_transfer),
	DEVMETHOD(iicbus_get_frequency,		iicmux_get_frequency),

#ifdef FDT
	/* ofwbus_if methods */
	DEVMETHOD(ofw_bus_get_node,		iicmux_get_node),
#endif

	DEVMETHOD_END
};

static int
iicmux_modevent(module_t mod, int type, void *unused)
{
	switch (type) {
	case MOD_LOAD:
		return 0;
	case MOD_UNLOAD:
		return 0;
	}
	return EINVAL;
}

static moduledata_t iicmux_mod = {
	"iicmux",
	iicmux_modevent,
	0
};

DEFINE_CLASS_0(iicmux, iicmux_driver, iicmux_methods,
    sizeof(struct iicmux_softc));

DECLARE_MODULE(iicmux, iicmux_mod, SI_SUB_DRIVERS, SI_ORDER_ANY);
MODULE_VERSION(iicmux, 1);

MODULE_DEPEND(iicmux, iicbus, 1, 1, 1);
