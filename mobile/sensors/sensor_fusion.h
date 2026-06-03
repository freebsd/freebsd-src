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

#ifndef _MOBILE_SENSORS_FUSION_H_
#define _MOBILE_SENSORS_FUSION_H_

#include <sys/types.h>
#include <sys/param.h>
#include <stdint.h>
#include <stdbool.h>
#include "../sensors/sensor.h"

#define SF_FUSION_ACCEL		(1U << 0)
#define SF_FUSION_GYRO		(1U << 1)
#define SF_FUSION_MAG		(1U << 2)
#define SF_FUSION_BARO		(1U << 3)
#define SF_FUSION_STEP		(1U << 4)
#define SF_FUSION_GPS		(1U << 5)

struct quaternion_t {
	float w;
	float x;
	float y;
	float z;
};

struct rotation_vector_t {
	struct quaternion_t q;
	struct vector3d gyro;
	int64_t timestamp;
};

struct euler_angles {
	float roll;
	float pitch;
	float yaw;
};

#define SF_IOCTL_ENABLE		_IOW('F', 0x01, uint32_t)
#define SF_IOCTL_DISABLE		_IO('F',  0x02)
#define SF_IOCTL_GETORIENTATION	_IOWR('F', 0x03, struct quaternion_t)
#define SF_IOCTL_GETROTATION	_IOWR('F', 0x04, struct rotation_vector_t)
#define SF_IOCTL_GETEULER	_IOWR('F', 0x05, struct euler_angles)
#define SF_IOCTL_SETRATE	_IOW('F',  0x06, uint32_t)
#define SF_IOCTL_SETDAMP	_IOW('F',  0x07, float)
#define SF_IOCTL_GETSTEPCOUNT	_IOWR('F', 0x08, uint64_t)

#define SF_MAX_WINDOW		128

struct sf_context {
	void *accel_handle;
	void *gyro_handle;
	void *mag_handle;

	struct sf_fifo_entry {
		struct vector3d data;
		int64_t timestamp;
		enum sensor_type type;
	} fifo[SF_MAX_WINDOW];

	uint32_t f_head;
	uint32_t f_tail;

	struct quaternion_t q;
	struct euler_angles ang;
	float alpha;

	uint64_t step_count;
	bool step_detected;

	uint32_t fusion_enabled;
	float rate_hz;
	uint8_t running;
};

int sf_init(void);
int sf_enable_fusion(uint32_t types);
int sf_disable_fusion(void);
int sf_get_orientation(struct quaternion_t *q);
int sf_get_rotation_vector(struct rotation_vector_t *rv);
int sf_get_euler(struct euler_angles *ang);
int sf_set_rate(float hz);
int sf_set_dampening(float alpha);
int sf_get_step_count(uint64_t *count);
int sf_reset_step_count(void);
int sf_feed_accel(const struct vector3d *acc, int64_t ts);
int sf_feed_gyro(const struct vector3d *gyr, int64_t ts);
int sf_feed_mag(const struct vector3d *mag, int64_t ts);
void sf_madgwick_update(const struct vector3d *acc, const struct vector3d *gyr,
    const struct vector3d *mag, float dt);
void sf_complementary_filter(const struct vector3d *acc, const struct vector3d *gyr,
    float dt);
void sf_shutdown(void);

#endif /* _MOBILE_SENSORS_FUSION_H_ */
