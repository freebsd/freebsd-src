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
 * $FreeBSD$
 */

/*
 * This is a PCI shim for the Aironet PC4500/4800 wireless network
 * driver. Aironet makes PCMCIA, ISA and PCI versions of these devices,
 * which all have basically the same interface. The ISA and PCI cards
 * are actually bridge adapters with PCMCIA cards inserted into them,
 * however they appear as normal PCI or ISA devices to the host.
 *
 * All we do here is handle the PCI probe and attach and set up an
 * interrupt handler entry point. The PCI version of the card uses
 * a PLX 9050 PCI to "dumb bus" bridge chip, which provides us with
 * multiple PCI address space mappings. The primary mapping at PCI
 * register 0x14 is for the PLX chip itself, *NOT* the Aironet card.
 * The I/O address of the Aironet is actually at register 0x18, which
 * is the local bus mapping register for bus space 0. There are also
 * registers for additional register spaces at registers 0x1C and
 * 0x20, but these are unused in the Aironet devices. To find out
 * more, you need a datasheet for the 9050 from PLX, but you have
 * to go through their sales office to get it. Bleh.
 */

#include "opt_inet.h"

#ifdef INET
#define ANCACHE
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#ifndef lint
static const char rcsid[] =
 "$FreeBSD$";
#endif

#include <dev/an/if_aironet_ieee.h>
#include <dev/an/if_anreg.h>

struct an_type {
	u_int16_t		an_vid;
	u_int16_t		an_did;
	char			*an_name;
};

#define AIRONET_VENDORID	0x14B9
#define AIRONET_DEVICEID_35x	0x0350
#define AIRONET_DEVICEID_4500	0x4500
#define AIRONET_DEVICEID_4800	0x4800
#define AIRONET_DEVICEID_4xxx	0x0001
#define AIRONET_DEVICEID_MPI350	0xA504
#define AN_PCI_PLX_LOIO		0x14	/* PLX chip iobase */
#define AN_PCI_LOIO		0x18	/* Aironet iobase */

static struct an_type an_devs[] = {
	{ AIRONET_VENDORID, AIRONET_DEVICEID_35x, "Cisco Aironet 350 Series" },
	{ AIRONET_VENDORID, AIRONET_DEVICEID_4500, "Aironet PCI4500" },
	{ AIRONET_VENDORID, AIRONET_DEVICEID_4800, "Aironet PCI4800" },
	{ AIRONET_VENDORID, AIRONET_DEVICEID_4xxx, "Aironet PCI4500/PCI4800" },
	{ 0, 0, NULL }
};

static int an_probe_pci		__P((device_t));
static int an_attach_pci	__P((device_t));
static int an_detach_pci	__P((device_t));

static int
an_probe_pci(device_t dev)
{
	struct an_type		*t;

	t = an_devs;

	while (t->an_name != NULL) {
		if (pci_get_vendor(dev) == t->an_vid &&
		    pci_get_device(dev) == t->an_did) {
			device_set_desc(dev, t->an_name);
			return(0);
		}
		t++;
	}

	if (pci_get_vendor(dev) == AIRONET_VENDORID &&
	    pci_get_device(dev) == AIRONET_DEVICEID_MPI350) {
		device_set_desc(dev, "Cisco Aironet MPI350");
		return(0);
	}

	return(ENXIO);
}

