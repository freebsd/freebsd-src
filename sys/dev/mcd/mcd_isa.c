/*
 * $FreeBSD$
 */

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

#include <dev/mcd/mcdreg.h>
#include <dev/mcd/mcdvar.h>

static int	mcd_isa_probe	(device_t);
static int	mcd_isa_attach	(device_t);
static int	mcd_isa_detach	(device_t);

static int	mcd_alloc_resources	(device_t);
static void	mcd_release_resources	(device_t);

static int
mcd_isa_probe (device_t dev)
{
	struct mcd_softc *	sc;
	int			error;

	/* No pnp support */
	if (isa_get_vendorid(dev))
		return (ENXIO);

	/* IO port must be configured. */
	if (bus_get_resource_start(dev, SYS_RES_IOPORT, 0) == 0)
		return (ENXIO);

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->port_rid = 0;
	sc->port_type = SYS_RES_IOPORT;
	error = mcd_alloc_resources(dev);
	if (error)
		goto fail;

	error = mcd_probe(sc);
	if (error) {
		device_printf(dev, "Probe failed.\n");
		goto fail;
	}

	device_set_desc(dev, sc->data.name);

fail:
	mcd_release_resources(dev);
	return (error);
}

static int
mcd_isa_attach (device_t dev)
{
	struct mcd_softc *	sc;
	int			error;

	sc = device_get_softc(dev);
	error = 0;

	sc->dev = dev;
	sc->port_rid = 0;
	sc->port_type = SYS_RES_IOPORT;
	error = mcd_alloc_resources(dev);
	if (error)
		goto fail;

	error = mcd_probe(sc);
	if (error) {
		device_printf(dev, "Re-Probe failed.\n");
		goto fail;
	}

	error = mcd_attach(sc);
	if (error) {
		device_printf(dev, "Attach failed.\n");
		goto fail;
	}

	return (0);
fail:
	mcd_release_resources(dev);
	return (error);
}

static int
mcd_isa_detach (device_t dev)
{
	struct mcd_softc *	sc;
	int			error;

	sc = device_get_softc(dev);
	error = 0;

	destroy_dev(sc->mcd_dev_t);

	mcd_release_resources(dev);

	return (error);
}

static int
mcd_alloc_resources (device_t dev)
{
	struct mcd_softc *	sc;
	int			error;

	sc = device_get_softc(dev);
	error = 0;

	if (sc->port_type) {
		sc->port = bus_alloc_resource(dev, sc->port_type, &sc->port_rid,
				0, ~0, 1, RF_ACTIVE);
		if (sc->port == NULL) {
			device_printf(dev, "Unable to allocate PORT resource.\n");
			error = ENOMEM;
			goto bad;
		}
		sc->port_bst = rman_get_bustag(sc->port);
		sc->port_bsh = rman_get_bushandle(sc->port);
	}

	if (sc->irq_type) {
		sc->irq = bus_alloc_resource(dev, sc->irq_type, &sc->irq_rid,
				0, ~0, 1, RF_ACTIVE);
		if (sc->irq == NULL) {
			device_printf(dev, "Unable to allocate IRQ resource.\n");
			error = ENOMEM;
			goto bad;
		}
	}

	if (sc->drq_type) {
		sc->drq = bus_alloc_resource(dev, sc->drq_type, &sc->drq_rid,
				0, ~0, 1, RF_ACTIVE);
		if (sc->drq == NULL) {
			device_printf(dev, "Unable to allocate DRQ resource.\n");
			error = ENOMEM;
			goto bad;
		}
	}

	mtx_init(&sc->mtx, device_get_nameunit(dev),
		"Interrupt lock", MTX_DEF | MTX_RECURSE);

bad:
	return (error);
}

void
mcd_release_resources (device_t dev)
{
	struct mcd_softc *	sc;

	sc = device_get_softc(dev);

	if (sc->irq_ih)
		bus_teardown_intr(dev, sc->irq, sc->irq_ih);
	if (sc->port) {
		bus_release_resource(dev, sc->port_type, sc->port_rid, sc->port);
		sc->port_bst = 0;
		sc->port_bsh = 0;
	}
	if (sc->irq)
		bus_release_resource(dev, sc->irq_type, sc->irq_rid, sc->irq);
	if (sc->drq)
		bus_release_resource(dev, sc->drq_type, sc->drq_rid, sc->drq);

	if (mtx_initialized(&sc->mtx) != 0)
		mtx_destroy(&sc->mtx);

	return;
}

static device_method_t mcd_isa_methods[] = {
	DEVMETHOD(device_probe,         mcd_isa_probe),
	DEVMETHOD(device_attach,        mcd_isa_attach),
	DEVMETHOD(device_detach,        mcd_isa_detach),

	{ 0, 0 }
};

static driver_t mcd_isa_driver = {
	"mcd",
	mcd_isa_methods,
	sizeof(struct mcd_softc)
};

static devclass_t	mcd_devclass;

DRIVER_MODULE(mcd, isa, mcd_isa_driver, mcd_devclass, NULL, 0);
