/*-
 * Copyright (C) 2008-2010 Nathan Whitehorn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/pciio.h>
#include <sys/rman.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_pci.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <machine/bus.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/openpicvar.h>
#include <machine/pio.h>
#include <machine/resource.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include "pcib_if.h"
#include "pic_if.h"

/*
 * IBM CPC9X5 Hypertransport Device interface.
 */
static int		cpcht_probe(device_t);
static int		cpcht_attach(device_t);

static void		cpcht_configure_htbridge(device_t, phandle_t);

/*
 * Bus interface.
 */
static int		cpcht_read_ivar(device_t, device_t, int,
			    uintptr_t *);
static struct resource *cpcht_alloc_resource(device_t bus, device_t child,
			    int type, int *rid, u_long start, u_long end,
			    u_long count, u_int flags);
static int		cpcht_activate_resource(device_t bus, device_t child,
			    int type, int rid, struct resource *res);
static int		cpcht_release_resource(device_t bus, device_t child,
			    int type, int rid, struct resource *res);
static int		cpcht_deactivate_resource(device_t bus, device_t child,
			    int type, int rid, struct resource *res);

/*
 * pcib interface.
 */
static int		cpcht_maxslots(device_t);
static u_int32_t	cpcht_read_config(device_t, u_int, u_int, u_int,
			    u_int, int);
static void		cpcht_write_config(device_t, u_int, u_int, u_int,
			    u_int, u_int32_t, int);
static int		cpcht_route_interrupt(device_t bus, device_t dev,
			    int pin);
static int		cpcht_alloc_msi(device_t dev, device_t child,
			    int count, int maxcount, int *irqs);
static int		cpcht_release_msi(device_t dev, device_t child,
			    int count, int *irqs);
static int		cpcht_alloc_msix(device_t dev, device_t child,
			    int *irq);
static int		cpcht_release_msix(device_t dev, device_t child,
			    int irq);
static int		cpcht_map_msi(device_t dev, device_t child,
			    int irq, uint64_t *addr, uint32_t *data);

/*
 * ofw_bus interface
 */

static phandle_t	cpcht_get_node(device_t bus, device_t child);

/*
 * Driver methods.
 */
static device_method_t	cpcht_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cpcht_probe),
	DEVMETHOD(device_attach,	cpcht_attach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_read_ivar,	cpcht_read_ivar),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	cpcht_alloc_resource),
	DEVMETHOD(bus_release_resource,	cpcht_release_resource),
	DEVMETHOD(bus_activate_resource,	cpcht_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	cpcht_deactivate_resource),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,	cpcht_maxslots),
	DEVMETHOD(pcib_read_config,	cpcht_read_config),
	DEVMETHOD(pcib_write_config,	cpcht_write_config),
	DEVMETHOD(pcib_route_interrupt,	cpcht_route_interrupt),
	DEVMETHOD(pcib_alloc_msi,	cpcht_alloc_msi),
	DEVMETHOD(pcib_release_msi,	cpcht_release_msi),
	DEVMETHOD(pcib_alloc_msix,	cpcht_alloc_msix),
	DEVMETHOD(pcib_release_msix,	cpcht_release_msix),
	DEVMETHOD(pcib_map_msi,		cpcht_map_msi),

	/* ofw_bus interface */
	DEVMETHOD(ofw_bus_get_node,     cpcht_get_node),
	{ 0, 0 }
};

struct cpcht_irq {
	enum {
	    IRQ_NONE, IRQ_HT, IRQ_MSI, IRQ_INTERNAL
	}		irq_type; 

	int		ht_source;

	vm_offset_t	ht_base;
	vm_offset_t	apple_eoi;
	uint32_t	eoi_data;
	int		edge;
};

static struct cpcht_irq *cpcht_irqmap = NULL;
uint32_t cpcht_msipic = 0;

struct cpcht_softc {
	device_t		sc_dev;
	phandle_t		sc_node;
	vm_offset_t		sc_data;
	uint64_t		sc_populated_slots;
	struct			rman sc_mem_rman;
	struct			rman sc_io_rman;

	struct cpcht_irq	htirq_map[128];
	struct mtx		htirq_mtx;
};

static driver_t	cpcht_driver = {
	"pcib",
	cpcht_methods,
	sizeof(struct cpcht_softc)
};

