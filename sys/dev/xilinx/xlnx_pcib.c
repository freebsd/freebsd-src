/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Ruslan Bukin <br@bsdpad.com>
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

#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/kernel.h>
#include <sys/rman.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/cpuset.h>
#include <sys/mutex.h>
#include <sys/proc.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_host_generic.h>
#include <dev/pci/pci_host_generic_fdt.h>
#include <dev/pci/pcib_private.h>

#include "xlnx_pcib.h"

#include "ofw_bus_if.h"
#include "msi_if.h"
#include "pcib_if.h"
#include "pic_if.h"

#define	XLNX_PCIB_MAX_MSI	64

static int xlnx_pcib_fdt_attach(device_t);
static int xlnx_pcib_fdt_probe(device_t);
static int xlnx_pcib_fdt_get_id(device_t, device_t, enum pci_id_type,
    uintptr_t *);
static void xlnx_pcib_msi_mask(device_t dev, struct intr_irqsrc *isrc,
    bool mask);

struct xlnx_pcib_softc {
	struct generic_pcie_fdt_softc	fdt_sc;
	struct resource			*res[4];
	struct mtx			mtx;
	vm_offset_t			msi_page;
	struct xlnx_pcib_irqsrc		*isrcs;
	device_t			dev;
	void				*intr_cookie[3];
};

static struct resource_spec xlnx_pcib_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		1,	RF_ACTIVE },
	{ SYS_RES_IRQ,		2,	RF_ACTIVE },
	{ -1, 0 }
};

struct xlnx_pcib_irqsrc {
	struct intr_irqsrc	isrc;
	u_int			irq;
#define	XLNX_IRQ_FLAG_USED	(1 << 0)
	u_int			flags;
};

static void
xlnx_pcib_clear_err_interrupts(struct generic_pcie_core_softc *sc)
{
	uint32_t reg;

	reg = bus_read_4(sc->res, XLNX_PCIE_RPERRFRR);

	if (reg & RPERRFRR_VALID) {
		device_printf(sc->dev, "Requested ID: %x\n",
		    reg & RPERRFRR_REQ_ID_M);
		bus_write_4(sc->res, XLNX_PCIE_RPERRFRR, ~0U);
	}
}

static int
xlnx_pcib_intr(void *arg)
{
	struct generic_pcie_fdt_softc *fdt_sc;
	struct generic_pcie_core_softc *sc;
	struct xlnx_pcib_softc *xlnx_sc;
	uint32_t val, mask, status;

	xlnx_sc = arg;
	fdt_sc = &xlnx_sc->fdt_sc;
	sc = &fdt_sc->base;

	val = bus_read_4(sc->res, XLNX_PCIE_IDR);
	mask = bus_read_4(sc->res, XLNX_PCIE_IMR);

	status = val & mask;
	if (!status)
		return (FILTER_HANDLED);

	if (status & IMR_LINK_DOWN)
		device_printf(sc->dev, "Link down");

	if (status & IMR_HOT_RESET)
		device_printf(sc->dev, "Hot reset");

	if (status & IMR_CORRECTABLE)
		xlnx_pcib_clear_err_interrupts(sc);

	if (status & IMR_FATAL)
		xlnx_pcib_clear_err_interrupts(sc);

	if (status & IMR_NON_FATAL)
		xlnx_pcib_clear_err_interrupts(sc);

	if (status & IMR_MSI) {
		device_printf(sc->dev, "MSI interrupt");

		/* FIFO mode MSI not implemented. */
	}

	if (status & IMR_INTX) {
		device_printf(sc->dev, "INTx received");

		/* Not implemented. */
	}

	if (status & IMR_SLAVE_UNSUPP_REQ)
		device_printf(sc->dev, "Slave unsupported request");

	if (status & IMR_SLAVE_UNEXP_COMPL)
		device_printf(sc->dev, "Slave unexpected completion");

	if (status & IMR_SLAVE_COMPL_TIMOUT)
		device_printf(sc->dev, "Slave completion timeout");

	if (status & IMR_SLAVE_ERROR_POISON)
		device_printf(sc->dev, "Slave error poison");

	if (status & IMR_SLAVE_COMPL_ABORT)
		device_printf(sc->dev, "Slave completion abort");

	if (status & IMR_SLAVE_ILLEG_BURST)
		device_printf(sc->dev, "Slave illegal burst");

	if (status & IMR_MASTER_DECERR)
		device_printf(sc->dev, "Master decode error");

	if (status & IMR_MASTER_SLVERR)
		device_printf(sc->dev, "Master slave error");

	bus_write_4(sc->res, XLNX_PCIE_IDR, val);

	return (FILTER_HANDLED);
}

