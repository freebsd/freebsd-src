/*	$FreeBSD$ */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@carlstedt.se) at
 * Carlstedt Research & Technology.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * USB Enhanced Host Controller Driver, a.k.a. USB 2.0 controller.
 *
 * The EHCI 1.0 spec can be found at
 * http://developer.intel.com/technology/usb/download/ehci-r10.pdf
 * and the USB 2.0 spec at
 * http://www.usb.org/developers/docs/usb_20.zip
 *
 */

/* The low level controller code for EHCI has been split into
 * PCI probes and EHCI specific code. This was done to facilitate the
 * sharing of code between *BSD's
 */


#include "opt_bus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/queue.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/ehcireg.h>
#include <dev/usb/ehcivar.h>

#define PCI_EHCI_VENDORID_ACERLABS	0x10b9
#define PCI_EHCI_VENDORID_AMD		0x1022
#define PCI_EHCI_VENDORID_APPLE		0x106b
#define PCI_EHCI_VENDORID_CMDTECH	0x1095
#define PCI_EHCI_VENDORID_NEC		0x1033
#define PCI_EHCI_VENDORID_OPTI		0x1045
#define PCI_EHCI_VENDORID_SIS		0x1039

#define PCI_EHCI_DEVICEID_NEC		0x00e01033
static const char *ehci_device_nec = "NEC uPD 720100 USB 2.0 controller";

static const char *ehci_device_generic = "EHCI (generic) USB 2.0 controller";

#define PCI_EHCI_BASE_REG	0x10


static int ehci_pci_attach(device_t self);
static int ehci_pci_detach(device_t self);

static const char *
ehci_pci_match(device_t self)
{
	u_int32_t device_id = pci_get_devid(self);

	switch (device_id) {
	case PCI_EHCI_DEVICEID_NEC:
		return (ehci_device_nec);
	default:
		if (pci_get_class(self) == PCIC_SERIALBUS
		    && pci_get_subclass(self) == PCIS_SERIALBUS_USB
		    && pci_get_progif(self) == PCI_INTERFACE_EHCI) {
			return (ehci_device_generic);
		}
	}

	return NULL;		/* dunno */
}

static int
ehci_pci_probe(device_t self)
{
	const char *desc = ehci_pci_match(self);

	if (desc) {
		device_set_desc(self, desc);
		return 0;
	} else {
		return ENXIO;
	}
}

