/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Jari Sihvola <jsihv@gmx.com>
 */

/* JH7110 PCIe controller driver */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/resource.h>

#include <dev/clk/clk.h>
#include <dev/gpio/gpiobusvar.h>
#include <dev/hwreset/hwreset.h>
#include <dev/regulator/regulator.h>
#include <dev/syscon/syscon.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/ofwpci.h>
#include <dev/pci/pci_host_generic.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcib_private.h>

#include "msi_if.h"
#include "ofw_bus_if.h"
#include "pcib_if.h"
#include "pic_if.h"
#include "syscon_if.h"

#define	IRQ_LOCAL_MASK				0x180
#define	IRQ_LOCAL_STATUS			0x184
#define	IRQ_MSI_BASE				0x190
#define	IRQ_MSI_STATUS				0x194

#define	MSI_MASK				0x10000000
#define	INTX_MASK				0xf000000
#define	ERROR_MASK				0x80770000

#define	MSI_COUNT				32
#define	MSI_USED				0x1
#define	MSI_PCIE0_MASK_OFFSET			0xa0;
#define	MSI_PCIE1_MASK_OFFSET			0xf0;

#define	ATR0_AXI4_SLV0_SRCADDR_PARAM		0x800
#define	ATR0_AXI4_SLV0_SRC_ADDR			0x804
#define	ATR0_AXI4_SLV0_TRSL_ADDR_LSB		0x808
#define	ATR0_AXI4_SLV0_TRSL_PARAM		0x810
#define	ATR0_AXI4_SLV0_TRSL_ADDR_UDW		0x80c
#define	ATR_ENTRY_SIZE				0x20
#define	ATR0_PCIE_ATR_SIZE			0x25
#define	ATR0_PCIE_ATR_SIZE_SHIFT		1
#define	ATR0_PCIE_WIN0_SRCADDR_PARAM		0x600
#define	ATR0_PCIE_WIN0_SRC_ADDR			0x604
#define	ATR0_ENABLE				1

#define	PCIE_TXRX_INTERFACE			0x0
#define	PCIE_CONF_INTERFACE			0x1
#define	PCIE_WINCONF				0xfc
#define	PREF_MEM_WIN_64_SUPPORT			(1U << 3)

#define	STG_AXI4_SLVL_AW_MASK			0x7fff
#define	STG_AXI4_SLVL_AR_MASK			0x7fff00
#define	STG_PCIE0_BASE				0x48
#define	STG_PCIE1_BASE				0x1f8
#define	STG_RP_NEP_OFFSET			0xe8
#define	STG_K_RP_NEP				(1U << 8)
#define	STG_CKREF_MASK				0xC0000
#define	STG_CKREF_VAL				0x80000
#define	STG_CLKREQ				(1U << 22)
#define	STG_AR_OFFSET				0x78
#define	STG_AW_OFFSET				0x7c
#define	STG_AXI4_SLVL_ARFUNC_SHIFT		0x8
#define	STG_LNKSTA_OFFSET			0x170
#define	STG_LINK_UP				(1U << 5)

#define	PHY_FUNC_SHIFT				9
#define	PHY_FUNC_DIS				(1U << 15)
#define	PCI_MISC_REG				0xb4
#define	PCI_GENERAL_SETUP_REG			0x80
#define	PCI_CONF_SPACE_REGS			0x1000
#define	ROOTPORT_ENABLE				0x1
#define	PMSG_RX_SUPPORT_REG			0x3f0
#define	PMSG_LTR_SUPPORT			(1U << 2)
#define	PCI_CLASS_BRIDGE_PCI			0x0604
#define	PCI_IDS_CLASS_CODE_SHIFT		16
#define	PCIE_PCI_IDS_REG			0x9c
#define	REV_ID_MASK				0xff

#define	PLDA_AXI_POST_ERR			(1U << 16)
#define	PLDA_AXI_FETCH_ERR			(1U << 17)
#define	PLDA_AXI_DISCARD_ERR			(1U << 18)
#define	PLDA_PCIE_POST_ERR			(1U << 20)
#define	PLDA_PCIE_FETCH_ERR			(1U << 21)
#define	PLDA_PCIE_DISCARD_ERR			(1U << 22)
#define	PLDA_SYS_ERR				(1U << 31)

/* Compatible devices. */
static struct ofw_compat_data compat_data[] = {
	{"starfive,jh7110-pcie", 1},
	{NULL,			 0},
};

struct jh7110_pcie_irqsrc {
	struct intr_irqsrc	isrc;
	u_int			irq;
	u_int			is_used;
};

struct jh7110_pcie_softc {
	struct ofw_pci_softc	ofw_pci;
	device_t		dev;
	phandle_t		node;

