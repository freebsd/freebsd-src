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
#include <sys/device.h>
#include <sys/proc.h>
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
#define PCI_OHCI_VENDORID_CMDTECH	0x1095
#define PCI_OHCI_VENDORID_COMPAQ	0x0e11
#define PCI_OHCI_VENDORID_NEC		0x1033
#define PCI_OHCI_VENDORID_OPTI		0x1045
#define PCI_OHCI_VENDORID_SIS		0x1039

#define PCI_OHCI_DEVICEID_ALADDIN_V	0x523710b9
static const char *ohci_device_aladdin_v = "AcerLabs M5237 (Aladdin-V) USB controller";
#define PCI_OHCI_DEVICEID_FIRELINK	0xc8611045
static const char *ohci_device_firelink  = "OPTi 82C861 (FireLink) USB controller";
#define PCI_OHCI_DEVICEID_NEC		0x00351033
static const char *ohci_device_nec	 = "NEC uPD 9210 USB controller";
#define PCI_OHCI_DEVICEID_USB0670	0x06701095
static const char *ohci_device_usb0670	 = "CMD Tech 670 (USB0670) USB controller";
#define PCI_OHCI_DEVICEID_USB0673	0x06731095
static const char *ohci_device_usb0673	 = "CMD Tech 673 (USB0673) USB controller";

static const char *ohci_device_generic   = "OHCI (generic) USB controller";

#define PCI_OHCI_BASE_REG	0x10

static const char *
ohci_pci_match(device_t dev)
{
	u_int32_t device_id = pci_get_devid(dev);

	switch(device_id) {
	case PCI_OHCI_DEVICEID_ALADDIN_V:
		return (ohci_device_aladdin_v);
	case PCI_OHCI_DEVICEID_USB0670:
		return (ohci_device_usb0670);
	case PCI_OHCI_DEVICEID_USB0673:
		return (ohci_device_usb0673);
	case PCI_OHCI_DEVICEID_FIRELINK:
		return (ohci_device_firelink);
	case PCI_OHCI_DEVICEID_NEC:
		return (ohci_device_nec);
	default:
		if (   pci_get_class(dev)    == PCIC_SERIALBUS
		    && pci_get_subclass(dev) == PCIS_SERIALBUS_USB
		    && pci_get_progif(dev)   == PCI_INTERFACE_OHCI) {
			return (ohci_device_generic);
		}
	}

	return NULL;	/* dunno */
}

static int
ohci_pci_probe(device_t dev)
{
	const char *desc = ohci_pci_match(dev);
	if (desc) {
		device_set_desc(dev, desc);
		return 0;
	} else {
		return ENXIO;
	}
}

static int
ohci_pci_attach(device_t dev)
{
	int unit = device_get_unit(dev);
	ohci_softc_t *sc = device_get_softc(dev);
	device_t usbus;
	usbd_status err;
	int rid;
	struct resource *res;
	void *ih;
	int error;

	rid = PCI_CBMEM;
	res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
				 0, ~0, 1, RF_ACTIVE);
	if (!res) {
		device_printf(dev, "could not map memory\n");
		return ENXIO;
        }

	sc->iot = rman_get_bustag(res);
	sc->ioh = rman_get_bushandle(res);

	rid = 0;
	res = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
				 RF_SHAREABLE | RF_ACTIVE);
	if (res == NULL) {
		device_printf(dev, "could not allocate irq\n");
		return ENOMEM;
	}

	error = bus_setup_intr(dev, res, INTR_TYPE_BIO,
			       (driver_intr_t *) ohci_intr, sc, &ih);
	if (error) {
		device_printf(dev, "could not setup irq\n");
		return error;
	}

	usbus = device_add_child(dev, "usb", -1, sc);
	if (!usbus) {
		printf("ohci%d: could not add USB device\n", unit);
		return ENOMEM;
	}

	switch (pci_get_devid(dev)) {
	case PCI_OHCI_DEVICEID_ALADDIN_V:
		device_set_desc(usbus, ohci_device_aladdin_v);
		sprintf(sc->sc_vendor, "AcerLabs");
		break;
	case PCI_OHCI_DEVICEID_FIRELINK:
		device_set_desc(usbus, ohci_device_firelink);
		sprintf(sc->sc_vendor, "OPTi");
		break;
	case PCI_OHCI_DEVICEID_NEC:
		device_set_desc(usbus, ohci_device_nec);
		sprintf(sc->sc_vendor, "NEC");
		break;
	case PCI_OHCI_DEVICEID_USB0670:
		device_set_desc(usbus, ohci_device_usb0670);
		sprintf(sc->sc_vendor, "CMDTECH");
		break;
	case PCI_OHCI_DEVICEID_USB0673:
		device_set_desc(usbus, ohci_device_usb0673);
		sprintf(sc->sc_vendor, "CMDTECH");
		break;
	default:
		if (bootverbose)
			printf("(New OHCI DeviceId=0x%08x)\n", pci_get_devid(dev));
		device_set_desc(usbus, ohci_device_generic);
		sprintf(sc->sc_vendor, "(unknown)");
	}

	sc->sc_bus.bdev = usbus;
	err = ohci_init(sc);
	if (err != USBD_NORMAL_COMPLETION) {
		printf("ohci%d: init failed, error=%d\n", unit, err);
		device_delete_child(dev, usbus);
	}

	return device_probe_and_attach(sc->sc_bus.bdev);
}

static device_method_t ohci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ohci_pci_probe),
	DEVMETHOD(device_attach,	ohci_pci_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	{ 0, 0 }
};

static driver_t ohci_driver = {
	"ohci",
	ohci_methods,
	sizeof(ohci_softc_t),
};

static devclass_t ohci_devclass;

DRIVER_MODULE(ohci, pci, ohci_driver, ohci_devclass, 0, 0);