static devclass_t	cpcht_devclass;

DRIVER_MODULE(cpcht, nexus, cpcht_driver, cpcht_devclass, 0, 0);

#define CPCHT_IOPORT_BASE	0xf4000000UL /* Hardwired */
#define CPCHT_IOPORT_SIZE	0x00400000UL

#define HTAPIC_REQUEST_EOI	0x20
#define HTAPIC_TRIGGER_LEVEL	0x02
#define HTAPIC_MASK		0x01

struct cpcht_range {
	u_int32_t       pci_hi;
	u_int32_t       pci_mid;
	u_int32_t       pci_lo;
	u_int32_t       junk;
	u_int32_t       host_hi;
	u_int32_t       host_lo;
	u_int32_t       size_hi;
	u_int32_t       size_lo;
};

static int
cpcht_probe(device_t dev)
{
	const char	*type, *compatible;

	type = ofw_bus_get_type(dev);
	compatible = ofw_bus_get_compat(dev);

	if (type == NULL || compatible == NULL)
		return (ENXIO);

	if (strcmp(type, "ht") != 0)
		return (ENXIO);

	if (strcmp(compatible, "u3-ht") != 0)
		return (ENXIO);


	device_set_desc(dev, "IBM CPC9X5 HyperTransport Tunnel");
	return (0);
}

static int
cpcht_attach(device_t dev)
{
	struct		cpcht_softc *sc;
	phandle_t	node, child;
	u_int32_t	reg[3];
	int		i, error;

	node = ofw_bus_get_node(dev);
	sc = device_get_softc(dev);

	if (OF_getprop(node, "reg", reg, sizeof(reg)) < 12)
		return (ENXIO);

	sc->sc_dev = dev;
	sc->sc_node = node;
	sc->sc_populated_slots = 0;
	sc->sc_data = (vm_offset_t)pmap_mapdev(reg[1], reg[2]);

	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "CPCHT Device Memory";
	error = rman_init(&sc->sc_mem_rman);
	if (error) {
		device_printf(dev, "rman_init() failed. error = %d\n", error);
		return (error);
	}

	sc->sc_io_rman.rm_type = RMAN_ARRAY;
	sc->sc_io_rman.rm_descr = "CPCHT I/O Memory";
	error = rman_init(&sc->sc_io_rman);
	if (error) {
		device_printf(dev, "rman_init() failed. error = %d\n", error);
		return (error);
	}

	/*
	 * Set up the resource manager and the HT->MPIC mapping. For cpcht,
	 * the ranges are properties of the child bridges, and this is also
	 * where we get the HT interrupts properties.
	 */

	/* I/O port mappings are usually not in the device tree */
	rman_manage_region(&sc->sc_io_rman, 0, CPCHT_IOPORT_SIZE - 1);

	bzero(sc->htirq_map, sizeof(sc->htirq_map));
	mtx_init(&sc->htirq_mtx, "cpcht irq", NULL, MTX_DEF);
	for (i = 0; i < 8; i++)
		sc->htirq_map[i].irq_type = IRQ_INTERNAL;
	for (child = OF_child(node); child != 0; child = OF_peer(child))
		cpcht_configure_htbridge(dev, child);

	/* Now make the mapping table available to the MPIC */
	cpcht_irqmap = sc->htirq_map;

	device_add_child(dev, "pci", device_get_unit(dev));

	return (bus_generic_attach(dev));
}

