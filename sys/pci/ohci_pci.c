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
 * USB spec: http://www.teleport.com/cgi-bin/mailmerge.cgi/~usb/cgiform.tpl
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/queue.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>

#define PCI_CLASS_SERIALBUS                     0x0c000000
#define PCI_SUBCLASS_COMMUNICATIONS_SERIAL      0x00000000
#define PCI_SUBCLASS_SERIALBUS_FIREWIRE         0x00000000
#define PCI_SUBCLASS_SERIALBUS_ACCESS           0x00010000
#define PCI_SUBCLASS_SERIALBUS_SSA              0x00020000
#define PCI_SUBCLASS_SERIALBUS_USB              0x00030000
#define PCI_SUBCLASS_SERIALBUS_FIBER            0x00040000

#define PCI_INTERFACE(d)        (((d) >> 8) & 0xff)
#define PCI_SUBCLASS(d)         ((d) & PCI_SUBCLASS_MASK)
#define PCI_CLASS(d)            ((d) & PCI_CLASS_MASK)


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
static const char *ohci_device_aladdin_v = "AcerLabs M5237 (Aladdin-V) USB Host Controller";
#define PCI_OHCI_DEVICEID_FIRELINK	0xc8611045
static const char *ohci_device_firelink  = "OPTi 82C861 (FireLink) USB Host Controller";
#define PCI_OHCI_DEVICEID_NEC		0x00351033
static const char *ohci_device_nec	 = "NEC uPD 9210 USB Host Controller";
#define PCI_OHCI_DEVICEID_USB0670	0x06701095
static const char *ohci_device_usb0670	 = "CMD Tech 670 (USB0670) USB Host Controller";
#define PCI_OHCI_DEVICEID_USB0673	0x06731095
static const char *ohci_device_usb0673	 = "CMD Tech 673 (USB0673) USB Host Controller";

static const char *ohci_device_generic   = "OHCI (generic) USB Host Controller";


static const char *ohci_pci_probe              __P((pcici_t, pcidi_t));
static void ohci_pci_attach              __P((pcici_t, int));

static u_long ohci_count = 0;

static struct pci_device ohci_pci_device = {
        "ohci",
	ohci_pci_probe,
	ohci_pci_attach,
	&ohci_count,
	NULL
};

DATA_SET(pcidevice_set, ohci_pci_device);


static const char *
ohci_pci_probe(pcici_t config_id, pcidi_t device_id)
{
        u_int32_t class;

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
		class = pci_conf_read(config_id, PCI_CLASS_REG);
		if (   (PCI_CLASS(class)     == PCI_CLASS_SERIALBUS)
		    && (PCI_SUBCLASS(class)  == PCI_SUBCLASS_SERIALBUS_USB)
		    && (PCI_INTERFACE(class) == PCI_INTERFACE_OHCI)) {
			return(ohci_device_generic);
		}
	}

	return NULL;	/* dunno */
}

static void
ohci_pci_attach(pcici_t config_id, int unit)
{
	vm_offset_t pbase;
	device_t usbus;
	ohci_softc_t *sc;
	usbd_status err;
	int id;

	sc = malloc(sizeof(ohci_softc_t), M_DEVBUF, M_NOWAIT);
	/* Do not free it below, intr might use the sc */
	if ( sc == NULL ) {
		printf("ohci%d: could not allocate memory", unit);
		return;
	}
	memset(sc, 0, sizeof(ohci_softc_t));

	if(!pci_map_mem(config_id, PCI_CBMEM,
           (vm_offset_t *)&sc->sc_iobase, &pbase)) {
		printf("ohci%d: could not map memory\n", unit);
		return;
        }

	if ( !pci_map_int(config_id, (pci_inthand_t *)ohci_intr,
			  (void *) sc, &bio_imask)) {
		printf("ohci%d: could not map irq\n", unit);
		return;                    
	}

	usbus = device_add_child(root_bus, "usb", -1, sc);
	if (!usbus) {
		printf("ohci%d: could not add USB device to root bus\n", unit);
		return;
	}

	id = pci_conf_read(config_id, PCI_ID_REG);
	switch(id) {
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
			printf("(New OHCI DeviceId=0x%08x)\n", id);
		device_set_desc(usbus, ohci_device_generic);
		sprintf(sc->sc_vendor, "(unknown)");
	}

	sc->sc_bus.bdev = usbus;
	err = ohci_init(sc);
	if (err != USBD_NORMAL_COMPLETION) {
		printf("ohci%d: init failed, error=%d\n", unit, err);
		device_delete_child(root_bus, usbus);
	}

	return;
}
