/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Rubicon Communications, LLC (Netgate)
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/intr.h>
#include <dev/syscon/syscon.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "syscon_if.h"

#define	CONTROL0		0	/* Offset in config->regs[] array */
#define	 CONTROL0_TSEN_START	(1 << 0)
#define	 CONTROL0_TSEN_RESET	(1 << 1)
#define	 CONTROL0_TSEN_EN	(1 << 2)
#define	 CONTROL0_CHANNEL_SHIFT	13
#define	 CONTROL0_CHANNEL_MASK	0xF
#define	 CONTROL0_OSR_SHIFT	24
#define	 CONTROL0_OSR_MAX	3	/* OSR = 512 * 4uS = ~2mS */
#define	 CONTROL0_MODE_SHIFT	30
#define	 CONTROL0_MODE_EXTERNAL	0x2
#define	 CONTROL0_MODE_MASK	0x3

#define	CONTROL1		1	/* Offset in config->regs[] array */
/* This doesn't seems to work */
#define	CONTROL1_TSEN_SENS_SHIFT	21
#define	CONTROL1_TSEN_SENS_MASK		0x7

#define	STATUS			2	/* Offset in config->regs[] array */
#define	STATUS_TEMP_MASK	0x3FF

enum mv_thermal_type {
	MV_AP806 = 1,
	MV_CP110,
};

struct mv_thermal_config {
	enum mv_thermal_type	type;
	int			regs[3];
	int			ncpus;
	int64_t			calib_mul;
	int64_t			calib_add;
	int64_t			calib_div;
	uint32_t		valid_mask;
	bool			signed_value;
};

struct mv_thermal_softc {
	device_t		dev;
	struct syscon		*syscon;
	struct mtx		mtx;

	struct mv_thermal_config *config;
	int			cur_sensor;
};

static struct mv_thermal_config mv_ap806_config = {
	.type = MV_AP806,
	.regs = {0x84, 0x88, 0x8C},
	.ncpus = 4,
	.calib_mul = 423,
	.calib_add = -150000,
	.calib_div = 100,
	.valid_mask = (1 << 16),
	.signed_value = true,
};

static struct mv_thermal_config mv_cp110_config = {
	.type = MV_CP110,
	.regs = {0x70, 0x74, 0x78},
	.calib_mul = 2000096,
	.calib_add = 1172499100,
	.calib_div = 420100,
	.valid_mask = (1 << 10),
	.signed_value = false,
};

static struct ofw_compat_data compat_data[] = {
	{"marvell,armada-ap806-thermal", (uintptr_t) &mv_ap806_config},
	{"marvell,armada-cp110-thermal", (uintptr_t) &mv_cp110_config},
	{NULL,             0}
};

#define	RD4(sc, reg)							\
    SYSCON_READ_4((sc)->syscon, sc->config->regs[reg])
#define	WR4(sc, reg, val)						\
	SYSCON_WRITE_4((sc)->syscon, sc->config->regs[reg], (val))

static inline int32_t sign_extend(uint32_t value, int index)
{
	uint8_t shift;

	shift = 31 - index;
	return ((int32_t)(value << shift) >> shift);
}

static int
mv_thermal_wait_sensor(struct mv_thermal_softc *sc)
{
	uint32_t reg;
	uint32_t timeout;

	timeout = 100000;
	while (--timeout > 0) {
		reg = RD4(sc, STATUS);
		if ((reg & sc->config->valid_mask) == sc->config->valid_mask)
			break;
		DELAY(100);
	}
	if (timeout == 0) {
		return (ETIMEDOUT);
	}

	return (0);
}

static int
mv_thermal_select_sensor(struct mv_thermal_softc *sc, int sensor)
{
	uint32_t reg;

	if (sc->cur_sensor == sensor)
		return (0);

	/* Stop the current reading and reset the module */
	reg = RD4(sc, CONTROL0);
	reg &= ~(CONTROL0_TSEN_START | CONTROL0_TSEN_EN);
	WR4(sc, CONTROL0, reg);

	/* Switch to the selected sensor */
	/*
	 * NOTE : Datasheet says to use CONTROL1 for selecting
	 * but when doing so the sensors >0 are never ready
	 * Do what Linux does using undocumented bits in CONTROL0
	 */
	/* This reset automatically to the sensor 0 */
	reg &= ~(CONTROL0_MODE_MASK << CONTROL0_MODE_SHIFT);
	if (sensor) {
		/* Select external sensor */
		reg |= CONTROL0_MODE_EXTERNAL << CONTROL0_MODE_SHIFT;
		reg &= ~(CONTROL0_CHANNEL_MASK << CONTROL0_CHANNEL_SHIFT);
		reg |= (sensor - 1) << CONTROL0_CHANNEL_SHIFT;
	}
	WR4(sc, CONTROL0, reg);
	sc->cur_sensor = sensor;

	/* Start the reading */
	reg = RD4(sc, CONTROL0);
	reg |= CONTROL0_TSEN_START | CONTROL0_TSEN_EN;
	WR4(sc, CONTROL0, reg);

	return (mv_thermal_wait_sensor(sc));
}

