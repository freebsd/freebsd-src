/*
 * Copyright (c) 1999, M. Warner Losh.
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
 * $FreeBSD: src/sys/pccard/pccard_nbk.c,v 1.15.2.1 2000/05/23 03:56:58 imp Exp $
 */

/*
 * This file contains various kludges to allow the legacy pccard system to
 * work in the newbus system until the pccard system can be converted
 * wholesale to newbus.  As that is a while off, I'm providing this glue to
 * allow newbus drivers to have pccard attachments.
 *
 * We do *NOT* implement ISA ivars at all.  We are not an isa bus, and drivers
 * that abuse isa_{set,get}_* must be fixed in order to work with pccard.
 * We use ivars for something else anyway, so it becomes fairly awkward
 * to do so.
 *
 * Here's a summary of the glue that we do to make things work.
 *
 * First, we have pccard node in the device and driver trees.  The pccard
 * device lives in the instance tree attached to the nexus.  The pccard
 * attachments will be attached to that node.  This allows one to pass things
 * up the tree that terminates at the nexus, like other buses.  The pccard
 * code will create a device instance for each of the drivers that are to
 * be attached.
 *
 * These compatibility nodes are called pccnbk.  PCCard New Bus Kludge.
 */

#include "opt_bus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/select.h>
#include <sys/types.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <i386/isa/isa_device.h>
#include <pccard/cardinfo.h>
#include <pccard/slot.h>
#include <pccard/pccard_nbk.h>

#include <dev/pccard/pccardvar.h>
#include <net/ethernet.h>

#include "card_if.h"

devclass_t	pccard_devclass;

#define PCCARD_NPORT	2
#define PCCARD_NMEM	5
#define PCCARD_NIRQ	1
#define PCCARD_NDRQ	0

#define PCCARD_DEVINFO(d) (struct pccard_devinfo *) device_get_ivars(d)

static int
pccard_probe(device_t dev)
{
	device_set_desc(dev, "PC Card bus -- kludge version");
	return 0;
}

static void
pccard_print_resources(struct resource_list *rl, const char *name, int type,
    int count, const char *format)
{
	struct resource_list_entry *rle;
	int printed;
	int i;

	printed = 0;
	for (i = 0; i < count; i++) {
		rle = resource_list_find(rl, type, i);
		if (rle) {
			if (printed == 0)
				printf(" %s ", name);
			else if (printed > 0)
				printf(",");
			printed++;
			printf(format, rle->start);
			if (rle->count > 1) {
				printf("-");
				printf(format, rle->start + rle->count - 1);
			}
		} else if (i > 3) {
			/* check the first few regardless */
			break;
		}
	}
}

static int
pccard_print_child(device_t dev, device_t child)
{
	struct pccard_devinfo *devi = PCCARD_DEVINFO(child);
	struct resource_list *rl = &devi->resources;
	int retval = 0;

	retval += bus_print_child_header(dev, child);
	retval += printf(" at");

	if (devi) {
		pccard_print_resources(rl, "port", SYS_RES_IOPORT,
		    PCCARD_NPORT, "%#lx");
		pccard_print_resources(rl, "iomem", SYS_RES_MEMORY,
		    PCCARD_NMEM, "%#lx");
		pccard_print_resources(rl, "irq", SYS_RES_IRQ, PCCARD_NIRQ,
		    "%ld");
		pccard_print_resources(rl, "drq", SYS_RES_DRQ, PCCARD_NDRQ,
		    "%ld");
		retval += printf(" slot %d", devi->slt->slotnum);
	}

	retval += bus_print_child_footer(dev, child);

	return (retval);
}

static int
pccard_set_resource(device_t dev, device_t child, int type, int rid,
    u_long start, u_long count)
{
	struct pccard_devinfo *devi = PCCARD_DEVINFO(child);
	struct resource_list *rl = &devi->resources;

	if (type != SYS_RES_IOPORT && type != SYS_RES_MEMORY
	    && type != SYS_RES_IRQ && type != SYS_RES_DRQ)
		return EINVAL;
	if (rid < 0)
		return EINVAL;
	if (type == SYS_RES_IOPORT && rid >= PCCARD_NPORT)
		return EINVAL;
	if (type == SYS_RES_MEMORY && rid >= PCCARD_NMEM)
		return EINVAL;
	if (type == SYS_RES_IRQ && rid >= PCCARD_NIRQ)
		return EINVAL;
	if (type == SYS_RES_DRQ && rid >= PCCARD_NDRQ)
		return EINVAL;

	resource_list_add(rl, type, rid, start, start + count - 1, count);

	return 0;
}

