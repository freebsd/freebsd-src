/*-
 * Copyright 2006-2007 by Juniper Networks.
 * Copyright 2008 Semihalf.
 * Copyright 2010 The FreeBSD Foundation
 * All rights reserved.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * From: FreeBSD: src/sys/powerpc/mpc85xx/pci_ocp.c,v 1.9 2010/03/23 23:46:28 marcel
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
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
#include <machine/intr_machdep.h>

#include <powerpc/mpc85xx/mpc85xx.h>

#define	REG_CFG_ADDR	0x0000
#define	CONFIG_ACCESS_ENABLE	0x80000000

#define	REG_CFG_DATA	0x0004
#define	REG_INT_ACK	0x0008

#define	REG_POTAR(n)	(0x0c00 + 0x20 * (n))
#define	REG_POTEAR(n)	(0x0c04 + 0x20 * (n))
#define	REG_POWBAR(n)	(0x0c08 + 0x20 * (n))
#define	REG_POWAR(n)	(0x0c10 + 0x20 * (n))

#define	REG_PITAR(n)	(0x0e00 - 0x20 * (n))
#define	REG_PIWBAR(n)	(0x0e08 - 0x20 * (n))
#define	REG_PIWBEAR(n)	(0x0e0c - 0x20 * (n))
#define	REG_PIWAR(n)	(0x0e10 - 0x20 * (n))

#define	REG_PEX_MES_DR	0x0020
#define	REG_PEX_MES_IER	0x0028
#define	REG_PEX_ERR_DR	0x0e00
#define	REG_PEX_ERR_EN	0x0e08

#define PCIR_LTSSM	0x404
#define LTSSM_STAT_L0	0x16

#define	DEVFN(b, s, f)	((b << 16) | (s << 8) | f)

struct fsl_pcib_softc {
	device_t	sc_dev;

	struct rman	sc_iomem;
	bus_addr_t	sc_iomem_va;		/* Virtual mapping. */
	bus_addr_t	sc_iomem_size;
	bus_addr_t	sc_iomem_alloc;		/* Next allocation. */
	int		sc_iomem_target;
	struct rman	sc_ioport;
	bus_addr_t	sc_ioport_va;		/* Virtual mapping. */
	bus_addr_t	sc_ioport_size;
	bus_addr_t	sc_ioport_alloc;	/* Next allocation. */
	int		sc_ioport_target;

	struct resource	*sc_res;
	bus_space_handle_t sc_bsh;
	bus_space_tag_t	sc_bst;
	int		sc_rid;

	int		sc_busnr;
	int		sc_pcie;
	uint8_t		sc_pcie_capreg;		/* PCI-E Capability Reg Set */

	/* Devices that need special attention. */
	int		sc_devfn_tundra;
	int		sc_devfn_via_ide;

	struct fdt_pci_intr	sc_intr_info;
};

/* Local forward declerations. */
static uint32_t fsl_pcib_cfgread(struct fsl_pcib_softc *, u_int, u_int, u_int,
    u_int, int);
static void fsl_pcib_cfgwrite(struct fsl_pcib_softc *, u_int, u_int, u_int,
    u_int, uint32_t, int);
static int fsl_pcib_decode_win(phandle_t, struct fsl_pcib_softc *);
static void fsl_pcib_err_init(device_t);
static void fsl_pcib_inbound(struct fsl_pcib_softc *, int, int, u_long,
    u_long, u_long);
static int fsl_pcib_init(struct fsl_pcib_softc *, int, int);
static int fsl_pcib_intr_info(phandle_t, struct fsl_pcib_softc *);
static int fsl_pcib_set_range(struct fsl_pcib_softc *, int, int, u_long,
    u_long);
static void fsl_pcib_outbound(struct fsl_pcib_softc *, int, int, u_long,
    u_long, u_long);

/* Forward declerations. */
static int fsl_pcib_attach(device_t);
static int fsl_pcib_detach(device_t);
static int fsl_pcib_probe(device_t);

static struct resource *fsl_pcib_alloc_resource(device_t, device_t, int, int *,
    u_long, u_long, u_long, u_int);
static int fsl_pcib_read_ivar(device_t, device_t, int, uintptr_t *);
static int fsl_pcib_release_resource(device_t, device_t, int, int,
    struct resource *);
