/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Alstom Group.
 * Copyright (c) 2021 Semihalf.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/ctype.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/sysctl.h>

#include <dev/iicbus/iicbus.h>
#include <dev/iicbus/iiconf.h>

#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_bus.h>

#define BIT(x)					(1UL << (x))

/* register map */
#define TMP461_LOCAL_TEMP_REG_MSB		0x0
#define TMP461_LOCAL_TEMP_REG_LSB		0x15
#define TMP461_GLOBAL_TEMP_REG_MSB		0x1
#define TMP461_GLOBAL_TEMP_REG_LSB		0x10
#define TMP461_STATUS_REG			0x2
#define TMP461_STATUS_REG_TEMP_LOCAL		BIT(2)
#define TMP461_CONFIG_REG_R			0x3
#define TMP461_CONFIG_REG_W			0x9
#define TMP461_CONFIG_REG_TEMP_RANGE_BIT	BIT(2)
#define TMP461_CONFIG_REG_STANDBY_BIT		BIT(6)
#define TMP461_CONVERSION_RATE_REG		0x4
#define TMP461_ONESHOT_REG			0xF
#define TMP461_EXTENDED_TEMP_MODIFIER		64

/* 28.4 fixed point representation of 273.15f */
#define TMP461_C_TO_K_FIX			4370

#define TMP461_SENSOR_MAX_CONV_TIME		16000000
#define TMP461_LOCAL_MEASURE			0
#define TMP461_REMOTE_MEASURE			1

/* flags */
#define TMP461_LOCAL_TEMP_DOUBLE_REG		BIT(0)
#define TMP461_REMOTE_TEMP_DOUBLE_REG		BIT(1)

static int tmp461_probe(device_t dev);
static int tmp461_attach(device_t dev);
static int tmp461_read_1(device_t dev, uint8_t reg, uint8_t *data);
static int tmp461_write_1(device_t dev, uint8_t reg, uint8_t data);
static int tmp461_read_temperature(device_t dev, int32_t *temperature, bool mode);
static int tmp461_detach(device_t dev);
static int tmp461_sensor_sysctl(SYSCTL_HANDLER_ARGS);

static device_method_t tmp461_methods[] = {
	DEVMETHOD(device_probe,		tmp461_probe),
	DEVMETHOD(device_attach,	tmp461_attach),
	DEVMETHOD(device_detach,	tmp461_detach),

	DEVMETHOD_END
};

struct tmp461_softc {
	struct mtx		mtx;
	uint8_t			conf;
};

static driver_t tmp461_driver = {
	"tmp461_dev",
	tmp461_methods,
	sizeof(struct tmp461_softc)
};

struct tmp461_data {
	const char	*compat;
	const char	*desc;
	uint8_t		flags;
};

static struct tmp461_data sensor_list[] = {
	{"adt7461", "ADT7461 Thernal Sensor Information",
	    TMP461_REMOTE_TEMP_DOUBLE_REG},
	{"tmp461", "TMP461 Thernal Sensor Information",
	    TMP461_LOCAL_TEMP_DOUBLE_REG | TMP461_REMOTE_TEMP_DOUBLE_REG}
};

static struct ofw_compat_data tmp461_compat_data[] = {
	{"adi,adt7461",		(uintptr_t)&sensor_list[0]},
	{"ti,tmp461",		(uintptr_t)&sensor_list[1]},
	{NULL,			0}
};

DRIVER_MODULE(tmp461, iicbus, tmp461_driver, 0, 0);
IICBUS_FDT_PNP_INFO(tmp461_compat_data);

static int
tmp461_attach(device_t dev)
{
	struct sysctl_oid *sensor_root_oid;
	struct tmp461_data *compat_data;
	struct sysctl_ctx_list *ctx;
	struct tmp461_softc *sc;
	uint8_t data;

	sc = device_get_softc(dev);
	compat_data = (struct tmp461_data *)
	    ofw_bus_search_compatible(dev, tmp461_compat_data)->ocd_data;
	sc->conf = compat_data->flags;
	ctx = device_get_sysctl_ctx(dev);

	mtx_init(&sc->mtx, "tmp461 temperature", "temperature", MTX_DEF);

	sensor_root_oid = SYSCTL_ADD_NODE(ctx, SYSCTL_STATIC_CHILDREN(_hw),
	    OID_AUTO, "temperature", CTLFLAG_RD | CTLFLAG_MPSAFE, NULL,
	    "Thermal Sensor Information");
	if (sensor_root_oid == NULL)
		return (ENXIO);

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(sensor_root_oid), OID_AUTO,
	    "local_sensor", CTLTYPE_INT | CTLFLAG_RD, dev,
	    TMP461_LOCAL_MEASURE, tmp461_sensor_sysctl,
	    "IK1", compat_data->desc);

	/* get status register */
	if (tmp461_read_1(dev, TMP461_STATUS_REG, &data) != 0)
		return (ENXIO);

	if (!(data & TMP461_STATUS_REG_TEMP_LOCAL))
		SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(sensor_root_oid), OID_AUTO,
		    "remote_sensor", CTLTYPE_INT | CTLFLAG_RD, dev,
		    TMP461_REMOTE_MEASURE, tmp461_sensor_sysctl,
		    "IK1", compat_data->desc);

	/* set standby mode */
	if (tmp461_read_1(dev, TMP461_CONFIG_REG_R, &data) != 0)
		return (ENXIO);

	data |= TMP461_CONFIG_REG_STANDBY_BIT;
	if (tmp461_write_1(dev, TMP461_CONFIG_REG_W, data) != 0)
		return (ENXIO);

	return (0);
}