static int
pccard_get_resource(device_t dev, device_t child, int type, int rid,
    u_long *startp, u_long *countp)
{
	struct pccard_devinfo *devi = PCCARD_DEVINFO(child);
	struct resource_list *rl = &devi->resources;
	struct resource_list_entry *rle;

	rle = resource_list_find(rl, type, rid);
	if (!rle)
		return ENOENT;
	
	if (startp)
		*startp = rle->start;
	if (countp)
		*countp = rle->count;

	return 0;
}

static void
pccard_delete_resource(device_t dev, device_t child, int type, int rid)
{
	struct pccard_devinfo *devi = PCCARD_DEVINFO(child);
	struct resource_list *rl = &devi->resources;
	resource_list_delete(rl, type, rid);
}

static struct resource *
pccard_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	/*
	 * Consider adding a resource definition. We allow rid 0 for
	 * irq, 0-3 for memory and 0-1 for ports
	 */
	int passthrough = (device_get_parent(child) != bus);
	int isdefault;
	struct pccard_devinfo *devi = device_get_ivars(child);
	struct resource_list *rl = &devi->resources;
	struct resource_list_entry *rle;
	struct resource *res;

	if (start == 0 && end == ~0 && type == SYS_RES_MEMORY && count != 1) {
		start = 0xd0000;
		end = 0xdffff;
	}
	isdefault = (start == 0UL && end == ~0UL);
	if (!passthrough && !isdefault) {
		rle = resource_list_find(rl, type, *rid);
		if (!rle) {
			if (*rid < 0)
				return 0;
			switch (type) {
			case SYS_RES_IRQ:
				if (*rid >= PCCARD_NIRQ)
					return 0;
				break;
			case SYS_RES_DRQ:
				if (*rid >= PCCARD_NDRQ)
					return 0;
				break;
			case SYS_RES_MEMORY:
				if (*rid >= PCCARD_NMEM)
					return 0;
				break;
			case SYS_RES_IOPORT:
				if (*rid >= PCCARD_NPORT)
					return 0;
				break;
			default:
				return 0;
			}
			resource_list_add(rl, type, *rid, start, end, count);
		}
	}
	res = resource_list_alloc(rl, bus, child, type, rid, start, end,
	    count, flags);
	return (res);
}

static int
pccard_release_resource(device_t bus, device_t child, int type, int rid,
		     struct resource *r)
{
	struct pccard_devinfo *devi = PCCARD_DEVINFO(child);
	struct resource_list *rl = &devi->resources;
	return resource_list_release(rl, bus, child, type, rid, r);
}

static int
pccard_read_ivar(device_t bus, device_t child, int which, u_char *result)
{
	struct pccard_devinfo *devi = PCCARD_DEVINFO(child);
	
	switch (which) {
	case PCCARD_IVAR_ETHADDR:
		bcopy(devi->misc, result, ETHER_ADDR_LEN);
		return 0;
	}
	return ENOENT;
}

/* Pass card requests up to pcic.  This may mean a bad design XXX */

static int
pccard_set_res_flags(device_t bus, device_t child, int restype, int rid,
    u_long value)
{
	return CARD_SET_RES_FLAGS(device_get_parent(bus), child, restype,
	    rid, value);
}

static int
pccard_get_res_flags(device_t bus, device_t child, int restype, int rid,
    u_long *value)
{
	return CARD_GET_RES_FLAGS(device_get_parent(bus), child, restype,
	    rid, value);
}

static int
pccard_set_memory_offset(device_t bus, device_t child, int rid, 
    u_int32_t offset)
{
	return CARD_SET_MEMORY_OFFSET(device_get_parent(bus), child, rid,
	    offset);
}

static device_method_t pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pccard_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	pccard_suspend),
	DEVMETHOD(device_resume,	pccard_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	pccard_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
	DEVMETHOD(bus_alloc_resource,	pccard_alloc_resource),
	DEVMETHOD(bus_release_resource,	pccard_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_set_resource,	pccard_set_resource),
	DEVMETHOD(bus_get_resource,	pccard_get_resource),
	DEVMETHOD(bus_delete_resource,	pccard_delete_resource),
	DEVMETHOD(bus_read_ivar,	pccard_read_ivar),

	/* Card interface */
	DEVMETHOD(card_set_res_flags,	pccard_set_res_flags),
	DEVMETHOD(card_get_res_flags,	pccard_get_res_flags),
	DEVMETHOD(card_set_memory_offset, pccard_set_memory_offset),

	{ 0, 0 }
};

static driver_t pccard_driver = {
	"pccard",
	pccard_methods,
	1,			/* no softc */
};

DRIVER_MODULE(pccard, pcic, pccard_driver, pccard_devclass, 0, 0);
DRIVER_MODULE(pccard, pc98pcic, pccard_driver, pccard_devclass, 0, 0);
DRIVER_MODULE(pccard, cbb, pccard_driver, pccard_devclass, 0, 0);
