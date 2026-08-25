/*-
 * Copyright (c) 2011-2012 Semihalf.
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

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/smp.h>

#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_subr.h>

#include "bman.h"
#include "bman_var.h"
#include "portals.h"

#define	FBMAN_DEVSTR	"Freescale Buffer Manager"

static int bman_fdt_probe(device_t);

static device_method_t bman_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bman_fdt_probe),
	DEVMETHOD(device_attach,	bman_attach),
	DEVMETHOD(device_detach,	bman_detach),

	DEVMETHOD(device_suspend,	bman_suspend),
	DEVMETHOD(device_resume,	bman_resume),
	DEVMETHOD(device_shutdown,	bman_shutdown),

	DEVMETHOD_END
};

DEFINE_CLASS_0(bman, bman_driver, bman_methods, sizeof(struct bman_softc));
EARLY_DRIVER_MODULE(bman, simplebus, bman_driver, 0, 0, BUS_PASS_SUPPORTDEV);

static int
bman_fdt_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "fsl,bman"))
		return (ENXIO);

	device_set_desc(dev, FBMAN_DEVSTR);

	return (BUS_PROBE_DEFAULT);
}

/*
 * BMAN Portals
 */
#define	BMAN_PORT_DEVSTR	"Freescale Buffer Manager - Portal"

static int portal_ncpus;
static device_probe_t bman_portal_fdt_probe;
static device_attach_t bman_portal_fdt_attach;

static device_method_t bman_portal_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		bman_portal_fdt_probe),
	DEVMETHOD(device_attach,	bman_portal_fdt_attach),
	DEVMETHOD(device_detach,	bman_portal_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_0(bman_portal, bman_portal_driver, bman_portal_methods,
    sizeof(struct bman_portal_softc));
EARLY_DRIVER_MODULE(bman_portal, simplebus, bman_portal_driver, 0, 0,
    BUS_PASS_SUPPORTDEV + BUS_PASS_ORDER_MIDDLE);

static int
bman_portal_fdt_probe(device_t dev)
{
	if (!ofw_bus_is_compatible(dev, "fsl,bman-portal"))
		return (ENXIO);

	device_set_desc(dev, BMAN_PORT_DEVSTR);
	return (BUS_PROBE_DEFAULT);
}

static int
bman_portal_fdt_attach(device_t dev)
{
	int portal_cpu = portal_ncpus;

	/* Don't attach to more portals than we have CPUs */
	if (mp_ncpus == portal_ncpus)
		return (ENXIO);

	portal_ncpus++;

	return (bman_portal_attach(dev, portal_cpu));
}
