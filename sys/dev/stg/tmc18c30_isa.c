/*	$FreeBSD$	*/
/*	$NecBSD: tmc18c30_pisa.c,v 1.22 1998/11/26 01:59:21 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [Ported for FreeBSD]
 *  Copyright (c) 2000
 *      Noriaki Mitsunaga, Mitsuru Iwasaki and Takanori Watanabe.
 *      All rights reserved.
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1996, 1997, 1998
 *	Naofumi HONDA. All rights reserved.
 *  Copyright (c) 1996, 1997, 1998
 *	Kouichi Matsuda. All rights reserved.
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
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/disklabel.h>
#if defined(__FreeBSD__) && __FreeBSD_version >= 500001
#include <sys/bio.h>
#endif
#include <sys/buf.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/errno.h>

#include <vm/vm.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <isa/isavar.h>

#include <machine/dvcfg.h>

#include <sys/device_port.h>

#include <cam/scsi/scsi_low.h>
#include <isa/isa_common.h>
#include <cam/scsi/scsi_low_pisa.h>

#include <dev/stg/tmc18c30reg.h>
#include <dev/stg/tmc18c30var.h>

#define	STG_HOSTID	7

#include	<sys/kernel.h>
#include	<sys/module.h>
#include	<sys/select.h>

static	int	stgprobe(device_t devi);
static	int	stgattach(device_t devi);

static	void	stg_isa_unload	__P((device_t));

static void
stg_isa_intr(void * arg)
{
	stgintr(arg);
}

static void
stg_release_resource(device_t dev)
{
	struct stg_softc	*sc = device_get_softc(dev);

	if (sc->stg_intrhand) {
		bus_teardown_intr(dev, sc->irq_res, sc->stg_intrhand);
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
stg_alloc_resource(device_t dev)
{
	struct stg_softc	*sc = device_get_softc(dev);
	u_long			maddr, msize;
	int			error;

	sc->port_rid = 0;
	sc->port_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->port_rid,
					  0, ~0, STGIOSZ, RF_ACTIVE);
	if (sc->port_res == NULL) {
		stg_release_resource(dev);
		return(ENOMEM);
	}

	sc->irq_rid = 0;
	sc->irq_res = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irq_rid,
					 0, ~0, 1, RF_ACTIVE);
	if (sc->irq_res == NULL) {
		stg_release_resource(dev);
		return(ENOMEM);
	}

	error = bus_get_resource(dev, SYS_RES_MEMORY, 0, &maddr, &msize);
	if (error) {
		return(0);      /* XXX */
	}

	/* no need to allocate memory if not configured */
	if (maddr == 0 || msize == 0) {
		return(0);
	}

	sc->mem_rid = 0;
	sc->mem_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->mem_rid,
					 0, ~0, msize, RF_ACTIVE);
	if (sc->mem_res == NULL) {
		stg_release_resource(dev);
		return(ENOMEM);
	}

	return(0);
}

static int
stg_isa_probe(device_t dev)
{
	struct stg_softc	*sc = device_get_softc(dev);
	int			error;

	bzero(sc, sizeof(struct stg_softc));

	error = stg_alloc_resource(dev);
	if (error) {
		return(error);
	}

	if (stgprobe(dev) == 0) {
		stg_release_resource(dev);
		return(ENXIO);
	}

	stg_release_resource(dev);

	return(0);
}

static int
stg_isa_attach(device_t dev)
{
	struct stg_softc	*sc = device_get_softc(dev);
	int			error;

	error = stg_alloc_resource(dev);
	if (error) {
		return(error);
	}

	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_CAM,
			       stg_isa_intr, (void *)sc, &sc->stg_intrhand);
	if (error) {
		stg_release_resource(dev);
		return(error);
	}

	if (stgattach(dev) == 0) {
		stg_release_resource(dev);
		return(ENXIO);
	}

	return(0);
}

static	void
stg_isa_detach(device_t dev)
{
	stg_isa_unload(dev);
	stg_release_resource(dev);
}

static device_method_t stg_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		stg_isa_probe),
	DEVMETHOD(device_attach,	stg_isa_attach),
	DEVMETHOD(device_detach,	stg_isa_detach),

	{ 0, 0 }
};

static driver_t stg_isa_driver = {
	"stg",
	stg_isa_methods,
	sizeof(struct stg_softc),
};

static devclass_t stg_devclass;

DRIVER_MODULE(stg, isa, stg_isa_driver, stg_devclass, 0, 0);

static	void
stg_isa_unload(device_t devi)
{
	struct stg_softc *sc = device_get_softc(devi);

	printf("%s: unload\n",sc->sc_sclow.sl_xname);
	scsi_low_deactivate((struct scsi_low_softc *)sc);
	scsi_low_dettach(&sc->sc_sclow);
}

static	int
stgprobe(device_t devi)
{
	int rv;
	struct stg_softc *sc = device_get_softc(devi);

	rv = stgprobesubr(rman_get_bustag(sc->port_res),
			  rman_get_bushandle(sc->port_res),
			  device_get_flags(devi));

	return rv;
}

static	int
stgattach(device_t devi)
{
	struct stg_softc *sc;
	struct scsi_low_softc *slp;
	u_int32_t flags = device_get_flags(devi);
	u_int iobase = bus_get_resource_start(devi, SYS_RES_IOPORT, 0);

	char	dvname[16];

	strcpy(dvname,"stg");


	if (iobase == 0)
	{
		printf("%s: no ioaddr is given\n", dvname);
		return (0);
	}

	sc = device_get_softc(devi);
	if (sc == NULL) {
		return(0);
	}

	slp = &sc->sc_sclow;
	slp->sl_dev = devi;
	sc->sc_iot = rman_get_bustag(sc->port_res);
	sc->sc_ioh = rman_get_bushandle(sc->port_res);

	slp->sl_hostid = STG_HOSTID;
	slp->sl_cfgflags = flags;

	stgattachsubr(sc);

	sc->sc_ih = stgintr;

	printf("stg%d",device_get_unit(devi));
	return(STGIOSZ);
}
