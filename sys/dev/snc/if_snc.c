/*-
 * Copyright (c) 1995, David Greenman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 *	National Semiconductor  DP8393X SONIC Driver
 *
 *	This is the bus independent attachment on FreeBSD 4.x
 *		written by Motomichi Matsuzaki <mzaki@e-mail.ne.jp>
 */

#include <sys/param.h>
#include <sys/socket.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h>
#include <net/if_var.h>

#include <dev/snc/dp83932reg.h>
#include <dev/snc/dp83932var.h>
#include <dev/snc/dp83932subr.h>
#include <dev/snc/if_sncreg.h>
#include <dev/snc/if_sncvar.h>

/* devclass for "snc" */
devclass_t snc_devclass;

/****************************************************************
  Resource management functions
 ****************************************************************/

/*
 * Allocate a port resource with the given resource id.
 */
int
snc_alloc_port(device_t dev, int rid)
{
	struct snc_softc *sc = device_get_softc(dev);
	struct resource *res;

	res = bus_alloc_resource_anywhere(dev, SYS_RES_IOPORT, &rid,
					  SNEC_NREGS, RF_ACTIVE);
	if (res) {
		sc->ioport = res;
		sc->ioport_rid = rid;
		sc->sc_iot = rman_get_bustag(res);
		sc->sc_ioh = rman_get_bushandle(res);
		return (0);
	} else {
		device_printf(dev, "can't assign port\n");
		return (ENOENT);
	}
}

/*
 * Allocate a memory resource with the given resource id.
 */
int
snc_alloc_memory(device_t dev, int rid)
{
	struct snc_softc *sc = device_get_softc(dev);
	struct resource *res;

	res = bus_alloc_resource_anywhere(dev, SYS_RES_MEMORY, &rid,
					  SNEC_NMEMS, RF_ACTIVE);
	if (res) {
		sc->iomem = res;
		sc->iomem_rid = rid;
		sc->sc_memt = rman_get_bustag(res);
		sc->sc_memh = rman_get_bushandle(res);
		return (0);
	} else {
		device_printf(dev, "can't assign memory\n");
		return (ENOENT);
	}
}

/*
 * Allocate an irq resource with the given resource id.
 */
int
snc_alloc_irq(device_t dev, int rid, int flags)
{
	struct snc_softc *sc = device_get_softc(dev);
	struct resource *res;

	res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE | flags);
	if (res) {
		sc->irq = res;
		sc->irq_rid = rid;
		return (0);
	} else {
		device_printf(dev, "can't assign irq\n");
		return (ENOENT);
	}
}

/*
 * Release all resources
 */
void
snc_release_resources(device_t dev)
{
	struct snc_softc *sc = device_get_softc(dev);

	if (sc->ioport) {
		bus_release_resource(dev, SYS_RES_IOPORT,
				     sc->ioport_rid, sc->ioport);
		sc->ioport = 0;
	}
	if (sc->iomem) {
		bus_release_resource(dev, SYS_RES_MEMORY,
				     sc->iomem_rid, sc->iomem);
		sc->iomem = 0;
	}
	if (sc->irq) {
		bus_release_resource(dev, SYS_RES_IRQ,
				     sc->irq_rid, sc->irq);
		sc->irq = 0;
	}
	if (sc->sc_ifp) {
		if_free(sc->sc_ifp);
		sc->sc_ifp = 0;
	}
}

/****************************************************************
  Probe routine
 ****************************************************************/

int
snc_probe(device_t dev, int type)
{
	struct snc_softc *sc = device_get_softc(dev);

	return snc_nec16_detectsubr(sc->sc_iot, sc->sc_ioh,
				    sc->sc_memt, sc->sc_memh,
				    rman_get_start(sc->irq),
				    rman_get_start(sc->iomem),
				    type);
}

/****************************************************************
  Attach routine
 ****************************************************************/

int
snc_attach(device_t dev)
{
	struct snc_softc *sc = device_get_softc(dev);
	u_int8_t myea[ETHER_ADDR_LEN];
	int error;

	if (snc_nec16_register_irq(sc, rman_get_start(sc->irq)) == 0 || 
	    snc_nec16_register_mem(sc, rman_get_start(sc->iomem)) == 0) {
		snc_release_resources(dev);
		return(ENOENT);
	}

	snc_nec16_get_enaddr(sc->sc_iot, sc->sc_ioh, myea);
	device_printf(dev, "%s Ethernet\n", snc_nec16_detect_type(myea));

	sc->sc_dev = dev;

	sc->sncr_dcr = DCR_SYNC | DCR_WAIT0 |
            DCR_DMABLOCK | DCR_RFT16 | DCR_TFT28;
	sc->sncr_dcr2 = 0;	/* XXX */
	sc->bitmode = 0;	/* 16 bit card */

	sc->sc_nic_put = snc_nec16_nic_put;
	sc->sc_nic_get = snc_nec16_nic_get;
	sc->sc_writetodesc = snc_nec16_writetodesc;
	sc->sc_readfromdesc = snc_nec16_readfromdesc;
	sc->sc_copytobuf = snc_nec16_copytobuf;
	sc->sc_copyfrombuf = snc_nec16_copyfrombuf;
	sc->sc_zerobuf = snc_nec16_zerobuf;

	/* sncsetup returns 1 if something fails */
	if (sncsetup(sc, myea)) {
		snc_release_resources(dev);
		return(ENOENT);
	}

	mtx_init(&sc->sc_lock, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF);
	callout_init_mtx(&sc->sc_timer, &sc->sc_lock, 0);
	error = sncconfig(sc, NULL, 0, 0, myea);
	if (error) {
		snc_release_resources(dev);
		mtx_destroy(&sc->sc_lock);
		return (error);
	}

	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET | INTR_MPSAFE,
			       NULL, sncintr, sc, &sc->irq_handle);
	if (error) {
		printf("snc_isa_attach: bus_setup_intr() failed\n");
		ether_ifdetach(sc->sc_ifp);		
		snc_release_resources(dev);
		mtx_destroy(&sc->sc_lock);
		return (error);
	}

	return 0;
}

/****************************************************************
  Shutdown routine
 ****************************************************************/

int
snc_shutdown(device_t dev)
{
	struct snc_softc *sc = device_get_softc(dev);

	SNC_LOCK(sc);
	sncshutdown(sc);
	SNC_UNLOCK(sc);

	return (0);
}
