/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Project.
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

#ifndef _MOBILE_SENSORS_SENSOR_H_
#define _MOBILE_SENSORS_SENSOR_H_

#include <sys/types.h>
#include <sys/param.h>
#include <stdint.h>
#include <stdbool.h>

#define SEN_MAX_SENSORS	32

enum sensor_type {
	SENSOR_TYPE_ACCELEROMETER,
	SENSOR_TYPE_GYROSCOPE,
	SENSOR_TYPE_MAGNETOMETER,
	SENSOR_TYPE_BAROMETER,
	SENSOR_TYPE_LIGHT,
	SENSOR_TYPE_PROXIMITY,
	SENSOR_TYPE_STEP_DETECTOR,
	SENSOR_TYPE_STEP_COUNTER,
	SENSOR_TYPE_HEART_RATE,
	SENSOR_TYPE_GPS,
	SENSOR_TYPE_TEMPERATURE,
	SENSOR_TYPE_HUMIDITY,
	SENSOR_TYPE_MAX,
};

enum sensor_report_mode {
	SENSOR_MODE_ON_CHANGE,
	SENSOR_MODE_CONTINUOUS,
	SENSOR_MODE_ONE_SHOT,
};

struct vector3d {
	float x;
	float y;
	float z;
};

struct sensor_data {
	union {
		struct vector3d vec;
		float scalar;
		struct {
			double lat;
			double lon;
			double alt;
			float speed;
			float bearing;
			float accuracy;
		} gps;
	} u;
	int64_t timestamp;
};

struct sensor_info {
	enum sensor_type type;
	char name[64];
	enum sensor_report_mode mode;
	float max_range;
	float resolution;
	float power_mah;
	float delay_us;
	uint8_t enabled;
};

struct sensor_t {
	struct sensor_info info;
	void *priv;
	int (*read)(struct sensor_t *s, struct sensor_data *data);
	int (*enable)(struct sensor_t *s, bool on);
	void (*destroy)(struct sensor_t *s);
};

#define SENSOR_IOCTL_GETINFO	_IOWR('S', 0x01, struct sensor_info)
#define SENSOR_IOCTL_READ	_IOWR('S', 0x02, struct sensor_data)
#define SENSOR_IOCTL_ENABLE	_IOW('S',  0x03, bool)
#define SENSOR_IOCTL_SETDELAY	_IOW('S',  0x04, uint32_t)
#define SENSOR_IOCTL_ACTIVATE	_IOW('S',  0x05, bool)
#define SENSOR_IOCTL_FLUSH	_IO('S',  0x06)

#define SENSORS_BASE_DIR		"/sys/class/sensors"
#define SENSORS_HIDRAW_BASE		"/sys/class/hidraw"

int sensor_init(void);
struct sensor_t *sensor_register(enum sensor_type type, const char *name,
    float rate_hz, void *priv);
int sensor_start(struct sensor_t *s);
int sensor_stop(struct sensor_t *s);
int sensor_read(struct sensor_t *s, struct sensor_data *data);
int sensor_unregister(struct sensor_t *s);
struct sensor_t *sensor_get(enum sensor_type type);
int sensor_set_delay(struct sensor_t *s, float rate_hz);
void sensor_get_info(struct sensor_t *s, struct sensor_info *info);
const char *sensor_type_name(enum sensor_type type);

#endif /* _MOBILE_SENSORS_SENSOR_H_ */
