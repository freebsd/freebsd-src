/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Project.
 * All rights reserved.
 *
 * See accelerometer.h for full license text.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/time.h>

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <math.h>

#include "accelerometer.h"
#include "sensor.h"

#define ACCEL_LOG(level, fmt, ...)	do {		\
	printf("accel: " fmt "\n", ##__VA_ARGS__);\
} while (0)

FEATURE(module_mobile_accel, "Accelerometer sensor driver");

static struct sensor_t *accel_sensor;
static enum accel_range accel_range = ACCEL_RANGE_4G;
static float accel_rate = 50.0f;
static bool accel_fifo = false;

static int
accel_hid_read(struct vector3d *out, int64_t *ts)
{
	int fd;
	char path[64];
	uint8_t raw[8] = { 0 };
	int ret;

	snprintf(path, sizeof(path), ACCEL_HID_BASE, 0);
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return (errno);

	ret = read(fd, raw, sizeof(raw));
	close(fd);

	if (ret < (int)sizeof(raw))
		return (EIO);

	*ts = ticks * 1000000ULL / hz;

	out->x = (float)(raw[0] | (raw[1] << 8)) / 32767.0f * accel_range;
	out->y = (float)(raw[2] | (raw[3] << 8)) / 32767.0f * accel_range;
	out->z = (float)(raw[4] | (raw[5] << 8)) / 32767.0f * accel_range;

	return (0);
}

static int
accel_sensor_read(struct sensor_t *s, struct sensor_data *data)
{
	struct vector3d v;
	int64_t ts;
	int ret;

	ret = accel_hid_read(&v, &ts);
	if (ret)
		return (ret);

	data->u.vec = v;
	data->timestamp = ts;

	return (0);
}

static int
accel_sensor_enable(struct sensor_t *s, bool on)
{
	(void)s;
	ACCEL_LOG(LOG_DEBUG, "Accelerometer %s", on ? "enabled" : "disabled");

	return (0);
}

static void
accel_sensor_destroy(struct sensor_t *s)
{
	free(s, M_SENSORS);
	accel_sensor = NULL;
	ACCEL_LOG(LOG_DEBUG, "Accelerometer destroyed");
}

int
accel_init(void)
{
	struct sensor_info info;

	accel_sensor = malloc(sizeof(*accel_sensor), M_SENSORS, M_WAITOK | M_ZERO);
	if (accel_sensor == NULL)
		return (ENOMEM);

	memset(&info, 0, sizeof(info));
	info.type = SENSOR_TYPE_ACCELEROMETER;
	strlcpy(info.name, "mpu6050-accel", sizeof(info.name));
	info.mode = SENSOR_MODE_CONTINUOUS;
	info.max_range = ACCEL_MAX_RANGE_MPS2;
	info.resolution = 0.001f;
	info.power_mah = 0.5f;
	info.delay_us = 1000000.0f / accel_rate;
	info.enabled = 1;
	accel_sensor->info = info;

	accel_sensor->read = accel_sensor_read;
	accel_sensor->enable = accel_sensor_enable;
	accel_sensor->destroy = accel_sensor_destroy;
	accel_sensor->priv = NULL;

	accel_set_range(ACCEL_RANGE_4G);

	ACCEL_LOG(LOG_INFO, "Accelerometer initialized (%s, range=%dg, rate=%.0fHz)",
	    info.name, accel_range, accel_rate);

	return (0);
}

int
accel_read(struct vector3d *out, int64_t *ts)
{
	struct sensor_data data;
	int ret;

	if (accel_sensor == NULL)
		return (ENXIO);

	ret = accel_sensor_read(accel_sensor, &data);
	if (ret)
		return (ret);

	if (out)
		*out = data.u.vec;
	if (ts)
		*ts = data.timestamp;

	return (0);
}

int
accel_set_range(enum accel_range range)
{
	if (range != 2 && range != 4 && range != 8 && range != 16)
		return (EINVAL);

	accel_range = range;
	ACCEL_LOG(LOG_DEBUG, "Accelerometer range set to %dg", range);

	return (0);
}

int
accel_set_rate(float rate_hz)
{
	if (rate_hz < 1.0f || rate_hz > 1000.0f)
		return (EINVAL);

	accel_rate = rate_hz;

	if (accel_sensor)
		accel_sensor->info.delay_us = 1000000.0f / rate_hz;

	ACCEL_LOG(LOG_DEBUG, "Accelerometer rate set to %.0fHz", rate_hz);

	return (0);
}

int
accel_get_fifo_count(uint32_t *count)
{
	if (count == NULL)
		return (EINVAL);

	*count = accel_fifo ? ACCEL_FIFO_SIZE : 0;
	return (0);
}

int
accel_flush_fifo(void)
{
	if (!accel_fifo)
		return (EINVAL);

	ACCEL_LOG(LOG_DEBUG, "FIFO flushed");
	return (0);
}

void
accel_shutdown(void)
{
	if (accel_sensor) {
		accel_sensor->destroy(accel_sensor);
		accel_sensor = NULL;
	}

	ACCEL_LOG(LOG_DEBUG, "Accelerometer shutdown");
}
