/*-
 * Copyright (c) 2010 Marcel Moolenaar
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/pcpu.h>
#include <sys/rman.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>

#include "pcib_if.h"

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/pci_cfgreg.h>
#include <machine/resource.h>
#include <machine/sal.h>
#include <machine/sgisn.h>

#include <ia64/sgisn/sgisn_pcib.h>

static struct sgisn_fwdev sgisn_dev;
static struct sgisn_fwirq sgisn_irq;

struct sgisn_pcib_softc {
	device_t	sc_dev;
	struct sgisn_fwbus *sc_fwbus;
	bus_addr_t	sc_ioaddr;
	bus_space_tag_t	sc_tag;
	bus_space_handle_t sc_hndl;
	u_int		sc_domain;
	u_int		sc_busnr;
	struct rman	sc_ioport;
	struct rman	sc_iomem;
};

static int sgisn_pcib_attach(device_t);
static int sgisn_pcib_probe(device_t);

static int sgisn_pcib_activate_resource(device_t, device_t, int, int,
    struct resource *);
static struct resource *sgisn_pcib_alloc_resource(device_t, device_t, int,
    int *, u_long, u_long, u_long, u_int);
static int sgisn_pcib_deactivate_resource(device_t, device_t, int, int,
    struct resource *);
static void sgisn_pcib_delete_resource(device_t, device_t, int, int);
static int sgisn_pcib_get_resource(device_t, device_t, int, int, u_long *,
    u_long *);
static struct resource_list *sgisn_pcib_get_resource_list(device_t, device_t);
static int sgisn_pcib_release_resource(device_t, device_t, int, int,
    struct resource *);
static int sgisn_pcib_set_resource(device_t, device_t, int, int, u_long,
    u_long);

static int sgisn_pcib_read_ivar(device_t, device_t, int, uintptr_t *);
static int sgisn_pcib_write_ivar(device_t, device_t, int, uintptr_t);

static int sgisn_pcib_maxslots(device_t);
static uint32_t sgisn_pcib_cfgread(device_t, u_int, u_int, u_int, u_int, int);
static void sgisn_pcib_cfgwrite(device_t, u_int, u_int, u_int, u_int, uint32_t,
    int);

/*
 * Bus interface definitions.
 */
static device_method_t sgisn_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sgisn_pcib_probe),
	DEVMETHOD(device_attach,	sgisn_pcib_attach),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	sgisn_pcib_read_ivar),
	DEVMETHOD(bus_write_ivar,	sgisn_pcib_write_ivar),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_activate_resource, sgisn_pcib_activate_resource),
	DEVMETHOD(bus_alloc_resource,	sgisn_pcib_alloc_resource),
	DEVMETHOD(bus_deactivate_resource, sgisn_pcib_deactivate_resource),
	DEVMETHOD(bus_delete_resource,	sgisn_pcib_delete_resource),
	DEVMETHOD(bus_get_resource,	sgisn_pcib_get_resource),
	DEVMETHOD(bus_get_resource_list, sgisn_pcib_get_resource_list),
	DEVMETHOD(bus_release_resource,	sgisn_pcib_release_resource),
	DEVMETHOD(bus_set_resource,	sgisn_pcib_set_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	sgisn_pcib_maxslots),
	DEVMETHOD(pcib_read_config,	sgisn_pcib_cfgread),
	DEVMETHOD(pcib_write_config,	sgisn_pcib_cfgwrite),
	DEVMETHOD(pcib_route_interrupt,	pcib_route_interrupt),

	{ 0, 0 }
};

static driver_t sgisn_pcib_driver = {
	"pcib",
	sgisn_pcib_methods,
	sizeof(struct sgisn_pcib_softc),
};

devclass_t pcib_devclass;

DRIVER_MODULE(pcib, shub, sgisn_pcib_driver, pcib_devclass, 0, 0);

static int
sgisn_pcib_maxslots(device_t dev)
{

	return (PCI_SLOTMAX);
}

