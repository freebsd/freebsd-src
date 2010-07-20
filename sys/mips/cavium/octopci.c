/*-
 * Copyright (c) 2010 Juli Mallett <jmallett@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/interrupt.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/pmap.h>

#include <contrib/octeon-sdk/cvmx.h>
#include <contrib/octeon-sdk/cvmx-interrupt.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/pci/pcib_private.h>

#include <mips/cavium/octopcireg.h>
#include <mips/cavium/octopcivar.h>

#include "pcib_if.h"

struct octopci_softc {
	device_t sc_dev;

	unsigned sc_domain;
	unsigned sc_bus;

	struct rman sc_io;
	struct rman sc_mem1;
};

static void		octopci_identify(driver_t *, device_t);
static int		octopci_probe(device_t);
static int		octopci_attach(device_t);
static int		octopci_read_ivar(device_t, device_t, int,
					  uintptr_t *);
static struct resource	*octopci_alloc_resource(device_t, device_t, int, int *,
						u_long, u_long, u_long, u_int);
static int		octopci_activate_resource(device_t, device_t, int, int,
						  struct resource *);
static int	octopci_maxslots(device_t);
static uint32_t	octopci_read_config(device_t, u_int, u_int, u_int, u_int, int);
static void	octopci_write_config(device_t, u_int, u_int, u_int, u_int,
				     uint32_t, int);
static int	octopci_route_interrupt(device_t, device_t, int);

static uint64_t	octopci_cs_addr(unsigned, unsigned, unsigned, unsigned);

static void
octopci_identify(driver_t *drv, device_t parent)
{
	BUS_ADD_CHILD(parent, 0, "pcib", 0);
}

static int
octopci_probe(device_t dev)
{
	if (device_get_unit(dev) != 0)
		return (ENXIO);
	/* XXX Check sysinfo flag.  */
	device_set_desc(dev, "Cavium Octeon PCI bridge");
	return (0);
}

static int
octopci_attach(device_t dev)
{
	struct octopci_softc *sc;
	int error;

	/*
	 * XXX
	 * We currently rely on U-Boot to set up the PCI in host state.  We
	 * should properly initialize the PCI bus here.
	 */

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_domain = 0;
	sc->sc_bus = 0;

	sc->sc_io.rm_type = RMAN_ARRAY;
	sc->sc_io.rm_descr = "Cavium Octeon PCI I/O Ports";
	error = rman_init(&sc->sc_io);
	if (error != 0)
		return (error);

	error = rman_manage_region(&sc->sc_io, CVMX_OCT_PCI_IO_BASE,
	    CVMX_OCT_PCI_IO_BASE + CVMX_OCT_PCI_IO_SIZE);
	if (error != 0)
		return (error);

	sc->sc_mem1.rm_type = RMAN_ARRAY;
	sc->sc_mem1.rm_descr = "Cavium Octeon PCI Memory";
	error = rman_init(&sc->sc_mem1);
	if (error != 0)
		return (error);

	error = rman_manage_region(&sc->sc_mem1, CVMX_OCT_PCI_MEM1_BASE,
	    CVMX_OCT_PCI_MEM1_BASE + CVMX_OCT_PCI_MEM1_SIZE);
	if (error != 0)
		return (error);

	device_add_child(dev, "pci", 0);

	return (bus_generic_attach(dev));
}

static int
octopci_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct octopci_softc *sc;
	
	sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_DOMAIN:
		*result = sc->sc_domain;
		return (0);
	case PCIB_IVAR_BUS:
		*result = sc->sc_bus;
		return (0);
		
	}
	return (ENOENT);
}

static struct resource *
octopci_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct octopci_softc *sc;
	struct resource *res;
	struct rman *rm;
	int error;

	sc = device_get_softc(bus);

	switch (type) {
	case SYS_RES_IRQ:
		res = bus_generic_alloc_resource(bus, child, type, rid, start,
		    end, count, flags);
		if (res != NULL)
			return (res);
		return (NULL);
	case SYS_RES_MEMORY:
		rm = &sc->sc_mem1;
		break;
	case SYS_RES_IOPORT:
		rm = &sc->sc_io;
		break;
	default:
		return (NULL);
	}

	res = rman_reserve_resource(rm, start, end, count, flags, child);
	if (res == NULL)
		return (NULL);

	rman_set_rid(res, *rid);
	rman_set_bustag(res, octopci_bus_space);

	switch (type) {
	case SYS_RES_MEMORY:
		rman_set_bushandle(res, CVMX_ADDR_DID(CVMX_FULL_DID(CVMX_OCT_DID_PCI, CVMX_OCT_SUBDID_PCI_MEM1)) + rman_get_start(res));
		break;
	case SYS_RES_IOPORT:
		rman_set_bushandle(res, CVMX_ADDR_DID(CVMX_FULL_DID(CVMX_OCT_DID_PCI, CVMX_OCT_SUBDID_PCI_IO)) + rman_get_start(res));
		/*
		 * XXX
		 * We should just disallow use of io ports on !n64 since without
		 * 64-bit PTEs we can't even do a 32-bit virtual address
		 * mapped to them.
		 */
#if 0
		rman_set_virtual(res, (void *)rman_get_bushandle(res));
#endif
		break;
	}

	if ((flags & RF_ACTIVE) != 0) {
		error = bus_activate_resource(child, type, *rid, res);
		if (error != 0) {
			rman_release_resource(res);
			return (NULL);
		}
	}

	return (res);
}

