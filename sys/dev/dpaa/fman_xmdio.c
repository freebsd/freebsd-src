/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Justin Hibbits
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/socket.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <dev/mdio/mdio.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "fman.h"
#include "miibus_if.h"
#include "mdio_if.h"

#define MDIO_LOCK()	mtx_lock(&sc->sc_lock)
#define MDIO_UNLOCK()	mtx_unlock(&sc->sc_lock)
#define	MDIO_WRITE4(sc, r, v) \
	bus_write_4(sc->sc_res, r, v)
#define	MDIO_READ4(sc, r) \
	bus_read_4(sc->sc_res, r)

#define	MDIO_CFG		0x30
#define	  CFG_ENC45		  0x00000040
#define	MDIO_STAT		0x30
#define	  STAT_BUSY		  0x80000000
#define	  STAT_MDIO_RD_ER	  0x00000002
#define	MDIO_CTL		0x34
#define	  CTL_READ		  0x00008000
#define	MDIO_DATA		0x38
#define	MDIO_ADDR		0x3c

static int xmdio_fdt_probe(device_t dev);
static int xmdio_fdt_attach(device_t dev);
static int xmdio_detach(device_t dev);
static int xmdio_miibus_readreg(device_t dev, int phy, int reg);
static int xmdio_miibus_writereg(device_t dev, int phy, int reg, int value);
static int xmdio_mdio_readextreg(device_t dev, int phy, int devad, int reg);
static int xmdio_mdio_writeextreg(device_t dev, int phy, int devad, int reg,
    int val);

struct xmdio_softc {
	struct mtx sc_lock;
	struct resource *sc_res;
};

static struct ofw_compat_data mdio_compat_data[] = {
	{"fsl,fman-memac-mdio", 0},
	{"fsl,fman-xmdio", 0},
	{NULL, 0}
};

static device_method_t xmdio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		xmdio_fdt_probe),
	DEVMETHOD(device_attach,	xmdio_fdt_attach),
	DEVMETHOD(device_detach,	xmdio_detach),
	DEVMETHOD(bus_add_child,	bus_generic_add_child),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	xmdio_miibus_readreg),
	DEVMETHOD(miibus_writereg,	xmdio_miibus_writereg),

	/* MDIO interface */
	DEVMETHOD(mdio_readreg,		xmdio_miibus_readreg),
	DEVMETHOD(mdio_writereg,	xmdio_miibus_writereg),
	DEVMETHOD(mdio_readextreg,	xmdio_mdio_readextreg),
	DEVMETHOD(mdio_writeextreg,	xmdio_mdio_writeextreg),

	DEVMETHOD_END
};

static driver_t xmdio_driver = {
	"xmdio",
	xmdio_methods,
	sizeof(struct xmdio_softc),
};

EARLY_DRIVER_MODULE(xmdio, fman, xmdio_driver, 0, 0,
    BUS_PASS_SUPPORTDEV);
DRIVER_MODULE(miibus, xmdio, miibus_driver, 0, 0);
DRIVER_MODULE(mdio, xmdio, mdio_driver, 0, 0);
MODULE_DEPEND(xmdio, miibus, 1, 1, 1);

static int
xmdio_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, mdio_compat_data)->ocd_str)
		return (ENXIO);

	device_set_desc(dev, "Freescale XGMAC MDIO");

	return (BUS_PROBE_DEFAULT);
}

static int
xmdio_fdt_attach(device_t dev)
{
	struct xmdio_softc *sc;

	sc = device_get_softc(dev);

	sc->sc_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, 0, RF_ACTIVE);

	OF_device_register_xref(OF_xref_from_node(ofw_bus_get_node(dev)), dev);

	mtx_init(&sc->sc_lock, device_get_nameunit(dev), "XMDIO lock",
	    MTX_DEF);

	return (0);
}

