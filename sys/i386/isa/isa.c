/*-
 * Copyright (c) 1998 Doug Rabson
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
 *	$Id: isa.c,v 1.123 1999/05/08 18:15:49 peter Exp $
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

MALLOC_DEFINE(M_ISADEV, "isadev", "ISA device");

/*
 * The structure used to attach devices to the isa bus.
 */
struct isa_device {
	int		id_port[ISA_NPORT_IVARS];
	u_short		id_portsize[ISA_NPORT_IVARS];
	vm_offset_t	id_maddr[ISA_NMEM_IVARS];
	vm_size_t	id_msize[ISA_NMEM_IVARS];
	int		id_irq[ISA_NIRQ_IVARS];
	int		id_drq[ISA_NDRQ_IVARS];
	int		id_flags;
	struct resource	*id_portres[ISA_NPORT_IVARS];
	struct resource	*id_memres[ISA_NMEM_IVARS];
	struct resource	*id_irqres[ISA_NIRQ_IVARS];
	struct resource	*id_drqres[ISA_NDRQ_IVARS];
};

#define DEVTOISA(dev)	((struct isa_device *) device_get_ivars(dev))

static devclass_t isa_devclass;

static void
isa_add_device(device_t dev, const char *name, int unit)
{
	struct	isa_device *idev;
	device_t	child;
	int		sensitive, t;
	static	device_t last_sensitive;

	/* device-specific flag overrides any wildcard */
	sensitive = 0;
	if (resource_int_value(name, unit, "sensitive", &sensitive) != 0)
		resource_int_value(name, -1, "sensitive", &sensitive);

	idev = malloc(sizeof(struct isa_device), M_ISADEV, M_NOWAIT);
	if (!idev)
		return;
	bzero(idev, sizeof *idev);

	if (resource_int_value(name, unit, "port", &t) == 0)
		idev->id_port[0] = t;
	else
		idev->id_port[0] = -1;
	idev->id_port[1] = 0;

	if (resource_int_value(name, unit, "portsize", &t) == 0)
		idev->id_portsize[0] = t;
	else
		idev->id_portsize[0] = 0;
	idev->id_portsize[1] = 0;

	if (resource_int_value(name, unit, "maddr", &t) == 0)
		idev->id_maddr[0] = t;
	else
		idev->id_maddr[0] = 0;
	idev->id_maddr[1] = 0;

	if (resource_int_value(name, unit, "msize", &t) == 0)
		idev->id_msize[0] = t;
	else
		idev->id_msize[0] = 0;
	idev->id_msize[1] = 0;

	if (resource_int_value(name, unit, "flags", &t) == 0)
		idev->id_flags = t;
	else
		idev->id_flags = 0;

	if (resource_int_value(name, unit, "irq", &t) == 0)
		idev->id_irq[0] = t;
	else
		idev->id_irq[0] = -1;
	idev->id_irq[1] = -1;

	if (resource_int_value(name, unit, "drq", &t) == 0)
		idev->id_drq[0] = t;
	else
		idev->id_drq[0] = -1;
	idev->id_drq[1] = -1;

	if (sensitive)
		child = device_add_child_after(dev, last_sensitive, name, 
					       unit, idev);
	else
		child = device_add_child(dev, name, unit, idev);
	if (child == 0)
		return;
	else if (sensitive)
		last_sensitive = child;

	if (resource_int_value(name, unit, "disabled", &t) == 0 && t != 0)
		device_disable(child);
}

/*
 * At 'probe' time, we add all the devices which we know about to the
 * bus.  The generic attach routine will probe and attach them if they
 * are alive.
 */
