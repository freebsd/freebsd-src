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
#include <sys/mutex.h>
#include <sys/proc.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <pci/agppriv.h>
#include <pci/agpreg.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/pmap.h>

struct agp_intel_softc {
	struct agp_softc agp;
	u_int32_t	initial_aperture; /* aperture size at startup */
	struct agp_gatt *gatt;
};

static const char*
agp_intel_match(device_t dev)
{
	if (pci_get_class(dev) != PCIC_BRIDGE
	    || pci_get_subclass(dev) != PCIS_BRIDGE_HOST)
		return NULL;

	if (agp_find_caps(dev) == 0)
		return NULL;

	switch (pci_get_devid(dev)) {
	/* Intel -- vendor 0x8086 */
	case 0x71808086:
		return ("Intel 82443LX (440 LX) host to PCI bridge");

	case 0x71908086:
		return ("Intel 82443BX (440 BX) host to PCI bridge");

 	case 0x71a08086:
 		return ("Intel 82443GX host to PCI bridge");

 	case 0x71a18086:
 		return ("Intel 82443GX host to AGP bridge");

	case 0x11308086:
		return ("Intel 82815 (i815 GMCH) host to PCI bridge");

	case 0x25008086:
		return ("Intel 82820 host to AGP bridge");

	case 0x1a218086:
		return ("Intel 82840 host to AGP bridge");

	case 0x1a308086:
		return ("Intel 82845 host to AGP bridge");

	case 0x25308086:
		return ("Intel 82850 host to AGP bridge");

	case 0x25318086:
		return ("Intel 82860 host to AGP bridge");
	};

	if (pci_get_vendor(dev) == 0x8086)
		return ("Intel Generic host to PCI bridge");

	return NULL;
}

static int
agp_intel_probe(device_t dev)
{
	const char *desc;

	desc = agp_intel_match(dev);
	if (desc) {
		device_verbose(dev);
		device_set_desc(dev, desc);
		return 0;
	}

	return ENXIO;
}

static int
agp_intel_attach(device_t dev)
{
	struct agp_intel_softc *sc = device_get_softc(dev);
	struct agp_gatt *gatt;
	u_int32_t type = pci_get_devid(dev);
	int error;

	error = agp_generic_attach(dev);
	if (error)
		return error;

	sc->initial_aperture = AGP_GET_APERTURE(dev);

	for (;;) {
		gatt = agp_alloc_gatt(dev);
		if (gatt)
			break;

		/*
		 * Probably contigmalloc failure. Try reducing the
		 * aperture so that the gatt size reduces.
		 */
		if (AGP_SET_APERTURE(dev, AGP_GET_APERTURE(dev) / 2)) {
			agp_generic_detach(dev);
			return ENOMEM;
		}
	}
	sc->gatt = gatt;

	/* Install the gatt. */
	pci_write_config(dev, AGP_INTEL_ATTBASE, gatt->ag_physical, 4);
	
	/* Enable things, clear errors etc. */
	switch (type) {
	case 0x1a218086: /* i840 */
	case 0x25308086: /* i850 */
	case 0x25318086: /* i860 */
		pci_write_config(dev, AGP_INTEL_AGPCTRL, 0x0000, 4);
		pci_write_config(dev, AGP_INTEL_MCHCFG,
				 (pci_read_config(dev, AGP_INTEL_MCHCFG, 2)
				  | (1 << 9)), 2);
		break;

	case 0x25008086: /* i820 */
		pci_write_config(dev, AGP_INTEL_AGPCTRL, 0x0000, 4);
		pci_write_config(dev, AGP_INTEL_I820_RDCR,
				 (pci_read_config(dev, AGP_INTEL_I820_RDCR, 1)
				  | (1 << 1)), 1);
		break;

	case 0x1a308086: /* i845 */
		pci_write_config(dev, AGP_INTEL_AGPCTRL, 0x0000, 4);
		pci_write_config(dev, AGP_INTEL_I845_MCHCFG,
				 (pci_read_config(dev, AGP_INTEL_I845_MCHCFG, 1)
				  | (1 << 1)), 1);
		break;

	default: /* Intel Generic (maybe) */
		pci_write_config(dev, AGP_INTEL_AGPCTRL, 0x2280, 4);
		pci_write_config(dev, AGP_INTEL_NBXCFG,
				 (pci_read_config(dev, AGP_INTEL_NBXCFG, 4)
				  & ~(1 << 10)) | (1 << 9), 4);
	}

	switch (type) {
	case 0x1a218086: /* i840 */
		pci_write_config(dev, AGP_INTEL_I8XX_ERRSTS, 0xc000, 2);
		break;

	case 0x25008086: /* i820 */
	case 0x1a308086: /* i845 */
	case 0x25308086: /* i850 */
	case 0x25318086: /* i860 */
		pci_write_config(dev, AGP_INTEL_I8XX_ERRSTS, 0x001c, 2);
		break;

	default: /* Intel Generic (maybe) */
		pci_write_config(dev, AGP_INTEL_ERRSTS + 1, 7, 1);
	}

	return 0;
}

