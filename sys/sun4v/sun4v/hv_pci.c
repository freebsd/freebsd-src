/*-
 * Copyright 2006 John-Mark Gurney.
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/sun4v/sun4v/hv_pci.c,v 1.4.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

/*
 * Support for the Hypervisor PCI bus.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/pcpu.h>
#include <sys/endian.h>
#include <sys/rman.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <machine/bus.h>

#include <machine/hypervisorvar.h>
#include <machine/hv_api.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/openfirm.h>
#include <sparc64/pci/ofw_pci.h>

#include <machine/hv_pcivar.h>
#include <machine/hviommu.h>
#include <machine/vmparam.h>
#include <machine/tlb.h>
#include <machine/nexusvar.h>

#include "pcib_if.h"

/*
 * XXX - should get this through the bus, but Sun overloaded the reg OFW
 * property, so there isn't normal resources associated w/ this device.
 */
extern struct bus_space_tag nexus_bustag;
/*
 * Methods
 */
static device_probe_t hvpci_probe;
static device_attach_t hvpci_attach;
static bus_read_ivar_t hvpci_read_ivar;
static bus_write_ivar_t hvpci_write_ivar;
static bus_alloc_resource_t hvpci_alloc_resource;
static bus_activate_resource_t hvpci_activate_resource;
static bus_deactivate_resource_t hvpci_deactivate_resource;
static bus_release_resource_t hvpci_release_resource;
static bus_get_dma_tag_t hvpci_get_dma_tag;
static pcib_maxslots_t hvpci_maxslots;
static pcib_read_config_t hvpci_read_config;
static pcib_write_config_t hvpci_write_config;
static pcib_route_interrupt_t hvpci_route_interrupt;
static ofw_bus_get_node_t hvpci_get_node;

static device_method_t hv_pcib_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		hvpci_probe),
	DEVMETHOD(device_attach,	hvpci_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	hvpci_read_ivar),
	DEVMETHOD(bus_write_ivar,	hvpci_write_ivar),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	hvpci_alloc_resource),
	DEVMETHOD(bus_activate_resource,	hvpci_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	hvpci_deactivate_resource),
	DEVMETHOD(bus_release_resource,	hvpci_release_resource),
	DEVMETHOD(bus_get_dma_tag,	hvpci_get_dma_tag),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	hvpci_maxslots),
	DEVMETHOD(pcib_read_config,	hvpci_read_config),
	DEVMETHOD(pcib_write_config,	hvpci_write_config),
	DEVMETHOD(pcib_route_interrupt,	hvpci_route_interrupt),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,	hvpci_get_node),

	{ 0, 0 }
};

static driver_t hvpci_driver = {
	"pcib",
	hv_pcib_methods,
	sizeof(struct hvpci_softc),
};

static devclass_t hvpci_devclass;

DRIVER_MODULE(hvpci, nexus, hvpci_driver, hvpci_devclass, 0, 0);

static int
hvpci_probe(device_t dev)
{

	if (strcmp(ofw_bus_get_name(dev), "pci") != 0)
		return (ENXIO);

	device_set_desc(dev, "Hypervisor PCI Bridge");
	return (0);
}

