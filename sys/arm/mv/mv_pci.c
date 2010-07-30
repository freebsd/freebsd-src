/*-
 * Copyright (c) 2008 MARVELL INTERNATIONAL LTD.
 * Copyright (c) 2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * Developed by Semihalf.
 *
 * Portions of this software were developed by Semihalf
 * under sponsorship from the FreeBSD Foundation.
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

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>

#include "ofw_bus_if.h"
#include "pcib_if.h"

#include <machine/resource.h>
#include <machine/bus.h>

#include <arm/mv/mvreg.h>
#include <arm/mv/mvvar.h>
#include <arm/mv/mvwin.h>

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

#define STATUS_LINK_DOWN	1
#define STATUS_BUS_OFFS		8
#define STATUS_BUS_MASK		(0xFF << STATUS_BUS_OFFS)
#define STATUS_DEV_OFFS		16
#define STATUS_DEV_MASK		(0x1F << STATUS_DEV_OFFS)

#define P2P_CONF_BUS_OFFS	16
#define P2P_CONF_BUS_MASK	(0xFF << P2P_CONF_BUS_OFFS)
#define P2P_CONF_DEV_OFFS	24
#define P2P_CONF_DEV_MASK	(0x1F << P2P_CONF_DEV_OFFS)

#define PCI_VENDORID_MRVL	0x11AB

struct mv_pcib_softc {
	device_t	sc_dev;

	struct rman	sc_mem_rman;
	bus_addr_t	sc_mem_base;
	bus_addr_t	sc_mem_size;
	bus_addr_t	sc_mem_alloc;		/* Next allocation. */
	int		sc_mem_win_target;
	int		sc_mem_win_attr;

	struct rman	sc_io_rman;
	bus_addr_t	sc_io_base;
	bus_addr_t	sc_io_size;
	bus_addr_t	sc_io_alloc;		/* Next allocation. */
	int		sc_io_win_target;
	int		sc_io_win_attr;

	struct resource	*sc_res;
	bus_space_handle_t sc_bsh;
	bus_space_tag_t	sc_bst;
	int		sc_rid;

	int		sc_busnr;		/* Host bridge bus number */
	int		sc_devnr;		/* Host bridge device number */
	int		sc_type;

	struct fdt_pci_intr	sc_intr_info;
};

/* Local forward prototypes */
static int mv_pcib_decode_win(phandle_t, struct mv_pcib_softc *);
static void mv_pcib_hw_cfginit(void);
static uint32_t mv_pcib_hw_cfgread(struct mv_pcib_softc *, u_int, u_int,
    u_int, u_int, int);
static void mv_pcib_hw_cfgwrite(struct mv_pcib_softc *, u_int, u_int,
    u_int, u_int, uint32_t, int);
static int mv_pcib_init(struct mv_pcib_softc *, int, int);
static int mv_pcib_init_all_bars(struct mv_pcib_softc *, int, int, int, int);
static void mv_pcib_init_bridge(struct mv_pcib_softc *, int, int, int);
static int mv_pcib_intr_info(phandle_t, struct mv_pcib_softc *);
static inline void pcib_write_irq_mask(struct mv_pcib_softc *, uint32_t);


/* Forward prototypes */
static int mv_pcib_probe(device_t);
static int mv_pcib_attach(device_t);

static struct resource *mv_pcib_alloc_resource(device_t, device_t, int, int *,
    u_long, u_long, u_long, u_int);
static int mv_pcib_release_resource(device_t, device_t, int, int,
    struct resource *);
static int mv_pcib_read_ivar(device_t, device_t, int, uintptr_t *);
static int mv_pcib_write_ivar(device_t, device_t, int, uintptr_t);

static int mv_pcib_maxslots(device_t);
static uint32_t mv_pcib_read_config(device_t, u_int, u_int, u_int, u_int, int);
static void mv_pcib_write_config(device_t, u_int, u_int, u_int, u_int,
    uint32_t, int);
static int mv_pcib_route_interrupt(device_t, device_t, int);

/*
 * Bus interface definitions.
 */
static device_method_t mv_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			mv_pcib_probe),
	DEVMETHOD(device_attach,		mv_pcib_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,		bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,		mv_pcib_read_ivar),
	DEVMETHOD(bus_write_ivar,		mv_pcib_write_ivar),
	DEVMETHOD(bus_alloc_resource,		mv_pcib_alloc_resource),
	DEVMETHOD(bus_release_resource,		mv_pcib_release_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,		bus_generic_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,		mv_pcib_maxslots),
	DEVMETHOD(pcib_read_config,		mv_pcib_read_config),
	DEVMETHOD(pcib_write_config,		mv_pcib_write_config),
	DEVMETHOD(pcib_route_interrupt,		mv_pcib_route_interrupt),

	/* OFW bus interface */
	DEVMETHOD(ofw_bus_get_compat,   ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,    ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,     ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,     ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,     ofw_bus_gen_get_type),

	{ 0, 0 }
};

