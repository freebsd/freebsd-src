/*
 * Copyright (c) 1998-2002 Katsushi Kobayashi and Hidetoshi Shimokawa
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the acknowledgement as bellow:
 *
 *    This product includes software developed by K. Kobayashi and H. SHimokawa
 *
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * $FreeBSD$
 */

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

#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirebusreg.h>
#include <dev/firewire/firewirereg.h>

#include <dev/firewire/fwohcireg.h>
#include <dev/firewire/fwohcivar.h>

static int fwohci_pci_attach(device_t self);
static int fwohci_pci_detach(device_t self);

/*
 * The probe routine.
 */
static int
fwohci_pci_probe( device_t dev )
{
#if 1
	if ((pci_get_vendor(dev) == FW_VENDORID_NEC) &&
	    (pci_get_device(dev) == FW_DEVICE_UPD861)) {
		device_set_desc(dev, "NEC uPD72861");
		return 0;
	}
	if ((pci_get_vendor(dev) == FW_VENDORID_TI) &&
	    (pci_get_device(dev) == FW_DEVICE_TITSB22)) {
		device_set_desc(dev, "Texas Instruments TSB12LV22");
		return 0;
	}
	if ((pci_get_vendor(dev) == FW_VENDORID_TI) &&
	    (pci_get_device(dev) == FW_DEVICE_TITSB23)) {
		device_set_desc(dev, "Texas Instruments TSB12LV23");
		return 0;
	}
	if ((pci_get_vendor(dev) == FW_VENDORID_TI) &&
	    (pci_get_device(dev) == FW_DEVICE_TITSB26)) {
		device_set_desc(dev, "Texas Instruments TSB12LV26");
		return 0;
	}
	if ((pci_get_vendor(dev) == FW_VENDORID_TI) &&
	    (pci_get_device(dev) == FW_DEVICE_TITSB43)) {
		device_set_desc(dev, "Texas Instruments TSB43AA22");
		return 0;
	}
	if ((pci_get_vendor(dev) == FW_VENDORID_SONY) &&
	    (pci_get_device(dev) == FW_DEVICE_CX3022)) {
		device_set_desc(dev, "SONY CX3022");
		return 0;
	}
	if ((pci_get_vendor(dev) == FW_VENDORID_VIA) &&
	    (pci_get_device(dev) == FW_DEVICE_VT6306)) {
		device_set_desc(dev, "VIA VT6306");
		return 0;
	}
	if ((pci_get_vendor(dev) == FW_VENDORID_RICOH) &&
	    (pci_get_device(dev) == FW_DEVICE_R5C552)) {
		device_set_desc(dev, "Ricoh R5C552");
		return 0;
	}
	if ((pci_get_vendor(dev) == FW_VENDORID_APPLE) &&
	    (pci_get_device(dev) == FW_DEVICE_PANGEA)) {
		device_set_desc(dev, "Apple Pangea");
		return 0;
	}
	if ((pci_get_vendor(dev) == FW_VENDORID_APPLE) &&
	    (pci_get_device(dev) == FW_DEVICE_UNINORTH)) {
		device_set_desc(dev, "Apple UniNorth");
		return 0;
	}
	if ((pci_get_vendor(dev) == FW_VENDORID_LUCENT) &&
	    (pci_get_device(dev) == FW_DEVICE_FW322)) {
		device_set_desc(dev, "Lucent FW322/323");
		return 0;
	}
#endif
	if (pci_get_class(dev) == PCIC_SERIALBUS
			&& pci_get_subclass(dev) == PCIS_SERIALBUS_FW
			&& pci_get_progif(dev) == PCI_INTERFACE_OHCI) {
		printf("XXXfw: vendid=%x, dev=%x\n", pci_get_vendor(dev),
			pci_get_device(dev));
		device_set_desc(dev, "1394 Open Host Controller Interface");
		return 0;
	}

	return ENXIO;
}

#if __FreeBSD_version < 500000
static void
fwohci_dummy_intr(void *arg)
{
	/* XXX do nothing */
}
#endif