static int fsl_pcib_write_ivar(device_t, device_t, int, uintptr_t);

static int fsl_pcib_maxslots(device_t);
static uint32_t fsl_pcib_read_config(device_t, u_int, u_int, u_int, u_int, int);
static void fsl_pcib_write_config(device_t, u_int, u_int, u_int, u_int,
    uint32_t, int);

/* Configuration r/w mutex. */
struct mtx pcicfg_mtx;
static int mtx_initialized = 0;

/*
 * Bus interface definitions.
 */
static device_method_t fsl_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fsl_pcib_probe),
	DEVMETHOD(device_attach,	fsl_pcib_attach),
	DEVMETHOD(device_detach,	fsl_pcib_detach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	fsl_pcib_read_ivar),
	DEVMETHOD(bus_write_ivar,	fsl_pcib_write_ivar),
	DEVMETHOD(bus_alloc_resource,	fsl_pcib_alloc_resource),
	DEVMETHOD(bus_release_resource,	fsl_pcib_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	fsl_pcib_maxslots),
	DEVMETHOD(pcib_read_config,	fsl_pcib_read_config),
	DEVMETHOD(pcib_write_config,	fsl_pcib_write_config),
	DEVMETHOD(pcib_route_interrupt,	pcib_route_interrupt),

	/* OFW bus interface */
	DEVMETHOD(ofw_bus_get_compat,   ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,    ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,     ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,     ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,     ofw_bus_gen_get_type),

	{ 0, 0 }
};

static driver_t fsl_pcib_driver = {
	"pcib",
	fsl_pcib_methods,
	sizeof(struct fsl_pcib_softc),
};

devclass_t pcib_devclass;

DRIVER_MODULE(pcib, fdtbus, fsl_pcib_driver, pcib_devclass, 0, 0);

static int
fsl_pcib_probe(device_t dev)
{
	phandle_t node;

	node = ofw_bus_get_node(dev);
	if (!fdt_is_type(node, "pci"))
		return (ENXIO);

	if (!(fdt_is_compatible(node, "fsl,mpc8540-pci") ||
	    fdt_is_compatible(node, "fsl,mpc8548-pcie")))
		return (ENXIO);

	device_set_desc(dev, "Freescale Integrated PCI/PCI-E Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
fsl_pcib_attach(device_t dev)
{
	struct fsl_pcib_softc *sc;
	phandle_t node;
	uint32_t cfgreg;
	int maxslot;
	uint8_t ltssm, capptr;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	sc->sc_rid = 0;
	sc->sc_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->sc_rid,
	    RF_ACTIVE);
	if (sc->sc_res == NULL) {
		device_printf(dev, "could not map I/O memory\n");
		return (ENXIO);
	}
	sc->sc_bst = rman_get_bustag(sc->sc_res);
	sc->sc_bsh = rman_get_bushandle(sc->sc_res);
	sc->sc_busnr = 0;

	if (!mtx_initialized) {
		mtx_init(&pcicfg_mtx, "pcicfg", NULL, MTX_SPIN);
		mtx_initialized = 1;
	}

	cfgreg = fsl_pcib_cfgread(sc, 0, 0, 0, PCIR_VENDOR, 2);
	if (cfgreg != 0x1057 && cfgreg != 0x1957)
		goto err;

	capptr = fsl_pcib_cfgread(sc, 0, 0, 0, PCIR_CAP_PTR, 1);
	while (capptr != 0) {
		cfgreg = fsl_pcib_cfgread(sc, 0, 0, 0, capptr, 2);
		switch (cfgreg & 0xff) {
		case PCIY_PCIX:
			break;
		case PCIY_EXPRESS:
			sc->sc_pcie = 1;
			sc->sc_pcie_capreg = capptr;
			break;
		}
		capptr = (cfgreg >> 8) & 0xff;
	}

	node = ofw_bus_get_node(dev);
	/*
	 * Get PCI interrupt info.
	 */
	if (fsl_pcib_intr_info(node, sc) != 0) {
		device_printf(dev, "could not retrieve interrupt info\n");
		goto err;
	}

	/*
	 * Configure decode windows for PCI(E) access.
	 */
	if (fsl_pcib_decode_win(node, sc) != 0)
		goto err;

	cfgreg = fsl_pcib_cfgread(sc, 0, 0, 0, PCIR_COMMAND, 2);
	cfgreg |= PCIM_CMD_SERRESPEN | PCIM_CMD_BUSMASTEREN | PCIM_CMD_MEMEN |
	    PCIM_CMD_PORTEN;
	fsl_pcib_cfgwrite(sc, 0, 0, 0, PCIR_COMMAND, cfgreg, 2);

	sc->sc_devfn_tundra = -1;
	sc->sc_devfn_via_ide = -1;


	/*
	 * Scan bus using firmware configured, 0 based bus numbering.
	 */
	sc->sc_busnr = 0;
	maxslot = (sc->sc_pcie) ? 0 : PCI_SLOTMAX;
	fsl_pcib_init(sc, sc->sc_busnr, maxslot);

	if (sc->sc_pcie) {
		ltssm = fsl_pcib_cfgread(sc, 0, 0, 0, PCIR_LTSSM, 1);
		if (ltssm < LTSSM_STAT_L0) {
			if (bootverbose)
				printf("PCI %d: no PCIE link, skipping\n",
				    device_get_unit(dev));
			return (0);
		}
	}

	fsl_pcib_err_init(dev);

	device_add_child(dev, "pci", -1);
	return (bus_generic_attach(dev));

err:
	bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_rid, sc->sc_res);
	return (ENXIO);
}

