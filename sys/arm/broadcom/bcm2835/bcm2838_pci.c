/*-
 * SPDX-License-Identifier: ISC
 *
 * Copyright (c) 2020 Dr Robert Harvey Crowston <crowston@protonmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *
 * $FreeBSD$
 *
 */

/*
 * BCM2838-compatible PCI-express controller.
 *
 * Broadcom likes to give the same chip lots of different names. The name of
 * this driver is taken from the Raspberry Pi 4 Broadcom 2838 chip.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/intr.h>
#include <sys/mutex.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/pci/pci_host_generic.h>
#include <dev/pci/pci_host_generic_fdt.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include "pcib_if.h"
#include "msi_if.h"

#define PCI_ID_VAL3		0x43c
#define CLASS_SHIFT		0x10
#define SUBCLASS_SHIFT		0x8

#define REG_CONTROLLER_HW_REV			0x406c
#define REG_BRIDGE_CTRL				0x9210
#define BRIDGE_DISABLE_FLAG	0x1
#define BRIDGE_RESET_FLAG	0x2
#define REG_BRIDGE_SERDES_MODE			0x4204
#define REG_DMA_CONFIG				0x4008
#define REG_DMA_WINDOW_LOW			0x4034
#define REG_DMA_WINDOW_HIGH			0x4038
#define REG_DMA_WINDOW_1			0x403c
#define REG_BRIDGE_GISB_WINDOW			0x402c
#define REG_BRIDGE_STATE			0x4068
#define REG_BRIDGE_LINK_STATE			0x00bc
#define REG_BUS_WINDOW_LOW			0x400c
#define REG_BUS_WINDOW_HIGH			0x4010
#define REG_CPU_WINDOW_LOW			0x4070
#define REG_CPU_WINDOW_START_HIGH		0x4080
#define REG_CPU_WINDOW_END_HIGH			0x4084

#define REG_MSI_ADDR_LOW			0x4044
#define REG_MSI_ADDR_HIGH			0x4048
#define REG_MSI_CONFIG				0x404c
#define REG_MSI_CLR				0x4508
#define REG_MSI_MASK_CLR			0x4514
#define REG_MSI_RAISED				0x4500
#define REG_MSI_EOI				0x4060
#define NUM_MSI			32

#define REG_EP_CONFIG_CHOICE			0x9000
#define REG_EP_CONFIG_DATA			0x8000

/*
 * The system memory controller can address up to 16 GiB of physical memory
 * (although at time of writing the largest memory size available for purchase
 * is 8 GiB). However, the system DMA controller is capable of accessing only a
 * limited portion of the address space. Worse, the PCI-e controller has further
 * constraints for DMA, and those limitations are not wholly clear to the
 * author. NetBSD and Linux allow DMA on the lower 3 GiB of the physical memory,
 * but experimentation shows DMA performed above 960 MiB results in data
 * corruption with this driver. The limit of 960 MiB is taken from OpenBSD, but
 * apparently that value was chosen for satisfying a constraint of an unrelated
 * peripheral.
 *
 * Whatever the true maximum address, 960 MiB works.
 */
#define DMA_HIGH_LIMIT			0x3c000000
#define MAX_MEMORY_LOG2			0x21
#define REG_VALUE_DMA_WINDOW_LOW	(MAX_MEMORY_LOG2 - 0xf)
#define REG_VALUE_DMA_WINDOW_HIGH	0x0
#define DMA_WINDOW_ENABLE		0x3000
#define REG_VALUE_DMA_WINDOW_CONFIG	\
    (((MAX_MEMORY_LOG2 - 0xf) << 0x1b) | DMA_WINDOW_ENABLE)

#define REG_VALUE_MSI_CONFIG	0xffe06540

struct bcm_pcib_irqsrc {
	struct intr_irqsrc	isrc;
	u_int			irq;
	bool			allocated;
};

struct bcm_pcib_softc {
	struct generic_pcie_fdt_softc	base;
	device_t			dev;
	bus_dma_tag_t			dmat;
	struct mtx			config_mtx;
	struct mtx			msi_mtx;
	struct resource 		*msi_irq_res;
	void				*msi_intr_cookie;
	struct bcm_pcib_irqsrc		*msi_isrcs;
	pci_addr_t			msi_addr;
};