	struct resource		*reg_mem_res;
	struct resource		*cfg_mem_res;
	struct resource		*irq_res;
	struct jh7110_pcie_irqsrc *isrcs;
	void			*irq_cookie;
	struct syscon		*stg_syscon;
	uint64_t		stg_baddr;

	struct ofw_pci_range	range_mem32;
	struct ofw_pci_range	range_mem64;

	struct mtx		msi_mtx;
	uint64_t		msi_mask_offset;

	gpio_pin_t		perst_pin;

	clk_t			clk_noc;
	clk_t			clk_tl;
	clk_t			clk_axi;
	clk_t			clk_apb;

	hwreset_t		rst_mst0;
	hwreset_t		rst_slv0;
	hwreset_t		rst_slv;
	hwreset_t		rst_brg;
	hwreset_t		rst_core;
	hwreset_t		rst_apb;
};

#define	LOW32(val)		(uint32_t)(val)
#define	HI32(val)		(uint32_t)(val >> 32)

#define	RD4(sc, reg)		bus_read_4((sc)->reg_mem_res, (reg))
#define	WR4(sc, reg, val)	bus_write_4((sc)->reg_mem_res, (reg), (val))

static uint32_t
jh7110_pcie_read_config(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, int bytes)
{
	struct jh7110_pcie_softc *sc;
	uint32_t data, offset;

	sc = device_get_softc(dev);
	offset = PCIE_ADDR_OFFSET(bus, slot, func, reg);

	/* Certain config registers are not supposed to be accessed from here */
	if (bus == 0 && (offset == PCIR_BAR(0) || offset == PCIR_BAR(1)))
		return (~0U);

	switch (bytes) {
	case 1:
		data = bus_read_1(sc->cfg_mem_res, offset);
		break;
	case 2:
		data = le16toh(bus_read_2(sc->cfg_mem_res, offset));
		break;
	case 4:
		data = le32toh(bus_read_4(sc->cfg_mem_res, offset));
		break;
	default:
		return (~0U);
	}

	return (data);
}

static void
jh7110_pcie_write_config(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, uint32_t val, int bytes)
{
	struct jh7110_pcie_softc *sc;
	uint32_t offset;

	sc = device_get_softc(dev);
	offset = PCIE_ADDR_OFFSET(bus, slot, func, reg);

	/* Certain config registers are not supposed to be accessed from here */
	if (bus == 0 && (offset == PCIR_BAR(0) || offset == PCIR_BAR(1)))
		return;

	switch (bytes) {
	case 1:
		bus_write_1(sc->cfg_mem_res, offset, val);
		break;
	case 2:
		bus_write_2(sc->cfg_mem_res, offset, htole16(val));
		break;
	case 4:
		bus_write_4(sc->cfg_mem_res, offset, htole32(val));
		break;
	default:
		return;
	}
}

static int
jh7110_pcie_intr(void *arg)
{
	struct jh7110_pcie_softc *sc;
	struct trapframe *tf;
	struct jh7110_pcie_irqsrc *irq;
	uint32_t reg, irqbits;
	int err, i;

	sc = (struct jh7110_pcie_softc *)arg;
	tf = curthread->td_intr_frame;

	reg = RD4(sc, IRQ_LOCAL_STATUS);
	if (reg == 0)
		return (ENXIO);

	if ((reg & MSI_MASK) != 0) {
		WR4(sc, IRQ_LOCAL_STATUS, MSI_MASK);

		irqbits = RD4(sc, IRQ_MSI_STATUS);
		for (i = 0; irqbits != 0; i++) {
			if ((irqbits & (1U << i)) != 0) {
				irq = &sc->isrcs[i];
				err = intr_isrc_dispatch(&irq->isrc, tf);
				if (err != 0)
					device_printf(sc->dev,
					    "MSI 0x%x gives error %d\n",
						i, err);
				irqbits &= ~(1U << i);
			}
		}
	}
	if ((reg & INTX_MASK) != 0) {
		irqbits = (reg & INTX_MASK);
		WR4(sc, IRQ_LOCAL_STATUS, irqbits);
	}
	if ((reg & ERROR_MASK) != 0) {
		irqbits = (reg & ERROR_MASK);
		if ((reg & PLDA_AXI_POST_ERR) != 0)
			device_printf(sc->dev, "axi post error\n");
		if ((reg & PLDA_AXI_FETCH_ERR) != 0)
			device_printf(sc->dev, "axi fetch error\n");
		if ((reg & PLDA_AXI_DISCARD_ERR) != 0)
			device_printf(sc->dev, "axi discard error\n");
		if ((reg & PLDA_PCIE_POST_ERR) != 0)
			device_printf(sc->dev, "pcie post error\n");
		if ((reg & PLDA_PCIE_FETCH_ERR) != 0)
			device_printf(sc->dev, "pcie fetch error\n");
		if ((reg & PLDA_PCIE_DISCARD_ERR) != 0)
			device_printf(sc->dev, "pcie discard error\n");
		if ((reg & PLDA_SYS_ERR) != 0)
			device_printf(sc->dev, "pcie sys error\n");
		WR4(sc, IRQ_LOCAL_STATUS, irqbits);
	}

	return (FILTER_HANDLED);
}

