/*	$FreeBSD$	*/
/*	$NecBSD: nsp_pisa.c,v 1.4 1999/04/15 01:35:54 kmatsuda Exp $	*/
/*	$NetBSD$	*/

/*
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/disklabel.h>
#if defined(__FreeBSD__) && __FreeBSD_version >= 500001
#include <sys/bio.h>
#endif
#include <sys/buf.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/errno.h>

#include <vm/vm.h>

#include <machine/bus.h>
#include <i386/isa/isa_device.h>

#include <machine/dvcfg.h>

#if defined(__FreeBSD__) && __FreeBSD_version < 400001
static struct nsp_softc *nsp_get_softc(int);
extern struct nsp_softc *nspdata[];
#define DEVPORT_ALLOCSOFTCFUNC	nsp_get_softc
#define DEVPORT_SOFTCARRAY	nspdata
#endif
#include <sys/device_port.h>

#include <cam/scsi/scsi_low.h>
#include <cam/scsi/scsi_low_pisa.h>

#include <dev/nsp/nspreg.h>
#include <dev/nsp/nspvar.h>
#if defined(__NetBSD__) || (defined(__FreeBSD__) && __FreeBSD_version < 400001)
#include "nsp.h"
#endif

#define	NSP_HOSTID	7

/* pccard support */
#include	"card.h"
#if NCARD > 0
#include	<sys/kernel.h>
#include	<sys/module.h>
#if !defined(__FreeBSD__) || __FreeBSD_version < 500014
#include	<sys/select.h>
#endif
#include 	<pccard/cardinfo.h>
#include	<pccard/slot.h>

#define	PIO_MODE 0x100		/* pd_flags */

static int nspprobe(DEVPORT_PDEVICE devi);
static int nspattach(DEVPORT_PDEVICE devi);

static	void	nsp_card_unload	__P((DEVPORT_PDEVICE));
#if defined(__FreeBSD__) && __FreeBSD_version < 400001
static	int	nsp_card_init	__P((DEVPORT_PDEVICE));
static	int	nsp_card_intr	__P((DEVPORT_PDEVICE));
#endif

#if defined(__FreeBSD__) && __FreeBSD_version >= 400001
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

	if (sc->nsp_intrhand) {
		bus_teardown_intr(dev, sc->irq_res, sc->nsp_intrhand);
	}

	if (sc->port_res) {
		bus_release_resource(dev, SYS_RES_IOPORT,
				     sc->port_rid, sc->port_res);
	}

	if (sc->irq_res) {
		bus_release_resource(dev, SYS_RES_IRQ,
				     sc->irq_rid, sc->irq_res);
	}

	if (sc->mem_res) {
		bus_release_resource(dev, SYS_RES_MEMORY,
				     sc->mem_rid, sc->mem_res);
	}
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
	sc->irq_res = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irq_rid,
					 0, ~0, 1, RF_ACTIVE);
	if (sc->irq_res == NULL) {
		nsp_release_resource(dev);
		return(ENOMEM);
	}

	error = bus_get_resource(dev, SYS_RES_MEMORY, 0, &maddr, &msize);
	if (error) {
		return(0);	/* XXX */
	}

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
	sc->mem_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->mem_rid,
					 0, ~0, 1, RF_ACTIVE);
	if (sc->mem_res == NULL) {
		nsp_release_resource(dev);
		return(ENOMEM);
	}

	return(0);
}

static int
nsp_pccard_probe(DEVPORT_PDEVICE dev)
{
	struct nsp_softc	*sc = device_get_softc(dev);
	int			error;

	bzero(sc, sizeof(struct nsp_softc));

	error = nsp_alloc_resource(dev);
	if (error) {
		return(error);
	}

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
	if (error) {
		return(error);
	}

	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_CAM,
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

static	void
nsp_pccard_detach(DEVPORT_PDEVICE dev)
{
	nsp_card_unload(dev);
	nsp_release_resource(dev);
}

static device_method_t nsp_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nsp_pccard_probe),
	DEVMETHOD(device_attach,	nsp_pccard_attach),
	DEVMETHOD(device_detach,	nsp_pccard_detach),

	{ 0, 0 }
};

static driver_t nsp_pccard_driver = {
	"nsp",
	nsp_pccard_methods,
	sizeof(struct nsp_softc),
};

static devclass_t nsp_devclass;

DRIVER_MODULE(nsp, pccard, nsp_pccard_driver, nsp_devclass, 0, 0);