static void
xlnx_pcib_handle_msi_intr(void *arg, int msireg)
{ 
	struct generic_pcie_fdt_softc *fdt_sc;
	struct generic_pcie_core_softc *sc;
	struct xlnx_pcib_softc *xlnx_sc;
	struct xlnx_pcib_irqsrc *xi;
	struct trapframe *tf;
	int irq;
	int reg;
	int i;

	xlnx_sc = arg;
	fdt_sc = &xlnx_sc->fdt_sc;
	sc = &fdt_sc->base;
	tf = curthread->td_intr_frame;

	do {
		reg = bus_read_4(sc->res, msireg);

		for (i = 0; i < sizeof(uint32_t) * 8; i++) {
			if (reg & (1 << i)) {
				bus_write_4(sc->res, msireg, (1 << i));

				irq = i;
				if (msireg == XLNX_PCIE_RPMSIID2)
					irq += 32;

				xi = &xlnx_sc->isrcs[irq];
				if (intr_isrc_dispatch(&xi->isrc, tf) != 0) {
					/* Disable stray. */
					xlnx_pcib_msi_mask(sc->dev,
					    &xi->isrc, 1);
					device_printf(sc->dev,
					    "Stray irq %u disabled\n", irq);
				}
			}
		}
	} while (reg != 0);
}

static int
xlnx_pcib_msi0_intr(void *arg)
{

	xlnx_pcib_handle_msi_intr(arg, XLNX_PCIE_RPMSIID1);

	return (FILTER_HANDLED);
}

static int
xlnx_pcib_msi1_intr(void *arg)
{

	xlnx_pcib_handle_msi_intr(arg, XLNX_PCIE_RPMSIID2);

	return (FILTER_HANDLED);
}

static int
xlnx_pcib_register_msi(struct xlnx_pcib_softc *sc)
{
	const char *name;
	int error;
	int irq;

	sc->isrcs = malloc(sizeof(*sc->isrcs) * XLNX_PCIB_MAX_MSI, M_DEVBUF,
	    M_WAITOK | M_ZERO);

	name = device_get_nameunit(sc->dev);

	for (irq = 0; irq < XLNX_PCIB_MAX_MSI; irq++) {
		sc->isrcs[irq].irq = irq;
		error = intr_isrc_register(&sc->isrcs[irq].isrc,
		    sc->dev, 0, "%s,%u", name, irq);
		if (error != 0)
			return (error); /* XXX deregister ISRCs */
	}

	if (intr_msi_register(sc->dev,
	    OF_xref_from_node(ofw_bus_get_node(sc->dev))) != 0)
		return (ENXIO);

	return (0);
}