static int
jh7110_pcie_route_interrupt(device_t bus, device_t dev, int pin)
{
	struct jh7110_pcie_softc *sc;
	u_int irq;

	sc = device_get_softc(bus);
	irq = intr_map_clone_irq(rman_get_start(sc->irq_res));
	device_printf(bus, "route pin %d for device %d.%d to %u\n",
	    pin, pci_get_slot(dev), pci_get_function(dev), irq);

	return (irq);
}

static int
jh7110_pcie_maxslots(device_t dev)
{
	return (PCI_SLOTMAX);
}

static int
jh7110_pcie_msi_alloc_msi(device_t dev, device_t child, int count, int maxcount,
    device_t *pic, struct intr_irqsrc **srcs)
{
	struct jh7110_pcie_softc *sc;
	int i, beg;

	sc = device_get_softc(dev);
	mtx_lock(&sc->msi_mtx);

	/* Search for a requested contiguous region */
	for (beg = 0; beg + count < MSI_COUNT; ) {
		for (i = beg; i < beg + count; i++) {
			if (sc->isrcs[i].is_used == MSI_USED)
				goto next;
		}
		goto found;
next:
		beg = i + 1;
	}

	/* Requested area not found */
	mtx_unlock(&sc->msi_mtx);
	device_printf(dev, "warning: failed to allocate %d MSIs.\n", count);

	return (ENXIO);

found:
	/* Mark and allocate messages */
	for (i = 0; i < count; ++i) {
		sc->isrcs[i + beg].is_used = MSI_USED;
		srcs[i] = &(sc->isrcs[i + beg].isrc);
	}

	mtx_unlock(&sc->msi_mtx);
	*pic = device_get_parent(dev);

	return (0);
}

static int
jh7110_pcie_alloc_msi(device_t pci, device_t child, int count,
    int maxcount, int *irqs)
{
	phandle_t msi_parent;
	int err;

	msi_parent = OF_xref_from_node(ofw_bus_get_node(pci));
	err = intr_alloc_msi(pci, child, msi_parent, count, maxcount, irqs);

	return (err);
}

static int
jh7110_pcie_release_msi(device_t pci, device_t child, int count, int *irqs)
{
	phandle_t msi_parent;
	int err;

	msi_parent = OF_xref_from_node(ofw_bus_get_node(pci));
	err = intr_release_msi(pci, child, msi_parent, count, irqs);

	return (err);
}

static int
jh7110_pcie_msi_map_msi(device_t dev, device_t child, struct intr_irqsrc *isrc,
    uint64_t *addr, uint32_t *data)
{
	struct jh7110_pcie_irqsrc *jhirq = (struct jh7110_pcie_irqsrc *)isrc;

	*addr = IRQ_MSI_BASE;
	*data = jhirq->irq;

	return (0);
}


static int
jh7110_pcie_map_msi(device_t pci, device_t child, int irq, uint64_t *addr,
    uint32_t *data)
{
	phandle_t msi_parent;
	int err;

	msi_parent = OF_xref_from_node(ofw_bus_get_node(pci));

	err = intr_map_msi(pci, child, msi_parent, irq, addr, data);
	if (err != 0) {
		device_printf(pci, "intr_map_msi() failed\n");
		return (err);
	}

	return (err);
}

static int
jh7110_pcie_alloc_msix(device_t pci, device_t child, int *irq)
{
	return (jh7110_pcie_alloc_msi(pci, child, 1, 32, irq));
}

static int
jh7110_pcie_release_msix(device_t pci, device_t child, int irq)
{
	phandle_t msi_parent;
	int err;

	msi_parent = OF_xref_from_node(ofw_bus_get_node(pci));
	err = intr_release_msix(pci, child, msi_parent, irq);

	return (err);
}

static int
jh7110_pcie_msi_alloc_msix(device_t dev, device_t child, device_t *pic,
    struct intr_irqsrc **isrcp)
{
	return (jh7110_pcie_msi_alloc_msi(dev, child, 1, 32, pic, isrcp));
}

static int
jh7110_pcie_msi_release_msi(device_t dev, device_t child, int count,
    struct intr_irqsrc **isrc)
{
	struct jh7110_pcie_softc *sc;
	struct jh7110_pcie_irqsrc *irq;
	int i;

