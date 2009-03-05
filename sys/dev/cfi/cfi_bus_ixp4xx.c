/*-
 * Copyright (c) 2009 Roelof Jonkman, Carlson Wireless Inc.
 * Copyright (c) 2009 Sam Leffler, Errno Consulting
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>   
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <machine/bus.h>

#include <dev/cfi/cfi_var.h>

#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>

static int
cfi_ixp4xx_probe(device_t dev)
{
	struct cfi_softc *sc = device_get_softc(dev);
	/*
	 * NB: we assume the boot loader sets up EXP_TIMING_CS0_OFFSET
	 * according to the flash on the board.  If it does not then it
	 * can be done here.
	 */
	if (bootverbose) {
		struct ixp425_softc *sa =
		    device_get_softc(device_get_parent(dev));
		device_printf(dev, "EXP_TIMING_CS0_OFFSET 0x%x\n",
		    EXP_BUS_READ_4(sa, EXP_TIMING_CS0_OFFSET));
	}
	sc->sc_width = 2;		/* NB: don't probe interface width */
	return cfi_probe(dev);
}

static device_method_t cfi_ixp4xx_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		cfi_ixp4xx_probe),
	DEVMETHOD(device_attach,	cfi_attach),
	DEVMETHOD(device_detach,	cfi_detach),

	{0, 0}
};

static driver_t cfi_ixp4xx_driver = {
	cfi_driver_name,
	cfi_ixp4xx_methods,
	sizeof(struct cfi_softc),
};
DRIVER_MODULE(cfi, ixp, cfi_ixp4xx_driver, cfi_devclass, 0, 0);
