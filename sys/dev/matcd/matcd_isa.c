/*matcd_isa.c----------------------------------------------------------------

	Matsushita(Panasonic) / Creative CD-ROM Driver	(matcd)
	Authored by Frank Durda IV

Copyright 1994, 1995, 2002, 2003  Frank Durda IV.  All rights reserved.
"FDIV" is a trademark of Frank Durda IV.

------------------------------------------------------------------------------

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of the author nor the names of their contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

-----------------------------------------------------------------------------*/

/* $FreeBSD$
*/

/*---------------------------------------------------------------------------*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/bio.h>
#include <sys/cdio.h>
#include <sys/disklabel.h>
#include <sys/bus.h>

#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <isa/isavar.h>

#include <dev/matcd/options.h>
#include <dev/matcd/matcd_data.h>

extern int matcd_probe(struct matcd_softc *sc);
extern int matcd_attach(struct matcd_softc *sc);

static int	matcd_isa_probe	(device_t);
static int	matcd_isa_attach	(device_t);
static int	matcd_isa_detach	(device_t);

static int	matcd_alloc_resources	(device_t);
static void	matcd_release_resources	(device_t);


static int matcd_isa_probe (device_t dev)
{
	struct matcd_softc *	sc;
	int	error;

	/*Any Plug aNd Pray Support?*/
	if (isa_get_vendorid(dev)) {
		return (ENXIO);
	}

	if (bus_get_resource_start(dev,SYS_RES_IOPORT,0)==0) {
		return (ENXIO);
	}

	sc = device_get_softc(dev);
	sc->dev=dev;
	sc->port_rid=0;
	sc->port_type=SYS_RES_IOPORT;

	error=matcd_alloc_resources(dev);
	if (error==0) {
		error = matcd_probe(sc);
#if 0
		if (error==0) {
			device_set_desc(dev, "Matsushita CR-562/CR-563");
		}
#endif /*0*/
	}
	matcd_release_resources(dev);
	return (error);
}


static int matcd_isa_attach (device_t dev)
{
	struct matcd_softc *	sc;
	int error;

	sc=device_get_softc(dev);
	error=0;

	sc->dev=dev;
	sc->port_rid=0;
	sc->port_type=SYS_RES_IOPORT;

	error=matcd_alloc_resources(dev);
	if (error==0) {
		error=matcd_probe(sc);		/*Redundant Probe*/
		if (error==0) {
			error=matcd_attach(sc);
			if (error==0) {
				return(error);
			}
		}
	}
	matcd_release_resources(dev);
	return (error);
}


static int matcd_isa_detach (device_t dev)
{
	struct matcd_softc *	sc;

	sc=device_get_softc(dev);
	destroy_dev(sc->matcd_dev_t);
	matcd_release_resources(dev);
	return(0);
}


static int matcd_alloc_resources (device_t dev)
{
	struct matcd_softc *	sc;

	sc = device_get_softc(dev);
	if (sc->port_type) {
		sc->port=bus_alloc_resource(dev, sc->port_type, &sc->port_rid,
					    0, ~0, 1, RF_ACTIVE);
		if (sc->port == NULL) {
			device_printf(dev,
				      "Port resource not available.\n");
			return(ENOMEM);
		}
		sc->port_bst = rman_get_bustag(sc->port);
		sc->port_bsh = rman_get_bushandle(sc->port);
	}
	return(0);
}


static void matcd_release_resources (device_t dev)
{
	struct matcd_softc *	sc;

	sc = device_get_softc(dev);
	if (sc->port) {
		bus_release_resource(dev, sc->port_type, sc->port_rid,
				     sc->port);
		sc->port_bst=0;
		sc->port_bsh=0;
	}
	return;
}


static device_method_t matcd_isa_methods[] = {
	DEVMETHOD(device_probe,         matcd_isa_probe),
	DEVMETHOD(device_attach,        matcd_isa_attach),
	DEVMETHOD(device_detach,        matcd_isa_detach),
	{ 0, 0 }
};

static driver_t matcd_isa_driver = {
	"matcd",
	matcd_isa_methods,
	sizeof(struct matcd_softc)
};

static devclass_t	matcd_devclass;

DRIVER_MODULE(matcd, isa, matcd_isa_driver, matcd_devclass, NULL, 0);

/*End of matcd_isa.c*/