static driver_t mv_pcib_driver = {
	"pcib",
	mv_pcib_methods,
	sizeof(struct mv_pcib_softc),
};

devclass_t pcib_devclass;

DRIVER_MODULE(pcib, fdtbus, mv_pcib_driver, pcib_devclass, 0, 0);

static struct mtx pcicfg_mtx;

static int
mv_pcib_probe(device_t self)
{
	phandle_t parnode;

	/*
	 * The PCI subnode does not have the 'compatible' property, so we need
	 * to check in the parent PCI node. However the parent is not
	 * represented by a separate ofw_bus child, and therefore
	 * ofw_bus_is_compatible() cannot be used, but direct fdt equivalent.
	 */
	parnode = OF_parent(ofw_bus_get_node(self));
	if (parnode == 0)
		return (ENXIO);
	if (!(fdt_is_compatible(parnode, "mrvl,pcie") ||
	    fdt_is_compatible(parnode, "mrvl,pci")))
		return (ENXIO);

	device_set_desc(self, "Marvell Integrated PCI/PCI-E Controller");

	return (BUS_PROBE_DEFAULT);
}

static int
mv_pcib_attach(device_t self)
{
	struct mv_pcib_softc *sc;
	phandle_t node, parnode;
	uint32_t val;
	int err;

	sc = device_get_softc(self);
	sc->sc_dev = self;

	parnode = OF_parent(ofw_bus_get_node(self));
	if (fdt_is_compatible(parnode, "mrvl,pcie")) {
		sc->sc_type = MV_TYPE_PCIE;
		sc->sc_mem_win_target = MV_WIN_PCIE_MEM_TARGET;
		sc->sc_mem_win_attr = MV_WIN_PCIE_MEM_ATTR;
		sc->sc_io_win_target = MV_WIN_PCIE_IO_TARGET;
		sc->sc_io_win_attr = MV_WIN_PCIE_IO_ATTR;
#ifdef SOC_MV_ORION
	} else if (fdt_is_compatible(parnode, "mrvl,pci")) {
		sc->sc_type = MV_TYPE_PCI;
		sc->sc_mem_win_target = MV_WIN_PCI_MEM_TARGET;
		sc->sc_mem_win_attr = MV_WIN_PCI_MEM_ATTR;
		sc->sc_io_win_target = MV_WIN_PCI_IO_TARGET;
		sc->sc_io_win_attr = MV_WIN_PCI_IO_ATTR;
#endif
	} else
		return (ENXIO);

	node = ofw_bus_get_node(self);

	/*
	 * Get PCI interrupt info.
	 */
	if (mv_pcib_intr_info(node, sc) != 0) {
		device_printf(self, "could not retrieve interrupt info\n");
		return (ENXIO);
	}

	/*
	 * Retrieve our mem-mapped registers range.
	 */
	sc->sc_rid = 0;
	sc->sc_res = bus_alloc_resource_any(self, SYS_RES_MEMORY, &sc->sc_rid,
	    RF_ACTIVE);
	if (sc->sc_res == NULL) {
		device_printf(self, "could not map memory\n");
		return (ENXIO);
	}
	sc->sc_bst = rman_get_bustag(sc->sc_res);
	sc->sc_bsh = rman_get_bushandle(sc->sc_res);

	/*
	 * Configure decode windows for PCI(E) access.
	 */
	if (mv_pcib_decode_win(node, sc) != 0)
		return (ENXIO);

	mv_pcib_hw_cfginit();

	/*
	 * Enable PCI bridge.
	 */
	val = mv_pcib_hw_cfgread(sc, sc->sc_busnr, sc->sc_devnr, 0,
	    PCIR_COMMAND, 2);
	val |= PCIM_CMD_SERRESPEN | PCIM_CMD_BUSMASTEREN | PCIM_CMD_MEMEN |
	    PCIM_CMD_PORTEN;
	mv_pcib_hw_cfgwrite(sc, sc->sc_busnr, sc->sc_devnr, 0,
	    PCIR_COMMAND, val, 2);

	sc->sc_mem_alloc = sc->sc_mem_base;
	sc->sc_io_alloc = sc->sc_io_base;

	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	err = rman_init(&sc->sc_mem_rman);
	if (err)
		return (err);

	sc->sc_io_rman.rm_type = RMAN_ARRAY;
	err = rman_init(&sc->sc_io_rman);
	if (err) {
		rman_fini(&sc->sc_mem_rman);
		return (err);
	}

	err = rman_manage_region(&sc->sc_mem_rman, sc->sc_mem_base,
	    sc->sc_mem_base + sc->sc_mem_size - 1);
	if (err)
		goto error;

	err = rman_manage_region(&sc->sc_io_rman, sc->sc_io_base,
	    sc->sc_io_base + sc->sc_io_size - 1);
	if (err)
		goto error;

	err = mv_pcib_init(sc, sc->sc_busnr, mv_pcib_maxslots(sc->sc_dev));
	if (err)
		goto error;

	device_add_child(self, "pci", -1);
	return (bus_generic_attach(self));

error:
	/* XXX SYS_RES_ should be released here */
	rman_fini(&sc->sc_mem_rman);
	rman_fini(&sc->sc_io_rman);
	return (err);
}

