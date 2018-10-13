/*-
 * Copyright (C) 2018 Cavium Inc.
 * Copyright (c) 2015 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2014 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under
 * the sponsorship of the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Generic ECAM PCIe driver */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/rman.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/cpuset.h>
#include <sys/rwlock.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpi_pcibvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>
#include <dev/pci/pci_host_generic.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include "pcib_if.h"

int pci_host_generic_acpi_attach(device_t);

/* Assembling ECAM Configuration Address */
#define	PCIE_BUS_SHIFT		20
#define	PCIE_SLOT_SHIFT		15
#define	PCIE_FUNC_SHIFT		12
#define	PCIE_BUS_MASK		0xFF
#define	PCIE_SLOT_MASK		0x1F
#define	PCIE_FUNC_MASK		0x07
#define	PCIE_REG_MASK		0xFFF

#define	PCIE_ADDR_OFFSET(bus, slot, func, reg)			\
	((((bus) & PCIE_BUS_MASK) << PCIE_BUS_SHIFT)	|	\
	(((slot) & PCIE_SLOT_MASK) << PCIE_SLOT_SHIFT)	|	\
	(((func) & PCIE_FUNC_MASK) << PCIE_FUNC_SHIFT)	|	\
	((reg) & PCIE_REG_MASK))

#define	PCI_IO_WINDOW_OFFSET	0x1000

#define	SPACE_CODE_SHIFT	24
#define	SPACE_CODE_MASK		0x3
#define	SPACE_CODE_IO_SPACE	0x1
#define	PROPS_CELL_SIZE		1
#define	PCI_ADDR_CELL_SIZE	2

struct generic_pcie_acpi_softc {
	struct generic_pcie_core_softc base;
	ACPI_BUFFER		ap_prt;		/* interrupt routing table */
};

/* Forward prototypes */

static int generic_pcie_acpi_probe(device_t dev);
static ACPI_STATUS pci_host_generic_acpi_parse_resource(ACPI_RESOURCE *, void *);
static int generic_pcie_acpi_read_ivar(device_t, device_t, int, uintptr_t *);

static int
generic_pcie_acpi_probe(device_t dev)
{
	ACPI_DEVICE_INFO *devinfo;
	ACPI_HANDLE h;
	int root;

	if (acpi_disabled("pcib") || (h = acpi_get_handle(dev)) == NULL ||
	    ACPI_FAILURE(AcpiGetObjectInfo(h, &devinfo)))
		return (ENXIO);
	root = (devinfo->Flags & ACPI_PCI_ROOT_BRIDGE) != 0;
	AcpiOsFree(devinfo);
	if (!root)
		return (ENXIO);

	device_set_desc(dev, "Generic PCI host controller");
	return (BUS_PROBE_GENERIC);
}

int
pci_host_generic_acpi_attach(device_t dev)
{
	struct generic_pcie_acpi_softc *sc;
	ACPI_HANDLE handle;
	ACPI_STATUS status;
	int error;

	sc = device_get_softc(dev);

	handle = acpi_get_handle(dev);
	if (ACPI_FAILURE(acpi_GetInteger(handle, "_CCA", &sc->base.coherent)))
		sc->base.coherent = 0;
	if (bootverbose)
		device_printf(dev, "Bus is%s cache-coherent\n",
		    sc->base.coherent ? "" : " not");

	if (!ACPI_FAILURE(acpi_GetInteger(handle, "_BBN", &sc->base.ecam)))
		sc->base.ecam >>= 7;
	else
		sc->base.ecam = 0;

	acpi_pcib_fetch_prt(dev, &sc->ap_prt);

	error = pci_host_generic_core_attach(dev);
	if (error != 0)
		return (error);

	status = AcpiWalkResources(handle, "_CRS",
	    pci_host_generic_acpi_parse_resource, (void *)dev);

	if (ACPI_FAILURE(status))
		return (ENXIO);

	device_add_child(dev, "pci", -1);
	return (bus_generic_attach(dev));
}

static ACPI_STATUS
pci_host_generic_acpi_parse_resource(ACPI_RESOURCE *res, void *arg)
{
	device_t dev = (device_t)arg;
	struct generic_pcie_acpi_softc *sc;
	rman_res_t min, max;
	int error;

	switch (res->Type) {
		case ACPI_RESOURCE_TYPE_ADDRESS32:
			    min = (rman_res_t)res->Data.Address32.Address.Minimum;
			    max = (rman_res_t)res->Data.Address32.Address.Maximum;
			break;
		case ACPI_RESOURCE_TYPE_ADDRESS64:
			    min = (rman_res_t)res->Data.Address64.Address.Minimum;
			    max = (rman_res_t)res->Data.Address64.Address.Maximum;
			break;
		default:
			return (AE_OK);
	}

	sc = device_get_softc(dev);

	error = rman_manage_region(&sc->base.mem_rman, min, max);
	if (error) {
		device_printf(dev, "unable to allocate %lx-%lx range\n", min, max);
		return (AE_NOT_FOUND);
	}
	device_printf(dev, "allocating %lx-%lx range\n", min, max);

	return (AE_OK);
}

static int
generic_pcie_acpi_read_ivar(device_t dev, device_t child, int index,
    uintptr_t *result)
{
	ACPI_HANDLE handle;
	struct generic_pcie_acpi_softc *sc;
	int secondary_bus;

	sc = device_get_softc(dev);

	if (index == PCIB_IVAR_BUS) {
		handle = acpi_get_handle(dev);
		if (ACPI_FAILURE(acpi_GetInteger(handle, "_BBN", &secondary_bus)))
			secondary_bus = sc->base.ecam * 0x80;
		*result = secondary_bus;
		return (0);
	}

	if (index == PCIB_IVAR_DOMAIN) {
		*result = sc->base.ecam;
		return (0);
	}

	if (bootverbose)
		device_printf(dev, "ERROR: Unknown index %d.\n", index);
	return (ENOENT);
}

