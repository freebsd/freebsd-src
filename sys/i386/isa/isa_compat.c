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
 * $FreeBSD: src/sys/i386/isa/isa_compat.c,v 1.18 2000/01/29 15:37:36 peter Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <sys/rman.h>

#include <machine/vmparam.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>
#include <machine/md_var.h>

#include <machine/resource.h>
#include <isa/isavar.h>
#include <i386/isa/isa_compat.h>
#include <i386/isa/isa_device.h>

struct isa_compat_resources {
    struct resource *ports;
    struct resource *memory;
    struct resource *drq;
    struct resource *irq;
};

int
isa_compat_nextid(void)
{
	static int id = 2;	/* id_id of -1, 0 and 1 are "used" */

	return id++;
}

static void
isa_compat_alloc_resources(device_t dev, struct isa_compat_resources *res)
{
	int rid;
	u_long start, count;

	if (bus_get_resource(dev, SYS_RES_IOPORT, 0,
			     &start, &count) == 0) {
		rid = 0;
		res->ports = bus_alloc_resource(dev, SYS_RES_IOPORT,
						&rid, 0ul, ~0ul, 1,
						RF_ACTIVE);
		if (res->ports == NULL)
			printf("isa_compat: didn't get ports for %s\n",
			       device_get_name(dev));
	} else
		res->ports = 0;

	if (bus_get_resource(dev, SYS_RES_MEMORY, 0,
			     &start, &count) == 0
	    && start != 0) {
		rid = 0;
		res->memory = bus_alloc_resource(dev, SYS_RES_MEMORY,
						 &rid, 0ul, ~0ul, 1,
						 RF_ACTIVE);
		if (res->memory == NULL)
			printf("isa_compat: didn't get memory for %s\n",
			       device_get_name(dev));
	} else
		res->memory = 0;

	if (bus_get_resource(dev, SYS_RES_DRQ, 0,
			     &start, &count) == 0) {
		rid = 0;
		res->drq = bus_alloc_resource(dev, SYS_RES_DRQ,
					      &rid, 0ul, ~0ul, 1,
					      RF_ACTIVE);
		if (res->drq == NULL)
			printf("isa_compat: didn't get drq for %s\n",
			       device_get_name(dev));
	} else
		res->drq = 0;

	if (bus_get_resource(dev, SYS_RES_IRQ, 0,
			     &start, &count) == 0) {
		rid = 0;
		res->irq = bus_alloc_resource(dev, SYS_RES_IRQ,
					      &rid, 0ul, ~0ul, 1,
					      RF_SHAREABLE | RF_ACTIVE);
		if (res->irq == NULL)
			printf("isa_compat: didn't get irq for %s\n",
			       device_get_name(dev));
	} else
		res->irq = 0;
}

static void
isa_compat_release_resources(device_t dev, struct isa_compat_resources *res)
{
	if (res->ports) {
		bus_release_resource(dev, SYS_RES_IOPORT, 0, res->ports);
		res->ports = 0;
	}
	if (res->memory) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0, res->memory);
		res->memory = 0;
	}
	if (res->drq) {
		bus_release_resource(dev, SYS_RES_DRQ, 0, res->drq);
		res->drq = 0;
	}
	if (res->irq) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, res->irq);
		res->irq = 0;
	}
}

#define	irqmask(x)	((x) < 0 ? 0 : (1 << (x)))

