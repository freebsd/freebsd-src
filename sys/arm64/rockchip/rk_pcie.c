/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Michal Meloun <mmel@FreeBSD.org>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/* Rockchip PCIe controller driver */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/resource.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>
#include <dev/extres/phy/phy.h>
#include <dev/extres/regulator/regulator.h>
#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/ofwpci.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>

#include <dev/ofw/ofw_bus.h>

#include "pcib_if.h"

#define ATU_CFG_BUS(x)		(((x) & 0x0ff) << 20)
#define ATU_CFG_SLOT(x)		(((x) & 0x01f) << 15)
#define ATU_CFG_FUNC(x)		(((x) & 0x007) << 12)
#define ATU_CFG_REG(x)		(((x) & 0xfff) << 0)

#define ATU_TYPE_MEM		0x2
#define ATU_TYPE_IO		0x6
#define ATU_TYPE_CFG0		0xA
#define ATU_TYPE_CFG1		0xB
#define ATY_TYPE_NOR_MSG	0xC

#define ATU_OB_REGIONS		33
#define	ATU_OB_REGION_SHIFT	20
#define ATU_OB_REGION_SIZE	(1 << ATU_OB_REGION_SHIFT)
#define ATU_OB_REGION_0_SIZE	(( ATU_OB_REGIONS - 1) * ATU_OB_REGION_SIZE)

#define ATU_IB_REGIONS		3

#define	PCIE_CLIENT_BASIC_STRAP_CONF		0x000000
#define	 STRAP_CONF_GEN_2				(1 << 7)
#define	 STRAP_CONF_MODE_RC				(1 << 6)
#define	 STRAP_CONF_LANES(n)				((((n) / 2) & 0x3) << 4)
#define	 STRAP_CONF_ARI_EN				(1 << 3)
#define	 STRAP_CONF_SR_IOV_EN				(1 << 2)
#define	 STRAP_CONF_LINK_TRAIN_EN			(1 << 1)
#define	 STRAP_CONF_CONF_EN				(1 << 0)
#define	PCIE_CLIENT_HOT_RESET_CTRL		0x000018
#define	 HOT_RESET_CTRL_LINK_DOWN_RESET			(1 << 1)
#define	 HOT_RESET_CTRL_HOT_RESET_IN			(1 << 0)
#define	PCIE_CLIENT_BASIC_STATUS0		0x000044
#define	PCIE_CLIENT_BASIC_STATUS1		0x000048
#define	 STATUS1_LINK_ST_GET(x)				(((x) >> 20) & 0x3)
#define	  STATUS1_LINK_ST_UP				3
#define	PCIE_CLIENT_INT_MASK			0x00004C
#define	PCIE_CLIENT_INT_STATUS			0x000050
#define	 PCIE_CLIENT_INT_LEGACY_DONE			(1 << 15)
#define	 PCIE_CLIENT_INT_MSG				(1 << 14)
#define	 PCIE_CLIENT_INT_HOT_RST			(1 << 13)
#define	 PCIE_CLIENT_INT_DPA				(1 << 12)
#define	 PCIE_CLIENT_INT_FATAL_ERR			(1 << 11)
#define	 PCIE_CLIENT_INT_NFATAL_ERR			(1 << 10)
#define	 PCIE_CLIENT_INT_CORR_ERR			(1 << 9)
#define	 PCIE_CLIENT_INT_INTD				(1 << 8)
#define	 PCIE_CLIENT_INT_INTC				(1 << 7)
#define	 PCIE_CLIENT_INT_INTB				(1 << 6)
#define	 PCIE_CLIENT_INT_INTA				(1 << 5)
#define	 PCIE_CLIENT_INT_LOCAL				(1 << 4)
#define	 PCIE_CLIENT_INT_UDMA				(1 << 3)
#define	 PCIE_CLIENT_INT_PHY				(1 << 2)
#define	 PCIE_CLIENT_INT_HOT_PLUG			(1 << 1)
#define	 PCIE_CLIENT_INT_PWR_STCG			(1 << 0)
#define	 PCIE_CLIENT_INT_LEGACY			(PCIE_CLIENT_INT_INTA | \
						PCIE_CLIENT_INT_INTB | \
						PCIE_CLIENT_INT_INTC | \
						PCIE_CLIENT_INT_INTD)

#define	PCIE_CORE_CTRL0				0x900000
#define	 CORE_CTRL_LANES_GET(x)				(((x) >> 20) & 0x3)
#define	PCIE_CORE_CTRL1				0x900004
#define	PCIE_CORE_CONFIG_VENDOR			0x900044
#define	PCIE_CORE_INT_STATUS			0x90020c
#define	 PCIE_CORE_INT_PRFPE				(1 << 0)
#define	 PCIE_CORE_INT_CRFPE				(1 << 1)
#define	 PCIE_CORE_INT_RRPE				(1 << 2)
#define	 PCIE_CORE_INT_PRFO				(1 << 3)
#define	 PCIE_CORE_INT_CRFO				(1 << 4)
#define	 PCIE_CORE_INT_RT				(1 << 5)
#define	 PCIE_CORE_INT_RTR				(1 << 6)
#define	 PCIE_CORE_INT_PE				(1 << 7)
#define	 PCIE_CORE_INT_MTR				(1 << 8)
#define	 PCIE_CORE_INT_UCR				(1 << 9)
#define	 PCIE_CORE_INT_FCE				(1 << 10)
#define	 PCIE_CORE_INT_CT				(1 << 11)
#define	 PCIE_CORE_INT_UTC				(1 << 18)
#define	 PCIE_CORE_INT_MMVC				(1 << 19)
#define	PCIE_CORE_INT_MASK			0x900210
#define	PCIE_CORE_PHY_FUNC_CONF			0x9002C0
#define	PCIE_CORE_RC_BAR_CONF			0x900300

#define PCIE_RC_CONFIG_STD_BASE			0x800000
#define PCIE_RC_CONFIG_PRIV_BASE		0xA00000
#define	PCIE_RC_CONFIG_DCSR			0xA000C8
#define	 PCIE_RC_CONFIG_DCSR_MPS_MASK			(0x7 << 5)
#define	 PCIE_RC_CONFIG_DCSR_MPS_128			(0 << 5)
#define	 PCIE_RC_CONFIG_DCSR_MPS_256			(1 << 5)
#define	 PCIE_RC_CONFIG_LINK_CAP		0xA00CC
#define   PCIE_RC_CONFIG_LINK_CAP_L0S			(1 << 10)

#define	PCIE_RC_CONFIG_LCS			0xA000D0
#define	PCIE_RC_CONFIG_THP_CAP			0xA00274
#define	 PCIE_RC_CONFIG_THP_CAP_NEXT_MASK		0xFFF00000