static int
tmp461_probe(device_t dev)
{
	struct tmp461_data *compat_data;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	compat_data = (struct tmp461_data *)
	    ofw_bus_search_compatible(dev, tmp461_compat_data)->ocd_data;
	if (!compat_data)
		return (ENXIO);

	device_set_desc(dev, compat_data->compat);

	return (BUS_PROBE_GENERIC);
}

static int
tmp461_detach(device_t dev)
{
	struct tmp461_softc *sc;

	sc = device_get_softc(dev);
	mtx_destroy(&sc->mtx);

	return (0);
}

static int
tmp461_read_1(device_t dev, uint8_t reg, uint8_t *data)
{
	int error;

	error = iicdev_readfrom(dev, reg, (void *) data, 1, IIC_DONTWAIT);
	if (error != 0)
		device_printf(dev, "Failed to read from device\n");

	return (error);
}

static int
tmp461_write_1(device_t dev, uint8_t reg, uint8_t data)
{
	int error;

	error = iicdev_writeto(dev, reg, (void *) &data, 1, IIC_DONTWAIT);
	if (error != 0)
		device_printf(dev, "Failed to write to device\n");

	return (error);
}

static int
tmp461_read_temperature(device_t dev, int32_t *temperature, bool remote_measure)
{
	uint8_t data, offset, reg;
	struct tmp461_softc *sc;
	int error;

	sc = device_get_softc(dev);

	mtx_lock(&sc->mtx);

	error = tmp461_read_1(dev, TMP461_CONVERSION_RATE_REG, &data);
	if (error != 0)
		goto fail;

	/* trigger sample*/
	error = tmp461_write_1(dev, TMP461_ONESHOT_REG, 0xFF);
	if (error != 0)
		goto fail;

	/* wait for conversion time */
	DELAY(TMP461_SENSOR_MAX_CONV_TIME/(1UL<<data));

	/* read config register offset */
	error = tmp461_read_1(dev, TMP461_CONFIG_REG_R, &data);
	if (error != 0)
		goto fail;

	offset = (data & TMP461_CONFIG_REG_TEMP_RANGE_BIT ?
	    TMP461_EXTENDED_TEMP_MODIFIER : 0);

	reg = remote_measure ?
	    TMP461_GLOBAL_TEMP_REG_MSB : TMP461_LOCAL_TEMP_REG_MSB;

	/* read temeperature value*/
	error = tmp461_read_1(dev, reg, &data);
	if (error != 0)
		goto fail;

	data -= offset;
	*temperature = signed_extend32(data, 0, 8) << 4;

	if (remote_measure) {
		if (sc->conf & TMP461_REMOTE_TEMP_DOUBLE_REG) {
			error = tmp461_read_1(dev,
			    TMP461_GLOBAL_TEMP_REG_LSB, &data);
			if (error != 0)
				goto fail;

			*temperature |= data >> 4;
		}
	} else {
		if (sc->conf & TMP461_LOCAL_TEMP_DOUBLE_REG) {
			error = tmp461_read_1(dev,
			    TMP461_LOCAL_TEMP_REG_LSB, &data);
			if (error != 0)
				goto fail;

			*temperature |= data >> 4;
		}
	}
	*temperature = (((*temperature + TMP461_C_TO_K_FIX) * 10) >> 4);

fail:
	mtx_unlock(&sc->mtx);
	return (error);
}

static int
tmp461_sensor_sysctl(SYSCTL_HANDLER_ARGS)
{
	int32_t temperature;
	device_t dev;
	int error;
	bool mode;

	dev = arg1;
	mode = arg2;

	error = tmp461_read_temperature(dev, &temperature, mode);
	if (error != 0)
		return (error);

	return (sysctl_handle_int(oidp, &temperature, 0, req));
}
