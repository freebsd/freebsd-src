/*-
 * Copyright (c) 2019 Emmanuel Vadot <manu@FreeBSD.org>
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


/* Based on sys/arm/ti/ti_sysc.c */

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

#define TI_AM3_SCM			14
#define TI_AM4_SCM			13
#define TI_DM814_SCRM			12
#define TI_DM816_SCRM			11
#define TI_OMAP2_SCM			10
#define TI_OMAP3_SCM			9
#define TI_OMAP4_SCM_CORE		8
#define TI_OMAP4_SCM_PADCONF_CORE	7
#define TI_OMAP4_SCM_WKUP		6
#define TI_OMAP4_SCM_PADCONF_WKUP	5
#define TI_OMAP5_SCM_CORE		4
#define TI_OMAP5_SCM_PADCONF_CORE	3
#define TI_OMAP5_SCM_WKUP_PAD_CONF	2
#define TI_DRA7_SCM_CORE		1
#define TI_SCM_END			0

static struct ofw_compat_data compat_data[] = {
	{ "ti,am3-scm",			TI_AM3_SCM },
	{ "ti,am4-scm",			TI_AM4_SCM },
	{ "ti,dm814-scrm",		TI_DM814_SCRM },
	{ "ti,dm816-scrm",		TI_DM816_SCRM },
	{ "ti,omap2-scm",		TI_OMAP2_SCM },
	{ "ti,omap3-scm",		TI_OMAP3_SCM },
	{ "ti,omap4-scm-core",		TI_OMAP4_SCM_CORE },
	{ "ti,omap4-scm-padconf-core",	TI_OMAP4_SCM_PADCONF_CORE },
	{ "ti,omap4-scm-wkup",		TI_OMAP4_SCM_WKUP },
	{ "ti,omap4-scm-padconf-wkup",	TI_OMAP4_SCM_PADCONF_WKUP },
	{ "ti,omap5-scm-core",		TI_OMAP5_SCM_CORE },
	{ "ti,omap5-scm-padconf-core",	TI_OMAP5_SCM_PADCONF_CORE },
	{ "ti,omap5-scm-wkup-pad-conf",	TI_OMAP5_SCM_WKUP_PAD_CONF },
	{ "ti,dra7-scm-core",		TI_DRA7_SCM_CORE },
	{ NULL,				TI_SCM_END }
};

struct ti_scm_softc {
	struct simplebus_softc	sc;
	device_t		dev;
};

static int ti_scm_probe(device_t dev);
static int ti_scm_attach(device_t dev);
static int ti_scm_detach(device_t dev);

static int
ti_scm_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "TI OMAP Control Module");

	return (BUS_PROBE_DEFAULT);
}

static int
ti_scm_attach(device_t dev)
{
	struct ti_scm_softc *sc;
	device_t cdev;
	phandle_t node, child;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);

	simplebus_init(dev, node);
	if (simplebus_fill_ranges(node, &sc->sc) < 0) {
		device_printf(dev, "could not get ranges\n");
		return (ENXIO);
	}

	for (child = OF_child(node); child > 0; child = OF_peer(child)) {
		cdev = simplebus_add_device(dev, child, 0, NULL, -1, NULL);
		if (cdev != NULL)
			device_probe_and_attach(cdev);
	}

	return (bus_generic_attach(dev));
}

static int
ti_scm_detach(device_t dev)
{
	return (EBUSY);
}

static device_method_t ti_scm_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ti_scm_probe),
	DEVMETHOD(device_attach,	ti_scm_attach),
	DEVMETHOD(device_detach,	ti_scm_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(ti_scm, ti_scm_driver, ti_scm_methods,
    sizeof(struct ti_scm_softc), simplebus_driver);

static devclass_t ti_scm_devclass;

EARLY_DRIVER_MODULE(ti_scm, simplebus, ti_scm_driver,
    ti_scm_devclass, 0, 0, BUS_PASS_BUS + BUS_PASS_ORDER_FIRST);
MODULE_VERSION(ti_scm, 1);
MODULE_DEPEND(ti_scm, ti_sysc, 1, 1, 1);