#define	PCIE_CORE_OB_ADDR0(n)			(0xC00000 + 0x20 * (n) + 0x00)
#define	PCIE_CORE_OB_ADDR1(n)			(0xC00000 + 0x20 * (n) + 0x04)
#define	PCIE_CORE_OB_DESC0(n)			(0xC00000 + 0x20 * (n) + 0x08)
#define	PCIE_CORE_OB_DESC1(n)			(0xC00000 + 0x20 * (n) + 0x0C)
#define	PCIE_CORE_OB_DESC2(n)			(0xC00000 + 0x20 * (n) + 0x10)
#define	PCIE_CORE_OB_DESC3(n)			(0xC00000 + 0x20 * (n) + 0x14)

#define	PCIE_CORE_IB_ADDR0(n)			(0xC00800 + 0x8 * (n) + 0x00)
#define	PCIE_CORE_IB_ADDR1(n)			(0xC00800 + 0x8 * (n) + 0x04)

#define	PRIV_CFG_RD4(sc, reg)						\
    (uint32_t)rk_pcie_local_cfg_read(sc, true, reg, 4)
#define	PRIV_CFG_RD2(sc, reg)						\
    (uint16_t)rk_pcie_local_cfg_read(sc, true, reg, 2)
#define	PRIV_CFG_RD1(sc, reg)						\
    (uint8_t)rk_pcie_local_cfg_read(sc, true, reg, 1)
#define	PRIV_CFG_WR4(sc, reg, val)					\
    rk_pcie_local_cfg_write(sc, true, reg, val, 4)
#define	PRIV_CFG_WR2(sc, reg, val)					\
    rk_pcie_local_cfg_write(sc, true, reg, val, 2)
#define	PRIV_CFG_WR1(sc, reg, val)					\
    rk_pcie_local_cfg_write(sc, true, reg, val, 1)

#define APB_WR4(_sc, _r, _v)	bus_write_4((_sc)->apb_mem_res, (_r), (_v))
#define	APB_RD4(_sc, _r)	bus_read_4((_sc)->apb_mem_res, (_r))

#define	MAX_LANES	4

#define RK_PCIE_ENABLE_MSI
#define RK_PCIE_ENABLE_MSIX

struct rk_pcie_softc {
	struct ofw_pci_softc	ofw_pci;	/* Must be first */

	struct resource		*axi_mem_res;
	struct resource		*apb_mem_res;
	struct resource		*client_irq_res;
	struct resource		*legacy_irq_res;
	struct resource		*sys_irq_res;
	void			*client_irq_cookie;
	void			*legacy_irq_cookie;
	void			*sys_irq_cookie;

	device_t		dev;
	phandle_t		node;
	struct mtx		mtx;

	struct ofw_pci_range	mem_range;
	struct ofw_pci_range	pref_mem_range;
	struct ofw_pci_range	io_range;

	bool			coherent;
	bus_dma_tag_t		dmat;

	int			num_lanes;
	bool			link_is_gen2;
	bool			no_l0s;

	u_int 			bus_start;
	u_int 			bus_end;
	u_int 			root_bus;
	u_int 			sub_bus;

	regulator_t		supply_12v;
	regulator_t		supply_3v3;
	regulator_t		supply_1v8;
	regulator_t		supply_0v9;
	hwreset_t		hwreset_core;
	hwreset_t		hwreset_mgmt;
	hwreset_t		hwreset_mgmt_sticky;
	hwreset_t		hwreset_pipe;
	hwreset_t		hwreset_pm;
	hwreset_t		hwreset_aclk;
	hwreset_t		hwreset_pclk;
	clk_t			clk_aclk;
	clk_t			clk_aclk_perf;
	clk_t			clk_hclk;
	clk_t			clk_pm;
	phy_t 			phys[MAX_LANES];
	gpio_pin_t		gpio_ep;
};

/* Compatible devices. */
static struct ofw_compat_data compat_data[] = {
	{"rockchip,rk3399-pcie", 1},
	{NULL,		 	 0},
};

static uint32_t
rk_pcie_local_cfg_read(struct rk_pcie_softc *sc, bool priv, u_int reg,
    int bytes)
{
	uint32_t val;
	bus_addr_t base;

	if (priv)
		base = PCIE_RC_CONFIG_PRIV_BASE;
	else
		base = PCIE_RC_CONFIG_STD_BASE;

	switch (bytes) {
	case 4:
		val = bus_read_4(sc->apb_mem_res, base + reg);
		break;
	case 2:
		val = bus_read_2(sc->apb_mem_res, base + reg);
		break;
	case 1:
		val = bus_read_1(sc->apb_mem_res, base + reg);
		break;
	default:
		val = 0xFFFFFFFF;
	}
	return (val);
}

static void
rk_pcie_local_cfg_write(struct rk_pcie_softc *sc, bool priv, u_int reg,
    uint32_t val, int bytes)
{
	uint32_t val2;
	bus_addr_t base;

	if (priv)
		base = PCIE_RC_CONFIG_PRIV_BASE;
	else
		base = PCIE_RC_CONFIG_STD_BASE;

	switch (bytes) {
	case 4:
		bus_write_4(sc->apb_mem_res, base + reg, val);
		break;
	case 2:
		val2 = bus_read_4(sc->apb_mem_res, base + (reg & ~3));
		val2 &= ~(0xffff << ((reg & 3) << 3));
		val2 |= ((val & 0xffff) << ((reg & 3) << 3));
		bus_write_4(sc->apb_mem_res, base + (reg & ~3), val2);
		break;
	case 1:
		val2 = bus_read_4(sc->apb_mem_res, base + (reg & ~3));
		val2 &= ~(0xff << ((reg & 3) << 3));
		val2 |= ((val & 0xff) << ((reg & 3) << 3));
		bus_write_4(sc->apb_mem_res, base + (reg & ~3), val2);
		break;
	}
}

static bool
rk_pcie_check_dev(struct rk_pcie_softc *sc, u_int bus, u_int slot, u_int func,
    u_int reg)
{
	uint32_t val;

	if (bus < sc->bus_start || bus > sc->bus_end || slot > PCI_SLOTMAX ||
	    func > PCI_FUNCMAX || reg > PCIE_REGMAX)
		return (false);

	if (bus == sc->root_bus) {
		/* we have only 1 device with 1 function root port */
		if (slot > 0 || func > 0)
			return (false);
		return (true);
	}

	/* link is needed for accessing non-root busses */
	val = APB_RD4(sc, PCIE_CLIENT_BASIC_STATUS1);
	if (STATUS1_LINK_ST_GET(val) != STATUS1_LINK_ST_UP)
		return (false);

	/* only one device can be on first subordinate bus */
	if (bus == sc->sub_bus  && slot != 0 )
		return (false);
	return (true);
}

