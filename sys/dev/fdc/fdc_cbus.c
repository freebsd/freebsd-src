/*-
 * Copyright (c) 2004 TAKAHASHI Yoshihiro
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
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
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <pc98/cbus/cbus.h>
#include <dev/fdc/fdcvar.h>

#include <isa/isavar.h>

static bus_addr_t fdc_iat[] = {0, 2, 4};

static int
fdc_cbus_alloc_resources(device_t dev, struct fdc_data *fdc)
{
	struct resource *res;
	int i, rid;

	fdc->fdc_dev = dev;
	rid = 0;
	res = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid, fdc_iat, 3,
	    RF_ACTIVE);
	if (res == NULL) {
		device_printf(dev, "cannot reserve I/O port range\n");
		return (ENXIO);
	}
	isa_load_resourcev(res, fdc_iat, 3);
	for (i = 0; i < 3; i++) {
		fdc->resio[i] = res;
		fdc->ridio[i] = rid;
		fdc->ioff[i] = i;
		fdc->ioh[i] = rman_get_bushandle(res);
	}
	fdc->iot = rman_get_bustag(res);
		
	rid = 3;
	bus_set_resource(dev, SYS_RES_IOPORT, rid, IO_FDPORT, 1);
	res = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid, RF_ACTIVE);
	if (res == NULL) {
		device_printf(dev, "cannot reserve I/O port range\n");
		return (ENXIO);
	}
	fdc->resio[3] = res;
	fdc->ridio[3] = rid;
	fdc->ioff[3] = 0;
	fdc->ioh[3] = rman_get_bushandle(res);
	/* XXX: Reuses fdc->iot */

	rid = 4;
	bus_set_resource(dev, SYS_RES_IOPORT, rid, 0x4be, 1);
	res = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid, RF_ACTIVE);
	if (res == NULL) {
		device_printf(dev, "cannot reserve I/O port range\n");
		return (ENXIO);
	}
	fdc->resio[4] = res;
	fdc->ridio[4] = rid;
	fdc->ioff[4] = 0;
	fdc->ioh[4] = rman_get_bushandle(res);
	/* XXX: Reuses fdc->iot */

	fdc->res_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &fdc->rid_irq,
	    RF_ACTIVE);
	if (fdc->res_irq == NULL) {
		device_printf(dev, "cannot reserve interrupt line\n");
		return (ENXIO);
	}

	if ((fdc->flags & FDC_NODMA) == 0) {
		fdc->res_drq = bus_alloc_resource_any(dev, SYS_RES_DRQ,
		    &fdc->rid_drq, RF_ACTIVE);
		if (fdc->res_drq == NULL) {
			device_printf(dev, "cannot reserve DMA request line\n");
			return (ENXIO);
		}
		fdc->dmachan = rman_get_start(fdc->res_drq);
	}

	return (0);
}

static int
fdc_cbus_probe(device_t dev)
{
	int	error;
	struct	fdc_data *fdc;

	fdc = device_get_softc(dev);

	/* Check pnp ids */
	if (isa_get_vendorid(dev))
		return (ENXIO);

	/* Attempt to allocate our resources for the duration of the probe */
	error = fdc_cbus_alloc_resources(dev, fdc);
	if (error == 0)
		error = fdc_initial_reset(dev, fdc);

	fdc_release_resources(fdc);
	return (error);
}

static int
fdc_cbus_attach(device_t dev)
{
	struct	fdc_data *fdc;
	int error;

	fdc = device_get_softc(dev);
	error = fdc_cbus_alloc_resources(dev, fdc);
	if (error == 0)
		error = fdc_attach(dev);
	if (error == 0)
		error = fdc_hints_probe(dev);
	if (error == 0)
		fdc_start_worker(dev);
	else
		fdc_release_resources(fdc);
	return (error);
}

static device_method_t fdc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fdc_cbus_probe),
	DEVMETHOD(device_attach,	fdc_cbus_attach),
	DEVMETHOD(device_detach,	fdc_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	fdc_print_child),
	DEVMETHOD(bus_read_ivar,	fdc_read_ivar),
	DEVMETHOD(bus_write_ivar,       fdc_write_ivar),
	/* Our children never use any other bus interface methods. */

	{ 0, 0 }
};

static driver_t fdc_driver = {
	"fdc",
	fdc_methods,
	sizeof(struct fdc_data)
};

DRIVER_MODULE(fdc, isa, fdc_driver, fdc_devclass, 0, 0);
