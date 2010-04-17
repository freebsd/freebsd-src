/*-
 * Copyright (c) 2010 Marcel Moolenaar
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
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/pcpu.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>

#include "pcib_if.h"

#include <machine/pci_cfgreg.h>
#include <machine/sal.h>
#include <machine/sgisn.h>

static struct sgisn_hub sgisn_hub;

struct sgisn_pcib_softc {
	device_t	sc_dev;
	void		*sc_promaddr;
	u_int		sc_domain;
	u_int		sc_busnr;
};

static int sgisn_pcib_attach(device_t);
static void sgisn_pcib_identify(driver_t *, device_t);
static int sgisn_pcib_probe(device_t);

static int sgisn_pcib_read_ivar(device_t, device_t, int, uintptr_t *);
static int sgisn_pcib_write_ivar(device_t, device_t, int, uintptr_t);

static int sgisn_pcib_maxslots(device_t);
static uint32_t sgisn_pcib_cfgread(device_t, u_int, u_int, u_int, u_int, int);
static void sgisn_pcib_cfgwrite(device_t, u_int, u_int, u_int, u_int, uint32_t,
    int);

/*
 * Bus interface definitions.
 */
static device_method_t sgisn_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	sgisn_pcib_identify),
	DEVMETHOD(device_probe,		sgisn_pcib_probe),
	DEVMETHOD(device_attach,	sgisn_pcib_attach),

	/* Bus interface */
        DEVMETHOD(bus_read_ivar,	sgisn_pcib_read_ivar),
        DEVMETHOD(bus_write_ivar,	sgisn_pcib_write_ivar),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	sgisn_pcib_maxslots),
	DEVMETHOD(pcib_read_config,	sgisn_pcib_cfgread),
	DEVMETHOD(pcib_write_config,	sgisn_pcib_cfgwrite),
	DEVMETHOD(pcib_route_interrupt,	pcib_route_interrupt),

	{ 0, 0 }
};

static driver_t sgisn_pcib_driver = {
	"pcib",
	sgisn_pcib_methods,
	sizeof(struct sgisn_pcib_softc),
};

devclass_t pcib_devclass;

DRIVER_MODULE(pcib, nexus, sgisn_pcib_driver, pcib_devclass, 0, 0);

static int
sgisn_pcib_maxslots(device_t dev)
{

	return (31);
}

static uint32_t
sgisn_pcib_cfgread(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, int bytes)
{
	struct sgisn_pcib_softc *sc;
	uint32_t val;

	sc = device_get_softc(dev);

	val = pci_cfgregread((sc->sc_domain << 8) | bus, slot, func, reg,
	    bytes);
	return (val);
}

static void
sgisn_pcib_cfgwrite(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, uint32_t val, int bytes)
{
	struct sgisn_pcib_softc *sc;

	sc = device_get_softc(dev);

	pci_cfgregwrite((sc->sc_domain << 8) | bus, slot, func, reg, val,
	    bytes);
}

static void
sgisn_pcib_identify(driver_t *drv, device_t bus)
{
	struct ia64_sal_result r;
	device_t dev;
	struct sgisn_pcib_softc *sc;
	void *addr;
	u_int busno, segno;

	sgisn_hub.hub_pci_maxseg = 0xffffffff;
	sgisn_hub.hub_pci_maxbus = 0xff;
	r = ia64_sal_entry(SAL_SGISN_IOHUB_INFO, PCPU_GET(md.sgisn_nasid),
	    ia64_tpa((uintptr_t)&sgisn_hub), 0, 0, 0, 0, 0);
	if (r.sal_status != 0)
		return;

	for (segno = 0; segno <= sgisn_hub.hub_pci_maxseg; segno++) {
		for (busno = 0; busno <= sgisn_hub.hub_pci_maxbus; busno++) {
			r = ia64_sal_entry(SAL_SGISN_IOBUS_INFO, segno, busno,
			    ia64_tpa((uintptr_t)&addr), 0, 0, 0, 0);

			if (r.sal_status == 0 && addr != NULL) {
				dev = BUS_ADD_CHILD(bus, 0, drv->name, -1);
				if (dev == NULL)
					continue;
				device_set_driver(dev, drv);
				sc = device_get_softc(dev);
				sc->sc_promaddr = addr;
				sc->sc_domain = segno;
				sc->sc_busnr = busno;
			}
		}
	}
}

static int
sgisn_pcib_probe(device_t dev)
{

	device_set_desc(dev, "SGI PCI-X host controller");
	return (BUS_PROBE_DEFAULT);
}

static int
sgisn_pcib_attach(device_t dev)
{
	struct sgisn_pcib_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	device_add_child(dev, "pci", -1);
	return (bus_generic_attach(dev));
}

static int
sgisn_pcib_read_ivar(device_t dev, device_t child, int which, uintptr_t *res)
{
	struct sgisn_pcib_softc *sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_BUS:
		*res = sc->sc_busnr;
		return (0);
	case PCIB_IVAR_DOMAIN:
		*res = sc->sc_domain;
		return (0);
	}
	return (ENOENT);
}

static int
sgisn_pcib_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct sgisn_pcib_softc *sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_BUS:
		sc->sc_busnr = value;
		return (0);
	}
	return (ENOENT);
}
