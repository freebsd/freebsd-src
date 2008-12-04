/*-
 * Copyright (C) 2008 MARVELL INTERNATIONAL LTD.
 * All rights reserved.
 *
 * Developed by Semihalf.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of MARVELL nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Marvell integrated PCI/PCI-Express controller driver.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/endian.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>

#include "pcib_if.h"

#include <machine/resource.h>
#include <machine/bus.h>

#include <arm/mv/mvreg.h>
#include <arm/mv/mvvar.h>

#define PCI_CFG_ENA		(1 << 31)
#define PCI_CFG_BUS(bus)	(((bus) & 0xff) << 16)
#define PCI_CFG_DEV(dev)	(((dev) & 0x1f) << 11)
#define PCI_CFG_FUN(fun)	(((fun) & 0x7) << 8)
#define PCI_CFG_PCIE_REG(reg)	((reg) & 0xfc)

#define PCI_REG_CFG_ADDR	0x0C78
#define PCI_REG_CFG_DATA	0x0C7C
#define PCI_REG_P2P_CONF	0x1D14

#define PCIE_REG_CFG_ADDR	0x18F8
#define PCIE_REG_CFG_DATA	0x18FC
#define PCIE_REG_CONTROL	0x1A00
#define   PCIE_CTRL_LINK1X	0x00000001
#define PCIE_REG_STATUS		0x1A04
#define PCIE_REG_IRQ_MASK	0x1910

#define STATUS_BUS_OFFS		8
#define STATUS_BUS_MASK		(0xFF << STATUS_BUS_OFFS)
#define STATUS_DEV_OFFS		16
#define STATUS_DEV_MASK		(0x1F << STATUS_DEV_OFFS)

#define P2P_CONF_BUS_OFFS	16
#define P2P_CONF_BUS_MASK	(0xFF << P2P_CONF_BUS_OFFS)
#define P2P_CONF_DEV_OFFS	24
#define P2P_CONF_DEV_MASK	(0x1F << P2P_CONF_DEV_OFFS)

#define PCI_VENDORID_MRVL	0x11AB

struct pcib_mbus_softc {
	device_t	sc_dev;

	bus_addr_t	sc_iomem_base;
	bus_addr_t	sc_iomem_size;
	bus_addr_t	sc_iomem_alloc;		/* Next allocation. */

	bus_addr_t	sc_ioport_base;
	bus_addr_t	sc_ioport_size;
	bus_addr_t	sc_ioport_alloc;	/* Next allocation. */

	struct resource	*sc_res;
	bus_space_handle_t sc_bsh;
	bus_space_tag_t	sc_bst;
	int		sc_rid;

	int		sc_busnr;		/* Host bridge bus number */
	int		sc_devnr;		/* Host bridge device number */

	const struct obio_pci *sc_info;
};

static void pcib_mbus_identify(driver_t *driver, device_t parent);
static int pcib_mbus_probe(device_t);
static int pcib_mbus_attach(device_t);

static struct resource *pcib_mbus_alloc_resource(device_t, device_t, int, int *,
    u_long, u_long, u_long, u_int);
static int pcib_mbus_release_resource(device_t, device_t, int, int,
    struct resource *);
static int pcib_mbus_read_ivar(device_t, device_t, int, uintptr_t *);
static int pcib_mbus_write_ivar(device_t, device_t, int, uintptr_t);

static int pcib_mbus_maxslots(device_t);
static uint32_t pcib_mbus_read_config(device_t, u_int, u_int, u_int, u_int,
    int);
static void pcib_mbus_write_config(device_t, u_int, u_int, u_int, u_int,
    uint32_t, int);
static int pcib_mbus_init(struct pcib_mbus_softc *sc, int bus, int maxslot);
static int pcib_mbus_init_bar(struct pcib_mbus_softc *sc, int bus, int slot,
    int func, int barno);
static void pcib_mbus_init_bridge(struct pcib_mbus_softc *sc, int bus, int slot,
    int func);
static int pcib_mbus_init_resources(struct pcib_mbus_softc *sc, int bus,
    int slot, int func, int hdrtype);

/*
 * Bus interface definitions.
 */