static void
cpcht_configure_htbridge(device_t dev, phandle_t child)
{
	struct cpcht_softc *sc;
	struct ofw_pci_register pcir;
	struct cpcht_range ranges[7], *rp;
	int nranges, ptr, nextptr;
	uint32_t vend, val;
	int i, nirq, irq;
	u_int f, s;

	sc = device_get_softc(dev);
	if (OF_getprop(child, "reg", &pcir, sizeof(pcir)) == -1)
		return;

	s = OFW_PCI_PHYS_HI_DEVICE(pcir.phys_hi);
	f = OFW_PCI_PHYS_HI_FUNCTION(pcir.phys_hi);

	/*
	 * Mark this slot is populated. The remote south bridge does
	 * not like us talking to unpopulated slots on the root bus.
	 */
	sc->sc_populated_slots |= (1 << s);

	/*
	 * Next grab this child bus's bus ranges.
	 */
	bzero(ranges, sizeof(ranges));
	nranges = OF_getprop(child, "ranges", ranges, sizeof(ranges));
	nranges /= sizeof(ranges[0]);
	
	ranges[6].pci_hi = 0;
	for (rp = ranges; rp < ranges + nranges && rp->pci_hi != 0; rp++) {
		switch (rp->pci_hi & OFW_PCI_PHYS_HI_SPACEMASK) {
		case OFW_PCI_PHYS_HI_SPACE_CONFIG:
			break;
		case OFW_PCI_PHYS_HI_SPACE_IO:
			rman_manage_region(&sc->sc_io_rman, rp->pci_lo,
			    rp->pci_lo + rp->size_lo - 1);
			break;
		case OFW_PCI_PHYS_HI_SPACE_MEM32:
			rman_manage_region(&sc->sc_mem_rman, rp->pci_lo,
			    rp->pci_lo + rp->size_lo - 1);
			break;
		case OFW_PCI_PHYS_HI_SPACE_MEM64:
			panic("64-bit CPCHT reserved memory!");
			break;
		}
	}

	/*
	 * Next build up any HT->MPIC mappings for this sub-bus. One would
	 * naively hope that enabling, disabling, and EOIing interrupts would
	 * cause the appropriate HT bus transactions to that effect. This is
	 * not the case.
	 *
	 * Instead, we have to muck about on the HT peer's root PCI bridges,
	 * figure out what interrupts they send, enable them, and cache
	 * the location of their WaitForEOI registers so that we can
	 * send EOIs later.
	 */

	/* All the devices we are interested in have caps */
	if (!(PCIB_READ_CONFIG(dev, 0, s, f, PCIR_STATUS, 2)
	    & PCIM_STATUS_CAPPRESENT))
		return;

	nextptr = PCIB_READ_CONFIG(dev, 0, s, f, PCIR_CAP_PTR, 1);
	while (nextptr != 0) {
		ptr = nextptr;
		nextptr = PCIB_READ_CONFIG(dev, 0, s, f,
		    ptr + PCICAP_NEXTPTR, 1);

		/* Find the HT IRQ capabilities */
		if (PCIB_READ_CONFIG(dev, 0, s, f,
		    ptr + PCICAP_ID, 1) != PCIY_HT)
			continue;

		val = PCIB_READ_CONFIG(dev, 0, s, f, ptr + PCIR_HT_COMMAND, 2);
		if ((val & PCIM_HTCMD_CAP_MASK) != PCIM_HTCAP_INTERRUPT)
			continue;

		/* Ask for the IRQ count */
		PCIB_WRITE_CONFIG(dev, 0, s, f, ptr + PCIR_HT_COMMAND, 0x1, 1);
		nirq = PCIB_READ_CONFIG(dev, 0, s, f, ptr + 4, 4);
		nirq = ((nirq >> 16) & 0xff) + 1;

		device_printf(dev, "%d HT IRQs on device %d.%d\n", nirq, s, f);

		for (i = 0; i < nirq; i++) {
			PCIB_WRITE_CONFIG(dev, 0, s, f,
			     ptr + PCIR_HT_COMMAND, 0x10 + (i << 1), 1);
			irq = PCIB_READ_CONFIG(dev, 0, s, f, ptr + 4, 4);

			/*
			 * Mask this interrupt for now.
			 */
			PCIB_WRITE_CONFIG(dev, 0, s, f, ptr + 4,
			    irq | HTAPIC_MASK, 4);
			irq = (irq >> 16) & 0xff;

			sc->htirq_map[irq].irq_type = IRQ_HT;
			sc->htirq_map[irq].ht_source = i;
			sc->htirq_map[irq].ht_base = sc->sc_data + 
			    (((((s & 0x1f) << 3) | (f & 0x07)) << 8) | (ptr));

			PCIB_WRITE_CONFIG(dev, 0, s, f,
			     ptr + PCIR_HT_COMMAND, 0x11 + (i << 1), 1);
			sc->htirq_map[irq].eoi_data =
			    PCIB_READ_CONFIG(dev, 0, s, f, ptr + 4, 4) |
			    0x80000000;

			/*
			 * Apple uses a non-compliant IO/APIC that differs
			 * in how we signal EOIs. Check if this device was 
			 * made by Apple, and act accordingly.
			 */
			vend = PCIB_READ_CONFIG(dev, 0, s, f,
			    PCIR_DEVVENDOR, 4);
			if ((vend & 0xffff) == 0x106b)
				sc->htirq_map[irq].apple_eoi = 
				 (sc->htirq_map[irq].ht_base - ptr) + 0x60;
		}
	}
}