static int
fwohci_pci_attach(device_t self)
{
	fwohci_softc_t *sc = device_get_softc(self);
	int err;
	int rid;
	int latency, cache_line;
	u_int16_t cmd;
#if __FreeBSD_version < 500000
	int intr;
	/* For the moment, put in a message stating what is wrong */
	intr = pci_read_config(self, PCIR_INTLINE, 1);
	if (intr == 0 || intr == 255) {
		device_printf(self, "Invalid irq %d\n", intr);
#ifdef __i386__
		device_printf(self, "Please switch PNP-OS to 'No' in BIOS\n");
#endif
#if 0
		return ENXIO;
#endif
	}
#endif

	cmd = pci_read_config(self, PCIR_COMMAND, 2);
	cmd |= PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN | PCIM_CMD_MWRICEN;
	pci_write_config(self, PCIR_COMMAND, cmd, 2);

	latency = pci_read_config(self, PCIR_LATTIMER, 1);
#define DEF_LATENCY 250 /* Derived from Max Bulk Transfer size 512 Bytes*/
	if( latency < DEF_LATENCY ) {
		latency = DEF_LATENCY;
		device_printf(self, "PCI bus latency was changing to");
		pci_write_config(self, PCIR_LATTIMER,latency, 1);
	} else
	 {
		device_printf(self, "PCI bus latency is");
	}
	printf(" %d.\n", (int) latency);
	cache_line = pci_read_config(self, PCIR_CACHELNSZ, 1);
#if 0
#define DEF_CACHE_LINE 0xc
	cache_line = DEF_CACHE_LINE;
	pci_write_config(self, PCIR_CACHELNSZ, cache_line, 1);
#endif
	printf("cache size %d.\n", (int) cache_line);
/**/
	rid = PCI_CBMEM;
	sc->bsr = bus_alloc_resource(self, SYS_RES_MEMORY, &rid,
					0, ~0, 1, RF_ACTIVE);
	if (!sc->bsr) {
		device_printf(self, "Could not map memory\n");
		return ENXIO;
        }

	sc->bst = rman_get_bustag(sc->bsr);
	sc->bsh = rman_get_bushandle(sc->bsr);

	rid = 0;
	sc->irq_res = bus_alloc_resource(self, SYS_RES_IRQ, &rid, 0, ~0, 1,
				     RF_SHAREABLE | RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(self, "Could not allocate irq\n");
		fwohci_pci_detach(self);
		return ENXIO;
	}

	sc->fc.bdev = device_add_child(self, "firewire", -1);
	if (!sc->fc.bdev) {
		device_printf(self, "Could not add firewire device\n");
		fwohci_pci_detach(self);
		return ENOMEM;
	}
	device_set_ivars(sc->fc.bdev, sc);

	err = bus_setup_intr(self, sc->irq_res, INTR_TYPE_NET,
		     (driver_intr_t *) fwohci_intr, sc, &sc->ih);
#if __FreeBSD_version < 500000
	/* XXX splcam() should mask this irq for sbp.c*/
	err = bus_setup_intr(self, sc->irq_res, INTR_TYPE_CAM,
		     (driver_intr_t *) fwohci_dummy_intr, sc, &sc->ih_cam);
#endif
	if (err) {
		device_printf(self, "Could not setup irq, %d\n", err);
		fwohci_pci_detach(self);
		return ENXIO;
	}

	err = fwohci_init(sc, self);

	if (!err)
		err = device_probe_and_attach(sc->fc.bdev);

	if (err) {
		device_printf(self, "Firewire init failed\n");
		fwohci_pci_detach(self);
		return EIO;
	}

	return 0;
}

static int
fwohci_pci_detach(device_t self)
{
	fwohci_softc_t *sc = device_get_softc(self);
	int s;


	s = splfw();
	bus_generic_detach(self);

	/* disable interrupts that might have been switched on */
	if (sc->bst && sc->bsh)
		bus_space_write_4(sc->bst, sc->bsh,
				  FWOHCI_INTMASKCLR, OHCI_INT_EN);

	if (sc->irq_res) {
		int err = bus_teardown_intr(self, sc->irq_res, sc->ih);
		if (err)
			/* XXX or should we panic? */
			device_printf(self, "Could not tear down irq, %d\n",
				      err);
#if __FreeBSD_version < 500000
		err = bus_teardown_intr(self, sc->irq_res, sc->ih_cam);
#endif
		sc->ih = NULL;
	}

	if (sc->fc.bdev) {
		device_delete_child(self, sc->fc.bdev);
		sc->fc.bdev = NULL;
	}

	if (sc->irq_res) {
		bus_release_resource(self, SYS_RES_IRQ, 0, sc->irq_res);
		sc->irq_res = NULL;
	}

	if (sc->bsr) {
		bus_release_resource(self, SYS_RES_MEMORY,PCI_CBMEM,sc->bsr);
		sc->bsr = NULL;
		sc->bst = 0;
		sc->bsh = 0;
	}
	splx(s);

	return 0;
}

static device_method_t fwohci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fwohci_pci_probe),
	DEVMETHOD(device_attach,	fwohci_pci_attach),
	DEVMETHOD(device_detach,	fwohci_pci_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),

	{ 0, 0 }
};

static driver_t fwohci_driver = {
	"fwohci",
	fwohci_methods,
	sizeof(fwohci_softc_t),
};

static devclass_t fwohci_devclass;

DRIVER_MODULE(fwohci, pci, fwohci_driver, fwohci_devclass, 0, 0);
DRIVER_MODULE(fwohci, cardbus, fwohci_driver, fwohci_devclass, 0, 0);