static void
rk_pcie_map_out_atu(struct rk_pcie_softc *sc, int idx, int type,
   int num_bits, uint64_t pa)
{
	uint32_t addr0;
	uint64_t max_size __diagused;

	/* Check HW constrains */
	max_size = idx == 0 ? ATU_OB_REGION_0_SIZE: ATU_OB_REGION_SIZE;
	KASSERT(idx <  ATU_OB_REGIONS, ("Invalid region index: %d\n", idx));
	KASSERT(num_bits  >= 7 &&  num_bits <= 63,
	    ("Bit width of region is invalid: %d\n", num_bits));
	KASSERT(max_size <= (1ULL << (num_bits + 1)),
	    ("Bit width is invalid for given region[%d]: %d\n",	idx, num_bits));

	addr0 = (uint32_t)pa & 0xFFFFFF00;
	addr0 |= num_bits;
	APB_WR4(sc, PCIE_CORE_OB_ADDR0(idx), addr0);
	APB_WR4(sc, PCIE_CORE_OB_ADDR1(idx), (uint32_t)(pa >> 32));
	APB_WR4(sc, PCIE_CORE_OB_DESC0(idx), 1 << 23 | type);
	APB_WR4(sc, PCIE_CORE_OB_DESC1(idx), sc->root_bus);

	/* Readback for sync */
	APB_RD4(sc, PCIE_CORE_OB_DESC1(idx));
}

static void
rk_pcie_map_cfg_atu(struct rk_pcie_softc *sc, int idx, int type)
{

	/* Check HW constrains */
	KASSERT(idx <  ATU_OB_REGIONS, ("Invalid region index: %d\n", idx));

	/*
	 * Config window is only 25 bits width, so we cannot encode full bus
	 * range into it. Remaining bits of bus number should be taken from
	 * DESC1 field.
	 */
	APB_WR4(sc, PCIE_CORE_OB_ADDR0(idx), 25 - 1);
	APB_WR4(sc, PCIE_CORE_OB_ADDR1(idx), 0);
	APB_WR4(sc, PCIE_CORE_OB_DESC0(idx), 1 << 23 |  type);
	APB_WR4(sc, PCIE_CORE_OB_DESC1(idx), sc->root_bus);

	/* Readback for sync */
	APB_RD4(sc, PCIE_CORE_OB_DESC1(idx));

}

static void
rk_pcie_map_in_atu(struct rk_pcie_softc *sc, int idx, int num_bits, uint64_t pa)
{
	uint32_t addr0;

	/* Check HW constrains */
	KASSERT(idx <  ATU_IB_REGIONS, ("Invalid region index: %d\n", idx));
	KASSERT(num_bits  >= 7 &&  num_bits <= 63,
	    ("Bit width of region is invalid: %d\n", num_bits));

	addr0 = (uint32_t)pa & 0xFFFFFF00;
	addr0 |= num_bits;
	APB_WR4(sc, PCIE_CORE_IB_ADDR0(idx), addr0);
	APB_WR4(sc, PCIE_CORE_IB_ADDR1(idx), (uint32_t)(pa >> 32));

	/* Readback for sync */
	APB_RD4(sc, PCIE_CORE_IB_ADDR1(idx));
}

static int
rk_pcie_decode_ranges(struct rk_pcie_softc *sc, struct ofw_pci_range *ranges,
     int nranges)
{
	int i;

	for (i = 0; i < nranges; i++) {
		switch(ranges[i].pci_hi & OFW_PCI_PHYS_HI_SPACEMASK) {
		case OFW_PCI_PHYS_HI_SPACE_IO:
			if (sc->io_range.size != 0) {
				device_printf(sc->dev,
				    "Duplicated IO range found in DT\n");
				return (ENXIO);
			}
			sc->io_range = ranges[i];
			break;
		case OFW_PCI_PHYS_HI_SPACE_MEM32:
		case OFW_PCI_PHYS_HI_SPACE_MEM64:
			if (ranges[i].pci_hi & OFW_PCI_PHYS_HI_PREFETCHABLE) {
				if (sc->pref_mem_range.size != 0) {
					device_printf(sc->dev,
					    "Duplicated memory range found "
					    "in DT\n");
					return (ENXIO);
				}
				sc->pref_mem_range = ranges[i];
			} else {
				if (sc->mem_range.size != 0) {
					device_printf(sc->dev,
					    "Duplicated memory range found "
					    "in DT\n");
					return (ENXIO);
				}
				sc->mem_range = ranges[i];
			}
		}
	}
	if (sc->mem_range.size == 0) {
		device_printf(sc->dev,
		    " At least memory range should be defined in DT.\n");
		return (ENXIO);
	}
	return (0);
}

/*-----------------------------------------------------------------------------
 *
 *  P C I B   I N T E R F A C E
 */
static uint32_t
rk_pcie_read_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, int bytes)
{
	struct rk_pcie_softc *sc;
	uint32_t d32, data;
	uint16_t d16;
	uint8_t d8;
	uint64_t addr;
	int type, ret;

	sc = device_get_softc(dev);

	if (!rk_pcie_check_dev(sc, bus, slot, func, reg))
		return (0xFFFFFFFFU);
	if (bus == sc->root_bus)
		return (rk_pcie_local_cfg_read(sc, false, reg, bytes));

	addr = ATU_CFG_BUS(bus) | ATU_CFG_SLOT(slot) | ATU_CFG_FUNC(func) |
	    ATU_CFG_REG(reg);
	type = bus == sc->sub_bus ? ATU_TYPE_CFG0: ATU_TYPE_CFG1;
	rk_pcie_map_cfg_atu(sc, 0, type);

	ret = -1;
	switch (bytes) {
	case 1:
		ret = bus_peek_1(sc->axi_mem_res, addr, &d8);
		data = d8;
		break;
	case 2:
		ret = bus_peek_2(sc->axi_mem_res, addr, &d16);
		data = d16;
		break;
	case 4:
		ret = bus_peek_4(sc->axi_mem_res, addr, &d32);
		data = d32;
		break;
	}
	if (ret != 0)
		data = 0xFFFFFFFF;
	return (data);
}