static struct ofw_compat_data compat_data[] = {
	{"brcm,bcm2711-pcie",			1},
	{"brcm,bcm7211-pcie",			1},
	{"brcm,bcm7445-pcie",			1},
	{NULL,					0}
};

static int
bcm_pcib_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev,
	    "BCM2838-compatible PCI-express controller");
	return (BUS_PROBE_DEFAULT);
}

static bus_dma_tag_t
bcm_pcib_get_dma_tag(device_t dev, device_t child)
{
	struct bcm_pcib_softc *sc;

	sc = device_get_softc(dev);
	return (sc->dmat);
}

static void
bcm_pcib_set_reg(struct bcm_pcib_softc *sc, uint32_t reg, uint32_t val)
{

	bus_space_write_4(sc->base.base.bst, sc->base.base.bsh, reg,
	    htole32(val));
}

static uint32_t
bcm_pcib_read_reg(struct bcm_pcib_softc *sc, uint32_t reg)
{

	return (le32toh(bus_space_read_4(sc->base.base.bst, sc->base.base.bsh,
	    reg)));
}

static void
bcm_pcib_reset_controller(struct bcm_pcib_softc *sc)
{
	uint32_t val;

	val = bcm_pcib_read_reg(sc, REG_BRIDGE_CTRL);
	val = val | BRIDGE_RESET_FLAG | BRIDGE_DISABLE_FLAG;
	bcm_pcib_set_reg(sc, REG_BRIDGE_CTRL, val);

	DELAY(100);

	val = bcm_pcib_read_reg(sc, REG_BRIDGE_CTRL);
	val = val & ~BRIDGE_RESET_FLAG;
	bcm_pcib_set_reg(sc, REG_BRIDGE_CTRL, val);

	DELAY(100);

	bcm_pcib_set_reg(sc, REG_BRIDGE_SERDES_MODE, 0);

	DELAY(100);
}

static void
bcm_pcib_enable_controller(struct bcm_pcib_softc *sc)
{
	uint32_t val;

	val = bcm_pcib_read_reg(sc, REG_BRIDGE_CTRL);
	val = val & ~BRIDGE_DISABLE_FLAG;
	bcm_pcib_set_reg(sc, REG_BRIDGE_CTRL, val);

	DELAY(100);
}

static int
bcm_pcib_check_ranges(device_t dev)
{
	struct bcm_pcib_softc *sc;
	struct pcie_range *ranges;
	int error = 0, i;

	sc = device_get_softc(dev);
	ranges = &sc->base.base.ranges[0];

	/* The first range needs to be non-zero. */
	if (ranges[0].size == 0) {
		device_printf(dev, "error: first outbound memory range "
		    "(pci addr: 0x%jx, cpu addr: 0x%jx) has zero size.\n",
		    ranges[0].pci_base, ranges[0].phys_base);
		error = ENXIO;
	}

	/*
	 * The controller can actually handle three distinct ranges, but we
	 * only implement support for one.
	 */
	for (i = 1; (bootverbose || error) && i < MAX_RANGES_TUPLES; ++i) {
		if (ranges[i].size > 0)
			device_printf(dev,
			    "note: outbound memory range %d (pci addr: 0x%jx, "
			    "cpu addr: 0x%jx, size: 0x%jx) will be ignored.\n",
			    i, ranges[i].pci_base, ranges[i].phys_base,
			    ranges[i].size);
	}

	return (error);
}

static const char *
bcm_pcib_link_state_string(uint32_t mode)
{

	switch(mode & PCIEM_LINK_STA_SPEED) {
	case 0:
		return ("not up");
	case 1:
		return ("2.5 GT/s");
	case 2:
		return ("5.0 GT/s");
	case 4:
		return ("8.0 GT/s");
	default:
		return ("unknown");
	}
}

static bus_addr_t
bcm_get_offset_and_prepare_config(struct bcm_pcib_softc *sc, u_int bus,
    u_int slot, u_int func, u_int reg)
{
	/*
	 * Config for an end point is only available through a narrow window for
	 * one end point at a time. We first tell the controller which end point
	 * we want, then access it through the window.
	 */
	uint32_t func_index;

	if (bus == 0 && slot == 0 && func == 0)
		/*
		 * Special case for root device; its config is always available
		 * through the zero-offset.
		 */
		return (reg);

	/* Tell the controller to show us the config in question. */
	func_index = PCIE_ADDR_OFFSET(bus, slot, func, 0);
	bcm_pcib_set_reg(sc, REG_EP_CONFIG_CHOICE, func_index);

	return (REG_EP_CONFIG_DATA + reg);
}

