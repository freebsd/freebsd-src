/*	$NecBSD: nsp_pisa.c,v 1.4 1999/04/15 01:35:54 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*-
 * [Ported for FreeBSD]
 *  Copyright (c) 2000
 *      Noriaki Mitsunaga, Mitsuru Iwasaki and Takanori Watanabe.
 *      All rights reserved.
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <compat/netbsd/dvcfg.h>

#include <sys/device_port.h>

#include <dev/pccard/pccardvar.h>

#include <cam/scsi/scsi_low.h>
#include <cam/scsi/scsi_low_pisa.h>

#include <dev/nsp/nspreg.h>
#include <dev/nsp/nspvar.h>

#define	NSP_HOSTID	7

#include "pccarddevs.h"

#define	PIO_MODE 0x100		/* pd_flags */

static int nspprobe(DEVPORT_PDEVICE devi);
static int nspattach(DEVPORT_PDEVICE devi);

static	void	nsp_card_unload	(DEVPORT_PDEVICE);

const struct pccard_product nsp_products[] = {
  	PCMCIA_CARD(IODATA3, CBSC16, 0),
  	PCMCIA_CARD(PANASONIC, KME, 0),
	PCMCIA_CARD(WORKBIT2, NINJA_SCSI3, 0),
	PCMCIA_CARD(WORKBIT, ULTRA_NINJA_16, 0),
  	{ NULL }
};

/*
 * Additional code for FreeBSD new-bus PCCard frontend
 */

static void
nsp_pccard_intr(void * arg)
{
	nspintr(arg);
}

static void
nsp_release_resource(DEVPORT_PDEVICE dev)
{
	struct nsp_softc	*sc = device_get_softc(dev);

	if (sc->nsp_intrhand)
		bus_teardown_intr(dev, sc->irq_res, sc->nsp_intrhand);
	if (sc->port_res)
		bus_release_resource(dev, SYS_RES_IOPORT,
				     sc->port_rid, sc->port_res);
	if (sc->irq_res)
		bus_release_resource(dev, SYS_RES_IRQ,
				     sc->irq_rid, sc->irq_res);
	if (sc->mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY,
				     sc->mem_rid, sc->mem_res);
}

static int
nsp_alloc_resource(DEVPORT_PDEVICE dev)
{
	struct nsp_softc	*sc = device_get_softc(dev);
	u_long			ioaddr, iosize, maddr, msize;
	int			error;

	error = bus_get_resource(dev, SYS_RES_IOPORT, 0, &ioaddr, &iosize);
	if (error || iosize < NSP_IOSIZE)
		return(ENOMEM);

	sc->port_rid = 0;
	sc->port_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->port_rid,
					  0, ~0, NSP_IOSIZE, RF_ACTIVE);
	if (sc->port_res == NULL) {
		nsp_release_resource(dev);
		return(ENOMEM);
	}

	sc->irq_rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irq_rid,
					     RF_ACTIVE);
	if (sc->irq_res == NULL) {
		nsp_release_resource(dev);
		return(ENOMEM);
	}

	error = bus_get_resource(dev, SYS_RES_MEMORY, 0, &maddr, &msize);
	if (error)
		return(0);	/* XXX */

	/* No need to allocate memory if not configured and it's in PIO mode */
	if (maddr == 0 || msize == 0) {
		if ((DEVPORT_PDEVFLAGS(dev) & PIO_MODE) == 0) {
			printf("Memory window was not configured. Configure or use in PIO mode.");
			nsp_release_resource(dev);
			return(ENOMEM);
		}
		/* no need to allocate memory if PIO mode */
		return(0);
	}

	sc->mem_rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->mem_rid,
					     RF_ACTIVE);
	if (sc->mem_res == NULL) {
		nsp_release_resource(dev);
		return(ENOMEM);
	}

	return(0);
}

static int nsp_pccard_match(device_t dev)
{
  	const struct pccard_product *pp;

	if ((pp = pccard_product_lookup(dev, nsp_products,
	    sizeof(nsp_products[0]), NULL)) != NULL) {
		if (pp->pp_name)
			device_set_desc(dev, pp->pp_name);
		return(0);
	}
	return(EIO);
}

