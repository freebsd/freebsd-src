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
 * $FreeBSD: src/sys/isa/isa_common.c,v 1.16.2.1 2000/09/16 15:49:52 roger Exp $
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

static int	isa_print_child		__P((device_t, device_t));

MALLOC_DEFINE(M_ISADEV, "isadev", "ISA device");

static devclass_t isa_devclass;
static int isa_running;

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
	return 0;
}

extern device_t isa_bus_device;

static int
isa_attach(device_t dev)
{
	/*
	 * Arrange for isa_probe_children(dev) to be called later. XXX
	 */
	isa_bus_device = dev;
	return 0;
}

/*
 * Find a working set of memory regions for a child using the ranges
 * in *config  and return the regions in *result. Returns non-zero if
 * a set of ranges was found.
 */
static int
isa_find_memory(device_t child,
		struct isa_config *config,
		struct isa_config *result)
{
	int success, i;
	struct resource *res[ISA_NMEM];

	/*
	 * First clear out any existing resource definitions.
	 */
	for (i = 0; i < ISA_NMEM; i++) {
		bus_delete_resource(child, SYS_RES_MEMORY, i);
		res[i] = NULL;
	}

	success = 1;
	result->ic_nmem = config->ic_nmem;
	for (i = 0; i < config->ic_nmem; i++) {
		u_int32_t start, end, size, align;
		for (start = config->ic_mem[i].ir_start,
			     end = config->ic_mem[i].ir_end,
			     size = config->ic_mem[i].ir_size,
			     align = config->ic_mem[i].ir_align;
		     start + size - 1 <= end;
		     start += align) {
			bus_set_resource(child, SYS_RES_MEMORY, i,
					 start, size);
			res[i] = bus_alloc_resource(child,
						    SYS_RES_MEMORY, &i,
						    0, ~0, 1, 0 /* !RF_ACTIVE */);
			if (res[i]) {
				result->ic_mem[i].ir_start = start;
				result->ic_mem[i].ir_end = start + size - 1;
				result->ic_mem[i].ir_size = size;
				result->ic_mem[i].ir_align = align;
				break;
			}
		}

		/*
		 * If we didn't find a place for memory range i, then 
		 * give up now.
		 */
		if (!res[i]) {
			success = 0;
			break;
		}
	}

	for (i = 0; i < ISA_NMEM; i++) {
		if (res[i])
			bus_release_resource(child, SYS_RES_MEMORY,
					     i, res[i]);
	}

	return success;
}

/*
 * Find a working set of port regions for a child using the ranges
 * in *config  and return the regions in *result. Returns non-zero if
 * a set of ranges was found.
 */
static int
isa_find_port(device_t child,
	      struct isa_config *config,
	      struct isa_config *result)
{
	int success, i;
	struct resource *res[ISA_NPORT];

	/*
	 * First clear out any existing resource definitions.
	 */
	for (i = 0; i < ISA_NPORT; i++) {
		bus_delete_resource(child, SYS_RES_IOPORT, i);
		res[i] = NULL;
	}

	success = 1;
	result->ic_nport = config->ic_nport;
	for (i = 0; i < config->ic_nport; i++) {
		u_int32_t start, end, size, align;
		for (start = config->ic_port[i].ir_start,
			     end = config->ic_port[i].ir_end,
			     size = config->ic_port[i].ir_size,
			     align = config->ic_port[i].ir_align;
		     start + size - 1 <= end;
		     start += align) {
			bus_set_resource(child, SYS_RES_IOPORT, i,
					 start, size);
			res[i] = bus_alloc_resource(child,
						    SYS_RES_IOPORT, &i,
						    0, ~0, 1, 0 /* !RF_ACTIVE */);
			if (res[i]) {
				result->ic_port[i].ir_start = start;
				result->ic_port[i].ir_end = start + size - 1;
				result->ic_port[i].ir_size = size;
				result->ic_port[i].ir_align = align;
				break;
			}
		}

		/*
		 * If we didn't find a place for port range i, then 
		 * give up now.
		 */
		if (!res[i]) {
			success = 0;
			break;
		}
	}

	for (i = 0; i < ISA_NPORT; i++) {
		if (res[i])
			bus_release_resource(child, SYS_RES_IOPORT,
					     i, res[i]);
	}