static void
xlnx_pcib_init(struct xlnx_pcib_softc *sc)
{
	bus_addr_t addr;
	int reg;

	/* Disable interrupts. */
	bus_write_4(sc->res[0], XLNX_PCIE_IMR, 0);

	/* Clear pending interrupts.*/
	reg = bus_read_4(sc->res[0], XLNX_PCIE_IDR);
	bus_write_4(sc->res[0], XLNX_PCIE_IDR, reg);

	/* Setup an MSI page. */
	sc->msi_page = kmem_alloc_contig(PAGE_SIZE, M_WAITOK, 0,
	    BUS_SPACE_MAXADDR, PAGE_SIZE, 0, VM_MEMATTR_DEFAULT);
	addr = vtophys(sc->msi_page);
	bus_write_4(sc->res[0], XLNX_PCIE_RPMSIBR1, (addr >> 32));
	bus_write_4(sc->res[0], XLNX_PCIE_RPMSIBR2, (addr >>  0));

	/* Enable the bridge. */
	reg = bus_read_4(sc->res[0], XLNX_PCIE_RPSCR);
	reg |= RPSCR_BE;
	bus_write_4(sc->res[0], XLNX_PCIE_RPSCR, reg);

	/* Enable interrupts. */
	reg = IMR_LINK_DOWN
		| IMR_HOT_RESET
		| IMR_CFG_COMPL_STATUS_M
		| IMR_CFG_TIMEOUT
		| IMR_CORRECTABLE
		| IMR_NON_FATAL
		| IMR_FATAL
		| IMR_INTX
		| IMR_MSI
		| IMR_SLAVE_UNSUPP_REQ
		| IMR_SLAVE_UNEXP_COMPL
		| IMR_SLAVE_COMPL_TIMOUT
		| IMR_SLAVE_ERROR_POISON
		| IMR_SLAVE_COMPL_ABORT
		| IMR_SLAVE_ILLEG_BURST
		| IMR_MASTER_DECERR
		| IMR_MASTER_SLVERR;
	bus_write_4(sc->res[0], XLNX_PCIE_IMR, reg);
}