static int
hvpci_attach(device_t dev)
{
	struct ofw_pci_ranges *range;
	struct rman *rmanp;
	struct hvpci_softc *sc;
	struct hviommu *himp;
	bus_space_tag_t *btp;
	phandle_t node;
	uint32_t *dvma;
	int br[2];
	int n, type;
	int i, nrange;

	sc = device_get_softc(dev);

	node = ofw_bus_get_node(dev);
	if (node == -1)
		panic("%s: ofw_bus_get_node failed.", __func__);

	sc->hs_node = node;

	/* Setup the root bus number for this bus */
	n = OF_getprop(node, "bus-range", &br[0], sizeof br);
	if (n == -1)
		panic("%s: could not get bus-range", __func__);
	if (n != sizeof br)
		panic("%s: broken bus-range (%d)", __func__, n);
	sc->hs_busnum = br[0];

	/* Setup the HyperVisor devhandle for this bus */
	sc->hs_devhandle = nexus_get_devhandle(dev);

	/* Pull in the ra addresses out of OFW */
	nrange = OF_getprop_alloc(node, "ranges", sizeof *range,
	    (void **)&range);

	/* Initialize memory and I/O rmans. */
	for (i = 0; i < nrange; i++) {
/* XXX - from sun4v/io/px/px_lib4v.c: px_ranges_phi_mask */
#define PHYS_MASK	((1ll << (28 + 32)) - 1)
		switch (OFW_PCI_RANGE_CS(&range[i])) {
		case OFW_PCI_CS_IO:
			rmanp = &sc->hs_pci_io_rman;
			rmanp->rm_descr = "HyperVisor PCI I/O Ports";
			btp = &sc->hs_pci_iot;
			sc->hs_pci_ioh = OFW_PCI_RANGE_PHYS(&range[i]) &
			    PHYS_MASK;
			type = PCI_IO_BUS_SPACE;
#ifdef DEBUG
			printf("io handle: %#lx\n", sc->hs_pci_ioh);
#endif
			break;

		case OFW_PCI_CS_MEM32:
			rmanp = &sc->hs_pci_mem_rman;
			rmanp->rm_descr = "HyperVisor PCI Memory";
			btp = &sc->hs_pci_memt;
			sc->hs_pci_memh = OFW_PCI_RANGE_PHYS(&range[i]) &
			    PHYS_MASK;
			type = PCI_MEMORY_BUS_SPACE;
			break;

		case OFW_PCI_CS_MEM64:
			continue;

		default:
			panic("%s: unknown range type: %d", __func__,
			    OFW_PCI_RANGE_CS(&range[i]));
		}
		rmanp->rm_type = RMAN_ARRAY;
		if (rman_init(rmanp) != 0 || rman_manage_region(rmanp, 0,
		    OFW_PCI_RANGE_SIZE(&range[i])) != 0)
			panic("%s: failed to set up rman type: %d", __func__,
			    OFW_PCI_RANGE_CS(&range[i]));

		*btp = (bus_space_tag_t)malloc(sizeof **btp, M_DEVBUF,
		    M_WAITOK|M_ZERO);
		(*btp)->bst_parent = &nexus_bustag;
		(*btp)->bst_type = type;
	}
	free(range, M_OFWPROP);

	nrange = OF_getprop_alloc(node, "virtual-dma", sizeof *dvma,
	    (void **)&dvma);
	KASSERT(nrange == 2, ("virtual-dma propery invalid"));

	/* Setup bus_dma_tag */
	himp = hviommu_init(sc->hs_devhandle, dvma[0], dvma[1]);
	sc->hs_dmatag.dt_cookie = himp;
	sc->hs_dmatag.dt_mt = &hviommu_dma_methods;
	sc->hs_dmatag.dt_lowaddr = ~0;
	sc->hs_dmatag.dt_highaddr = ~0;
	sc->hs_dmatag.dt_boundary = BUS_SPACE_MAXADDR_32BIT + 1;

	free(dvma, M_OFWPROP);

	/* Setup ofw imap */
	ofw_bus_setup_iinfo(node, &sc->hs_pci_iinfo, sizeof(ofw_pci_intr_t));

	device_add_child(dev, "pci", -1);

	return (bus_generic_attach(dev));
}

static int
hvpci_maxslots(device_t dev)
{

	return (0);
}

#define HVPCI_BDF(b, d, f)	\
		((b & 0xff) << 16) | ((d & 0x1f) << 11) | ((f & 0x7) << 8)
static uint32_t
hvpci_read_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
    int width)
{
	struct hvpci_softc *sc;
	uint32_t data = -1;
	uint64_t r;
	uint32_t ret;

	sc = device_get_softc(dev);

	r = hv_pci_config_get(sc->hs_devhandle, HVPCI_BDF(bus, slot, func),
			    reg, width, (pci_cfg_data_t *)&data);
		
	if (r == H_EOK) {
		switch (width) {
		case 1:
			ret = data & 0xff;
			if (ret == 0 && reg == PCIR_INTLINE)
				ret = PCI_INVALID_IRQ;
			break;
		case 2:
			ret = data & 0xffff;
			break;
		case 4:
			ret = data;
			break;
		default:
			ret = -1;
		}
		return ret;
	}

	return -1;
}