	return success;
}

/*
 * Return the index of the first bit in the mask (or -1 if mask is empty.
 */
static int
find_first_bit(u_int32_t mask)
{
	return ffs(mask) - 1;
}

/*
 * Return the index of the next bit in the mask, or -1 if there are no more.
 */
static int
find_next_bit(u_int32_t mask, int bit)
{
	bit++;
	while (bit < 32 && !(mask & (1 << bit)))
		bit++;
	if (bit != 32)
		return bit;
	return -1;
}

/*
 * Find a working set of irqs for a child using the masks in *config
 * and return the regions in *result. Returns non-zero if a set of
 * irqs was found.
 */
static int
isa_find_irq(device_t child,
	     struct isa_config *config,
	     struct isa_config *result)
{
	int success, i;
	struct resource *res[ISA_NIRQ];

	/*
	 * First clear out any existing resource definitions.
	 */
	for (i = 0; i < ISA_NIRQ; i++) {
		bus_delete_resource(child, SYS_RES_IRQ, i);
		res[i] = NULL;
	}

	success = 1;
	result->ic_nirq = config->ic_nirq;
	for (i = 0; i < config->ic_nirq; i++) {
		u_int32_t mask = config->ic_irqmask[i];
		int irq;
		for (irq = find_first_bit(mask);
		     irq != -1;
		     irq = find_next_bit(mask, irq)) {
			bus_set_resource(child, SYS_RES_IRQ, i,
					 irq, 1);
			res[i] = bus_alloc_resource(child,
						    SYS_RES_IRQ, &i,
						    0, ~0, 1, 0 /* !RF_ACTIVE */ );
			if (res[i]) {
				result->ic_irqmask[i] = (1 << irq);
				break;
			}
		}

		/*
		 * If we didn't find a place for irq range i, then 
		 * give up now.
		 */
		if (!res[i]) {
			success = 0;
			break;
		}
	}

	for (i = 0; i < ISA_NIRQ; i++) {
		if (res[i])
			bus_release_resource(child, SYS_RES_IRQ,
					     i, res[i]);
	}

	return success;
}

/*
 * Find a working set of drqs for a child using the masks in *config
 * and return the regions in *result. Returns non-zero if a set of
 * drqs was found.
 */
static int
isa_find_drq(device_t child,
	     struct isa_config *config,
	     struct isa_config *result)
{
	int success, i;
	struct resource *res[ISA_NDRQ];

	/*
	 * First clear out any existing resource definitions.
	 */
	for (i = 0; i < ISA_NDRQ; i++) {
		bus_delete_resource(child, SYS_RES_DRQ, i);
		res[i] = NULL;
	}

	success = 1;
	result->ic_ndrq = config->ic_ndrq;
	for (i = 0; i < config->ic_ndrq; i++) {
		u_int32_t mask = config->ic_drqmask[i];
		int drq;
		for (drq = find_first_bit(mask);
		     drq != -1;
		     drq = find_next_bit(mask, drq)) {
			bus_set_resource(child, SYS_RES_DRQ, i,
					 drq, 1);
			res[i] = bus_alloc_resource(child,
						    SYS_RES_DRQ, &i,
						    0, ~0, 1, 0 /* !RF_ACTIVE */);
			if (res[i]) {
				result->ic_drqmask[i] = (1 << drq);
				break;
			}
		}

		/*
		 * If we didn't find a place for drq range i, then 
		 * give up now.
		 */
		if (!res[i]) {
			success = 0;
			break;
		}
	}

	for (i = 0; i < ISA_NDRQ; i++) {
		if (res[i])
			bus_release_resource(child, SYS_RES_DRQ,
					     i, res[i]);
	}

	return success;
}

/*
 * Attempt to find a working set of resources for a device. Return
 * non-zero if a working configuration is found.
 */
