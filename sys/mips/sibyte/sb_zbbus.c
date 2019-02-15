/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2009 Neelkanth Natu
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
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/resource.h>
#include <machine/intr_machdep.h>

#include "sb_scd.h"

static MALLOC_DEFINE(M_INTMAP, "sb1250 intmap", "Sibyte 1250 Interrupt Mapper");

static struct mtx zbbus_intr_mtx;
MTX_SYSINIT(zbbus_intr_mtx, &zbbus_intr_mtx, "zbbus_intr_mask/unmask lock",
	    MTX_SPIN);

/*
 * This array holds the mapping between a MIPS hard interrupt and the
 * interrupt sources that feed into that it.
 */
static uint64_t hardint_to_intsrc_mask[NHARD_IRQS];

struct sb_intmap {
	int intsrc;		/* interrupt mapper register number (0 - 63) */
	int hardint;		/* cpu interrupt from 0 to NHARD_IRQS - 1 */

	/*
	 * The device that the interrupt belongs to. Note that multiple
	 * devices may share an interrupt. For e.g. PCI_INT_x lines.
	 *
	 * The device 'dev' in combination with the 'rid' uniquely
	 * identify this interrupt source.
	 */
	device_t dev;
	int rid;

	SLIST_ENTRY(sb_intmap) next;
};

static SLIST_HEAD(, sb_intmap) sb_intmap_head;

static struct sb_intmap *
sb_intmap_lookup(int intrnum, device_t dev, int rid)
{
	struct sb_intmap *map;

	SLIST_FOREACH(map, &sb_intmap_head, next) {
		if (dev == map->dev && rid == map->rid &&
		    intrnum == map->hardint)
			break;
	}
	return (map);
}

/*
 * Keep track of which (dev,rid,hardint) tuple is using the interrupt source.
 *
 * We don't actually unmask the interrupt source until the device calls
 * a bus_setup_intr() on the resource.
 */
static void
sb_intmap_add(int intrnum, device_t dev, int rid, int intsrc)
{
	struct sb_intmap *map;
	
	KASSERT(intrnum >= 0 && intrnum < NHARD_IRQS,
		("intrnum is out of range: %d", intrnum));

	map = sb_intmap_lookup(intrnum, dev, rid);
	if (map) {
		KASSERT(intsrc == map->intsrc,
			("%s%d allocating SYS_RES_IRQ resource with rid %d "
			 "with a different intsrc (%d versus %d)",
			device_get_name(dev), device_get_unit(dev), rid,
			intsrc, map->intsrc));
		return;
	}

	map = malloc(sizeof(*map), M_INTMAP, M_WAITOK | M_ZERO);
	map->intsrc = intsrc;
	map->hardint = intrnum;
	map->dev = dev;
	map->rid = rid;

	SLIST_INSERT_HEAD(&sb_intmap_head, map, next);
}

static void
sb_intmap_activate(int intrnum, device_t dev, int rid)
{
	struct sb_intmap *map;
	
	KASSERT(intrnum >= 0 && intrnum < NHARD_IRQS,
		("intrnum is out of range: %d", intrnum));

	map = sb_intmap_lookup(intrnum, dev, rid);
	if (map) {
		/*
		 * Deliver all interrupts to CPU0.
		 */
		mtx_lock_spin(&zbbus_intr_mtx);
		hardint_to_intsrc_mask[intrnum] |= 1ULL << map->intsrc;
		sb_enable_intsrc(0, map->intsrc);
		mtx_unlock_spin(&zbbus_intr_mtx);
	} else {
		/*
		 * In zbbus_setup_intr() we blindly call sb_intmap_activate()
		 * for every interrupt activation that comes our way.
		 *
		 * We might end up here if we did not "hijack" the SYS_RES_IRQ
		 * resource in zbbus_alloc_resource().
		 */
		printf("sb_intmap_activate: unable to activate interrupt %d "
		       "for device %s%d rid %d.\n", intrnum,
		       device_get_name(dev), device_get_unit(dev), rid);
	}
}

/*
 * Replace the default interrupt mask and unmask routines in intr_machdep.c
 * with routines that are SMP-friendly. In contrast to the default mask/unmask
 * routines in intr_machdep.c these routines do not change the SR.int_mask bits.
 *
 * Instead they use the interrupt mapper to either mask or unmask all
 * interrupt sources feeding into a particular interrupt line of the processor.
 *
 * This means that these routines have an identical effect irrespective of
 * which cpu is executing them. This is important because the ithread may
 * be scheduled to run on either of the cpus.
 */
