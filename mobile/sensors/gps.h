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

#ifndef _MOBILE_SENSORS_GPS_H_
#define _MOBILE_SENSORS_GPS_H_

#include <sys/types.h>
#include <sys/param.h>
#include <stdint.h>
#include <stdbool.h>

#define GPS_MAX_SATS	12

enum gps_fix_quality {
	GPS_FIX_NONE,
	GPS_FIX_GPS,
	GPS_FIX_DGPS,
	GPS_FIX_RTK_FIXED,
	GPS_FIX_RTK_FLOAT,
	GPS_FIX_DEAD_RECKONING,
	GPS_FIX_MAX,
};

enum gps_mode {
	GPS_MODE_OFF,
	GPS_MODE_ONE_SHOT,
	GPS_MODE_CONTINUOUS,
	GPS_MODE_MAX,
};

struct gps_location {
	double lat;
	double lon;
	double alt;
	float speed;
	float bearing;
	float accuracy;
	time_t	timestamp;
};

struct gps_satellite {
	uint32_t prn;
	float elevation;
	float azimuth;
	float snr;
};

struct gps_status {
	enum gps_fix_quality fix;
	uint32_t sats_in_use;
	uint32_t sats_in_view;
	struct gps_satellite sats[GPS_MAX_SATS];
};

#define GPS_IOCTL_START		_IOW('R', 0x01, enum gps_mode)
#define GPS_IOCTL_STOP		_IO('R',  0x02)
#define GPS_IOCTL_GETLOC	_IOWR('R', 0x03, struct gps_location)
#define GPS_IOCTL_GETSTATUS	_IOWR('R', 0x04, struct gps_status)
#define GPS_IOCTL_GETMODE	_IOWR('R', 0x05, enum gps_mode)
#define GPS_IOCTL_SETRATE	_IOW('R',  0x06, uint32_t)
#define GPS_IOCTL_SETMININTERVAL	_IOW('R',  0x07, uint32_t)
#define GPS_IOCTL_STOPSUPL	_IO('R',  0x08)
#define GPS_IOCTL_INJECTLOC	_IOW('R',  0x09, struct gps_location)
#define GPS_IOCTL_DELETEAIDING	_IO('R',  0x0A)
#define GPS_IOCTL_SETPROXY	_IOW('R',  0x0B, int)

#define GPS_SERIAL_PATH		"/dev/gps"
#define GPS_SHMEM_PATH		"/dev/shm/gps"
#define GPS_AGPS_PATH		"/var/lib/gps"
#define GPS_DEFAULT_RATE_MS	1000

struct gps_context {
	int fd;
	enum gps_mode mode;
	uint32_t rate_ms;
	struct gps_location last_loc;
	struct gps_status status;
	uint8_t running;
};

int gps_init(void);
int gps_start(enum gps_mode mode);
int gps_stop(void);
int gps_get_location(struct gps_location *loc);
int gps_get_status(struct gps_status *st);
int gps_set_rate(uint32_t rate_ms);
int gps_inject_location(const struct gps_location *loc);
int gps_delete_aiding_data(void);
int gps_set_proxy(int fd);
void gps_shutdown(void);

#endif /* _MOBILE_SENSORS_GPS_H_ */