static int
isa_assign_resources(device_t child)
{
	struct isa_device *idev = DEVTOISA(child);
	struct isa_config_entry *ice;
	struct isa_config config;

	bzero(&config, sizeof config);
	TAILQ_FOREACH(ice, &idev->id_configs, ice_link) {
		if (!isa_find_memory(child, &ice->ice_config, &config))
			continue;
		if (!isa_find_port(child, &ice->ice_config, &config))
			continue;
		if (!isa_find_irq(child, &ice->ice_config, &config))
			continue;
		if (!isa_find_drq(child, &ice->ice_config, &config))
			continue;

		/*
		 * A working configuration was found enable the device 
		 * with this configuration.
		 */
		if (idev->id_config_cb) {
			idev->id_config_cb(idev->id_config_arg,
					   &config, 1);
			return 1;
		}
	}

	/*
	 * Disable the device.
	 */
	bus_print_child_header(device_get_parent(child), child);
	printf(" can't assign resources\n");
	if (bootverbose)
	    isa_print_child(device_get_parent(child), child);
	bzero(&config, sizeof config);
	if (idev->id_config_cb)
		idev->id_config_cb(idev->id_config_arg, &config, 0);
	device_disable(child);

	return 0;
}

/*
 * Called after other devices have initialised to probe for isa devices.
 */
void
isa_probe_children(device_t dev)
{
	device_t *children;
	int nchildren, i;

	/*
	 * Create all the children by calling driver's identify methods.
	 */
	bus_generic_probe(dev);

	if (device_get_children(dev, &children, &nchildren))
		return;

	/*
	 * First disable all pnp devices so that they don't get
	 * matched by legacy probes.
	 */
	if (bootverbose)
		printf("isa_probe_children: disabling PnP devices\n");
	for (i = 0; i < nchildren; i++) {
		device_t child = children[i];
		struct isa_device *idev = DEVTOISA(child);
		struct isa_config config;

		bzero(&config, sizeof config);
		if (idev->id_config_cb)
			idev->id_config_cb(idev->id_config_arg, &config, 0);
	}

	/*
	 * Next probe all non-pnp devices so that they claim their
	 * resources first.
	 */
	if (bootverbose)
		printf("isa_probe_children: probing non-PnP devices\n");
	for (i = 0; i < nchildren; i++) {
		device_t child = children[i];
		struct isa_device *idev = DEVTOISA(child);

		if (TAILQ_FIRST(&idev->id_configs))
			continue;

		device_probe_and_attach(child);
	}

	/*
	 * Finally assign resource to pnp devices and probe them.
	 */
	if (bootverbose)
		printf("isa_probe_children: probing PnP devices\n");
	for (i = 0; i < nchildren; i++) {
		device_t child = children[i];
		struct isa_device* idev = DEVTOISA(child);

		if (!TAILQ_FIRST(&idev->id_configs))
			continue;

		if (isa_assign_resources(child)) {
			struct resource_list *rl = &idev->id_resources;
			struct resource_list_entry *rle;

			device_probe_and_attach(child);

			/*
			 * Claim any unallocated resources to keep other
			 * devices from using them.
			 */
			SLIST_FOREACH(rle, rl, link) {
				if (!rle->res) {
					int rid = rle->rid;
					resource_list_alloc(rl, dev, child,
							    rle->type,
							    &rid,
							    0, ~0, 1,
							    0 /* !RF_ACTIVE */);
				}
			}
		}
	}

	free(children, M_TEMP);

	isa_running = 1;
}

/*
 * Add a new child with default ivars.
 */
static device_t
isa_add_child(device_t dev, int order, const char *name, int unit)
{
	device_t child;
	struct	isa_device *idev;

	idev = malloc(sizeof(struct isa_device), M_ISADEV, M_NOWAIT);
	if (!idev)
		return 0;
	bzero(idev, sizeof *idev);

	resource_list_init(&idev->id_resources);
	TAILQ_INIT(&idev->id_configs);

	child = device_add_child_ordered(dev, order, name, unit);
 	device_set_ivars(child, idev);

	return child;
}

static int
isa_print_resources(struct resource_list *rl, const char *name, int type,
		    int count, const char *format)
{
	struct resource_list_entry *rle;
	int printed;
	int i, retval = 0;;

	printed = 0;
	for (i = 0; i < count; i++) {
		rle = resource_list_find(rl, type, i);
		if (rle) {
			if (printed == 0)
				retval += printf(" %s ", name);
			else if (printed > 0)
				retval += printf(",");
			printed++;
			retval += printf(format, rle->start);
			if (rle->count > 1) {
				retval += printf("-");
				retval += printf(format,
						 rle->start + rle->count - 1);
			}
		} else if (i > 3) {
			/* check the first few regardless */
			break;
		}
	}
	return retval;
}