static void
zbbus_intr_mask(void *arg)
{
	uint64_t mask;
	int irq;
	
	irq = (uintptr_t)arg;

	mtx_lock_spin(&zbbus_intr_mtx);

	mask = sb_read_intsrc_mask(0);
	mask |= hardint_to_intsrc_mask[irq];
	sb_write_intsrc_mask(0, mask);

	mtx_unlock_spin(&zbbus_intr_mtx);
}

static void
zbbus_intr_unmask(void *arg)
{
	uint64_t mask;
	int irq;
	
	irq = (uintptr_t)arg;

	mtx_lock_spin(&zbbus_intr_mtx);

	mask = sb_read_intsrc_mask(0);
	mask &= ~hardint_to_intsrc_mask[irq];
	sb_write_intsrc_mask(0, mask);

	mtx_unlock_spin(&zbbus_intr_mtx);
}

struct zbbus_devinfo {
	struct resource_list resources;
};

static MALLOC_DEFINE(M_ZBBUSDEV, "zbbusdev", "zbbusdev");

static int
zbbus_probe(device_t dev)
{

	device_set_desc(dev, "Broadcom/Sibyte ZBbus");
	return (BUS_PROBE_NOWILDCARD);
}

static int
zbbus_attach(device_t dev)
{

	if (bootverbose) {
		device_printf(dev, "attached.\n");
	}

	cpu_set_hardintr_mask_func(zbbus_intr_mask);
	cpu_set_hardintr_unmask_func(zbbus_intr_unmask);

	bus_generic_probe(dev);
	bus_enumerate_hinted_children(dev);
	bus_generic_attach(dev);

	return (0);
}

static void
zbbus_hinted_child(device_t bus, const char *dname, int dunit)
{
	device_t child;
	long maddr, msize;
	int err, irq;

	if (resource_disabled(dname, dunit))
		return;

	child = BUS_ADD_CHILD(bus, 0, dname, dunit);
	if (child == NULL) {
		panic("zbbus: could not add child %s unit %d\n", dname, dunit);
	}

	if (bootverbose)
		device_printf(bus, "Adding hinted child %s%d\n", dname, dunit);

	/*
	 * Assign any pre-defined resources to the child.
	 */
	if (resource_long_value(dname, dunit, "msize", &msize) == 0 &&
	    resource_long_value(dname, dunit, "maddr", &maddr) == 0) {
		if (bootverbose) {
			device_printf(bus, "Assigning memory resource "
					   "0x%0lx/%ld to child %s%d\n",
					   maddr, msize, dname, dunit);
		}
		err = bus_set_resource(child, SYS_RES_MEMORY, 0, maddr, msize);
		if (err) {
			device_printf(bus, "Unable to set memory resource "
					   "0x%0lx/%ld for child %s%d: %d\n",
					   maddr, msize, dname, dunit, err);
		}
	}

	if (resource_int_value(dname, dunit, "irq", &irq) == 0) {
		if (bootverbose) {
			device_printf(bus, "Assigning irq resource %d to "
					   "child %s%d\n", irq, dname, dunit);
		}
		err = bus_set_resource(child, SYS_RES_IRQ, 0, irq, 1);
		if (err) {
			device_printf(bus, "Unable to set irq resource %d"
					   "for child %s%d: %d\n",
					   irq, dname, dunit, err);
		}
	}
}

