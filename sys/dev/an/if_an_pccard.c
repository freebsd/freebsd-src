/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/an/if_an_pccard.c,v 1.1.2.2 2000/08/02 22:29:50 peter Exp $
 */

/*
 * Aironet 4500/4800 802.11 PCMCIA/ISA/PCI driver for FreeBSD.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

#include "opt_inet.h"

#ifdef INET
#define ANCACHE
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/kernel.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <dev/an/if_aironet_ieee.h>
#include <dev/an/if_anreg.h>

/*
 * Support for PCMCIA cards.
 */
static int  an_pccard_probe(device_t);
static int  an_pccard_attach(device_t);
static int  an_pccard_detach(device_t);

static device_method_t an_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		an_pccard_probe),
	DEVMETHOD(device_attach,	an_pccard_attach),
	DEVMETHOD(device_detach,	an_pccard_detach),
	DEVMETHOD(device_shutdown,	an_shutdown),

	{ 0, 0 }
};

static driver_t an_pccard_driver = {
	"an",
	an_pccard_methods,
	sizeof(struct an_softc)
};

static devclass_t an_pccard_devclass;

DRIVER_MODULE(if_an, pccard, an_pccard_driver, an_pccard_devclass, 0, 0);

static int 
an_pccard_detach(device_t dev)
{
	struct an_softc		*sc = device_get_softc(dev);
	struct ifnet		*ifp = &sc->arpcom.ac_if;

	if (sc->an_gone) {
		device_printf(dev,"already unloaded\n");
		return(0);
	}
	an_stop(sc);
	ifp->if_flags &= ~IFF_RUNNING;
	ether_ifdetach(ifp, ETHER_BPF_SUPPORTED);
	sc->an_gone = 1;
	bus_teardown_intr(dev, sc->irq_res, sc->irq_handle);
	an_release_resources(dev);
	return (0);
}

static int
an_pccard_probe(device_t dev)
{
	int     error;

	error = an_probe(dev); /* 0 is failure for now */
	if (error != 0) {
		device_set_desc(dev, "Aironet PC4500/PC4800");
		error = an_alloc_irq(dev, 0, 0);
	} else
	        error = 1;
	an_release_resources(dev);
	return (error);
}


static int
an_pccard_attach(device_t dev)
{
	struct an_softc *sc = device_get_softc(dev);
	int flags = device_get_flags(dev);
	int error;
	
	an_alloc_port(dev, sc->port_rid, AN_IOSIZ);
	an_alloc_irq(dev, sc->irq_rid, 0);
		
	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET,
			       an_intr, sc, &sc->irq_handle);
	if (error) {
		printf("setup intr failed %d \n", error);
		an_release_resources(dev);
		return (error);
	}
	      
	sc->an_bhandle = rman_get_bushandle(sc->port_res);
	sc->an_btag = rman_get_bustag(sc->port_res);

	error = an_attach(sc, device_get_unit(dev), flags);
	return (error);
} 