static int
cpcht_maxslots(device_t dev)
{

	return (PCI_SLOTMAX);
}

static u_int32_t
cpcht_read_config(device_t dev, u_int bus, u_int slot, u_int func, u_int reg,
    int width)
{
	struct		cpcht_softc *sc;
	vm_offset_t	caoff;

	sc = device_get_softc(dev);
	caoff = sc->sc_data + 
		(((((slot & 0x1f) << 3) | (func & 0x07)) << 8) | reg);

	if (bus == 0 && (!(sc->sc_populated_slots & (1 << slot)) || func > 0))
		return (0xffffffff);

	if (bus > 0)
		caoff += 0x01000000UL + (bus << 16);

	switch (width) {
	case 1:
		return (in8rb(caoff));
		break;
	case 2:
		return (in16rb(caoff));
		break;
	case 4:
		return (in32rb(caoff));
		break;
	}

	return (0xffffffff);
}

static void
cpcht_write_config(device_t dev, u_int bus, u_int slot, u_int func,
    u_int reg, u_int32_t val, int width)
{
	struct		cpcht_softc *sc;
	vm_offset_t	caoff;

	sc = device_get_softc(dev);
	caoff = sc->sc_data + 
		(((((slot & 0x1f) << 3) | (func & 0x07)) << 8) | reg);

	if (bus == 0 && (!(sc->sc_populated_slots & (1 << slot)) || func > 0))
		return;

	if (bus > 0)
		caoff += 0x01000000UL + (bus << 16);

	switch (width) {
	case 1:
		out8rb(caoff, val);
		break;
	case 2:
		out16rb(caoff, val);
		break;
	case 4:
		out32rb(caoff, val);
		break;
	}
}

static int
cpcht_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	switch (which) {
	case PCIB_IVAR_DOMAIN:
		*result = device_get_unit(dev);
		return (0);
	case PCIB_IVAR_BUS:
		*result = 0;	/* Root bus */
		return (0);
	}

	return (ENOENT);
}

static phandle_t
cpcht_get_node(device_t bus, device_t dev)
{
	struct cpcht_softc *sc;

	sc = device_get_softc(bus);
	/* We only have one child, the PCI bus, which needs our own node. */
	return (sc->sc_node);
}

static int
cpcht_route_interrupt(device_t bus, device_t dev, int pin)
{
	return (pin);
}

static struct resource *
cpcht_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct			cpcht_softc *sc;
	struct			resource *rv;
	struct			rman *rm;
	int			needactivate;

	needactivate = flags & RF_ACTIVE;
	flags &= ~RF_ACTIVE;

	sc = device_get_softc(bus);

	switch (type) {
	case SYS_RES_IOPORT:
		end = min(end, start + count);
		rm = &sc->sc_io_rman;
		break;

	case SYS_RES_MEMORY:
		rm = &sc->sc_mem_rman;
		break;

	case SYS_RES_IRQ:
		return (bus_alloc_resource(bus, type, rid, start, end, count,
		    flags));

	default:
		device_printf(bus, "unknown resource request from %s\n",
		    device_get_nameunit(child));
		return (NULL);
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == NULL) {
		device_printf(bus, "failed to reserve resource for %s\n",
		    device_get_nameunit(child));
		return (NULL);
	}

	rman_set_rid(rv, *rid);

	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv) != 0) {
			device_printf(bus,
			    "failed to activate resource for %s\n",
			    device_get_nameunit(child));
			rman_release_resource(rv);
			return (NULL);
		}
	}

	return (rv);
}

static int
cpcht_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{
	void	*p;

	if (type == SYS_RES_IRQ)
		return (bus_activate_resource(bus, type, rid, res));

	if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		vm_offset_t start;

		start = (vm_offset_t)rman_get_start(res);

		if (type == SYS_RES_IOPORT)
			start += CPCHT_IOPORT_BASE;

		if (bootverbose)
			printf("cpcht mapdev: start %zx, len %ld\n", start,
			    rman_get_size(res));

		p = pmap_mapdev(start, (vm_size_t)rman_get_size(res));
		if (p == NULL)
			return (ENOMEM);
		rman_set_virtual(res, p);
		rman_set_bustag(res, &bs_le_tag);
		rman_set_bushandle(res, (u_long)p);
	}

	return (rman_activate_resource(res));
}