static void
rk_pcie_write_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, uint32_t val, int bytes)
{
	struct rk_pcie_softc *sc;
	uint64_t addr;
	int type;

	sc = device_get_softc(dev);

	if (!rk_pcie_check_dev(sc, bus, slot, func, reg))
		return;

	if (bus == sc->root_bus)
		return (rk_pcie_local_cfg_write(sc, false,  reg, val, bytes));

	addr = ATU_CFG_BUS(bus) | ATU_CFG_SLOT(slot) | ATU_CFG_FUNC(func) |
	    ATU_CFG_REG(reg);
	type = bus == sc->sub_bus ? ATU_TYPE_CFG0: ATU_TYPE_CFG1;
	rk_pcie_map_cfg_atu(sc, 0, type);

	switch (bytes) {
	case 1:
		bus_poke_1(sc->axi_mem_res, addr, (uint8_t)val);
		break;
	case 2:
		bus_poke_2(sc->axi_mem_res, addr, (uint16_t)val);
		break;
	case 4:
		bus_poke_4(sc->axi_mem_res, addr, val);
		break;
	default:
		break;
	}
}

#ifdef RK_PCIE_ENABLE_MSI
static int
rk_pcie_alloc_msi(device_t pci, device_t child, int count,
    int maxcount, int *irqs)
{
	phandle_t msi_parent;
	int rv;

	rv = ofw_bus_msimap(ofw_bus_get_node(pci), pci_get_rid(child),
	    &msi_parent, NULL);
	if (rv != 0)
		return (rv);

	rv = intr_alloc_msi(pci, child, msi_parent, count, maxcount,irqs);
	return (rv);
}

static int
rk_pcie_release_msi(device_t pci, device_t child, int count, int *irqs)
{
	phandle_t msi_parent;
	int rv;

	rv = ofw_bus_msimap(ofw_bus_get_node(pci), pci_get_rid(child),
	    &msi_parent, NULL);
	if (rv != 0)
		return (rv);
	rv = intr_release_msi(pci, child, msi_parent, count, irqs);
	return (rv);
}
#endif

static int
rk_pcie_map_msi(device_t pci, device_t child, int irq, uint64_t *addr,
    uint32_t *data)
{
	phandle_t msi_parent;
	int rv;

	rv = ofw_bus_msimap(ofw_bus_get_node(pci), pci_get_rid(child),
	    &msi_parent, NULL);
	if (rv != 0)
		return (rv);
	rv = intr_map_msi(pci, child, msi_parent, irq, addr, data);
	return (rv);
}

#ifdef RK_PCIE_ENABLE_MSIX
static int
rk_pcie_alloc_msix(device_t pci, device_t child, int *irq)
{
	phandle_t msi_parent;
	int rv;

	rv = ofw_bus_msimap(ofw_bus_get_node(pci), pci_get_rid(child),
	    &msi_parent, NULL);
	if (rv != 0)
		return (rv);
	rv = intr_alloc_msix(pci, child, msi_parent, irq);
	return (rv);
}

static int
rk_pcie_release_msix(device_t pci, device_t child, int irq)
{
	phandle_t msi_parent;
	int rv;

	rv = ofw_bus_msimap(ofw_bus_get_node(pci), pci_get_rid(child),
	    &msi_parent, NULL);
	if (rv != 0)
		return (rv);
	rv = intr_release_msix(pci, child, msi_parent, irq);
	return (rv);
}
#endif

static int
rk_pcie_get_id(device_t pci, device_t child, enum pci_id_type type,
    uintptr_t *id)
{
	phandle_t node;
	int rv;
	uint32_t rid;
	uint16_t pci_rid;

	if (type != PCI_ID_MSI)
		return (pcib_get_id(pci, child, type, id));

	node = ofw_bus_get_node(pci);
	pci_rid = pci_get_rid(child);

	rv = ofw_bus_msimap(node, pci_rid, NULL, &rid);
	if (rv != 0)
		return (rv);

	*id = rid;
	return (0);
}

static int
rk_pcie_route_interrupt(device_t bus, device_t dev, int pin)
{
	struct rk_pcie_softc *sc;
	u_int irq;

	sc = device_get_softc(bus);
	irq = intr_map_clone_irq(rman_get_start(sc->legacy_irq_res));
	device_printf(bus, "route pin %d for device %d.%d to %u\n",
		    pin, pci_get_slot(dev), pci_get_function(dev), irq);

	return (irq);
}

/*-----------------------------------------------------------------------------
 *
 *  B U S  / D E V I C E   I N T E R F A C E
 */
static int
rk_pcie_parse_fdt_resources(struct rk_pcie_softc *sc)
{
	int i, rv;
	char buf[16];

	/* Regulators. All are optional. */
	rv = regulator_get_by_ofw_property(sc->dev, 0,
	    "vpcie12v-supply", &sc->supply_12v);
	if (rv != 0 && rv != ENOENT) {
		device_printf(sc->dev,"Cannot get 'vpcie12' regulator\n");
		return (ENXIO);
	}
	rv = regulator_get_by_ofw_property(sc->dev, 0,
	    "vpcie3v3-supply", &sc->supply_3v3);
	if (rv != 0 && rv != ENOENT) {
		device_printf(sc->dev,"Cannot get 'vpcie3v3' regulator\n");
		return (ENXIO);
	}
	rv = regulator_get_by_ofw_property(sc->dev, 0,
	    "vpcie1v8-supply", &sc->supply_1v8);
	if (rv != 0 && rv != ENOENT) {
		device_printf(sc->dev,"Cannot get 'vpcie1v8' regulator\n");
		return (ENXIO);
	}
	rv = regulator_get_by_ofw_property(sc->dev, 0,
	    "vpcie0v9-supply", &sc->supply_0v9);
	if (rv != 0 && rv != ENOENT) {
		device_printf(sc->dev,"Cannot get 'vpcie0v9' regulator\n");
		return (ENXIO);
	}

	/* Resets. */
	rv = hwreset_get_by_ofw_name(sc->dev, 0, "core", &sc->hwreset_core);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'core' reset\n");
		return (ENXIO);
	}
	rv = hwreset_get_by_ofw_name(sc->dev, 0, "mgmt", &sc->hwreset_mgmt);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'mgmt' reset\n");
		return (ENXIO);
	}
	rv = hwreset_get_by_ofw_name(sc->dev, 0, "mgmt-sticky",
	    &sc->hwreset_mgmt_sticky);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'mgmt-sticky' reset\n");
		return (ENXIO);
	}
	rv = hwreset_get_by_ofw_name(sc->dev, 0, "pipe", &sc->hwreset_pipe);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'pipe' reset\n");
		return (ENXIO);
	}
	rv = hwreset_get_by_ofw_name(sc->dev, 0, "pm", &sc->hwreset_pm);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'pm' reset\n");
		return (ENXIO);
	}
	rv = hwreset_get_by_ofw_name(sc->dev, 0, "aclk", &sc->hwreset_aclk);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'aclk' reset\n");
		return (ENXIO);
	}
	rv = hwreset_get_by_ofw_name(sc->dev, 0, "pclk", &sc->hwreset_pclk);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'pclk' reset\n");
		return (ENXIO);
	}

	/* Clocks. */
	rv = clk_get_by_ofw_name(sc->dev, 0, "aclk", &sc->clk_aclk);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'aclk' clock\n");
		return (ENXIO);
	}
	rv = clk_get_by_ofw_name(sc->dev, 0, "aclk-perf", &sc->clk_aclk_perf);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'aclk-perf' clock\n");
		return (ENXIO);
	}
	rv = clk_get_by_ofw_name(sc->dev, 0, "hclk", &sc->clk_hclk);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'hclk' clock\n");
		return (ENXIO);
	}
	rv = clk_get_by_ofw_name(sc->dev, 0, "pm", &sc->clk_pm);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'pm' clock\n");
		return (ENXIO);
	}

	/* Phys. */
	for (i = 0; i < MAX_LANES; i++ ) {
		sprintf (buf, "pcie-phy-%d", i);
		rv = phy_get_by_ofw_name(sc->dev, 0, buf, sc->phys + i);
		if (rv != 0) {
			device_printf(sc->dev, "Cannot get '%s' phy\n", buf);
			return (ENXIO);
		}
	}

	/* GPIO for PERST#. Optional */
	rv = gpio_pin_get_by_ofw_property(sc->dev, sc->node, "ep-gpios",
	    &sc->gpio_ep);
	if (rv != 0 && rv != ENOENT) {
		device_printf(sc->dev, "Cannot get 'ep-gpios' gpio\n");
		return (ENXIO);
	}

	return (0);
}