static uint32_t
fsl_pcib_cfgread(struct fsl_pcib_softc *sc, u_int bus, u_int slot, u_int func,
    u_int reg, int bytes)
{
	uint32_t addr, data;

	if (bus == sc->sc_busnr - 1)
		bus = 0;

	addr = CONFIG_ACCESS_ENABLE;
	addr |= (bus & 0xff) << 16;
	addr |= (slot & 0x1f) << 11;
	addr |= (func & 0x7) << 8;
	addr |= reg & 0xfc;
	if (sc->sc_pcie)
		addr |= (reg & 0xf00) << 16;

	mtx_lock_spin(&pcicfg_mtx);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, REG_CFG_ADDR, addr);

	switch (bytes) {
	case 1:
		data = bus_space_read_1(sc->sc_bst, sc->sc_bsh,
		    REG_CFG_DATA + (reg & 3));
		break;
	case 2:
		data = le16toh(bus_space_read_2(sc->sc_bst, sc->sc_bsh,
		    REG_CFG_DATA + (reg & 2)));
		break;
	case 4:
		data = le32toh(bus_space_read_4(sc->sc_bst, sc->sc_bsh,
		    REG_CFG_DATA));
		break;
	default:
		data = ~0;
		break;
	}
	mtx_unlock_spin(&pcicfg_mtx);
	return (data);
}

static void
fsl_pcib_cfgwrite(struct fsl_pcib_softc *sc, u_int bus, u_int slot, u_int func,
    u_int reg, uint32_t data, int bytes)
{
	uint32_t addr;

	if (bus == sc->sc_busnr - 1)
		bus = 0;

	addr = CONFIG_ACCESS_ENABLE;
	addr |= (bus & 0xff) << 16;
	addr |= (slot & 0x1f) << 11;
	addr |= (func & 0x7) << 8;
	addr |= reg & 0xfc;
	if (sc->sc_pcie)
		addr |= (reg & 0xf00) << 16;

	mtx_lock_spin(&pcicfg_mtx);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, REG_CFG_ADDR, addr);

	switch (bytes) {
	case 1:
		bus_space_write_1(sc->sc_bst, sc->sc_bsh,
		    REG_CFG_DATA + (reg & 3), data);
		break;
	case 2:
		bus_space_write_2(sc->sc_bst, sc->sc_bsh,
		    REG_CFG_DATA + (reg & 2), htole16(data));
		break;
	case 4:
		bus_space_write_4(sc->sc_bst, sc->sc_bsh,
		    REG_CFG_DATA, htole32(data));
		break;
	}
	mtx_unlock_spin(&pcicfg_mtx);
}

