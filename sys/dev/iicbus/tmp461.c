/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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
__FBSDID("$FreeBSD$");

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/bus.h>
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

#define TMP461_LOCAL_TEMP_REG_MSB		0x0
#define TMP461_LOCAL_TEMP_REG_LSB		0x15
#define TMP461_GLOBAL_TEMP_REG_MSB		0x1
#define TMP461_GLOBAL_TEMP_REG_LSB		0x10
#define TMP461_CONFIG_REG			0x3
#define TMP461_CONFIG_REG_TEMP_RANGE_BIT	BIT(2)

#define TMP461_EXTENDED_TEMP_MODIFIER		64
/* 28.4 fixed point representation of 273.15f */
#define TMP461_C_TO_K_FIX			4370
#define TMP461_TEMP_LSB				0

static int tmp461_probe(device_t dev);
static int tmp461_attach(device_t dev);
static int tmp461_read_1(device_t dev, uint8_t reg, uint8_t *data);
static int tmp461_read_temp(device_t dev, int32_t *temp);
static int tmp461_detach(device_t dev);
static int tmp461_sensor_sysctl(SYSCTL_HANDLER_ARGS);
static int32_t tmp461_signed_extend32(uint32_t value, int sign_pos);

static device_method_t tmp461_methods[] = {
	DEVMETHOD(device_probe,		tmp461_probe),
	DEVMETHOD(device_attach,	tmp461_attach),
	DEVMETHOD(device_detach,	tmp461_detach),

	DEVMETHOD_END
};

static driver_t tmp461_driver = {
	"tmp461_dev",
	tmp461_methods,
	0
};

static devclass_t tmp461_devclass;

static struct ofw_compat_data tmp461_compat_data[] = {
	{ "ti,tmp461",		1 },
	{ NULL,			0 }
};

DRIVER_MODULE(tmp461, iicbus, tmp461_driver, tmp461_devclass, 0, 0);
IICBUS_FDT_PNP_INFO(tmp461_compat_data);

static int
tmp461_attach(device_t dev)
{
	struct sysctl_oid *sensor_root_oid;
	struct sysctl_ctx_list *ctx;

	ctx = device_get_sysctl_ctx(dev);
	
	sensor_root_oid = SYSCTL_ADD_NODE(ctx, SYSCTL_STATIC_CHILDREN(_hw),
	    OID_AUTO, "temperature", CTLFLAG_RD | CTLFLAG_MPSAFE, NULL,
	    "TMP 461 Thermal Sensor Information");
	if (sensor_root_oid == NULL)
		return (ENXIO);

	SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(sensor_root_oid), OID_AUTO,
	    "tmp461", CTLTYPE_INT | CTLFLAG_RD, dev, 0,
	    tmp461_sensor_sysctl, "IK0", "TMP461 Thermal Sensor");

	return (0);
}

static int
tmp461_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev, tmp461_compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "TMP461 Thermal Sensor");

	return (BUS_PROBE_GENERIC);
}

static int
tmp461_detach(device_t dev)
{

	return (0);
}

static int
tmp461_read_1(device_t dev,  uint8_t reg, uint8_t *data)
{
	int error;

	error = iicdev_readfrom(dev, reg, (void *) data, 1, IIC_WAIT);
	if (error != 0)
		device_printf(dev, "Failed to read from device\n");

	return (error);
}

static int
tmp461_read_temp(device_t dev, int32_t *temp)
{
	bool extended_mode;
	uint8_t data;
	int error;

	/* read temperature range */
	error = tmp461_read_1(dev, TMP461_CONFIG_REG, &data);
	if (error != 0)
		return (ENXIO);

	extended_mode = data & TMP461_CONFIG_REG_TEMP_RANGE_BIT;

	/* read temp MSB */
	error = tmp461_read_1(dev, TMP461_LOCAL_TEMP_REG_MSB, &data);
	if (error != 0)
		return (ENXIO);

	*temp = signed_extend32(data, TMP461_TEMP_LSB, 8) -
	    (extended_mode ? TMP461_EXTENDED_TEMP_MODIFIER : 0);

	error = tmp461_read_1(dev, TMP461_LOCAL_TEMP_REG_LSB, &data);
	if (error != 0)
		return (ENXIO);

	*temp = (((*temp << 4) | (data >> 4)) + TMP461_C_TO_K_FIX) >> 4;

	return (0);
}

static int
tmp461_sensor_sysctl(SYSCTL_HANDLER_ARGS)
{
	device_t dev;
	int32_t temp;
	int error;

	dev = arg1;

	error = tmp461_read_temp(dev, &temp);
	if (error != 0)
		return (error);

	return (sysctl_handle_int(oidp, &temp, 0, req));
}

