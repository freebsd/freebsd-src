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

#ifndef _MOBILE_SENSORS_GYRO_H_
#define _MOBILE_SENSORS_GYRO_H_

#include <sys/types.h>
#include <sys/param.h>
#include <stdint.h>
#include <stdbool.h>
#include "../sensors/sensor.h"

#define GYRO_MAX_RANGE_RPS	34.90658f
#define GYRO_PI		3.14159265358979323846f

enum gyro_range {
	GYRO_RANGE_250DPS  = 250,
	GYRO_RANGE_500DPS  = 500,
	GYRO_RANGE_1000DPS = 1000,
	GYRO_RANGE_2000DPS = 2000,
};

#define GYRO_IOCTL_GETRANGE	_IOWR('G', 0x01, enum gyro_range)
#define GYRO_IOCTL_SETRANGE	_IOW('G',  0x02, enum gyro_range)
#define GYRO_IOCTL_GETSAMPLERATE	_IOWR('G', 0x03, float)
#define GYRO_IOCTL_SETSAMPLERATE	_IOW('G',  0x04, float)

#define GYRO_HID_BASE		"/sys/class/hidraw/hidraw%d"

struct gyro_config {
	enum gyro_range range;
	float sample_rate_hz;
	bool fifo_enabled;
	float bandwidth_hz;
};

struct gyro_sample {
	struct vector3d data;
	int64_t timestamp;
};

int gyro_init(void);
int gyro_read(struct vector3d *out, int64_t *ts);
int gyro_set_range(enum gyro_range range);
int gyro_set_rate(float rate_hz);
void gyro_shutdown(void);

#endif /* _MOBILE_SENSORS_GYRO_H_ */
