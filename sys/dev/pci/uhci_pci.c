/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Author: Lennart Augustsson <augustss@carlstedt.se>
 *         Carlstedt Research & Technology
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

#include <dev/usb/usb_port.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#if defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#endif
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/queue.h>

#if defined(__NetBSD__)
#include <machine/bus.h>
#include <dev/pci/pcivar.h>
#elif defined(__FreeBSD__)
#include <pci/pcivar.h>
#include <pci/pcireg.h>

#define PCI_CLASS_SERIALBUS			0x0c000000
#define PCI_SUBCLASS_COMMUNICATIONS_SERIAL	0x00000000
#define PCI_SUBCLASS_SERIALBUS_FIREWIRE		0x00000000
#define PCI_SUBCLASS_SERIALBUS_ACCESS		0x00010000
#define PCI_SUBCLASS_SERIALBUS_SSA		0x00020000
#define PCI_SUBCLASS_SERIALBUS_USB		0x00030000
#define PCI_SUBCLASS_SERIALBUS_FIBER		0x00040000
#endif

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/uhcireg.h>
#include <dev/usb/uhcivar.h>

#if defined(__NetBSD__)
int	uhci_pci_match __P((struct device *, struct cfdata *, void *));
void	uhci_pci_attach __P((struct device *, struct device *, void *));

struct cfattach uhci_pci_ca = {
	sizeof(uhci_softc_t), uhci_pci_match, uhci_pci_attach
};

#elif defined(__FreeBSD__)

#define PCI_INTERFACE_MASK	0x0000ff00
#define PCI_INTERFACE_SHIFT	8
#define PCI_INTERFACE(d)	(((d)>>8)&PCI_INTERFACE_MASK)
#define PCI_SUBCLASS(d)		((d)&PCI_SUBCLASS_MASK)
#define PCI_CLASS(d)		((d)&PCI_CLASS_MASK)

#define PCI_VENDOR(d)		((d)&0xffff)
#define PCI_DEVICE(d)		(((d)>>8)&0xffff)

#define PCI_UHCI_DEVICEID_PIIX3         0x70208086ul
#define PCI_UHCI_DEVICEID_PIIX4         0x71128086ul
#define PCI_UHCI_DEVICEID_PIIX4E        0x71128086ul    /* no separate step */

#define PCI_UHCI_BASE_REG               0x20

static char *uhci_pci_probe              __P((pcici_t, pcidi_t));
static void uhci_pci_attach              __P((pcici_t, int));

u_long uhci_count = 0;           /* global counter for nr. of devices found */

static struct pci_device uhci_pci_device = {
	"uhci",
	uhci_pci_probe,
	uhci_pci_attach,
	&uhci_count,
	NULL
};

DATA_SET(pcidevice_set, uhci_pci_device);
#endif


#if defined(__NetBSD__)
int
uhci_pci_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_SERIALBUS &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_SERIALBUS_USB &&
	    PCI_INTERFACE(pa->pa_class) == PCI_INTERFACE_UHCI)
		return 1;
 
	return 0;
}

#elif defined(__FreeBSD__)
static char *
uhci_pci_probe(pcici_t config_id, pcidi_t device_id)
{
	u_int32_t class;

	if (device_id == PCI_UHCI_DEVICEID_PIIX3)
		return ("Intel 82371SB USB Host Controller");
	else if (device_id == PCI_UHCI_DEVICEID_PIIX4)
		return ("Intel 82371AB/EB USB Host Controller");
	else {
		class = pci_conf_read(config_id, PCI_CLASS_REG);
		if (   PCI_CLASS(class)	    == PCI_CLASS_SERIALBUS
		    && PCI_SUBCLASS(class)  == PCI_SUBCLASS_SERIALBUS_USB
		    && PCI_INTERFACE(class) == PCI_INTERFACE_UHCI) {
			return ("UHCI Host Controller");
		}
	}

	return NULL;    /* dunno... */
}
#endif


