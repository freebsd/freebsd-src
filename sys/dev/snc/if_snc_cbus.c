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
 *	National Semiconductor  DP8393X SONIC Driver
 *
 *	This is the C-bus specific attachment on FreeBSD
 *		written by Motomichi Matsuzaki <mzaki@e-mail.ne.jp>
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/kernel.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_media.h>

#include <isa/isavar.h>
#include <sys/malloc.h>		/* as dependency for isa/isa_common.h */
#include <isa/isa_common.h>	/* for snc_isapnp_reconfig() */

#include <dev/snc/dp83932var.h>
#include <dev/snc/if_sncreg.h>
#include <dev/snc/if_sncvar.h>

static void snc_isapnp_reconfig	(device_t);
static int snc_isa_probe	(device_t);
static int snc_isa_attach	(device_t);

static struct isa_pnp_id snc_ids[] = {
	{ 0x6180a3b8,	NULL },		/* NEC8061 NEC PC-9801-104 */
	{ 0,		NULL }
};

static void
snc_isapnp_reconfig(dev)
	device_t dev;
{
	struct isa_device *idev = DEVTOISA(dev);
        struct isa_config config;
	u_long start, count;
	int rid;

	bzero(&config, sizeof(config));

	for (rid = 0; rid < ISA_NMEM; rid++) {
		if (bus_get_resource(dev, SYS_RES_MEMORY, rid, &start, &count))
			break;
		config.ic_mem[rid].ir_start = start;
		config.ic_mem[rid].ir_end = start;
		config.ic_mem[rid].ir_size = count;
	}
	config.ic_nmem = rid;
	for (rid = 0; rid < ISA_NPORT; rid++) {
		if (bus_get_resource(dev, SYS_RES_IOPORT, rid, &start, &count))
			break;
		config.ic_port[rid].ir_start = start;
		config.ic_port[rid].ir_end = start;
		config.ic_port[rid].ir_size = count;
	}
	config.ic_nport = rid;
	for (rid = 0; rid < ISA_NIRQ; rid++) {
		if (bus_get_resource(dev, SYS_RES_IRQ, rid, &start, &count))
			break;
		config.ic_irqmask[rid] = 1 << start;
	}
	config.ic_nirq = rid;
	for (rid = 0; rid < ISA_NDRQ; rid++) {
		if (bus_get_resource(dev, SYS_RES_DRQ, rid, &start, &count))
			break;
		config.ic_drqmask[rid] = 1 << start;
	}
	config.ic_ndrq = rid;
	
	idev->id_config_cb(idev->id_config_arg, &config, 1);
}

static int
snc_isa_probe(dev)
	device_t dev;
{
	struct snc_softc *sc = device_get_softc(dev);
	int type;
 	int error = 0;

	bzero(sc, sizeof(struct snc_softc));

	/* Check isapnp ids */
	error = ISA_PNP_PROBE(device_get_parent(dev), dev, snc_ids);

	/* If the card had a PnP ID that didn't match any we know about */
	if (error == ENXIO) {
		return(error);
	}

	switch (error) {
	case 0:		/* Matched PnP */
		type = SNEC_TYPE_PNP;
		break;

	case ENOENT:	/* Legacy ISA */
		type = SNEC_TYPE_LEGACY;
		break;

	default:	/* If we had some other problem. */
		return(error);
	}

	if (type == SNEC_TYPE_PNP && isa_get_portsize(dev) == 0) {
		int port;
		int rid = 0;
		struct resource *res = NULL;

		for (port = 0x0888; port <= 0x3888; port += 0x1000) {
			bus_set_resource(dev, SYS_RES_IOPORT, rid,
					 port, SNEC_NREGS);
			res = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
						 0, ~0, SNEC_NREGS,
						 0 /* !RF_ACTIVE */);
			if (res) break;
		}

		printf("snc_isa_probe: broken PnP resource, ");
		if (res) {
			printf("use port 0x%x\n", port);
			bus_release_resource(dev, SYS_RES_IOPORT, rid, res);
			snc_isapnp_reconfig(dev);
		} else {
			printf("and can't find port\n");
		}
	}

	error = snc_alloc_port(dev, 0);
	error = max(error, snc_alloc_memory(dev, 0));
	error = max(error, snc_alloc_irq(dev, 0, 0));

	if (!error && !snc_probe(dev, type))
		error = ENOENT;

	snc_release_resources(dev);
	return (error);
}

static int
snc_isa_attach(dev)
	device_t dev;
{
	struct snc_softc *sc = device_get_softc(dev);
	int error;
	
	bzero(sc, sizeof(struct snc_softc));

	snc_alloc_port(dev, 0);
	snc_alloc_memory(dev, 0);
	snc_alloc_irq(dev, 0, 0);
		
	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET,
			       sncintr, sc, &sc->irq_handle);
	if (error) {
		printf("snc_isa_attach: bus_setup_intr() failed\n");
		snc_release_resources(dev);
		return (error);
	}	       

	/* This interface is always enabled. */
	sc->sc_enabled = 1;

	return snc_attach(dev);
} 

static device_method_t snc_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		snc_isa_probe),
	DEVMETHOD(device_attach,	snc_isa_attach),
	DEVMETHOD(device_shutdown,	snc_shutdown),

	{ 0, 0 }
};

static driver_t snc_isa_driver = {
	"snc",
	snc_isa_methods,
	sizeof(struct snc_softc)
};

DRIVER_MODULE(if_snc, isa, snc_isa_driver, snc_devclass, 0, 0);