static int
rk_pcie_enable_resources(struct rk_pcie_softc *sc)
{
	int i, rv;
	uint32_t val;

	/* Assert all resets */
	rv = hwreset_assert(sc->hwreset_pclk);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot assert 'pclk' reset\n");
		return (rv);
	}
	rv = hwreset_assert(sc->hwreset_aclk);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot assert 'aclk' reset\n");
		return (rv);
	}
	rv = hwreset_assert(sc->hwreset_pm);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot assert 'pm' reset\n");
		return (rv);
	}
	rv = hwreset_assert(sc->hwreset_pipe);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot assert 'pipe' reset\n");
		return (rv);
	}
	rv = hwreset_assert(sc->hwreset_mgmt_sticky);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot assert 'mgmt_sticky' reset\n");
		return (rv);
	}
	rv = hwreset_assert(sc->hwreset_mgmt);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot assert 'hmgmt' reset\n");
		return (rv);
	}
	rv = hwreset_assert(sc->hwreset_core);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot assert 'hcore' reset\n");
		return (rv);
	}
	DELAY(10000);

	/* Enable clockls */
	rv = clk_enable(sc->clk_aclk);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot enable 'aclk' clock\n");
		return (rv);
	}
	rv = clk_enable(sc->clk_aclk_perf);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot enable 'aclk_perf' clock\n");
		return (rv);
	}
	rv = clk_enable(sc->clk_hclk);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot enable 'hclk' clock\n");
		return (rv);
	}
	rv = clk_enable(sc->clk_pm);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot enable 'pm' clock\n");
		return (rv);
	}

	/* Power up regulators */
	if (sc->supply_12v != NULL) {
		rv = regulator_enable(sc->supply_12v);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot enable 'vpcie12' regulator\n");
			return (rv);
		}
	}
	if (sc->supply_3v3 != NULL) {
		rv = regulator_enable(sc->supply_3v3);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot enable 'vpcie3v3' regulator\n");
			return (rv);
		}
	}
	if (sc->supply_1v8 != NULL) {
		rv = regulator_enable(sc->supply_1v8);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot enable 'vpcie1v8' regulator\n");
			return (rv);
		}
	}
	if (sc->supply_0v9 != NULL) {
		rv = regulator_enable(sc->supply_0v9);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot enable 'vpcie1v8' regulator\n");
			return (rv);
		}
	}
	DELAY(1000);

	/* Deassert basic resets*/
	rv = hwreset_deassert(sc->hwreset_pm);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot deassert 'pm' reset\n");
		return (rv);
	}
	rv = hwreset_deassert(sc->hwreset_aclk);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot deassert 'aclk' reset\n");
		return (rv);
	}
	rv = hwreset_deassert(sc->hwreset_pclk);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot deassert 'pclk' reset\n");
		return (rv);
	}

	/* Set basic PCIe core mode (RC, lanes, gen1 or 2) */
	val  = STRAP_CONF_GEN_2 << 16 |
	    (sc->link_is_gen2 ? STRAP_CONF_GEN_2: 0);
	val |= STRAP_CONF_MODE_RC << 16 | STRAP_CONF_MODE_RC;
	val |= STRAP_CONF_LANES(~0) << 16 | STRAP_CONF_LANES(sc->num_lanes);
	val |= STRAP_CONF_ARI_EN << 16 | STRAP_CONF_ARI_EN;
	val |= STRAP_CONF_CONF_EN << 16 | STRAP_CONF_CONF_EN;
	APB_WR4(sc, PCIE_CLIENT_BASIC_STRAP_CONF, val);

	for (i = 0; i < MAX_LANES; i++) {
		rv = phy_enable(sc->phys[i]);
		if (rv != 0) {
			device_printf(sc->dev, "Cannot enable phy %d\n", i);
			return (rv);
		}
	}

	/* Deassert rest of resets - order is important ! */
	rv = hwreset_deassert(sc->hwreset_mgmt_sticky);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot deassert 'mgmt_sticky' reset\n");
		return (rv);
	}
	rv = hwreset_deassert(sc->hwreset_core);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot deassert 'core' reset\n");
		return (rv);
	}
	rv = hwreset_deassert(sc->hwreset_mgmt);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot deassert 'mgmt' reset\n");
		return (rv);
	}
	rv = hwreset_deassert(sc->hwreset_pipe);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot deassert 'pipe' reset\n");
		return (rv);
	}
	return (0);
}