static bool
bcm_pcib_is_valid_quad(struct bcm_pcib_softc *sc, u_int bus, u_int slot,
    u_int func, u_int reg)
{

	if ((bus < sc->base.base.bus_start) || (bus > sc->base.base.bus_end))
		return (false);
	if ((slot > PCI_SLOTMAX) || (func > PCI_FUNCMAX) || (reg > PCIE_REGMAX))
		return (false);

	if (bus == 0 && slot == 0 && func == 0)
		return (true);
	if (bus == 0)
		/*
		 * Probing other slots and funcs on bus 0 will lock up the
		 * memory controller.
		 */
		return (false);

	return (true);
}

static uint32_t
bcm_pcib_read_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
    int bytes)
{
	struct bcm_pcib_softc *sc;
	bus_space_handle_t h;
	bus_space_tag_t	t;
	bus_addr_t offset;
	uint32_t data;

	sc = device_get_softc(dev);
	if (!bcm_pcib_is_valid_quad(sc, bus, slot, func, reg))
		return (~0U);

	mtx_lock(&sc->config_mtx);
	offset = bcm_get_offset_and_prepare_config(sc, bus, slot, func, reg);

	t = sc->base.base.bst;
	h = sc->base.base.bsh;

	switch (bytes) {
	case 1:
		data = bus_space_read_1(t, h, offset);
		break;
	case 2:
		data = le16toh(bus_space_read_2(t, h, offset));
		break;
	case 4:
		data = le32toh(bus_space_read_4(t, h, offset));
		break;
	default:
		data = ~0U;
		break;
	}

	mtx_unlock(&sc->config_mtx);
	return (data);
}

static void
bcm_pcib_write_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, uint32_t val, int bytes)
{
	struct bcm_pcib_softc *sc;
	bus_space_handle_t h;
	bus_space_tag_t	t;
	uint32_t offset;

	sc = device_get_softc(dev);
	if (!bcm_pcib_is_valid_quad(sc, bus, slot, func, reg))
		return;

	mtx_lock(&sc->config_mtx);
	offset = bcm_get_offset_and_prepare_config(sc, bus, slot, func, reg);

	t = sc->base.base.bst;
	h = sc->base.base.bsh;

	switch (bytes) {
	case 1:
		bus_space_write_1(t, h, offset, val);
		break;
	case 2:
		bus_space_write_2(t, h, offset, htole16(val));
		break;
	case 4:
		bus_space_write_4(t, h, offset, htole32(val));
		break;
	default:
		break;
	}

	mtx_unlock(&sc->config_mtx);
}

static void
bcm_pcib_msi_intr_process(struct bcm_pcib_softc *sc, uint32_t interrupt_bitmap,
    struct trapframe *tf)
{
	struct bcm_pcib_irqsrc *irqsrc;
	uint32_t bit, irq;

	while ((bit = ffs(interrupt_bitmap))) {
		irq = bit - 1;

		/* Acknowledge interrupt. */
		bcm_pcib_set_reg(sc, REG_MSI_CLR, 1 << irq);

		/* Send EOI. */
		bcm_pcib_set_reg(sc, REG_MSI_EOI, 1);

		/* Despatch to handler. */
		irqsrc = &sc->msi_isrcs[irq];
		if (intr_isrc_dispatch(&irqsrc->isrc, tf))
			device_printf(sc->dev,
			    "note: unexpected interrupt (%d) triggered.\n",
			    irq);

		/* Done with this interrupt. */
		interrupt_bitmap = interrupt_bitmap & ~(1 << irq);
	}
}

static int
bcm_pcib_msi_intr(void *arg)
{
	struct bcm_pcib_softc *sc;
	struct trapframe *tf;
	uint32_t interrupt_bitmap;

	sc = (struct bcm_pcib_softc *) arg;
	tf = curthread->td_intr_frame;

	while ((interrupt_bitmap = bcm_pcib_read_reg(sc, REG_MSI_RAISED)))
		bcm_pcib_msi_intr_process(sc, interrupt_bitmap, tf);

	return (FILTER_HANDLED);
}