static int
nsp_pccard_probe(DEVPORT_PDEVICE dev)
{
	int			error;

	error = nsp_alloc_resource(dev);
	if (error)
		return(error);
	if (nspprobe(dev) == 0) {
		nsp_release_resource(dev);
		return(ENXIO);
	}
	nsp_release_resource(dev);

	return(0);
}

static int
nsp_pccard_attach(DEVPORT_PDEVICE dev)
{
	struct nsp_softc	*sc = device_get_softc(dev);
	int			error;

	error = nsp_alloc_resource(dev);
	if (error)
		return(error);
	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_CAM | INTR_ENTROPY,
			       nsp_pccard_intr, (void *)sc, &sc->nsp_intrhand);
	if (error) {
		nsp_release_resource(dev);
		return(error);
	}
	if (nspattach(dev) == 0) {
		nsp_release_resource(dev);
		return(ENXIO);
	}

	return(0);
}

static void
nsp_pccard_detach(DEVPORT_PDEVICE dev)
{
	nsp_card_unload(dev);
	nsp_release_resource(dev);
}

static device_method_t nsp_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pccard_compat_probe),
	DEVMETHOD(device_attach,	pccard_compat_attach),
	DEVMETHOD(device_detach,	nsp_pccard_detach),

	/* Card interface */
	DEVMETHOD(card_compat_match,	nsp_pccard_match),
	DEVMETHOD(card_compat_probe,	nsp_pccard_probe),
	DEVMETHOD(card_compat_attach,	nsp_pccard_attach),

	{ 0, 0 }
};

static driver_t nsp_pccard_driver = {
	"nsp",
	nsp_pccard_methods,
	sizeof(struct nsp_softc),
};

static devclass_t nsp_devclass;

MODULE_DEPEND(nsp, scsi_low, 1, 1, 1);
DRIVER_MODULE(nsp, pccard, nsp_pccard_driver, nsp_devclass, 0, 0);

static void
nsp_card_unload(DEVPORT_PDEVICE devi)
{
	struct nsp_softc *sc = DEVPORT_PDEVGET_SOFTC(devi);
	intrmask_t s;

	s = splcam();
	scsi_low_deactivate((struct scsi_low_softc *)sc);
        scsi_low_dettach(&sc->sc_sclow);
	splx(s);
}

static	int
nspprobe(DEVPORT_PDEVICE devi)
{
	int rv;
	struct nsp_softc *sc = device_get_softc(devi);

	rv = nspprobesubr(rman_get_bustag(sc->port_res),
			  rman_get_bushandle(sc->port_res),
			  DEVPORT_PDEVFLAGS(devi));

	return rv;
}

static	int
nspattach(DEVPORT_PDEVICE devi)
{
	struct nsp_softc *sc;
	struct scsi_low_softc *slp;
	u_int32_t flags = DEVPORT_PDEVFLAGS(devi);
	u_int	iobase = DEVPORT_PDEVIOBASE(devi);
	intrmask_t s;
	char	dvname[16];

	strcpy(dvname,"nsp");

	if (iobase == 0) {
		printf("%s: no ioaddr is given\n", dvname);
		return (0);
	}

	sc = DEVPORT_PDEVALLOC_SOFTC(devi);
	if (sc == NULL)
		return (0);

	slp = &sc->sc_sclow;
	slp->sl_dev = devi;
	sc->sc_iot = rman_get_bustag(sc->port_res);
	sc->sc_ioh = rman_get_bushandle(sc->port_res);

	if (sc->mem_res == NULL) {
		printf("WARNING: CANNOT GET Memory RESOURCE going PIO mode");
		flags |= PIO_MODE;
	}

	if ((flags & PIO_MODE) == 0) {
		sc->sc_memt = rman_get_bustag(sc->mem_res);
		sc->sc_memh = rman_get_bushandle(sc->mem_res);
	} else {
		sc->sc_memh = 0;
	}
	/* slp->sl_irq = devi->pd_irq; */
	sc->sc_iclkdiv = CLKDIVR_20M;
	sc->sc_clkdiv = CLKDIVR_40M;

	slp->sl_hostid = NSP_HOSTID;
	slp->sl_cfgflags = flags;

	s = splcam();
	nspattachsubr(sc);
	splx(s);

	return(NSP_IOSIZE);
}