static uint32_t
sgisn_pcib_cfgread(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, int bytes)
{
	struct sgisn_pcib_softc *sc;
	uint32_t val;

	sc = device_get_softc(dev);

	val = pci_cfgregread((sc->sc_domain << 8) | bus, slot, func, reg,
	    bytes);
	return (val);
}

static void
sgisn_pcib_cfgwrite(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, uint32_t val, int bytes)
{
	struct sgisn_pcib_softc *sc;

	sc = device_get_softc(dev);

	pci_cfgregwrite((sc->sc_domain << 8) | bus, slot, func, reg, val,
	    bytes);
}

static int
sgisn_pcib_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *res)
{
	int error;

	error = rman_activate_resource(res);
	return (error);
}

static struct resource *
sgisn_pcib_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct ia64_sal_result r;
	struct rman *rm;
	struct resource *rv;
	struct sgisn_pcib_softc *sc;
	device_t parent;
	void *vaddr;
	uint64_t base;
	uintptr_t func, slot;
	int bar, error;

	if (type == SYS_RES_IRQ)
		return (bus_generic_alloc_resource(dev, child, type, rid,
		    start, end, count, flags));

	bar = PCI_RID2BAR(*rid);
	if (bar < 0 || bar > PCIR_MAX_BAR_0)
		return (NULL);

	sc = device_get_softc(dev);
	rm = (type == SYS_RES_MEMORY) ? &sc->sc_iomem : &sc->sc_ioport;
	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == NULL)
		return (NULL);

	parent = device_get_parent(child);
	error = BUS_READ_IVAR(parent, child, PCI_IVAR_SLOT, &slot);
	if (error)
		goto fail;
	error = BUS_READ_IVAR(parent, child, PCI_IVAR_FUNCTION, &func);
	if (error)
		goto fail;

	r = ia64_sal_entry(SAL_SGISN_IODEV_INFO, sc->sc_domain, sc->sc_busnr,
	    (slot << 3) | func, ia64_tpa((uintptr_t)&sgisn_dev),
	    ia64_tpa((uintptr_t)&sgisn_irq), 0, 0);
	if (r.sal_status != 0)
		goto fail;

	base = sgisn_dev.dev_bar[bar] & 0x7fffffffffffffffL;
	if (base != start)
		device_printf(dev, "PCI bus address %#lx mapped to CPU "
		    "address %#lx\n", start, base);

	/* I/O port space is presented as memory mapped I/O. */
	rman_set_bustag(rv, IA64_BUS_SPACE_MEM);
	vaddr = pmap_mapdev(base, count);
	rman_set_bushandle(rv, (bus_space_handle_t)vaddr);
	if (type == SYS_RES_MEMORY)
		rman_set_virtual(rv, vaddr);
	return (rv);

 fail:
	rman_release_resource(rv);
	return (NULL);
}

static int
sgisn_pcib_deactivate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *res)
{
	int error;

	error = rman_deactivate_resource(res);
	return (error);
}

static void
sgisn_pcib_delete_resource(device_t dev, device_t child, int type, int rid)
{
}

static int
sgisn_pcib_get_resource(device_t dev, device_t child, int type, int rid,
    u_long *startp, u_long *countp)
{

	return (ENOENT);
}

static struct resource_list *
sgisn_pcib_get_resource_list(device_t dev, device_t child)
{

	return (NULL);
}

static int
sgisn_pcib_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *res)
{
	int error;

	if (rman_get_flags(res) & RF_ACTIVE) {
		error = rman_deactivate_resource(res);
		if (error)
			return (error);
	}
	error = rman_release_resource(res);
	return (error);
}

static int
sgisn_pcib_set_resource(device_t dev, device_t child, int type, int rid,
    u_long start, u_long count)
{

	return (ENXIO);
}