static int
bcm_pcib_alloc_msi(device_t dev, device_t child, int count, int maxcount,
    device_t *pic, struct intr_irqsrc **srcs)
{
	struct bcm_pcib_softc *sc;
	int first_int, i;

	sc = device_get_softc(dev);
	mtx_lock(&sc->msi_mtx);

	/* Find a continguous region of free message-signalled interrupts. */
	for (first_int = 0; first_int + count < NUM_MSI; ) {
		for (i = first_int; i < first_int + count; ++i) {
			if (sc->msi_isrcs[i].allocated)
				goto next;
		}
		goto found;
next:
		first_int = i + 1;
	}

	/* No appropriate region available. */
	mtx_unlock(&sc->msi_mtx);
	device_printf(dev, "warning: failed to allocate %d MSI messages.\n",
	    count);
	return (ENXIO);

found:
	/* Mark the messages as in use. */
	for (i = 0; i < count; ++i) {
		sc->msi_isrcs[i + first_int].allocated = true;
		srcs[i] = &(sc->msi_isrcs[i + first_int].isrc);
	}

	mtx_unlock(&sc->msi_mtx);
	*pic = device_get_parent(dev);

	return (0);
}

static int
bcm_pcib_map_msi(device_t dev, device_t child, struct intr_irqsrc *isrc,
    uint64_t *addr, uint32_t *data)
{
	struct bcm_pcib_softc *sc;
	struct bcm_pcib_irqsrc *msi_msg;

	sc = device_get_softc(dev);
	msi_msg = (struct bcm_pcib_irqsrc *) isrc;

	*addr = sc->msi_addr;
	*data = (REG_VALUE_MSI_CONFIG & 0xffff) | msi_msg->irq;
	return (0);
}

static int
bcm_pcib_release_msi(device_t dev, device_t child, int count,
    struct intr_irqsrc **isrc)
{
	struct bcm_pcib_softc *sc;
	struct bcm_pcib_irqsrc *msi_isrc;
	int i;

	sc = device_get_softc(dev);
	mtx_lock(&sc->msi_mtx);

	for (i = 0; i < count; i++) {
		msi_isrc = (struct bcm_pcib_irqsrc *) isrc[i];
		msi_isrc->allocated = false;
	}

	mtx_unlock(&sc->msi_mtx);
	return (0);
}

static int
bcm_pcib_msi_attach(device_t dev)
{
	struct bcm_pcib_softc *sc;
	phandle_t node, xref;
	char const *bcm_name;
	int i, rid;

	sc = device_get_softc(dev);
	sc->msi_addr = 0xffffffffc;

	/* Clear any pending interrupts. */
	bcm_pcib_set_reg(sc, REG_MSI_CLR, 0xffffffff);

	rid = 1;
	sc->msi_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->msi_irq_res == NULL) {
		device_printf(dev, "could not allocate MSI irq resource.\n");
		return (ENXIO);
	}

	sc->msi_isrcs = malloc(sizeof(*sc->msi_isrcs) * NUM_MSI, M_DEVBUF,
	    M_WAITOK | M_ZERO);

	int error = bus_setup_intr(dev, sc->msi_irq_res, INTR_TYPE_BIO |
	    INTR_MPSAFE, bcm_pcib_msi_intr, NULL, sc, &sc->msi_intr_cookie);
	if (error) {
		device_printf(dev, "error: failed to setup MSI handler.\n");
		return (ENXIO);
	}

	bcm_name = device_get_nameunit(dev);
	for (i = 0; i < NUM_MSI; i++) {
		sc->msi_isrcs[i].irq = i;
		error = intr_isrc_register(&sc->msi_isrcs[i].isrc, dev, 0,
		    "%s,%u", bcm_name, i);
		if (error) {
			device_printf(dev,
			"error: failed to register interrupt %d.\n", i);
			return (ENXIO);
		}
	}

	node = ofw_bus_get_node(dev);
	xref = OF_xref_from_node(node);
	OF_device_register_xref(xref, dev);

	error = intr_msi_register(dev, xref);
	if (error)
		return (ENXIO);

	mtx_init(&sc->msi_mtx, "bcm_pcib: msi_mtx", NULL, MTX_DEF);

	bcm_pcib_set_reg(sc, REG_MSI_MASK_CLR, 0xffffffff);
	bcm_pcib_set_reg(sc, REG_MSI_ADDR_LOW, (sc->msi_addr & 0xffffffff) | 1);
	bcm_pcib_set_reg(sc, REG_MSI_ADDR_HIGH, (sc->msi_addr >> 32));
	bcm_pcib_set_reg(sc, REG_MSI_CONFIG, REG_VALUE_MSI_CONFIG);

	return (0);
}