static int
agp_intel_detach(device_t dev)
{
	struct agp_intel_softc *sc = device_get_softc(dev);
	u_int32_t type = pci_get_devid(dev);
	int error;

	error = agp_generic_detach(dev);
	if (error)
		return error;

	switch (type) {
	case 0x1a218086: /* i840 */
	case 0x25308086: /* i850 */
	case 0x25318086: /* i860 */
		printf("%s: set MCHCFG to %x\n", __FUNCTION__, (unsigned)
				(pci_read_config(dev, AGP_INTEL_MCHCFG, 2)
				& ~(1 << 9)));
		pci_write_config(dev, AGP_INTEL_MCHCFG,
				(pci_read_config(dev, AGP_INTEL_MCHCFG, 2)
				& ~(1 << 9)), 2);

	case 0x25008086: /* i820 */
		printf("%s: set RDCR to %x\n", __FUNCTION__, (unsigned)
				(pci_read_config(dev, AGP_INTEL_I820_RDCR, 1)
				& ~(1 << 1)));
		pci_write_config(dev, AGP_INTEL_I820_RDCR,
				(pci_read_config(dev, AGP_INTEL_I820_RDCR, 1)
				& ~(1 << 1)), 1);

	case 0x1a308086: /* i845 */
		printf("%s: set MCHCFG to %x\n", __FUNCTION__, (unsigned)
				(pci_read_config(dev, AGP_INTEL_I845_MCHCFG, 1)
				& ~(1 << 1)));
		pci_write_config(dev, AGP_INTEL_MCHCFG,
				(pci_read_config(dev, AGP_INTEL_I845_MCHCFG, 1)
				& ~(1 << 1)), 1);

	default: /* Intel Generic (maybe) */
		printf("%s: set NBXCFG to %x\n", __FUNCTION__,
				 (pci_read_config(dev, AGP_INTEL_NBXCFG, 4)
				  & ~(1 << 9)));
		pci_write_config(dev, AGP_INTEL_NBXCFG,
				 (pci_read_config(dev, AGP_INTEL_NBXCFG, 4)
				  & ~(1 << 9)), 4);
	}
	pci_write_config(dev, AGP_INTEL_ATTBASE, 0, 4);
	AGP_SET_APERTURE(dev, sc->initial_aperture);
	agp_free_gatt(sc->gatt);

	return 0;
}

static u_int32_t
agp_intel_get_aperture(device_t dev)
{
	u_int32_t apsize;

	apsize = pci_read_config(dev, AGP_INTEL_APSIZE, 1) & 0x1f;

	/*
	 * The size is determined by the number of low bits of
	 * register APBASE which are forced to zero. The low 22 bits
	 * are always forced to zero and each zero bit in the apsize
	 * field just read forces the corresponding bit in the 27:22
	 * to be zero. We calculate the aperture size accordingly.
	 */
	return (((apsize ^ 0x1f) << 22) | ((1 << 22) - 1)) + 1;
}

static int
agp_intel_set_aperture(device_t dev, u_int32_t aperture)
{
	u_int32_t apsize;

	/*
	 * Reverse the magic from get_aperture.
	 */
	apsize = ((aperture - 1) >> 22) ^ 0x1f;

	/*
	 * Double check for sanity.
	 */
	if ((((apsize ^ 0x1f) << 22) | ((1 << 22) - 1)) + 1 != aperture)
		return EINVAL;

	pci_write_config(dev, AGP_INTEL_APSIZE, apsize, 1);

	return 0;
}

static int
agp_intel_bind_page(device_t dev, int offset, vm_offset_t physical)
{
	struct agp_intel_softc *sc = device_get_softc(dev);

	if (offset < 0 || offset >= (sc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return EINVAL;

	sc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] = physical | 0x17;
	return 0;
}

static int
agp_intel_unbind_page(device_t dev, int offset)
{
	struct agp_intel_softc *sc = device_get_softc(dev);

	if (offset < 0 || offset >= (sc->gatt->ag_entries << AGP_PAGE_SHIFT))
		return EINVAL;

	sc->gatt->ag_virtual[offset >> AGP_PAGE_SHIFT] = 0;
	return 0;
}

static void
agp_intel_flush_tlb(device_t dev)
{
	pci_write_config(dev, AGP_INTEL_AGPCTRL, 0x2200, 4);
	pci_write_config(dev, AGP_INTEL_AGPCTRL, 0x2280, 4);
}

static device_method_t agp_intel_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		agp_intel_probe),
	DEVMETHOD(device_attach,	agp_intel_attach),
	DEVMETHOD(device_detach,	agp_intel_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* AGP interface */
	DEVMETHOD(agp_get_aperture,	agp_intel_get_aperture),
	DEVMETHOD(agp_set_aperture,	agp_intel_set_aperture),
	DEVMETHOD(agp_bind_page,	agp_intel_bind_page),
	DEVMETHOD(agp_unbind_page,	agp_intel_unbind_page),
	DEVMETHOD(agp_flush_tlb,	agp_intel_flush_tlb),
	DEVMETHOD(agp_enable,		agp_generic_enable),
	DEVMETHOD(agp_alloc_memory,	agp_generic_alloc_memory),
	DEVMETHOD(agp_free_memory,	agp_generic_free_memory),
	DEVMETHOD(agp_bind_memory,	agp_generic_bind_memory),
	DEVMETHOD(agp_unbind_memory,	agp_generic_unbind_memory),

	{ 0, 0 }
};

static driver_t agp_intel_driver = {
	"agp",
	agp_intel_methods,
	sizeof(struct agp_intel_softc),
};

static devclass_t agp_devclass;

DRIVER_MODULE(agp_intel, pci, agp_intel_driver, agp_devclass, 0, 0);
