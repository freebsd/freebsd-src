/*
 * Copyright (c) 2022 Mark Kettenis <kettenis@openbsd.org>
 * Copyright (c) 2023-2024 Jari Sihvola <jsihv@gmx.com>
 * Copyright (c) 2026 Brian Scott <bscott@bunyatech.com.au>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/resource.h>

#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

/* Registers */
#define	TEMP		0x0000
#define	TEMP_RSTN	(1 << 0)
#define	TEMP_PD		(1 << 1)
#define	TEMP_RUN	(1 << 2)
#define	TEMP_DOUT_MASK	0x0fff0000
#define	TEMP_DOUT_SHIFT	16

/*
 * These calibration numbers are based on the openbsd driver.
 *
 * The y intercept is -81.1C (192.05K). The slope is roughly 0.058012C/bit
 * (expressed so integer arrithmetic can work).
 *
 * The result is in milliKelvin hence the numerator and offset being factored
 * up by 1000. The 4094 looks like an error (i.e. should be 4095) but could be
 * an artifact from the methods used to calculate the slope value.
 */
#define	TEMP_Y_INTERCEPT	192050
#define	TEMP_SLOPE		237500
#define	TEMP_RANGE		4094

struct jh7110_temp_softc {
	device_t		dev;
	struct resource		*res;
	struct mtx		mtx;
	clk_t			bus;
	hwreset_t		rst_bus;
	clk_t			sense;
	hwreset_t		rst_sense;
	struct sysctl_oid	*sysctl[2];
};

static struct ofw_compat_data compat_data[] = {
	{ "starfive,jh7100-temp", 1 },
	{ "starfive,jh7110-temp", 1 },
	{ NULL, 0 }
};

static struct resource_spec jh7110_temp_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE },
	{ -1, 0 }
};

#define	RD4(sc, reg)		bus_read_4((sc)->res, (reg))
#define	WR4(sc, reg, val)	bus_write_4((sc)->res, (reg), (val))

#define	JH7110_TEMP_LOCK(_sc)		mtx_lock(&(_sc)->mtx)
#define	JH7110_TEMP_UNLOCK(_sc)		mtx_unlock(&(_sc)->mtx)
#define	JH7110_TEMP_ASSERT_LOCKED(_sc)	mtx_assert(&(_sc)->mtx, MA_OWNED)

static int32_t
jh7110_temp_get_temperature(struct jh7110_temp_softc *sc)
{
	int32_t value;

	JH7110_TEMP_LOCK(sc);
	value = RD4(sc, TEMP);
	value = (value & TEMP_DOUT_MASK) >> TEMP_DOUT_SHIFT;
	JH7110_TEMP_UNLOCK(sc);

	return (value * TEMP_SLOPE) / TEMP_RANGE + TEMP_Y_INTERCEPT;
}

static int
jh7110_temperature(SYSCTL_HANDLER_ARGS)
{
	struct jh7110_temp_softc *sc = arg1;
	int val;
	int err;
	/* get realtime value */
	val = jh7110_temp_get_temperature(sc);

	err = sysctl_handle_int(oidp, &val, 0, req);
	if (err != 0 || !req->newptr) /* error || read request */
		return (err);

	return (0);
}

static int
jh7110_temp_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "StarFive JH7110 Temperature sensor");

	return (BUS_PROBE_DEFAULT);
}

static int
jh7110_temp_detach(device_t dev)
{
	struct jh7110_temp_softc *sc;

	sc = device_get_softc(dev);

	bus_release_resources(dev, jh7110_temp_spec, &sc->res);
	if (sc->bus != NULL)
		clk_release(sc->bus);
	if (sc->sense != NULL)
		clk_release(sc->sense);
	if (sc->rst_bus != NULL)
		hwreset_release(sc->rst_bus);
	if (sc->rst_sense != NULL)
		hwreset_release(sc->rst_sense);
	if (sc->sysctl[0] != NULL)
		sysctl_remove_oid(sc->sysctl[0],1,0);
	if (sc->sysctl[1] != NULL)
		sysctl_remove_oid(sc->sysctl[1],1,0);

	mtx_destroy(&sc->mtx);

	return (0);
}