static int
xlnx_pcib_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "xlnx,xdma-host-3.00")) {
		device_set_desc(dev, "Xilinx XDMA PCIe Controller");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
xlnx_pcib_fdt_attach(device_t dev)
{
	struct xlnx_pcib_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	mtx_init(&sc->mtx, "msi_mtx", NULL, MTX_DEF);

	if (bus_alloc_resources(dev, xlnx_pcib_spec, sc->res)) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	/* Setup MISC interrupt handler. */
	error = bus_setup_intr(dev, sc->res[1], INTR_TYPE_MISC | INTR_MPSAFE,
	    xlnx_pcib_intr, NULL, sc, &sc->intr_cookie[0]);
	if (error != 0) {
		device_printf(dev, "could not setup interrupt handler.\n");
		return (ENXIO);
	}

	/* Setup MSI0 interrupt handler. */
	error = bus_setup_intr(dev, sc->res[2], INTR_TYPE_MISC | INTR_MPSAFE,
	    xlnx_pcib_msi0_intr, NULL, sc, &sc->intr_cookie[1]);
	if (error != 0) {
		device_printf(dev, "could not setup interrupt handler.\n");
		return (ENXIO);
	}

	/* Setup MSI1 interrupt handler. */
	error = bus_setup_intr(dev, sc->res[3], INTR_TYPE_MISC | INTR_MPSAFE,
	    xlnx_pcib_msi1_intr, NULL, sc, &sc->intr_cookie[2]);
	if (error != 0) {
		device_printf(dev, "could not setup interrupt handler.\n");
		return (ENXIO);
	}

	xlnx_pcib_init(sc);

	/*
	 * Allow the core driver to map registers.
	 * We will be accessing the device memory using core_softc.
	 */
	bus_release_resources(dev, xlnx_pcib_spec, sc->res);

	error = xlnx_pcib_register_msi(sc);
	if (error)
		return (error);

	return (pci_host_generic_fdt_attach(dev));
}

static int
xlnx_pcib_fdt_get_id(device_t pci, device_t child, enum pci_id_type type,
    uintptr_t *id)
{
	phandle_t node;
	int bsf;

	if (type != PCI_ID_MSI)
		return (pcib_get_id(pci, child, type, id));

	node = ofw_bus_get_node(pci);
	if (OF_hasprop(node, "msi-map"))
		return (generic_pcie_get_id(pci, child, type, id));

	bsf = pci_get_rid(child);
	*id = (pci_get_domain(child) << PCI_RID_DOMAIN_SHIFT) | bsf;

	return (0);
}

static int
xlnx_pcib_req_valid(struct generic_pcie_core_softc *sc,
    u_int bus, u_int slot, u_int func, u_int reg)
{
	bus_space_handle_t h;
	bus_space_tag_t t;
	uint32_t val;

	t = sc->bst;
	h = sc->bsh;

	if ((bus < sc->bus_start) || (bus > sc->bus_end))
		return (0);
	if ((slot > PCI_SLOTMAX) || (func > PCI_FUNCMAX) ||
	    (reg > PCIE_REGMAX))
		return (0);

	if (bus == 0 && slot > 0)
		return (0);

	val = bus_space_read_4(t, h, XLNX_PCIE_PHYSCR);
	if ((val & PHYSCR_LINK_UP) == 0) {
		/* Link is down */
		return (0);
	}

	/* Valid */

	return (1);
}

static uint32_t
xlnx_pcib_read_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, int bytes)
{
	struct generic_pcie_fdt_softc *fdt_sc;
	struct xlnx_pcib_softc *xlnx_sc;
	struct generic_pcie_core_softc *sc;
	bus_space_handle_t h;
	bus_space_tag_t t;
	uint64_t offset;
	uint32_t data;

	xlnx_sc = device_get_softc(dev);
	fdt_sc = &xlnx_sc->fdt_sc;
	sc = &fdt_sc->base;

	if (!xlnx_pcib_req_valid(sc, bus, slot, func, reg))
		return (~0U);

	offset = PCIE_ADDR_OFFSET(bus - sc->bus_start, slot, func, reg);
	t = sc->bst;
	h = sc->bsh;

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
xlnx_pcib_write_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, uint32_t val, int bytes)
{
	struct generic_pcie_fdt_softc *fdt_sc;
	struct xlnx_pcib_softc *xlnx_sc;
	struct generic_pcie_core_softc *sc;
	bus_space_handle_t h;
	bus_space_tag_t t;
	uint64_t offset;
	uint32_t data;

	xlnx_sc = device_get_softc(dev);
	fdt_sc = &xlnx_sc->fdt_sc;
	sc = &fdt_sc->base;

	if (!xlnx_pcib_req_valid(sc, bus, slot, func, reg))
		return;

	offset = PCIE_ADDR_OFFSET(bus - sc->bus_start, slot, func, reg);

	t = sc->bst;
	h = sc->bsh;

	/*
	 * 32-bit access used due to a bug in the Xilinx bridge that
	 * requires to write primary and secondary buses in one blast.
	 *
	 * TODO: This is probably wrong on big-endian.
	 */
	switch (bytes) {
	case 1:
		data = bus_space_read_4(t, h, offset & ~3);
		data &= ~(0xff << ((offset & 3) * 8));
		data |= (val & 0xff) << ((offset & 3) * 8);
		bus_space_write_4(t, h, offset & ~3, htole32(data));
		break;
	case 2:
		data = bus_space_read_4(t, h, offset & ~3);
		data &= ~(0xffff << ((offset & 3) * 8));
		data |= (val & 0xffff) << ((offset & 3) * 8);
		bus_space_write_4(t, h, offset & ~3, htole32(data));
		break;
	case 4:
		bus_space_write_4(t, h, offset, htole32(val));
		break;
	default:
		return;
	}
}

static int
xlnx_pcib_alloc_msi(device_t pci, device_t child, int count, int maxcount,
    int *irqs)
{
	phandle_t msi_parent;

	ofw_bus_msimap(ofw_bus_get_node(pci), pci_get_rid(child), &msi_parent,
	    NULL);
	msi_parent = OF_xref_from_node(ofw_bus_get_node(pci));
	return (intr_alloc_msi(pci, child, msi_parent, count, maxcount,
	    irqs));
}

