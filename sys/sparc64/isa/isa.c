/*-
 * Copyright (c) 1998 Doug Rabson
 * Copyright (c) 2001 Thomas Moestl <tmm@FreeBSD.org>
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
 *	from: FreeBSD: src/sys/alpha/isa/isa.c,v 1.26 2001/07/11
 *
 * $FreeBSD$
 */

#include "opt_ofw_pci.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/rman.h>
#include <sys/interrupt.h>

#include <isa/isareg.h>
#include <isa/isavar.h>
#include <isa/isa_common.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <ofw/ofw_pci.h>
#include <ofw/openfirm.h>

#include <machine/intr_machdep.h>
#include <machine/ofw_bus.h>
#include <machine/resource.h>

#include <sparc64/pci/ofw_pci.h>
#include <sparc64/isa/ofw_isa.h>

/* There can be only one ISA bus, so it is safe to use globals. */
bus_space_tag_t isa_io_bt = NULL;
bus_space_handle_t isa_io_hdl;
bus_space_tag_t isa_mem_bt = NULL;
bus_space_handle_t isa_mem_hdl;

u_int64_t isa_io_base;
u_int64_t isa_io_limit;
u_int64_t isa_mem_base;
u_int64_t isa_mem_limit;

device_t isa_bus_device;

static phandle_t isab_node;
static ofw_pci_intr_t isa_ino[8];

#ifdef OFW_NEWPCI
struct ofw_bus_iinfo isa_iinfo;
#endif

/*
 * XXX: This is really partly partly PCI-specific, but unfortunately is
 * differently enough to have to duplicate it here...
 */
#define	ISAB_RANGE_PHYS(r)						\
	(((u_int64_t)(r)->phys_mid << 32) | (u_int64_t)(r)->phys_lo)
#define	ISAB_RANGE_SPACE(r)	(((r)->phys_hi >> 24) & 0x03)

#define	ISAR_SPACE_IO		0x01
#define	ISAR_SPACE_MEM		0x02

#define INRANGE(x, start, end)	((x) >= (start) && (x) <= (end))

static int isa_route_intr_res(device_t, u_long, u_long);

intrmask_t
isa_irq_pending(void)
{
	intrmask_t pending;
	int i;

	/* XXX: Is this correct? */
	for (i = 7, pending = 0; i >= 0; i--) {
		pending <<= 1;
		if (isa_ino[i] != PCI_INVALID_IRQ) {
			pending |= (OFW_PCI_INTR_PENDING(isa_bus_device,
			    isa_ino[i]) == 0) ? 0 : 1;
		}
	}
	return (pending);
}

void
isa_init(device_t dev)
{
	device_t bridge;
	phandle_t node;
	ofw_isa_intr_t ino;
#ifndef OFW_NEWPCI
	ofw_pci_intr_t rino;
#endif
	struct isa_ranges *br;
	int nbr, i;

	/* The parent of the bus must be a PCI-ISA bridge. */
	bridge = device_get_parent(dev);
#ifdef OFW_NEWPCI
	isab_node = ofw_pci_get_node(bridge);
#else
	isab_node = ofw_pci_node(bridge);
#endif
	nbr = OF_getprop_alloc(isab_node, "ranges", sizeof(*br), (void **)&br);
	if (nbr <= 0)
		panic("isa_init: cannot get bridge range property");

#ifdef OFW_NEWPCI
	ofw_bus_setup_iinfo(isab_node, &isa_iinfo, sizeof(ofw_isa_intr_t));
#endif

	/*
	 * This is really a bad kludge; however, it is needed to provide
	 * isa_irq_pending(), which is unfortunately still used by some
	 * drivers.
	 */
	for (i = 0; i < 8; i++)
		isa_ino[i] = PCI_INVALID_IRQ;
	for (node = OF_child(isab_node); node != 0; node = OF_peer(node)) {
		if (OF_getprop(node, "interrupts", &ino, sizeof(ino)) == -1)
			continue;
		if (ino > 7)
			panic("isa_init: XXX: ino too large");
#ifdef OFW_NEWPCI
		isa_ino[ino] = ofw_isa_route_intr(bridge, node, &isa_iinfo,
		    ino);
#else
		rino = ofw_bus_route_intr(node, ino, ofw_pci_orb_callback, dev);
		isa_ino[ino] = rino == ORIR_NOTFOUND ? PCI_INVALID_IRQ : rino;
#endif
	}

	for (nbr -= 1; nbr >= 0; nbr--) {
		switch(ISAB_RANGE_SPACE(br + nbr)) {
		case ISAR_SPACE_IO:
			/* This is probably always 0. */
			isa_io_base = ISAB_RANGE_PHYS(&br[nbr]);
			isa_io_limit = br[nbr].size;
			isa_io_hdl = OFW_PCI_GET_BUS_HANDLE(bridge,
			    SYS_RES_IOPORT, isa_io_base, &isa_io_bt);
			break;
		case ISAR_SPACE_MEM:
			/* This is probably always 0. */
			isa_mem_base = ISAB_RANGE_PHYS(&br[nbr]);
			isa_mem_limit = br[nbr].size;
			isa_mem_hdl = OFW_PCI_GET_BUS_HANDLE(bridge,
			    SYS_RES_MEMORY, isa_mem_base, &isa_mem_bt);
			break;
		}
	}
	free(br, M_OFWPROP);
}