static int
sgisn_pcib_probe(device_t dev)
{
	device_t parent;
	uintptr_t bus, seg;

	parent = device_get_parent(dev);
	if (parent == NULL)
		return (ENXIO);

	if (BUS_READ_IVAR(parent, dev, SHUB_IVAR_PCISEG, &seg) ||
	    BUS_READ_IVAR(parent, dev, SHUB_IVAR_PCIBUS, &bus))
		return (ENXIO);

	device_set_desc(dev, "SGI PCI-X host controller");
	return (BUS_PROBE_DEFAULT);
}

static void
sgisn_pcib_callout(void *arg)
{
	static u_long islast = ~0UL;
	struct sgisn_pcib_softc *sc = arg;
	u_long is;

	is = bus_space_read_8(sc->sc_tag, sc->sc_hndl, PIC_REG_INT_STATUS);
	if (is != islast) {
		islast = is;
		printf("XXX: %s: INTR status = %lu, IRR=%#lx:%#lx:%#lx:%#lx\n",
		    __func__, is, ia64_get_irr0(), ia64_get_irr1(),
		    ia64_get_irr2(), ia64_get_irr3());
	}

	timeout(sgisn_pcib_callout, sc, hz);
}

static int
sgisn_pcib_rm_init(struct sgisn_pcib_softc *sc, struct rman *rm,
    const char *what)
{
	char descr[128];
	int error;

	rm->rm_start = 0UL;
	rm->rm_end = 0x3ffffffffUL;		/* 16GB */
	rm->rm_type = RMAN_ARRAY;
	error = rman_init(rm);
	if (error)
		return (error);

	snprintf(descr, sizeof(descr), "PCI %u:%u local I/O %s addresses",
	    sc->sc_domain, sc->sc_busnr, what);
	rm->rm_descr = strdup(descr, M_DEVBUF);

	error = rman_manage_region(rm, rm->rm_start, rm->rm_end);
	if (error)
		rman_fini(rm);

	return (error);
}

static int
sgisn_pcib_attach(device_t dev)
{
	struct sgisn_pcib_softc *sc;
	device_t parent;
	uintptr_t addr, bus, seg;
	int error;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	parent = device_get_parent(dev);
	BUS_READ_IVAR(parent, dev, SHUB_IVAR_PCIBUS, &bus);
	sc->sc_busnr = bus;
	BUS_READ_IVAR(parent, dev, SHUB_IVAR_PCISEG, &seg);
	sc->sc_domain = seg;

	error = sgisn_pcib_rm_init(sc, &sc->sc_ioport, "port");
	if (error)
		return (error);
	error = sgisn_pcib_rm_init(sc, &sc->sc_iomem, "memory");
	if (error) {
		rman_fini(&sc->sc_ioport);
		return (error);
	}

	(void)ia64_sal_entry(SAL_SGISN_IOBUS_INFO, seg, bus,
	    ia64_tpa((uintptr_t)&addr), 0, 0, 0, 0);
	sc->sc_fwbus = (void *)IA64_PHYS_TO_RR7(addr);
	sc->sc_ioaddr = IA64_RR_MASK(sc->sc_fwbus->bus_base);
	sc->sc_tag = IA64_BUS_SPACE_MEM;
	bus_space_map(sc->sc_tag, sc->sc_ioaddr, PIC_REG_SIZE, 0,
	    &sc->sc_hndl);

	if (bootverbose)
		device_printf(dev, "ASIC=%x, XID=%u\n", sc->sc_fwbus->bus_asic,
		    sc->sc_fwbus->bus_xid);

	timeout(sgisn_pcib_callout, sc, hz);

	device_add_child(dev, "pci", -1);
	return (bus_generic_attach(dev));
}

static int
sgisn_pcib_read_ivar(device_t dev, device_t child, int which, uintptr_t *res)
{
	struct sgisn_pcib_softc *sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_BUS:
		*res = sc->sc_busnr;
		return (0);
	case PCIB_IVAR_DOMAIN:
		*res = sc->sc_domain;
		return (0);
	}
	return (ENOENT);
}

static int
sgisn_pcib_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct sgisn_pcib_softc *sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_BUS:
		sc->sc_busnr = value;
		return (0);
	}
	return (ENOENT);
}
