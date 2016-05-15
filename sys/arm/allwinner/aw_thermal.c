/*-
 * Copyright (c) 2016 Jared McNeill <jmcneill@invisible.ca>
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Allwinner thermal sensor controller
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/module.h>
#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/allwinner/aw_sid.h>

#define	THS_CTRL0		0x00
#define	THS_CTRL2		0x40
#define	 SENSOR_ACQ1_SHIFT	16
#define	 SENSOR2_EN		(1 << 2)
#define	 SENSOR1_EN		(1 << 1)
#define	 SENSOR0_EN		(1 << 0)
#define	THS_INTC		0x44
#define	THS_INTS		0x48
#define	THS_FILTER		0x70
#define	 FILTER_EN		(1 << 2)
#define	THS_CALIB0		0x74
#define	THS_CALIB1		0x78
#define	THS_DATA0		0x80
#define	THS_DATA1		0x84
#define	THS_DATA2		0x88
#define	 DATA_MASK		0xfff

#define	TEMP_BASE		2719
#define	TEMP_MUL		1000
#define	TEMP_DIV		14186
#define	TEMP_TO_K		273
#define	ADC_ACQUIRE_TIME	(24 - 1)
#define	SENSOR_ENABLE_ALL	(SENSOR0_EN|SENSOR1_EN|SENSOR2_EN)

enum aw_thermal_sensor {
	THS_SENSOR_CPU_CLUSTER0,
	THS_SENSOR_CPU_CLUSTER1,
	THS_SENSOR_GPU,
	THS_SENSOR_END = -1
};

struct aw_thermal_sensor_config {
	enum aw_thermal_sensor	sensor;
	const char		*name;
	const char		*desc;
};

static const struct aw_thermal_sensor_config a83t_sensor_config[] = {
	{ .sensor = THS_SENSOR_CPU_CLUSTER0,
	  .name = "cluster0",	.desc = "CPU cluster 0 temperature" },
	{ .sensor = THS_SENSOR_CPU_CLUSTER1,
	  .name = "cluster1",	.desc = "CPU cluster 1 temperature" },
	{ .sensor = THS_SENSOR_GPU,
	  .name = "gpu",	.desc = "GPU temperature" },
	{ .sensor = THS_SENSOR_END }
};

static struct ofw_compat_data compat_data[] = {
	{ "allwinner,sun8i-a83t-ts",	(uintptr_t)&a83t_sensor_config },
	{ NULL,				(uintptr_t)NULL }
};

#define	THS_CONF(d)		\
	(void *)ofw_bus_search_compatible((d), compat_data)->ocd_data

struct aw_thermal_softc {
	struct resource			*res;
	struct aw_thermal_sensor_config	*conf;
};

static struct resource_spec aw_thermal_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

#define	RD4(sc, reg)		bus_read_4((sc)->res, (reg))
#define	WR4(sc, reg, val)	bus_write_4((sc)->res, (reg), (val))

static int
aw_thermal_init(struct aw_thermal_softc *sc)
{
	uint32_t calib0, calib1;
	int error;

	/* Read calibration settings from SRAM */
	error = aw_sid_read_tscalib(&calib0, &calib1);
	if (error != 0)
		return (error);

	/* Write calibration settings to thermal controller */
	WR4(sc, THS_CALIB0, calib0);
	WR4(sc, THS_CALIB1, calib1);

	/* Configure ADC acquire time (CLK_IN/(N+1)) and enable sensors */
	WR4(sc, THS_CTRL0, ADC_ACQUIRE_TIME);
	WR4(sc, THS_CTRL2, (ADC_ACQUIRE_TIME << SENSOR_ACQ1_SHIFT) |
	    SENSOR_ENABLE_ALL);

	/* Disable interrupts */
	WR4(sc, THS_INTC, 0);
	WR4(sc, THS_INTS, RD4(sc, THS_INTS));

	/* Enable average filter */
	WR4(sc, THS_FILTER, RD4(sc, THS_FILTER) | FILTER_EN);

	return (0);
}

static int
aw_thermal_gettemp(uint32_t val)
{
	int raw;

	raw = val & DATA_MASK;
	return (((TEMP_BASE - raw) * TEMP_MUL) / TEMP_DIV) + TEMP_TO_K;
}

static int
aw_thermal_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct aw_thermal_softc *sc;
	enum aw_thermal_sensor sensor;
	int val;

	sc = arg1;
	sensor = arg2;

	val = aw_thermal_gettemp(RD4(sc, THS_DATA0 + (sensor * 4)));

	return sysctl_handle_opaque(oidp, &val, sizeof(val), req);
}

static int
aw_thermal_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (THS_CONF(dev) == NULL)
		return (ENXIO);

	device_set_desc(dev, "Allwinner Thermal Sensor Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
aw_thermal_attach(device_t dev)
{
	struct aw_thermal_softc *sc;
	int i;

	sc = device_get_softc(dev);

	sc->conf = THS_CONF(dev);

	if (bus_alloc_resources(dev, aw_thermal_spec, &sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		return (ENXIO);
	}

	if (aw_thermal_init(sc) != 0)
		return (ENXIO);

	for (i = 0; sc->conf[i].sensor != THS_SENSOR_END; i++)
		SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
		    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
		    OID_AUTO, sc->conf[i].name,
		    CTLTYPE_INT | CTLFLAG_RD,
		    sc, sc->conf[i].sensor, aw_thermal_sysctl, "IK0",
		    sc->conf[i].desc);

	return (0);
}

static device_method_t aw_thermal_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		aw_thermal_probe),
	DEVMETHOD(device_attach,	aw_thermal_attach),

	DEVMETHOD_END
};

static driver_t aw_thermal_driver = {
	"aw_thermal",
	aw_thermal_methods,
	sizeof(struct aw_thermal_softc),
};

static devclass_t aw_thermal_devclass;

DRIVER_MODULE(aw_thermal, simplebus, aw_thermal_driver, aw_thermal_devclass,
    0, 0);
MODULE_VERSION(aw_thermal, 1);
