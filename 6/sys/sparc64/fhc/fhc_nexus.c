/*-
 * Copyright (c) 2003 Jake Burkholder.
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/ofw_upa.h>
#include <machine/nexusvar.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <sparc64/fhc/fhcreg.h>
#include <sparc64/fhc/fhcvar.h>

static device_probe_t fhc_nexus_probe;
static device_attach_t fhc_nexus_attach;

static device_method_t fhc_nexus_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,		fhc_nexus_probe),
	DEVMETHOD(device_attach,	fhc_nexus_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface. */
	DEVMETHOD(bus_print_child,	fhc_print_child),
	DEVMETHOD(bus_probe_nomatch,	fhc_probe_nomatch),
	DEVMETHOD(bus_setup_intr,	fhc_setup_intr),
	DEVMETHOD(bus_teardown_intr,	fhc_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	fhc_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_rl_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_get_resource_list, fhc_get_resource_list),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),

        /* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_compat,	fhc_get_compat),
	DEVMETHOD(ofw_bus_get_model,	fhc_get_model),
	DEVMETHOD(ofw_bus_get_name,	fhc_get_name),
	DEVMETHOD(ofw_bus_get_node,	fhc_get_node),
	DEVMETHOD(ofw_bus_get_type,	fhc_get_type),

	{ NULL, NULL }
};

static driver_t fhc_nexus_driver = {
	"fhc",
	fhc_nexus_methods,
	sizeof(struct fhc_softc),
};

static devclass_t fhc_nexus_devclass;

DRIVER_MODULE(fhc, nexus, fhc_nexus_driver, fhc_nexus_devclass, 0, 0);

static int
fhc_nexus_probe(device_t dev)
{

	if (strcmp(nexus_get_name(dev), "fhc") == 0) {
		device_set_desc(dev, "fhc");
		return (fhc_probe(dev));
	}
	return (ENXIO);
}

static int
fhc_nexus_attach(device_t dev)
{
	struct fhc_softc *sc;
	struct upa_regs *reg;
	bus_addr_t phys;
	bus_addr_t size;
	phandle_t node;
	int nreg;
	int rid;
	int i;

	sc = device_get_softc(dev);
	node = nexus_get_node(dev);
	sc->sc_node = node;

	reg = nexus_get_reg(dev);
	nreg = nexus_get_nreg(dev);
	if (nreg != FHC_NREG) {
		device_printf(dev, "wrong number of regs\n");
		return (ENXIO);
	}
	for (i = 0; i < nreg; i++) {
		phys = UPA_REG_PHYS(reg + i);
		size = UPA_REG_SIZE(reg + i);
		rid = 0;
		sc->sc_memres[i] = bus_alloc_resource(dev, SYS_RES_MEMORY,
		    &rid, phys, phys + size - 1, size, RF_ACTIVE);
		if (sc->sc_memres[i] == NULL)
			panic("%s: can't allocate registers", __func__);
		sc->sc_bt[i] = rman_get_bustag(sc->sc_memres[i]);
		sc->sc_bh[i] = rman_get_bushandle(sc->sc_memres[i]);
	}

	if (OF_getprop(node, "board#", &sc->sc_board,
	    sizeof(sc->sc_board)) == -1) {
		device_printf(dev, "could not get board number\n");
		return (ENXIO);
	}

	return (fhc_attach(dev));
}
