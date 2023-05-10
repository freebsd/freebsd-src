/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ian Lepore <ian@FreeBSD.org>
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/module.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>
#include <dev/pwm/pwmbus.h>

#include "pwmbus_if.h"

struct ofw_pwmbus_ivars {
	struct pwmbus_ivars	base;
	struct ofw_bus_devinfo	devinfo;
};

struct ofw_pwmbus_softc {
	struct pwmbus_softc	base;
};

/*
 * bus_if methods...
 */

static device_t
ofw_pwmbus_add_child(device_t dev, u_int order, const char *name, int unit)
{
	device_t child;
	struct ofw_pwmbus_ivars *ivars;

	if ((ivars = malloc(sizeof(struct ofw_pwmbus_ivars), M_DEVBUF,
	    M_NOWAIT | M_ZERO)) == NULL) {
		return (NULL);
	}

	if ((child = device_add_child_ordered(dev, order, name, unit)) == NULL) {
		free(ivars, M_DEVBUF);
		return (NULL);
	}

	ivars->devinfo.obd_node = -1;
	device_set_ivars(child, ivars);

	return (child);
}

static void
ofw_pwmbus_child_deleted(device_t dev, device_t child)
{
	struct ofw_pwmbus_ivars *ivars;

	ivars = device_get_ivars(child);
	if (ivars != NULL) {
		ofw_bus_gen_destroy_devinfo(&ivars->devinfo);
		free(ivars, M_DEVBUF);
	}
}

static const struct ofw_bus_devinfo *
ofw_pwmbus_get_devinfo(device_t bus, device_t dev)
{
	struct ofw_pwmbus_ivars *ivars;

	ivars = device_get_ivars(dev);
	return (&ivars->devinfo);
}

/*
 * device_if methods...
 */

static int
ofw_pwmbus_probe(device_t dev)
{

	if (ofw_bus_get_node(dev) == -1) {
		return (ENXIO);
	}
	device_set_desc(dev, "OFW PWM bus");

	return (BUS_PROBE_DEFAULT);
}

static int
ofw_pwmbus_attach(device_t dev)
{
	struct ofw_pwmbus_softc *sc;
	struct ofw_pwmbus_ivars *ivars;
	phandle_t node;
	device_t child, parent;
	pcell_t  chan;
	bool any_children;

	sc = device_get_softc(dev);
	sc->base.dev = dev;
	parent = device_get_parent(dev);

	if (PWMBUS_CHANNEL_COUNT(parent, &sc->base.nchannels) != 0 ||
	    sc->base.nchannels == 0) {
		device_printf(dev, "No channels on parent %s\n",
		    device_get_nameunit(parent));
		return (ENXIO);
	}

	/*
	 * Attach the children found in the fdt node of the hardware controller.
	 * Hardware controllers must implement the ofw_bus_get_node method so
	 * that our call to ofw_bus_get_node() gets back the controller's node.
	 */
	any_children = false;
	node = ofw_bus_get_node(dev);
	for (node = OF_child(node); node != 0; node = OF_peer(node)) {
		/*
		 * The child has to have a reg property; its value is the
		 * channel number so range-check it.
		 */
		if (OF_getencprop(node, "reg", &chan, sizeof(chan)) == -1)
			continue;
		if (chan >= sc->base.nchannels)
			continue;

		if ((child = ofw_pwmbus_add_child(dev, 0, NULL, -1)) == NULL)
			continue;

		ivars = device_get_ivars(child);
		ivars->base.pi_channel = chan;

		/* Set up the standard ofw devinfo. */
		if (ofw_bus_gen_setup_devinfo(&ivars->devinfo, node) != 0) {
			device_delete_child(dev, child);
			continue;
		}
		any_children = true;
	}

	/*
	 * If we didn't find any children in the fdt data, add a pwmc(4) child
	 * for each channel, like the base pwmbus does.  The idea is that if
	 * there is any fdt data, then we do exactly what it says and nothing
	 * more, otherwise we just provide generic userland access to all the
	 * pwm channels that exist like the base pwmbus's attach code does.
	 */
	if (!any_children) {
		for (chan = 0; chan < sc->base.nchannels; ++chan) {
			child = ofw_pwmbus_add_child(dev, 0, "pwmc", -1);
			if (child == NULL) {
				device_printf(dev, "failed to add pwmc child "
				    " device for channel %u\n", chan);
				continue;
			}
			ivars = device_get_ivars(child);
			ivars->base.pi_channel = chan;
		}
	}
	bus_enumerate_hinted_children(dev);
	bus_generic_probe(dev);

	return (bus_generic_attach(dev));
}

static device_method_t ofw_pwmbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,           ofw_pwmbus_probe),
	DEVMETHOD(device_attach,          ofw_pwmbus_attach),

	/* Bus interface */
	DEVMETHOD(bus_child_pnpinfo,	  ofw_bus_gen_child_pnpinfo),
	DEVMETHOD(bus_add_child,          ofw_pwmbus_add_child),
	DEVMETHOD(bus_child_deleted,      ofw_pwmbus_child_deleted),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,    ofw_pwmbus_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,     ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,      ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,       ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,       ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,       ofw_bus_gen_get_type),

	DEVMETHOD_END
};

DEFINE_CLASS_1(pwmbus, ofw_pwmbus_driver, ofw_pwmbus_methods,
    sizeof(struct pwmbus_softc), pwmbus_driver);
EARLY_DRIVER_MODULE(ofw_pwmbus, pwm, ofw_pwmbus_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(ofw_pwmbus, 1);
MODULE_DEPEND(ofw_pwmbus, pwmbus, 1, 1, 1);