static int
xlnx_pcib_release_msi(device_t pci, device_t child, int count, int *irqs)
{
	phandle_t msi_parent;

	ofw_bus_msimap(ofw_bus_get_node(pci), pci_get_rid(child), &msi_parent,
	    NULL);
	msi_parent = OF_xref_from_node(ofw_bus_get_node(pci));
	return (intr_release_msi(pci, child, msi_parent, count, irqs));
}

static int
xlnx_pcib_map_msi(device_t pci, device_t child, int irq, uint64_t *addr,
    uint32_t *data)
{
	phandle_t msi_parent;

	ofw_bus_msimap(ofw_bus_get_node(pci), pci_get_rid(child), &msi_parent,
	    NULL);
	msi_parent = OF_xref_from_node(ofw_bus_get_node(pci));
	return (intr_map_msi(pci, child, msi_parent, irq, addr, data));
}

static int
xlnx_pcib_msi_alloc_msi(device_t dev, device_t child, int count, int maxcount,
    device_t *pic, struct intr_irqsrc **srcs)
{
	struct xlnx_pcib_softc *sc;
	int irq, end_irq, i;
	bool found;

	sc = device_get_softc(dev);

	mtx_lock(&sc->mtx);

	found = false;

	for (irq = 0; (irq + count - 1) < XLNX_PCIB_MAX_MSI; irq++) {
		/* Assume the range is valid. */
		found = true;

		/* Check this range is valid. */
		for (end_irq = irq; end_irq < irq + count; end_irq++) {
			if (sc->isrcs[end_irq].flags & XLNX_IRQ_FLAG_USED) {
				/* This is already used. */
				found = false;
				break;
			}
		}

		if (found)
			break;
	}

	if (!found || irq == (XLNX_PCIB_MAX_MSI - 1)) {
		/* Not enough interrupts were found. */
		mtx_unlock(&sc->mtx);
		return (ENXIO);
	}

	/* Mark the interrupt as used. */
	for (i = 0; i < count; i++)
		sc->isrcs[irq + i].flags |= XLNX_IRQ_FLAG_USED;

	mtx_unlock(&sc->mtx);

	for (i = 0; i < count; i++)
		srcs[i] = (struct intr_irqsrc *)&sc->isrcs[irq + i];

	*pic = device_get_parent(dev);

	return (0);
}

static int
xlnx_pcib_msi_release_msi(device_t dev, device_t child, int count,
    struct intr_irqsrc **isrc)
{
	struct xlnx_pcib_softc *sc;
	struct xlnx_pcib_irqsrc *xi;
	int i;

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);
	for (i = 0; i < count; i++) {
		xi = (struct xlnx_pcib_irqsrc *)isrc[i];

		KASSERT(xi->flags & XLNX_IRQ_FLAG_USED,
		    ("%s: Releasing an unused MSI interrupt", __func__));

		xi->flags &= ~XLNX_IRQ_FLAG_USED;
	}

	mtx_unlock(&sc->mtx);
	return (0);
}

static int
xlnx_pcib_msi_map_msi(device_t dev, device_t child, struct intr_irqsrc *isrc,
    uint64_t *addr, uint32_t *data)
{
	struct xlnx_pcib_softc *sc;
	struct xlnx_pcib_irqsrc *xi;

	sc = device_get_softc(dev);
	xi = (struct xlnx_pcib_irqsrc *)isrc;

	*addr = vtophys(sc->msi_page);
	*data = xi->irq;

	return (0);
}

