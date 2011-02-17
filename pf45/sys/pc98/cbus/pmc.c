/*-
 * PMC (Power Management Controller of NEC PC-98Note) Driver
 *
 * Copyright (c) 2001 Chiharu Shibata.
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
 *    derived from this software without specific prior written permission
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/reboot.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
 
#include <isa/isavar.h>

struct pmc_isa_softc {
	struct	resource	*port_res;
	eventhandler_tag	evt;
	int			flags;
};

static int	pmc_isa_alloc_resources(device_t);
static void	pmc_isa_release_resources(device_t);
static int	pmc_isa_probe(device_t);
static int	pmc_isa_attach(device_t);
static int	pmc_isa_detach(device_t);

#define	PMC_ISA_PORT		0x8f0
#define	PMC_ISA_PORTSIZE	4

#define	sc_inw(sc, port) \
	bus_space_read_2(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), (port))

#define	sc_outw(sc, port, value) \
	bus_space_write_2(rman_get_bustag((sc)->port_res), \
		rman_get_bushandle((sc)->port_res), (port), (value))

static void
pmc_poweroff(void *arg, int howto)
{
	struct pmc_isa_softc *sc = (struct pmc_isa_softc *)arg;

	if (!sc->flags) {
		outb(0x5e8e, inb(0x5e8e) & ~0x11);	/* FDD LED off */
	}

	if (!(howto & RB_POWEROFF)) {
		return;
	}

	sc_outw(sc, 0, 0x0044);
	sc_outw(sc, 2, 1 << 10);
#if 1
	/* for 9801NS/T */
	sc_outw(sc, 0, 0xf00a);
	sc_outw(sc, 2, 1 << 9);
#endif
}

static int
pmc_isa_alloc_resources(device_t dev)
{
	struct pmc_isa_softc *sc = device_get_softc(dev);
	int	rid;

	bzero(sc, sizeof(*sc));

	rid = 0;
	sc->port_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
					  0ul, ~0ul, PMC_ISA_PORTSIZE,
					  RF_ACTIVE);
	if (sc->port_res == NULL) {
		return (ENOMEM);
	}

	return 0;
}

static void
pmc_isa_release_resources(device_t dev)
{
	struct pmc_isa_softc *sc = device_get_softc(dev);

	if (sc->port_res != NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, 0, sc->port_res);
	}
	sc->port_res = NULL;
}

static int
pmc_isa_probe(device_t dev)
{
	struct pmc_isa_softc *sc = device_get_softc(dev);
	u_int	port;
	u_int16_t	save, tmp;

#if 0
	if (isa_get_vendorid(dev)) {
		return ENXIO;
	}
	if (device_get_unit(dev) > 0) {
		printf("pmc: Only one PMC driver supported.\n");
		return ENXIO;
	}
#endif
	port = isa_get_port(dev);
	if (port == -1) {
		port = PMC_ISA_PORT;
	}
	if (bootverbose) {
		device_printf(dev, "port = 0x%x\n", port);
	}

	if (bus_set_resource(dev, SYS_RES_IOPORT, 0, port, PMC_ISA_PORTSIZE)) {
		if (bootverbose) {
			device_printf(dev, "bus_set_resource failed\n");
		}
		return ENXIO;
	}
	if (pmc_isa_alloc_resources(dev)) {
		if (bootverbose) {
			device_printf(dev, "pmc_isa_alloc_resources failed\n");
		}
		return ENXIO;
	}

	/* Check the existence of PMC */
	sc_outw(sc, 0, 0x0052);
	save = sc_inw(sc, 2);
	tmp = save & ~0x3f;
	sc_outw(sc, 2, tmp);
	if (sc_inw(sc, 2) != tmp) {
		if (bootverbose) {
			device_printf(dev, "failed to clear index(0x0052)\n");
		}

		pmc_isa_release_resources(dev);
		return ENXIO;
	}

	tmp |= 0x3e;
	sc_outw(sc, 2, tmp);
	if (sc_inw(sc, 2) != tmp) {
		if (bootverbose) {
			device_printf(dev, "failed to set index(0x0052)\n");
		}

		pmc_isa_release_resources(dev);
		return ENXIO;
	}
	sc_outw(sc, 2, save);

	pmc_isa_release_resources(dev);

	device_set_desc(dev, "Power Management Controller");
	return 0;
}

static int
pmc_isa_attach(device_t dev)
{
	struct pmc_isa_softc *sc = device_get_softc(dev);
	int error;

	error = pmc_isa_alloc_resources(dev);
	if (error) {
		device_printf(dev, "resource allocation failed\n");
		return error;
	}

	/* Power the system off using PMC */
	sc->evt = EVENTHANDLER_REGISTER(shutdown_final, pmc_poweroff, sc,
					SHUTDOWN_PRI_LAST);
	sc->flags = device_get_flags(dev);
	return 0;
}

static int
pmc_isa_detach(device_t dev)
{
	struct pmc_isa_softc *sc = device_get_softc(dev);

	if (bootverbose) {
		device_printf(dev, "pmc_isa_detach called\n");
	}

	if (sc->evt != NULL) {
		EVENTHANDLER_DEREGISTER(shutdown_final, sc->evt);
	}
	sc->evt = NULL;

	pmc_isa_release_resources(dev);
	return 0;
}

static device_method_t pmc_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pmc_isa_probe),
	DEVMETHOD(device_attach,	pmc_isa_attach),
	DEVMETHOD(device_detach,	pmc_isa_detach),
	{0, 0}
};

static driver_t pmc_isa_driver = {
	"pmc",
	pmc_isa_methods, sizeof(struct pmc_isa_softc),
};

devclass_t pmc_devclass;

DRIVER_MODULE(pmc, isa, pmc_isa_driver, pmc_devclass, 0, 0);