static int
mv_pcib_init_bar(struct mv_pcib_softc *sc, int bus, int slot, int func,
    int barno)
{
	bus_addr_t *allocp, limit;
	uint32_t addr, bar, mask, size;
	int reg, width;

	reg = PCIR_BAR(barno);
	bar = mv_pcib_read_config(sc->sc_dev, bus, slot, func, reg, 4);
	if (bar == 0)
		return (1);

	/* Calculate BAR size: 64 or 32 bit (in 32-bit units) */
	width = ((bar & 7) == 4) ? 2 : 1;

	mv_pcib_write_config(sc->sc_dev, bus, slot, func, reg, ~0, 4);
	size = mv_pcib_read_config(sc->sc_dev, bus, slot, func, reg, 4);

	/* Get BAR type and size */
	if (bar & 1) {
		/* I/O port */
		allocp = &sc->sc_io_alloc;
		limit = sc->sc_io_base + sc->sc_io_size;
		size &= ~0x3;
		if ((size & 0xffff0000) == 0)
			size |= 0xffff0000;
	} else {
		/* Memory */
		allocp = &sc->sc_mem_alloc;
		limit = sc->sc_mem_base + sc->sc_mem_size;
		size &= ~0xF;
	}
	mask = ~size;
	size = mask + 1;

	/* Sanity check (must be a power of 2) */
	if (size & mask)
		return (width);

	addr = (*allocp + mask) & ~mask;
	if ((*allocp = addr + size) > limit)
		return (-1);

	if (bootverbose)
		printf("PCI %u:%u:%u: reg %x: size=%08x: addr=%08x\n",
		    bus, slot, func, reg, size, addr);

	mv_pcib_write_config(sc->sc_dev, bus, slot, func, reg, addr, 4);
	if (width == 2)
		mv_pcib_write_config(sc->sc_dev, bus, slot, func, reg + 4,
		    0, 4);

	return (width);
}

static void
mv_pcib_init_bridge(struct mv_pcib_softc *sc, int bus, int slot, int func)
{
	bus_addr_t io_base, mem_base;
	uint32_t io_limit, mem_limit;
	int secbus;

	io_base = sc->sc_io_base;
	io_limit = io_base + sc->sc_io_size - 1;
	mem_base = sc->sc_mem_base;
	mem_limit = mem_base + sc->sc_mem_size - 1;

	/* Configure I/O decode registers */
	mv_pcib_write_config(sc->sc_dev, bus, slot, func, PCIR_IOBASEL_1,
	    io_base >> 8, 1);
	mv_pcib_write_config(sc->sc_dev, bus, slot, func, PCIR_IOBASEH_1,
	    io_base >> 16, 2);
	mv_pcib_write_config(sc->sc_dev, bus, slot, func, PCIR_IOLIMITL_1,
	    io_limit >> 8, 1);
	mv_pcib_write_config(sc->sc_dev, bus, slot, func, PCIR_IOLIMITH_1,
	    io_limit >> 16, 2);

	/* Configure memory decode registers */
	mv_pcib_write_config(sc->sc_dev, bus, slot, func, PCIR_MEMBASE_1,
	    mem_base >> 16, 2);
	mv_pcib_write_config(sc->sc_dev, bus, slot, func, PCIR_MEMLIMIT_1,
	    mem_limit >> 16, 2);

	/* Disable memory prefetch decode */
	mv_pcib_write_config(sc->sc_dev, bus, slot, func, PCIR_PMBASEL_1,
	    0x10, 2);
	mv_pcib_write_config(sc->sc_dev, bus, slot, func, PCIR_PMBASEH_1,
	    0x0, 4);
	mv_pcib_write_config(sc->sc_dev, bus, slot, func, PCIR_PMLIMITL_1,
	    0xF, 2);
	mv_pcib_write_config(sc->sc_dev, bus, slot, func, PCIR_PMLIMITH_1,
	    0x0, 4);

	secbus = mv_pcib_read_config(sc->sc_dev, bus, slot, func,
	    PCIR_SECBUS_1, 1);

	/* Configure buses behind the bridge */
	mv_pcib_init(sc, secbus, PCI_SLOTMAX);
}