static int
isa_probe(device_t dev)
{
	int i;
	static char buf[] = "isaXXX";

	device_set_desc(dev, "ISA bus");

	/*
	 * Add all devices configured to be attached to isa0.
	 */
	sprintf(buf, "isa%d", device_get_unit(dev));
	for (i = resource_query_string(-1, "at", buf);
	     i != -1;
	     i = resource_query_string(i, "at", buf)) {
		if (strcmp(resource_query_name(i), "atkbd") == 0)
			continue;	/* old GENERIC kludge */
		isa_add_device(dev, resource_query_name(i),
			       resource_query_unit(i));
	}

	/*
	 * and isa?
	 */
	for (i = resource_query_string(-1, "at", "isa");
	     i != -1;
	     i = resource_query_string(i, "at", "isa")) {
		if (strcmp(resource_query_name(i), "atkbd") == 0)
			continue;	/* old GENERIC kludge */
		isa_add_device(dev, resource_query_name(i),
			       resource_query_unit(i));
	}

	isa_wrap_old_drivers();

	return 0;
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

static void
isa_print_child(device_t bus, device_t dev)
{
	struct	isa_device *id = DEVTOISA(dev);

	if (id->id_port[0] > 0 || id->id_port[1] > 0
	    || id->id_maddr[0] > 0 || id->id_maddr[1] > 0
	    || id->id_irq[0] >= 0 || id->id_irq[1] >= 0
	    || id->id_drq[0] >= 0 || id->id_drq[1] >= 0)
		printf(" at");
	if (id->id_port[0] > 0 && id->id_port[1] > 0) {
		printf(" ports %#x", (u_int)id->id_port[0]);
		if (id->id_portsize[0] > 1)
			printf("-%#x", (u_int)(id->id_port[0] 
					       + id->id_portsize[0] - 1));
		printf(" and %#x", (u_int)id->id_port[1]);
		if (id->id_portsize[1] > 1)
			printf("-%#x", (u_int)(id->id_port[1] 
					       + id->id_portsize[1] - 1));
	} else if (id->id_port[0] > 0) {
		printf(" port %#x", (u_int)id->id_port[0]);
		if (id->id_portsize[0] > 1)
			printf("-%#x", (u_int)(id->id_port[0]
					       + id->id_portsize[0] - 1));
	} else if (id->id_port[1] > 0) {
		printf(" port %#x", (u_int)id->id_port[1]);
		if (id->id_portsize[1] > 1)
			printf("-%#x", (u_int)(id->id_port[1]
					       + id->id_portsize[1] - 1));
	}
	if (id->id_maddr[0] && id->id_maddr[1]) {
		printf(" iomem %#x", (u_int)id->id_maddr[0]);
		if (id->id_msize[0])
			printf("-%#x", (u_int)(id->id_maddr[0] 
					       + id->id_msize[0] - 1));
		printf(" and %#x", (u_int)id->id_maddr[1]);
		if (id->id_msize[1])
			printf("-%#x", (u_int)(id->id_maddr[1] 
					       + id->id_msize[1] - 1));
	} else if (id->id_maddr[0]) {
		printf(" iomem %#x", (u_int)id->id_maddr[0]);
		if (id->id_msize[0])
			printf("-%#x", (u_int)(id->id_maddr[0]
					       + id->id_msize[0] - 1));
	} else if (id->id_maddr[1]) {
		printf(" iomem %#x", (u_int)id->id_maddr[1]);
		if (id->id_msize[1])
			printf("-%#x", (u_int)(id->id_maddr[1]
					       + id->id_msize[1] - 1));
	}
	if (id->id_irq[0] >= 0 && id->id_irq[1] >= 0)
		printf(" irqs %d and %d", id->id_irq[0], id->id_irq[1]);
	else if (id->id_irq[0] >= 0)
		printf(" irq %d", id->id_irq[0]);
	else if (id->id_irq[1] >= 0)
		printf(" irq %d", id->id_irq[1]);
	if (id->id_drq[0] >= 0 && id->id_drq[1] >= 0)
		printf(" drqs %d and %d", id->id_drq[0], id->id_drq[1]);
	else if (id->id_drq[0] >= 0)
		printf(" drq %d", id->id_drq[0]);
	else if (id->id_drq[1] >= 0)
		printf(" drq %d", id->id_drq[1]);

	if (id->id_flags)
		printf(" flags %#x", id->id_flags);

	printf(" on %s%d",
	       device_get_name(bus), device_get_unit(bus));
}

static int
isa_read_ivar(device_t bus, device_t dev, int index, uintptr_t * result)
{
	struct isa_device* idev = DEVTOISA(dev);

	switch (index) {
	case ISA_IVAR_PORT_0:
		*result = idev->id_port[0];
		break;
	case ISA_IVAR_PORT_1:
		*result = idev->id_port[1];
		break;
	case ISA_IVAR_PORTSIZE_0:
		*result = idev->id_portsize[0];
		break;
	case ISA_IVAR_PORTSIZE_1:
		*result = idev->id_portsize[1];
		break;
	case ISA_IVAR_MADDR_0:
		*result = idev->id_maddr[0];
		break;
	case ISA_IVAR_MADDR_1:
		*result = idev->id_maddr[1];
		break;
	case ISA_IVAR_MSIZE_0:
		*result = idev->id_msize[0];
		break;
	case ISA_IVAR_MSIZE_1:
		*result = idev->id_msize[1];
		break;
	case ISA_IVAR_IRQ_0:
		*result = idev->id_irq[0];
		break;
	case ISA_IVAR_IRQ_1:
		*result = idev->id_irq[1];
		break;
	case ISA_IVAR_DRQ_0:
		*result = idev->id_drq[0];
		break;
	case ISA_IVAR_DRQ_1:
		*result = idev->id_drq[1];
		break;
	case ISA_IVAR_FLAGS:
		*result = idev->id_flags;
		break;
	}
	return ENOENT;
}

/*
 * XXX -- this interface is pretty much irrelevant in the presence of
 * BUS_ALLOC_RESOURCE / BUS_RELEASE_RESOURCE (at least for the ivars which
 * are defined at this point).
 */
static int
isa_write_ivar(device_t bus, device_t dev,
	       int index, uintptr_t value)
{
	struct isa_device* idev = DEVTOISA(dev);

	switch (index) {
	case ISA_IVAR_PORT_0:
		idev->id_port[0] = value;
		break;
	case ISA_IVAR_PORT_1:
		idev->id_port[1] = value;
		break;
	case ISA_IVAR_PORTSIZE_0:
		idev->id_portsize[0] = value;
		break;
	case ISA_IVAR_PORTSIZE_1:
		idev->id_portsize[1] = value;
		break;
	case ISA_IVAR_MADDR_0:
		idev->id_maddr[0] = value;
		break;
	case ISA_IVAR_MADDR_1:
		idev->id_maddr[1] = value;
		break;
	case ISA_IVAR_MSIZE_0:
		idev->id_msize[0] = value;
		break;
	case ISA_IVAR_MSIZE_1:
		idev->id_msize[1] = value;
		break;
	case ISA_IVAR_IRQ_0:
		idev->id_irq[0] = value;
		break;
	case ISA_IVAR_IRQ_1:
		idev->id_irq[1] = value;
		break;
	case ISA_IVAR_DRQ_0:
		idev->id_drq[0] = value;
		break;
	case ISA_IVAR_DRQ_1:
		idev->id_drq[1] = value;
		break;
	case ISA_IVAR_FLAGS:
		idev->id_flags = value;
		break;
	default:
		return (ENOENT);
	}
	return (0);
}

/*
 * This implementation simply passes the request up to the parent
 * bus, which in our case is the special i386 nexus, substituting any
 * configured values if the caller defaulted.  We can get away with
 * this because there is no special mapping for ISA resources on an Intel
 * platform.  When porting this code to another architecture, it may be
 * necessary to interpose a mapping layer here.
 */
static struct resource *
isa_alloc_resource(device_t bus, device_t child, int type, int *rid,
		   u_long start, u_long end, u_long count, u_int flags)
{
	int	isdefault;
	struct	resource *rv, **rvp = 0;
	struct	isa_device *id = DEVTOISA(child);

	if (child) {
		/*
		 * If this is our child, then use the isa_device to find 
		 * defaults and to record results.
		 */
		if (device_get_devclass(device_get_parent(child)) == isa_devclass)
			id = DEVTOISA(child);
		else
			id = NULL;
	} else
		id = NULL;
	isdefault = (id != NULL && start == 0UL && end == ~0UL && *rid == 0);
	if (*rid > 1)
		return 0;

	switch (type) {
	case SYS_RES_IRQ:
		if (isdefault && id->id_irq[0] >= 0) {
			start = id->id_irq[0];
			end = id->id_irq[0];
			count = 1;
		}
		if (id)
			rvp = &id->id_irqres[*rid];
		break;

	case SYS_RES_DRQ:
		if (isdefault && id->id_drq[0] >= 0) {
			start = id->id_drq[0];
			end = id->id_drq[0];
			count = 1;
		}
		if (id)
			rvp = &id->id_drqres[*rid];
		break;

	case SYS_RES_MEMORY:
		if (isdefault && id->id_maddr[0]) {
			start = id->id_maddr[0];
			count = max(count, (u_long)id->id_msize[0]);
			end = id->id_maddr[0] + count;
		}
		if (id)
			rvp = &id->id_memres[*rid];
		break;

	case SYS_RES_IOPORT:
		if (isdefault && id->id_port[0]) {
			start = id->id_port[0];
			count = max(count, (u_long)id->id_portsize[0]);
			end = id->id_port[0] + count;
		}
		if (id)
			rvp = &id->id_portres[*rid];
		break;

	default:
		return 0;
	}

	/*
	 * If the client attempts to reallocate a resource without
	 * releasing what was there previously, die horribly so that
	 * he knows how he !@#$ed up.
	 */
	if (rvp && *rvp != 0)
		panic("%s%d: (%d, %d) not free for %s%d\n",
		      device_get_name(bus), device_get_unit(bus),
		      type, *rid, 
		      device_get_name(child), device_get_unit(child));

	/*
	 * nexus_alloc_resource had better not change *rid...
	 */
	rv = BUS_ALLOC_RESOURCE(device_get_parent(bus), child, type, rid,
				start, end, count, flags);
	if (rvp && (*rvp = rv) != 0) {
		switch (type) {
		case SYS_RES_MEMORY:
			id->id_maddr[*rid] = rv->r_start;
			id->id_msize[*rid] = count;
			break;
		case SYS_RES_IOPORT:
			id->id_port[*rid] = rv->r_start;
			id->id_portsize[*rid] = count;
			break;
		case SYS_RES_IRQ:
			id->id_irq[*rid] = rv->r_start;
			break;
		case SYS_RES_DRQ:
			id->id_drq[*rid] = rv->r_start;
			break;
		}
	}
	return rv;
}

static int
isa_release_resource(device_t bus, device_t child, int type, int rid,
		     struct resource *r)
{
	int	rv;
	struct	isa_device *id = DEVTOISA(child);

	if (rid > 1)
		return EINVAL;

	switch (type) {
	case SYS_RES_IRQ:
	case SYS_RES_DRQ:
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		break;
	default:
		return (ENOENT);
	}

	rv = BUS_RELEASE_RESOURCE(device_get_parent(bus), child, type, rid, r);

	if (rv == 0) {
		switch (type) {
		case SYS_RES_IRQ:
			id->id_irqres[rid] = 0;
			break;

		case SYS_RES_DRQ:
			id->id_drqres[rid] = 0;
			break;

		case SYS_RES_MEMORY:
			id->id_memres[rid] = 0;
			break;

		case SYS_RES_IOPORT:
			id->id_portres[rid] = 0;
			break;

		default:
			return ENOENT;
		}
	}

	return rv;
}

/*
 * We can't use the bus_generic_* versions of these methods because those
 * methods always pass the bus param as the requesting device, and we need
 * to pass the child (the i386 nexus knows about this and is prepared to
 * deal).
 */
static int
isa_setup_intr(device_t bus, device_t child, struct resource *r,
	       void (*ihand)(void *), void *arg, void **cookiep)
{
	return (BUS_SETUP_INTR(device_get_parent(bus), child, r, ihand, arg,
			       cookiep));
}

static int
isa_teardown_intr(device_t bus, device_t child, struct resource *r,
		  void *cookie)
{
	return (BUS_TEARDOWN_INTR(device_get_parent(bus), child, r, cookie));
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
	DEVMETHOD(bus_print_child,	isa_print_child),
	DEVMETHOD(bus_read_ivar,	isa_read_ivar),
	DEVMETHOD(bus_write_ivar,	isa_write_ivar),
	DEVMETHOD(bus_alloc_resource,	isa_alloc_resource),
	DEVMETHOD(bus_release_resource,	isa_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	isa_setup_intr),
	DEVMETHOD(bus_teardown_intr,	isa_teardown_intr),

	{ 0, 0 }
};

static driver_t isa_driver = {
	"isa",
	isa_methods,
	DRIVER_TYPE_MISC,
	1,			/* no softc */
};

/*
 * ISA can be attached to a PCI-ISA bridge or directly to the nexus.
 */
DRIVER_MODULE(isa, isab, isa_driver, isa_devclass, 0, 0);
DRIVER_MODULE(isa, nexus, isa_driver, isa_devclass, 0, 0);