	sc = device_get_softc(dev);
	mtx_lock(&sc->msi_mtx);

	for (i = 0; i < count; i++) {
		irq = (struct jh7110_pcie_irqsrc *)isrc[i];

		KASSERT((irq->is_used & MSI_USED) == MSI_USED,
		    ("%s: Trying to release an unused MSI(-X) interrupt",
		    __func__));

		irq->is_used = 0;
	}

	mtx_unlock(&sc->msi_mtx);
	return (0);
}

static int
jh7110_pcie_msi_release_msix(device_t dev, device_t child,
    struct intr_irqsrc *isrc)
{
	return (jh7110_pcie_msi_release_msi(dev, child, 1, &isrc));
}

static void
jh7110_pcie_msi_mask(device_t dev, struct intr_irqsrc *isrc, bool mask)
{
	struct jh7110_pcie_softc *sc;
	struct jh7110_pcie_irqsrc *jhirq = (struct jh7110_pcie_irqsrc *)isrc;
	uint32_t reg, irq;

	sc = device_get_softc(dev);
	irq = jhirq->irq;

	reg = bus_read_4(sc->cfg_mem_res, sc->msi_mask_offset);
	if (mask != 0)
		reg &= ~(1U << irq);
	else
		reg |= (1U << irq);
	bus_write_4(sc->cfg_mem_res, sc->msi_mask_offset, reg);
}

static void
jh7110_pcie_msi_disable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	jh7110_pcie_msi_mask(dev, isrc, true);
}

static void
jh7110_pcie_msi_enable_intr(device_t dev, struct intr_irqsrc *isrc)
{
	jh7110_pcie_msi_mask(dev, isrc, false);
}

static void
jh7110_pcie_msi_post_filter(device_t dev, struct intr_irqsrc *isrc)
{
}

static void
jh7110_pcie_msi_post_ithread(device_t dev, struct intr_irqsrc *isrc)
{
}

static void
jh7110_pcie_msi_pre_ithread(device_t dev, struct intr_irqsrc *isrc)
{
	struct jh7110_pcie_softc *sc;
	struct jh7110_pcie_irqsrc *jhirq = (struct jh7110_pcie_irqsrc *)isrc;
	uint32_t irq;

	sc = device_get_softc(dev);
	irq = jhirq->irq;

	/* MSI bottom ack */
	WR4(sc, IRQ_MSI_STATUS, (1U << irq));
}

static int
jh7110_pcie_decode_ranges(struct jh7110_pcie_softc *sc,
    struct ofw_pci_range *ranges, int nranges)
{
	int i;

	for (i = 0; i < nranges; i++) {
		if (((ranges[i].pci_hi & OFW_PCI_PHYS_HI_SPACEMASK) ==
		    OFW_PCI_PHYS_HI_SPACE_MEM64)) {
			if (sc->range_mem64.size != 0) {
				device_printf(sc->dev,
				      "Duplicate range mem64 found in DT\n");
				return (ENXIO);
			}
			sc->range_mem64 = ranges[i];
		} else if (((ranges[i].pci_hi & OFW_PCI_PHYS_HI_SPACEMASK) ==
		    OFW_PCI_PHYS_HI_SPACE_MEM32)) {
			if (sc->range_mem32.size != 0) {
				device_printf(sc->dev,
					"Duplicated range mem32 found in DT\n");
				return (ENXIO);
			}
			sc->range_mem32 = ranges[i];
		}
	}
	return (0);
}

static int
jh7110_pcie_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Starfive JH7110 PCIe controller");

	return (BUS_PROBE_DEFAULT);
}

static void
jh7110_pcie_set_atr(device_t dev, uint64_t axi_begin, uint64_t pci_begin,
    uint64_t win_size, uint32_t win_idx)
{
	struct jh7110_pcie_softc *sc;
	uint32_t val, taddr_size;

	sc = device_get_softc(dev);

	if (win_idx == 0)
		val = PCIE_CONF_INTERFACE;
	else
		val = PCIE_TXRX_INTERFACE;

	WR4(sc, ATR0_AXI4_SLV0_TRSL_PARAM + win_idx * ATR_ENTRY_SIZE, val);

	taddr_size = ilog2(win_size) - 1;
	val = LOW32(axi_begin) | taddr_size << ATR0_PCIE_ATR_SIZE_SHIFT |
	    ATR0_ENABLE;

	WR4(sc, ATR0_AXI4_SLV0_SRCADDR_PARAM + win_idx * ATR_ENTRY_SIZE, val);

	val = HI32(axi_begin);
	WR4(sc, ATR0_AXI4_SLV0_SRC_ADDR + win_idx * ATR_ENTRY_SIZE, val);

	val = LOW32(pci_begin);
	WR4(sc, ATR0_AXI4_SLV0_TRSL_ADDR_LSB + win_idx * ATR_ENTRY_SIZE, val);

	val = HI32(pci_begin);
	WR4(sc, ATR0_AXI4_SLV0_TRSL_ADDR_UDW + win_idx * ATR_ENTRY_SIZE, val);

	val = RD4(sc, ATR0_PCIE_WIN0_SRCADDR_PARAM);
	val |= (ATR0_PCIE_ATR_SIZE << ATR0_PCIE_ATR_SIZE_SHIFT);

	WR4(sc, ATR0_PCIE_WIN0_SRCADDR_PARAM, val);
	WR4(sc, ATR0_PCIE_WIN0_SRC_ADDR, 0);
}

