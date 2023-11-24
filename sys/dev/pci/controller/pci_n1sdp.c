/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Andrew Turner
 * Copyright (c) 2019 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_page.h>
#include <vm/vm_phys.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpi_pcibvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>
#include <dev/pci/pci_host_generic.h>
#include <dev/pci/pci_host_generic_acpi.h>

#include "pcib_if.h"

#define	AP_NS_SHARED_MEM_BASE	0x06000000
#define	N1SDP_MAX_SEGMENTS	2 /* Two PCIe root complex devices. */
#define	BDF_TABLE_SIZE		(16 * 1024)
#define	PCI_CFG_SPACE_SIZE	0x1000

_Static_assert(BDF_TABLE_SIZE >= PAGE_SIZE,
    "pci_n1sdp.c assumes a 4k or 16k page size when mapping the shared data");

struct pcie_discovery_data {
	uint32_t rc_base_addr;
	uint32_t nr_bdfs;
	uint32_t valid_bdfs[0];
};

struct generic_pcie_n1sdp_softc {
	struct generic_pcie_acpi_softc acpi;
	struct pcie_discovery_data *n1_discovery_data;
	bus_space_handle_t n1_bsh;
};

static int
n1sdp_init(struct generic_pcie_n1sdp_softc *sc)
{
	struct pcie_discovery_data *shared_data;
	vm_offset_t vaddr;
	vm_paddr_t paddr_rc;
	vm_paddr_t paddr;
	vm_page_t m[BDF_TABLE_SIZE / PAGE_SIZE];
	int table_count;
	int bdfs_size;
	int error, i;

	paddr = AP_NS_SHARED_MEM_BASE + sc->acpi.segment * BDF_TABLE_SIZE;
	vm_phys_fictitious_reg_range(paddr, paddr + BDF_TABLE_SIZE,
	    VM_MEMATTR_UNCACHEABLE);

	for (i = 0; i < nitems(m); i++) {
		m[i] = PHYS_TO_VM_PAGE(paddr + i * PAGE_SIZE);
		MPASS(m[i] != NULL);
	}

	vaddr = kva_alloc((vm_size_t)BDF_TABLE_SIZE);
	if (vaddr == 0) {
		printf("%s: Can't allocate KVA memory.", __func__);
		error = ENXIO;
		goto out;
	}
	pmap_qenter(vaddr, m, nitems(m));

	shared_data = (struct pcie_discovery_data *)vaddr;
	paddr_rc = (vm_offset_t)shared_data->rc_base_addr;
	error = bus_space_map(sc->acpi.base.res->r_bustag, paddr_rc,
	    PCI_CFG_SPACE_SIZE, 0, &sc->n1_bsh);
	if (error != 0)
		goto out_pmap;

	bdfs_size = sizeof(struct pcie_discovery_data) +
	    sizeof(uint32_t) * shared_data->nr_bdfs;
	sc->n1_discovery_data = malloc(bdfs_size, M_DEVBUF,
	    M_WAITOK | M_ZERO);
	memcpy(sc->n1_discovery_data, shared_data, bdfs_size);

	if (bootverbose) {
		table_count = sc->n1_discovery_data->nr_bdfs;
		for (i = 0; i < table_count; i++)
			printf("valid bdf %x\n",
			    sc->n1_discovery_data->valid_bdfs[i]);
	}

out_pmap:
	pmap_qremove(vaddr, nitems(m));
	kva_free(vaddr, (vm_size_t)BDF_TABLE_SIZE);

out:
	vm_phys_fictitious_unreg_range(paddr, paddr + BDF_TABLE_SIZE);
	return (error);
}

static int
n1sdp_check_bdf(struct generic_pcie_n1sdp_softc *sc,
    u_int bus, u_int slot, u_int func)
{
	int table_count;
	int bdf;
	int i;

	bdf = PCIE_ADDR_OFFSET(bus, slot, func, 0);
	if (bdf == 0)
		return (1);

	table_count = sc->n1_discovery_data->nr_bdfs;

	for (i = 0; i < table_count; i++)
		if (bdf == sc->n1_discovery_data->valid_bdfs[i])
			return (1);

	return (0);
}

static int
n1sdp_pcie_acpi_probe(device_t dev)
{
	ACPI_DEVICE_INFO *devinfo;
	ACPI_TABLE_HEADER *hdr;
	ACPI_STATUS status;
	ACPI_HANDLE h;
	int root;

	if (acpi_disabled("pcib") || (h = acpi_get_handle(dev)) == NULL ||
	    ACPI_FAILURE(AcpiGetObjectInfo(h, &devinfo)))
		return (ENXIO);

	root = (devinfo->Flags & ACPI_PCI_ROOT_BRIDGE) != 0;
	AcpiOsFree(devinfo);
	if (!root)
		return (ENXIO);

	/* TODO: Move this to an ACPI quirk? */
	status = AcpiGetTable(ACPI_SIG_MCFG, 1, &hdr);
	if (ACPI_FAILURE(status))
		return (ENXIO);

	if (memcmp(hdr->OemId, "ARMLTD", ACPI_OEM_ID_SIZE) != 0 ||
	    memcmp(hdr->OemTableId, "ARMN1SDP", ACPI_OEM_TABLE_ID_SIZE) != 0 ||
	    hdr->OemRevision != 0x20181101)
		return (ENXIO);

	device_set_desc(dev, "ARM N1SDP PCI host controller");
	return (BUS_PROBE_DEFAULT);
}

