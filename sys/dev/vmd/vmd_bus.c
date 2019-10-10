/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2019 Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sys/taskqueue.h>

#include <sys/pciio.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pci_private.h>
#include <dev/pci/pcib_private.h>
#include <dev/pci/pci_host_generic.h>
#include <dev/vmd/vmd.h>

#include "pcib_if.h"
#include "pci_if.h"

static int
vmd_bus_probe(device_t dev)
{
	device_set_desc(dev, "VMD bus");

	return (-1000);
}

/* PCI interface. */

static int
vmd_bus_attach(device_t dev)
{
	struct vmd_softc *sc;
	struct pci_devinfo *dinfo;
	rman_res_t start, end;
	int b, s, f;

	sc = device_get_softc(device_get_parent(dev));

	/* Start at max PCI vmd_domain and work down */
	b = s = f = 0;
	dinfo = pci_read_device(device_get_parent(dev), dev,
	     PCI_DOMAINMAX - device_get_unit(device_get_parent(dev)),
	     b, s, f);
	if (dinfo == NULL) {
		device_printf(dev, "Cannot allocate dinfo!\n");
		return (ENOENT);
	}

	pci_add_child(dev, dinfo);

	start = rman_get_start(sc->vmd_regs_resource[1]);
	end = rman_get_end(sc->vmd_regs_resource[1]);
	resource_list_add_next(&dinfo->resources, SYS_RES_MEMORY, start, end,
	    end - start + 1);

	start = rman_get_start(sc->vmd_io_resource);
	end = rman_get_end(sc->vmd_io_resource);
	resource_list_add_next(&dinfo->resources, SYS_RES_IOPORT, start, end,
	    end - start + 1);
 
	bus_generic_attach(dev);

	return (0);
}

static int
vmd_bus_detach(device_t dev)
{
	struct pci_devinfo *dinfo;
	int b, s, f;

	device_delete_children(dev);

	b = s = f = 0;
	dinfo = pci_read_device(device_get_parent(dev), dev,
	    PCI_DOMAINMAX - device_get_unit(device_get_parent(dev)),
	    b, s, f);
	if (dinfo == NULL) {
		resource_list_free(&dinfo->resources);
	}
	return (0);
}

static int
vmd_bus_adjust_resource(device_t dev, device_t child, int type,
    struct resource *r, rman_res_t start, rman_res_t end)
{
	struct resource *res = r;
	if (type == SYS_RES_MEMORY) {
		/* VMD device controls this */
		return (0);
	}

	return (bus_generic_adjust_resource(dev, child, type, res, start, end));
}

static int
vmd_bus_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	if (type == SYS_RES_MEMORY) {
		/* VMD device controls this */
		return (0);
	}

	return (pci_release_resource(dev, child, type, rid, r));
}

static struct resource *
vmd_bus_alloc_resource(device_t dev, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct vmd_softc *sc;

	sc = device_get_softc(device_get_parent(dev));

	if (type == SYS_RES_MEMORY) {
		/* remap to VMD resources */
		if (*rid == PCIR_MEMBASE_1) {
			return (sc->vmd_regs_resource[1]);
		} else 	if (*rid == PCIR_PMBASEL_1) {
			return (sc->vmd_regs_resource[2]);
		} else {
			return (sc->vmd_regs_resource[2]);
		}
	}
	return (pci_alloc_resource(dev, child, type, rid, start, end,
	   count, flags));
}

static int
vmd_bus_shutdown(device_t dev)
{
	return (0);
}

static device_method_t vmd_bus_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		vmd_bus_probe),
	DEVMETHOD(device_attach,	vmd_bus_attach),
	DEVMETHOD(device_detach,	vmd_bus_detach),
	DEVMETHOD(device_shutdown,	vmd_bus_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_alloc_resource,	vmd_bus_alloc_resource),
	DEVMETHOD(bus_adjust_resource,	vmd_bus_adjust_resource),
	DEVMETHOD(bus_release_resource,	vmd_bus_release_resource),

	/* pci interface */
	DEVMETHOD(pci_read_config,	pci_read_config_method),
	DEVMETHOD(pci_write_config,	pci_write_config_method),
	DEVMETHOD(pci_alloc_devinfo,	pci_alloc_devinfo_method),

	DEVMETHOD_END
};

static devclass_t vmd_bus_devclass;

DEFINE_CLASS_1(vmd_bus, vmd_bus_pci_driver, vmd_bus_pci_methods,
    sizeof(struct pci_softc), pci_driver);

DRIVER_MODULE(vmd_bus, vmd, vmd_bus_pci_driver, vmd_bus_devclass, NULL, NULL);
MODULE_VERSION(vmd_bus, 1);