static void
bcm_pcib_relocate_bridge_window(device_t dev)
{
	/*
	 * In principle an out-of-bounds bridge window could be automatically
	 * adjusted at resource-activation time to lie within the bus address
	 * space by pcib_grow_window(), but that is not possible because the
	 * out-of-bounds resource allocation fails at allocation time. Instead,
	 * we will just fix up the window on the controller here, before it is
	 * re-discovered by pcib_probe_windows().
	 */

	struct bcm_pcib_softc *sc;
	pci_addr_t base, size, new_base, new_limit;
	uint16_t val;

	sc = device_get_softc(dev);

	val = bcm_pcib_read_config(dev, 0, 0, 0, PCIR_MEMBASE_1, 2);
	base = PCI_PPBMEMBASE(0, val);

	val = bcm_pcib_read_config(dev, 0, 0, 0, PCIR_MEMLIMIT_1, 2);
	size = PCI_PPBMEMLIMIT(0, val) - base;

	new_base = sc->base.base.ranges[0].pci_base;
	val = (uint16_t) (new_base >> 16);
	bcm_pcib_write_config(dev, 0, 0, 0, PCIR_MEMBASE_1, val, 2);

	new_limit = new_base + size;
	val = (uint16_t) (new_limit >> 16);
	bcm_pcib_write_config(dev, 0, 0, 0, PCIR_MEMLIMIT_1, val, 2);
}

static uint32_t
encode_cpu_window_low(pci_addr_t phys_base, bus_size_t size)
{

	return (((phys_base >> 0x10) & 0xfff0) |
	    ((phys_base + size - 1) & 0xfff00000));
}

static uint32_t
encode_cpu_window_start_high(pci_addr_t phys_base)
{

	return ((phys_base >> 0x20) & 0xff);
}

static uint32_t
encode_cpu_window_end_high(pci_addr_t phys_base, bus_size_t size)
{

	return (((phys_base + size - 1) >> 0x20) & 0xff);
}

