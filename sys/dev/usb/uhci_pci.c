/*	FreeBSD $Id: uhci_pci.c,v 1.6 1999/04/16 21:22:53 peter Exp $ */

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

/* Universal Host Controller Interface
 *
 * UHCI spec: http://www.intel.com/
 */

/* The low level controller code for UHCI has been split into
 * PCI probes and UHCI specific code. This was done to facilitate the
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
#if defined(__FreeBSD__)
#include <machine/bus_pio.h>
#endif
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/uhcireg.h>
#include <dev/usb/uhcivar.h>

#define PCI_UHCI_VENDORID_INTEL		0x8086
#define PCI_UHCI_VENDORID_VIA		0x1106

#define PCI_UHCI_DEVICEID_PIIX3         0x70208086ul
static const char *uhci_device_piix3	= "Intel 82371SB (PIIX3) USB Host Controller";
#define PCI_UHCI_DEVICEID_PIIX4         0x71128086ul
#define PCI_UHCI_DEVICEID_PIIX4E        0x71128086ul    /* no separate step */
static const char *uhci_device_piix4	= "Intel 82371AB/EB (PIIX4) USB Host Controller";
#define PCI_UHCI_DEVICEID_VT83C572	0x30381106ul
static const char *uhci_device_vt83c572	= "VIA 83C572 USB Host Controller";
static const char *uhci_device_generic	= "UHCI (generic) USB Controller";

#define PCI_UHCI_BASE_REG               0x20

static const char *
uhci_pci_match(device_t dev)
{
	u_int32_t device_id = pci_get_devid(dev);

	if (device_id == PCI_UHCI_DEVICEID_PIIX3) {
		return (uhci_device_piix3);
	} else if (device_id == PCI_UHCI_DEVICEID_PIIX4) {
		return (uhci_device_piix4);
	} else if (device_id == PCI_UHCI_DEVICEID_VT83C572) {
		return (uhci_device_vt83c572);
	} else {
		if (   pci_get_class(dev)    == PCIC_SERIALBUS
		    && pci_get_subclass(dev) == PCIS_SERIALBUS_USB
		    && pci_get_progif(dev)   == PCI_INTERFACE_UHCI) {
			return (uhci_device_generic);
		}
	}

	return NULL;    /* dunno... */
}

static int
uhci_pci_probe(device_t dev)
{
	const char *desc = uhci_pci_match(dev);
	if (desc) {
		device_set_desc(dev, desc);
		return 0;
	} else {
		return ENXIO;
	}
}

static int
uhci_pci_attach(device_t dev)
{
	int unit = device_get_unit(dev);
	int legsup;
	char *typestr;
	usbd_status err;
	device_t usbus;
	uhci_softc_t *sc = device_get_softc(dev);
	int rid;
	struct resource *res;
	void *ih;
	int error;

	rid = PCI_UHCI_BASE_REG;
	res = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				 0, ~0, 1, RF_ACTIVE);
	if (!res) {
		device_printf(dev, "could not map ports\n");
		return ENXIO;
        }

	sc->iot = rman_get_bustag(res);
	sc->ioh = rman_get_bushandle(res);

	bus_space_write_2(sc->iot, sc->ioh, UHCI_INTR, 0);	/* disable interrupts */

	rid = 0;
	res = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
				 RF_SHAREABLE | RF_ACTIVE);
	if (res == NULL) {
		device_printf(dev, "could not allocate irq\n");
		return ENOMEM;
	}
		
	error = bus_setup_intr(dev, res, (driver_intr_t *) uhci_intr, sc, &ih);
	if (error) {
		device_printf(dev, "could not setup irq\n");
		return error;
	}

	usbus = device_add_child(dev, "usb", -1, sc);
	if (!usbus) {
		printf("usb%d: could not add USB device\n", unit);
		return ENOMEM;
	}

	switch (pci_get_devid(dev)) {
	case PCI_UHCI_DEVICEID_PIIX3:
		device_set_desc(usbus, uhci_device_piix3);
		sprintf(sc->sc_vendor, "Intel");
		break;
	case PCI_UHCI_DEVICEID_PIIX4:
		device_set_desc(usbus, uhci_device_piix4);
		sprintf(sc->sc_vendor, "Intel");
		break;
	case PCI_UHCI_DEVICEID_VT83C572:
		device_set_desc(usbus, uhci_device_vt83c572);
		sprintf(sc->sc_vendor, "VIA");
		break;
	default:
		printf("(New UHCI DeviceId=0x%08x)\n", pci_get_devid(dev));
		device_set_desc(usbus, uhci_device_generic);
		sprintf(sc->sc_vendor, "(0x%08x)", pci_get_devid(dev));
	}

	if (bootverbose) {
		switch(pci_read_config(dev, PCI_USBREV, 4) & PCI_USBREV_MASK) {
		case PCI_USBREV_PRE_1_0:
			typestr = "pre 1.0";
			break;
		case PCI_USBREV_1_0:
			typestr = "1.0";
			break;
		default:
			typestr = "unknown";
			break;
		}
		printf("uhci%d: USB version %s, chip rev. %d\n", unit, typestr,
			pci_get_revid(dev));
	}

	legsup = pci_read_config(dev, PCI_LEGSUP, 4);
	if ( !(legsup & PCI_LEGSUP_USBPIRQDEN) ) {
#if ! (defined(USBVERBOSE) || defined(USB_DEBUG))
		if (bootverbose)
#endif
			printf("uhci%d: PIRQD enable not set\n", unit);
		legsup |= PCI_LEGSUP_USBPIRQDEN;
		pci_write_config(dev, PCI_LEGSUP, legsup, 4);
	}

	sc->sc_bus.bdev = usbus;
	err = uhci_init(sc);
	if (err != USBD_NORMAL_COMPLETION) {
		printf("uhci%d: init failed, error=%d\n", unit, err);
		device_delete_child(dev, usbus);
	}

	return device_probe_and_attach(sc->sc_bus.bdev);
}

static device_method_t uhci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		uhci_pci_probe),
	DEVMETHOD(device_attach,	uhci_pci_attach),

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

static driver_t uhci_driver = {
	"uhci",
	uhci_methods,
	DRIVER_TYPE_BIO,
	sizeof(uhci_softc_t),
};

static devclass_t uhci_devclass;

DRIVER_MODULE(uhci, pci, uhci_driver, uhci_devclass, 0, 0);
