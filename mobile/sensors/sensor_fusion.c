/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Project.
 * All rights reserved.
 *
 * See sensor_fusion.h for full license text.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/sched.h>

#include <string.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>

#include "sensor_fusion.h"

#define SF_LOG(level, fmt, ...)	do {		\
	printf("sf: " fmt "\n", ##__VA_ARGS__);\
} while (0)

FEATURE(module_mobile_sf, "Sensor fusion filter subsystem");

static struct sf_context sf_ctx;

static void
sf_normalize_quat(struct quaternion_t *q)
{
	float norm;

	norm = sqrtf(q->w * q->w + q->x * q->x + q->y * q->y + q->z * q->z);
	if (norm > 0.0001f) {
		norm = 1.0f / norm;
		q->w *= norm;
		q->x *= norm;
		q->y *= norm;
		q->z *= norm;
	}
}

static void
sf_quat_to_euler(const struct quaternion_t *q, struct euler_angles *e)
{
	float sinr_cosp = 2.0f * (q->w * q->x + q->y * q->z);
	float cosr_cosp = 1.0f - 2.0f * (q->x * q->x + q->y * q->y);
	e->roll = atan2f(sinr_cosp, cosr_cosp);

	float sinp = 2.0f * (q->w * q->y - q->z * q->x);
	if (fabsf(sinp) >= 1.0f)
		e->pitch = copysignf(M_PI / 2.0f, sinp);
	else
		e->pitch = asinf(sinp);

	float siny_cosp = 2.0f * (q->w * q->z + q->x * q->y);
	float cosy_cosp = 1.0f - 2.0f * (q->y * q->y + q->z * q->z);
	e->yaw = atan2f(siny_cosp, cosy_cosp);
}

void
sf_madgwick_update(const struct vector3d *acc, const struct vector3d *gyr,
    const struct vector3d *mag, float dt)
{
	float gx = gyr->x, gy = gyr->y, gz = gyr->z;
	float ax = acc->x, ay = acc->y, az = acc->z;
	float mx = mag ? mag->x : 0.0f;
	float my = mag ? mag->y : 0.0f;
	float mz = mag ? mag->z : 0.0f;
	float qw = sf_ctx.q.w, qx = sf_ctx.q.x, qy = sf_ctx.q.y, qz = sf_ctx.q.z;
	float _2q1 = 2.0f * qw, _2q2 = 2.0f * qx, _2q3 = 2.0f * qy;
	float _2q4 = 2.0f * qz;
	float _4q1 = 4.0f * qw, _4q2 = 4.0f * qx;
	float _8q1 = 8.0f * qw, _8q2 = 8.0f * qx;
	float recipNorm;
	float s0, s1, s2, s3;
	float qDot0, qDot1, qDot2, qDot3;
	float hx, hy, _2q1mx, _2q1my, _2q1mz;

	(void)mag;

	recipNorm = 1.0f / sqrtf(ax * ax + ay * ay + az * az);
	ax *= recipNorm;
	ay *= recipNorm;
	az *= recipNorm;

	qDot0 = 0.5f * (-qx * gx - qy * gy - qz * gz);
	qDot1 = 0.5f * (qw * gx + qy * gz - qz * gy);
	qDot2 = 0.5f * (qw * gy - qx * gz + qz * gx);
	qDot3 = 0.5f * (qw * gz + qx * gy - qy * gx);

	if (mag) {
		recipNorm = 1.0f / sqrtf(mx * mx + my * my + mz * mz);
		mx *= recipNorm;
		my *= recipNorm;
		mz *= recipNorm;

		_2q1mx = _2q1 * mx;
		_2q1my = _2q1 * my;
		_2q1mz = _2q1 * mz;
		hx = mx * qw - _2q1my * qz + _2q1mz * qy - mx * qw + _2q2 * qx + _2q3 * qz - _2q4 * qy;
		hy = my * qw + _2q1mx * qz - _2q1mz * qx - my * qw + _2q3 * qx - _2q2 * qz + _2q4 * qx;
		_2q0 = 2.0f * qw;

		s0 = -_2q2 * (2.0f * (qx * qz - qw * qy) - mx) + _2q1 * (2.0f * (qy * qz + qw * qx) - my) - _2q4 * (2.0f * 0.5f - qx * qx - qy * qy) + mx;
		s1 = _2q1 * (2.0f * (qx * qz - qw * qy) - mx) + _2q2 * (2.0f * (qy * qz + qw * qx) - my) - _2q3 * (2.0f * 0.5f - qx * qx - qy * qy) + my;
		s2 = -_2q4 * (2.0f * (qx * qz - qw * qy) - mx) + _2q3 * (2.0f * (qy * qz + qw * qx) - my) - _2q1 * (2.0f * 0.5f - qw * qw - qz * qz) + mz;
		s3 = -_2q1 * (2.0f * (qx * qz - qw * qy) - mx) - _2q4 * (2.0f * (qy * qz + qw * qx) - my) - _2q2 * (2.0f * 0.5f - qw * qw - qz * qz) + mz;
	} else {
		s0 = -_2q2 * (2.0f * (qx * qz - qw * qy) - ax) + _2q1 * (2.0f * (qy * qz + qw * qx) - ay) - _2q4 * (2.0f * 0.5f - qx * qx - qy * qy) + ax;
		s1 = _2q1 * (2.0f * (qx * qz - qw * qy) - ax) + _2q2 * (2.0f * (qy * qz + qw * qx) - ay) - _2q3 * (2.0f * 0.5f - qx * qx - qy * qy) + ay;
		s2 = -_2q4 * (2.0f * (qx * qz - qw * qy) - ax) + _2q3 * (2.0f * (qy * qz + qw * qx) - ay) - _2q1 * (2.0f * 0.5f - qw * qw - qz * qz) + az;
		s3 = -_2q1 * (2.0f * (qx * qz - qw * qy) - ax) - _2q4 * (2.0f * (qy * qz + qw * qx) - ay) - _2q2 * (2.0f * 0.5f - qw * qw - qz * qz) + az;
	}

	recipNorm = 1.0f / sqrtf(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3 + 1e-6f);
	s0 *= recipNorm;
	s1 *= recipNorm;
	s2 *= recipNorm;
	s3 *= recipNorm;

	float beta = 0.04f;
	qDot0 -= beta * s0;
	qDot1 -= beta * s1;
	qDot2 -= beta * s2;
	qDot3 -= beta * s3;

	qw += qDot0 * dt;
	qx += qDot1 * dt;
	qy += qDot2 * dt;
	qz += qDot3 * dt;

	sf_normalize_quat(&sf_ctx.q);

	qw = sf_ctx.q.w;
	sf_ctx.q.w = qw;
	sf_ctx.q.x = qx;
	sf_ctx.q.y = qy;
	sf_ctx.q.z = qz;

	sf_quat_to_euler(&sf_ctx.q, &sf_ctx.ang);
}

