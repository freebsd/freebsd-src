/*-
 * Copyright (c) 2004 M. Warner Losh.
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

#include <dev/fdc/fdcvar.h>

#include <isa/isavar.h>
#include <isa/isareg.h>

static int fdc_isa_probe(device_t);

static struct isa_pnp_id fdc_ids[] = {
	{0x0007d041, "PC standard floppy disk controller"}, /* PNP0700 */
	{0x0107d041, "Standard floppy controller supporting MS Device Bay Spec"}, /* PNP0701 */
	{0}
};

int
fdc_isa_alloc_resources(device_t dev, struct fdc_data *fdc)
{
	int ispnp, nports;

	ispnp = (fdc->flags & FDC_ISPNP) != 0;
	fdc->fdc_dev = dev;
	fdc->rid_ioport = 0;
	fdc->rid_irq = 0;
	fdc->rid_drq = 0;
	fdc->rid_ctl = 1;

	/*
	 * On standard ISA, we don't just use an 8 port range
	 * (e.g. 0x3f0-0x3f7) since that covers an IDE control
	 * register at 0x3f6.
	 *
	 * Isn't PC hardware wonderful.
	 */
	nports = ispnp ? 1 : 6;

	/*
	 * Some ACPI BIOSen have _CRS objects for the floppy device that
	 * split the I/O port resource into several resources.  We detect
	 * this case by checking if there are more than 2 IOPORT resources.
	 * If so, we use the resource with the smallest start address as
	 * the port RID and the largest start address as the control RID.
	 */
	if (bus_get_resource_count(dev, SYS_RES_IOPORT, 2) != 0) {
		u_long min_start, max_start, tmp;
		int i;

		/* Find the min/max start addresses and their RIDs. */
		max_start = 0ul;
		min_start = ~0ul;
		for (i = 0; bus_get_resource_count(dev, SYS_RES_IOPORT, i) > 0;
		    i++) {
			tmp = bus_get_resource_start(dev, SYS_RES_IOPORT, i);
			KASSERT(tmp != 0, ("bogus resource"));
			if (tmp < min_start) {
				min_start = tmp;
				fdc->rid_ioport = i;
			}
			if (tmp > max_start) {
				max_start = tmp;
				fdc->rid_ctl = i;
			}
		}
	}

	fdc->res_ioport = bus_alloc_resource(dev, SYS_RES_IOPORT,
	    &fdc->rid_ioport, 0ul, ~0ul, nports, RF_ACTIVE);
	if (fdc->res_ioport == 0) {
		device_printf(dev, "cannot reserve I/O port range (%d ports)\n",
			      nports);
		return ENXIO;
	}
	fdc->portt = rman_get_bustag(fdc->res_ioport);
	fdc->porth = rman_get_bushandle(fdc->res_ioport);

	/*
	 * Some BIOSen report the device at 0x3f2-0x3f5,0x3f7
	 * and some at 0x3f0-0x3f5,0x3f7. We detect the former
	 * by checking the size and adjust the port address
	 * accordingly.
	 */
	if (bus_get_resource_count(dev, SYS_RES_IOPORT, 0) == 4)
		fdc->port_off = -2;

	/*
	 * Register the control port range as rid 1 if it
	 * isn't there already. Most PnP BIOSen will have
	 * already done this but non-PnP configurations don't.
	 *
	 * And some (!!) report 0x3f2-0x3f5 and completely
	 * leave out the control register!  It seems that some
	 * non-antique controller chips have a different
	 * method of programming the transfer speed which
	 * doesn't require the control register, but it's
	 * mighty bogus as the chip still responds to the
	 * address for the control register.
	 */
	if (bus_get_resource_count(dev, SYS_RES_IOPORT, 1) == 0) {
		u_long ctlstart;
		/* Find the control port, usually 0x3f7 */
		ctlstart = rman_get_start(fdc->res_ioport) + fdc->port_off + 7;
		bus_set_resource(dev, SYS_RES_IOPORT, 1, ctlstart, 1);
	}

	/*
	 * Now (finally!) allocate the control port.
	 */
	fdc->res_ctl = bus_alloc_resource_any(dev, SYS_RES_IOPORT,
	    &fdc->rid_ctl, RF_ACTIVE);
	if (fdc->res_ctl == 0) {
		device_printf(dev,
		    "cannot reserve control I/O port range (control port)\n");
		return ENXIO;
	}
	fdc->ctlt = rman_get_bustag(fdc->res_ctl);
	fdc->ctlh = rman_get_bushandle(fdc->res_ctl);
	fdc->ctl_off = 0;

	fdc->res_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &fdc->rid_irq,
					      RF_ACTIVE | RF_SHAREABLE);
	if (fdc->res_irq == 0) {
		device_printf(dev, "cannot reserve interrupt line\n");
		return ENXIO;
	}

	if ((fdc->flags & FDC_NODMA) == 0) {
		fdc->res_drq = bus_alloc_resource_any(dev, SYS_RES_DRQ,
		    &fdc->rid_drq, RF_ACTIVE | RF_SHAREABLE);
		if (fdc->res_drq == 0) {
			device_printf(dev, "cannot reserve DMA request line\n");
			fdc->flags |= FDC_NODMA;
		} else
			fdc->dmachan = rman_get_start(fdc->res_drq);
	}

	return 0;
}

static int
fdc_isa_probe(device_t dev)
{
	int	error;
	struct	fdc_data *fdc;

	fdc = device_get_softc(dev);
	fdc->fdc_dev = dev;

	/* Check pnp ids */
	error = ISA_PNP_PROBE(device_get_parent(dev), dev, fdc_ids);
	if (error == ENXIO)
		return (ENXIO);

	/* Attempt to allocate our resources for the duration of the probe */
	error = fdc_isa_alloc_resources(dev, fdc);
	if (error == 0)
		error = fdc_initial_reset(dev, fdc);

	fdc_release_resources(fdc);
	return (error);
}

static int
fdc_isa_attach(device_t dev)
{
	struct	fdc_data *fdc;
	int error;

	fdc = device_get_softc(dev);
	error = ISA_PNP_PROBE(device_get_parent(dev), dev, fdc_ids);
	if (error == 0)
		fdc->flags |= FDC_ISPNP;

	if (error == 0)
		error = fdc_isa_alloc_resources(dev, fdc);
	if (error == 0)
		error = fdc_attach(dev);
	if (error == 0)
		error = fdc_hints_probe(dev);
	if (error)
		fdc_release_resources(fdc);
	return (error);
}

static device_method_t fdc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fdc_isa_probe),
	DEVMETHOD(device_attach,	fdc_isa_attach),
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