#if defined(__NetBSD__)
void
uhci_pci_attach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	uhci_softc_t *sc = (uhci_softc_t *)self;
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	char const *intrstr;
	pci_intr_handle_t ih;
	pcireg_t csr;
	char *typestr, *vendor;
	char devinfo[256];
	usbd_status r;

	pci_devinfo(pa->pa_id, pa->pa_class, 0, devinfo);
	printf(": %s (rev. 0x%02x)\n", devinfo, PCI_REVISION(pa->pa_class));

	/* Map I/O registers */
	if (pci_mapreg_map(pa, PCI_CBIO, PCI_MAPREG_TYPE_IO, 0,
			   &sc->iot, &sc->ioh, NULL, NULL)) {
		printf("%s: can't map i/o space\n", sc->sc_bus.bdev.dv_xname);
		return;
	}

	sc->sc_dmatag = pa->pa_dmat;


	/* Enable the device. */
	csr = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
		       csr | PCI_COMMAND_MASTER_ENABLE);

	/* Map and establish the interrupt. */
               return EFAULT;

	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf("%s: couldn't map interrupt\n", 
		       sc->sc_bus.bdev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_USB, uhci_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		    sc->sc_bus.bdev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
 	printf("%s: interrupting at %s\n", sc->sc_bus.bdev.dv_xname, intrstr);

	switch(pci_conf_read(pc, pa->pa_tag, PCI_USBREV) & PCI_USBREV_MASK) {
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
	printf("%s: USB version %s\n", sc->sc_bus.bdev.dv_xname, typestr);

	/* Figure out vendor for root hub descriptor. */
	vendor = pci_findvendor(pa->pa_id);
	if (vendor)
		strncpy(sc->sc_vendor, vendor, sizeof(sc->sc_vendor));
	else
		sprintf(sc->sc_vendor, "vendor 0x%04x", PCI_VENDOR(pa->pa_id));
	
	r = uhci_init(sc);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("%s: init failed, error=%d\n", 
		       sc->sc_bus.bdev.dv_xname, r);
		return;
	}

	/* Attach usb device. */
	config_found((void *)sc, &sc->sc_bus, usbctlprint);
}


#elif defined(__FreeBSD__)

static void
uhci_pci_attach(config_id, unit)
        pcici_t config_id;
        int unit;
{
	int irq;
	int id;
	char *typestr;
	char devinfo[256];
	usbd_status r;
	uhci_softc_t *sc = NULL;
	int legsup;

	sc = malloc(sizeof(uhci_softc_t), M_DEVBUF, M_NOWAIT);
	if ( sc == NULL ) {
		printf("usb%d: could not allocate memory", unit);
		return;
	}
	memset(sc, 0, sizeof(uhci_softc_t));

	sc->sc_iobase = pci_conf_read(config_id,PCI_UHCI_BASE_REG) & 0xffe0;
	sc->sc_int    = pci_conf_read(config_id,PCI_INTERRUPT_REG) & 0xff;
	sc->unit      = unit;

	if ( !pci_map_int(config_id, (pci_inthand_t *)uhci_intr,
			  (void *) sc, &bio_imask)) {
		printf("usb%d: Unable to map irq\n", unit);
		return;                    
	}

	if (bootverbose) {
		printf("usb%d: interrupting at %d\n", unit, sc->sc_int);
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
		printf("usb%d: USB version %s\n", unit, typestr);
	}

	/* Figure out vendor for root hub descriptor. */
	id = pci_conf_read(config_id, PCI_ID_REG);
	if (PCI_VENDOR(id) == 0x8086)
		sprintf(sc->sc_vendor, "Intel");
	else
		sprintf(sc->sc_vendor, "Vendor 0x%04x", PCI_VENDOR(id));

	r = uhci_init(sc);
	if (r != USBD_NORMAL_COMPLETION) {
		printf("usb%d: init failed, error=%d\n", unit, r);
		return;
	}


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
	if (!sc->sc_bus.bdev)
		DEVICE_ERROR(sc->sc_bus.bdev,
			("unable to add USB device to root bus\n"));

	id = pci_conf_read(config_id, PCI_ID_REG);
	switch (id) {
	case PCI_UHCI_DEVICEID_PIIX3:
		device_set_desc(sc->sc_bus.bdev, "Intel 82371SB USB Host Controller");
		break;
	case PCI_UHCI_DEVICEID_PIIX4:
		device_set_desc(sc->sc_bus.bdev, "Intel 82371AB/EB USB Host Controller");
		break;
	default:
		device_set_desc(sc->sc_bus.bdev, "UHCI Host Controller");
	}

	return;
}
#endif