static int
mv_thermal_read_sensor(struct mv_thermal_softc *sc, int sensor, int *temp)
{
	uint32_t reg;
	int64_t sample, rv;

	rv = mv_thermal_select_sensor(sc, sensor);
	if (rv != 0)
		return (rv);

	reg = RD4(sc, STATUS) & STATUS_TEMP_MASK;

	if (sc->config->signed_value)
		sample = sign_extend(reg, fls(STATUS_TEMP_MASK) - 1);
	else
		sample = reg;

	*temp = ((sample * sc->config->calib_mul) - sc->config->calib_add) /
		sc->config->calib_div;

	return (0);
}

static int
ap806_init(struct mv_thermal_softc *sc)
{
	uint32_t reg;

	/* Start the temp capture/conversion */
	reg = RD4(sc, CONTROL0);
	reg &= ~CONTROL0_TSEN_RESET;
	reg |= CONTROL0_TSEN_START | CONTROL0_TSEN_EN;

	/* Sample every ~2ms */
	reg |= CONTROL0_OSR_MAX << CONTROL0_OSR_SHIFT;

	WR4(sc, CONTROL0, reg);

	/* Since we just started the module wait for the sensor to be ready */
	mv_thermal_wait_sensor(sc);

	return (0);
}

static int
cp110_init(struct mv_thermal_softc *sc)
{
	uint32_t reg;

	reg = RD4(sc, CONTROL1);
	reg &= (1 << 7);
	reg |= (1 << 8);
	WR4(sc, CONTROL1, reg);

	/* Sample every ~2ms */
	reg = RD4(sc, CONTROL0);
	reg |= CONTROL0_OSR_MAX << CONTROL0_OSR_SHIFT;
	WR4(sc, CONTROL0, reg);

	return (0);
}

static int
mv_thermal_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct mv_thermal_softc *sc;
	device_t dev = arg1;
	int sensor = arg2;
	int val = 0;

	sc = device_get_softc(dev);
	mtx_lock(&(sc)->mtx);

	if (mv_thermal_read_sensor(sc, sensor, &val) == 0) {
		/* Convert to Kelvin */
		val = val + 2732;
	} else {
		device_printf(dev, "Timeout waiting for sensor\n");
	}

	mtx_unlock(&(sc)->mtx);
	return sysctl_handle_opaque(oidp, &val, sizeof(val), req);
}

static int
mv_thermal_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Marvell Thermal Sensor Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
mv_thermal_attach(device_t dev)
{
	struct mv_thermal_softc *sc;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *oid;
	char name[255];
	char desc[255];
	int i;

	sc = device_get_softc(dev);
	sc->dev = dev;

	sc->config = (struct mv_thermal_config *)
	    ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	if (SYSCON_GET_HANDLE(sc->dev, &sc->syscon) != 0 ||
	    sc->syscon == NULL) {
		device_printf(dev, "cannot get syscon for device\n");
		return (ENXIO);
	}

	sc->cur_sensor = -1;
	switch (sc->config->type) {
	case MV_AP806:
		ap806_init(sc);
		break;
	case MV_CP110:
		cp110_init(sc);
		break;
	}

	ctx = device_get_sysctl_ctx(dev);
	oid = SYSCTL_CHILDREN(device_get_sysctl_tree(dev));
	/* There is always at least one sensor */
	SYSCTL_ADD_PROC(ctx, oid, OID_AUTO, "internal",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
	    dev, 0, mv_thermal_sysctl,
	    "IK",
	    "Internal Temperature");

	for (i = 0; i < sc->config->ncpus; i++) {
		snprintf(name, sizeof(name), "cpu%d", i);
		snprintf(desc, sizeof(desc), "CPU%d Temperature", i);
		SYSCTL_ADD_PROC(ctx, oid, OID_AUTO, name,
		    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
		    dev, i + 1, mv_thermal_sysctl,
		    "IK",
		    desc);
	}

	return (0);
}

static int
mv_thermal_detach(device_t dev)
{
	return (0);
}

static device_method_t mv_thermal_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mv_thermal_probe),
	DEVMETHOD(device_attach,	mv_thermal_attach),
	DEVMETHOD(device_detach,	mv_thermal_detach),

	DEVMETHOD_END
};

static driver_t mv_thermal_driver = {
	"mv_thermal",
	mv_thermal_methods,
	sizeof(struct mv_thermal_softc),
};

DRIVER_MODULE(mv_thermal, simplebus, mv_thermal_driver, 0, 0);