static device_method_t pcib_mbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,		pcib_mbus_identify),
	DEVMETHOD(device_probe,			pcib_mbus_probe),
	DEVMETHOD(device_attach,		pcib_mbus_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,		bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,		pcib_mbus_read_ivar),
	DEVMETHOD(bus_write_ivar,		pcib_mbus_write_ivar),
	DEVMETHOD(bus_alloc_resource,		pcib_mbus_alloc_resource),
	DEVMETHOD(bus_release_resource,		pcib_mbus_release_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,		bus_generic_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,		pcib_mbus_maxslots),
	DEVMETHOD(pcib_read_config,		pcib_mbus_read_config),
	DEVMETHOD(pcib_write_config,		pcib_mbus_write_config),
	DEVMETHOD(pcib_route_interrupt,		pcib_route_interrupt),

	{ 0, 0 }
};

static driver_t pcib_mbus_driver = {
	"pcib",
	pcib_mbus_methods,
	sizeof(struct pcib_mbus_softc),
};

devclass_t pcib_devclass;

DRIVER_MODULE(pcib, mbus, pcib_mbus_driver, pcib_devclass, 0, 0);

static struct mtx pcicfg_mtx;

static inline void
pcib_write_irq_mask(struct pcib_mbus_softc *sc, uint32_t mask)
{

	if (!sc->sc_info->op_type != MV_TYPE_PCI)
		return;

	bus_space_write_4(sc->sc_bst, sc->sc_bsh,
	    PCIE_REG_IRQ_MASK, mask);
}

static void
pcib_mbus_hw_cfginit(void)
{
	static int opened = 0;

	if (opened)
		return;

	mtx_init(&pcicfg_mtx, "pcicfg", NULL, MTX_SPIN);
	opened = 1;
}

static uint32_t
pcib_mbus_hw_cfgread(struct pcib_mbus_softc *sc, u_int bus, u_int slot,
    u_int func, u_int reg, int bytes)
{
	uint32_t addr, data, ca, cd;

	ca = (sc->sc_info->op_type != MV_TYPE_PCI) ?
	    PCIE_REG_CFG_ADDR : PCI_REG_CFG_ADDR;
	cd = (sc->sc_info->op_type != MV_TYPE_PCI) ?
	    PCIE_REG_CFG_DATA : PCI_REG_CFG_DATA;
	addr = PCI_CFG_ENA | PCI_CFG_BUS(bus) | PCI_CFG_DEV(slot) |
	    PCI_CFG_FUN(func) | PCI_CFG_PCIE_REG(reg);

	mtx_lock_spin(&pcicfg_mtx);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, ca, addr);

	data = ~0;
	switch (bytes) {
	case 1:
		data = bus_space_read_1(sc->sc_bst, sc->sc_bsh,
		    cd + (reg & 3));
		break;
	case 2:
		data = le16toh(bus_space_read_2(sc->sc_bst, sc->sc_bsh,
		    cd + (reg & 2)));
		break;
	case 4:
		data = le32toh(bus_space_read_4(sc->sc_bst, sc->sc_bsh,
		    cd));
		break;
	}
	mtx_unlock_spin(&pcicfg_mtx);
	return (data);
}

static void
pcib_mbus_hw_cfgwrite(struct pcib_mbus_softc *sc, u_int bus, u_int slot,
    u_int func, u_int reg, uint32_t data, int bytes)
{
	uint32_t addr, ca, cd;

	ca = (sc->sc_info->op_type != MV_TYPE_PCI) ?
	    PCIE_REG_CFG_ADDR : PCI_REG_CFG_ADDR;
	cd = (sc->sc_info->op_type != MV_TYPE_PCI) ?
	    PCIE_REG_CFG_DATA : PCI_REG_CFG_DATA;
	addr = PCI_CFG_ENA | PCI_CFG_BUS(bus) | PCI_CFG_DEV(slot) |
	    PCI_CFG_FUN(func) | PCI_CFG_PCIE_REG(reg);

	mtx_lock_spin(&pcicfg_mtx);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, ca, addr);

	switch (bytes) {
	case 1:
		bus_space_write_1(sc->sc_bst, sc->sc_bsh,
		    cd + (reg & 3), data);
		break;
	case 2:
		bus_space_write_2(sc->sc_bst, sc->sc_bsh,
		    cd + (reg & 2), htole16(data));
		break;
	case 4:
		bus_space_write_4(sc->sc_bst, sc->sc_bsh,
		    cd, htole32(data));
		break;
	}
	mtx_unlock_spin(&pcicfg_mtx);
}