static int
isa_print_all_resources(device_t dev)
{
	struct	isa_device *idev = DEVTOISA(dev);
	struct resource_list *rl = &idev->id_resources;
	int retval = 0;

	if (SLIST_FIRST(rl) || device_get_flags(dev))
		retval += printf(" at");
	
	retval += isa_print_resources(rl, "port", SYS_RES_IOPORT,
				      ISA_NPORT, "%#lx");
	retval += isa_print_resources(rl, "iomem", SYS_RES_MEMORY,
				      ISA_NMEM, "%#lx");
	retval += isa_print_resources(rl, "irq", SYS_RES_IRQ,
				      ISA_NIRQ, "%ld");
	retval += isa_print_resources(rl, "drq", SYS_RES_DRQ,
				      ISA_NDRQ, "%ld");
	if (device_get_flags(dev))
		retval += printf(" flags %#x", device_get_flags(dev));

	return retval;
}

static int
isa_print_child(device_t bus, device_t dev)
{
	int retval = 0;

	retval += bus_print_child_header(bus, dev);
	retval += isa_print_all_resources(dev);
	retval += bus_print_child_footer(bus, dev);

	return (retval);
}

static void
isa_probe_nomatch(device_t dev, device_t child)
{
	if (bootverbose) {
		bus_print_child_header(dev, child);
		printf(" failed to probe");
		isa_print_all_resources(child);
		bus_print_child_footer(dev, child);
	}
                                      
	return;
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

	case ISA_IVAR_VENDORID:
		*result = idev->id_vendorid;
		break;

	case ISA_IVAR_SERIAL:
		*result = idev->id_serial;
		break;

	case ISA_IVAR_LOGICALID:
		*result = idev->id_logicalid;
		break;

	case ISA_IVAR_COMPATID:
		*result = idev->id_compatid;
		break;

	default:
		return ENOENT;
	}

	return 0;
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

	case ISA_IVAR_VENDORID:
		idev->id_vendorid = value;
		break;

	case ISA_IVAR_SERIAL:
		idev->id_serial = value;
		break;

	case ISA_IVAR_LOGICALID:
		idev->id_logicalid = value;
		break;

	case ISA_IVAR_COMPATID:
		idev->id_compatid = value;
		break;

	default:
		return (ENOENT);
	}

	return (0);
}

/*
 * Free any resources which the driver missed or which we were holding for
 * it (see isa_probe_children).
 */
static void
isa_child_detached(device_t dev, device_t child)
{
	struct isa_device* idev = DEVTOISA(child);
	struct resource_list *rl = &idev->id_resources;
	struct resource_list_entry *rle;

	if (TAILQ_FIRST(&idev->id_configs)) {
		/*
		 * Claim any unallocated resources to keep other
		 * devices from using them.
		 */
		SLIST_FOREACH(rle, rl, link) {
			if (!rle->res) {
				int rid = rle->rid;
				resource_list_alloc(rl, dev, child,
						    rle->type,
						    &rid, 0, ~0, 1, 0);
			}
		}
	}
}