static int
mv_pcib_init(struct mv_pcib_softc *sc, int bus, int maxslot)
{
	int slot, func, maxfunc, error;
	uint8_t hdrtype, command, class, subclass;

	for (slot = 0; slot <= maxslot; slot++) {
		maxfunc = 0;
		for (func = 0; func <= maxfunc; func++) {
			hdrtype = mv_pcib_read_config(sc->sc_dev, bus, slot,
			    func, PCIR_HDRTYPE, 1);

			if ((hdrtype & PCIM_HDRTYPE) > PCI_MAXHDRTYPE)
				continue;

			if (func == 0 && (hdrtype & PCIM_MFDEV))
				maxfunc = PCI_FUNCMAX;

			command = mv_pcib_read_config(sc->sc_dev, bus, slot,
			    func, PCIR_COMMAND, 1);
			command &= ~(PCIM_CMD_MEMEN | PCIM_CMD_PORTEN);
			mv_pcib_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_COMMAND, command, 1);

			error = mv_pcib_init_all_bars(sc, bus, slot, func,
			    hdrtype);

			if (error)
				return (error);

			command |= PCIM_CMD_BUSMASTEREN | PCIM_CMD_MEMEN |
			    PCIM_CMD_PORTEN;
			mv_pcib_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_COMMAND, command, 1);

			/* Handle PCI-PCI bridges */
			class = mv_pcib_read_config(sc->sc_dev, bus, slot,
			    func, PCIR_CLASS, 1);
			subclass = mv_pcib_read_config(sc->sc_dev, bus, slot,
			    func, PCIR_SUBCLASS, 1);

			if (class != PCIC_BRIDGE ||
			    subclass != PCIS_BRIDGE_PCI)
				continue;

			mv_pcib_init_bridge(sc, bus, slot, func);
		}
	}

	/* Enable all ABCD interrupts */
	pcib_write_irq_mask(sc, (0xF << 24));

	return (0);
}

static int
mv_pcib_init_all_bars(struct mv_pcib_softc *sc, int bus, int slot,
    int func, int hdrtype)
{
	int maxbar, bar, i;

	maxbar = (hdrtype & PCIM_HDRTYPE) ? 0 : 6;
	bar = 0;

	/* Program the base address registers */
	while (bar < maxbar) {
		i = mv_pcib_init_bar(sc, bus, slot, func, bar);
		bar += i;
		if (i < 0) {
			device_printf(sc->sc_dev,
			    "PCI IO/Memory space exhausted\n");
			return (ENOMEM);
		}
	}

	return (0);
}

static struct resource *
mv_pcib_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct mv_pcib_softc *sc = device_get_softc(dev);
	struct rman *rm = NULL;
	struct resource *res;

	switch (type) {
	case SYS_RES_IOPORT:
		rm = &sc->sc_io_rman;
		break;
	case SYS_RES_MEMORY:
		rm = &sc->sc_mem_rman;
		break;
	default:
		return (BUS_ALLOC_RESOURCE(device_get_parent(dev), child,
		    type, rid, start, end, count, flags));
	};

	res = rman_reserve_resource(rm, start, end, count, flags, child);
	if (res == NULL)
		return (NULL);

	rman_set_rid(res, *rid);
	rman_set_bustag(res, fdtbus_bs_tag);
	rman_set_bushandle(res, start);

	if (flags & RF_ACTIVE)
		if (bus_activate_resource(child, type, *rid, res)) {
			rman_release_resource(res);
			return (NULL);
		}

	return (res);
}

static int
mv_pcib_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *res)
{

	if (type != SYS_RES_IOPORT && type != SYS_RES_MEMORY)
		return (BUS_RELEASE_RESOURCE(device_get_parent(dev), child,
		    type, rid, res));

	return (rman_release_resource(res));
}

static int
mv_pcib_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct mv_pcib_softc *sc = device_get_softc(dev);

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
mv_pcib_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct mv_pcib_softc *sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_BUS:
		sc->sc_busnr = value;
		return (0);
	}

	return (ENOENT);
}