#if 0
static void
dump(struct fsl_pcib_softc *sc)
{
	unsigned int i;

#define RD(o)	bus_space_read_4(sc->sc_bst, sc->sc_bsh, o)
	for (i = 0; i < 5; i++) {
		printf("POTAR%u  =0x%08x\n", i, RD(REG_POTAR(i)));
		printf("POTEAR%u =0x%08x\n", i, RD(REG_POTEAR(i)));
		printf("POWBAR%u =0x%08x\n", i, RD(REG_POWBAR(i)));
		printf("POWAR%u  =0x%08x\n", i, RD(REG_POWAR(i)));
	}
	printf("\n");
	for (i = 1; i < 4; i++) {
		printf("PITAR%u  =0x%08x\n", i, RD(REG_PITAR(i)));
		printf("PIWBAR%u =0x%08x\n", i, RD(REG_PIWBAR(i)));
		printf("PIWBEAR%u=0x%08x\n", i, RD(REG_PIWBEAR(i)));
		printf("PIWAR%u  =0x%08x\n", i, RD(REG_PIWAR(i)));
	}
	printf("\n");
#undef RD

	for (i = 0; i < 0x48; i += 4) {
		printf("cfg%02x=0x%08x\n", i, fsl_pcib_cfgread(sc, 0, 0, 0,
		    i, 4));
	}
}
#endif

static int
fsl_pcib_maxslots(device_t dev)
{
	struct fsl_pcib_softc *sc = device_get_softc(dev);

	return ((sc->sc_pcie) ? 0 : PCI_SLOTMAX);
}

static uint32_t
fsl_pcib_read_config(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, int bytes)
{
	struct fsl_pcib_softc *sc = device_get_softc(dev);
	u_int devfn;

	if (bus == sc->sc_busnr && !sc->sc_pcie && slot < 10)
		return (~0);
	devfn = DEVFN(bus, slot, func);
	if (devfn == sc->sc_devfn_tundra)
		return (~0);
	if (devfn == sc->sc_devfn_via_ide && reg == PCIR_INTPIN)
		return (1);
	return (fsl_pcib_cfgread(sc, bus, slot, func, reg, bytes));
}

static void
fsl_pcib_write_config(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, uint32_t val, int bytes)
{
	struct fsl_pcib_softc *sc = device_get_softc(dev);

	if (bus == sc->sc_busnr && !sc->sc_pcie && slot < 10)
		return;
	fsl_pcib_cfgwrite(sc, bus, slot, func, reg, val, bytes);
}

static void
fsl_pcib_init_via(struct fsl_pcib_softc *sc, uint16_t device, int bus,
    int slot, int fn)
{

	if (device == 0x0686) {
		fsl_pcib_write_config(sc->sc_dev, bus, slot, fn, 0x52, 0x34, 1);
		fsl_pcib_write_config(sc->sc_dev, bus, slot, fn, 0x77, 0x00, 1);
		fsl_pcib_write_config(sc->sc_dev, bus, slot, fn, 0x83, 0x98, 1);
		fsl_pcib_write_config(sc->sc_dev, bus, slot, fn, 0x85, 0x03, 1);
	} else if (device == 0x0571) {
		sc->sc_devfn_via_ide = DEVFN(bus, slot, fn);
		fsl_pcib_write_config(sc->sc_dev, bus, slot, fn, 0x40, 0x0b, 1);
	}
}

static int
fsl_pcib_init_bar(struct fsl_pcib_softc *sc, int bus, int slot, int func,
    int barno)
{
	bus_addr_t *allocp;
	uint32_t addr, mask, size;
	int reg, width;

	reg = PCIR_BAR(barno);

	if (DEVFN(bus, slot, func) == sc->sc_devfn_via_ide) {
		switch (barno) {
		case 0:	addr = 0x1f0; break;
		case 1: addr = 0x3f4; break;
		case 2: addr = 0x170; break;
		case 3: addr = 0x374; break;
		case 4: addr = 0xcc0; break;
		default: return (1);
		}
		fsl_pcib_write_config(sc->sc_dev, bus, slot, func, reg, addr, 4);
		return (1);
	}

	fsl_pcib_write_config(sc->sc_dev, bus, slot, func, reg, ~0, 4);
	size = fsl_pcib_read_config(sc->sc_dev, bus, slot, func, reg, 4);
	if (size == 0)
		return (1);
	width = ((size & 7) == 4) ? 2 : 1;

	if (size & 1) {		/* I/O port */
		allocp = &sc->sc_ioport_alloc;
		size &= ~3;
		if ((size & 0xffff0000) == 0)
			size |= 0xffff0000;
	} else {		/* memory */
		allocp = &sc->sc_iomem_alloc;
		size &= ~15;
	}
	mask = ~size;
	size = mask + 1;
	/* Sanity check (must be a power of 2). */
	if (size & mask)
		return (width);

	addr = (*allocp + mask) & ~mask;
	*allocp = addr + size;

	if (bootverbose)
		printf("PCI %u:%u:%u:%u: reg %x: size=%08x: addr=%08x\n",
		    device_get_unit(sc->sc_dev), bus, slot, func, reg,
		    size, addr);

	fsl_pcib_write_config(sc->sc_dev, bus, slot, func, reg, addr, 4);
	if (width == 2)
		fsl_pcib_write_config(sc->sc_dev, bus, slot, func, reg + 4,
		    0, 4);
	return (width);
}