static struct resource *
zbbus_alloc_resource(device_t bus, device_t child, int type, int *rid,
		     rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct resource *res;
	int intrnum, intsrc, isdefault;
	struct resource_list *rl;
	struct resource_list_entry *rle;
	struct zbbus_devinfo *dinfo;

	isdefault = (RMAN_IS_DEFAULT_RANGE(start, end) && count == 1);

	/*
	 * Our direct child is asking for a default resource allocation.
	 */
	if (device_get_parent(child) == bus) {
		dinfo = device_get_ivars(child);
		rl = &dinfo->resources;
		rle = resource_list_find(rl, type, *rid);
		if (rle) {
			if (rle->res)
				panic("zbbus_alloc_resource: resource is busy");
			if (isdefault) {
				start = rle->start;
				count = ulmax(count, rle->count);
				end = ulmax(rle->end, start + count - 1);
			}
		} else {
			if (isdefault) {
				/*
				 * Our child is requesting a default
				 * resource allocation but we don't have the
				 * 'type/rid' tuple in the resource list.
				 *
				 * We have to fail the resource allocation.
				 */
				return (NULL);
			} else {
				/*
				 * The child is requesting a non-default
				 * resource. We just pass the request up
				 * to our parent. If the resource allocation
				 * succeeds we will create a resource list
				 * entry corresponding to that resource.
				 */
			}
		}
	} else {
		rl = NULL;
		rle = NULL;
	}

	/*
	 * nexus doesn't know about the interrupt mapper and only wants to
	 * see the hard irq numbers [0-6]. We translate from the interrupt
	 * source presented to the mapper to the interrupt number presented
	 * to the cpu.
	 */
	if ((count == 1) && (type == SYS_RES_IRQ)) {
		intsrc = start;
		intrnum = sb_route_intsrc(intsrc);
		start = end = intrnum;
	} else {
		intsrc = -1;		/* satisfy gcc */
		intrnum = -1;
	}

	res = bus_generic_alloc_resource(bus, child, type, rid,
 					 start, end, count, flags);

	/*
	 * Keep track of the input into the interrupt mapper that maps
	 * to the resource allocated by 'child' with resource id 'rid'.
	 *
	 * If we don't record the mapping here then we won't be able to
	 * locate the interrupt source when bus_setup_intr(child,rid) is
	 * called.
	 */
	if (res != NULL && intrnum != -1)
		sb_intmap_add(intrnum, child, rman_get_rid(res), intsrc);

	/*
	 * If a non-default resource allocation by our child was successful
	 * then keep track of the resource in the resource list associated
	 * with the child.
	 */
	if (res != NULL && rle == NULL && device_get_parent(child) == bus) {
		resource_list_add(rl, type, *rid, start, end, count);
		rle = resource_list_find(rl, type, *rid);
		if (rle == NULL)
			panic("zbbus_alloc_resource: cannot find resource");
	}

	if (rle != NULL) {
		KASSERT(device_get_parent(child) == bus,
			("rle should be NULL for passthru device"));
		rle->res = res;
		if (rle->res) {
			rle->start = rman_get_start(rle->res);
			rle->end = rman_get_end(rle->res);
			rle->count = count;
		}
	}

	return (res);
}

static int
zbbus_setup_intr(device_t dev, device_t child, struct resource *irq, int flags,
		 driver_filter_t *filter, driver_intr_t *intr, void *arg, 
		 void **cookiep)
{
	int error;

	error = bus_generic_setup_intr(dev, child, irq, flags,
				       filter, intr, arg, cookiep);
	if (error == 0)
		sb_intmap_activate(rman_get_start(irq), child,
				   rman_get_rid(irq));

	return (error);
}

static device_t
zbbus_add_child(device_t bus, u_int order, const char *name, int unit)
{
	device_t child;
	struct zbbus_devinfo *dinfo;

	child = device_add_child_ordered(bus, order, name, unit);
	if (child != NULL) {
		dinfo = malloc(sizeof(struct zbbus_devinfo), M_ZBBUSDEV,
			       M_WAITOK | M_ZERO);
		resource_list_init(&dinfo->resources);
		device_set_ivars(child, dinfo);
	}

	return (child);
}

static struct resource_list *
zbbus_get_resource_list(device_t dev, device_t child)
{
	struct zbbus_devinfo *dinfo = device_get_ivars(child);

	return (&dinfo->resources);
}

static device_method_t zbbus_methods[] ={
	/* Device interface */
	DEVMETHOD(device_probe,		zbbus_probe),
	DEVMETHOD(device_attach,	zbbus_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_alloc_resource,	zbbus_alloc_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_get_resource_list,zbbus_get_resource_list),
	DEVMETHOD(bus_set_resource,	bus_generic_rl_set_resource),
	DEVMETHOD(bus_get_resource,	bus_generic_rl_get_resource),
	DEVMETHOD(bus_delete_resource,	bus_generic_rl_delete_resource),
	DEVMETHOD(bus_setup_intr,	zbbus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_add_child,	zbbus_add_child),
	DEVMETHOD(bus_hinted_child,	zbbus_hinted_child),
	
	{ 0, 0 }
};

static driver_t zbbus_driver = {
	"zbbus",
	zbbus_methods
};

static devclass_t zbbus_devclass;

DRIVER_MODULE(zbbus, nexus, zbbus_driver, zbbus_devclass, 0, 0);