static int
pcib_mbus_maxslots(device_t dev)
{
	struct pcib_mbus_softc *sc = device_get_softc(dev);

	return ((sc->sc_info->op_type != MV_TYPE_PCI) ? 1 : PCI_SLOTMAX);
}

static uint32_t
pcib_mbus_read_config(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, int bytes)
{
	struct pcib_mbus_softc *sc = device_get_softc(dev);

	/* Skip self */
	if (bus == sc->sc_busnr && slot == sc->sc_devnr)
		return (~0U);

	return (pcib_mbus_hw_cfgread(sc, bus, slot, func, reg, bytes));
}

static void
pcib_mbus_write_config(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, uint32_t val, int bytes)
{
	struct pcib_mbus_softc *sc = device_get_softc(dev);

	/* Skip self */
	if (bus == sc->sc_busnr && slot == sc->sc_devnr)
		return;

	pcib_mbus_hw_cfgwrite(sc, bus, slot, func, reg, val, bytes);
}

static void
pcib_mbus_add_child(driver_t *driver, device_t parent, struct pcib_mbus_softc *sc)
{
	device_t child;
	int error;

	/* Configure CPU decoding windows */
	error = decode_win_cpu_set(sc->sc_info->op_io_win_target,
	    sc->sc_info->op_io_win_attr, sc->sc_info->op_io_base,
	    sc->sc_info->op_io_size, -1);
	if (error < 0) {
		device_printf(parent, "Could not set up CPU decode "
		    "window for PCI IO\n");
		return;
	}
	error = decode_win_cpu_set(sc->sc_info->op_mem_win_target,
	    sc->sc_info->op_mem_win_attr, sc->sc_info->op_mem_base,
	    sc->sc_info->op_mem_size, -1);
	if (error < 0) {
		device_printf(parent, "Could not set up CPU decode "
		    "windows for PCI MEM\n");
		return;
	}

	/* Create driver instance */
	child = BUS_ADD_CHILD(parent, 0, driver->name, -1);
	bus_set_resource(child, SYS_RES_MEMORY, 0,
	    sc->sc_info->op_base, sc->sc_info->op_size);
	device_set_softc(child, sc);
}

static void
pcib_mbus_identify(driver_t *driver, device_t parent)
{
	const struct obio_pci *info = mv_pci_info;
	struct pcib_mbus_softc *sc;
	uint32_t control;

	while (info->op_base) {
		sc = malloc(driver->size, M_DEVBUF, M_NOWAIT | M_ZERO);
		if (sc == NULL) {
			device_printf(parent, "Could not allocate pcib "
			    "memory\n");
			break;
		}
		sc->sc_info = info++;

		/*
		 * PCI bridge objects are instantiated immediately. PCI-Express
		 * bridges require more complicated handling depending on
		 * platform configuration.
		 */
		if (sc->sc_info->op_type == MV_TYPE_PCI) {
			pcib_mbus_add_child(driver, parent, sc);
			continue;
		}

		/*
		 * Read link configuration
		 */
		sc->sc_rid = 0;
		sc->sc_res = BUS_ALLOC_RESOURCE(parent, parent, SYS_RES_MEMORY,
		    &sc->sc_rid, sc->sc_info->op_base, sc->sc_info->op_base +
		    sc->sc_info->op_size - 1, sc->sc_info->op_size,
		    RF_ACTIVE);
		if (sc->sc_res == NULL) {
			device_printf(parent, "Could not map pcib memory\n");
			break;
		}

		sc->sc_bst = rman_get_bustag(sc->sc_res);
		sc->sc_bsh = rman_get_bushandle(sc->sc_res);

		control = bus_space_read_4(sc->sc_bst, sc->sc_bsh,
		    PCIE_REG_CONTROL);

		BUS_RELEASE_RESOURCE(parent, parent, SYS_RES_MEMORY, sc->sc_rid,
		    sc->sc_res);

		/*
		 * If this PCI-E port (controller) is configured (by the
		 * underlying firmware) with lane width other than 1x, there
		 * are auxiliary resources defined for aggregating more width
		 * on our lane. Skip all such entries as they are not
		 * standalone ports and must not have a device object
		 * instantiated.
		 */
		if ((control & PCIE_CTRL_LINK1X) == 0)
			while (info->op_base &&
			    info->op_type == MV_TYPE_PCIE_AGGR_LANE)
				info++;

		pcib_mbus_add_child(driver, parent, sc);
	}
}

