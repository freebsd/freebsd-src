/*	$NetBSD: puc.c,v 1.7 2000/07/29 17:43:38 jlam Exp $	*/

/*-
 * Copyright (c) 2002 JF Hay.  All rights reserved.
 * Copyright (c) 2000 M. Warner Losh.  All rights reserved.
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
 */

/*
 * Copyright (c) 1996, 1998, 1999
 *	Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#define PUC_ENTRAILS	1
#include <dev/puc/pucvar.h>

#include <opt_puc.h>

static int
puc_pci_probe(device_t dev)
{
	uint32_t v1, v2, d1, d2;
	const struct puc_device_description *desc;

	if ((pci_read_config(dev, PCIR_HEADERTYPE, 1) & 0x7f) != 0)
		return (ENXIO);

	v1 = pci_read_config(dev, PCIR_VENDOR, 2);
	d1 = pci_read_config(dev, PCIR_DEVICE, 2);
	v2 = pci_read_config(dev, PCIR_SUBVEND_0, 2);
	d2 = pci_read_config(dev, PCIR_SUBDEV_0, 2);

	desc = puc_find_description(v1, d1, v2, d2);
	if (desc == NULL)
		return (ENXIO);
	device_set_desc(dev, desc->name);
	return (0);
}

static int
puc_pci_attach(device_t dev)
{
	uint32_t v1, v2, d1, d2;

	v1 = pci_read_config(dev, PCIR_VENDOR, 2);
	d1 = pci_read_config(dev, PCIR_DEVICE, 2);
	v2 = pci_read_config(dev, PCIR_SUBVEND_0, 2);
	d2 = pci_read_config(dev, PCIR_SUBDEV_0, 2);
	return (puc_attach(dev, puc_find_description(v1, d1, v2, d2)));
}

static device_method_t puc_pci_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,		puc_pci_probe),
    DEVMETHOD(device_attach,		puc_pci_attach),

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

static driver_t puc_pci_driver = {
	"puc",
	puc_pci_methods,
	sizeof(struct puc_softc),
};

DRIVER_MODULE(puc, pci, puc_pci_driver, puc_devclass, 0, 0);
DRIVER_MODULE(puc, cardbus, puc_pci_driver, puc_devclass, 0, 0);