static int
jh7110_pcie_parse_fdt_resources(struct jh7110_pcie_softc *sc)
{
	uint32_t val;
	int err;

	/* Getting clocks */
	if (clk_get_by_ofw_name(sc->dev, 0, "noc", &sc->clk_noc) != 0) {
		device_printf(sc->dev, "could not get noc clock\n");
		sc->clk_noc = NULL;
		return (ENXIO);
	}
	if (clk_get_by_ofw_name(sc->dev, 0, "tl", &sc->clk_tl) != 0) {
		device_printf(sc->dev, "could not get tl clock\n");
		sc->clk_tl = NULL;
		return (ENXIO);
	}
	if (clk_get_by_ofw_name(sc->dev, 0, "axi_mst0", &sc->clk_axi) != 0) {
		device_printf(sc->dev, "could not get axi_mst0 clock\n");
		sc->clk_axi = NULL;
		return (ENXIO);
	}
	if (clk_get_by_ofw_name(sc->dev, 0, "apb", &sc->clk_apb) != 0) {
		device_printf(sc->dev, "could not get apb clock\n");
		sc->clk_apb = NULL;
		return (ENXIO);
	}

	/* Getting resets */
	err = hwreset_get_by_ofw_name(sc->dev, 0, "mst0", &sc->rst_mst0);
	if (err != 0) {
		device_printf(sc->dev, "cannot get 'rst_mst0' reset\n");
		return (ENXIO);
	}
	err = hwreset_get_by_ofw_name(sc->dev, 0, "slv0", &sc->rst_slv0);
	if (err != 0) {
		device_printf(sc->dev, "cannot get 'rst_slv0' reset\n");
		return (ENXIO);
	}
	err = hwreset_get_by_ofw_name(sc->dev, 0, "slv", &sc->rst_slv);
	if (err != 0) {
		device_printf(sc->dev, "cannot get 'rst_slv' reset\n");
		return (ENXIO);
	}
	err = hwreset_get_by_ofw_name(sc->dev, 0, "brg", &sc->rst_brg);
	if (err != 0) {
		device_printf(sc->dev, "cannot get 'rst_brg' reset\n");
		return (ENXIO);
	}
	err = hwreset_get_by_ofw_name(sc->dev, 0, "core", &sc->rst_core);
	if (err != 0) {
		device_printf(sc->dev, "cannot get 'rst_core' reset\n");
		return (ENXIO);
	}
	err = hwreset_get_by_ofw_name(sc->dev, 0, "apb", &sc->rst_apb);
	if (err != 0) {
		device_printf(sc->dev, "cannot get 'rst_apb' reset\n");
		return (ENXIO);
	}

	/* Getting PCI endpoint reset pin */
	err = gpio_pin_get_by_ofw_property(sc->dev, sc->node, "perst-gpios",
	    &sc->perst_pin);
	if (err != 0) {
		device_printf(sc->dev, "Cannot get perst-gpios\n");
		return (ENXIO);
	}

	/* Getting syscon property */
	if (syscon_get_by_ofw_property(sc->dev, sc->node, "starfive,stg-syscon",
	    &sc->stg_syscon) != 0) {
		device_printf(sc->dev, "Cannot get starfive,stg-syscon\n");
		return (ENXIO);
	}

	/* Assigning syscon base address and MSI mask offset */
	err = OF_getencprop(sc->node, "linux,pci-domain", &val, sizeof(val));
	if (err == -1) {
		device_printf(sc->dev,
		    "Couldn't get pci-domain property, error: %d\n", err);
		return (ENXIO);
	}

	if (val == 0) {
		sc->stg_baddr = STG_PCIE0_BASE;
		sc->msi_mask_offset = MSI_PCIE0_MASK_OFFSET;
	} else if (val == 1) {
		sc->stg_baddr = STG_PCIE1_BASE;
		sc->msi_mask_offset = MSI_PCIE1_MASK_OFFSET;
	} else {
		device_printf(sc->dev, "Error: an invalid pci-domain value\n");
		return (ENXIO);
	}

	return (0);
}