static u_int
fsl_pcib_route_int(struct fsl_pcib_softc *sc, u_int bus, u_int slot, u_int func,
    u_int intpin)
{
	int err, unit;
	u_int devfn, intline;

	unit = device_get_unit(sc->sc_dev);

	devfn = DEVFN(bus, slot, func);
	if (devfn == sc->sc_devfn_via_ide)
		intline = INTR_VEC(0, 14);
	else if (devfn == sc->sc_devfn_via_ide + 1)
		intline = INTR_VEC(0, 10);
	else if (devfn == sc->sc_devfn_via_ide + 2)
		intline = INTR_VEC(0, 10);
	else {
		if (intpin != 0)
			err = fdt_pci_route_intr(bus, slot, func, intpin,
			    &sc->sc_intr_info, &intline);
		else
			intline = 0xff;
	}

	if (bootverbose)
		printf("PCI %u:%u:%u:%u: intpin %u: intline=%u\n",
		    unit, bus, slot, func, intpin, intline);

	return (intline);
}

static int
fsl_pcib_init(struct fsl_pcib_softc *sc, int bus, int maxslot)
{
	int secbus;
	int old_pribus, old_secbus, old_subbus;
	int new_pribus, new_secbus, new_subbus;
	int slot, func, maxfunc;
	int bar, maxbar;
	uint16_t vendor, device;
	uint8_t command, hdrtype, class, subclass;
	uint8_t intline, intpin;

	secbus = bus;
	for (slot = 0; slot <= maxslot; slot++) {
		maxfunc = 0;
		for (func = 0; func <= maxfunc; func++) {
			hdrtype = fsl_pcib_read_config(sc->sc_dev, bus, slot,
			    func, PCIR_HDRTYPE, 1);

			if ((hdrtype & PCIM_HDRTYPE) > PCI_MAXHDRTYPE)
				continue;

			if (func == 0 && (hdrtype & PCIM_MFDEV))
				maxfunc = PCI_FUNCMAX;

			vendor = fsl_pcib_read_config(sc->sc_dev, bus, slot,
			    func, PCIR_VENDOR, 2);
			device = fsl_pcib_read_config(sc->sc_dev, bus, slot,
			    func, PCIR_DEVICE, 2);

			if (vendor == 0x1957 && device == 0x3fff) {
				sc->sc_devfn_tundra = DEVFN(bus, slot, func);
				continue;
			}

			command = fsl_pcib_read_config(sc->sc_dev, bus, slot,
			    func, PCIR_COMMAND, 1);
			command &= ~(PCIM_CMD_MEMEN | PCIM_CMD_PORTEN);
			fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_COMMAND, command, 1);

			if (vendor == 0x1106)
				fsl_pcib_init_via(sc, device, bus, slot, func);

			/* Program the base address registers. */
			maxbar = (hdrtype & PCIM_HDRTYPE) ? 1 : 6;
			bar = 0;
			while (bar < maxbar)
				bar += fsl_pcib_init_bar(sc, bus, slot, func,
				    bar);

			/* Perform interrupt routing. */
			intpin = fsl_pcib_read_config(sc->sc_dev, bus, slot,
			    func, PCIR_INTPIN, 1);
			intline = fsl_pcib_route_int(sc, bus, slot, func,
			    intpin);
			fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_INTLINE, intline, 1);

			command |= PCIM_CMD_MEMEN | PCIM_CMD_PORTEN;
			fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_COMMAND, command, 1);

			/*
			 * Handle PCI-PCI bridges
			 */
			class = fsl_pcib_read_config(sc->sc_dev, bus, slot,
			    func, PCIR_CLASS, 1);
			subclass = fsl_pcib_read_config(sc->sc_dev, bus, slot,
			    func, PCIR_SUBCLASS, 1);

			/* Allow only proper PCI-PCI briges */
			if (class != PCIC_BRIDGE)
				continue;
			if (subclass != PCIS_BRIDGE_PCI)
				continue;

			secbus++;

			/* Program I/O decoder. */
			fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_IOBASEL_1, sc->sc_ioport.rm_start >> 8, 1);
			fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_IOLIMITL_1, sc->sc_ioport.rm_end >> 8, 1);
			fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_IOBASEH_1, sc->sc_ioport.rm_start >> 16, 2);
			fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_IOLIMITH_1, sc->sc_ioport.rm_end >> 16, 2);

			/* Program (non-prefetchable) memory decoder. */
			fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_MEMBASE_1, sc->sc_iomem.rm_start >> 16, 2);
			fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_MEMLIMIT_1, sc->sc_iomem.rm_end >> 16, 2);

			/* Program prefetchable memory decoder. */
			fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_PMBASEL_1, 0x0010, 2);
			fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_PMLIMITL_1, 0x000f, 2);
			fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_PMBASEH_1, 0x00000000, 4);
			fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_PMLIMITH_1, 0x00000000, 4);

			/* Read currect bus register configuration */
			old_pribus = fsl_pcib_read_config(sc->sc_dev, bus,
			    slot, func, PCIR_PRIBUS_1, 1);
			old_secbus = fsl_pcib_read_config(sc->sc_dev, bus,
			    slot, func, PCIR_SECBUS_1, 1);
			old_subbus = fsl_pcib_read_config(sc->sc_dev, bus,
			    slot, func, PCIR_SUBBUS_1, 1);

			if (bootverbose)
				printf("PCI: reading firmware bus numbers for "
				    "secbus = %d (bus/sec/sub) = (%d/%d/%d)\n",
				    secbus, old_pribus, old_secbus, old_subbus);

			new_pribus = bus;
			new_secbus = secbus;

			secbus = fsl_pcib_init(sc, secbus,
			    (subclass == PCIS_BRIDGE_PCI) ? PCI_SLOTMAX : 0);

			new_subbus = secbus;

			if (bootverbose)
				printf("PCI: translate firmware bus numbers "
				    "for secbus %d (%d/%d/%d) -> (%d/%d/%d)\n",
				    secbus, old_pribus, old_secbus, old_subbus,
				    new_pribus, new_secbus, new_subbus);

			fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_PRIBUS_1, new_pribus, 1);
			fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_SECBUS_1, new_secbus, 1);
			fsl_pcib_write_config(sc->sc_dev, bus, slot, func,
			    PCIR_SUBBUS_1, new_subbus, 1);
		}
	}

	return (secbus);
}

