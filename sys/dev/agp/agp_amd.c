/*-
 * Copyright (c) 2000 Doug Rabson
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
 *
 *	$FreeBSD$
 */

#include "opt_bus.h"
#include "opt_pci.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/lock.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <pci/agppriv.h>
#include <pci/agpreg.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/pmap.h>
#include <machine/clock.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#define READ2(off)	bus_space_read_2(sc->bst, sc->bsh, off)
#define READ4(off)	bus_space_read_4(sc->bst, sc->bsh, off)
#define WRITE2(off,v)	bus_space_write_2(sc->bst, sc->bsh, off, v)
#define WRITE4(off,v)	bus_space_write_4(sc->bst, sc->bsh, off, v)

struct agp_amd_softc {
	struct agp_softc agp;
	struct resource *regs;	/* memory mapped control registers */
	bus_space_tag_t bst;	/* bus_space tag */
	bus_space_handle_t bsh;	/* bus_space handle */
	u_int32_t	initial_aperture; /* aperture size at startup */
	struct agp_gatt *gatt;
};

static const char*
agp_amd_match(device_t dev)
{
	if (pci_get_class(dev) != PCIC_BRIDGE
	    || pci_get_subclass(dev) != PCIS_BRIDGE_HOST)
		return NULL;

	if (agp_find_caps(dev) == 0)
		return NULL;

	switch (pci_get_devid(dev)) {
	case 0x70061022:
		return ("AMD 751 host to AGP bridge");
	};

	return NULL;
}

static int
agp_amd_probe(device_t dev)
{
	const char *desc;

	desc = agp_amd_match(dev);
	if (desc) {
		device_verbose(dev);
		device_set_desc(dev, desc);
		return 0;
	}

	return ENXIO;
}

static int
agp_amd_attach(device_t dev)
{
	struct agp_amd_softc *sc = device_get_softc(dev);
	struct agp_gatt *gatt;
	int error, rid;

	error = agp_generic_attach(dev);
	if (error)
		return error;

	rid = AGP_AMD751_REGISTERS;
	sc->regs = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
				      0, ~0, 1, RF_ACTIVE);
	if (!sc->regs) {
		agp_generic_detach(dev);
		return ENOMEM;
	}

	sc->bst = rman_get_bustag(sc->regs);
	sc->bsh = rman_get_bushandle(sc->regs);

	sc->initial_aperture = AGP_GET_APERTURE(dev);

	for (;;) {
		gatt = agp_alloc_gatt(dev);
		if (gatt)
			break;

		/*
		 * Probably contigmalloc failure. Try reducing the
		 * aperture so that the gatt size reduces.
		 */
		if (AGP_SET_APERTURE(dev, AGP_GET_APERTURE(dev) / 2))
			return ENOMEM;
	}
	sc->gatt = gatt;

	/* Install the gatt. */
	WRITE4(AGP_AMD751_ATTBASE, gatt->ag_physical);
	
	/* Enable synchronisation between host and agp. */
	pci_write_config(dev, AGP_AMD751_MODECTRL, 0x80, 1);

	/* Enable the TLB and flush */
	WRITE2(AGP_AMD751_STATUS,
	       READ2(AGP_AMD751_STATUS) | AGP_AMD751_STATUS_GCE);
	AGP_FLUSH_TLB(dev);

	return agp_generic_attach(dev);
}

static int
agp_amd_detach(device_t dev)
{
	struct agp_amd_softc *sc = device_get_softc(dev);

	/* Disable the TLB.. */
	WRITE2(AGP_AMD751_STATUS,
	       READ2(AGP_AMD751_STATUS) & ~AGP_AMD751_STATUS_GCE);
	
	/* Disable host-agp sync */
	pci_write_config(dev, AGP_AMD751_MODECTRL, 0x00, 1);
	
	/* Clear the GATT base */
	WRITE4(AGP_AMD751_ATTBASE, 0);

	/* Put the aperture back the way it started. */
	AGP_SET_APERTURE(dev, sc->initial_aperture);

	agp_free_gatt(sc->gatt);
	return 0;
}

static u_int32_t
agp_amd_get_aperture(device_t dev)
{
	int vas;

	/*
	 * The aperture size is equal to 32M<<vas.
	 */
	vas = (pci_read_config(dev, AGP_AMD751_APCTRL, 1) & 0x06) >> 1;
	return (32*1024*1024) << vas;
}

static int
agp_amd_set_aperture(device_t dev, u_int32_t aperture)
{
	int vas;

	/*
	 * Check for a power of two and make sure its within the
	 * programmable range.
	 */
	if (aperture & (aperture - 1)
	    || aperture < 32*1024*1024
	    || aperture > 2U*1024*1024*1024)
		return EINVAL;

	vas = ffs(aperture / 32*1024*1024) - 1;
	
	pci_write_config(dev, AGP_AMD751_APCTRL,
			 ((pci_read_config(dev, AGP_AMD751_APCTRL, 1) & ~0x06)
			  | vas << 1), 1);

	return 0;
}

static int
agp_amd_bind_page(device_t dev, int offset, vm_offset_t physical)
{
	struct agp_amd_softc *sc = device_get_softc(dev);

	if (offset < 0 || offset >= (sc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return EINVAL;

	sc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] = physical | 1;
	return 0;
}

static int
agp_amd_unbind_page(device_t dev, int offset)
{
	struct agp_amd_softc *sc = device_get_softc(dev);

	if (offset < 0 || offset >= (sc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return EINVAL;

	sc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] = 0;
	return 0;
}

static void
agp_amd_flush_tlb(device_t dev)
{
	struct agp_amd_softc *sc = device_get_softc(dev);

	/* Set the cache invalidate bit and wait for the chipset to clear */
	WRITE4(AGP_AMD751_TLBCTRL, 1);
	do {
		DELAY(1);
	} while (READ4(AGP_AMD751_TLBCTRL));
}

static device_method_t agp_amd_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		agp_amd_probe),
	DEVMETHOD(device_attach,	agp_amd_attach),
	DEVMETHOD(device_detach,	agp_amd_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* AGP interface */
	DEVMETHOD(agp_get_aperture,	agp_amd_get_aperture),
	DEVMETHOD(agp_set_aperture,	agp_amd_set_aperture),
	DEVMETHOD(agp_bind_page,	agp_amd_bind_page),
	DEVMETHOD(agp_unbind_page,	agp_amd_unbind_page),
	DEVMETHOD(agp_flush_tlb,	agp_amd_flush_tlb),
	DEVMETHOD(agp_enable,		agp_generic_enable),
	DEVMETHOD(agp_alloc_memory,	agp_generic_alloc_memory),
	DEVMETHOD(agp_free_memory,	agp_generic_free_memory),
	DEVMETHOD(agp_bind_memory,	agp_generic_bind_memory),
	DEVMETHOD(agp_unbind_memory,	agp_generic_unbind_memory),

	{ 0, 0 }
};

static driver_t agp_amd_driver = {
	"agp",
	agp_amd_methods,
	sizeof(struct agp_amd_softc),
};

static devclass_t agp_devclass;

DRIVER_MODULE(agp_amd, pci, agp_amd_driver, agp_devclass, 0, 0);
