/*-
 * Copyright (c) 1999 Doug Rabson
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
 *	$Id$
 */
/*
 * Modifications for Intel architecture by Garrett A. Wollman.
 * Copyright 1998 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Parts of the ISA bus implementation common to all architectures.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <machine/resource.h>

#include <isa/isavar.h>
#include <isa/isa_common.h>
#ifdef __alpha__		/* XXX workaround a stupid warning */
#include <alpha/isa/isavar.h>
#endif

MALLOC_DEFINE(M_ISADEV, "isadev", "ISA device");

static devclass_t isa_devclass;

/*
 * At 'probe' time, we add all the devices which we know about to the
 * bus.  The generic attach routine will probe and attach them if they
 * are alive.
 */
static int
isa_probe(device_t dev)
{
	device_set_desc(dev, "ISA bus");
	isa_init();		/* Allow machdep code to initialise */
	return bus_generic_probe(dev);
}

extern device_t isa_bus_device;

static int
isa_attach(device_t dev)
{
	/*
	 * Arrange for bus_generic_attach(dev) to be called later.
	 */
	isa_bus_device = dev;
	return 0;
}

/*
 * Add a new child with default ivars.
 */
static device_t
isa_add_child(device_t dev, device_t place, const char *name, int unit)
{
	struct	isa_device *idev;

	idev = malloc(sizeof(struct isa_device), M_ISADEV, M_NOWAIT);
	if (!idev)
		return 0;
	bzero(idev, sizeof *idev);

	resource_list_init(&idev->id_resources);
	idev->id_flags = 0;

	if (place)
		return device_add_child_after(dev, place, name, unit, idev);
	else
		return device_add_child(dev, name, unit, idev);
}

static void
isa_print_resources(struct resource_list *rl, const char *name, int type,
		    const char *format)
{
	struct resource_list_entry *rle0 = resource_list_find(rl, type, 0);
	struct resource_list_entry *rle1 = resource_list_find(rl, type, 1);

	if (rle0 || rle1) {
		printf(" %s ", name);
		if (rle0) {
			printf(format, rle0->start);
			if (rle0->count > 1) {
				printf("-");
				printf(format, rle0->start + rle0->count - 1);
			}
		}
		if (rle1) {
			if (rle0)
				printf(",");
			printf(format, rle1->start);
			if (rle1->count > 1) {
				printf("-");
				printf(format, rle1->start + rle1->count - 1);
			}
		}
	}
}

static void
isa_print_child(device_t bus, device_t dev)
{
	struct	isa_device *idev = DEVTOISA(dev);
	struct resource_list *rl = &idev->id_resources;

	if (SLIST_FIRST(rl) || idev->id_flags)
		printf(" at");
	
	isa_print_resources(rl, "port", SYS_RES_IOPORT, "%#lx");
	isa_print_resources(rl, "iomem", SYS_RES_MEMORY, "%#lx");
	isa_print_resources(rl, "irq", SYS_RES_IRQ, "%ld");
	isa_print_resources(rl, "drq", SYS_RES_DRQ, "%ld");
	if (idev->id_flags)
		printf(" flags %#x", idev->id_flags);

	printf(" on %s%d",
	       device_get_name(bus), device_get_unit(bus));
}

