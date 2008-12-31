/*
 * Copyright (c) 1998, Larry Lile
 * All rights reserved.
 *
 * For latest sources and information on this driver, please
 * go to http://anarchy.stdio.com.
 *
 * Questions, comments or suggestions should be directed to
 * Larry Lile <lile@stdio.com>.
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
 * $FreeBSD: src/sys/contrib/dev/oltr/if_oltr_isa.c,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/iso88025.h>
#include <net/if_media.h>
#include <net/bpf.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <sys/bus.h>
#include <sys/rman.h>

#include <isa/isavar.h>
#include <isa/pnpvar.h>

#include "contrib/dev/oltr/trlld.h"
#include "contrib/dev/oltr/if_oltrvar.h"

extern TRlldDriver_t LldDriver;

struct AdapterNameEntry {
        int type;
        char *name; } ;

static struct AdapterNameEntry AdapterNameList[] = {
	{ 1, "Olicom OC-3115" },
        { 2, "Olicom ISA 16/4 Adapter (OC-3117)" },
        { 3, "Olicom ISA 16/4 Adapter (OC-3118)" },
	{ 0, "Olicom Unsupported Adapter" }
};

static int oltr_isa_probe	__P((device_t));
static int oltr_isa_attach	__P((device_t));

static struct isa_pnp_id oltr_ids[] = {
	{ 0x0100833d,	NULL },		/* OLC9430 */
	{ 0,		NULL },
};


static int
oltr_isa_probe(dev)
	device_t dev;
{
	TRlldAdapterConfig_t	config;
	struct resource		*port_res;
        int			port_rid;
	struct AdapterNameEntry	*list;
	int			error;
	int			iobase;
	int			success;

	error = ISA_PNP_PROBE(device_get_parent(dev), dev, oltr_ids);
	if (error != 0 && error != ENOENT)
		return (error);

	iobase = bus_get_resource_start(dev, SYS_RES_IOPORT, 0);
	if (iobase == 0)
		return (ENXIO);

	if (error == ENOENT
            && bus_set_resource(dev, SYS_RES_IOPORT, 0, iobase,
				OLTR_PORT_COUNT) < 0)
                 return (ENXIO);

	port_rid = 0;
	port_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &port_rid,
				      0, ~0, 0, RF_ACTIVE);
	if (port_res == NULL)
		return (ENXIO);

	success = TRlldIOAddressConfig(&LldDriver, &config, iobase);

	bus_release_resource(dev, SYS_RES_IOPORT, port_rid, port_res);

	if (!success)
		return (ENXIO);

	for (list = AdapterNameList;
	     list->type != 0 && list->type != config.type;
	     list++) ;
	device_set_desc(dev, list->name);
 
	return (0);
}


static void
oltr_dmamap_callback(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	*(unsigned long *)arg = segs->ds_addr;
}


