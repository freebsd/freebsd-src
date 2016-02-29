/*-
 * Copyright (c) 2012 Semihalf.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

#include "opt_platform.h"

static MALLOC_DEFINE(M_DPAA, "dpaa", "dpaa devices information");

static int dpaa_probe(device_t dev);
static int dpaa_attach(device_t dev);

static const struct ofw_bus_devinfo *dpaa_get_devinfo(device_t bus,
    device_t child);

struct dpaa_softc {

};

struct dpaa_devinfo {
	struct ofw_bus_devinfo  di_ofw;
};

static device_method_t dpaa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dpaa_probe),
	DEVMETHOD(device_attach,	dpaa_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* OFW bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	dpaa_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	{0, 0}
};

static driver_t dpaa_driver = {
        "dpaa",
        dpaa_methods,
        sizeof(struct dpaa_softc),
};

static devclass_t dpaa_devclass;
DRIVER_MODULE_ORDERED(dpaa, ofwbus, dpaa_driver, dpaa_devclass, 0, 0,
    SI_ORDER_ANY);

static int
dpaa_probe(device_t dev)
{

        if (!ofw_bus_is_compatible(dev, "fsl,dpaa"))
                return (ENXIO);

	device_set_desc(dev, "Freescale Data Path Acceleration Architecture");

	return (BUS_PROBE_DEFAULT);
}

static int
dpaa_attach(device_t dev)
{
	device_t dev_child;
	phandle_t dt_node, dt_child, enet_node;
	struct dpaa_devinfo *di;
	pcell_t cell_index;

	cell_index = 0;
	/*
	 * Walk dpaa and add direct subordinates as our children.
	 */
	dt_node = ofw_bus_get_node(dev);
	dt_child = OF_child(dt_node);

	for (; dt_child != 0; dt_child = OF_peer(dt_child)) {

		/* Check and process 'status' property. */
		if (!(fdt_is_enabled(dt_child)))
			continue;

		di = (struct dpaa_devinfo *)malloc(sizeof(*di), M_DPAA,
		    M_WAITOK | M_ZERO);

		if (ofw_bus_gen_setup_devinfo(&di->di_ofw, dt_child) != 0) {
			free(di, M_DPAA);
			device_printf(dev, "could not set up devinfo\n");
			continue;
		}

		/*
		 * dTSEC number from SoC is equal to number get from
		 * dts file.
		 */
		if (OF_getprop(dt_child, "fsl,fman-mac",
		    (void *)&enet_node, sizeof(enet_node)) == -1) {
			device_printf(dev, "Could not get fsl,fman-mac "
			    "from dts\n");
			continue;
		}

		if ((enet_node = OF_instance_to_package(enet_node)) == -1) {
			device_printf(dev, "Could not get enet node\n");
			continue;
		}

		if (OF_getprop(enet_node, "cell-index",
		    (void *)&cell_index, sizeof(cell_index)) == -1) {
			device_printf(dev, "Could not get cell-index from enet "
			    "node\n");
			continue;
		}

		/* Add newbus device for this FDT node */
		dev_child = device_add_child(dev, "dtsec", (int)cell_index);
		if (dev_child == NULL) {
			device_printf(dev, "could not add child: %s\n",
			    di->di_ofw.obd_name);
			ofw_bus_gen_destroy_devinfo(&di->di_ofw);
			free(di, M_DPAA);
			continue;
		}

#ifdef DEBUG
		device_printf(dev, "added child: %s\n\n", di->di_ofw.obd_name);
#endif

		device_set_ivars(dev_child, di);
	}

	return (bus_generic_attach(dev));
}

static const struct ofw_bus_devinfo *
dpaa_get_devinfo(device_t bus, device_t child)
{
	struct dpaa_devinfo *di;

	di = device_get_ivars(child);
	return (&di->di_ofw);
}
