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
 * USB Open Host Controller driver.
 *
 * OHCI spec: http://www.intel.com/design/usb/ohci11d.pdf
 */

/* The low level controller code for OHCI has been split into
 * PCI probes and OHCI specific code. This was done to facilitate the
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

#include <dev/usb/ohcireg.h>
#include <dev/usb/ohcivar.h>

#define PCI_OHCI_VENDORID_ALI		0x10b9
#define PCI_OHCI_VENDORID_AMD756	0x1022
#define PCI_OHCI_VENDORID_CMDTECH	0x1095
#define PCI_OHCI_VENDORID_COMPAQ	0x0e11
#define PCI_OHCI_VENDORID_NEC		0x1033
#define PCI_OHCI_VENDORID_OPTI		0x1045
#define PCI_OHCI_VENDORID_SIS		0x1039

#define PCI_OHCI_DEVICEID_ALADDIN_V	0x523710b9
static const char *ohci_device_aladdin_v = "AcerLabs M5237 (Aladdin-V) USB controller";
#define PCI_OHCI_DEVICEID_AMD756	0x740c1022
static const char *ohci_device_amd756	 = "AMD-756 USB Controller";
#define PCI_OHCI_DEVICEID_FIRELINK	0xc8611045
static const char *ohci_device_firelink  = "OPTi 82C861 (FireLink) USB controller";
#define PCI_OHCI_DEVICEID_NEC		0x00351033
static const char *ohci_device_nec	 = "NEC uPD 9210 USB controller";
#define PCI_OHCI_DEVICEID_USB0670	0x06701095
static const char *ohci_device_usb0670	 = "CMD Tech 670 (USB0670) USB controller";
#define PCI_OHCI_DEVICEID_USB0673	0x06731095
static const char *ohci_device_usb0673	 = "CMD Tech 673 (USB0673) USB controller";
#define PCI_OHCI_DEVICEID_SIS5571	0x70011039
static const char *ohci_device_sis5571	 = "SiS 5571 USB controller";

static const char *ohci_device_generic   = "OHCI (generic) USB controller";

#define PCI_OHCI_BASE_REG	0x10


static int ohci_pci_attach(device_t self);
static int ohci_pci_detach(device_t self);

static const char *
ohci_pci_match(device_t self)
{
	u_int32_t device_id = pci_get_devid(self);

	switch(device_id) {
	case PCI_OHCI_DEVICEID_ALADDIN_V:
		return (ohci_device_aladdin_v);
	case PCI_OHCI_DEVICEID_AMD756:
		return (ohci_device_amd756);
	case PCI_OHCI_DEVICEID_USB0670:
		return (ohci_device_usb0670);
	case PCI_OHCI_DEVICEID_USB0673:
		return (ohci_device_usb0673);
	case PCI_OHCI_DEVICEID_FIRELINK:
		return (ohci_device_firelink);
	case PCI_OHCI_DEVICEID_NEC:
		return (ohci_device_nec);
	case PCI_OHCI_DEVICEID_SIS5571:
		return (ohci_device_sis5571);
	default:
		if (   pci_get_class(self)    == PCIC_SERIALBUS
		    && pci_get_subclass(self) == PCIS_SERIALBUS_USB
		    && pci_get_progif(self)   == PCI_INTERFACE_OHCI) {
			return (ohci_device_generic);
		}
	}

	return NULL;	/* dunno */
}

static int
ohci_pci_probe(device_t self)
{
	const char *desc = ohci_pci_match(self);
	if (desc) {
		device_set_desc(self, desc);
		return 0;
	} else {
		return ENXIO;
	}
}

