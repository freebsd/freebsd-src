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
 *
 *	$FreeBSD$
 */

/*
 * PCI:EISA bridge support
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>

static int	eisab_probe(device_t dev);
static int	eisab_attach(device_t dev);

static device_method_t eisab_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,		eisab_probe),
    DEVMETHOD(device_attach,		eisab_attach),
    DEVMETHOD(device_shutdown,		bus_generic_shutdown),
    DEVMETHOD(device_suspend,		bus_generic_suspend),
    DEVMETHOD(device_resume,		bus_generic_resume),

    /* Bus interface */
    DEVMETHOD(bus_print_child,		bus_generic_print_child),
    DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
    DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
    DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
    DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
    DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
    DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),

    { 0, 0 }
};

static driver_t eisab_driver = {
    "eisab",
    eisab_methods,
    0,
};

static devclass_t eisab_devclass;

DRIVER_MODULE(eisab, pci, eisab_driver, eisab_devclass, 0, 0);

static int
eisab_probe(device_t dev)
{
    int		matched = 0;

    /*
     * Generic match by class/subclass.
     */
    if ((pci_get_class(dev) == PCIC_BRIDGE) &&
	(pci_get_subclass(dev) == PCIS_BRIDGE_EISA))
	matched = 1;

    /*
     * Some bridges don't correctly report their class.
     */
    switch (pci_get_devid(dev)) {
    case 0x04828086:		/* reports PCI-HOST class (!) */
	matched = 1;
	break;
    default:
	break;
    }
    
    if (matched) {
	device_set_desc(dev, "PCI-EISA bridge");
	return(-10000);
    }
    return(ENXIO);
}

static int
eisab_attach(device_t dev)
{
    device_t	child;

    /*
     * Attach an EISA bus.  Note that we can only have one EISA bus.
     */
    child = device_add_child(dev, "eisa", 0);
    if (child != NULL)
	return(bus_generic_attach(dev));

    /*
     * Attach an ISA bus as well (should this be a child of EISA?)
     */
    child = device_add_child(dev, "isa", 0);
    if (child != NULL)
	bus_generic_attach(dev);

    return(0);
}

