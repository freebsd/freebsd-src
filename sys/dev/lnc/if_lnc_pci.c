/*-
 * Copyright (c) 1994-2000
 *	Paul Richards. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name Paul Richards may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL RICHARDS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PAUL RICHARDS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/lnc/if_lncreg.h>
#include <dev/lnc/if_lncvar.h>

#define AMD_VENDOR_ID 0x1022
#define PCI_DEVICE_ID_PCNet_PCI	0x2000
#define PCI_DEVICE_ID_PCHome_PCI 0x2001

#define LNC_PROBE_PRIORITY -1

static int
lnc_pci_probe(device_t dev)
{
	if (pci_get_vendor(dev) != AMD_VENDOR_ID)
		return (ENXIO);

	switch(pci_get_device(dev)) {
	case PCI_DEVICE_ID_PCNet_PCI:
		device_set_desc(dev, "PCNet/PCI Ethernet adapter");
		return(LNC_PROBE_PRIORITY);
		break;
	case PCI_DEVICE_ID_PCHome_PCI:
		device_set_desc(dev, "PCHome/PCI Ethernet adapter");
		return(LNC_PROBE_PRIORITY);
		break;
	default:
		return (ENXIO);
		break;
	}
	return (ENXIO);
}

static void
lnc_alloc_callback(void *arg, bus_dma_segment_t *seg, int nseg, int error)
{
	/* Do nothing */
	return;
}

static int
lnc_pci_attach(device_t dev)
{
	lnc_softc_t *sc = device_get_softc(dev);
	unsigned command;
	int rid = 0;
	int err = 0;
	bus_size_t lnc_mem_size;

	device_printf(dev, "Attaching %s\n", device_get_desc(dev));

	command = pci_read_config(dev, PCIR_COMMAND, 4);
	command |= PCIM_CMD_PORTEN | PCIM_CMD_BUSMASTEREN;
	pci_write_config(dev, PCIR_COMMAND, command, 4);

	rid = PCIR_MAPS;
	sc->portres = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1,
	                                 RF_ACTIVE);

	if (! sc->portres)
		device_printf(dev, "Cannot allocate I/O ports\n");

	rid = 0;
	sc->irqres = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	                                RF_ACTIVE|RF_SHAREABLE);

	if (! sc->irqres)
		device_printf(dev, "Cannot allocate irq\n");

	err = bus_setup_intr(dev, sc->irqres, INTR_TYPE_NET, lncintr,
	                     sc, &sc->intrhand);
	if (err)
		device_printf(dev, "Cannot setup irq handler\n");

	sc->lnc_btag = rman_get_bustag(sc->portres);
	sc->lnc_bhandle = rman_get_bushandle(sc->portres);

	/* XXX temp setting for nic */
	sc->nic.ic = PCnet_PCI;
	sc->nic.ident = NE2100;
	sc->nic.mem_mode = DMA_FIXED;
	sc->nrdre  = NRDRE;
	sc->ntdre  = NTDRE;
	sc->rap = PCNET_RAP;
	sc->rdp = PCNET_RDP;
	sc->bdp = PCNET_BDP;

	/* Create a DMA tag describing the ring memory we need */

	lnc_mem_size = ((NDESC(sc->nrdre) + NDESC(sc->ntdre)) *
			 sizeof(struct host_ring_entry));

	lnc_mem_size += sizeof(struct init_block) + (sizeof(struct mds) *
			(NDESC(sc->nrdre) + NDESC(sc->ntdre))) + MEM_SLEW;

	lnc_mem_size += (NDESC(sc->nrdre) * RECVBUFSIZE) +
			(NDESC(sc->ntdre) * TRANSBUFSIZE);

	err = bus_dma_tag_create(NULL,			/* parent */
				 1,			/* alignement */
				 0,			/* boundary */
				 BUS_SPACE_MAXADDR,	/* lowaddr */
				 BUS_SPACE_MAXADDR,	/* highaddr */
				 NULL, NULL,		/* filter, filterarg */
				 lnc_mem_size,		/* segsize */
				 1,			/* nsegments */
				 BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
				 0,			/* flags */
				 busdma_lock_mutex,	/* lockfunc */
				 &Giant,		/* lockarg */
				 &sc->dmat);

	if (err) {
		device_printf(dev, "Can't create DMA tag\n");
		/* XXX need to free currently allocated resources here */
		return (ENOMEM);
	}

	err = bus_dmamem_alloc(sc->dmat, (void **)&sc->recv_ring,
	                       BUS_DMA_NOWAIT, &sc->dmamap);

	if (err) {
		device_printf(dev, "Couldn't allocate memory\n");
		/* XXX need to free currently allocated resources here */
		return (ENOMEM);
	}

	bus_dmamap_load(sc->dmat, sc->dmamap, sc->recv_ring, lnc_mem_size,
			lnc_alloc_callback, sc->recv_ring, BUS_DMA_NOWAIT);

	/* Call generic attach code */
	if (! lnc_attach_common(dev)) {
		device_printf(dev, "Generic attach code failed\n");
	}
	return (0);
}

static int
lnc_pci_detach(device_t dev)
{
	lnc_softc_t *sc = device_get_softc(dev);
	int s = splimp();

	ether_ifdetach(&sc->arpcom.ac_if);

	lnc_stop(sc);
	bus_teardown_intr(dev, sc->irqres, sc->intrhand);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irqres);
	bus_release_resource(dev, SYS_RES_IOPORT, PCIR_MAPS, sc->portres);

	bus_dmamap_unload(sc->dmat, sc->dmamap);
	bus_dmamem_free(sc->dmat, sc->recv_ring, sc->dmamap);
	bus_dma_tag_destroy(sc->dmat);

	splx(s);
	return (0);
}

static device_method_t lnc_pci_methods[] = {
	DEVMETHOD(device_probe,		lnc_pci_probe),
	DEVMETHOD(device_attach,	lnc_pci_attach),
	DEVMETHOD(device_detach,	lnc_pci_detach),
#ifdef notyet
	DEVMETHOD(device_suspend,	lnc_pci_suspend),
	DEVMETHOD(device_resume,	lnc_pci_resume),
	DEVMETHOD(device_shutdown,	lnc_pci_shutdown),
#endif
	{ 0, 0 }
};

static driver_t lnc_pci_driver = {
	"lnc",
	lnc_pci_methods,
	sizeof(struct lnc_softc),
};

DRIVER_MODULE(lnc, pci, lnc_pci_driver, lnc_devclass, 0, 0);
MODULE_DEPEND(lnc, pci, 1, 1, 1);
MODULE_DEPEND(lnc, ether, 1, 1, 1);
