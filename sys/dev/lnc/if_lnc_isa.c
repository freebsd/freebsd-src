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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/socket.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>

#include <isa/isavar.h>

#include <dev/lnc/if_lncvar.h>
#include <dev/lnc/if_lncreg.h>

static struct isa_pnp_id lnc_pnp_ids[] = {
	{0,	NULL}
};

static int
lnc_legacy_probe(device_t dev)
{
	struct lnc_softc *sc = device_get_softc(dev);

	sc->portrid = 0;
	sc->portres = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &sc->portrid,
					     RF_ACTIVE);

	if (! sc->portres) {
		device_printf(dev, "Failed to allocate I/O ports\n");
		lnc_release_resources(dev);
		return (ENXIO);
	}

	sc->lnc_btag = rman_get_bustag(sc->portres);
	sc->lnc_bhandle = rman_get_bushandle(sc->portres);

	/*
	 * There isn't any way to determine if a NIC is a BICC. Basically, if
	 * the lance probe succeeds using the i/o addresses of the BICC then
	 * we assume it's a BICC.
	 *
	 */
	sc->rap = BICC_RAP;
	sc->rdp = BICC_RDP;
	sc->nic.mem_mode = DMA_FIXED;
	/* XXX Should set BICC_IOSIZE et al somewhere to alloc
	   resources correctly */

	if ((sc->nic.ic = lance_probe(sc))) {
		device_set_desc(dev, "BICC Isolan");
		sc->nic.ident = BICC;
		lnc_release_resources(dev);
		return (0);
	} else {
	    /* It's not a BICC so try the standard NE2100 ports */
	    sc->rap = PCNET_RAP;
	    sc->rdp = PCNET_RDP;
	    if ((sc->nic.ic = lance_probe(sc))) {
		sc->nic.ident = NE2100;
		device_set_desc(dev, "NE2100");
		lnc_release_resources(dev);
		return (0);
	    } else {
		lnc_release_resources(dev);
		return (ENXIO);
	    }
	}
}

static int
lnc_isa_probe(device_t dev)
{
	int pnp;

	pnp = ISA_PNP_PROBE(device_get_parent(dev), dev, lnc_pnp_ids);
	if (pnp == ENOENT) {
		/* It's not a PNP card, see if we support it by probing it */
		return (lnc_legacy_probe(dev));
	} else if (pnp == ENXIO) {
		return (ENXIO);
	} else {
		/* Found PNP card we support */
		return (0);
	}
}

static void
lnc_alloc_callback(void *arg, bus_dma_segment_t *seg, int nseg, int error)
{
	/* Do nothing */
	return;
}

static int
lnc_isa_attach(device_t dev)
{
	lnc_softc_t *sc = device_get_softc(dev);
	int err = 0;
	bus_size_t lnc_mem_size;

	device_printf(dev, "Attaching %s\n", device_get_desc(dev));

	sc->portrid = 0;
	sc->portres = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &sc->portrid,
					     RF_ACTIVE);

	if (! sc->portres) {
		device_printf(dev, "Failed to allocate I/O ports\n");
		lnc_release_resources(dev);
		return (ENXIO);
	}

	sc->drqrid = 0;
	sc->drqres = bus_alloc_resource_any(dev, SYS_RES_DRQ, &sc->drqrid,
					    RF_ACTIVE);

	if (! sc->drqres) {
		device_printf(dev, "Failed to allocate DMA channel\n");
		lnc_release_resources(dev);
		return (ENXIO);
	}

	if (isa_get_irq(dev) == -1)
		bus_set_resource(dev, SYS_RES_IRQ, 0, 10, 1);

	sc->irqrid = 0;
	sc->irqres = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irqrid,
					    RF_ACTIVE);

	if (! sc->irqres) {
		device_printf(dev, "Failed to allocate irq\n");
		lnc_release_resources(dev);
		return (ENXIO);
	}

	err = bus_setup_intr(dev, sc->irqres, INTR_TYPE_NET, lncintr,
	                     sc, &sc->intrhand);

	if (err) {
		device_printf(dev, "Failed to setup irq handler\n");
		lnc_release_resources(dev);
		return (err);
	}

	/* XXX temp setting for nic */
	sc->nic.mem_mode = DMA_FIXED;
	sc->nrdre  = NRDRE;
	sc->ntdre  = NTDRE;

	if (sc->nic.ident == NE2100) {
	    sc->rap = PCNET_RAP;
	    sc->rdp = PCNET_RDP;
	    sc->bdp = PCNET_BDP;
	} else {
	    sc->rap = BICC_RAP;
	    sc->rdp = BICC_RDP;
	}

	/* Create a DMA tag describing the ring memory we need */

	lnc_mem_size = ((NDESC(sc->nrdre) + NDESC(sc->ntdre)) *
			 sizeof(struct host_ring_entry));

	lnc_mem_size += (NDESC(sc->nrdre) * RECVBUFSIZE) +
			(NDESC(sc->ntdre) * TRANSBUFSIZE);

	err = bus_dma_tag_create(NULL,			/* parent */
				 4,			/* alignement */
				 0,			/* boundary */
				 BUS_SPACE_MAXADDR_24BIT,	/* lowaddr */
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
		lnc_release_resources(dev);
		return (ENOMEM);
	}

	err = bus_dmamem_alloc(sc->dmat, (void **)&sc->recv_ring,
	                       BUS_DMA_NOWAIT, &sc->dmamap);

	if (err) {
		device_printf(dev, "Couldn't allocate memory\n");
		lnc_release_resources(dev);
		return (ENOMEM);
	}

	err = bus_dmamap_load(sc->dmat, sc->dmamap, sc->recv_ring, lnc_mem_size,
			lnc_alloc_callback, sc->recv_ring, BUS_DMA_NOWAIT);

	if (err) {
		device_printf(dev, "Couldn't load DMA map\n");
		lnc_release_resources(dev);
		return (ENOMEM);
	}

	isa_dmacascade(rman_get_start(sc->drqres));

	/* Call generic attach code */
	if (! lnc_attach_common(dev)) {
		device_printf(dev, "Generic attach code failed\n");
		lnc_release_resources(dev);
		return (ENXIO);
	}
	return (0);
}

static int
lnc_isa_detach(device_t dev)
{
	lnc_softc_t *sc = device_get_softc(dev);
	int s = splimp();

	ether_ifdetach(&sc->arpcom.ac_if);
	splx(s);

	lnc_stop(sc);
	lnc_release_resources(dev);

	return (0);
}

static device_method_t lnc_isa_methods[] = {
/*	DEVMETHOD(device_identify,	lnc_isa_identify), */
	DEVMETHOD(device_probe,		lnc_isa_probe),
	DEVMETHOD(device_attach,	lnc_isa_attach),
	DEVMETHOD(device_detach,	lnc_isa_detach),
#ifdef notyet
	DEVMETHOD(device_suspend,	lnc_isa_suspend),
	DEVMETHOD(device_resume,	lnc_isa_resume),
	DEVMETHOD(device_shutdown,	lnc_isa_shutdown),
#endif
	{ 0, 0 }
};

static driver_t lnc_isa_driver = {
	"lnc",
	lnc_isa_methods,
	sizeof(struct lnc_softc),
};

DRIVER_MODULE(lnc, isa, lnc_isa_driver, lnc_devclass, 0, 0);
MODULE_DEPEND(lnc, isa, 1, 1, 1);
MODULE_DEPEND(lnc, ether, 1, 1, 1);