static int
cpcht_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{

	if (rman_get_flags(res) & RF_ACTIVE) {
		int error = bus_deactivate_resource(child, type, rid, res);
		if (error)
			return error;
	}

	return (rman_release_resource(res));
}

static int
cpcht_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{

	/*
	 * If this is a memory resource, unmap it.
	 */
	if ((type == SYS_RES_MEMORY) || (type == SYS_RES_IOPORT)) {
		u_int32_t psize;

		psize = rman_get_size(res);
		pmap_unmapdev((vm_offset_t)rman_get_virtual(res), psize);
	}

	return (rman_deactivate_resource(res));
}

static int
cpcht_alloc_msi(device_t dev, device_t child, int count, int maxcount,
    int *irqs)
{
	struct cpcht_softc *sc;
	int i, j;

	sc = device_get_softc(dev);
	j = 0;

	/* Bail if no MSI PIC yet */
	if (cpcht_msipic == 0)
		return (ENXIO);

	mtx_lock(&sc->htirq_mtx);
	for (i = 8; i < 124 - count; i++) {
		for (j = 0; j < count; j++) {
			if (sc->htirq_map[i+j].irq_type != IRQ_NONE)
				break;
		}
		if (j == count)
			break;

		i += j; /* We know there isn't a large enough run */
	}

	if (j != count) {
		mtx_unlock(&sc->htirq_mtx);
		return (ENXIO);
	}

	for (j = 0; j < count; j++) {
		irqs[j] = MAP_IRQ(cpcht_msipic, i+j);
		sc->htirq_map[i+j].irq_type = IRQ_MSI;
	}
	mtx_unlock(&sc->htirq_mtx);

	return (0);
}

static int
cpcht_release_msi(device_t dev, device_t child, int count, int *irqs)
{
	struct cpcht_softc *sc;
	int i;

	sc = device_get_softc(dev);

	mtx_lock(&sc->htirq_mtx);
	for (i = 0; i < count; i++)
		sc->htirq_map[irqs[i] & 0xff].irq_type = IRQ_NONE;
	mtx_unlock(&sc->htirq_mtx);

	return (0);
}

static int
cpcht_alloc_msix(device_t dev, device_t child, int *irq)
{
	struct cpcht_softc *sc;
	int i;

	sc = device_get_softc(dev);

	/* Bail if no MSI PIC yet */
	if (cpcht_msipic == 0)
		return (ENXIO);

	mtx_lock(&sc->htirq_mtx);
	for (i = 8; i < 124; i++) {
		if (sc->htirq_map[i].irq_type == IRQ_NONE) {
			sc->htirq_map[i].irq_type = IRQ_MSI;
			*irq = MAP_IRQ(cpcht_msipic, i);

			mtx_unlock(&sc->htirq_mtx);
			return (0);
		}
	}
	mtx_unlock(&sc->htirq_mtx);

	return (ENXIO);
}
	
static int
cpcht_release_msix(device_t dev, device_t child, int irq)
{
	struct cpcht_softc *sc;

	sc = device_get_softc(dev);

	mtx_lock(&sc->htirq_mtx);
	sc->htirq_map[irq & 0xff].irq_type = IRQ_NONE;
	mtx_unlock(&sc->htirq_mtx);

	return (0);
}

static int
cpcht_map_msi(device_t dev, device_t child, int irq, uint64_t *addr,
    uint32_t *data)
{
	device_t pcib;
	struct pci_devinfo *dinfo;
	struct pcicfg_ht *ht = NULL;

	for (pcib = child; pcib != dev; pcib =
	    device_get_parent(device_get_parent(pcib))) {
		dinfo = device_get_ivars(pcib);
		ht = &dinfo->cfg.ht;

		if (ht == NULL)
			continue;
	}

	if (ht == NULL)
		return (ENXIO);

	*addr = ht->ht_msiaddr;
	*data = irq & 0xff;

	return (0);
}

