/*	$FreeBSD$	*/
/*	$NecBSD: ncr53c500_pisa.c,v 1.28 1998/11/26 01:59:11 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [Ported for FreeBSD]
 *  Copyright (c) 2000
 *      Noriaki Mitsunaga, Mitsuru Iwasaki and Takanori Watanabe.
 *      All rights reserved.
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1995, 1996, 1997, 1998
 *	Naofumi HONDA. All rights reserved.
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
#include <machine/bus_pio.h>
#include <machine/dvcfg.h>

#include <sys/device_port.h>

#include <cam/scsi/scsi_low.h>
#include <cam/scsi/scsi_low_pisa.h>

#include <dev/ncv/ncr53c500reg.h>
#include <dev/ncv/ncr53c500hw.h>
#include <dev/ncv/ncr53c500var.h>

#define KME_KXLC004_01 0x100
#define OFFSET_KME_KXLC004_01 0x10

#include	<sys/kernel.h>
#include	<sys/module.h>
#if !defined(__FreeBSD__) || __FreeBSD_version < 500014
#include	<sys/select.h>
#endif
#include	<pccard/cardinfo.h>
#include	<pccard/slot.h>

static int ncvprobe(DEVPORT_PDEVICE devi);
static int ncvattach(DEVPORT_PDEVICE devi);

static void	ncv_card_unload __P((DEVPORT_PDEVICE));

/*
 * Additional code for FreeBSD new-bus PCCard frontend
 */

static void
ncv_pccard_intr(void * arg)
{
	ncvintr(arg);
}

static void
ncv_release_resource(DEVPORT_PDEVICE dev)
{
	struct ncv_softc	*sc = device_get_softc(dev);

	if (sc->ncv_intrhand) {
		bus_teardown_intr(dev, sc->irq_res, sc->ncv_intrhand);
	}

	if (sc->port_res) {
		bus_release_resource(dev, SYS_RES_IOPORT,
				     sc->port_rid, sc->port_res);
	}

	if (sc->port_res_dmy) {
		bus_release_resource(dev, SYS_RES_IOPORT,
				     sc->port_rid_dmy, sc->port_res_dmy);
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
ncv_alloc_resource(DEVPORT_PDEVICE dev)
{
	struct ncv_softc	*sc = device_get_softc(dev);
	u_int32_t		flags = DEVPORT_PDEVFLAGS(dev);
	u_long			ioaddr, iosize, maddr, msize;
	int			error;
	bus_addr_t		offset = 0;

	if(flags & KME_KXLC004_01)
		offset = OFFSET_KME_KXLC004_01;

	error = bus_get_resource(dev, SYS_RES_IOPORT, 0, &ioaddr, &iosize);
	if (error || (iosize < (offset + NCVIOSZ))) {
		return(ENOMEM);
	}

	sc->port_rid = 0;
	sc->port_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->port_rid,
					  ioaddr+offset, ioaddr+iosize-offset,
					  iosize-offset, RF_ACTIVE);
	if (sc->port_res == NULL) {
		ncv_release_resource(dev);
		return(ENOMEM);
	}

	if (offset != 0) {
		sc->port_rid_dmy = 0;
		sc->port_res_dmy = bus_alloc_resource(dev, SYS_RES_IOPORT, 
						&sc->port_rid_dmy,
						ioaddr, ioaddr+offset, offset, 
						RF_ACTIVE);
		if (sc->port_res_dmy == NULL) {
			printf("Warning: cannot allocate IOPORT partially.\n");
		}
	} else {
		sc->port_rid_dmy = 0;
		sc->port_res_dmy = NULL;
	}

	sc->irq_rid = 0;
	sc->irq_res = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irq_rid,
					 0, ~0, 1, RF_ACTIVE);
	if (sc->irq_res == NULL) {
		ncv_release_resource(dev);
		return(ENOMEM);
	}

	error = bus_get_resource(dev, SYS_RES_MEMORY, 0, &maddr, &msize);
	if (error) {
		return(0);	/* XXX */
	}

	/* no need to allocate memory if not configured */
	if (maddr == 0 || msize == 0) {
		return(0);
	}

	sc->mem_rid = 0;
	sc->mem_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->mem_rid,
					 0, ~0, 1, RF_ACTIVE);
	if (sc->mem_res == NULL) {
		ncv_release_resource(dev);
		return(ENOMEM);
	}

	return(0);
}

