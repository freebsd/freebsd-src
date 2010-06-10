/*-
 * Copyright (c) 2010 Nathan Whitehorn
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/cpu.h>
#include <sys/ctype.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/openfirm.h>

struct smu_sensor {
	cell_t	reg;
	char	location[32];
	enum {
		SMU_CURRENT_SENSOR,
		SMU_VOLTAGE_SENSOR,
		SMU_POWER_SENSOR,
		SMU_TEMP_SENSOR
	} type;
};

static int	smusat_probe(device_t);
static int	smusat_attach(device_t);
static int	smusat_sensor_sysctl(SYSCTL_HANDLER_ARGS);

MALLOC_DEFINE(M_SMUSAT, "smusat", "SMU Sattelite Sensors");

static device_method_t  smusat_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		smusat_probe),
	DEVMETHOD(device_attach,	smusat_attach),
	{ 0, 0 },
};

struct smusat_softc {
	struct smu_sensor *sc_sensors;
	int	sc_nsensors;

	uint8_t	sc_cache[16];
	time_t	sc_last_update;
};

static driver_t smusat_driver = {
	"smusat",
	smusat_methods,
	sizeof(struct smusat_softc)
};

static devclass_t smusat_devclass;

DRIVER_MODULE(smusat, iicbus, smusat_driver, smusat_devclass, 0, 0);

static int
smusat_probe(device_t dev)
{
	const char *compat = ofw_bus_get_compat(dev);

	if (compat == NULL || strcmp(compat, "smu-sat") != 0)
		return (ENXIO);

	device_set_desc(dev, "SMU Satellite Sensors");
	return (0);
}

static int
smusat_attach(device_t dev)
{
	phandle_t child;
	struct smu_sensor *sens;
	struct smusat_softc *sc;
	struct sysctl_oid *sensroot_oid;
	struct sysctl_ctx_list *ctx;
	char type[32];
	int i;

	sc = device_get_softc(dev);
	sc->sc_nsensors = 0;
	sc->sc_last_update = 0;

	for (child = OF_child(ofw_bus_get_node(dev)); child != 0;
	    child = OF_peer(child))
		sc->sc_nsensors++;

	if (sc->sc_nsensors == 0) {
		device_printf(dev, "WARNING: No sensors detected!\n");
		return (-1);
	}
	    
	sc->sc_sensors = malloc(sc->sc_nsensors * sizeof(struct smu_sensor),
	    M_SMUSAT, M_WAITOK | M_ZERO);

	sens = sc->sc_sensors;
	sc->sc_nsensors = 0;

	ctx = device_get_sysctl_ctx(dev);
	sensroot_oid = device_get_sysctl_tree(dev);

	for (child = OF_child(ofw_bus_get_node(dev)); child != 0;
	    child = OF_peer(child)) {
		char sysctl_name[40], sysctl_desc[40];
		const char *units;

		sens->reg = 0;
		OF_getprop(child, "reg", &sens->reg, sizeof(sens->reg));
		if (sens->reg < 0x30)
			continue;

		sens->reg -= 0x30;
		OF_getprop(child, "location", sens->location,
		    sizeof(sens->location));

		OF_getprop(child, "device_type", type, sizeof(type));

		if (strcmp(type, "current-sensor") == 0) {
			sens->type = SMU_CURRENT_SENSOR;
			units = "mA";
		} else if (strcmp(type, "temp-sensor") == 0) {
			sens->type = SMU_TEMP_SENSOR;
			units = "C";
		} else if (strcmp(type, "voltage-sensor") == 0) {
			sens->type = SMU_VOLTAGE_SENSOR;
			units = "mV";
		} else if (strcmp(type, "power-sensor") == 0) {
			sens->type = SMU_POWER_SENSOR;
			units = "mW";
		} else {
			continue;
		}

		for (i = 0; i < strlen(sens->location); i++) {
			sysctl_name[i] = tolower(sens->location[i]);
			if (isspace(sysctl_name[i]))
				sysctl_name[i] = '_';
		}
		sysctl_name[i] = 0;

		sprintf(sysctl_desc,"%s (%s)", sens->location, units);
		SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(sensroot_oid), OID_AUTO,
		    sysctl_name, CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, dev,
		    sc->sc_nsensors, smusat_sensor_sysctl, "I", sysctl_desc);

		sens++;
		sc->sc_nsensors++;
	}

	return (0);
}

static int
smusat_updatecache(device_t dev)
{
	uint8_t reg = 0x3f;
	struct smusat_softc *sc = device_get_softc(dev);
	struct iic_msg msgs[2] = {
	    {0, IIC_M_WR | IIC_M_NOSTOP, 1, &reg},
	    {0, IIC_M_RD, 16, sc->sc_cache},
	};

	msgs[0].slave = msgs[1].slave = iicbus_get_addr(dev);
	sc->sc_last_update = time_uptime;

	return (iicbus_transfer(dev, msgs, 2));
}

static int
smusat_sensor_read(device_t dev, struct smu_sensor *sens, int *val)
{
	int value;
	struct smusat_softc *sc;

	sc = device_get_softc(dev);

	if (time_uptime - sc->sc_last_update > 1)
		smusat_updatecache(dev);

	value = (sc->sc_cache[sens->reg*2] << 8) +
	    sc->sc_cache[sens->reg*2 + 1];

	switch (sens->type) {
	case SMU_TEMP_SENSOR:
		/* 16.16 */
		value <<= 10;
		/* Kill the .16 */
		value >>= 16;
		break;
	case SMU_VOLTAGE_SENSOR:
		/* 16.16 */
		value <<= 4;
		/* Kill the .16 */
		value >>= 16;
		break;
	case SMU_CURRENT_SENSOR:
		/* 16.16 */
		value <<= 8;
		/* Kill the .16 */
		value >>= 16;
		break;
	case SMU_POWER_SENSOR:
		/* Doesn't exist */
		break;
	}

	*val = value;
	return (0);
}

static int
smusat_sensor_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev;
	struct smusat_softc *sc;
	struct smu_sensor *sens;
	int value, error;

	dev = arg1;
	sc = device_get_softc(dev);
	sens = &sc->sc_sensors[arg2];

	error = smusat_sensor_read(dev, sens, &value);
	if (error != 0)
		return (error);

	error = sysctl_handle_int(oidp, &value, 0, req);

	return (error);
}