/*
 * Driver for the integrated MPIC on U3/U4 (CPC925/CPC945)
 */

static int	openpic_cpcht_probe(device_t);
static int	openpic_cpcht_attach(device_t);
static void	openpic_cpcht_config(device_t, u_int irq,
		    enum intr_trigger trig, enum intr_polarity pol);
static void	openpic_cpcht_enable(device_t, u_int irq, u_int vector);
static void	openpic_cpcht_unmask(device_t, u_int irq);
static void	openpic_cpcht_eoi(device_t, u_int irq);

static device_method_t  openpic_cpcht_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		openpic_cpcht_probe),
	DEVMETHOD(device_attach,	openpic_cpcht_attach),

	/* PIC interface */
	DEVMETHOD(pic_bind,		openpic_bind),
	DEVMETHOD(pic_config,		openpic_cpcht_config),
	DEVMETHOD(pic_dispatch,		openpic_dispatch),
	DEVMETHOD(pic_enable,		openpic_cpcht_enable),
	DEVMETHOD(pic_eoi,		openpic_cpcht_eoi),
	DEVMETHOD(pic_ipi,		openpic_ipi),
	DEVMETHOD(pic_mask,		openpic_mask),
	DEVMETHOD(pic_unmask,		openpic_cpcht_unmask),

	{ 0, 0 },
};

struct openpic_cpcht_softc {
	struct openpic_softc sc_openpic;

	struct mtx sc_ht_mtx;
};

static driver_t openpic_cpcht_driver = {
	"htpic",
	openpic_cpcht_methods,
	sizeof(struct openpic_cpcht_softc),
};

DRIVER_MODULE(openpic, unin, openpic_cpcht_driver, openpic_devclass, 0, 0);

static int
openpic_cpcht_probe(device_t dev)
{
	const char *type = ofw_bus_get_type(dev);

	if (strcmp(type, "open-pic") != 0)
                return (ENXIO);

	device_set_desc(dev, OPENPIC_DEVSTR);
	return (0);
}

static int
openpic_cpcht_attach(device_t dev)
{
	struct openpic_cpcht_softc *sc;
	phandle_t node;
	int err, irq;

	node = ofw_bus_get_node(dev);
	err = openpic_common_attach(dev, node);
	if (err != 0)
		return (err);

	/*
	 * The HT APIC stuff is not thread-safe, so we need a mutex to
	 * protect it.
	 */
	sc = device_get_softc(dev);
	mtx_init(&sc->sc_ht_mtx, "htpic", NULL, MTX_SPIN);

	/*
	 * Interrupts 0-3 are internally sourced and are level triggered
	 * active low. Interrupts 4-123 are connected to a pulse generator
	 * and should be programmed as edge triggered low-to-high.
	 * 
	 * IBM CPC945 Manual, Section 9.3.
	 */

	for (irq = 0; irq < 4; irq++)
		openpic_config(dev, irq, INTR_TRIGGER_LEVEL, INTR_POLARITY_LOW);
	for (irq = 4; irq < 124; irq++)
		openpic_config(dev, irq, INTR_TRIGGER_EDGE, INTR_POLARITY_LOW);

	/*
	 * Use this PIC for MSI only if it is the root PIC. This may not
	 * be necessary, but Linux does it, and I cannot find any U3 machines
	 * with MSI devices to test.
	 */
	if (dev == root_pic)
		cpcht_msipic = node;

	return (0);
}

static void
openpic_cpcht_config(device_t dev, u_int irq, enum intr_trigger trig,
    enum intr_polarity pol)
{
	struct openpic_cpcht_softc *sc;
	uint32_t ht_irq;

	/*
	 * The interrupt settings for the MPIC are completely determined
	 * by the internal wiring in the northbridge. Real changes to these
	 * settings need to be negotiated with the remote IO-APIC on the HT
	 * link.
	 */

	sc = device_get_softc(dev);