static void
jh7110_pcie_release_resources(device_t dev)
{
	struct jh7110_pcie_softc *sc;

	sc = device_get_softc(dev);

	if (sc->irq_res != NULL)
		bus_teardown_intr(dev, sc->irq_res, sc->irq_cookie);
	if (sc->irq_res != NULL)
		bus_free_resource(dev, SYS_RES_IRQ, sc->irq_res);
	if (sc->reg_mem_res != NULL)
		bus_free_resource(dev, SYS_RES_MEMORY, sc->reg_mem_res);
	if (sc->cfg_mem_res != NULL)
		bus_free_resource(dev, SYS_RES_MEMORY, sc->cfg_mem_res);

	if (sc->clk_noc != NULL)
		clk_release(sc->clk_noc);
	if (sc->clk_tl != NULL)
		clk_release(sc->clk_tl);
	if (sc->clk_axi != NULL)
		clk_release(sc->clk_axi);
	if (sc->clk_apb != NULL)
		clk_release(sc->clk_apb);

	gpio_pin_release(sc->perst_pin);

	hwreset_release(sc->rst_mst0);
	hwreset_release(sc->rst_slv0);
	hwreset_release(sc->rst_slv);
	hwreset_release(sc->rst_brg);
	hwreset_release(sc->rst_core);
	hwreset_release(sc->rst_apb);

	mtx_destroy(&sc->msi_mtx);
}

static int
jh7110_pcie_detach(device_t dev)
{
	ofw_pcib_fini(dev);
	jh7110_pcie_release_resources(dev);

	return (0);
}

