/*
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/kernel.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_mib.h>

#include <sys/select.h>
#include <pccard/cardinfo.h>
#include <pccard/slot.h>

#include <dev/ed/if_edvar.h>

/*
 *      PC-Card (PCMCIA) specific code.
 */
static int	ed_pccard_probe(device_t);
static int	ed_pccard_attach(device_t);
static void	ed_pccard_detach(device_t);

static device_method_t ed_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ed_pccard_probe),
	DEVMETHOD(device_attach,	ed_pccard_attach),
	DEVMETHOD(device_detach,	ed_pccard_detach),

	{ 0, 0 }
};

static driver_t ed_pccard_driver = {
	"ed",
	ed_pccard_methods,
	sizeof(struct ed_softc)
};

static devclass_t ed_pccard_devclass;

DRIVER_MODULE(ed, pccard, ed_pccard_driver, ed_pccard_devclass, 0, 0);

/*
 *      ed_pccard_detach - unload the driver and clear the table.
 *      XXX TODO:
 *      This is usually called when the card is ejected, but
 *      can be caused by a modunload of a controller driver.
 *      The idea is to reset the driver's view of the device
 *      and ensure that any driver entry points such as
 *      read and write do not hang.
 */
static void
ed_pccard_detach(device_t dev)
{
	struct ed_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = &sc->arpcom.ac_if;

	if (sc->gone) {
		device_printf(dev, "already unloaded\n");
		return;
	}
	ifp->if_flags &= ~IFF_RUNNING;
	if_down(ifp);
	bus_teardown_intr(dev, sc->irq_res, &sc->irq_handle);
	sc->gone = 1;
	device_printf(dev, "unload\n");
}

/* 
 * Probe framework for pccards.  Replicates the standard framework,
 * minus the pccard driver registration and ignores the ether address
 * supplied (from the CIS), relying on the probe to find it instead.
 */
static int
ed_pccard_probe(device_t dev)
{
	int     error;

	error = ed_probe_WD80x3(dev);
	if (error == 0)
		goto end;
	ed_release_resources(dev);

	error = ed_probe_Novell(dev);
	if (error == 0)
		goto end;
	ed_release_resources(dev);

end:
	if (error == 0)
		error = ed_alloc_irq(dev, 0, 0);

	ed_release_resources(dev);
	return (error);
}

static int
ed_pccard_attach(device_t dev)
{
	struct ed_softc *sc = device_get_softc(dev);
	int flags = device_get_flags(dev);
	int error;
	
	if (sc->port_used > 0)
		ed_alloc_port(dev, sc->port_rid, sc->port_used);
	if (sc->mem_used)
		ed_alloc_memory(dev, sc->mem_rid, sc->mem_used);
	ed_alloc_irq(dev, sc->irq_rid, 0);
		
	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET,
			       edintr, sc, &sc->irq_handle);
	if (error) {
		printf("setup intr failed %d \n", error);
		ed_release_resources(dev);
		return (error);
	}	      

	error = ed_attach(sc, device_get_unit(dev), flags);
	return (error);
} 