#else

PCCARD_MODULE(nsp, nsp_card_init,nsp_card_unload, nsp_card_intr,0, cam_imask);

#endif

#if defined(__FreeBSD__) && __FreeBSD_version < 400001
static struct nsp_softc *
nsp_get_softc(int unit)
{
	struct nsp_softc *sc;

	if (unit >= NNSP) {
		return(NULL);
	}

	if (nspdata[unit] == NULL) {
		sc = malloc(sizeof(struct nsp_softc), M_TEMP,M_NOWAIT);
		if (sc == NULL) {
			printf("nsp_get_softc: cannot malloc!\n");
			return(NULL);
		}
		nspdata[unit] = sc;
	} else {
		sc = nspdata[unit];
	}

	return(sc);
}

static	int
nsp_card_init(DEVPORT_PDEVICE devi)
{
	int unit = DEVPORT_PDEVUNIT(devi);

	if (NNSP <= unit)
		return (ENODEV);

	if (nspprobe(devi) == 0)
		return (ENXIO);

	if (nspattach(devi) == 0)
		return (ENXIO);
	
	return (0);
}

static	int
nsp_card_intr(DEVPORT_PDEVICE devi)
{
	nspintr(DEVPORT_PDEVGET_SOFTC(devi));
	return 1;
}
#endif

static	void
nsp_card_unload(DEVPORT_PDEVICE devi)
{
	struct nsp_softc *sc = DEVPORT_PDEVGET_SOFTC(devi);
	intrmask_t s;

	printf("%s: unload\n",sc->sc_sclow.sl_xname);
	s = splcam();
	scsi_low_deactivate((struct scsi_low_softc *)sc);
        scsi_low_dettach(&sc->sc_sclow);
	splx(s);
}

static	int
nspprobe(DEVPORT_PDEVICE devi)
{
	int rv;
#if defined(__FreeBSD__) && __FreeBSD_version >= 400001
	struct nsp_softc *sc = device_get_softc(devi);

	rv = nspprobesubr(rman_get_bustag(sc->port_res),
			  rman_get_bushandle(sc->port_res),
			  DEVPORT_PDEVFLAGS(devi));
#else
	rv = nspprobesubr(I386_BUS_SPACE_IO,
			  DEVPORT_PDEVIOBASE(devi), DEVPORT_PDEVFLAGS(devi));
#endif

	return rv;
}

static	int
nspattach(DEVPORT_PDEVICE devi)
{
#if defined(__FreeBSD__) && __FreeBSD_version < 400001
	int unit = DEVPORT_PDEVUNIT(devi);
#endif
	struct nsp_softc *sc;
	struct scsi_low_softc *slp;
	u_int32_t flags = DEVPORT_PDEVFLAGS(devi);
	u_int	iobase = DEVPORT_PDEVIOBASE(devi);
	intrmask_t s;
	char	dvname[16];

	strcpy(dvname,"nsp");

#if defined(__FreeBSD__) && __FreeBSD_version < 400001
	if (unit >= NNSP)
	{
		printf("%s: unit number too high\n",dvname);
		return(0);
	}
#endif

	if (iobase == 0)
	{
		printf("%s: no ioaddr is given\n", dvname);
		return (0);
	}

	sc = DEVPORT_PDEVALLOC_SOFTC(devi);
	if (sc == NULL) {
		return (0);
	}

	slp = &sc->sc_sclow;
#if defined(__FreeBSD__) && __FreeBSD_version >= 400001
	slp->sl_dev = devi;
	sc->sc_iot = rman_get_bustag(sc->port_res);
	sc->sc_ioh = rman_get_bushandle(sc->port_res);
#else
	bzero(sc, sizeof(struct nsp_softc));
	strcpy(slp->sl_dev.dv_xname, dvname);
	slp->sl_dev.dv_unit = unit;
	sc->sc_iot = I386_BUS_SPACE_IO;
	sc->sc_ioh = iobase;
#endif

	if((flags & PIO_MODE) == 0) {
#if defined(__FreeBSD__) && __FreeBSD_version >= 400001
		sc->sc_memt = rman_get_bustag(sc->mem_res);
		sc->sc_memh = rman_get_bushandle(sc->mem_res);
#else
		sc->sc_memt = I386_BUS_SPACE_MEM;
		sc->sc_memh = (bus_space_handle_t)DEVPORT_PDEVMADDR(devi);
#endif
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
#endif /* NCARD>0 */