void
sf_complementary_filter(const struct vector3d *acc, const struct vector3d *gyr,
    float dt)
{
	float roll, pitch;
	float a_roll, a_pitch;

	a_roll = atan2f(acc->y, acc->z) * 180.0f / GYRO_PI;
	a_pitch = atan2f(-acc->x, acc->z) * 180.0f / GYRO_PI;

	roll = sf_ctx.q.x + gyr->x * dt * 180.0f / GYRO_PI;
	pitch = sf_ctx.q.y + gyr->y * dt * 180.0f / GYRO_PI;

	sf_ctx.ang.roll = sf_ctx.alpha * roll + (1.0f - sf_ctx.alpha) * a_roll;
	sf_ctx.ang.pitch = sf_ctx.alpha * pitch + (1.0f - sf_ctx.alpha) * a_pitch;
	sf_ctx.ang.yaw += gyr->z * dt * 180.0f / GYRO_PI;

	sf_ctx.q.x = sf_ctx.ang.roll;
	sf_ctx.q.y = sf_ctx.ang.pitch;
	sf_ctx.q.z = sf_ctx.ang.yaw;
	sf_ctx.q.w = 1.0f;
}

int
sf_init(void)
{
	memset(&sf_ctx, 0, sizeof(sf_ctx));

	sf_ctx.q.w = 1.0f;
	sf_ctx.q.x = 0.0f;
	sf_ctx.q.y = 0.0f;
	sf_ctx.q.z = 0.0f;
	sf_ctx.alpha = 0.98f;
	sf_ctx.rate_hz = 100.0f;
	sf_ctx.fusion_enabled = SF_FUSION_GYRO;

	SF_LOG(LOG_INFO, "Sensor fusion initialized, rate=%.0fHz", sf_ctx.rate_hz);

	return (0);
}

int
sf_enable_fusion(uint32_t types)
{
	sf_ctx.fusion_enabled = types;
	sf_ctx.running = 1;
	SF_LOG(LOG_INFO, "Fusion enabled: types=0x%08x", types);

	return (0);
}