static int
ehci_pci_attach(device_t self)
{
	ehci_softc_t *sc = device_get_softc(self);
	device_t parent;
	device_t *neighbors;
	device_t *nbus;
	struct usbd_bus *bsc;
	int err;
	int rid;
	int ncomp;
	int count, buscount;
	int slot, function;
	int res;
	int i;

	switch(pci_read_config(self, PCI_USBREV, 1) & PCI_USBREV_MASK) {
	case PCI_USBREV_PRE_1_0:
	case PCI_USBREV_1_0:
	case PCI_USBREV_1_1:
		sc->sc_bus.usbrev = USBREV_UNKNOWN;
		printf("pre-2.0 USB rev\n");
		return ENXIO;
	case PCI_USBREV_2_0:
		sc->sc_bus.usbrev = USBREV_2_0;
		break;
	default:
		sc->sc_bus.usbrev = USBREV_UNKNOWN;
		break;
	}

	pci_enable_busmaster(self);

	rid = PCI_CBMEM;
	sc->io_res = bus_alloc_resource(self, SYS_RES_MEMORY, &rid,
	    0, ~0, 1, RF_ACTIVE);
	if (!sc->io_res) {
		device_printf(self, "Could not map memory\n");
		return ENXIO;
	}
	sc->iot = rman_get_bustag(sc->io_res);
	sc->ioh = rman_get_bushandle(sc->io_res);

	rid = 0;
	sc->irq_res = bus_alloc_resource(self, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(self, "Could not allocate irq\n");
		ehci_pci_detach(self);
		return ENXIO;
	}
	sc->sc_bus.bdev = device_add_child(self, "usb", -1);
	if (!sc->sc_bus.bdev) {
		device_printf(self, "Could not add USB device\n");
		ehci_pci_detach(self);
		return ENOMEM;
	}
	device_set_ivars(sc->sc_bus.bdev, sc);

	/* ehci_pci_match will never return NULL if ehci_pci_probe succeeded */
	device_set_desc(sc->sc_bus.bdev, ehci_pci_match(self));
	switch (pci_get_vendor(self)) {
	case PCI_EHCI_VENDORID_ACERLABS:
		sprintf(sc->sc_vendor, "AcerLabs");
		break;
	case PCI_EHCI_VENDORID_AMD:
		sprintf(sc->sc_vendor, "AMD");
		break;
	case PCI_EHCI_VENDORID_APPLE:
		sprintf(sc->sc_vendor, "Apple");
		break;
	case PCI_EHCI_VENDORID_CMDTECH:
		sprintf(sc->sc_vendor, "CMDTECH");
		break;
	case PCI_EHCI_VENDORID_NEC:
		sprintf(sc->sc_vendor, "NEC");
		break;
	case PCI_EHCI_VENDORID_OPTI:
		sprintf(sc->sc_vendor, "OPTi");
		break;
	case PCI_EHCI_VENDORID_SIS:
		sprintf(sc->sc_vendor, "SiS");
		break;
	default:
		if (bootverbose)
			device_printf(self, "(New EHCI DeviceId=0x%08x)\n",
			    pci_get_devid(self));
		sprintf(sc->sc_vendor, "(0x%04x)", pci_get_vendor(self));
	}

	err = bus_setup_intr(self, sc->irq_res, INTR_TYPE_BIO,
	    (driver_intr_t *) ehci_intr, sc, &sc->ih);
	if (err) {
		device_printf(self, "Could not setup irq, %d\n", err);
		sc->ih = NULL;
		ehci_pci_detach(self);
		return ENXIO;
	}

	/*
	 * Find companion controllers.  According to the spec they always
	 * have lower function numbers so they should be enumerated already.
	 */
	parent = device_get_parent(self);
	res = device_get_children(parent, &neighbors, &count);
	if (res != 0) {
		device_printf(self, "Error finding companion busses\n");
		ehci_pci_detach(self);
		return ENXIO;
	}
	ncomp = 0;
	slot = pci_get_slot(self);
	function = pci_get_function(self);
	for (i = 0; i < count; i++) {
		if (pci_get_slot(neighbors[i]) == slot && \
			pci_get_function(neighbors[i]) < function) {
			res = device_get_children(neighbors[i],
				&nbus, &buscount);
			if (res != 0 || buscount != 1)
				continue;
			bsc = device_get_softc(nbus[0]);
			printf("ehci_pci_attach: companion %s\n",
			USBDEVNAME(bsc->bdev));
			sc->sc_comps[ncomp++] = bsc;  
			if (ncomp >= EHCI_COMPANION_MAX)
				break;
		}
	}
	sc->sc_ncomp = ncomp;

	err = ehci_init(sc);
	if (!err)
		err = device_probe_and_attach(sc->sc_bus.bdev);

	if (err) {
		device_printf(self, "USB init failed err=%d\n", err);
#if 0 /* TODO */
		ehci_pci_detach(self);
#endif
		return EIO;
	}
	return 0;
}

static int
ehci_pci_detach(device_t self)
{
	ehci_softc_t *sc = device_get_softc(self);

	/*
	 * XXX this code is not yet fit to be used as detach for the EHCI
	 * controller
	 */

	/*
	 * disable interrupts that might have been switched on in ehci_init
	 */
	if (sc->iot && sc->ioh)
		bus_space_write_4(sc->iot, sc->ioh, EHCI_USBINTR, 0);

	if (sc->irq_res && sc->ih) {
		int err = bus_teardown_intr(self, sc->irq_res, sc->ih);

		if (err)
			/* XXX or should we panic? */
			device_printf(self, "Could not tear down irq, %d\n",
			    err);
		sc->ih = NULL;
	}
	if (sc->sc_bus.bdev) {
		device_delete_child(self, sc->sc_bus.bdev);
		sc->sc_bus.bdev = NULL;
	}
	if (sc->irq_res) {
		bus_release_resource(self, SYS_RES_IRQ, 0, sc->irq_res);
		sc->irq_res = NULL;
	}
	if (sc->io_res) {
		bus_release_resource(self, SYS_RES_MEMORY, PCI_CBMEM, sc->io_res);
		sc->io_res = NULL;
		sc->iot = 0;
		sc->ioh = 0;
	}
	return 0;
}

static device_method_t ehci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ehci_pci_probe),
	DEVMETHOD(device_attach, ehci_pci_attach),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child, bus_generic_print_child),

	{0, 0}
};

static driver_t ehci_driver = {
	"ehci",
	ehci_methods,
	sizeof(ehci_softc_t),
};

static devclass_t ehci_devclass;

DRIVER_MODULE(ehci, pci, ehci_driver, ehci_devclass, 0, 0);
DRIVER_MODULE(ehci, cardbus, ehci_driver, ehci_devclass, 0, 0);