static int
bcm_pcib_attach(device_t dev)
{
	struct bcm_pcib_softc *sc;
	pci_addr_t phys_base, pci_base;
	bus_size_t size;
	uint32_t hardware_rev, bridge_state, link_state;
	int error, tries;

	sc = device_get_softc(dev);
	sc->dev = dev;

	/*
	 * This tag will be used in preference to the one created in
	 * pci_host_generic.c.
	 */
	error = bus_dma_tag_create(bus_get_dma_tag(dev), /* parent */
	    1, 0,				/* alignment, bounds */
	    DMA_HIGH_LIMIT,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filter, filterarg */
	    DMA_HIGH_LIMIT,			/* maxsize */
	    BUS_SPACE_UNRESTRICTED,		/* nsegments */
	    DMA_HIGH_LIMIT,			/* maxsegsize */
	    0, 					/* flags */
	    NULL, NULL,				/* lockfunc, lockarg */
	    &sc->dmat);
	if (error)
		return (error);

	error = pci_host_generic_setup_fdt(dev);
	if (error)
		return (error);

	error = bcm_pcib_check_ranges(dev);
	if (error)
		return (error);

	mtx_init(&sc->config_mtx, "bcm_pcib: config_mtx", NULL, MTX_DEF);

	bcm_pcib_reset_controller(sc);

	hardware_rev = bcm_pcib_read_reg(sc, REG_CONTROLLER_HW_REV) & 0xffff;
	device_printf(dev, "hardware identifies as revision 0x%x.\n",
	    hardware_rev);

	/*
	 * Set PCI->CPU memory window. This encodes the inbound window showing
	 * the system memory to the controller.
	 */
	bcm_pcib_set_reg(sc, REG_DMA_WINDOW_LOW, REG_VALUE_DMA_WINDOW_LOW);
	bcm_pcib_set_reg(sc, REG_DMA_WINDOW_HIGH, REG_VALUE_DMA_WINDOW_HIGH);
	bcm_pcib_set_reg(sc, REG_DMA_CONFIG, REG_VALUE_DMA_WINDOW_CONFIG);

	bcm_pcib_set_reg(sc, REG_BRIDGE_GISB_WINDOW, 0);
	bcm_pcib_set_reg(sc, REG_DMA_WINDOW_1, 0);

	bcm_pcib_enable_controller(sc);

	/* Wait for controller to start. */
	for(tries = 0; ; ++tries) {
		bridge_state = bcm_pcib_read_reg(sc, REG_BRIDGE_STATE);

		if ((bridge_state & 0x30) == 0x30)
			/* Controller ready. */
			break;

		if (tries > 100) {
			device_printf(dev,
			    "error: controller failed to start.\n");
			return (ENXIO);
		}

		DELAY(1000);
	}

	link_state = bcm_pcib_read_reg(sc, REG_BRIDGE_LINK_STATE) >> 0x10;
	if (!link_state) {
		device_printf(dev, "error: controller started but link is not "
		    "up.\n");
		return (ENXIO);
	}
	if (bootverbose)
		device_printf(dev, "note: reported link speed is %s.\n",
		    bcm_pcib_link_state_string(link_state));

	/*
	 * Set the CPU->PCI memory window. The map in this direction is not 1:1.
	 * Addresses seen by the CPU need to be adjusted to make sense to the
	 * controller as they pass through the window.
	 */
	pci_base  = sc->base.base.ranges[0].pci_base;
	phys_base = sc->base.base.ranges[0].phys_base;
	size      = sc->base.base.ranges[0].size;

	bcm_pcib_set_reg(sc, REG_BUS_WINDOW_LOW, pci_base & 0xffffffff);
	bcm_pcib_set_reg(sc, REG_BUS_WINDOW_HIGH, pci_base >> 32);

	bcm_pcib_set_reg(sc, REG_CPU_WINDOW_LOW,
	    encode_cpu_window_low(phys_base, size));
	bcm_pcib_set_reg(sc, REG_CPU_WINDOW_START_HIGH,
	    encode_cpu_window_start_high(phys_base));
	bcm_pcib_set_reg(sc, REG_CPU_WINDOW_END_HIGH,
	    encode_cpu_window_end_high(phys_base, size));

	/*
	 * The controller starts up declaring itself an endpoint; readvertise it
	 * as a bridge.
	 */
	bcm_pcib_set_reg(sc, PCI_ID_VAL3,
	    PCIC_BRIDGE << CLASS_SHIFT | PCIS_BRIDGE_PCI << SUBCLASS_SHIFT);

	bcm_pcib_set_reg(sc, REG_BRIDGE_SERDES_MODE, 0x2);
	DELAY(100);

	bcm_pcib_relocate_bridge_window(dev);

	/* Configure interrupts. */
	error = bcm_pcib_msi_attach(dev);
	if (error)
		return (error);

	/* Done. */
	device_add_child(dev, "pci", -1);
	return (bus_generic_attach(dev));
}

/*
 * Device method table.
 */
static device_method_t bcm_pcib_methods[] = {
	/* Bus interface. */
	DEVMETHOD(bus_get_dma_tag,		bcm_pcib_get_dma_tag),

	/* Device interface. */
	DEVMETHOD(device_probe,			bcm_pcib_probe),
	DEVMETHOD(device_attach,		bcm_pcib_attach),

	/* PCIB interface. */
	DEVMETHOD(pcib_read_config,		bcm_pcib_read_config),
	DEVMETHOD(pcib_write_config,		bcm_pcib_write_config),

	/* MSI interface. */
	DEVMETHOD(msi_alloc_msi,		bcm_pcib_alloc_msi),
	DEVMETHOD(msi_release_msi,		bcm_pcib_release_msi),
	DEVMETHOD(msi_map_msi,			bcm_pcib_map_msi),

	DEVMETHOD_END
};

DEFINE_CLASS_1(pcib, bcm_pcib_driver, bcm_pcib_methods,
    sizeof(struct bcm_pcib_softc), generic_pcie_fdt_driver);

static devclass_t bcm_pcib_devclass;
DRIVER_MODULE(bcm_pcib, simplebus, bcm_pcib_driver, bcm_pcib_devclass, 0, 0);