static void
hvpci_write_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
     uint32_t val, int width)
{
	struct hvpci_softc *sc;
	pci_cfg_data_t data = { 0 };
	uint64_t r;

	sc = device_get_softc(dev);
	switch (width) {
	case 1:
		data.qw = (uint8_t)val;
		break;
	case 2:
		data.qw = (uint16_t)(val & 0xffff);
		break;
	case 4:
		data.qw = (uint32_t)val;
		break;
	default:
		panic("unsupported width: %d", width);
	}

	r = hv_pci_config_put(sc->hs_devhandle, HVPCI_BDF(bus, slot, func), 
			    reg, width, (pci_cfg_data_t)data);

	if (r)
		printf("put failed with: %ld\n", r);
}

static int
hvpci_route_interrupt(device_t bridge, device_t dev, int pin)
{
	struct hvpci_softc *sc;
	struct ofw_pci_register reg;
	phandle_t node;
	ofw_pci_intr_t pintr, mintr;
	int obli;
	uint8_t maskbuf[sizeof(reg) + sizeof(pintr)];

	sc = device_get_softc(bridge);
	node = ofw_bus_get_node(dev);
	pintr = pin;
	obli = ofw_bus_lookup_imap(node, &sc->hs_pci_iinfo, &reg, sizeof(reg),
	    &pintr, sizeof(pintr), &mintr, sizeof(mintr), maskbuf);
	device_printf(dev, "called hvpci_route_intr: %d, got: mintr: %#x\n",
	    obli, mintr);
	if (obli)
		return (mintr);

	panic("pin %d not found in imap of %s", pin, device_get_nameunit(bridge));
}

static phandle_t
hvpci_get_node(device_t bus, device_t dev)
{
	struct hvpci_softc *sc;

	sc = device_get_softc(bus);

	return (sc->hs_node);
}

static int
hvpci_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct hvpci_softc *sc;

	sc = device_get_softc(dev);

	switch (which) {
	case PCIB_IVAR_DOMAIN:
		*result = 0;
		return (0);
	case PCIB_IVAR_BUS:
		*result = sc->hs_busnum;
		return (0);
	}

	return (ENOENT);
}

static int
hvpci_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct hvpci_softc *sc;

	sc = device_get_softc(dev);
	switch (which) {
	case PCIB_IVAR_DOMAIN:
		return (EINVAL);
	case PCIB_IVAR_BUS:
		sc->hs_busnum = value;
		return (0);
	}

	return (ENOENT);
}

static struct resource *
hvpci_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct hvpci_softc *sc;
	struct resource *rv;
	struct rman *rm;
	bus_space_tag_t bt;
	bus_space_handle_t bh;
	int needactivate;

	sc = device_get_softc(bus);

	needactivate = flags & RF_ACTIVE;
	flags &= ~RF_ACTIVE;

	switch (type) {
	case SYS_RES_IRQ:
		return BUS_ALLOC_RESOURCE(device_get_parent(bus), child, type,
		    rid, start, end, count, flags);
		break;

	case SYS_RES_MEMORY:
		rm = &sc->hs_pci_mem_rman;
		bt = sc->hs_pci_memt;
		bh = sc->hs_pci_memh;
		break;

	case SYS_RES_IOPORT:
#ifdef DEBUG
		printf("alloc: start: %#lx, end: %#lx, count: %#lx\n", start, end, count);
#endif
		rm = &sc->hs_pci_io_rman;
		bt = sc->hs_pci_iot;
		bh = sc->hs_pci_ioh;
		break;

	default:
		return (NULL);
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == NULL)
		return (NULL);

	bh += rman_get_start(rv);
	rman_set_bustag(rv, bt);
	rman_set_bushandle(rv, bh);

	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv)) {
			rman_release_resource(rv);
			return (NULL);
		}
	}

	return (rv);
}

static int
hvpci_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r) 
{
	void *p;

	if (type == SYS_RES_MEMORY) {
		/* XXX - we may still need to set the IE bit on the mapping */
		p = (void *)TLB_PHYS_TO_DIRECT(rman_get_bushandle(r));
		rman_set_virtual(r, p);
	}
	return (rman_activate_resource(r));
}

static int
hvpci_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r) 
{

	return (0);
}

static int
hvpci_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	return (0);
}

static bus_dma_tag_t
hvpci_get_dma_tag(device_t bus, device_t child)
{
	struct hvpci_softc *sc;

	sc = device_get_softc(bus);

	return &sc->hs_dmatag;
}
