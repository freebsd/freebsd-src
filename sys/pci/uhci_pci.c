/*	FreeBSD $Id: uhci_pci.c,v 1.1 1999/02/18 21:42:19 n_hibma Exp $ */

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

#define PCI_CLASS_SERIALBUS			0x0c000000
#define PCI_SUBCLASS_COMMUNICATIONS_SERIAL	0x00000000
#define PCI_SUBCLASS_SERIALBUS_FIREWIRE		0x00000000
#define PCI_SUBCLASS_SERIALBUS_ACCESS		0x00010000
#define PCI_SUBCLASS_SERIALBUS_SSA		0x00020000
#define PCI_SUBCLASS_SERIALBUS_USB		0x00030000
#define PCI_SUBCLASS_SERIALBUS_FIBER		0x00040000

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/uhcireg.h>
#include <dev/usb/uhcivar.h>

#define PCI_INTERFACE(d)	(((d)>>8)&0xff)
#define PCI_SUBCLASS(d)		((d)&PCI_SUBCLASS_MASK)
#define PCI_CLASS(d)		((d)&PCI_CLASS_MASK)

#define PCI_VENDOR(d)		((d)&0xffff)
#define PCI_DEVICE(d)		(((d)>>8)&0xffff)

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

static const char *uhci_pci_probe	__P((pcici_t, pcidi_t));
static void uhci_pci_attach		__P((pcici_t, int));

u_long uhci_count = 0;           /* global counter for nr. of devices found */

static struct pci_device uhci_pci_device = {
	"uhci",
	uhci_pci_probe,
	uhci_pci_attach,
	&uhci_count,
	NULL
};

DATA_SET(pcidevice_set, uhci_pci_device);


static const char *
uhci_pci_probe(pcici_t config_id, pcidi_t device_id)
{
	u_int32_t class;

	if (device_id == PCI_UHCI_DEVICEID_PIIX3) {
		return (uhci_device_piix3);
	} else if (device_id == PCI_UHCI_DEVICEID_PIIX4) {
		return (uhci_device_piix4);
	} else if (device_id == PCI_UHCI_DEVICEID_VT83C572) {
		return (uhci_device_vt83c572);
	} else {
		class = pci_conf_read(config_id, PCI_CLASS_REG);
		if (   PCI_CLASS(class)	    == PCI_CLASS_SERIALBUS
		    && PCI_SUBCLASS(class)  == PCI_SUBCLASS_SERIALBUS_USB
		    && PCI_INTERFACE(class) == PCI_INTERFACE_UHCI) {
			return (uhci_device_generic);
		}
	}

	return NULL;    /* dunno... */
}

static void
uhci_pci_attach(pcici_t config_id, int unit)
{
	int id, legsup;
	char *typestr;
	usbd_status r;
	uhci_softc_t *sc = NULL;

	sc = malloc(sizeof(uhci_softc_t), M_DEVBUF, M_NOWAIT);
	/* Do not free it below, intr might use the sc */
	if ( sc == NULL ) {
		printf("usb%d: could not allocate memory", unit);
		return;
	}
	memset(sc, 0, sizeof(uhci_softc_t));

	sc->sc_iobase = pci_conf_read(config_id,PCI_UHCI_BASE_REG) & 0xffe0;
	sc->unit      = unit;

	if ( !pci_map_int(config_id, (pci_inthand_t *)uhci_intr,
			  (void *) sc, &bio_imask)) {
		printf("usb%d: could not map irq\n", unit);
		return;                    
	}

#ifndef USBVERBOSE
	if (bootverbose)
#endif
	{
		switch(pci_conf_read(config_id, PCI_USBREV) & PCI_USBREV_MASK) {
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
		printf("usb%d: USB version %s, chip rev. %d\n", unit, typestr,
			(int) pci_conf_read(config_id, PCIR_REVID) & 0xff);
	}

	/* Figure out vendor for root hub descriptor. */
	id = pci_conf_read(config_id, PCI_ID_REG);
	if (PCI_VENDOR(id) == PCI_UHCI_VENDORID_INTEL)
		sprintf(sc->sc_vendor, "Intel");
	else if (PCI_VENDOR(id) == PCI_UHCI_VENDORID_VIA)
		sprintf(sc->sc_vendor, "VIA");
	else
		sprintf(sc->sc_vendor, "(0x%04x)", PCI_VENDOR(id));

	/* We add a child to the root bus. After PCI configuration
	 * has completed the root bus will start to probe and
	 * attach all the devices attached to it, including our new
	 * kid.
	 *
	 * FIXME Sometime in the future the UHCI controller itself will
	 * become a kid of PCI device and this device add will no longer
	 * be necessary.
	 *
	 * See README for an elaborate description of the bus
	 * structure in spe.
	 */
	sc->sc_bus.bdev = device_add_child(root_bus, "usb", unit, sc);
	if (!sc->sc_bus.bdev) {
		printf("usb%d: could not add USB device to root bus\n", unit);
		return;
	}

	switch (id) {
	case PCI_UHCI_DEVICEID_PIIX3:
		device_set_desc(sc->sc_bus.bdev, uhci_device_piix3);
		break;
	case PCI_UHCI_DEVICEID_PIIX4:
		device_set_desc(sc->sc_bus.bdev, uhci_device_piix4);
		break;
	case PCI_UHCI_DEVICEID_VT83C572:
		device_set_desc(sc->sc_bus.bdev, uhci_device_vt83c572);
		break;
	default:
		printf("(New UHCI DeviceId=0x%08x)\n", id);
		device_set_desc(sc->sc_bus.bdev, uhci_device_generic);
	}

	legsup = pci_conf_read(config_id, PCI_LEGSUP);
	if ( ! (legsup & PCI_LEGSUP_USBPIRQDEN) ) {
#if ! (defined(USBVERBOSE) || defined(USB_DEBUG))
		if (bootverbose)
#endif
			printf("%s%d: PIRQD enable not set\n",
				device_get_name(sc->sc_bus.bdev),
				device_get_unit(sc->sc_bus.bdev));
		legsup |= PCI_LEGSUP_USBPIRQDEN;
		pci_conf_write(config_id, PCI_LEGSUP, legsup);
	}

	r = uhci_init(sc);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("%s%d: init failed, error=%d\n",
			device_get_name(sc->sc_bus.bdev),
			device_get_unit(sc->sc_bus.bdev),
			r);
		device_delete_child(root_bus, sc->sc_bus.bdev);
	}

	return;
}