static inline void
pcib_write_irq_mask(struct mv_pcib_softc *sc, uint32_t mask)
{

	if (!sc->sc_type != MV_TYPE_PCI)
		return;

	bus_space_write_4(sc->sc_bst, sc->sc_bsh, PCIE_REG_IRQ_MASK, mask);
}

static void
mv_pcib_hw_cfginit(void)
{
	static int opened = 0;

	if (opened)
		return;

	mtx_init(&pcicfg_mtx, "pcicfg", NULL, MTX_SPIN);
	opened = 1;
}

static uint32_t
mv_pcib_hw_cfgread(struct mv_pcib_softc *sc, u_int bus, u_int slot,
    u_int func, u_int reg, int bytes)
{
	uint32_t addr, data, ca, cd;

	ca = (sc->sc_type != MV_TYPE_PCI) ?
	    PCIE_REG_CFG_ADDR : PCI_REG_CFG_ADDR;
	cd = (sc->sc_type != MV_TYPE_PCI) ?
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
mv_pcib_hw_cfgwrite(struct mv_pcib_softc *sc, u_int bus, u_int slot,
    u_int func, u_int reg, uint32_t data, int bytes)
{
	uint32_t addr, ca, cd;

	ca = (sc->sc_type != MV_TYPE_PCI) ?
	    PCIE_REG_CFG_ADDR : PCI_REG_CFG_ADDR;
	cd = (sc->sc_type != MV_TYPE_PCI) ?
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
mv_pcib_maxslots(device_t dev)
{
	struct mv_pcib_softc *sc = device_get_softc(dev);

	return ((sc->sc_type != MV_TYPE_PCI) ? 1 : PCI_SLOTMAX);
}

static uint32_t
mv_pcib_read_config(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, int bytes)
{
	struct mv_pcib_softc *sc = device_get_softc(dev);

	/* Skip self */
	if (bus == sc->sc_busnr && slot == sc->sc_devnr)
		return (~0U);

	return (mv_pcib_hw_cfgread(sc, bus, slot, func, reg, bytes));
}

static void
mv_pcib_write_config(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, uint32_t val, int bytes)
{
	struct mv_pcib_softc *sc = device_get_softc(dev);

	/* Skip self */
	if (bus == sc->sc_busnr && slot == sc->sc_devnr)
		return;

	mv_pcib_hw_cfgwrite(sc, bus, slot, func, reg, val, bytes);
}

static int
mv_pcib_route_interrupt(device_t pcib, device_t dev, int pin)
{
	struct mv_pcib_softc *sc;
	int err, interrupt;

	sc = device_get_softc(pcib);

	err = fdt_pci_route_intr(pci_get_bus(dev), pci_get_slot(dev),
	    pci_get_function(dev), pin, &sc->sc_intr_info, &interrupt);
	if (err == 0)
		return (interrupt);

	device_printf(pcib, "could not route pin %d for device %d.%d\n",
	    pin, pci_get_slot(dev), pci_get_function(dev));
	return (PCI_INVALID_IRQ);
}

static int
mv_pcib_decode_win(phandle_t node, struct mv_pcib_softc *sc)
{
	struct fdt_pci_range io_space, mem_space;
	device_t dev;
	int error;

	dev = sc->sc_dev;

	if ((error = fdt_pci_ranges(node, &io_space, &mem_space)) != 0) {
		device_printf(dev, "could not retrieve 'ranges' data\n");
		return (error);
	}

	/* Configure CPU decoding windows */
	error = decode_win_cpu_set(sc->sc_io_win_target,
	    sc->sc_io_win_attr, io_space.base_parent, io_space.len, -1);
	if (error < 0) {
		device_printf(dev, "could not set up CPU decode "
		    "window for PCI IO\n");
		return (ENXIO);
	}
	error = decode_win_cpu_set(sc->sc_mem_win_target,
	    sc->sc_mem_win_attr, mem_space.base_parent, mem_space.len, -1);
	if (error < 0) {
		device_printf(dev, "could not set up CPU decode "
		    "windows for PCI MEM\n");
		return (ENXIO);
	}

	sc->sc_io_base = io_space.base_parent;
	sc->sc_io_size = io_space.len;

	sc->sc_mem_base = mem_space.base_parent;
	sc->sc_mem_size = mem_space.len;

	return (0);
}

static int
mv_pcib_intr_info(phandle_t node, struct mv_pcib_softc *sc)
{
	int error;

	if ((error = fdt_pci_intr_info(node, &sc->sc_intr_info)) != 0)
		return (error);

	return (0);
}

#if 0
		control = bus_space_read_4(sc->sc_bst, sc->sc_bsh,
		    PCIE_REG_CONTROL);

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

		mv_pcib_add_child(driver, parent, sc);
#endif