static int
oltr_isa_attach(dev)
	device_t dev;
{
	struct oltr_softc	*sc = device_get_softc(dev);
	bus_dma_filter_t	*filter;
	void			*filter_arg;
	int			scratch_size;
	int			buffer_size;
	int			iobase;
	int			success;
	int			s, i;

	s = splimp();

	bzero(sc, sizeof(struct oltr_softc));
	sc->unit = device_get_unit(dev);
	sc->state = OL_UNKNOWN;

	iobase = bus_get_resource_start(dev, SYS_RES_IOPORT, 0);
	if (iobase == 0) {
		device_printf(dev, "couldn't get base address\n");
		goto config_failed;
	}

	sc->port_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->port_rid,
				 0, ~0, 0, RF_ACTIVE);
	if (sc->port_res == NULL) {
		device_printf(dev, "couldn't allocate io port\n");
		goto config_failed;
	}

	success = TRlldIOAddressConfig(&LldDriver, &sc->config, iobase);
	if (success == 0) {
		device_printf(dev, "adapter configuration failed\n");
		goto config_failed;
	}

	device_printf(dev, "MAC address %6D\n", sc->config.macaddress, ":");

	if (sc->config.dmalevel != TRLLD_DMA_PIO) {
		sc->drq_rid = 0;
		sc->drq_res = bus_alloc_resource(dev, SYS_RES_DRQ,
					 &sc->drq_rid, 0, ~0, 1, RF_ACTIVE);
		if (sc->drq_res == NULL) {
                	device_printf(dev, "couldn't setup dma channel\n");
			goto config_failed;
		} else
			isa_dmacascade(sc->config.dmalevel);
	}

        filter = NULL;
        filter_arg = NULL;

        if (bus_dma_tag_create(NULL, 1, 0, BUS_SPACE_MAXADDR_24BIT,
				BUS_SPACE_MAXADDR, filter,
		filter_arg, BUS_SPACE_MAXSIZE_24BIT, BUS_SPACE_UNRESTRICTED,
		BUS_SPACE_MAXSIZE_24BIT, 0, NULL, NULL, &sc->bus_tag) != 0) {

                device_printf(dev, "couldn't setup parent dma tag\n");
		return (ENOMEM);
	}

	scratch_size = TRlldAdapterSize();
	buffer_size = RING_BUFFER_LEN * (RX_BUFFER_LEN + TX_BUFFER_LEN);

        if (bus_dma_tag_create(sc->bus_tag, 1, 0, BUS_SPACE_MAXADDR,
                               BUS_SPACE_MAXADDR, NULL, NULL,
			       scratch_size + buffer_size,
                               1, BUS_SPACE_MAXSIZE_24BIT, 0,
			       NULL, NULL, &sc->mem_tag) != 0) {

                device_printf(dev, "couldn't setup buffer dma tag\n");
		goto config_failed;
        }
	
	if (bus_dmamem_alloc(sc->mem_tag, (void **)&sc->TRlldAdapter,
			     BUS_DMA_NOWAIT, &sc->mem_map) != 0) {

                device_printf(dev, "couldn't alloc buffer memory\n");
		goto config_failed;
        }

	bus_dmamap_load(sc->mem_tag, sc->mem_map, sc->TRlldAdapter,
			scratch_size + buffer_size, oltr_dmamap_callback,
			(void *)&sc->TRlldAdapter_phys, 0);

	if (sc->TRlldAdapter_phys == 0) {
		device_printf(dev, "couldn't load buffer memory\n");
		goto config_failed;
	}

	/*
	 * Allocate RX/TX Pools
	 */

	for (i = 0; i < RING_BUFFER_LEN; i++) {
		sc->rx_ring[i].index = i;
		sc->rx_ring[i].data = (char *)sc->TRlldAdapter + scratch_size +
		    i * (RX_BUFFER_LEN + TX_BUFFER_LEN);
		sc->rx_ring[i].address = sc->TRlldAdapter_phys + scratch_size +
		    i * (RX_BUFFER_LEN + TX_BUFFER_LEN);
		sc->tx_ring[i].index = i;
		sc->tx_ring[i].data = (char *)sc->TRlldAdapter + scratch_size +
		    i * (RX_BUFFER_LEN + TX_BUFFER_LEN) + RX_BUFFER_LEN;
		sc->tx_ring[i].address = sc->TRlldAdapter_phys + scratch_size +
		    i * (RX_BUFFER_LEN + TX_BUFFER_LEN) + RX_BUFFER_LEN;
	}

	if (oltr_attach(dev) != 0)
		goto config_failed;

	splx(s);
	return (0);

config_failed:

	if (sc->port_res) {
		bus_release_resource(dev, SYS_RES_IOPORT,
				     sc->port_rid, sc->port_res);
		sc->port_res = NULL;
		sc->port_rid = 0;
	}

	if (sc->oltr_intrhand) {
		bus_teardown_intr(dev, sc->irq_res, sc->oltr_intrhand);
		sc->oltr_intrhand = NULL;
	}

	if (sc->irq_res) {
		bus_release_resource(dev, SYS_RES_MEMORY,
				     sc->irq_rid, sc->irq_res);
		sc->irq_res = NULL;
		sc->irq_rid = 0;
	}

	if (sc->drq_res) {
		bus_release_resource(dev, SYS_RES_DRQ,
				     sc->drq_rid, sc->drq_res);
		sc->drq_res = NULL;
		sc->drq_rid = 0;
	}

	if (sc->TRlldAdapter) {
		bus_dmamem_free(sc->mem_tag, sc->TRlldAdapter, sc->mem_map);
		sc->TRlldAdapter = NULL;
	}

	if (sc->mem_map) {
		bus_dmamap_destroy(sc->mem_tag, sc->mem_map);
		sc->mem_map = NULL;
	}

	if (sc->mem_tag) {
		bus_dma_tag_destroy(sc->mem_tag);
		sc->mem_tag = NULL;
	}

	if (sc->bus_tag) {
		bus_dma_tag_destroy(sc->bus_tag);
		sc->bus_tag = NULL;
	}

	splx(s);
	return (ENXIO);
}


static device_method_t oltr_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		oltr_isa_probe),
	DEVMETHOD(device_attach,	oltr_isa_attach),

	{ 0, 0 }
};

static driver_t oltr_isa_driver = {
	"oltr",
	oltr_isa_methods,
	sizeof(struct oltr_softc)
};

static devclass_t oltr_isa_devclass;

DRIVER_MODULE(oltr, isa, oltr_isa_driver, oltr_isa_devclass, 0, 0);
MODULE_DEPEND(oltr, isa, 1, 1, 1);
MODULE_DEPEND(oltr, iso88025, 1, 1, 1);