static void
fsl_pcib_inbound(struct fsl_pcib_softc *sc, int wnd, int tgt, u_long start,
    u_long size, u_long pci_start)
{
	uint32_t attr, bar, tar;

	KASSERT(wnd > 0, ("%s: inbound window 0 is invalid", __func__));

	switch (tgt) {
	/* XXX OCP85XX_TGTIF_RAM2, OCP85XX_TGTIF_RAM_INTL should be handled */
	case OCP85XX_TGTIF_RAM1:
		attr = 0xa0f55000 | (ffsl(size) - 2);
		break;
	default:
		attr = 0;
		break;
	}
	tar = start >> 12;
	bar = pci_start >> 12;

	bus_space_write_4(sc->sc_bst, sc->sc_bsh, REG_PITAR(wnd), tar);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, REG_PIWBEAR(wnd), 0);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, REG_PIWBAR(wnd), bar);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, REG_PIWAR(wnd), attr);
}

static void
fsl_pcib_outbound(struct fsl_pcib_softc *sc, int wnd, int res, u_long start,
    u_long size, u_long pci_start)
{
	uint32_t attr, bar, tar;

	switch (res) {
	case SYS_RES_MEMORY:
		attr = 0x80044000 | (ffsl(size) - 2);
		break;
	case SYS_RES_IOPORT:
		attr = 0x80088000 | (ffsl(size) - 2);
		break;
	default:
		attr = 0x0004401f;
		break;
	}
	bar = start >> 12;
	tar = pci_start >> 12;

	bus_space_write_4(sc->sc_bst, sc->sc_bsh, REG_POTAR(wnd), tar);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, REG_POTEAR(wnd), 0);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, REG_POWBAR(wnd), bar);
	bus_space_write_4(sc->sc_bst, sc->sc_bsh, REG_POWAR(wnd), attr);
}