static int
isa_compat_probe(device_t dev)
{
	struct isa_device *dvp = device_get_softc(dev);
	struct isa_compat_resources res;
	struct old_isa_driver *op;
	u_long start, count;

	/* No pnp support */
	if (isa_get_vendorid(dev))
		return (ENXIO);

	bzero(&res, sizeof(res));
	/*
	 * Fill in the isa_device fields.
	 */
	op = device_get_driver(dev)->priv;
	dvp->id_id = isa_compat_nextid();
	dvp->id_driver = op->driver;
	if (bus_get_resource(dev, SYS_RES_IOPORT, 0,
			     &start, &count) == 0)
		dvp->id_iobase = start;
	else
		dvp->id_iobase = -1;
	if (bus_get_resource(dev, SYS_RES_IRQ, 0,
			     &start, &count) == 0)
		dvp->id_irq = irqmask(start);
	else
		dvp->id_irq = 0;
	if (bus_get_resource(dev, SYS_RES_DRQ, 0,
			     &start, &count) == 0)
		dvp->id_drq = start;
	else
		dvp->id_drq = -1;
	if (bus_get_resource(dev, SYS_RES_MEMORY,
			     0, &start, &count) == 0) {
		dvp->id_maddr = (void *)(uintptr_t)start;
		dvp->id_msize = count;
	} else {
		dvp->id_maddr = NULL;
		dvp->id_msize = 0;
	}
	dvp->id_unit = device_get_unit(dev);
	dvp->id_flags = device_get_flags(dev);
	dvp->id_enabled = device_is_enabled(dev);	/* XXX unused */
	dvp->id_device = dev;

	/*
	 * Do the wrapped probe.
	 */
	if (dvp->id_driver->probe) {
		int portsize;
		void *maddr;
		struct isa_device old;

		isa_compat_alloc_resources(dev, &res);
		if (res.memory)
			maddr = rman_get_virtual(res.memory);
		else
			maddr = 0;
		dvp->id_maddr = maddr;
		old = *dvp;
		portsize = dvp->id_driver->probe(dvp);
		isa_compat_release_resources(dev, &res);
		if (portsize != 0) {
			if (portsize > 0 || dvp->id_iobase != old.id_iobase)
				bus_set_resource(dev, SYS_RES_IOPORT,
						 0, dvp->id_iobase, portsize);
			if (dvp->id_irq != old.id_irq)
				bus_set_resource(dev, SYS_RES_IRQ, 0,
						 ffs(dvp->id_irq) - 1, 1);
			if (dvp->id_drq != old.id_drq)
				bus_set_resource(dev, SYS_RES_DRQ, 0,
						 dvp->id_drq, 1);
			if (dvp->id_maddr != old.id_maddr
			    || dvp->id_msize != old.id_msize) {
				maddr = dvp->id_maddr;
				if (maddr != NULL)
					bus_set_resource(dev,
							 SYS_RES_MEMORY,
							 0,
							 kvtop(maddr),
							 dvp->id_msize);
				else
					bus_delete_resource(dev,
							    SYS_RES_MEMORY,
							    0);
			}
			return 0;
		}
	}
	return ENXIO;
}

static int
isa_compat_attach(device_t dev)
{
	struct isa_device *dvp = device_get_softc(dev);
	struct isa_compat_resources res;
	int error;

	bzero(&res, sizeof(res));
	isa_compat_alloc_resources(dev, &res);
	if (dvp->id_driver->attach)
		dvp->id_driver->attach(dvp);
	if (res.irq && dvp->id_irq && dvp->id_intr) {
		struct old_isa_driver *op;
		void *ih;

		op = device_get_driver(dev)->priv;
		error = BUS_SETUP_INTR(device_get_parent(dev), dev,
				       res.irq, op->type,
				       dvp->id_intr,
				       (void *)(uintptr_t)dvp->id_unit,
				       &ih);
		if (error)
			printf("isa_compat_attach: failed to setup intr: %d\n",
			       error);
	}
	device_printf(dev, "driver is using old-style compatability shims\n");
	return 0;
}

static device_method_t isa_compat_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		isa_compat_probe),
	DEVMETHOD(device_attach,	isa_compat_attach),

	{ 0, 0 }
};

/*
 * Create a new style driver around each old isa driver.
 */
void
isa_wrap_old_drivers(void)
{
	int i;
	struct old_isa_driver *op;
	devclass_t isa_devclass = devclass_find("isa");

	for (i = 0, op = &old_drivers[0]; i < old_drivers_count; i++, op++) {
		driver_t *driver;
		driver = malloc(sizeof(driver_t), M_DEVBUF, M_NOWAIT);
		if (!driver)
			continue;
		bzero(driver, sizeof(driver_t));
		driver->name = op->driver->name;
		driver->methods = isa_compat_methods;
		driver->softc = sizeof(struct isa_device);
		driver->priv = op;
		if (op->driver->sensitive_hw)
			resource_set_int(op->driver->name, -1, "sensitive", 1);
		devclass_add_driver(isa_devclass, driver);
	}
}

int
haveseen_iobase(struct isa_device *dvp, int size)
{
	int rid;
	struct resource *res;
	device_t dev = dvp->id_device;
	int base = dvp->id_iobase;

	/*
	 * Ask for resource 1 so that we don't hurt our hints.  In theory
	 * this should work, but....
	 */
	rid = 1;
	res = bus_alloc_resource(dev, SYS_RES_IOPORT,
				 &rid, base, base + size, size, RF_ACTIVE);
	if (res)
		bus_release_resource(dev, SYS_RES_IOPORT, rid, res);
	return res ? 0 : 1;
}