static int
xmdio_detach(device_t dev)
{
	struct xmdio_softc *sc;

	sc = device_get_softc(dev);

	mtx_destroy(&sc->sc_lock);

	return (0);
}

static void
set_clause45(struct xmdio_softc *sc)
{
	uint32_t reg;

	reg = MDIO_READ4(sc, MDIO_CFG);
	MDIO_WRITE4(sc, MDIO_CFG, reg | CFG_ENC45);
}

static void
set_clause22(struct xmdio_softc *sc)
{
	uint32_t reg;

	reg = MDIO_READ4(sc, MDIO_CFG);
	MDIO_WRITE4(sc, MDIO_CFG, reg & ~CFG_ENC45);
}

static int
xmdio_wait_no_busy(struct xmdio_softc *sc)
{
	uint32_t count, val;

	for (count = 1000; count > 0; count--) {
		val = MDIO_READ4(sc, MDIO_CFG);
		if ((val & STAT_BUSY) == 0)
			break;
		DELAY(1);
	}

	if (count == 0)
		return (0xffff);

	return (0);
}

int
xmdio_miibus_readreg(device_t dev, int phy, int reg)
{
	struct xmdio_softc *sc;
	int                  rv;
	uint32_t ctl;

	sc = device_get_softc(dev);

	MDIO_LOCK();

	set_clause22(sc);
	ctl = (phy << 5) | reg;
	MDIO_WRITE4(sc, MDIO_CTL, ctl | CTL_READ);

	MDIO_READ4(sc, MDIO_CTL);

	if (xmdio_wait_no_busy(sc))
		rv = 0xffff;
	else
		rv = MDIO_READ4(sc, MDIO_DATA);

	MDIO_WRITE4(sc, MDIO_CTL, 0);
	MDIO_UNLOCK();

	return (rv);
}

int
xmdio_miibus_writereg(device_t dev, int phy, int reg, int value)
{
	struct xmdio_softc *sc;

	sc = device_get_softc(dev);

	MDIO_LOCK();
	set_clause22(sc);
	/* Stop the MII management read cycle */
	MDIO_WRITE4(sc, MDIO_CTL, (phy << 5) | reg);

	MDIO_WRITE4(sc, MDIO_DATA, value);

	/* Wait till MII management write is complete */
	xmdio_wait_no_busy(sc);
	MDIO_UNLOCK();

	return (0);
}

static int
xmdio_mdio_readextreg(device_t dev, int phy, int devad, int reg)
{
	struct xmdio_softc *sc;
	int                  rv;
	uint32_t ctl;

	sc = device_get_softc(dev);

	MDIO_LOCK();

	set_clause45(sc);
	ctl = (phy << 5) | devad;
	MDIO_WRITE4(sc, MDIO_CTL, ctl);
	MDIO_WRITE4(sc, MDIO_ADDR, reg);
	xmdio_wait_no_busy(sc);
	MDIO_WRITE4(sc, MDIO_CTL, ctl | CTL_READ);
	MDIO_READ4(sc, MDIO_CTL);

	xmdio_wait_no_busy(sc);

	if (MDIO_READ4(sc, MDIO_STAT) & STAT_MDIO_RD_ER)
		rv = 0xffff;
	else
		rv = MDIO_READ4(sc, MDIO_DATA);

	MDIO_WRITE4(sc, MDIO_CTL, 0);
	MDIO_UNLOCK();

	return (rv);
}

static int
xmdio_mdio_writeextreg(device_t dev, int phy, int devad, int reg, int val)
{
	struct xmdio_softc *sc;

	sc = device_get_softc(dev);

	MDIO_LOCK();
	set_clause45(sc);
	/* Stop the MII management read cycle */
	MDIO_WRITE4(sc, MDIO_CTL, (phy << 5) | devad);

	MDIO_WRITE4(sc, MDIO_DATA, val);

	/* Wait till MII management write is complete */
	xmdio_wait_no_busy(sc);
	MDIO_UNLOCK();

	return (0);
}