static int
fsl_pcib_set_range(struct fsl_pcib_softc *sc, int type, int wnd, u_long start,
    u_long size)
{
	struct rman *rm;
	u_long end, alloc;
	bus_addr_t pci_start, pci_end;
	bus_addr_t *vap, *allocp;
	int error;

	end = start + size - 1;

	switch (type) {
	case SYS_RES_IOPORT:
		rm = &sc->sc_ioport;
		pci_start = 0x0000;
		pci_end = 0xffff;
		alloc = 0x1000;
		vap = &sc->sc_ioport_va;
		allocp = &sc->sc_ioport_alloc;
		break;
	case SYS_RES_MEMORY:
		rm = &sc->sc_iomem;
		pci_start = start;
		pci_end = end;
		alloc = 0;
		vap = &sc->sc_iomem_va;
		allocp = &sc->sc_iomem_alloc;
		break;
	default:
		return (EINVAL);
	}

	rm->rm_type = RMAN_ARRAY;
	rm->rm_start = pci_start;
	rm->rm_end = pci_end;
	error = rman_init(rm);
	if (error)
		return (error);

	error = rman_manage_region(rm, pci_start, pci_end);
	if (error) {
		rman_fini(rm);
		return (error);
	}

	*allocp = pci_start + alloc;
	*vap = (uintptr_t)pmap_mapdev(start, size);
	fsl_pcib_outbound(sc, wnd, type, start, size, pci_start);
	return (0);
}

static void
fsl_pcib_err_init(device_t dev)
{
	struct fsl_pcib_softc *sc;
	uint16_t sec_stat, dsr;
	uint32_t dcr, err_en;

	sc = device_get_softc(dev);

	sec_stat = fsl_pcib_cfgread(sc, 0, 0, 0, PCIR_SECSTAT_1, 2);
	if (sec_stat)
		fsl_pcib_cfgwrite(sc, 0, 0, 0, PCIR_SECSTAT_1, 0xffff, 2);
	if (sc->sc_pcie) {
		/* Clear error bits */
		bus_space_write_4(sc->sc_bst, sc->sc_bsh, REG_PEX_MES_IER,
		    0xffffffff);
		bus_space_write_4(sc->sc_bst, sc->sc_bsh, REG_PEX_MES_DR,
		    0xffffffff);
		bus_space_write_4(sc->sc_bst, sc->sc_bsh, REG_PEX_ERR_DR,
		    0xffffffff);

		dsr = fsl_pcib_cfgread(sc, 0, 0, 0,
		    sc->sc_pcie_capreg + PCIR_EXPRESS_DEVICE_STA, 2);
		if (dsr)
			fsl_pcib_cfgwrite(sc, 0, 0, 0,
			    sc->sc_pcie_capreg + PCIR_EXPRESS_DEVICE_STA,
			    0xffff, 2);

		/* Enable all errors reporting */
		err_en = 0x00bfff00;
		bus_space_write_4(sc->sc_bst, sc->sc_bsh, REG_PEX_ERR_EN,
		    err_en);

		/* Enable error reporting: URR, FER, NFER */
		dcr = fsl_pcib_cfgread(sc, 0, 0, 0,
		    sc->sc_pcie_capreg + PCIR_EXPRESS_DEVICE_CTL, 4);
		dcr |= PCIM_EXP_CTL_URR_ENABLE | PCIM_EXP_CTL_FER_ENABLE |
		    PCIM_EXP_CTL_NFER_ENABLE;
		fsl_pcib_cfgwrite(sc, 0, 0, 0,
		    sc->sc_pcie_capreg + PCIR_EXPRESS_DEVICE_CTL, dcr, 4);
	}
}