static int
octopci_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{
	bus_space_handle_t bh;
	int error;

	switch (type) {
	case SYS_RES_IRQ:
		error = bus_generic_activate_resource(bus, child, type, rid,
						      res);
		if (error != 0)
			return (error);
		return (0);
	case SYS_RES_MEMORY:
	case SYS_RES_IOPORT:
		error = bus_space_map(rman_get_bustag(res),
		    rman_get_bushandle(res), rman_get_size(res), 0, &bh);
		if (error != 0)
			return (error);
		rman_set_bushandle(res, bh);
		break;
	default:
		return (ENXIO);
	}

	error = rman_activate_resource(res);
	if (error != 0)
		return (error);
	return (0);
}

static int
octopci_maxslots(device_t dev)
{
	return (PCI_SLOTMAX);
}

static uint32_t
octopci_read_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
    int bytes)
{
	struct octopci_softc *sc;
	uint64_t addr;
	uint32_t data;

	sc = device_get_softc(dev);

	addr = octopci_cs_addr(bus, slot, func, reg);

	switch (bytes) {
	case 4:
		data = le32toh(cvmx_read64_uint32(addr));
		return (data);
	case 2:
		data = le16toh(cvmx_read64_uint16(addr));
		return (data);
	case 1:
		data = cvmx_read64_uint8(addr);
		return (data);
	default:
		return ((uint32_t)-1);
	}
}

static void
octopci_write_config(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, uint32_t data, int bytes)
{
	struct octopci_softc *sc;
	uint64_t addr;

	sc = device_get_softc(dev);

	addr = octopci_cs_addr(bus, slot, func, reg);

	switch (bytes) {
	case 4:
		cvmx_write64_uint32(addr, htole32(data));
		return;
	case 2:
		cvmx_write64_uint16(addr, htole16(data));
		return;
	case 1:
		cvmx_write64_uint8(addr, data);
		return;
	default:
		return;
	}
}

static int
octopci_route_interrupt(device_t dev, device_t child, int pin)
{
	struct octopci_softc *sc;
	unsigned bus, slot, func;
	unsigned irq;

	sc = device_get_softc(dev);

        bus = pci_get_bus(child);
        slot = pci_get_slot(child);
        func = pci_get_function(child);

#if defined(OCTEON_VENDOR_LANNER)
	if (slot < 32) {
		if (slot == 3 || slot == 9)
			irq = pin;
		else
			irq = pin - 1;
		return (CVMX_IRQ_PCI_INT0 + (irq & 3));
	}
#endif

	irq = slot + pin - 3;

	return (CVMX_IRQ_PCI_INT0 + (irq & 3));
}

static uint64_t
octopci_cs_addr(unsigned bus, unsigned slot, unsigned func, unsigned reg)
{
	octeon_pci_config_space_address_t pci_addr;

	pci_addr.u64 = 0;
	pci_addr.s.upper = 2;
	pci_addr.s.io = 1;
	pci_addr.s.did = 3;
	pci_addr.s.subdid = CVMX_OCT_SUBDID_PCI_CFG;
	pci_addr.s.endian_swap = 1;
	pci_addr.s.bus = bus;
	pci_addr.s.dev = slot;
	pci_addr.s.func = func;
	pci_addr.s.reg = reg;

	return (pci_addr.u64);
}

static device_method_t octopci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	octopci_identify),
	DEVMETHOD(device_probe,		octopci_probe),
	DEVMETHOD(device_attach,	octopci_attach),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	octopci_read_ivar),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_alloc_resource,	octopci_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource,octopci_activate_resource),
	DEVMETHOD(bus_deactivate_resource,bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	octopci_maxslots),
	DEVMETHOD(pcib_read_config,	octopci_read_config),
	DEVMETHOD(pcib_write_config,	octopci_write_config),
	DEVMETHOD(pcib_route_interrupt,	octopci_route_interrupt),

	{0, 0}
};

static driver_t octopci_driver = {
	"pcib",
	octopci_methods,
	sizeof(struct octopci_softc),
};
static devclass_t octopci_devclass;
DRIVER_MODULE(octopci, ciu, octopci_driver, octopci_devclass, 0, 0);
