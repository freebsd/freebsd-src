/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Beckhoff Automation GmbH & Co. KG
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <machine/bus.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

struct ofw_firmware_softc {
	struct simplebus_softc	sc;
	device_t		dev;
};

static struct simplebus_devinfo *
ofw_firmware_setup_dinfo(device_t dev, phandle_t node,
    struct simplebus_devinfo *di)
{
	struct simplebus_softc *sc;
	struct simplebus_devinfo *ndi;

	sc = device_get_softc(dev);
	if (di == NULL)
		ndi = malloc(sizeof(*ndi), M_DEVBUF, M_WAITOK | M_ZERO);
	else
		ndi = di;
	if (ofw_bus_gen_setup_devinfo(&ndi->obdinfo, node) != 0) {
		if (di == NULL)
			free(ndi, M_DEVBUF);
		return (NULL);
	}

	/* reg resources is from the parent but interrupts is on the node itself */
	resource_list_init(&ndi->rl);
	ofw_bus_reg_to_rl(dev, OF_parent(node), sc->acells, sc->scells, &ndi->rl);
	ofw_bus_intr_to_rl(dev, node, &ndi->rl, NULL);

	return (ndi);
}

static device_t
ofw_firmware_add_device(device_t dev, phandle_t node, u_int order,
    const char *name, int unit, struct simplebus_devinfo *di)
{
	struct simplebus_devinfo *ndi;
	device_t cdev;

	if ((ndi = ofw_firmware_setup_dinfo(dev, node, di)) == NULL)
		return (NULL);
	cdev = device_add_child_ordered(dev, order, name, unit);
	if (cdev == NULL) {
		device_printf(dev, "<%s>: device_add_child failed\n",
		    ndi->obdinfo.obd_name);
		resource_list_free(&ndi->rl);
		ofw_bus_gen_destroy_devinfo(&ndi->obdinfo);
		if (di == NULL)
			free(ndi, M_DEVBUF);
		return (NULL);
	}
	device_set_ivars(cdev, ndi);

	return(cdev);
}

static int
ofw_firmware_probe(device_t dev)
{
	const char *name, *compat;
	phandle_t root, parent;

	name = ofw_bus_get_name(dev);
	if (name == NULL || strcmp(name, "firmware") != 0)
		return (ENXIO);
	parent = OF_parent(ofw_bus_get_node(dev));
	root = OF_finddevice("/");
	if (parent != root)
		return (ENXIO);
	compat = ofw_bus_get_compat(dev);
	if (compat != NULL)
		return (ENXIO);

	device_set_desc(dev, "OFW Firmware Group");
	return (BUS_PROBE_GENERIC);
}

static int
ofw_firmware_attach(device_t dev)
{
	struct ofw_firmware_softc *sc;
	phandle_t node, child;
	device_t cdev;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);

	if (OF_getencprop(node, "#address-cells", &sc->sc.acells,
	    sizeof(sc->sc.acells)) == -1) {
		if (OF_getencprop(OF_parent(node), "#address-cells", &sc->sc.acells,
		    sizeof(sc->sc.acells)) == -1) {
			sc->sc.acells = 2;
		}
	}
	if (OF_getencprop(node, "#size-cells", &sc->sc.scells,
	    sizeof(sc->sc.scells)) == -1) {
		if (OF_getencprop(OF_parent(node), "#size-cells", &sc->sc.scells,
		    sizeof(sc->sc.scells)) == -1) {
			sc->sc.scells = 1;
		}
	}

	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		cdev = ofw_firmware_add_device(dev, child, 0, NULL, -1, NULL);
		if (cdev != NULL)
			device_probe_and_attach(cdev);
	}

	return (bus_generic_attach(dev));
}

static device_method_t ofw_firmware_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe, 	ofw_firmware_probe),
	DEVMETHOD(device_attach, 	ofw_firmware_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(ofw_firmware, ofw_firmware_driver, ofw_firmware_methods,
  sizeof(struct ofw_firmware_softc), simplebus_driver);

EARLY_DRIVER_MODULE(ofw_firmware, simplebus, ofw_firmware_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(ofw_firmware, 1);