static int
fsl_pcib_detach(device_t dev)
{

	if (mtx_initialized) {
		mtx_destroy(&pcicfg_mtx);
		mtx_initialized = 0;
	}
	return (bus_generic_detach(dev));
}

static struct resource *
fsl_pcib_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct fsl_pcib_softc *sc = device_get_softc(dev);
	struct rman *rm;
	struct resource *res;
	bus_addr_t va;

	switch (type) {
	case SYS_RES_IOPORT:
		rm = &sc->sc_ioport;
		va = sc->sc_ioport_va;
		break;
	case SYS_RES_MEMORY:
		rm = &sc->sc_iomem;
		va = sc->sc_iomem_va;
		break;
	case SYS_RES_IRQ:
		if (start < 16) {
			device_printf(dev, "%s requested ISA interrupt %lu\n",
			    device_get_nameunit(child), start);
		}
		flags |= RF_SHAREABLE;
		return (BUS_ALLOC_RESOURCE(device_get_parent(dev), child,
		    type, rid, start, end, count, flags));
	default:
		return (NULL);
	}

	res = rman_reserve_resource(rm, start, end, count, flags, child);
	if (res == NULL)
		return (NULL);

	rman_set_bustag(res, &bs_le_tag);
	rman_set_bushandle(res, va + rman_get_start(res) - rm->rm_start);
	return (res);
}

static int
fsl_pcib_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *res)
{

	return (rman_release_resource(res));
}

static int
fsl_pcib_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct fsl_pcib_softc *sc = device_get_softc(dev);

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
fsl_pcib_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct fsl_pcib_softc *sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_BUS:
		sc->sc_busnr = value;
		return (0);
	}
	return (ENOENT);
}

static int
fsl_pcib_intr_info(phandle_t node, struct fsl_pcib_softc *sc)
{
	int error;

	if ((error = fdt_pci_intr_info(node, &sc->sc_intr_info)) != 0)
		return (error);

	return (0);
}

static int
fsl_pcib_decode_win(phandle_t node, struct fsl_pcib_softc *sc)
{
	struct fdt_pci_range io_space, mem_space;
	device_t dev;
	int error;

	dev = sc->sc_dev;

	if ((error = fdt_pci_ranges(node, &io_space, &mem_space)) != 0) {
		device_printf(dev, "could not retrieve 'ranges' data\n");
		return (error);
	}

	/*
	 * Configure LAW decode windows.
	 */
	error = law_pci_target(sc->sc_res, &sc->sc_iomem_target,
	    &sc->sc_ioport_target);
	if (error != 0) {
		device_printf(dev, "could not retrieve PCI LAW target info\n");
		return (error);
	}
	error = law_enable(sc->sc_iomem_target, mem_space.base_parent,
	    mem_space.len);
	if (error != 0) {
		device_printf(dev, "could not program LAW for PCI MEM range\n");
		return (error);
	}
	error = law_enable(sc->sc_ioport_target, io_space.base_parent,
	    io_space.len);
	if (error != 0) {
		device_printf(dev, "could not program LAW for PCI IO range\n");
		return (error);
	}

	/*
	 * Set outbout and inbound windows.
	 */
	fsl_pcib_outbound(sc, 0, -1, 0, 0, 0);
	if ((error = fsl_pcib_set_range(sc, SYS_RES_MEMORY, 1,
	    mem_space.base_parent, mem_space.len)) != 0)
		return (error);
	if ((error = fsl_pcib_set_range(sc, SYS_RES_IOPORT, 2,
	    io_space.base_parent, io_space.len)) != 0)
		return (error);

	fsl_pcib_outbound(sc, 3, -1, 0, 0, 0);
	fsl_pcib_outbound(sc, 4, -1, 0, 0, 0);

	fsl_pcib_inbound(sc, 1, -1, 0, 0, 0);
	fsl_pcib_inbound(sc, 2, -1, 0, 0, 0);
	fsl_pcib_inbound(sc, 3, OCP85XX_TGTIF_RAM1, 0,
	    2U * 1024U * 1024U * 1024U, 0);

	return (0);
}