static int
generic_pcie_acpi_route_interrupt(device_t bus, device_t dev, int pin)
{
	struct generic_pcie_acpi_softc *sc;

	sc = device_get_softc(bus);

	return (acpi_pcib_route_interrupt(bus, dev, pin, &sc->ap_prt));
}

static struct resource *
pci_host_generic_acpi_alloc_resource(device_t dev, device_t child, int type,
    int *rid, rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct resource *res = NULL;

#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	struct generic_pcie_acpi_softc *sc;

	if (type == PCI_RES_BUS) {
		sc = device_get_softc(dev);
		return (pci_domain_alloc_bus(sc->base.ecam, child, rid, start,
		    end, count, flags));
	}
#endif

	if (type == SYS_RES_MEMORY)
		res = pci_host_generic_core_alloc_resource(dev, child, type,
		    rid, start, end, count, flags);

	if (res == NULL)
		res = bus_generic_alloc_resource(dev, child, type, rid, start, end,
		    count, flags);

	return (res);
}

static int
generic_pcie_acpi_activate_resource(device_t dev, device_t child, int type,
    int rid, struct resource *r)
{
	struct generic_pcie_acpi_softc *sc;
	int res;

	sc = device_get_softc(dev);

	if ((res = rman_activate_resource(r)) != 0)
		return (res);

	res = BUS_ACTIVATE_RESOURCE(device_get_parent(dev), child, type, rid,r);
	return (res);
}

static int
generic_pcie_acpi_deactivate_resource(device_t dev, device_t child, int type,
    int rid, struct resource *r)
{
	int res;

	if ((res = rman_deactivate_resource(r)) != 0)
		return (res);

	res = BUS_DEACTIVATE_RESOURCE(device_get_parent(dev), child, type,
	    rid, r);
	return (res);
}

static int
generic_pcie_acpi_alloc_msi(device_t pci, device_t child, int count,
    int maxcount, int *irqs)
{

#if defined(INTRNG)
	return (intr_alloc_msi(pci, child, ACPI_MSI_XREF, count, maxcount,
	    irqs));
#else
	return (ENXIO);
#endif
}

static int
generic_pcie_acpi_release_msi(device_t pci, device_t child, int count,
    int *irqs)
{

#if defined(INTRNG)
	return (intr_release_msi(pci, child, ACPI_MSI_XREF, count, irqs));
#else
	return (ENXIO);
#endif
}

static int
generic_pcie_acpi_map_msi(device_t pci, device_t child, int irq, uint64_t *addr,
    uint32_t *data)
{

#if defined(INTRNG)
	return (intr_map_msi(pci, child, ACPI_MSI_XREF, irq, addr, data));
#else
	return (ENXIO);
#endif
}

static int
generic_pcie_acpi_alloc_msix(device_t pci, device_t child, int *irq)
{

#if defined(INTRNG)
	return (intr_alloc_msix(pci, child, ACPI_MSI_XREF, irq));
#else
	return (ENXIO);
#endif
}

static int
generic_pcie_acpi_release_msix(device_t pci, device_t child, int irq)
{

#if defined(INTRNG)
	return (intr_release_msix(pci, child, ACPI_MSI_XREF, irq));
#else
	return (ENXIO);
#endif
}

static int
generic_pcie_acpi_get_id(device_t pci, device_t child, enum pci_id_type type,
    uintptr_t *id)
{
	struct generic_pcie_acpi_softc *sc;
	int err;

	/* Use the PCI RID to find the MSI ID */
	if (type == PCI_ID_MSI) {
		sc = device_get_softc(pci);
		type = PCI_ID_RID;
		err = pcib_get_id(pci, child, type, id);
		if (err != 0)
			return (err);
		*id |= sc->base.ecam << 16;
		return (0);
	}

	return (pcib_get_id(pci, child, type, id));
}

static device_method_t generic_pcie_acpi_methods[] = {
	DEVMETHOD(device_probe,		generic_pcie_acpi_probe),
	DEVMETHOD(device_attach,	pci_host_generic_acpi_attach),
	DEVMETHOD(bus_alloc_resource,	pci_host_generic_acpi_alloc_resource),
	DEVMETHOD(bus_activate_resource, generic_pcie_acpi_activate_resource),
	DEVMETHOD(bus_deactivate_resource, generic_pcie_acpi_deactivate_resource),
	DEVMETHOD(bus_read_ivar,	generic_pcie_acpi_read_ivar),

	/* pcib interface */
	DEVMETHOD(pcib_route_interrupt,	generic_pcie_acpi_route_interrupt),
	DEVMETHOD(pcib_alloc_msi,	generic_pcie_acpi_alloc_msi),
	DEVMETHOD(pcib_release_msi,	generic_pcie_acpi_release_msi),
	DEVMETHOD(pcib_alloc_msix,	generic_pcie_acpi_alloc_msix),
	DEVMETHOD(pcib_release_msix,	generic_pcie_acpi_release_msix),
	DEVMETHOD(pcib_map_msi,		generic_pcie_acpi_map_msi),
	DEVMETHOD(pcib_get_id,		generic_pcie_acpi_get_id),

	DEVMETHOD_END
};

DEFINE_CLASS_1(pcib, generic_pcie_acpi_driver, generic_pcie_acpi_methods,
    sizeof(struct generic_pcie_acpi_softc), generic_pcie_core_driver);

static devclass_t generic_pcie_acpi_devclass;

DRIVER_MODULE(pcib, acpi, generic_pcie_acpi_driver, generic_pcie_acpi_devclass,
    0, 0);