static int
pcib_mbus_probe(device_t self)
{
	char buf[128];
	struct pcib_mbus_softc *sc;
	const char *id, *type;
	uint32_t val;
	int rv = ENOENT, bus, dev;

	sc = device_get_softc(self);

	sc->sc_rid = 0;
	sc->sc_res = bus_alloc_resource_any(self, SYS_RES_MEMORY, &sc->sc_rid,
	    RF_ACTIVE);
	if (sc->sc_res == NULL) {
		device_printf(self, "Could not map memory\n");
		return (ENXIO);
	}

	sc->sc_bst = rman_get_bustag(sc->sc_res);
	sc->sc_bsh = rman_get_bushandle(sc->sc_res);

	pcib_mbus_hw_cfginit();

	/* Retrieve configuration of the bridge */
	if (sc->sc_info->op_type == MV_TYPE_PCI) {
		val = bus_space_read_4(sc->sc_bst, sc->sc_bsh,
		    PCI_REG_P2P_CONF);
		bus = sc->sc_busnr = (val & P2P_CONF_BUS_MASK) >>
		    P2P_CONF_BUS_OFFS;
		dev = sc->sc_devnr = (val & P2P_CONF_DEV_MASK) >>
		    P2P_CONF_DEV_OFFS;
	} else {
		val = bus_space_read_4(sc->sc_bst, sc->sc_bsh, PCIE_REG_STATUS);
		bus = sc->sc_busnr = (val & STATUS_BUS_MASK) >> STATUS_BUS_OFFS;
		dev = sc->sc_devnr = (val & STATUS_DEV_MASK) >> STATUS_DEV_OFFS;
	}

	val = pcib_mbus_hw_cfgread(sc, bus, dev, 0, PCIR_VENDOR, 2);
	if (val != PCI_VENDORID_MRVL)
		goto out;

	val = pcib_mbus_hw_cfgread(sc, bus, dev, 0, PCIR_DEVICE, 2);
	switch (val) {
	case 0x5281:
		id = "88F5281";
		break;
	case 0x5182:
		id = "88F5182";
		break;
	case 0x6281:
		id = "88F6281";
		break;
	case 0x6381:
		id = "MV78100";
		break;
	default:
		device_printf(self, "unknown Marvell PCI bridge: %x\n", val);
		goto out;
	}

	type = "PCI";
	val = pcib_mbus_hw_cfgread(sc, bus, dev, 0, PCIR_CAP_PTR, 1);
	while (val != 0) {
		val = pcib_mbus_hw_cfgread(sc, bus, dev, 0, val, 2);
		switch (val & 0xff) {
		case PCIY_PCIX:
			type = "PCI-X";
			break;
		case PCIY_EXPRESS:
			type = "PCI-Express";
			break;
		}
		val = (val >> 8) & 0xff;
	}

	snprintf(buf, sizeof(buf), "Marvell %s %s host controller", id,
	    type);
	device_set_desc_copy(self, buf);
	rv = BUS_PROBE_DEFAULT;
out:
	bus_release_resource(self, SYS_RES_MEMORY, sc->sc_rid, sc->sc_res);
	return (rv);
}

static int
pcib_mbus_attach(device_t self)
{
	struct pcib_mbus_softc *sc;
	uint32_t val;
	int err;

	sc = device_get_softc(self);
	sc->sc_dev = self;

	sc->sc_rid = 0;
	sc->sc_res = bus_alloc_resource_any(self, SYS_RES_MEMORY, &sc->sc_rid,
	    RF_ACTIVE);
	if (sc->sc_res == NULL) {
		device_printf(self, "Could not map memory\n");
		return (ENXIO);
	}
	sc->sc_bst = rman_get_bustag(sc->sc_res);
	sc->sc_bsh = rman_get_bushandle(sc->sc_res);

	/* Enable PCI bridge */
	val = pcib_mbus_hw_cfgread(sc, sc->sc_busnr, sc->sc_devnr, 0,
	    PCIR_COMMAND, 2);
	val |= PCIM_CMD_SERRESPEN | PCIM_CMD_BUSMASTEREN | PCIM_CMD_MEMEN |
	    PCIM_CMD_PORTEN;
	pcib_mbus_hw_cfgwrite(sc, sc->sc_busnr, sc->sc_devnr, 0,
	    PCIR_COMMAND, val, 2);

	sc->sc_iomem_base = sc->sc_info->op_mem_base;
	sc->sc_iomem_size = sc->sc_info->op_mem_size;
	sc->sc_iomem_alloc = sc->sc_info->op_mem_base;

	sc->sc_ioport_base = sc->sc_info->op_io_base;
	sc->sc_ioport_size = sc->sc_info->op_io_size;
	sc->sc_ioport_alloc = sc->sc_info->op_io_base;

	err = pcib_mbus_init(sc, sc->sc_busnr, pcib_mbus_maxslots(sc->sc_dev));
	if (err)
		return(err);

	device_add_child(self, "pci", -1);
	return (bus_generic_attach(self));
}