static int
rk_pcie_setup_hw(struct rk_pcie_softc *sc)
{
	uint32_t val;
	int i, rv;

	/* Assert PERST# if defined */
	if (sc->gpio_ep != NULL) {
		rv = gpio_pin_set_active(sc->gpio_ep, 0);
		if (rv != 0) {
			device_printf(sc->dev,
			    "Cannot clear 'gpio-ep' gpio\n");
			return (rv);
		}
	}

	rv = rk_pcie_enable_resources(sc);
	if (rv != 0)
		return(rv);

	/* Fix wrong default value for transmited FTS for L0s exit */
	val = APB_RD4(sc, PCIE_CORE_CTRL1);
	val |= 0xFFFF << 8;
	APB_WR4(sc, PCIE_CORE_CTRL1, val);

	/* Setup PCIE Link Status & Control register */
	val = APB_RD4(sc, PCIE_RC_CONFIG_LCS);
	val |= PCIEM_LINK_CTL_COMMON_CLOCK;
	APB_WR4(sc, PCIE_RC_CONFIG_LCS, val);
	val = APB_RD4(sc, PCIE_RC_CONFIG_LCS);
	val |= PCIEM_LINK_CTL_RCB;
	APB_WR4(sc, PCIE_RC_CONFIG_LCS, val);

	/* Enable training for GEN1 */
	APB_WR4(sc, PCIE_CLIENT_BASIC_STRAP_CONF,
	    STRAP_CONF_LINK_TRAIN_EN << 16 | STRAP_CONF_LINK_TRAIN_EN);

	/* Deassert PERST# if defined */
	if (sc->gpio_ep != NULL) {
		rv = gpio_pin_set_active(sc->gpio_ep, 1);
		if (rv != 0) {
			device_printf(sc->dev, "Cannot set 'gpio-ep' gpio\n");
			return (rv);
		}
	}

	/* Wait for link */
	for (i = 500; i > 0; i--) {
		val = APB_RD4(sc, PCIE_CLIENT_BASIC_STATUS1);
		if (STATUS1_LINK_ST_GET(val) == STATUS1_LINK_ST_UP)
			break;
		DELAY(1000);
	}
	if (i <= 0) {
		device_printf(sc->dev,
		    "Gen1 link training timeouted: 0x%08X.\n", val);
		return (0);
	}

	if (sc->link_is_gen2) {
			val = APB_RD4(sc, PCIE_RC_CONFIG_LCS);
			val |= PCIEM_LINK_CTL_RETRAIN_LINK;
			APB_WR4(sc, PCIE_RC_CONFIG_LCS, val);

			/* Wait for link */
			for (i = 500; i > 0; i--) {
				val = APB_RD4(sc, PCIE_CLIENT_BASIC_STATUS1);
				if (STATUS1_LINK_ST_GET(val) ==
				    STATUS1_LINK_ST_UP)
					break;
				DELAY(1000);
			}
			if (i <= 0)
				device_printf(sc->dev, "Gen2 link training "
				    "timeouted: 0x%08X.\n", val);
	}

	val = APB_RD4(sc, PCIE_CORE_CTRL0);
	val = CORE_CTRL_LANES_GET(val);
	if (bootverbose)
		device_printf(sc->dev, "Link width: %d\n", 1 << val);

	return (0);
}

static int
rk_pcie_setup_sw(struct rk_pcie_softc *sc)
{
	uint32_t val;
	int i, region;

	pcib_bridge_init(sc->dev);

	/* Setup config registers */
	APB_WR4(sc, PCIE_CORE_CONFIG_VENDOR, 0x1D87); /* Rockchip vendor ID*/
	PRIV_CFG_WR1(sc, PCIR_CLASS, PCIC_BRIDGE);
	PRIV_CFG_WR1(sc, PCIR_SUBCLASS, PCIS_BRIDGE_PCI);
	PRIV_CFG_WR1(sc, PCIR_PRIBUS_1, sc->root_bus);
	PRIV_CFG_WR1(sc, PCIR_SECBUS_1, sc->sub_bus);
	PRIV_CFG_WR1(sc, PCIR_SUBBUS_1, sc->bus_end);
	PRIV_CFG_WR2(sc, PCIR_COMMAND, PCIM_CMD_MEMEN |
	   PCIM_CMD_BUSMASTEREN | PCIM_CMD_SERRESPEN);

	/* Don't advertise L1 power substate */
	val = APB_RD4(sc, PCIE_RC_CONFIG_THP_CAP);
	val &= ~PCIE_RC_CONFIG_THP_CAP_NEXT_MASK;
	APB_WR4(sc, PCIE_RC_CONFIG_THP_CAP, val);

	/* Don't advertise L0s */
	if (sc->no_l0s) {
		val = APB_RD4(sc, PCIE_RC_CONFIG_LINK_CAP);
		val &= ~PCIE_RC_CONFIG_THP_CAP_NEXT_MASK;
		APB_WR4(sc, PCIE_RC_CONFIG_LINK_CAP_L0S, val);
	}

	/*Adjust maximum payload size*/
	val = APB_RD4(sc, PCIE_RC_CONFIG_DCSR);
	val &= ~PCIE_RC_CONFIG_DCSR_MPS_MASK;
	val |= PCIE_RC_CONFIG_DCSR_MPS_128;
	APB_WR4(sc, PCIE_RC_CONFIG_DCSR, val);

	/*
	 * Prepare IB ATU
	 * map whole address range in 1:1 mappings
	 */
	rk_pcie_map_in_atu(sc, 2, 64 - 1, 0);

	/* Prepare OB ATU */
	/* - region 0 (32 MB) is used for config access */
	region = 0;
	rk_pcie_map_out_atu(sc, region++, ATU_TYPE_CFG0, 25 - 1, 0);

	/* - then map memory (by using 1MB regions */
	for (i = 0; i  < sc->mem_range.size / ATU_OB_REGION_SIZE; i++) {
		rk_pcie_map_out_atu(sc,  region++, ATU_TYPE_MEM,
		    ATU_OB_REGION_SHIFT - 1,
		    sc->mem_range.pci + ATU_OB_REGION_SIZE * i);
	}

	/* - IO space is next, one region typically*/
	for (i = 0; i  < sc->io_range.size / ATU_OB_REGION_SIZE; i++) {
		rk_pcie_map_out_atu(sc, region++, ATU_TYPE_IO,
		    ATU_OB_REGION_SHIFT - 1,
		    sc->io_range.pci + ATU_OB_REGION_SIZE * i);
	}
	APB_WR4(sc, PCIE_CORE_RC_BAR_CONF, 0);
	return (0);
}

static int
rk_pcie_sys_irq(void *arg)
{
	struct rk_pcie_softc *sc;
	uint32_t irq;

	sc = (struct rk_pcie_softc *)arg;
	irq = APB_RD4(sc, PCIE_CLIENT_INT_STATUS);
	if (irq & PCIE_CLIENT_INT_LOCAL) {
		irq = APB_RD4(sc, PCIE_CORE_INT_STATUS);
		APB_WR4(sc, PCIE_CORE_INT_STATUS, irq);
		APB_WR4(sc, PCIE_CLIENT_INT_STATUS, PCIE_CLIENT_INT_LOCAL);

		device_printf(sc->dev, "'sys' interrupt received: 0x%04X\n",
		    irq);
	}

	return (FILTER_HANDLED);
}

