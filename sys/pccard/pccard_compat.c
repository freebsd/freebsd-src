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
 * $FreeBSD$
 */

#include "opt_bus.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
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
#include <pccard/driver.h>
#include <pccard/pcic.h>
#include <pccard/slot.h>
#include <pccard/pccard_nbk.h>

/* ============================================================ */

static int
pccnbk_probe(device_t dev)
{
	char devnam[128];
	const char *name;
	struct pccard_devinfo *devi = device_get_ivars(dev);

	if (devi) {
		name = device_get_name(dev);
		snprintf(devnam, sizeof(devnam), "pccard-%s",
		    devi->drv->name);
		if (!name || strcmp(name, devnam) != 0)
			return ENXIO;
		device_set_desc(dev, devi->drv->name);
		return 0;
	}
	return ENXIO;
}

static int
pccnbk_attach(device_t dev)
{
	struct pccard_devinfo *devi = device_get_ivars(dev);
	struct pccard_device *drv;
	struct slot *slt;
	int err;
	int s;

	slt = devi->slt;
	drv = devi->drv;
	devi->next = slt->devices;
	slt->devices = devi;
	s = splhigh();
	err = drv->enable(devi);
	splx(s);
	/*
	 *	If the enable functions returns no error, then the
	 *	device has been successfully installed. If so, then
	 *	attach it to the slot, otherwise free it and return
	 *	the error.  We assume that when we free the device,
	 *	it will also set 'running' to off.
	 */
	if (err) {
		printf("pccard: %s%d Enable failed %d\n", devi->drv->name,
		    devi->isahd.id_unit, err);
		pccard_remove_device(devi);
	}
	return(err);
}

/*
 * Allocate resources for this device in the rman system.
 */
int
pccnbk_alloc_resources(device_t dev)
{
	struct pccard_devinfo *devi = device_get_ivars(dev);
	int rid;
	u_long start;
	u_long end;
	u_long count;

	start = devi->isahd.id_iobase;
	count = devi->isahd.id_iosize;
	end  = start + count - 1;
	    
	rid = 0;
	devi->iorv = BUS_ALLOC_RESOURCE(device_get_parent(dev), dev,
	    SYS_RES_IOPORT, &rid, start, end, count, RF_ACTIVE);
	if (!devi->iorv) {
		printf("Cannot allocate ports 0x%lx-0x%lx\n", start, end);
		return (ENOMEM);
	}
	rid = 0;
	start = end = ffs(devi->isahd.id_irq) - 1;
	count = 1;
	devi->irqrv = BUS_ALLOC_RESOURCE(device_get_parent(dev), dev,
	    SYS_RES_IRQ, &rid, start, end, count, RF_ACTIVE);
	if (!devi->irqrv) {
		return (ENOMEM);
	}
	return(0);
}

void
pccnbk_release_resources(device_t dev)
{
	struct pccard_devinfo *devi = device_get_ivars(dev);
	u_long start;
	u_long end;
	u_long count;

	start = devi->isahd.id_iobase;
	count = devi->isahd.id_iosize;
	end  = start + count - 1;

	if (devi->iorv) {
		BUS_RELEASE_RESOURCE(device_get_parent(dev), dev,
		    SYS_RES_IOPORT, 0, devi->iorv);
		devi->iorv = NULL;
	}
	if (devi->irqrv) {
		BUS_RELEASE_RESOURCE(device_get_parent(dev), dev, SYS_RES_IRQ,
		    0, devi->irqrv);
		devi->irqrv = NULL;
	}
}

static device_method_t pccnbk_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pccnbk_probe),
	DEVMETHOD(device_attach,	pccnbk_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

	{ 0, 0 }
};

void
pccnbk_wrap_old_driver(struct pccard_device *drv)
{
	char devnam[128];
	char *nm;
	driver_t *driver;

	snprintf(devnam, sizeof(devnam), "pccard-%s", drv->name);
	driver = malloc(sizeof(driver_t), M_DEVBUF, M_NOWAIT);
	if (!driver)
		return;
	bzero(driver, sizeof(driver_t));
	/* XXX May create a memory leak for load/unload :-( XXX */
	nm = malloc(strlen(devnam) + 1, M_DEVBUF, M_NOWAIT);
	strcpy(nm, devnam);
	driver->name = nm;
	driver->methods = pccnbk_methods;
	driver->softc = sizeof(struct pccard_device);
	driver->priv = drv;
	devclass_add_driver(pccard_devclass, driver);
	drv->driver = driver;
}

#if 0
static devclass_t	pccnbk_devclass;

static driver_t pccnbk_driver = {
	"pccnbk",
	pccnbk_methods,
	1,			/* no softc */
};

DRIVER_MODULE(pccnbk, pccard, pccnbk_driver, pccnbk_devclass, 0, 0);
#endif
