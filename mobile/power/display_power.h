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

#ifndef _MOBILE_POWER_DISPLAY_H_
#define _MOBILE_POWER_DISPLAY_H_

#include <sys/types.h>
#include <sys/param.h>
#include <stdint.h>
#include <stdbool.h>

#define DP_MAX_DISPLAYS	4
#define DP_MAX_DIM_LEVELS	256

enum dp_blank_state {
	DP_BLANK_ON,
	DP_BLANK_STANDBY,
	DP_BLANK_SUSPEND,
	DP_BLANK_OFF,
};

enum dp_doze_mode {
	DP_DOZE_NONE,
	DP_DOZE_LOW_POWER,
};

struct dp_display_info {
	uint32_t id;
	char name[64];
	uint32_t width;
	uint32_t height;
	uint32_t refresh_hz;
	uint32_t max_brightness;
	struct {
		int32_t		min_cd_m2;
		int32_t		max_cd_m2;
	} brightness_range;
	uint8_t	hdr;
};

struct dp_ambient_info {
	uint32_t sensor_id;
	int32_t	light_lux;
	int32_t	wavelength;
};

#define DP_IOCTL_GETBRIGHTNESS	_IOWR('D', 0x01, uint32_t)
#define DP_IOCTL_SETBRIGHTNESS	_IOW('D',  0x02, uint32_t)
#define DP_IOCTL_GETBLANK	_IOWR('D', 0x03, enum dp_blank_state)
#define DP_IOCTL_SETBLANK	_IOW('D',  0x04, enum dp_blank_state)
#define DP_IOCTL_GETINFO	_IOWR('D', 0x05, struct dp_display_info)
#define DP_IOCTL_SETAMBIENT	_IOW('D', 0x06, struct dp_ambient_info)
#define DP_IOCTL_SETDOZE	_IOW('D',  0x07, enum dp_doze_mode)
#define DP_IOCTL_GETDOZE	_IOWR('D', 0x08, enum dp_doze_mode)
#define DP_IOCTL_DIMENABLE	_IO('D',  0x09)
#define DP_IOCTL_DIMDISABLE	_IO('D',  0x0A)
#define DP_IOCTL_DIMTIMEOUT	_IOW('D',  0x0B, uint32_t)

#define DP_SYSFS_BRIGHTNESS	"/sys/class/backlight/%s/brightness"
#define DP_SYSFS_MAX_BRIGHT	"/sys/class/backlight/%s/max_brightness"
#define DP_SYSFS_ACTUAL		"/sys/class/backlight/%s/actual_brightness"
#define DP_SYSFS_AMBIENT	"/sys/class/backlight/%s/ambient_mode"
#define DP_SYSFS_DOZE		"/sys/class/backlight/%s/doze_enabled"
#define DP_TIMEOUT_DIM_MS	(DEFAULT_DIM_TIMEOUT)
#define DEFAULT_DIM_TIMEOUT	30000
#define DEFAULT_DIM_LEVEL	32
#define DEFAULT_DOZE_LEVEL	4

int dp_init(void);
int dp_set_brightness(uint32_t display, uint8_t level);
int dp_get_brightness(uint32_t display, uint8_t *level);
int dp_set_blank(uint32_t display, enum dp_blank_state state);
int dp_get_blank(uint32_t display, enum dp_blank_state *state);
int dp_get_display_info(uint32_t display, struct dp_display_info *info);
int dp_enumerate_displays(struct dp_display_info infos[DP_MAX_DISPLAYS],
    uint32_t *count);
int dp_set_ambient(const struct dp_ambient_info *ai);
int dp_enable_auto_brightness(bool enable);
int dp_set_doze(uint32_t display, enum dp_doze_mode mode);
int dp_get_doze(uint32_t display, enum dp_doze_mode *mode);
int dp_set_dim_timeout(uint32_t display, uint32_t timeout_ms);
int dp_set_dim_level(uint8_t level);
const char *dp_blank_state_name(enum dp_blank_state st);

#endif