static int
rk_pcie_client_irq(void *arg)
{
	struct rk_pcie_softc *sc;
	uint32_t irq;

	sc = (struct rk_pcie_softc *)arg;
	irq = APB_RD4(sc, PCIE_CLIENT_INT_STATUS);
	/* Clear causes handled by other interrups */
	irq &= ~PCIE_CLIENT_INT_LOCAL;
	irq &= ~PCIE_CLIENT_INT_LEGACY;
	APB_WR4(sc, PCIE_CLIENT_INT_STATUS, irq);

	device_printf(sc->dev, "'client' interrupt received: 0x%04X\n", irq);

	return (FILTER_HANDLED);
}

static int
rk_pcie_legacy_irq(void *arg)
{
	struct rk_pcie_softc *sc;
	uint32_t irq;

	sc = (struct rk_pcie_softc *)arg;
	irq = APB_RD4(sc, PCIE_CLIENT_INT_STATUS);
	irq &= PCIE_CLIENT_INT_LEGACY;
	APB_WR4(sc, PCIE_CLIENT_INT_STATUS, irq);

	/* all legacy interrupt are shared, do nothing */
	return (FILTER_STRAY);
}

static bus_dma_tag_t
rk_pcie_get_dma_tag(device_t dev, device_t child)
{
	struct rk_pcie_softc *sc;

	sc = device_get_softc(dev);
	return (sc->dmat);
}

static int
rk_pcie_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Rockchip PCIe controller");
	return (BUS_PROBE_DEFAULT);
}

static int
rk_pcie_attach(device_t dev)
{
	struct resource_map_request req;
	struct resource_map map;
	struct rk_pcie_softc *sc;
	uint32_t val;
	int rv, rid, max_speed;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->node = ofw_bus_get_node(dev);

	mtx_init(&sc->mtx, "rk_pcie_mtx", NULL, MTX_DEF);

	/* XXX Should not be this configurable ? */
	sc->bus_start = 0;
	sc->bus_end =  0x1F;
	sc->root_bus = sc->bus_start;
	sc->sub_bus = 1;

	/* Read FDT properties */
	rv = rk_pcie_parse_fdt_resources(sc);
	if (rv != 0)
		goto out;

	sc->coherent = OF_hasprop(sc->node, "dma-coherent");
	sc->no_l0s = OF_hasprop(sc->node, "aspm-no-l0s");
	rv = OF_getencprop(sc->node, "num-lanes", &sc->num_lanes,
	    sizeof(sc->num_lanes));
	if (rv != sizeof(sc->num_lanes))
		sc->num_lanes = 1;
	if (sc->num_lanes != 1 && sc->num_lanes != 2 && sc->num_lanes != 4) {
		device_printf(dev,
		    "invalid number of lanes: %d\n",sc->num_lanes);
		sc->num_lanes = 0;
		rv = ENXIO;
		goto out;
	}

	rv = OF_getencprop(sc->node, "max-link-speed", &max_speed,
	    sizeof(max_speed));
	if (rv != sizeof(max_speed) || max_speed != 1)
		sc->link_is_gen2 = true;
	else
		sc->link_is_gen2 = false;

	rv = ofw_bus_find_string_index(sc->node, "reg-names", "axi-base", &rid);
	if (rv != 0) {
		device_printf(dev, "Cannot get 'axi-base' memory\n");
		rv = ENXIO;
		goto out;
	}
	sc->axi_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE | RF_UNMAPPED);
	if (sc->axi_mem_res == NULL) {
		device_printf(dev, "Cannot allocate 'axi-base' (rid: %d)\n",
		    rid);
		rv = ENXIO;
		goto out;
	}
	resource_init_map_request(&req);
	req.memattr = VM_MEMATTR_DEVICE_NP;
	rv = bus_map_resource(dev, SYS_RES_MEMORY, sc->axi_mem_res, &req,
	    &map);
	if (rv != 0) {
		device_printf(dev, "Cannot map 'axi-base' (rid: %d)\n",
		    rid);
		goto out;
	}
	rman_set_mapping(sc->axi_mem_res, &map);

	rv = ofw_bus_find_string_index(sc->node, "reg-names", "apb-base", &rid);
	if (rv != 0) {
		device_printf(dev, "Cannot get 'apb-base' memory\n");
		rv = ENXIO;
		goto out;
	}
	sc->apb_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->apb_mem_res == NULL) {
		device_printf(dev, "Cannot allocate 'apb-base' (rid: %d)\n",
		    rid);
		rv = ENXIO;
		goto out;
	}

	rv = ofw_bus_find_string_index(sc->node, "interrupt-names",
	    "client", &rid);
	if (rv != 0) {
		device_printf(dev, "Cannot get 'client' IRQ\n");
		rv = ENXIO;
		goto out;
	}
	sc->client_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->client_irq_res == NULL) {
		device_printf(dev, "Cannot allocate 'client' IRQ resource\n");
		rv = ENXIO;
		goto out;
	}

	rv = ofw_bus_find_string_index(sc->node, "interrupt-names",
	    "legacy", &rid);
	if (rv != 0) {
		device_printf(dev, "Cannot get 'legacy' IRQ\n");
		rv = ENXIO;
		goto out;
	}
	sc->legacy_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->legacy_irq_res == NULL) {
		device_printf(dev, "Cannot allocate 'legacy' IRQ resource\n");
		rv = ENXIO;
		goto out;
	}

	rv = ofw_bus_find_string_index(sc->node, "interrupt-names",
	    "sys", &rid);
	if (rv != 0) {
		device_printf(dev, "Cannot get 'sys' IRQ\n");
		rv = ENXIO;
		goto out;
	}
	sc->sys_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sc->sys_irq_res == NULL) {
		device_printf(dev, "Cannot allocate 'sys' IRQ resource\n");
		rv = ENXIO;
		goto out;
	}

	if (bootverbose)
		device_printf(dev, "Bus is%s cache-coherent\n",
		    sc->coherent ? "" : " not");
	rv = bus_dma_tag_create(bus_get_dma_tag(dev), /* parent */
	    1, 0,				/* alignment, bounds */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filter, filterarg */
	    BUS_SPACE_MAXSIZE,			/* maxsize */
	    BUS_SPACE_UNRESTRICTED,		/* nsegments */
	    BUS_SPACE_MAXSIZE,			/* maxsegsize */
	    sc->coherent ? BUS_DMA_COHERENT : 0, /* flags */
	    NULL, NULL,				/* lockfunc, lockarg */
	    &sc->dmat);
	if (rv != 0)
		goto out;

	rv = ofw_pcib_init(dev);
	if (rv != 0)
		goto out;

	rv = rk_pcie_decode_ranges(sc, sc->ofw_pci.sc_range,
	    sc->ofw_pci.sc_nrange);
	if (rv != 0)
		goto out_full;
	rv = rk_pcie_setup_hw(sc);
	if (rv != 0)
		goto out_full;

	rv = rk_pcie_setup_sw(sc);
	if (rv != 0)
		goto out_full;

	rv = bus_setup_intr(dev, sc->client_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	   rk_pcie_client_irq, NULL, sc, &sc->client_irq_cookie);
	if (rv != 0) {
		device_printf(dev, "cannot setup client interrupt handler\n");
		rv = ENXIO;
		goto out_full;
	}

	rv = bus_setup_intr(dev, sc->legacy_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	   rk_pcie_legacy_irq, NULL, sc, &sc->legacy_irq_cookie);
	if (rv != 0) {
		device_printf(dev, "cannot setup client interrupt handler\n");
		rv = ENXIO;
		goto out_full;
	}

	rv = bus_setup_intr(dev, sc->sys_irq_res, INTR_TYPE_BIO | INTR_MPSAFE,
	   rk_pcie_sys_irq, NULL, sc, &sc->sys_irq_cookie);
	if (rv != 0) {
		device_printf(dev, "cannot setup client interrupt handler\n");
		rv = ENXIO;
		goto out_full;
	}

	/* Enable interrupts */
	val =
	    PCIE_CLIENT_INT_CORR_ERR | PCIE_CLIENT_INT_NFATAL_ERR |
	    PCIE_CLIENT_INT_FATAL_ERR | PCIE_CLIENT_INT_DPA |
	    PCIE_CLIENT_INT_HOT_RST | PCIE_CLIENT_INT_MSG |
	    PCIE_CLIENT_INT_LEGACY_DONE | PCIE_CLIENT_INT_INTA |
	    PCIE_CLIENT_INT_INTB | PCIE_CLIENT_INT_INTC |
	    PCIE_CLIENT_INT_INTD | PCIE_CLIENT_INT_PHY;

	APB_WR4(sc, PCIE_CLIENT_INT_MASK, (val << 16) &  ~val);

	val =
	    PCIE_CORE_INT_PRFPE | PCIE_CORE_INT_CRFPE |
	    PCIE_CORE_INT_RRPE | PCIE_CORE_INT_CRFO |
	    PCIE_CORE_INT_RT | PCIE_CORE_INT_RTR |
	    PCIE_CORE_INT_PE | PCIE_CORE_INT_MTR |
	    PCIE_CORE_INT_UCR | PCIE_CORE_INT_FCE |
	    PCIE_CORE_INT_CT | PCIE_CORE_INT_UTC |
	    PCIE_CORE_INT_MMVC;
	APB_WR4(sc, PCIE_CORE_INT_MASK, ~(val));

	val  = APB_RD4(sc, PCIE_RC_CONFIG_LCS);
	val |= PCIEM_LINK_CTL_LBMIE | PCIEM_LINK_CTL_LABIE;
	APB_WR4(sc, PCIE_RC_CONFIG_LCS, val);

	DELAY(250000);
	device_add_child(dev, "pci", -1);
	return (bus_generic_attach(dev));