static void
xlnx_pcib_msi_mask(device_t dev, struct intr_irqsrc *isrc, bool mask)
{
	struct generic_pcie_fdt_softc *fdt_sc;
	struct generic_pcie_core_softc *sc;
	struct xlnx_pcib_softc *xlnx_sc;
	struct xlnx_pcib_irqsrc *xi;
	uint32_t msireg, irq;
	uint32_t reg;

	xlnx_sc = device_get_softc(dev);
	fdt_sc = &xlnx_sc->fdt_sc;
	sc = &fdt_sc->base;

	xi = (struct xlnx_pcib_irqsrc *)isrc;

	irq = xi->irq;
	if (irq < 32)
		msireg = XLNX_PCIE_RPMSIID1_MASK;
	else
		msireg = XLNX_PCIE_RPMSIID2_MASK;

	reg = bus_read_4(sc->res, msireg);
	if (mask)
		reg &= ~(1 << irq);
	else
		reg |= (1 << irq);
	bus_write_4(sc->res, msireg, reg);
}

static void
xlnx_pcib_msi_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{

	xlnx_pcib_msi_mask(dev, isrc, true);
}

static void
xlnx_pcib_msi_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{

	xlnx_pcib_msi_mask(dev, isrc, false);
}

static void
xlnx_pcib_msi_post_filter(device_t dev, struct intr_irqsrc *isrc)
{

}

static void
xlnx_pcib_msi_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{

	xlnx_pcib_msi_mask(dev, isrc, false);
}

static void
xlnx_pcib_msi_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{

	xlnx_pcib_msi_mask(dev, isrc, true);
}

static int
xlnx_pcib_msi_setup_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{

	return (0);
}

static int
xlnx_pcib_msi_teardown_intr(device_t dev, struct intr_irqsrc *isrc,
    struct resource *res, struct intr_map_data *data)
{

	return (0);
}

static device_method_t xlnx_pcib_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		xlnx_pcib_fdt_probe),
	DEVMETHOD(device_attach,	xlnx_pcib_fdt_attach),

	/* pcib interface */
	DEVMETHOD(pcib_get_id,		xlnx_pcib_fdt_get_id),
	DEVMETHOD(pcib_read_config,	xlnx_pcib_read_config),
	DEVMETHOD(pcib_write_config,	xlnx_pcib_write_config),
	DEVMETHOD(pcib_alloc_msi,	xlnx_pcib_alloc_msi),
	DEVMETHOD(pcib_release_msi,	xlnx_pcib_release_msi),
	DEVMETHOD(pcib_map_msi,		xlnx_pcib_map_msi),

	/* MSI interface */
	DEVMETHOD(msi_alloc_msi,		xlnx_pcib_msi_alloc_msi),
	DEVMETHOD(msi_release_msi,		xlnx_pcib_msi_release_msi),
	DEVMETHOD(msi_map_msi,			xlnx_pcib_msi_map_msi),

	/* Interrupt controller interface */
	DEVMETHOD(pic_disable_intr,		xlnx_pcib_msi_disable_intr),
	DEVMETHOD(pic_enable_intr,		xlnx_pcib_msi_enable_intr),
	DEVMETHOD(pic_setup_intr,		xlnx_pcib_msi_setup_intr),
	DEVMETHOD(pic_teardown_intr,		xlnx_pcib_msi_teardown_intr),
	DEVMETHOD(pic_post_filter,		xlnx_pcib_msi_post_filter),
	DEVMETHOD(pic_post_ithread,		xlnx_pcib_msi_post_ithread),
	DEVMETHOD(pic_pre_ithread,		xlnx_pcib_msi_pre_ithread),

	/* End */
	DEVMETHOD_END
};

DEFINE_CLASS_1(pcib, xlnx_pcib_fdt_driver, xlnx_pcib_fdt_methods,
    sizeof(struct xlnx_pcib_softc), generic_pcie_fdt_driver);

DRIVER_MODULE(xlnx_pcib, simplebus, xlnx_pcib_fdt_driver, 0, 0);
DRIVER_MODULE(xlnx_pcib, ofwbus, xlnx_pcib_fdt_driver, 0, 0);
