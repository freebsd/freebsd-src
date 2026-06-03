/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Project.
 * All rights reserved.
 *
 * See gyroscope.h for full license text.
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

#include "gyroscope.h"
#include "sensor.h"

#define GYRO_LOG(level, fmt, ...)	do {		\
	printf("gyro: " fmt "\n", ##__VA_ARGS__);\
} while (0)

FEATURE(module_mobile_gyro, "Gyroscope sensor driver");

static struct sensor_t *gyro_sensor;
static enum gyro_range gyro_range = GYRO_RANGE_500DPS;
static float gyro_rate = 100.0f;

static int
gyro_hid_read(struct vector3d *out, int64_t *ts)
{
	int fd;
	char path[64];
	uint8_t raw[8] = { 0 };
	int ret;

	snprintf(path, sizeof(path), GYRO_HID_BASE, 1);
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return (errno);

	ret = read(fd, raw, sizeof(raw));
	close(fd);

	if (ret < (int)sizeof(raw))
		return (EIO);

	*ts = ticks * 1000000ULL / hz;

	out->x = (float)(raw[0] | (raw[1] << 8)) / 32767.0f * gyro_range * GYRO_PI / 180.0f;
	out->y = (float)(raw[2] | (raw[3] << 8)) / 32767.0f * gyro_range * GYRO_PI / 180.0f;
	out->z = (float)(raw[4] | (raw[5] << 8)) / 32767.0f * gyro_range * GYRO_PI / 180.0f;

	return (0);
}

static int
gyro_sensor_read(struct sensor_t *s, struct sensor_data *data)
{
	struct vector3d v;
	int64_t ts;
	int ret;

	ret = gyro_hid_read(&v, &ts);
	if (ret)
		return (ret);

	data->u.vec = v;
	data->timestamp = ts;

	return (0);
}

static int
gyro_sensor_enable(struct sensor_t *s, bool on)
{
	(void)s;
	GYRO_LOG(LOG_DEBUG, "Gyroscope %s", on ? "enabled" : "disabled");

	return (0);
}

static void
gyro_sensor_destroy(struct sensor_t *s)
{
	free(s, M_SENSORS);
	gyro_sensor = NULL;
	GYRO_LOG(LOG_DEBUG, "Gyroscope destroyed");
}

int
gyro_init(void)
{
	struct sensor_info info;

	gyro_sensor = malloc(sizeof(*gyro_sensor), M_SENSORS, M_WAITOK | M_ZERO);
	if (gyro_sensor == NULL)
		return (ENOMEM);

	memset(&info, 0, sizeof(info));
	info.type = SENSOR_TYPE_GYROSCOPE;
	strlcpy(info.name, "mpu6050-gyro", sizeof(info.name));
	info.mode = SENSOR_MODE_CONTINUOUS;
	info.max_range = GYRO_MAX_RANGE_RPS;
	info.resolution = 0.001f;
	info.power_mah = 0.5f;
	info.delay_us = 1000000.0f / gyro_rate;
	info.enabled = 1;
	gyro_sensor->info = info;

	gyro_sensor->read = gyro_sensor_read;
	gyro_sensor->enable = gyro_sensor_enable;
	gyro_sensor->destroy = gyro_sensor_destroy;
	gyro_sensor->priv = NULL;

	GYRO_LOG(LOG_INFO, "Gyroscope initialized (%s, range=%ddps, rate=%.0fHz)",
	    info.name, gyro_range, gyro_rate);

	return (0);
}

int
gyro_read(struct vector3d *out, int64_t *ts)
{
	struct sensor_data data;
	int ret;

	if (gyro_sensor == NULL)
		return (ENXIO);

	ret = gyro_sensor_read(gyro_sensor, &data);
	if (ret)
		return (ret);

	if (out)
		*out = data.u.vec;
	if (ts)
		*ts = data.timestamp;

	return (0);
}

int
gyro_set_range(enum gyro_range range)
{
	if (range != 250 && range != 500 && range != 1000 && range != 2000)
		return (EINVAL);

	gyro_range = range;
	GYRO_LOG(LOG_DEBUG, "Gyroscope range set to %ddps", range);

	return (0);
}

int
gyro_set_rate(float rate_hz)
{
	if (rate_hz < 1.0f || rate_hz > 1000.0f)
		return (EINVAL);

	gyro_rate = rate_hz;

	if (gyro_sensor)
		gyro_sensor->info.delay_us = 1000000.0f / rate_hz;

	GYRO_LOG(LOG_DEBUG, "Gyroscope rate set to %.0fHz", rate_hz);

	return (0);
}

void
gyro_shutdown(void)
{
	if (gyro_sensor) {
		gyro_sensor->destroy(gyro_sensor);
		gyro_sensor = NULL;
	}

	GYRO_LOG(LOG_DEBUG, "Gyroscope shutdown");
}