static int
an_attach_pci(dev)
	device_t		dev;
{
	int			s;
	u_int32_t		command;
	struct an_softc		*sc;
	int 			unit, flags, error = 0;

	s = splimp();

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	flags = device_get_flags(dev);
	bzero(sc, sizeof(struct an_softc));

	if (pci_get_vendor(dev) == AIRONET_VENDORID &&
	    pci_get_device(dev) == AIRONET_DEVICEID_MPI350) {
		sc->mpi350 = 1;
		sc->port_rid = PCIR_MAPS;
	} else {
		/*
		 * Map control/status registers.
	 	 */
		command = pci_read_config(dev, PCIR_COMMAND, 4);
		command |= PCIM_CMD_PORTEN;
		pci_write_config(dev, PCIR_COMMAND, command, 4);
		command = pci_read_config(dev, PCIR_COMMAND, 4);

		if (!(command & PCIM_CMD_PORTEN)) {
			printf("an%d: failed to enable I/O ports!\n", unit);
			error = ENXIO;
			goto fail;
		}
		sc->port_rid = AN_PCI_LOIO;
	}
	error = an_alloc_port(dev, sc->port_rid, 1);

	if (error) {
		printf("an%d: couldn't map ports\n", unit);
		goto fail;
	}

	sc->an_btag = rman_get_bustag(sc->port_res);
	sc->an_bhandle = rman_get_bushandle(sc->port_res);

	/* Allocate memory for MPI350 */
	if (sc->mpi350) {
		/* Allocate memory */
		sc->mem_rid = PCIR_MAPS + 4;
		error = an_alloc_memory(dev, sc->mem_rid, 1);
		if (error) {
			printf("an%d: couldn't map memory\n", unit);
			goto fail;
		}
		sc->an_mem_btag = rman_get_bustag(sc->mem_res);
		sc->an_mem_bhandle = rman_get_bushandle(sc->mem_res);

		/* Allocate aux. memory */
		sc->mem_aux_rid = PCIR_MAPS + 8;
		error = an_alloc_aux_memory(dev, sc->mem_aux_rid, 
		    AN_AUXMEMSIZE);
		if (error) {
			printf("an%d: couldn't map aux memory\n", unit);
			goto fail;
		}
		sc->an_mem_aux_btag = rman_get_bustag(sc->mem_aux_res);
		sc->an_mem_aux_bhandle = rman_get_bushandle(sc->mem_aux_res);

		/* Allocate DMA region */
		error = bus_dma_tag_create(NULL,	/* parent */
			       1, 0,			/* alignment, bounds */
			       BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,		/* filter, filterarg */
			       0x3ffff,			/* maxsize XXX */
			       1,			/* nsegments */
			       0xffff,			/* maxsegsize XXX */
			       BUS_DMA_ALLOCNOW,	/* flags */
			       &sc->an_dtag);
		if (error) {
			printf("an%d: couldn't get DMA region\n", unit);
			goto fail;
		}
	}

	/* Allocate interrupt */
	error = an_alloc_irq(dev, 0, RF_SHAREABLE);
	if (error) {
		goto fail;
        }

	error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET,
	    an_intr, sc, &sc->irq_handle);
	if (error) {
		goto fail;
	}

	sc->an_dev = dev;
	error = an_attach(sc, device_get_unit(dev), flags);

fail:
	if (error)
		an_release_resources(dev);
	splx(s);

	return(error);
}

static int
an_detach_pci(device_t dev)
{
	struct an_softc		*sc = device_get_softc(dev);
	struct ifnet		*ifp = &sc->arpcom.ac_if;

	an_stop(sc);
	ifmedia_removeall(&sc->an_ifmedia);
	ether_ifdetach(ifp, ETHER_BPF_SUPPORTED);
	bus_teardown_intr(dev, sc->irq_res, sc->irq_handle);
	an_release_resources(dev);

	return (0);
}

static device_method_t an_pci_methods[] = {
        /* Device interface */
        DEVMETHOD(device_probe,         an_probe_pci),
        DEVMETHOD(device_attach,        an_attach_pci),
	DEVMETHOD(device_detach,	an_detach_pci),
	DEVMETHOD(device_shutdown,	an_shutdown),
        { 0, 0 }
};

static driver_t an_pci_driver = {
        "an",
        an_pci_methods,
        sizeof(struct an_softc),
};

static devclass_t an_devclass;

DRIVER_MODULE(if_an, pci, an_pci_driver, an_devclass, 0, 0);