static int
jh7110_temp_attach(device_t dev)
{
	struct jh7110_temp_softc *sc;
	device_t cpu;
	struct sysctl_ctx_list *ctx;
	int rv;

	sc = device_get_softc(dev);
	sc->dev = dev;

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	if ((rv = bus_alloc_resources(dev, jh7110_temp_spec, &sc->res)) != 0) {
		device_printf(dev, "Could not allocate resources: %d\n", rv);
		mtx_destroy(&sc->mtx);
		return (ENXIO);
	}

	/* Find the clocks and set them up */
	if ((rv = clk_get_by_ofw_name(dev, 0, "bus", &sc->bus)) != 0) {
		device_printf(dev, "Cannot get bus clock: %d\n", rv);
		jh7110_temp_detach(dev);
		return (ENXIO);
	}
	if ((rv = clk_enable(sc->bus)) != 0) {
		device_printf(dev, "Could not enable bus clock %s: %d\n",
			clk_get_name(sc->bus), rv);
		jh7110_temp_detach(dev);
		return (ENXIO);
	}

	if ((rv = clk_get_by_ofw_name(dev, 0, "sense", &sc->sense)) != 0) {
		device_printf(dev, "Cannot get sense clock: %d\n", rv);
		jh7110_temp_detach(dev);
		return (ENXIO);
	}
	if ((rv = clk_enable(sc->sense)) != 0) {
		device_printf(dev, "Could not enable sense clock %s: %d\n",
			clk_get_name(sc->sense), rv);
		jh7110_temp_detach(dev);
		return (ENXIO);
	}

	/* Find the clock resets and de-assert them */
	if ((rv = hwreset_get_by_ofw_name(dev, 0, "bus", &sc->rst_bus)) != 0) {
		device_printf(sc->dev, "cannot get 'rst_bus' reset: %d\n", rv);
		jh7110_temp_detach(dev);
		return (ENXIO);
	}
	if ((rv = hwreset_deassert(sc->rst_bus)) != 0) {
		device_printf(sc->dev, "cannot deassert 'bus' reset: %d\n", rv);
		jh7110_temp_detach(dev);
		return (ENXIO);
	}
	if ((rv = hwreset_get_by_ofw_name(dev, 0, "sense",
	    &sc->rst_sense)) != 0) {
		device_printf(sc->dev, "cannot get 'rst_sense' reset: %d\n",
		    rv);
		jh7110_temp_detach(dev);
		return (ENXIO);
        }
	if ((rv = hwreset_deassert(sc->rst_sense)) != 0) {
		device_printf(sc->dev, "cannot deassert 'sense' reset: %d\n",rv);
		jh7110_temp_detach(dev);
		return (ENXIO);
	}

	/* (uncritically) from the openbsd driver */

	JH7110_TEMP_LOCK(sc);
	/* Power down */
	WR4(sc, TEMP, TEMP_PD);
	DELAY(1);

	/* Power up with reset asserted */
	WR4(sc, TEMP, 0);
	DELAY(60);

	/* Deassert reset */
	WR4(sc, TEMP, TEMP_RSTN);
	DELAY(1);

	/* Start measuring */
	WR4(sc, TEMP, TEMP_RSTN | TEMP_RUN);
	JH7110_TEMP_UNLOCK(sc);

	/* add human readable temperature to dev.cpu node */
	ctx = device_get_sysctl_ctx(dev);
	sc->sysctl[0] = SYSCTL_ADD_PROC(ctx,
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "temperature",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, sc, 0,
	    jh7110_temperature, "IK3",
	    "Current SoC temperature");
	/* Make a copy where the raspberry pi puts the temperature */
	cpu = device_lookup_by_name("cpu0");
	if (cpu != NULL) {
		ctx = device_get_sysctl_ctx(cpu);
		sc->sysctl[1] = SYSCTL_ADD_PROC(ctx,
		    SYSCTL_CHILDREN(device_get_sysctl_tree(cpu)), OID_AUTO,
		    "temperature",
		    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, sc, 0,
		    jh7110_temperature, "IK3",
		    "Current SoC temperature");
	}

	return (0);
}

static device_method_t jh7110_temp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		jh7110_temp_probe),
	DEVMETHOD(device_attach,	jh7110_temp_attach),
	DEVMETHOD(device_detach,	jh7110_temp_detach),

	DEVMETHOD_END
};

DEFINE_CLASS(jh7110_temp, jh7110_temp_methods, sizeof(struct jh7110_temp_softc));
DRIVER_MODULE(jh7110_temp, simplebus, jh7110_temp_class, NULL, NULL);
