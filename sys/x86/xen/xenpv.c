/*
 * Copyright (c) 2014 Roger Pau Monné <roger.pau@citrix.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS AS IS'' AND
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
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/pcpu.h>
#include <sys/smp.h>

#include <xen/xen-os.h>
#include <xen/gnttab.h>

static devclass_t xenpv_devclass;

static void
xenpv_identify(driver_t *driver, device_t parent)
{
	if (!xen_domain())
		return;

	/* Make sure there's only one xenpv device. */
	if (devclass_get_device(xenpv_devclass, 0))
		return;

	if (BUS_ADD_CHILD(parent, 0, "xenpv", 0) == NULL)
		panic("Unable to attach xenpv bus.");
}

static int
xenpv_probe(device_t dev)
{

	device_set_desc(dev, "Xen PV bus");
	return (BUS_PROBE_NOWILDCARD);
}

static int
xenpv_attach(device_t dev)
{
	device_t child;
	int error;

	/* Initialize grant table before any Xen specific device is attached */
	error = gnttab_init(dev);
	if (error != 0) {
		device_printf(dev, "error initializing grant table: %d\n",
		    error);
		return (error);
	}

	/*
	 * Let our child drivers identify any child devices that they
	 * can find.  Once that is done attach any devices that we
	 * found.
	 */
	bus_generic_probe(dev);
	bus_generic_attach(dev);

	if (!devclass_get_device(devclass_find("isa"), 0)) {
		child = BUS_ADD_CHILD(dev, 0, "isa", 0);
		if (child == NULL)
			panic("Failed to attach ISA bus.");
		device_probe_and_attach(child);
	}

	return (0);
}

static device_method_t xenpv_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,		xenpv_identify),
	DEVMETHOD(device_probe,			xenpv_probe),
	DEVMETHOD(device_attach,		xenpv_attach),
	DEVMETHOD(device_suspend,		bus_generic_suspend),
	DEVMETHOD(device_resume,		bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_add_child,		bus_generic_add_child),
	DEVMETHOD(bus_alloc_resource,		bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,		bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),

	DEVMETHOD_END
};

static driver_t xenpv_driver = {
	"xenpv",
	xenpv_methods,
	0,
};

DRIVER_MODULE(xenpv, nexus, xenpv_driver, xenpv_devclass, 0, 0);