static int
jh7110_pcie_attach(device_t dev)
{
	struct jh7110_pcie_softc *sc;
	phandle_t xref;
	uint32_t val;
	int i, err, rid, irq, win_idx = 0;
	char name[INTR_ISRC_NAMELEN];

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->node = ofw_bus_get_node(dev);

	sc->irq_res = NULL;
	sc->reg_mem_res = NULL;
	sc->cfg_mem_res = NULL;
	sc->clk_noc = NULL;
	sc->clk_tl = NULL;
	sc->clk_axi = NULL;
	sc->clk_apb = NULL;

	mtx_init(&sc->msi_mtx, "jh7110_pcie, msi_mtx", NULL, MTX_DEF);

	/* Allocating memory */
	err = ofw_bus_find_string_index(sc->node, "reg-names", "apb", &rid);
	if (err != 0) {
		device_printf(dev, "Cannot get apb memory\n");
		goto out;
	}

	sc->reg_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->reg_mem_res == NULL) {
		device_printf(dev, "Cannot allocate apb memory\n");
		err = ENXIO;
		goto out;
	}

	err = ofw_bus_find_string_index(sc->node, "reg-names", "cfg", &rid);
	if (err != 0) {
		device_printf(dev, "Cannot get cfg memory\n");
		err = ENXIO;
		goto out;
	}

	sc->cfg_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->cfg_mem_res == NULL) {
		device_printf(dev, "Cannot allocate cfg memory\n");
		err = ENXIO;
		goto out;
	}

	/* Getting device tree properties */
	if (jh7110_pcie_parse_fdt_resources(sc) != 0)
		goto out;

	/* Clearing interrupts, enabling MSI */
	WR4(sc, IRQ_LOCAL_STATUS, 0xffffffff);
	WR4(sc, IRQ_LOCAL_MASK, INTX_MASK | ERROR_MASK | MSI_MASK);

	/* Setting host up */
	SYSCON_MODIFY_4(sc->stg_syscon, sc->stg_baddr + STG_RP_NEP_OFFSET,
	    STG_K_RP_NEP, STG_K_RP_NEP);
	SYSCON_MODIFY_4(sc->stg_syscon, sc->stg_baddr + STG_AW_OFFSET,
	    STG_CKREF_MASK, STG_CKREF_VAL);
	SYSCON_MODIFY_4(sc->stg_syscon, sc->stg_baddr + STG_AW_OFFSET,
	    STG_CLKREQ, STG_CLKREQ);

	/* Enabling clocks */
	if (clk_enable(sc->clk_noc) != 0) {
		device_printf(dev, "could not enable noc clock\n");
		goto out;
	}
	if (clk_enable(sc->clk_tl) != 0) {
		device_printf(dev, "could not enable tl clock\n");
		goto out;
	}
	if (clk_enable(sc->clk_axi) != 0) {
		device_printf(dev, "could not enable axi_mst0 clock\n");
		goto out;
	}
	if (clk_enable(sc->clk_apb) != 0) {
		device_printf(dev, "could not enable apb clock\n");
		goto out;
	}

	/* Deasserting resets */
	err = hwreset_deassert(sc->rst_mst0);
	if (err != 0) {
		device_printf(sc->dev, "cannot deassert 'mst0' reset\n");
		goto out;
	}
	err = hwreset_deassert(sc->rst_slv0);
	if (err != 0) {
		device_printf(sc->dev, "cannot deassert 'slv0' reset\n");
		goto out;
	}
	err = hwreset_deassert(sc->rst_slv);
	if (err != 0) {
		device_printf(sc->dev, "cannot deassert 'slv' reset\n");
		goto out;
	}
	err = hwreset_deassert(sc->rst_brg);
	if (err != 0) {
		device_printf(sc->dev, "cannot deassert 'brg' reset\n");
		goto out;
	}
	err = hwreset_deassert(sc->rst_core);
	if (err != 0) {
		device_printf(sc->dev, "cannot deassert 'core' reset\n");
		goto out;
	}
	err = hwreset_deassert(sc->rst_apb);
	if (err != 0) {
		device_printf(sc->dev, "cannot deassert 'apb' reset\n");
		goto out;
	}

	err = gpio_pin_set_active(sc->perst_pin, true);
	if (err != 0) {
		device_printf(dev, "Cannot activate gpio pin, error %d\n", err);
		goto out;
	}

	/* Switching off PHY functions 1-3 */
	for (i = 1; i != 4; i++) {
		SYSCON_MODIFY_4(sc->stg_syscon, sc->stg_baddr + STG_AR_OFFSET,
		    STG_AXI4_SLVL_AR_MASK, (i << PHY_FUNC_SHIFT)
			<< STG_AXI4_SLVL_ARFUNC_SHIFT);
		SYSCON_MODIFY_4(sc->stg_syscon, sc->stg_baddr + STG_AW_OFFSET,
		    STG_AXI4_SLVL_AW_MASK, i << PHY_FUNC_SHIFT);

		val = RD4(sc, PCI_MISC_REG);
		WR4(sc, PCI_MISC_REG, val | PHY_FUNC_DIS);
	}

	SYSCON_MODIFY_4(sc->stg_syscon, sc->stg_baddr + STG_AR_OFFSET,
	    STG_AXI4_SLVL_AR_MASK, 0);
	SYSCON_MODIFY_4(sc->stg_syscon, sc->stg_baddr + STG_AW_OFFSET,
	    STG_AXI4_SLVL_AW_MASK, 0);

	/* Enabling root port */
	val = RD4(sc, PCI_GENERAL_SETUP_REG);
	WR4(sc, PCI_GENERAL_SETUP_REG, val | ROOTPORT_ENABLE);

	/* Zeroing RC BAR */
	WR4(sc, PCI_CONF_SPACE_REGS + PCIR_BAR(0), 0);
	WR4(sc, PCI_CONF_SPACE_REGS + PCIR_BAR(1), 0);

	/* Setting standard class */
	val = RD4(sc, PCIE_PCI_IDS_REG);
	val &= REV_ID_MASK;
	val |= (PCI_CLASS_BRIDGE_PCI << PCI_IDS_CLASS_CODE_SHIFT);
	WR4(sc, PCIE_PCI_IDS_REG, val);

	/* Disabling latency tolerance reporting */
	val = RD4(sc, PMSG_RX_SUPPORT_REG);
	WR4(sc, PMSG_RX_SUPPORT_REG, val & ~PMSG_LTR_SUPPORT);

	/* Setting support for 64-bit pref window */
	val = RD4(sc, PCIE_WINCONF);
	WR4(sc, PCIE_WINCONF, val | PREF_MEM_WIN_64_SUPPORT);

	/* Holding PCI endpoint reset (perst) for 100ms, setting the pin */
	DELAY(100);
	err = gpio_pin_set_active(sc->perst_pin, false);
	if (err != 0) {
		device_printf(dev, "Cannot deassert perst pin: %d\n", err);
		goto out;
	}

	/* Setting up an address translation window */
	jh7110_pcie_set_atr(dev, rman_get_start(sc->cfg_mem_res), 0,
	    rman_get_size(sc->cfg_mem_res), win_idx);

	err = ofw_pcib_init(dev);
	if (err != 0) {
		device_printf(dev, "ofw_pcib_init() fails\n");
		goto out;
	}

	jh7110_pcie_decode_ranges(sc, sc->ofw_pci.sc_range,
	    sc->ofw_pci.sc_nrange);

	jh7110_pcie_set_atr(dev, sc->range_mem32.pci, sc->range_mem32.pci,
	    sc->range_mem32.size, ++win_idx);
	jh7110_pcie_set_atr(dev, sc->range_mem64.pci, sc->range_mem64.pci,
	    sc->range_mem64.size, ++win_idx);

	/* Checking data link status */
	for (i = 0; i != 1000; i++) {
		val = SYSCON_READ_4(sc->stg_syscon,
		    sc->stg_baddr + STG_LNKSTA_OFFSET);
		if ((val & STG_LINK_UP) != 0) {
			device_printf(dev, "Link up\n");
			break;
		}
		DELAY(100);
	}
	if ((val & STG_LINK_UP) == 0) {
		device_printf(dev, "Cannot establish data link\n");
		goto out;
	}

	/* Setup interrupts */
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "Cannot allocate IRQ resource\n");
		err = ENXIO;
		goto out_full;
	}

	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	    jh7110_pcie_intr, NULL, sc, &sc->irq_cookie);
	if (err != 0) {
		device_printf(dev, "Cannot setup interrupt handler\n");
		err = ENXIO;
		goto out_full;
	}

	sc->isrcs = malloc(sizeof(*sc->isrcs) * MSI_COUNT, M_DEVBUF,
	    M_WAITOK | M_ZERO);

	snprintf(name, INTR_ISRC_NAMELEN, "%s, MSI",
	    device_get_nameunit(sc->dev));

	for (irq = 0; irq < MSI_COUNT; irq++) {
		sc->isrcs[irq].irq = irq;
		err = intr_isrc_register(&sc->isrcs[irq].isrc, sc->dev, 0,
		    "%s,%u", name, irq);
		if (err != 0) {
			device_printf(dev,
			    "intr_isrs_register failed for MSI irq %d\n", irq);
			goto out_full;
		}
	}

	xref = OF_xref_from_node(sc->node);
	OF_device_register_xref(xref, dev);

	err = intr_msi_register(dev, xref);
	if (err != 0) {
		device_printf(dev, "intr_msi_register() fails\n");
		goto out_full;
	}

	device_add_child(dev, "pci", DEVICE_UNIT_ANY);
	bus_attach_children(dev);

	return (0);

