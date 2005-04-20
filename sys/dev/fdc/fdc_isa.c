/*-
 * Copyright (c) 2004-2005 M. Warner Losh.
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
	int nports = 6;

	fdc->fdc_dev = dev;
	fdc->rid_ioport = 0;
	fdc->rid_irq = 0;
	fdc->rid_drq = 0;
	fdc->rid_ctl = 1;

	/*
	 * On standard ISA, we don't just use an 8 port range
	 * (e.g. 0x3f0-0x3f7) since that covers an IDE control
	 * register at 0x3f6.  So, on older hardware, we use
	 * 0x3f0-0x3f5 and 0x3f7.  However, some BIOSs omit the
	 * control port, while others start at 0x3f2.  Of the latter,
	 * sometimes we have two resources, other times we have one.
	 * We have to deal with the following cases:
	 *
	 * 1:	0x3f0-0x3f5			# very rare
	 * 2:	0x3f0				# hints -> 0x3f0-0x3f5,0x3f7
	 * 3:	0x3f0-0x3f5,0x3f7		# Most common
	 * 4:	0x3f2-0x3f5,0x3f7		# Second most common
	 * 5:	0x3f2-0x3f5			# implies 0x3f7 too.
	 * 6:	0x3f2-0x3f3,0x3f4-0x3f5,0x3f7	# becoming common
	 * 7:	0x3f2-0x3f3,0x3f4-0x3f5		# rare
	 * 8:	0x3f0-0x3f1,0x3f2-0x3f3,0x3f4-0x3f5,0x3f7
	 * 9:	0x3f0-0x3f3,0x3f4-0x3f5,0x3f7
	 *
	 * The following code is generic for any value of 0x3fx :-)
	 */

	/*
	 * First, allocated the main range of ports.  In the best of
	 * worlds, this is 4 or 6 ports.  In others, well, that's
	 * why this function is so complicated.
	 */
again_ioport:
	fdc->res_ioport = bus_alloc_resource(dev, SYS_RES_IOPORT,
	    &fdc->rid_ioport, 0ul, ~0ul, nports, RF_ACTIVE);
	if (fdc->res_ioport == 0) {
		device_printf(dev, "cannot allocate I/O port (%d ports)\n",
		    nports);
		return (ENXIO);
	}
	if ((rman_get_end(fdc->res_ioport) & 0x7) == 1) {
		/* Case 8 */
		bus_release_resource(dev, SYS_RES_IOPORT, fdc->rid_ioport,
		    fdc->res_ioport);
		fdc->rid_ioport++;
		goto again_ioport;
	}
	fdc->portt = rman_get_bustag(fdc->res_ioport);
	fdc->porth = rman_get_bushandle(fdc->res_ioport);

	/*
	 * Handle cases 4-8 above
	 */
	fdc->port_off = -(fdc->porth & 0x7);

	/*
	 * Deal with case 6-9: FDSTS and FDDATA.
	 */
	if ((rman_get_end(fdc->res_ioport) & 0x7) == 3) {
		fdc->rid_sts = fdc->rid_ioport + 1;
		fdc->res_sts = bus_alloc_resource_any(dev, SYS_RES_IOPORT,
		    &fdc->rid_sts, RF_ACTIVE);
		if (fdc->res_sts == NULL) {
			device_printf(dev, "Can't alloc rid 1");
			fdc_release_resources(fdc);
			return (ENXIO);
		}
		fdc->sts_off = -4;
		fdc->stst = rman_get_bustag(fdc->res_sts);
		fdc->stsh = rman_get_bushandle(fdc->res_sts);
	} else {
		fdc->res_sts = NULL;
		fdc->sts_off = fdc->port_off;
		fdc->stst = fdc->portt;
		fdc->stsh = fdc->porth;
	}

	/*
	 * allocate the control port.  For cases 1, 2, 5 and 7, we
	 * fake it from the ioports resource.  XXX IS THIS THE RIGHT THING
	 * TO DO, OR SHOULD WE CREATE A NEW RID? (I think we need a new rid)
	 */
	fdc->rid_ctl = fdc->rid_sts + 1;
	fdc->res_ctl = bus_alloc_resource_any(dev, SYS_RES_IOPORT,
	    &fdc->rid_ctl, RF_ACTIVE);
	if (fdc->res_ctl == NULL) {
		fdc->ctl_off = 7 + fdc->port_off;
		fdc->res_ctl = NULL;
		fdc->ctlt = fdc->portt;
		fdc->ctlh = fdc->porth;
	} else {
		fdc->ctl_off = 0;
		fdc->ctlt = rman_get_bustag(fdc->res_ctl);
		fdc->ctlh = rman_get_bushandle(fdc->res_ctl);
	}

	fdc->res_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &fdc->rid_irq,
	    RF_ACTIVE | RF_SHAREABLE);
	if (fdc->res_irq == 0) {
		device_printf(dev, "cannot reserve interrupt line\n");
		return (ENXIO);
	}

	if ((fdc->flags & FDC_NODMA) == 0) {
		fdc->res_drq = bus_alloc_resource_any(dev, SYS_RES_DRQ,
		    &fdc->rid_drq, RF_ACTIVE | RF_SHAREABLE);
		if (fdc->res_drq == 0) {
			device_printf(dev, "cannot reserve DMA request line\n");
			/* This is broken and doesn't work for ISA case */
			fdc->flags |= FDC_NODMA;
		} else
			fdc->dmachan = rman_get_start(fdc->res_drq);
	}

	return (0);
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