static int
n1sdp_pcie_acpi_attach(device_t dev)
{
	struct generic_pcie_n1sdp_softc *sc;
	ACPI_HANDLE handle;
	ACPI_STATUS status;
	int err;

	err = pci_host_generic_acpi_init(dev);
	if (err != 0)
		return (err);

	sc = device_get_softc(dev);
	handle = acpi_get_handle(dev);

	/* Get PCI Segment (domain) needed for IOMMU space remap. */
	status = acpi_GetInteger(handle, "_SEG", &sc->acpi.segment);
	if (ACPI_FAILURE(status)) {
		device_printf(dev, "No _SEG for PCI Bus\n");
		return (ENXIO);
	}

	if (sc->acpi.segment >= N1SDP_MAX_SEGMENTS) {
		device_printf(dev, "Unknown PCI Bus segment (domain) %d\n",
		    sc->acpi.segment);
		return (ENXIO);
	}

	err = n1sdp_init(sc);
	if (err)
		return (err);

	device_add_child(dev, "pci", -1);
	return (bus_generic_attach(dev));
}

static int
n1sdp_get_bus_space(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
    bus_space_tag_t *bst, bus_space_handle_t *bsh, bus_size_t *offset)
{
	struct generic_pcie_n1sdp_softc *sc;

	sc = device_get_softc(dev);

	if (n1sdp_check_bdf(sc, bus, slot, func) == 0)
		return (EINVAL);

	if (bus == sc->acpi.base.bus_start) {
		if (slot != 0 || func != 0)
			return (EINVAL);
		*bsh = sc->n1_bsh;
	} else {
		*bsh = rman_get_bushandle(sc->acpi.base.res);
	}

	*bst = rman_get_bustag(sc->acpi.base.res);
	*offset = PCIE_ADDR_OFFSET(bus - sc->acpi.base.bus_start, slot, func,
	    reg);

	return (0);
}

static uint32_t
n1sdp_pcie_read_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, int bytes)
{
	struct generic_pcie_n1sdp_softc *sc_n1sdp;
	struct generic_pcie_acpi_softc *sc_acpi;
	struct generic_pcie_core_softc *sc;
	bus_space_handle_t h;
	bus_space_tag_t t;
	bus_size_t offset;
	uint32_t data;

	sc_n1sdp = device_get_softc(dev);
	sc_acpi = &sc_n1sdp->acpi;
	sc = &sc_acpi->base;

	if ((bus < sc->bus_start) || (bus > sc->bus_end))
		return (~0U);
	if ((slot > PCI_SLOTMAX) || (func > PCI_FUNCMAX) ||
	    (reg > PCIE_REGMAX))
		return (~0U);

	if (n1sdp_get_bus_space(dev, bus, slot, func, reg, &t, &h, &offset) !=0)
		return (~0U);

	data = bus_space_read_4(t, h, offset & ~3);

	switch (bytes) {
	case 1:
		data >>= (offset & 3) * 8;
		data &= 0xff;
		break;
	case 2:
		data >>= (offset & 3) * 8;
		data = le16toh(data);
		break;
	case 4:
		data = le32toh(data);
		break;
	default:
		return (~0U);
	}

	return (data);
}

static void
n1sdp_pcie_write_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, uint32_t val, int bytes)
{
	struct generic_pcie_n1sdp_softc *sc_n1sdp;
	struct generic_pcie_acpi_softc *sc_acpi;
	struct generic_pcie_core_softc *sc;
	bus_space_handle_t h;
	bus_space_tag_t t;
	bus_size_t offset;
	uint32_t data;

	sc_n1sdp = device_get_softc(dev);
	sc_acpi = &sc_n1sdp->acpi;
	sc = &sc_acpi->base;

	if ((bus < sc->bus_start) || (bus > sc->bus_end))
		return;
	if ((slot > PCI_SLOTMAX) || (func > PCI_FUNCMAX) ||
	    (reg > PCIE_REGMAX))
		return;

	if (n1sdp_get_bus_space(dev, bus, slot, func, reg, &t, &h, &offset) !=0)
		return;

	data = bus_space_read_4(t, h, offset & ~3);

	switch (bytes) {
	case 1:
		data &= ~(0xff << ((offset & 3) * 8));
		data |= (val & 0xff) << ((offset & 3) * 8);
		break;
	case 2:
		data &= ~(0xffff << ((offset & 3) * 8));
		data |= (val & 0xffff) << ((offset & 3) * 8);
		break;
	case 4:
		data = val;
		break;
	default:
		return;
	}

	bus_space_write_4(t, h, offset & ~3, data);
}

static device_method_t n1sdp_pcie_acpi_methods[] = {
	DEVMETHOD(device_probe,		n1sdp_pcie_acpi_probe),
	DEVMETHOD(device_attach,	n1sdp_pcie_acpi_attach),

	/* pcib interface */
	DEVMETHOD(pcib_read_config,	n1sdp_pcie_read_config),
	DEVMETHOD(pcib_write_config,	n1sdp_pcie_write_config),

	DEVMETHOD_END
};

DEFINE_CLASS_1(pcib, n1sdp_pcie_acpi_driver, n1sdp_pcie_acpi_methods,
    sizeof(struct generic_pcie_n1sdp_softc), generic_pcie_acpi_driver);

DRIVER_MODULE(n1sdp_pcib, acpi, n1sdp_pcie_acpi_driver, 0, 0);
