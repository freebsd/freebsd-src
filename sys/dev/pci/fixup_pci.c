/*-
 * Copyright (c) 1994,1995 Stefan Esser, Wolfgang StanglMeier
 * Copyright (c) 2000 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2000 BSDi
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

/*
 * Chipset fixups.
 *
 * These routines are invoked during the probe phase for devices which
 * typically don't have specific device drivers, but which require
 * some cleaning up.
 */

static int	fixup_pci_probe(device_t dev);
static void	fixwsc_natoma(device_t dev);

static device_method_t fixup_pci_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,		fixup_pci_probe),
    DEVMETHOD(device_attach,		bus_generic_attach),
    { 0, 0 }
};

static driver_t fixup_pci_driver = {
    "fixup_pci",
    fixup_pci_methods,
    0,
};

static devclass_t fixup_pci_devclass;

DRIVER_MODULE(fixup_pci, pci, fixup_pci_driver, fixup_pci_devclass, 0, 0);

static int
fixup_pci_probe(device_t dev)
{
    switch (pci_get_devid(dev)) {
    case 0x12378086:		/* Intel 82440FX (Natoma) */
	fixwsc_natoma(dev);
	break;
    }
    return(ENXIO);
}

static void
fixwsc_natoma(device_t dev)
{
    int		pmccfg;

    pmccfg = pci_read_config(dev, 0x50, 2);
#if defined(SMP)
    if (pmccfg & 0x8000) {
	printf("Correcting Natoma config for SMP\n");
	pmccfg &= ~0x8000;
	pci_write_config(dev, 0x50, pmccfg, 2);
    }
#else
    if ((pmccfg & 0x8000) == 0) {
	printf("Correcting Natoma config for non-SMP\n");
	pmccfg |= 0x8000;
	pci_write_config(dev, 0x50, pmccfg, 2);
    }
#endif
}