out_full:
	ofw_pcib_fini(dev);
out:
	jh7110_pcie_release_resources(dev);

	return (err);
}

static device_method_t jh7110_pcie_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		jh7110_pcie_probe),
	DEVMETHOD(device_attach,	jh7110_pcie_attach),
	DEVMETHOD(device_detach,	jh7110_pcie_detach),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	jh7110_pcie_maxslots),
	DEVMETHOD(pcib_read_config,	jh7110_pcie_read_config),
	DEVMETHOD(pcib_write_config,	jh7110_pcie_write_config),
	DEVMETHOD(pcib_route_interrupt,	jh7110_pcie_route_interrupt),
	DEVMETHOD(pcib_map_msi,		jh7110_pcie_map_msi),
	DEVMETHOD(pcib_alloc_msi,	jh7110_pcie_alloc_msi),
	DEVMETHOD(pcib_release_msi,	jh7110_pcie_release_msi),
	DEVMETHOD(pcib_alloc_msix,	jh7110_pcie_alloc_msix),
	DEVMETHOD(pcib_release_msix,	jh7110_pcie_release_msix),
	DEVMETHOD(pcib_request_feature,	pcib_request_feature_allow),

	/* MSI/MSI-X */
	DEVMETHOD(msi_alloc_msi,	jh7110_pcie_msi_alloc_msi),
	DEVMETHOD(msi_alloc_msix,	jh7110_pcie_msi_alloc_msix),
	DEVMETHOD(msi_release_msi,	jh7110_pcie_msi_release_msi),
	DEVMETHOD(msi_release_msix,	jh7110_pcie_msi_release_msix),
	DEVMETHOD(msi_map_msi,		jh7110_pcie_msi_map_msi),

	/* Interrupt controller interface */
	DEVMETHOD(pic_enable_intr,	jh7110_pcie_msi_enable_intr),
	DEVMETHOD(pic_disable_intr,	jh7110_pcie_msi_disable_intr),
	DEVMETHOD(pic_post_filter,	jh7110_pcie_msi_post_filter),
	DEVMETHOD(pic_post_ithread,	jh7110_pcie_msi_post_ithread),
	DEVMETHOD(pic_pre_ithread,	jh7110_pcie_msi_pre_ithread),

	/* OFW bus interface */
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	DEVMETHOD_END
};

DEFINE_CLASS_1(pcib, jh7110_pcie_driver, jh7110_pcie_methods,
    sizeof(struct jh7110_pcie_softc), ofw_pcib_driver);
DRIVER_MODULE(jh7110_pcie, simplebus, jh7110_pcie_driver, NULL, NULL);
