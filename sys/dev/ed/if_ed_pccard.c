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

/*
 * BROKEN
 * 		There is no way to specify a softc struct
 *		for a PCCARD device.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <sys/select.h>
#include <sys/module.h>
#include <pccard/cardinfo.h>
#include <pccard/slot.h>

#include <dev/ed/if_edvar.h>

/*
 *      PC-Card (PCMCIA) specific code.
 */
static int	ed_pccard_init		__P((struct pccard_devinfo *));
static void	ed_pccard_unload	__P((struct pccard_devinfo *));
static int	ep_pccard_intr		__P((struct pccard_devinfo *)); 
static int	ed_pccard_probe		__P((struct isa_device *, u_char *));
static int	ed_pccard_attach	__P((device_t));

PCCARD_MODULE(ed, ed_pccard_init, ed_pccard_unload, ed_pccard_intr, 0, net_imask);

/*
 *      Initialize the device - called from Slot manager.
 */
static int
ed_pccard_init(struct pccard_devinfo *devi)
{
	int i;
	u_char e;
	struct ed_softc *sc = device_get_softc(devi->isahd.id_device);
	int error;

	/* validate unit number. */
	if (devi->isahd.id_unit >= NEDTOT)
		return(ENODEV);
	/*
	 * Probe the device. If a value is returned, the
	 * device was found at the location.
	 */
	sc->gone = 0;
	if (error = ed_pccard_probe(&devi->isahd, devi->misc))
		return(error);
	e = 0;
	for (i = 0; i < ETHER_ADDR_LEN; ++i)
		e |= devi->misc[i];
	if (e)
		for (i = 0; i < ETHER_ADDR_LEN; ++i)
			sc->arpcom.ac_enaddr[i] = devi->misc[i];
	if (ed_pccard_attach(devi->isahd.id_device) == 0)
		return(ENXIO);

	return(0);
}

/*
 *      edunload - unload the driver and clear the table.
 *      XXX TODO:
 *      This is usually called when the card is ejected, but
 *      can be caused by a modunload of a controller driver.
 *      The idea is to reset the driver's view of the device
 *      and ensure that any driver entry points such as
 *      read and write do not hang.
 */
static void
ed_pccard_unload(struct pccard_devinfo *devi)
{
	struct ed_softc *sc = device_get_softc(devi->isahd.id_device);
	struct ifnet *ifp = &sc->arpcom.ac_if;

	if (sc->gone) {
		printf("ed%d: already unloaded\n", devi->isahd.id_unit);
		return;
	}
	ifp->if_flags &= ~IFF_RUNNING;
	if_down(ifp);
	sc->gone = 1;
	printf("ed%d: unload\n", devi->isahd.id_unit);
}

/*
 *      card_intr - Shared interrupt called from
 *      front end of PC-Card handler.
 */
static int
ed_pccard_intr(struct pccard_devinfo *devi)
{
	struct ed_softc *sc = device_get_softc(devi->isahd.id_device);

	edintr(sc);

	return(1);
}

/* 
 * Probe framework for pccards.  Replicates the standard framework,
 * minus the pccard driver registration and ignores the ether address
 * supplied (from the CIS), relying on the probe to find it instead.
 */
static int
ed_pccard_probe(isa_dev, ether)
	struct isa_device *isa_dev;
	u_char *ether;
{
	int     error;
	  
	error = ed_probe_WD80x3(isa_dev->id_device);
	if (error == 0)
		goto end;
	ed_release_resources(dev);

	error = ed_probe_Novell(isa_dev->id_device);
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
ed_pccard_attach(dev)
        device_t dev;
{
        struct ed_softc *sc = device_get_softc(dev);
        int flags = device_get_flags(dev);
        int error;
        
        if (sc->port_used > 0)
                ed_alloc_port(dev, sc->port_rid, 1);
        if (sc->mem_used)
                ed_alloc_memory(dev, sc->mem_rid, 1);
        ed_alloc_irq(dev, sc->irq_rid, 0);
                
        error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET,
                               edintr, sc, &sc->irq_handle);
        if (error) {
                ed_release_resources(dev);
                return (error);
        }              

        return ed_attach(sc, device_get_unit(dev), flags);
} 