static int
isa_route_intr_res(device_t bus, u_long start, u_long end)
{
	int res;

	if (start != end) {
		panic("isa_route_intr_res: allocation of interrupt range not "
		    "supported (0x%lx - 0x%lx)", start, end);
	}
	if (start > 7)
		panic("isa_route_intr_res: start out of isa range");
	res = isa_ino[start];
	if (res == PCI_INVALID_IRQ)
		device_printf(bus, "could not map interrupt %d\n", res);
	return (res);
}

struct resource *
isa_alloc_resource(device_t bus, device_t child, int type, int *rid,
		   u_long start, u_long end, u_long count, u_int flags)
{
	/*
	 * Consider adding a resource definition. We allow rid 0-1 for
	 * irq and drq, 0-3 for memory and 0-7 for ports which is
	 * sufficient for isapnp.
	 */
	int passthrough = (device_get_parent(child) != bus);
	int isdefault = (start == 0UL && end == ~0UL);
	struct isa_device* idev = DEVTOISA(child);
	struct resource_list *rl = &idev->id_resources;
	struct resource_list_entry *rle;
	u_long base, limit;

	if (!passthrough && !isdefault) {
		rle = resource_list_find(rl, type, *rid);
		if (!rle) {
			if (*rid < 0)
				return 0;
			switch (type) {
			case SYS_RES_IRQ:
				if (*rid >= ISA_NIRQ)
					return 0;
				break;
			case SYS_RES_DRQ:
				if (*rid >= ISA_NDRQ)
					return 0;
				break;
			case SYS_RES_MEMORY:
				if (*rid >= ISA_NMEM)
					return 0;
				break;
			case SYS_RES_IOPORT:
				if (*rid >= ISA_NPORT)
					return 0;
				break;
			default:
				return 0;
			}
			resource_list_add(rl, type, *rid, start, end, count);
		}
	}

	/*
	 * Add the base, change default allocations to be between base and
	 * limit, and reject allocations if a resource type is not enabled.
	 */
	base = limit = 0;
	switch(type) {
	case SYS_RES_MEMORY:
		if (isa_mem_bt == NULL)
			return (NULL);
		base = isa_mem_base;
		limit = base + isa_mem_limit;
		break;
	case SYS_RES_IOPORT:
		if (isa_io_bt == NULL)
			return (NULL);
		base = isa_io_base;
		limit = base + isa_io_limit;
		break;
	case SYS_RES_IRQ:
		if (isdefault && passthrough)
			panic("isa_alloc_resource: cannot pass through default "
			    "irq allocation");
		if (!isdefault) {
			start = end = isa_route_intr_res(bus, start, end);
			if (start == PCI_INVALID_IRQ)
				return (NULL);
		}
		break;
	default:
		panic("isa_alloc_resource: unsupported resource type %d", type);
	}
	if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		start = ulmin(start + base, limit);
		end = ulmin(end + base, limit);
	}

	/*
	 * This inlines a modified resource_list_alloc(); this is needed
	 * because the resources need to have offsets added to them, which
	 * cannot be done beforehand without patching the resource list entries
	 * (which is ugly).
	 */
	if (passthrough) {
		return (BUS_ALLOC_RESOURCE(device_get_parent(bus), child,
		    type, rid, start, end, count, flags));
	}

	rle = resource_list_find(rl, type, *rid);
	if (rle == NULL)
		return (NULL);		/* no resource of that type/rid */

	if (rle->res != NULL)
		panic("isa_alloc_resource: resource entry is busy");

	if (isdefault) {
		start = rle->start;
		count = ulmax(count, rle->count);
		end = ulmax(rle->end, start + count - 1);
		switch (type) {
		case SYS_RES_MEMORY:
		case SYS_RES_IOPORT:
			start += base;
			end += base;
			if (!INRANGE(start, base, limit) ||
			    !INRANGE(end, base, limit))
				return (NULL);
			break;
		case SYS_RES_IRQ:
			start = end = isa_route_intr_res(bus, start, end);
			if (start == PCI_INVALID_IRQ)
				return (NULL);
			break;
		}
	}

	rle->res = BUS_ALLOC_RESOURCE(device_get_parent(bus), child,
	    type, rid, start, end, count, flags);

	/*
	 * Record the new range.
	 */
	if (rle->res != NULL) {
		rle->start = rman_get_start(rle->res) - base;
		rle->end = rman_get_end(rle->res) - base;
		rle->count = count;
	}

	return (rle->res);
}

int
isa_release_resource(device_t bus, device_t child, int type, int rid,
		     struct resource *res)
{
	struct isa_device* idev = DEVTOISA(child);
	struct resource_list *rl = &idev->id_resources;

	return (resource_list_release(rl, bus, child, type, rid, res));
}

int
isa_setup_intr(device_t dev, device_t child,
	       struct resource *irq, int flags,
	       driver_intr_t *intr, void *arg, void **cookiep)
{

	/*
	 * Just pass through. This is going to be handled by either one of
	 * the parent PCI buses or the nexus device.
	 * The interrupt was routed at allocation time.
	 */
	return (BUS_SETUP_INTR(device_get_parent(dev), child, irq, flags, intr,
	    arg, cookiep));
}

int
isa_teardown_intr(device_t dev, device_t child,
		  struct resource *irq, void *cookie)
{

	return (BUS_TEARDOWN_INTR(device_get_parent(dev), child, irq, cookie));
}