static int
pcib_mbus_init_bar(struct pcib_mbus_softc *sc, int bus, int slot, int func,
    int barno)
{
	bus_addr_t *allocp, limit;
	uint32_t addr, bar, mask, size;
	int reg, width;

	reg = PCIR_BAR(barno);
	bar = pcib_mbus_read_config(sc->sc_dev, bus, slot, func, reg, 4);
	if (bar == 0)
		return (1);

	/* Calculate BAR size: 64 or 32 bit (in 32-bit units) */
	width = ((bar & 7) == 4) ? 2 : 1;

	pcib_mbus_write_config(sc->sc_dev, bus, slot, func, reg, ~0, 4);
	size = pcib_mbus_read_config(sc->sc_dev, bus, slot, func, reg, 4);

	/* Get BAR type and size */
	if (bar & 1) {
		/* I/O port */
		allocp = &sc->sc_ioport_alloc;
		limit = sc->sc_ioport_base + sc->sc_ioport_size;
		size &= ~0x3;
		if ((size & 0xffff0000) == 0)
			size |= 0xffff0000;
	} else {
		/* Memory */
		allocp = &sc->sc_iomem_alloc;
		limit = sc->sc_iomem_base + sc->sc_iomem_size;
		size &= ~0xF;
	}
	mask = ~size;
	size = mask + 1;

	/* Sanity check (must be a power of 2) */
	if (size & mask)
		return (width);

	addr = (*allocp + mask) & ~mask;
	if ((*allocp = addr + size) >= limit)
		return (-1);

	if (bootverbose)
		printf("PCI %u:%u:%u: reg %x: size=%08x: addr=%08x\n",
		    bus, slot, func, reg, size, addr);

	pcib_mbus_write_config(sc->sc_dev, bus, slot, func, reg, addr, 4);
	if (width == 2)
		pcib_mbus_write_config(sc->sc_dev, bus, slot, func, reg + 4,
		    0, 4);

	return (width);
}

static void
pcib_mbus_init_bridge(struct pcib_mbus_softc *sc, int bus, int slot, int func)
{
	bus_addr_t io_base, mem_base;
	uint32_t io_limit, mem_limit;
	int secbus;

	io_base = sc->sc_info->op_io_base;
	io_limit = io_base + sc->sc_info->op_io_size - 1;
	mem_base = sc->sc_info->op_mem_base;
	mem_limit = mem_base + sc->sc_info->op_mem_size - 1;

	/* Configure I/O decode registers */
	pcib_mbus_write_config(sc->sc_dev, bus, slot, func, PCIR_IOLIMITL_1,
	    io_limit >> 8, 1);
	pcib_mbus_write_config(sc->sc_dev, bus, slot, func, PCIR_IOLIMITH_1,
	    io_limit >> 16, 2);

	/* Configure memory decode registers */
	pcib_mbus_write_config(sc->sc_dev, bus, slot, func, PCIR_MEMBASE_1,
	    mem_base >> 16, 2);
	pcib_mbus_write_config(sc->sc_dev, bus, slot, func, PCIR_MEMLIMIT_1,
	    mem_limit >> 16, 2);

	/* Disable memory prefetch decode */
	pcib_mbus_write_config(sc->sc_dev, bus, slot, func, PCIR_PMBASEL_1,
	    0x10, 2);
	pcib_mbus_write_config(sc->sc_dev, bus, slot, func, PCIR_PMBASEH_1,
	    0x0, 4);
	pcib_mbus_write_config(sc->sc_dev, bus, slot, func, PCIR_PMLIMITL_1,
	    0xF, 2);
	pcib_mbus_write_config(sc->sc_dev, bus, slot, func, PCIR_PMLIMITH_1,
	    0x0, 4);

	secbus = pcib_mbus_read_config(sc->sc_dev, bus, slot, func,
	    PCIR_SECBUS_1, 1);

	/* Configure buses behind the bridge */
	pcib_mbus_init(sc, secbus, PCI_SLOTMAX);
}

