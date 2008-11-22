/*-
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h> 

#include <cam/scsi/scsi_low.h>
#include <cam/scsi/scsi_low_pisa.h>

#include <dev/stg/tmc18c30reg.h>
#include <dev/stg/tmc18c30var.h>
#include <dev/stg/tmc18c30.h>

#define	STG_HOSTID	7

devclass_t stg_devclass;

int
stg_alloc_resource(device_t dev)
{
	struct stg_softc *	sc = device_get_softc(dev);
	u_long			maddr, msize;
	int			error;

	sc->port_res = bus_alloc_resource_any(dev, SYS_RES_IOPORT, 
					      &sc->port_rid, RF_ACTIVE);
	if (sc->port_res == NULL) {
		stg_release_resource(dev);
		return(ENOMEM);
	}

	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sc->irq_rid,
					     RF_ACTIVE);
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

	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->mem_rid,
					     RF_ACTIVE);
	if (sc->mem_res == NULL) {
		stg_release_resource(dev);
		return(ENOMEM);
	}

	return(0);
}

void
stg_release_resource(device_t dev)
{
	struct stg_softc	*sc = device_get_softc(dev);

	if (sc->stg_intrhand)
		bus_teardown_intr(dev, sc->irq_res, sc->stg_intrhand);
	if (sc->port_res)
		bus_release_resource(dev, SYS_RES_IOPORT,
				     sc->port_rid, sc->port_res);
	if (sc->irq_res)
		bus_release_resource(dev, SYS_RES_IRQ,
				     sc->irq_rid, sc->irq_res);
	if (sc->mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY,
				     sc->mem_rid, sc->mem_res);
	return;
}

int
stg_probe(device_t dev)
{
	int rv;
	struct stg_softc *sc = device_get_softc(dev);

	rv = stgprobesubr(rman_get_bustag(sc->port_res),
			  rman_get_bushandle(sc->port_res),
			  device_get_flags(dev));

	return rv;
}

int
stg_attach(device_t dev)
{
	struct stg_softc *sc;
	struct scsi_low_softc *slp;
	u_int32_t flags = device_get_flags(dev);
	intrmask_t s;
	char	dvname[16];

	sc = device_get_softc(dev);

	strcpy(dvname,"stg");

	slp = &sc->sc_sclow;
	slp->sl_dev = dev;
	sc->sc_iot = rman_get_bustag(sc->port_res);
	sc->sc_ioh = rman_get_bushandle(sc->port_res);

	slp->sl_hostid = STG_HOSTID;
	slp->sl_cfgflags = flags;

	s = splcam();
	stgattachsubr(sc);
	splx(s);

	return(STGIOSZ);
}

void
stg_detach (device_t dev)
{
	struct stg_softc *sc = device_get_softc(dev);
	intrmask_t s;

	printf("%s: unload\n",sc->sc_sclow.sl_xname);
	s = splcam();
	scsi_low_deactivate((struct scsi_low_softc *)sc);
	scsi_low_dettach(&sc->sc_sclow);
	splx(s);
	stg_release_resource(dev);
	return;
}

void
stg_intr (void *arg)
{
	stgintr(arg);
	return;
}
