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
 */

/*
 * Lucent WaveLAN/IEEE 802.11 PCMCIA driver for FreeBSD.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_ieee80211.h>

#include <dev/wi/if_wavelan_ieee.h>
#include <dev/wi/if_wivar.h>
#include <dev/wi/if_wireg.h>

#include "card_if.h"

#if !defined(lint)
static const char rcsid[] =
  "$FreeBSD$";
#endif

static int wi_pci_probe(device_t);
static int wi_pci_attach(device_t);

static device_method_t wi_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		wi_pci_probe),
	DEVMETHOD(device_attach,	wi_pci_attach),
	DEVMETHOD(device_detach,	wi_generic_detach),
	DEVMETHOD(device_shutdown,	wi_shutdown),

	{ 0, 0 }
};

static driver_t wi_pci_driver = {
	"wi",
	wi_pci_methods,
	sizeof(struct wi_softc)
};

static struct {
	unsigned int vendor,device;
	int bus_type;
	char *desc;
} pci_ids[] = {
	{0x1638, 0x1100, WI_BUS_PCI_PLX, "PRISM2STA PCI WaveLAN/IEEE 802.11"},
	{0x1385, 0x4100, WI_BUS_PCI_PLX, "Netgear MA301 PCI IEEE 802.11b"},
	{0x16ab, 0x1101, WI_BUS_PCI_PLX, "GLPRISM2 PCI WaveLAN/IEEE 802.11"},
	{0x16ab, 0x1102, WI_BUS_PCI_PLX, "Linksys WDT11 PCI IEEE 802.11b"},
	{0x1260, 0x3873, WI_BUS_PCI_NATIVE, "Linksys WMP11 PCI Prism2.5"},
	{0x10b7, 0x7770, WI_BUS_PCI_PLX, "3Com Airconnect IEEE 802.11b"},
	{0, 0, 0, NULL}
};

DRIVER_MODULE(if_wi, pci, wi_pci_driver, wi_devclass, 0, 0);

static int
wi_pci_probe(dev)
	device_t	dev;
{
	struct wi_softc		*sc;
	int i;

	sc = device_get_softc(dev);
	for(i=0; pci_ids[i].vendor != 0; i++) {
		if ((pci_get_vendor(dev) == pci_ids[i].vendor) &&
			(pci_get_device(dev) == pci_ids[i].device)) {
			sc->wi_bus_type = pci_ids[i].bus_type;
			device_set_desc(dev, pci_ids[i].desc);
			return (0);
		}
	}
	return(ENXIO);
}

static int
wi_pci_attach(device_t dev)
{
	struct wi_softc		*sc;
	u_int32_t		command, wanted;
	u_int16_t		reg;
	int			error;
	int			timeout;

	sc = device_get_softc(dev);

	command = pci_read_config(dev, PCIR_COMMAND, 4);
	wanted = PCIM_CMD_PORTEN|PCIM_CMD_MEMEN;
	command |= wanted;
	pci_write_config(dev, PCIR_COMMAND, command, 4);
	command = pci_read_config(dev, PCIR_COMMAND, 4);
	if ((command & wanted) != wanted) {
		device_printf(dev, "wi_pci_attach() failed to enable pci!\n");
		return (ENXIO);
	}

	if (sc->wi_bus_type != WI_BUS_PCI_NATIVE) {
		error = wi_alloc(dev, WI_PCI_IORES);
		if (error)
			return (error);

		/* Make sure interrupts are disabled. */
		CSR_WRITE_2(sc, WI_INT_EN, 0);
		CSR_WRITE_2(sc, WI_EVENT_ACK, 0xFFFF);

		/* We have to do a magic PLX poke to enable interrupts */
		sc->local_rid = WI_PCI_LOCALRES;
		sc->local = bus_alloc_resource(dev, SYS_RES_IOPORT,
		    &sc->local_rid, 0, ~0, 1, RF_ACTIVE);
		sc->wi_localtag = rman_get_bustag(sc->local);
		sc->wi_localhandle = rman_get_bushandle(sc->local);
		command = bus_space_read_4(sc->wi_localtag, sc->wi_localhandle,
		    WI_LOCAL_INTCSR);
		command |= WI_LOCAL_INTEN;
		bus_space_write_4(sc->wi_localtag, sc->wi_localhandle,
		    WI_LOCAL_INTCSR, command);
		bus_release_resource(dev, SYS_RES_IOPORT, sc->local_rid,
		    sc->local);
		sc->local = NULL;

		sc->mem_rid = WI_PCI_MEMRES;
		sc->mem = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->mem_rid,
					0, ~0, 1, RF_ACTIVE);
		if (sc->mem == NULL) {
			device_printf(dev, "couldn't allocate memory\n");
			wi_free(dev);
			return (ENXIO);
		}
		sc->wi_bmemtag = rman_get_bustag(sc->mem);
		sc->wi_bmemhandle = rman_get_bushandle(sc->mem);

		/*
		 * From Linux driver:
		 * Write COR to enable PC card
		 * This is a subset of the protocol that the pccard bus code
		 * would do.
		 */
		CSM_WRITE_1(sc, WI_COR_OFFSET, WI_COR_VALUE); 
		reg = CSM_READ_1(sc, WI_COR_OFFSET);
		if (reg != WI_COR_VALUE) {
			device_printf(dev, "CSM_READ_1(WI_COR_OFFSET) "
			    "wanted %d, got %d\n", WI_COR_VALUE, reg);
			wi_free(dev);
			return (ENXIO);
		}
	} else {
		error = wi_alloc(dev, WI_PCI_LMEMRES);
		if (error)
			return (error);

		CSR_WRITE_2(sc, WI_HFA384X_PCICOR_OFF, 0x0080);
		DELAY(250000);

		CSR_WRITE_2(sc, WI_HFA384X_PCICOR_OFF, 0x0000);
		DELAY(500000);

		timeout=2000000;
		while ((--timeout > 0) &&
		    (CSR_READ_2(sc, WI_COMMAND) & WI_CMD_BUSY))
			DELAY(10);

		if (timeout == 0) {
			device_printf(dev, "couldn't reset prism2.5 core.\n");
			wi_free(dev);
			return(ENXIO);
		}
	}

	CSR_WRITE_2(sc, WI_HFA384X_SWSUPPORT0_OFF, WI_PRISM2STA_MAGIC);
	reg = CSR_READ_2(sc, WI_HFA384X_SWSUPPORT0_OFF);
	if (reg != WI_PRISM2STA_MAGIC) {
		device_printf(dev,
		    "CSR_READ_2(WI_HFA384X_SWSUPPORT0_OFF) "
		    "wanted %d, got %d\n", WI_PRISM2STA_MAGIC, reg);
		wi_free(dev);
		return (ENXIO);
	}

	error = wi_generic_attach(dev);
	if (error != 0)
		return (error);

	return (0);
}