static int
isa_read_ivar(device_t bus, device_t dev, int index, uintptr_t * result)
{
	struct isa_device* idev = DEVTOISA(dev);
	struct resource_list *rl = &idev->id_resources;
	struct resource_list_entry *rle;

	switch (index) {
	case ISA_IVAR_PORT_0:
		rle = resource_list_find(rl, SYS_RES_IOPORT, 0);
		if (rle)
			*result = rle->start;
		else
			*result = -1;
		break;

	case ISA_IVAR_PORT_1:
		rle = resource_list_find(rl, SYS_RES_IOPORT, 1);
		if (rle)
			*result = rle->start;
		else
			*result = -1;
		break;

	case ISA_IVAR_PORTSIZE_0:
		rle = resource_list_find(rl, SYS_RES_IOPORT, 0);
		if (rle)
			*result = rle->count;
		else
			*result = 0;
		break;

	case ISA_IVAR_PORTSIZE_1:
		rle = resource_list_find(rl, SYS_RES_IOPORT, 1);
		if (rle)
			*result = rle->count;
		else
			*result = 0;
		break;

	case ISA_IVAR_MADDR_0:
		rle = resource_list_find(rl, SYS_RES_MEMORY, 0);
		if (rle)
			*result = rle->start;
		else
			*result = -1;
		break;

	case ISA_IVAR_MADDR_1:
		rle = resource_list_find(rl, SYS_RES_MEMORY, 1);
		if (rle)
			*result = rle->start;
		else
			*result = -1;
		break;

	case ISA_IVAR_MSIZE_0:
		rle = resource_list_find(rl, SYS_RES_MEMORY, 0);
		if (rle)
			*result = rle->count;
		else
			*result = 0;
		break;

	case ISA_IVAR_MSIZE_1:
		rle = resource_list_find(rl, SYS_RES_MEMORY, 1);
		if (rle)
			*result = rle->count;
		else
			*result = 0;
		break;

	case ISA_IVAR_IRQ_0:
		rle = resource_list_find(rl, SYS_RES_IRQ, 0);
		if (rle)
			*result = rle->start;
		else
			*result = -1;
		break;

	case ISA_IVAR_IRQ_1:
		rle = resource_list_find(rl, SYS_RES_IRQ, 1);
		if (rle)
			*result = rle->start;
		else
			*result = -1;
		break;

	case ISA_IVAR_DRQ_0:
		rle = resource_list_find(rl, SYS_RES_DRQ, 0);
		if (rle)
			*result = rle->start;
		else
			*result = -1;
		break;

	case ISA_IVAR_DRQ_1:
		rle = resource_list_find(rl, SYS_RES_DRQ, 1);
		if (rle)
			*result = rle->start;
		else
			*result = -1;
		break;

	case ISA_IVAR_FLAGS:
		*result = idev->id_flags;
		break;
	}
	return ENOENT;
}

static int
isa_write_ivar(device_t bus, device_t dev,
	       int index, uintptr_t value)
{
	struct isa_device* idev = DEVTOISA(dev);

	switch (index) {
	case ISA_IVAR_PORT_0:
	case ISA_IVAR_PORT_1:
	case ISA_IVAR_PORTSIZE_0:
	case ISA_IVAR_PORTSIZE_1:
	case ISA_IVAR_MADDR_0:
	case ISA_IVAR_MADDR_1:
	case ISA_IVAR_MSIZE_0:
	case ISA_IVAR_MSIZE_1:
	case ISA_IVAR_IRQ_0:
	case ISA_IVAR_IRQ_1:
	case ISA_IVAR_DRQ_0:
	case ISA_IVAR_DRQ_1:
		return EINVAL;

	case ISA_IVAR_FLAGS:
		idev->id_flags = value;
		break;
	default:
		return (ENOENT);
	}
	return (0);
}

static int
isa_set_resource(device_t dev, device_t child, int type, int rid,
		 u_long start, u_long count)
{
	struct isa_device* idev = DEVTOISA(child);
	struct resource_list *rl = &idev->id_resources;

	if (type != SYS_RES_IOPORT && type != SYS_RES_MEMORY
	    && type != SYS_RES_IRQ && type != SYS_RES_DRQ)
		return EINVAL;
	if (rid < 0 || rid > 1)
		return EINVAL;

	resource_list_add(rl, type, rid, start, start + count - 1, count);

	return 0;
}

static int
isa_get_resource(device_t dev, device_t child, int type, int rid,
		 u_long *startp, u_long *countp)
{
	struct isa_device* idev = DEVTOISA(child);
	struct resource_list *rl = &idev->id_resources;
	struct resource_list_entry *rle;

	rle = resource_list_find(rl, type, rid);
	if (!rle)
		return ENOENT;
	
	*startp = rle->start;
	*countp = rle->count;

	return 0;
}

static device_method_t isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		isa_probe),
	DEVMETHOD(device_attach,	isa_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_add_child,	isa_add_child),
	DEVMETHOD(bus_print_child,	isa_print_child),
	DEVMETHOD(bus_read_ivar,	isa_read_ivar),
	DEVMETHOD(bus_write_ivar,	isa_write_ivar),
	DEVMETHOD(bus_alloc_resource,	isa_alloc_resource),
	DEVMETHOD(bus_release_resource,	isa_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	isa_setup_intr),
	DEVMETHOD(bus_teardown_intr,	isa_teardown_intr),

	/* ISA interface */
	DEVMETHOD(isa_set_resource,	isa_set_resource),
	DEVMETHOD(isa_get_resource,	isa_get_resource),

	{ 0, 0 }
};

static driver_t isa_driver = {
	"isa",
	isa_methods,
	1,			/* no softc */
};

/*
 * ISA can be attached to a PCI-ISA bridge or directly to the nexus.
 */
DRIVER_MODULE(isa, isab, isa_driver, isa_devclass, 0, 0);
#ifdef __i386__
DRIVER_MODULE(isa, nexus, isa_driver, isa_devclass, 0, 0);
#endif