static int
pcib_mbus_init_resources(struct pcib_mbus_softc *sc, int bus, int slot,
    int func, int hdrtype)
{
	int maxbar = (hdrtype & PCIM_HDRTYPE) ? 0 : 6;
	int bar = 0, irq, pin, i;

	/* Program the base address registers */
	while (bar < maxbar) {
		i = pcib_mbus_init_bar(sc, bus, slot, func, bar);
		bar += i;
		if (i < 0) {
			device_printf(sc->sc_dev,
			    "PCI IO/Memory space exhausted\n");
			return (ENOMEM);
		}
	}

	/* Perform interrupt routing */
	pin = pcib_mbus_read_config(sc->sc_dev, bus, slot, func,
	    PCIR_INTPIN, 1);

	if (sc->sc_info->op_get_irq != NULL)
		irq = sc->sc_info->op_get_irq(bus, slot, func, pin);
	else
		irq = sc->sc_info->op_irq;

	if (irq >= 0)
		pcib_mbus_write_config(sc->sc_dev, bus, slot, func,
		    PCIR_INTLINE, irq, 1);
	else {
		device_printf(sc->sc_dev, "Missing IRQ routing information "
		    "for PCI device %u:%u:%u\n", bus, slot, func);
		return (ENXIO);
	}

	return (0);
}

static int
pcib_mbus_init(struct pcib_mbus_softc *sc, int bus, int maxslot)
{
	int slot, func, maxfunc, error;
	uint8_t hdrtype, command, class, subclass;

	for (slot = 0; slot <= maxslot; slot++) {
		maxfunc = 0;
		for (func = 0; func <= maxfunc; func++) {
			hdrtype = pcib_mbus_read_config(sc->sc_dev, bus, slot,
			    func, PCIR_HDRTYPE, 1);

			if ((hdrtype & PCIM_HDRTYPE) > PCI_MAXHDRTYPE)
				continue;

			if (func == 0 && (hdrtype & PCIM_MFDEV))
				maxfunc = PCI_FUNCMAX;

			command = pcib_mbus_read_config(sc->sc_dev, bus, slot,
			    func, PCIR_COMMAND, 1);
			command &= ~(PCIM_CMD_MEMEN | PCIM_CMD_PORTEN);
			pcib_mbus_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_COMMAND, command, 1);

			error = pcib_mbus_init_resources(sc, bus, slot, func,
			    hdrtype);

			if (error)
				return (error);

			command |= PCIM_CMD_BUSMASTEREN | PCIM_CMD_MEMEN |
			    PCIM_CMD_PORTEN;
			pcib_mbus_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_COMMAND, command, 1);

			/* Handle PCI-PCI bridges */
			class = pcib_mbus_read_config(sc->sc_dev, bus, slot,
			    func, PCIR_CLASS, 1);
			subclass = pcib_mbus_read_config(sc->sc_dev, bus, slot,
			    func, PCIR_SUBCLASS, 1);

			if (class != PCIC_BRIDGE ||
			    subclass != PCIS_BRIDGE_PCI)
				continue;

			pcib_mbus_init_bridge(sc, bus, slot, func);
		}
	}

	/* Enable all ABCD interrupts */
	pcib_write_irq_mask(sc, (0xF << 24));

	return (0);
}

static struct resource *
pcib_mbus_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{

	return (BUS_ALLOC_RESOURCE(device_get_parent(dev), child,
	    type, rid, start, end, count, flags));
}

static int
pcib_mbus_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *res)
{

	return (BUS_RELEASE_RESOURCE(device_get_parent(dev), child,
	    type, rid, res));
}

static int
pcib_mbus_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct pcib_mbus_softc *sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_BUS:
		*result = sc->sc_busnr;
		return (0);
	case PCIB_IVAR_DOMAIN:
		*result = device_get_unit(dev);
		return (0);
	}

	return (ENOENT);
}

static int
pcib_mbus_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct pcib_mbus_softc *sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_BUS:
		sc->sc_busnr = value;
		return (0);
	}

	return (ENOENT);
}
