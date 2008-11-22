/*-
 * Copyright 2003 by Peter Grehan. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/*
 * The macio attachment for the OpenPIC interrupt controller.
 * A nexus driver is defined so the number of interrupts can be
 * determined early in the boot sequence before the hardware
 * is accessed - the interrupt i/f is installed at this time,
 * and when h/w is finally accessed, interrupt sources allocated
 * prior to this are activated
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/nexusvar.h>
#include <machine/pio.h>
#include <machine/resource.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <sys/rman.h>

#include <machine/openpicvar.h>

#include "pic_if.h"

struct openpic_ofw_softc {
	struct openpic_softc osc;
	struct resource *sc_memr;	/* macio bus resource */
	device_t	sc_ndev;	/* nexus device */
};

static struct openpic_ofw_softc *ofwpicsoftc;

/*
 * MacIO interface
 */
static void	openpic_ofw_identify(driver_t *, device_t);
static int	openpic_ofw_probe(device_t);
static int	openpic_ofw_attach(device_t);
static int	openpic_macio_probe(device_t);
static int	openpic_macio_attach(device_t);

/*
 * Nexus attachment
 */
static device_method_t  openpic_ofw_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	openpic_ofw_identify),
	DEVMETHOD(device_probe,		openpic_ofw_probe),
	DEVMETHOD(device_attach,	openpic_ofw_attach),

	/* PIC interface */
	DEVMETHOD(pic_allocate_intr,	openpic_allocate_intr),
	DEVMETHOD(pic_setup_intr,	openpic_setup_intr),
	DEVMETHOD(pic_teardown_intr,	openpic_teardown_intr),
	DEVMETHOD(pic_release_intr,	openpic_release_intr),

	{ 0, 0 }
};

static driver_t openpic_ofw_driver = {
	"openpic",
	openpic_ofw_methods,
	sizeof(struct openpic_ofw_softc)
};

static devclass_t openpic_ofw_devclass;

DRIVER_MODULE(openpic_ofw, nexus, openpic_ofw_driver, openpic_ofw_devclass,
    0, 0);

static void
openpic_ofw_identify(driver_t *driver, device_t parent)
{
	device_t child;
	phandle_t pic;
	char type[40];

	pic = OF_finddevice("mpic");
	if (pic == -1)
		return;

	OF_getprop(pic, "device_type", type, sizeof(type));

	if (strcmp(type, "open-pic") != 0)
		return;

	child = BUS_ADD_CHILD(parent, 0, "openpic", 0);
	if (child != NULL)
		nexus_set_device_type(child, "macio");
}

static int
openpic_ofw_probe(device_t dev)
{
	char	*name;
	char	*type;

	name = nexus_get_name(dev);
	type = nexus_get_device_type(dev);

	if (strcmp(name, "openpic") != 0 ||
	    strcmp(type, "macio") != 0)
		return (ENXIO);

	device_set_desc(dev, OPENPIC_DEVSTR);
	return (0);
}

static int
openpic_ofw_attach(device_t dev)
{
	KASSERT(ofwpicsoftc == NULL, ("ofw openpic: already probed"));
	ofwpicsoftc = device_get_softc(dev);
	ofwpicsoftc->sc_ndev = dev;

	nexus_install_intcntlr(dev);
	openpic_early_attach(dev);
	return (0);
}

/*
 * MacIO attachment
 */
static device_method_t  openpic_macio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		openpic_macio_probe),
	DEVMETHOD(device_attach,	openpic_macio_attach),

	{ 0, 0 },
};

static driver_t openpic_macio_driver = {
	"openpicmacio",
	openpic_macio_methods,
	0
};

static devclass_t openpic_macio_devclass;

DRIVER_MODULE(openpicmacio, macio, openpic_macio_driver,
    openpic_macio_devclass, 0, 0);

static int
openpic_macio_probe(device_t dev)
{
	const char *type = ofw_bus_get_type(dev);

	if (strcmp(type, "open-pic") != 0)
                return (ENXIO);

	/*
	 * The description was already printed out in the nexus
	 * probe, so don't do it again here
	 */
	device_set_desc(dev, "OpenPIC MacIO interrupt cell");
	if (!bootverbose)
		device_quiet(dev);
	return (0);
}

static int
openpic_macio_attach(device_t dev)
{
	struct openpic_ofw_softc *sc;
	int rid;

	sc = ofwpicsoftc;
	KASSERT(sc != NULL, ("pic not nexus-probed\n"));

	rid = 0;
	sc->sc_memr = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	     RF_ACTIVE);

	if (sc->sc_memr == NULL) {
		device_printf(dev, "Could not alloc mem resource!\n");
		return (ENXIO);
	}

	sc->osc.sc_bt = rman_get_bustag(sc->sc_memr);
	sc->osc.sc_bh = rman_get_bushandle(sc->sc_memr);
	sc->osc.sc_altdev = dev;

	return (openpic_attach(sc->sc_ndev));
}


