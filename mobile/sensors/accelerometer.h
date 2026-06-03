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

#ifndef _MOBILE_SENSORS_ACCEL_H_
#define _MOBILE_SENSORS_ACCEL_H_

#include <sys/types.h>
#include <sys/param.h>
#include <stdint.h>
#include <stdbool.h>
#include "../sensors/sensor.h"

#define ACCEL_MAX_RANGE_MPS2	19.6133f

enum accel_range {
	ACCEL_RANGE_2G  = 2,
	ACCEL_RANGE_4G  = 4,
	ACCEL_RANGE_8G  = 8,
	ACCEL_RANGE_16G = 16,
};

#define ACCEL_IOCTL_GETRANGE	_IOWR('A', 0x01, enum accel_range)
#define ACCEL_IOCTL_SETRANGE	_IOW('A',  0x02, enum accel_range)
#define ACCEL_IOCTL_GETFIFO	_IOWR('A', 0x03, uint32_t)
#define ACCEL_IOCTL_FLUSHFIFO	_IO('A',  0x04)

#define ACCEL_HID_BASE		"/sys/class/hidraw/hidraw%d"
#define ACCEL_FIFO_SIZE		1024

struct accel_config {
	enum accel_range range;
	float sample_rate_hz;
	bool fifo_enabled;
	uint16_t fifo_threshold;
};

struct accel_sample {
	struct vector3d data;
	int64_t timestamp;
};

int accel_init(void);
int accel_read(struct vector3d *out, int64_t *ts);
int accel_set_range(enum accel_range range);
int accel_set_rate(float rate_hz);
int accel_get_fifo_count(uint32_t *count);
int accel_flush_fifo(void);
void accel_shutdown(void);

#endif /* _MOBILE_SENSORS_ACCEL_H_ */