out_full:
	bus_teardown_intr(dev, sc->sys_irq_res, sc->sys_irq_cookie);
	bus_teardown_intr(dev, sc->legacy_irq_res, sc->legacy_irq_cookie);
	bus_teardown_intr(dev, sc->client_irq_res, sc->client_irq_cookie);
	ofw_pcib_fini(dev);
out:
	bus_dma_tag_destroy(sc->dmat);
	bus_free_resource(dev, SYS_RES_IRQ, sc->sys_irq_res);
	bus_free_resource(dev, SYS_RES_IRQ, sc->legacy_irq_res);
	bus_free_resource(dev, SYS_RES_IRQ, sc->client_irq_res);
	bus_free_resource(dev, SYS_RES_MEMORY, sc->apb_mem_res);
	bus_free_resource(dev, SYS_RES_MEMORY, sc->axi_mem_res);
	/* GPIO */
	gpio_pin_release(sc->gpio_ep);
	/* Phys */
	for (int i = 0; i < MAX_LANES; i++) {
		phy_release(sc->phys[i]);
	}
	/* Clocks */
	clk_release(sc->clk_aclk);
	clk_release(sc->clk_aclk_perf);
	clk_release(sc->clk_hclk);
	clk_release(sc->clk_pm);
	/* Resets */
	hwreset_release(sc->hwreset_core);
	hwreset_release(sc->hwreset_mgmt);
	hwreset_release(sc->hwreset_pipe);
	hwreset_release(sc->hwreset_pm);
	hwreset_release(sc->hwreset_aclk);
	hwreset_release(sc->hwreset_pclk);
	/* Regulators */
	regulator_release(sc->supply_12v);
	regulator_release(sc->supply_3v3);
	regulator_release(sc->supply_1v8);
	regulator_release(sc->supply_0v9);
	return (rv);
}

static device_method_t rk_pcie_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk_pcie_probe),
	DEVMETHOD(device_attach,	rk_pcie_attach),

	/* Bus interface */
	DEVMETHOD(bus_get_dma_tag,	rk_pcie_get_dma_tag),

	/* pcib interface */
	DEVMETHOD(pcib_read_config,	rk_pcie_read_config),
	DEVMETHOD(pcib_write_config,	rk_pcie_write_config),
	DEVMETHOD(pcib_route_interrupt,	rk_pcie_route_interrupt),
#ifdef RK_PCIE_ENABLE_MSI
	DEVMETHOD(pcib_alloc_msi,	rk_pcie_alloc_msi),
	DEVMETHOD(pcib_release_msi,	rk_pcie_release_msi),
#endif
#ifdef RK_PCIE_ENABLE_MSIX
	DEVMETHOD(pcib_alloc_msix,	rk_pcie_alloc_msix),
	DEVMETHOD(pcib_release_msix,	rk_pcie_release_msix),
#endif
	DEVMETHOD(pcib_map_msi,		rk_pcie_map_msi),
	DEVMETHOD(pcib_get_id,		rk_pcie_get_id),

	/* OFW bus interface */
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	DEVMETHOD_END
};

DEFINE_CLASS_1(pcib, rk_pcie_driver, rk_pcie_methods,
    sizeof(struct rk_pcie_softc), ofw_pcib_driver);
DRIVER_MODULE( rk_pcie, simplebus, rk_pcie_driver, NULL, NULL);
