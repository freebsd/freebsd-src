/*
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
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/systm.h>

#include <dev/fdc/fdcvar.h>
#include <dev/fdc/fdcreg.h>

#include <isa/isavar.h>
#include <isa/isareg.h>

static int fdc_isa_probe(device_t);
static void fdctl_wr_isa(fdc_p, u_int8_t);

static struct isa_pnp_id fdc_ids[] = {
	{0x0007d041, "PC standard floppy disk controller"}, /* PNP0700 */
	{0x0107d041, "Standard floppy controller supporting MS Device Bay Spec"}, /* PNP0701 */
	{0}
};

static void
fdctl_wr_isa(fdc_p fdc, u_int8_t v)
{
	bus_space_write_1(fdc->ctlt, fdc->ctlh, 0, v);
}

static int
fdc_isa_probe(device_t dev)
{
	int	error, ic_type;
	struct	fdc_data *fdc;

	fdc = device_get_softc(dev);
	bzero(fdc, sizeof *fdc);
	fdc->fdc_dev = dev;
	fdc->fdctl_wr = fdctl_wr_isa;

	/* Check pnp ids */
	error = ISA_PNP_PROBE(device_get_parent(dev), dev, fdc_ids);
	if (error == ENXIO)
		return ENXIO;
	if (error == 0)
		fdc->flags |= FDC_ISPNP;

	/* Attempt to allocate our resources for the duration of the probe */
	error = fdc_alloc_resources(fdc);
	if (error)
		goto out;

	/* First - lets reset the floppy controller */
	fdout_wr(fdc, 0);
	DELAY(100);
	fdout_wr(fdc, FDO_FRST);

	/* see if it can handle a command */
	if (fd_cmd(fdc, 3, NE7CMD_SPECIFY, NE7_SPEC_1(3, 240), 
		   NE7_SPEC_2(2, 0), 0)) {
		error = ENXIO;
		goto out;
	}

	if (fd_cmd(fdc, 1, NE7CMD_VERSION, 1, &ic_type) == 0) {
		ic_type = (u_char)ic_type;
		switch (ic_type) {
		case 0x80:
			device_set_desc(dev, "NEC 765 or clone");
			fdc->fdct = FDC_NE765;
			break;
		case 0x81:	/* not mentioned in any hardware doc */
		case 0x90:
			device_set_desc(dev,
		"Enhanced floppy controller (i82077, NE72065 or clone)");
			fdc->fdct = FDC_ENHANCED;
			break;
		default:
			device_set_desc(dev, "Generic floppy controller");
			fdc->fdct = FDC_UNKNOWN;
			break;
		}
	}

out:
	fdc_release_resources(fdc);
	return (error);
}

static device_method_t fdc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fdc_isa_probe),
	DEVMETHOD(device_attach,	fdc_attach),
	DEVMETHOD(device_detach,	fdc_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	fdc_print_child),
	DEVMETHOD(bus_read_ivar,	fdc_read_ivar),
	/* Our children never use any other bus interface methods. */

	{ 0, 0 }
};

static driver_t fdc_driver = {
	"fdc",
	fdc_methods,
	sizeof(struct fdc_data)
};

DRIVER_MODULE(fdc, isa, fdc_driver, fdc_devclass, 0, 0);
DRIVER_MODULE(fdc, acpi, fdc_driver, fdc_devclass, 0, 0);