static int
ohci_pci_attach(device_t self)
{
	ohci_softc_t *sc = device_get_softc(self);
	int err;
	int rid;
	int intr;

	/* For the moment, put in a message stating what is wrong */
	intr = pci_read_config(self, PCIR_INTLINE, 1);
	if (intr == 0 || intr == 255) {
		device_printf(self, "Invalid irq %d\n", intr);
#ifdef __i386__
		device_printf(self, "Please switch on USB support and switch PNP-OS to 'No' in BIOS\n");
#endif
		return ENXIO;
	}

	/* XXX where does it say so in the spec? */
	sc->sc_bus.usbrev = USBREV_1_0;

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
		ohci_pci_detach(self);
		return ENXIO;
	}

	sc->sc_bus.bdev = device_add_child(self, "usb", -1);
	if (!sc->sc_bus.bdev) {
		device_printf(self, "Could not add USB device\n");
		ohci_pci_detach(self);
		return ENOMEM;
	}
	device_set_ivars(sc->sc_bus.bdev, sc);

	switch (pci_get_devid(self)) {
	case PCI_OHCI_DEVICEID_ALADDIN_V:
		device_set_desc(sc->sc_bus.bdev, ohci_device_aladdin_v);
		sprintf(sc->sc_vendor, "AcerLabs");
		break;
	case PCI_OHCI_DEVICEID_AMD756:
		device_set_desc(sc->sc_bus.bdev, ohci_device_amd756);
		sprintf(sc->sc_vendor, "AMD");
		break;
	case PCI_OHCI_DEVICEID_FIRELINK:
		device_set_desc(sc->sc_bus.bdev, ohci_device_firelink);
		sprintf(sc->sc_vendor, "OPTi");
		break;
	case PCI_OHCI_DEVICEID_NEC:
		device_set_desc(sc->sc_bus.bdev, ohci_device_nec);
		sprintf(sc->sc_vendor, "NEC");
		break;
	case PCI_OHCI_DEVICEID_USB0670:
		device_set_desc(sc->sc_bus.bdev, ohci_device_usb0670);
		sprintf(sc->sc_vendor, "CMDTECH");
		break;
	case PCI_OHCI_DEVICEID_USB0673:
		device_set_desc(sc->sc_bus.bdev, ohci_device_usb0673);
		sprintf(sc->sc_vendor, "CMDTECH");
		break;
	case PCI_OHCI_DEVICEID_SIS5571:
		device_set_desc(sc->sc_bus.bdev, ohci_device_sis5571);
		sprintf(sc->sc_vendor, "SiS");
		break;
	default:
		if (bootverbose)
			device_printf(self, "(New OHCI DeviceId=0x%08x)\n",
				      pci_get_devid(self));
		device_set_desc(sc->sc_bus.bdev, ohci_device_generic);
		sprintf(sc->sc_vendor, "(unknown)");
	}

	err = bus_setup_intr(self, sc->irq_res, INTR_TYPE_BIO,
			     (driver_intr_t *) ohci_intr, sc, &sc->ih);
	if (err) {
		device_printf(self, "Could not setup irq, %d\n", err);
		ohci_pci_detach(self);
		return ENXIO;
	}

	err = ohci_init(sc);
	if (!err)
		err = device_probe_and_attach(sc->sc_bus.bdev);

	if (err) {
		device_printf(self, "USB init failed\n");
		ohci_pci_detach(self);
		return EIO;
	}

	return 0;
}

static int
ohci_pci_detach(device_t self)
{
	ohci_softc_t *sc = device_get_softc(self);

	/* XXX this code is not yet fit to be used as detach for
	 * the OHCI controller
	 */

	/* disable interrupts that might have been switched on
	 * in ohci_init
	 */
	if (sc->iot && sc->ioh)
		bus_space_write_4(sc->iot, sc->ioh,
				  OHCI_INTERRUPT_DISABLE, OHCI_ALL_INTRS);

	if (sc->irq_res) {
		int err = bus_teardown_intr(self, sc->irq_res, sc->ih);
		if (err)
			/* XXX or should we panic? */
			device_printf(self, "Could not tear down irq, %d\n",
				      err);
		sc->irq_res = NULL;
	}

	if (sc->sc_bus.bdev) {
		device_delete_child(self, sc->sc_bus.bdev);
		sc->sc_bus.bdev = NULL;
	}

	if (sc->irq_res) {
		bus_release_resource(self, SYS_RES_IOPORT, 0, sc->irq_res);
		sc->irq_res = NULL;
	}

	if (sc->io_res) {
		bus_release_resource(self, SYS_RES_MEMORY,PCI_CBMEM,sc->io_res);
		sc->io_res = NULL;
		sc->iot = 0;
		sc->ioh = 0;
	}

	return 0;
}

static device_method_t ohci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ohci_pci_probe),
	DEVMETHOD(device_attach,	ohci_pci_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),

	{ 0, 0 }
};

static driver_t ohci_driver = {
	"ohci",
	ohci_methods,
	sizeof(ohci_softc_t),
};

static devclass_t ohci_devclass;

DRIVER_MODULE(ohci, pci, ohci_driver, ohci_devclass, 0, 0);