static void
isa_driver_added(device_t dev, driver_t *driver)
{
	device_t *children;
	int nchildren, i;

	/*
	 * Don't do anything if drivers are dynamically
	 * added during autoconfiguration (cf. ymf724).
	 * since that would end up calling identify
	 * twice.
	 */
	if (!isa_running)
		return;

	DEVICE_IDENTIFY(driver, dev);
	if (device_get_children(dev, &children, &nchildren))
		return;

	for (i = 0; i < nchildren; i++) {
		device_t child = children[i];
		struct isa_device *idev = DEVTOISA(child);
		struct resource_list *rl = &idev->id_resources;
		struct resource_list_entry *rle;

		if (device_get_state(child) != DS_NOTPRESENT)
			continue;
		if (!device_is_enabled(child))
			continue;

		/*
		 * Free resources which we were holding on behalf of
		 * the device.
		 */
		SLIST_FOREACH(rle, &idev->id_resources, link) {
			if (rle->res)
				resource_list_release(rl, dev, child,
						      rle->type,
						      rle->rid,
						      rle->res);
		}

		if (TAILQ_FIRST(&idev->id_configs))
			if (!isa_assign_resources(child))
				continue;

		device_probe_and_attach(child);

		if (TAILQ_FIRST(&idev->id_configs)) {
			/*
			 * Claim any unallocated resources to keep other
			 * devices from using them.
			 */
			SLIST_FOREACH(rle, rl, link) {
				if (!rle->res) {
					int rid = rle->rid;
					resource_list_alloc(rl, dev, child,
							    rle->type,
							    &rid,
							    0, ~0, 1,
							    0 /* !RF_ACTIVE */);
				}
			}
		}
	}

	free(children, M_TEMP);
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
	if (rid < 0)
		return EINVAL;
	if (type == SYS_RES_IOPORT && rid >= ISA_NPORT)
		return EINVAL;
	if (type == SYS_RES_MEMORY && rid >= ISA_NMEM)
		return EINVAL;
	if (type == SYS_RES_IRQ && rid >= ISA_NIRQ)
		return EINVAL;
	if (type == SYS_RES_DRQ && rid >= ISA_NDRQ)
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
	
	if (startp)
		*startp = rle->start;
	if (countp)
		*countp = rle->count;

	return 0;
}

static void
isa_delete_resource(device_t dev, device_t child, int type, int rid)
{
	struct isa_device* idev = DEVTOISA(child);
	struct resource_list *rl = &idev->id_resources;
	resource_list_delete(rl, type, rid);
}

static int
isa_add_config(device_t dev, device_t child,
	       int priority, struct isa_config *config)
{
	struct isa_device* idev = DEVTOISA(child);
	struct isa_config_entry *newice, *ice;

	newice = malloc(sizeof *ice, M_DEVBUF, M_NOWAIT);
	if (!newice)
		return ENOMEM;

	newice->ice_priority = priority;
	newice->ice_config = *config;
	
	TAILQ_FOREACH(ice, &idev->id_configs, ice_link) {
		if (ice->ice_priority > priority)
			break;
	}
	if (ice)
		TAILQ_INSERT_BEFORE(ice, newice, ice_link);
	else
		TAILQ_INSERT_TAIL(&idev->id_configs, newice, ice_link);

	return 0;
}

static void
isa_set_config_callback(device_t dev, device_t child,
			isa_config_cb *fn, void *arg)
{
	struct isa_device* idev = DEVTOISA(child);

	idev->id_config_cb = fn;
	idev->id_config_arg = arg;
}

static int
isa_pnp_probe(device_t dev, device_t child, struct isa_pnp_id *ids)
{
	struct isa_device* idev = DEVTOISA(child);

	if (!idev->id_vendorid)
		return ENOENT;

	while (ids->ip_id) {
		/*
		 * Really ought to support >1 compat id per device.
		 */
		if (idev->id_logicalid == ids->ip_id
		    || idev->id_compatid == ids->ip_id) {
			if (ids->ip_desc)
				device_set_desc(child, ids->ip_desc);
			return 0;
		}
		ids++;
	}

	return ENXIO;
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
	DEVMETHOD(bus_probe_nomatch,	isa_probe_nomatch),	
	DEVMETHOD(bus_read_ivar,	isa_read_ivar),
	DEVMETHOD(bus_write_ivar,	isa_write_ivar),
	DEVMETHOD(bus_child_detached,	isa_child_detached),
	DEVMETHOD(bus_driver_added,	isa_driver_added),
	DEVMETHOD(bus_alloc_resource,	isa_alloc_resource),
	DEVMETHOD(bus_release_resource,	isa_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	isa_setup_intr),
	DEVMETHOD(bus_teardown_intr,	isa_teardown_intr),
	DEVMETHOD(bus_set_resource,	isa_set_resource),
	DEVMETHOD(bus_get_resource,	isa_get_resource),
	DEVMETHOD(bus_delete_resource,	isa_delete_resource),

	/* ISA interface */
	DEVMETHOD(isa_add_config,	isa_add_config),
	DEVMETHOD(isa_set_config_callback, isa_set_config_callback),
	DEVMETHOD(isa_pnp_probe,	isa_pnp_probe),

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

