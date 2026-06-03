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
 *    notice, this list of conditions and the following following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
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

#ifndef _MOBILE_POWER_THERMAL_H_
#define _MOBILE_POWER_THERMAL_H_

#include <sys/types.h>
#include <sys/param.h>
#include <stdint.h>
#include <stdbool.h>

#define TH_MAX_ZONES		8
#define TH_MAX_CDEVS		16
#define TH_MAX_TRIP_HANDLERS	16

enum th_trip_type {
	TH_TRIP_PASSIVE,
	TH_TRIP_ACTIVE,
	TH_TRIP_CRITICAL,
	TH_TRIP_HOT,
	TH_TRIP_MAX
};

enum th_cooling_dev_type {
	TH_CDEV_PASSIVE,
	TH_CDEV_ACTIVE_FAN,
	TH_CDEV_ACTIVE_GPU,
	TH_CDEV_ACTIVE_CPU,
	TH_CDEV_MAX
};

struct thermal_zone {
	uint32_t	id;
	int32_t		temp_c;
	int32_t		critical_c;
	int32_t		passive_c;
	char		type[32];
	enum th_trip_type	trip;
	uint8_t		enabled;
};

struct cooling_dev {
	uint32_t	id;
	enum th_cooling_dev_type	type;
	uint32_t	state;
	uint32_t	max_state;
	uint8_t		online;
};

struct th_trip_handler {
	uint32_t		zone_id;
	enum th_trip_type	trip;
	void		(*callback)(uint32_t zone_id, int32_t temp_c);
	void		*arg;
	uint8_t		active;
};

#define TH_IOCTL_GETTEMP		_IOWR('T', 0x01, uint32_t)
#define TH_IOCTL_GETZONES		_IOWR('T', 0x02, struct thermal_zone)
#define TH_IOCTL_GETCDEVS		_IOWR('T', 0x03, struct cooling_dev)
#define TH_IOCTL_CDEVSTATE		_IOWR('T', 0x04, struct cooling_dev)
#define TH_IOCTL_SETCDEVSTATE		_IOW('T',  0x05, struct cooling_dev)
#define TH_IOCTL_TRIPREG		_IOW('T',  0x06, struct th_trip_handler)
#define TH_IOCTL_TRIPUNREG		_IOW('T',  0x07, uint32_t)

#define TH_ZONE_BASE		"/sys/class/thermal/thermal_zone%d"
#define TH_CDEV_BASE		"/sys/class/thermal/cooling_device%d"
#define TH_ZONE_TEMP		TH_ZONE_BASE "/temp"
#define TH_ZONE_TYPE		TH_ZONE_BASE "/type"
#define TH_ZONE_CRIT		TH_ZONE_BASE "/trip_point_0_temp"
#define TH_ZONE_PASSIVE		TH_ZONE_BASE "/trip_point_1_temp"
#define TH_CDEV_TYPE		TH_CDEV_BASE "/type"
#define TH_CDEV_CUR		TH_CDEV_BASE "/cur_state"
#define TH_CDEV_MAX		TH_CDEV_BASE "/max_state"

int th_init(void);
int th_get_temps(struct thermal_zone zones[TH_MAX_ZONES], uint32_t *count);
int th_get_cooling_devices(struct cooling_dev devs[TH_MAX_CDEVS], uint32_t *count);
int th_cooling_set_state(uint32_t dev_id, uint32_t state);
int th_cooling_get_state(uint32_t dev_id, uint32_t *state);
int th_register_trip_handler(uint32_t zone_id, enum th_trip_type trip,
    void (*callback)(uint32_t, int32_t), void *arg);
int th_unregister_trip_handler(uint32_t id);
int th_monitor_start(void);
int th_monitor_stop(void);
bool th_is_critical(uint32_t zone_id);
int32_t th_get_zone_temp(uint32_t zone_id);
const char *th_trip_type_name(enum th_trip_type trip);

#endif /* _MOBILE_POWER_THERMAL_H_ */
