/*-
 * Copyright (c) 2001 Brian Somers <brian@Awfulhak.org>
 *   based on work by Slawa Olhovchenkov
 *                    John Prince <johnp@knight-trosoft.com>
 *                    Eric Hernes
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/dev/digi/digi_pci.c,v 1.12 2005/03/05 18:30:10 imp Exp $");

#include <sys/param.h>

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/tty.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <dev/pci/pcivar.h>

#include <sys/digiio.h>
#include <dev/digi/digireg.h>
#include <dev/digi/digi.h>
#include <dev/digi/digi_pci.h>

static u_char *
digi_pci_setwin(struct digi_softc *sc, unsigned int addr)
{
	return (sc->vmem + addr);
}

static void
digi_pci_hidewin(struct digi_softc *sc)
{
	return;
}

static void
digi_pci_towin(struct digi_softc *sc, int win)
{
	return;
}

static int
digi_pci_probe(device_t dev)
{
	unsigned int device_id = pci_get_devid(dev);

	if (device_get_unit(dev) >= 16) {
		/* Don't overflow our control mask */
		device_printf(dev, "At most 16 digiboards may be used\n");
		return (ENXIO);
	}

	if ((device_id & 0xffff) != PCI_VENDOR_DIGI)
		return (ENXIO);

	switch (device_id >> 16) {
	case PCI_DEVICE_EPC:
	case PCI_DEVICE_XEM:
	case PCI_DEVICE_XR:
	case PCI_DEVICE_CX:
	case PCI_DEVICE_XRJ:
	case PCI_DEVICE_EPCJ:
	case PCI_DEVICE_920_4:
	case PCI_DEVICE_920_8:
	case PCI_DEVICE_920_2:
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
digi_pci_attach(device_t dev)
{
	struct digi_softc *sc;
	u_int32_t device_id;
#ifdef DIGI_INTERRUPT
	int retVal = 0;
#endif

	sc = device_get_softc(dev);
	KASSERT(sc, ("digi%d: softc not allocated in digi_pci_attach\n",
	    device_get_unit(dev)));

	bzero(sc, sizeof(*sc));
	sc->dev = dev;
	sc->res.unit = device_get_unit(dev);

	device_id = pci_get_devid(dev);
	switch (device_id >> 16) {
	case PCI_DEVICE_EPC:
		sc->name = "Digiboard PCI EPC/X ASIC";
		sc->res.mrid = 0x10;
		sc->model = PCIEPCX;
		sc->module = "EPCX_PCI";
		break;
	case PCI_DEVICE_XEM:
		sc->name = "Digiboard PCI PC/Xem ASIC";
		sc->res.mrid = 0x10;
		sc->model = PCXEM;
		sc->module = "Xem";
		break;
	case PCI_DEVICE_XR:
		sc->name = "Digiboard PCI PC/Xr ASIC";
		sc->res.mrid = 0x10;
		sc->model = PCIXR;
		sc->module = "Xr";
		break;
	case PCI_DEVICE_CX:
		sc->name = "Digiboard PCI C/X ASIC";
		sc->res.mrid = 0x10;
		sc->model = PCCX;
		sc->module = "CX_PCI";
		break;
	case PCI_DEVICE_XRJ:
		sc->name = "Digiboard PCI PC/Xr PLX";
		sc->res.mrid = 0x18;
		sc->model = PCIXR;
		sc->module = "Xr";
		break;
	case PCI_DEVICE_EPCJ:
		sc->name = "Digiboard PCI EPC/X PLX";
		sc->res.mrid = 0x18;
		sc->model = PCIEPCX;
		sc->module = "EPCX_PCI";
		break;
	case PCI_DEVICE_920_4:			/* Digi PCI4r 920 */
		sc->name = "Digiboard PCI4r 920";
		sc->res.mrid = 0x10;
		sc->model = PCIXR;
		sc->module = "Xr";
		break;
	case PCI_DEVICE_920_8:			/* Digi PCI8r 920 */
		sc->name = "Digiboard PCI8r 920";
		sc->res.mrid = 0x10;
		sc->model = PCIXR;
		sc->module = "Xr";
		break;
	case PCI_DEVICE_920_2:			/* Digi PCI2r 920 */
		sc->name = "Digiboard PCI2r 920";
		sc->res.mrid = 0x10;
		sc->model = PCIXR;
		sc->module = "Xr";
		break;
	default:
		device_printf(dev, "Unknown device id = %08x\n", device_id);
		return (ENXIO);
	}

	pci_write_config(dev, 0x40, 0, 4);
	pci_write_config(dev, 0x46, 0, 4);

	sc->res.mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->res.mrid,
	    RF_ACTIVE);

#ifdef DIGI_INTERRUPT
	sc->res.irqrid = 0;
	sc->res.irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->res.irqrid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->res.irq == NULL) {
		device_printf(dev, "couldn't map interrupt\n");
		return (ENXIO);
	}
	retVal = bus_setup_intr(dev, sc->res.irq, INTR_TYPE_TTY,
	    digiintr, sc, &sc->res.irqHandler);
#else
	DLOG(DIGIDB_IRQ, (sc->dev, "Interrupt support compiled out\n"));
#endif

	sc->vmem = rman_get_virtual(sc->res.mem);
	sc->pmem = vtophys(sc->vmem);
	sc->pcibus = 1;
	sc->win_size = 0x200000;
	sc->win_bits = 21;
	sc->csigs = &digi_normal_signals;
	sc->status = DIGI_STATUS_NOTINIT;
	callout_handle_init(&sc->callout);
	callout_handle_init(&sc->inttest);
	sc->setwin = digi_pci_setwin;
	sc->hidewin = digi_pci_hidewin;
	sc->towin = digi_pci_towin;

	PCIPORT = FEPRST;

	return (digi_attach(sc));
}

static device_method_t digi_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, digi_pci_probe),
	DEVMETHOD(device_attach, digi_pci_attach),
	DEVMETHOD(device_detach, digi_detach),
	DEVMETHOD(device_shutdown, digi_shutdown),
	{0, 0}
};

static driver_t digi_pci_drv = {
	"digi",
	digi_pci_methods,
	sizeof(struct digi_softc),
};
DRIVER_MODULE(digi, pci, digi_pci_drv, digi_devclass, 0, 0);