static int
ncv_pccard_probe(DEVPORT_PDEVICE dev)
{
	struct ncv_softc	*sc = device_get_softc(dev);
	int			error;

	bzero(sc, sizeof(struct ncv_softc));

	error = ncv_alloc_resource(dev);
	if (error) {
		return(error);
	}

	if (ncvprobe(dev) == 0) {
		ncv_release_resource(dev);
		return(ENXIO);
	}

	ncv_release_resource(dev);

	return(0);
}

static int
ncv_pccard_attach(DEVPORT_PDEVICE dev)
{
	struct ncv_softc	*sc = device_get_softc(dev);
	int			error;

	error = ncv_alloc_resource(dev);
	if (error) {
		return(error);
	}

	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_CAM | INTR_ENTROPY,
			       ncv_pccard_intr, (void *)sc, &sc->ncv_intrhand);
	if (error) {
		ncv_release_resource(dev);
		return(error);
	}

	if (ncvattach(dev) == 0) {
		ncv_release_resource(dev);
		return(ENXIO);
	}

	return(0);
}

static	void
ncv_pccard_detach(DEVPORT_PDEVICE dev)
{
	ncv_card_unload(dev);
	ncv_release_resource(dev);
}

static device_method_t ncv_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ncv_pccard_probe),
	DEVMETHOD(device_attach,	ncv_pccard_attach),
	DEVMETHOD(device_detach,	ncv_pccard_detach),

	{ 0, 0 }
};

static driver_t ncv_pccard_driver = {
	"ncv",
	ncv_pccard_methods,
	sizeof(struct ncv_softc),
};

static devclass_t ncv_devclass;

MODULE_DEPEND(ncv, scsi_low, 1, 1, 1);
DRIVER_MODULE(ncv, pccard, ncv_pccard_driver, ncv_devclass, 0, 0);

static void
ncv_card_unload(DEVPORT_PDEVICE devi)
{
	struct ncv_softc *sc = DEVPORT_PDEVGET_SOFTC(devi);
	intrmask_t s;

	printf("%s: unload\n", sc->sc_sclow.sl_xname);
	s = splcam();
	scsi_low_deactivate((struct scsi_low_softc *)sc);
        scsi_low_dettach(&sc->sc_sclow);
	splx(s);
}

static int
ncvprobe(DEVPORT_PDEVICE devi)
{
	int rv;
	struct ncv_softc *sc = device_get_softc(devi);
	u_int32_t flags = DEVPORT_PDEVFLAGS(devi);

	rv = ncvprobesubr(rman_get_bustag(sc->port_res),
			  rman_get_bushandle(sc->port_res),
			  flags, NCV_HOSTID);

	return rv;
}

static int
ncvattach(DEVPORT_PDEVICE devi)
{
	struct ncv_softc *sc;
	struct scsi_low_softc *slp;
	u_int32_t flags = DEVPORT_PDEVFLAGS(devi);
	intrmask_t s;
	char dvname[16]; /* SCSI_LOW_DVNAME_LEN */

	strcpy(dvname, "ncv");

	sc = DEVPORT_PDEVALLOC_SOFTC(devi);
	if (sc == NULL) {
		return(0);
	}

	slp = &sc->sc_sclow;
	slp->sl_dev = devi;
	sc->sc_iot = rman_get_bustag(sc->port_res);
	sc->sc_ioh = rman_get_bushandle(sc->port_res);

	slp->sl_hostid = NCV_HOSTID;
	slp->sl_cfgflags = flags;

	s = splcam();
	ncvattachsubr(sc);
	splx(s);

	return(NCVIOSZ);
}