	if (cpcht_irqmap != NULL && irq < 128 &&
	    cpcht_irqmap[irq].ht_base > 0 && !cpcht_irqmap[irq].edge) {
		mtx_lock_spin(&sc->sc_ht_mtx);

		/* Program the data port */
		out8rb(cpcht_irqmap[irq].ht_base + PCIR_HT_COMMAND,
		    0x10 + (cpcht_irqmap[irq].ht_source << 1));

		/* Grab the IRQ config register */
		ht_irq = in32rb(cpcht_irqmap[irq].ht_base + 4);

		/* Mask the IRQ while we fiddle settings */
		out32rb(cpcht_irqmap[irq].ht_base + 4, ht_irq | HTAPIC_MASK);
		
		/* Program the interrupt sense */
		ht_irq &= ~(HTAPIC_TRIGGER_LEVEL | HTAPIC_REQUEST_EOI);
		if (trig == INTR_TRIGGER_EDGE) {
			cpcht_irqmap[irq].edge = 1;
		} else {
			cpcht_irqmap[irq].edge = 0;
			ht_irq |= HTAPIC_TRIGGER_LEVEL | HTAPIC_REQUEST_EOI;
		}
		out32rb(cpcht_irqmap[irq].ht_base + 4, ht_irq);

		mtx_unlock_spin(&sc->sc_ht_mtx);
	}
}

static void
openpic_cpcht_enable(device_t dev, u_int irq, u_int vec)
{
	struct openpic_cpcht_softc *sc;
	uint32_t ht_irq;

	openpic_enable(dev, irq, vec);

	sc = device_get_softc(dev);

	if (cpcht_irqmap != NULL && irq < 128 &&
	    cpcht_irqmap[irq].ht_base > 0) {
		mtx_lock_spin(&sc->sc_ht_mtx);

		/* Program the data port */
		out8rb(cpcht_irqmap[irq].ht_base + PCIR_HT_COMMAND,
		    0x10 + (cpcht_irqmap[irq].ht_source << 1));

		/* Unmask the interrupt */
		ht_irq = in32rb(cpcht_irqmap[irq].ht_base + 4);
		ht_irq &= ~HTAPIC_MASK;
		out32rb(cpcht_irqmap[irq].ht_base + 4, ht_irq);

		mtx_unlock_spin(&sc->sc_ht_mtx);
	}
		
	openpic_cpcht_eoi(dev, irq);
}

static void
openpic_cpcht_unmask(device_t dev, u_int irq)
{
	struct openpic_cpcht_softc *sc;
	uint32_t ht_irq;

	openpic_unmask(dev, irq);

	sc = device_get_softc(dev);

	if (cpcht_irqmap != NULL && irq < 128 &&
	    cpcht_irqmap[irq].ht_base > 0) {
		mtx_lock_spin(&sc->sc_ht_mtx);

		/* Program the data port */
		out8rb(cpcht_irqmap[irq].ht_base + PCIR_HT_COMMAND,
		    0x10 + (cpcht_irqmap[irq].ht_source << 1));

		/* Unmask the interrupt */
		ht_irq = in32rb(cpcht_irqmap[irq].ht_base + 4);
		ht_irq &= ~HTAPIC_MASK;
		out32rb(cpcht_irqmap[irq].ht_base + 4, ht_irq);

		mtx_unlock_spin(&sc->sc_ht_mtx);
	}

	openpic_cpcht_eoi(dev, irq);
}

static void
openpic_cpcht_eoi(device_t dev, u_int irq)
{
	struct openpic_cpcht_softc *sc;
	uint32_t off, mask;

	if (irq == 255)
		return;

	sc = device_get_softc(dev);

	if (cpcht_irqmap != NULL && irq < 128 &&
	    cpcht_irqmap[irq].ht_base > 0 && !cpcht_irqmap[irq].edge) {
		/* If this is an HT IRQ, acknowledge it at the remote APIC */

		if (cpcht_irqmap[irq].apple_eoi) {
			off = (cpcht_irqmap[irq].ht_source >> 3) & ~3;
			mask = 1 << (cpcht_irqmap[irq].ht_source & 0x1f);
			out32rb(cpcht_irqmap[irq].apple_eoi + off, mask);
		} else {
			mtx_lock_spin(&sc->sc_ht_mtx);

			out8rb(cpcht_irqmap[irq].ht_base + PCIR_HT_COMMAND,
			    0x11 + (cpcht_irqmap[irq].ht_source << 1));
			out32rb(cpcht_irqmap[irq].ht_base + 4,
			    cpcht_irqmap[irq].eoi_data);

			mtx_unlock_spin(&sc->sc_ht_mtx);
		}
	}

	openpic_eoi(dev, irq);
}
