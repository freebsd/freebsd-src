/*-
 * Copyright (c) 2002 Poul-Henning Kamp.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "opt_puc.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#define PUC_ENTRAILS 1
#include <dev/puc/pucvar.h>

#include <dev/sio/sioreg.h>
#include <dev/pccard/pccardvar.h>

const struct puc_device_description rscom_devices = {

	"ARGOSY SP320 Dual port serial PCMCIA",
	/* http://www.argosy.com.tw/product/sp320.htm */
	NULL,
		{	0,	0,	0,	0	},
		{	0,	0,	0,	0	},
	{
		{ PUC_PORT_TYPE_COM, 0x0, 0x00, DEFAULT_RCLK, 0x100000 },
		{ PUC_PORT_TYPE_COM, 0x1, 0x00, DEFAULT_RCLK, 0 },
	}
};


static int
puc_pccard_probe(device_t dev)
{
	char *vendor, *product;
	int error;

	error = pccard_get_vendor_str(dev, &vendor);
	if (error)
		return(error);
	error = pccard_get_product_str(dev, &product);
	if (error)
		return(error);
	if (!strcmp(vendor, "PCMCIA") && !strcmp(product, "RS-COM 2P")) {
		device_set_desc(dev, rscom_devices.name);
		return (0);
	}

	return (ENXIO);
}

static int
puc_pccard_attach(device_t dev)
{

	return (puc_attach(dev, &rscom_devices));
}

static device_method_t puc_pccard_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,		puc_pccard_probe),
    DEVMETHOD(device_attach,		puc_pccard_attach),

    DEVMETHOD(bus_alloc_resource,	puc_alloc_resource),
    DEVMETHOD(bus_release_resource,	puc_release_resource),
    DEVMETHOD(bus_get_resource,		puc_get_resource),
    DEVMETHOD(bus_read_ivar,		puc_read_ivar),
    DEVMETHOD(bus_setup_intr,		puc_setup_intr),
    DEVMETHOD(bus_teardown_intr,	puc_teardown_intr),
    DEVMETHOD(bus_print_child,		bus_generic_print_child),
    DEVMETHOD(bus_driver_added,		bus_generic_driver_added),
    { 0, 0 }
};

static driver_t puc_pccard_driver = {
	"puc",
	puc_pccard_methods,
	sizeof(struct puc_softc),
};

DRIVER_MODULE(puc, pccard, puc_pccard_driver, puc_devclass, 0, 0);
