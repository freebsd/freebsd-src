/*-
 * Copyright (C) 2009 Nathan Whitehorn
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <machine/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

static int	ofw_cpulist_probe(device_t);
static int	ofw_cpulist_attach(device_t);
static const struct ofw_bus_devinfo *ofw_cpulist_get_devinfo(device_t dev,
    device_t child);

static MALLOC_DEFINE(M_OFWCPU, "ofwcpu", "OFW CPU device information");

static device_method_t ofw_cpulist_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ofw_cpulist_probe),
	DEVMETHOD(device_attach,	ofw_cpulist_attach),

	/* Bus interface */
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
	DEVMETHOD(bus_add_child,	bus_generic_add_child),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_child_pnpinfo_str, ofw_bus_gen_child_pnpinfo_str),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_devinfo,	ofw_cpulist_get_devinfo),
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	{ 0, 0 }
};

static driver_t ofw_cpulist_driver = {
	"cpulist",
	ofw_cpulist_methods,
	0
};

static devclass_t ofw_cpulist_devclass;

DRIVER_MODULE(ofw_cpulist, nexus, ofw_cpulist_driver, ofw_cpulist_devclass,
    0, 0);

static int 
ofw_cpulist_probe(device_t dev) 
{
	const char	*name;

	name = ofw_bus_get_name(dev);

	if (name == NULL || strcmp(name, "cpus") != 0)
		return (ENXIO);

	device_set_desc(dev, "Open Firmware CPU Group");

	return (0);
}

static int 
ofw_cpulist_attach(device_t dev) 
{
	phandle_t root, child;
	device_t cdev;
	struct ofw_bus_devinfo *dinfo;

	root = ofw_bus_get_node(dev);

	for (child = OF_child(root); child != 0; child = OF_peer(child)) {
		dinfo = malloc(sizeof(*dinfo), M_OFWCPU, M_WAITOK | M_ZERO);

                if (ofw_bus_gen_setup_devinfo(dinfo, child) != 0) {
                        free(dinfo, M_OFWCPU);
                        continue;
                }
                cdev = device_add_child(dev, NULL, -1);
                if (cdev == NULL) {
                        device_printf(dev, "<%s>: device_add_child failed\n",
                            dinfo->obd_name);
                        ofw_bus_gen_destroy_devinfo(dinfo);
                        free(dinfo, M_OFWCPU);
                        continue;
                }
		device_set_ivars(cdev, dinfo);
	}

	return (bus_generic_attach(dev));
}

static const struct ofw_bus_devinfo *
ofw_cpulist_get_devinfo(device_t dev, device_t child) 
{
	return (device_get_ivars(child));	
}

static int	ofw_cpu_probe(device_t);
static int	ofw_cpu_attach(device_t);
static int	ofw_cpu_read_ivar(device_t dev, device_t child, int index,
    uintptr_t *result);

static device_method_t ofw_cpu_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ofw_cpu_probe),
	DEVMETHOD(device_attach,	ofw_cpu_attach),

	/* Bus interface */
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
	DEVMETHOD(bus_add_child,	bus_generic_add_child),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	ofw_cpu_read_ivar),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource,bus_generic_activate_resource),

	{ 0, 0 }
};

static driver_t ofw_cpu_driver = {
	"cpu",
	ofw_cpu_methods,
	0
};

static devclass_t ofw_cpu_devclass;

DRIVER_MODULE(ofw_cpu, cpulist, ofw_cpu_driver, ofw_cpu_devclass, 0, 0);

static int
ofw_cpu_probe(device_t dev)
{
	const char *type = ofw_bus_get_type(dev);

	if (strcmp(type, "cpu") != 0)
		return (ENXIO);

	device_set_desc(dev, "Open Firmware CPU");
	return (0);
}

static int
ofw_cpu_attach(device_t dev)
{
	bus_generic_probe(dev);
	return (bus_generic_attach(dev));
}

static int
ofw_cpu_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{
	uint32_t cell;

	switch (index) {
	case CPU_IVAR_PCPU:
		OF_getprop(ofw_bus_get_node(dev), "reg", &cell, sizeof(cell));
		*result = (uintptr_t)(pcpu_find(cell));
		return (0);
	case CPU_IVAR_NOMINAL_MHZ:
		cell = 0;
		OF_getprop(ofw_bus_get_node(dev), "clock-frequency",
		    &cell, sizeof(cell));
		cell /= 1000000; /* convert to MHz */
		*result = (uintptr_t)(cell);
		return (0);
	}

	return (ENOENT);
}