int
sf_disable_fusion(void)
{
	sf_ctx.running = 0;
	sf_ctx.fusion_enabled = 0;
	SF_LOG(LOG_DEBUG, "Fusion disabled");

	return (0);
}

int
sf_get_orientation(struct quaternion_t *q)
{
	if (q == NULL)
		return (EINVAL);

	*q = sf_ctx.q;
	return (0);
}

int
sf_get_rotation_vector(struct rotation_vector_t *rv)
{
	if (rv == NULL)
		return (EINVAL);

	rv->q = sf_ctx.q;
	rv->timestamp = ticks * 1000000ULL / hz;

	return (0);
}

int
sf_get_euler(struct euler_angles *ang)
{
	if (ang == NULL)
		return (EINVAL);

	*ang = sf_ctx.ang;
	return (0);
}

int
sf_set_rate(float hz)
{
	if (hz < 1.0f || hz > 1000.0f)
		return (EINVAL);

	sf_ctx.rate_hz = hz;
	return (0);
}

int
sf_set_dampening(float alpha)
{
	if (alpha < 0.0f || alpha > 1.0f)
		return (EINVAL);

	sf_ctx.alpha = alpha;
	return (0);
}

int
sf_feed_accel(const struct vector3d *acc, int64_t ts)
{
	if (acc == NULL)
		return (EINVAL);

	if ((sf_ctx.f_head + 1) % SF_MAX_WINDOW != sf_ctx.f_tail) {
		sf_ctx.fifo[sf_ctx.f_head].data = *acc;
		sf_ctx.fifo[sf_ctx.f_head].timestamp = ts;
		sf_ctx.fifo[sf_ctx.f_head].type = SENSOR_TYPE_ACCELEROMETER;
		sf_ctx.f_head = (sf_ctx.f_head + 1) % SF_MAX_WINDOW;
	}

	return (0);
}

int
sf_feed_gyro(const struct vector3d *gyr, int64_t ts)
{
	struct vector3d tmp = *gyr;
	float dt = 1.0f / sf_ctx.rate_hz;
	struct vector3d acc = {0};

	if ((sf_ctx.f_head + 1) % SF_MAX_WINDOW != sf_ctx.f_tail) {
		sf_ctx.fifo[sf_ctx.f_head].data = *gyr;
		sf_ctx.fifo[sf_ctx.f_head].timestamp = ts;
		sf_ctx.fifo[sf_ctx.f_head].type = SENSOR_TYPE_GYROSCOPE;
		sf_ctx.f_head = (sf_ctx.f_head + 1) % SF_MAX_WINDOW;
	}

	if (sf_ctx.f_tail < sf_ctx.f_head && sf_ctx.f_head >= 1) {
		for (uint32_t i = sf_ctx.f_tail; i < sf_ctx.f_head; i++) {
			if (sf_ctx.fifo[i].type == SENSOR_TYPE_MAGNETOMETER) {
				sf_madgwick_update(&acc, &tmp,
				    &sf_ctx.fifo[i].data, dt);
				break;
			}
		}
	} else if (sf_ctx.f_tail > sf_ctx.f_head) {
		sf_complementary_filter(&acc, &tmp, dt);
	}

	return (0);
}

int
sf_feed_mag(const struct vector3d *mag, int64_t ts)
{
	if (mag == NULL)
		return (EINVAL);

	if ((sf_ctx.f_head + 1) % SF_MAX_WINDOW != sf_ctx.f_tail) {
		sf_ctx.fifo[sf_ctx.f_head].data = *mag;
		sf_ctx.fifo[sf_ctx.f_head].timestamp = ts;
		sf_ctx.fifo[sf_ctx.f_head].type = SENSOR_TYPE_MAGNETOMETER;
		sf_ctx.f_head = (sf_ctx.f_head + 1) % SF_MAX_WINDOW;
	}

	return (0);
}

int
sf_get_step_count(uint64_t *count)
{
	if (count == NULL)
		return (EINVAL);

	*count = sf_ctx.step_count;
	return (0);
}

int
sf_reset_step_count(void)
{
	sf_ctx.step_count = 0;
	return (0);
}

void
sf_shutdown(void)
{
	sf_disable_fusion();
	memset(&sf_ctx, 0, sizeof(sf_ctx));
	SF_LOG(LOG_DEBUG, "Sensor fusion shutdown");
}
