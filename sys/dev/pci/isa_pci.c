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

/*
 * PCI:ISA bridge support
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <isa/isavar.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#define	ELCR_IOADDR	0x4d0	/* Interrupt Edge/Level Control Registers */
#define	ELCR_IOLEN	2

struct isab_softc {
    struct resource *elcr_res;
    u_char saved_elcr[ELCR_IOLEN];
};

static int	isab_probe(device_t dev);
static int	pci_isab_attach(device_t dev);
static int	isab_detach(device_t dev);
static int	isab_resume(device_t dev);
static int	isab_suspend(device_t dev);

static device_method_t isab_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,		isab_probe),
    DEVMETHOD(device_attach,		pci_isab_attach),
    DEVMETHOD(device_detach,		isab_detach),
    DEVMETHOD(device_shutdown,		bus_generic_shutdown),
    DEVMETHOD(device_suspend,		isab_suspend),
    DEVMETHOD(device_resume,		isab_resume),

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

static driver_t isab_driver = {
    "isab",
    isab_methods,
    sizeof(struct isab_softc),
};

DRIVER_MODULE(isab, pci, isab_driver, isab_devclass, 0, 0);

/*
 * XXX we need to add a quirk list here for bridges that don't correctly
 *     report themselves.
 */
static int
isab_probe(device_t dev)
{
    int		matched = 0;

    /*
     * Try for a generic match based on class/subclass.
     */
    if ((pci_get_class(dev) == PCIC_BRIDGE) &&
	(pci_get_subclass(dev) == PCIS_BRIDGE_ISA)) {
	matched = 1;
    } else {
	/*
	 * These are devices that we *know* are PCI:ISA bridges. 
	 * Sometimes, however, they don't report themselves as
	 * such.  Check in case one of them is pretending to be
	 * something else.
	 */
	switch (pci_get_devid(dev)) {
	case 0x04848086:	/* Intel 82378ZB/82378IB */
	case 0x122e8086:	/* Intel 82371FB */
	case 0x70008086:	/* Intel 82371SB */
	case 0x71108086:	/* Intel 82371AB */
	case 0x71988086:	/* Intel 82443MX */
	case 0x24108086:	/* Intel 82801AA (ICH) */
	case 0x24208086:	/* Intel 82801AB (ICH0) */
	case 0x24408086:	/* Intel 82801AB (ICH2) */
	case 0x00061004:	/* VLSI 82C593 */
	case 0x05861106:	/* VIA 82C586 */
	case 0x05961106:	/* VIA 82C596 */
	case 0x06861106:	/* VIA 82C686 */
	case 0x153310b9:	/* AcerLabs M1533 */
	case 0x154310b9:	/* AcerLabs M1543 */
	case 0x00081039:	/* SiS 85c503 */
	case 0x00001078:	/* Cyrix Cx5510 */
	case 0x01001078:	/* Cyrix Cx5530 */
	case 0xc7001045:	/* OPTi 82C700 (FireStar) */
	case 0x00011033:	/* NEC 0001 (C-bus) */
	case 0x002c1033:	/* NEC 002C (C-bus) */
	case 0x003b1033:	/* NEC 003B (C-bus) */
	case 0x886a1060:	/* UMC UM8886 ISA */
	case 0x02001166:	/* ServerWorks IB6566 PCI */
	    if (bootverbose)
		printf("PCI-ISA bridge with incorrect subclass 0x%x\n",
		       pci_get_subclass(dev));
	    matched = 1;
	    break;
	
	default:
	    break;
	}
    }

    if (matched) {
	device_set_desc(dev, "PCI-ISA bridge");
	return(-10000);
    }
    return(ENXIO);
}

static int
pci_isab_attach(device_t dev)
{
    struct isab_softc *sc = device_get_softc(dev);
    int error, rid;

    /*
     * Attach an ISA bus.  Note that we can only have one ISA bus.
     */
    error = isab_attach(dev);
    if (error)
	    return (error);

    switch (pci_get_devid(dev)) {
    case 0x71108086: /* Intel 82371AB */
	/*
	 * Sometimes the ELCR (Edge/Level Control Register) is not restored
	 * correctly on resume by the BIOS, so we handle it ourselves.
	 */
	rid = 0;
	bus_set_resource(dev, SYS_RES_IOPORT, rid, ELCR_IOADDR, ELCR_IOLEN);
	sc->elcr_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1,
	    RF_ACTIVE);
	if (sc->elcr_res == NULL)
	    device_printf(dev, "failed to allocate ELCR resource\n");
        break;
    }

    return(0);
}

static int
isab_detach(device_t dev)
{
    struct isab_softc *sc = device_get_softc(dev);

    if (sc->elcr_res != NULL)
	bus_release_resource(dev, SYS_RES_IOPORT, 0, sc->elcr_res);

    return (bus_generic_detach(dev));
}

static int
isab_suspend(device_t dev)
{
    struct isab_softc *sc = device_get_softc(dev);
    bus_space_tag_t bst;
    bus_space_handle_t bsh;
    int i;

    /* Save the ELCR if required. */
    if (sc->elcr_res != NULL) {
	bst = rman_get_bustag(sc->elcr_res);
	bsh = rman_get_bushandle(sc->elcr_res);
	for (i = 0; i < ELCR_IOLEN; i++)
	    sc->saved_elcr[i] = bus_space_read_1(bst, bsh, i);
    }

    return (bus_generic_suspend(dev));
}

static int
isab_resume(device_t dev)
{
    struct isab_softc *sc = device_get_softc(dev);
    bus_space_tag_t bst;
    bus_space_handle_t bsh;
    int i;

    /* Restore the ELCR if required. */
    if (sc->elcr_res != NULL) {
	bst = rman_get_bustag(sc->elcr_res);
	bsh = rman_get_bushandle(sc->elcr_res);
	for (i = 0; i < ELCR_IOLEN; i++)
	    bus_space_write_1(bst, bsh, i, sc->saved_elcr[i]);
    }

    return (bus_generic_resume(dev));
}
