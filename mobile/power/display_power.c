/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Project.
 * All rights reserved.
 *
 * See display_power.h for full license text.
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
#include <sys/errno.h>
#include <sys/proc.h>

#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include "display_power.h"

#define DP_LOG(level, fmt, ...)		do {		\
	printf("display: " fmt "\n", ##__VA_ARGS__);\
} while (0)

FEATURE(module_mobile_display_power, "Display power management subsystem");

static uint32_t dp_count;
static struct dp_display_info dp_displays[DP_MAX_DISPLAYS];
static uint8_t dp_brightness[DP_MAX_DISPLAYS];
static enum dp_blank_state dp_blank_state[DP_MAX_DISPLAYS];
static enum dp_doze_mode dp_doze_mode[DP_MAX_DISPLAYS];
static uint8_t dp_auto_brightness;
static uint32_t dp_dim_timeout;
static uint8_t dp_dim_level;
static int dp_polling_enabled;
static void *dp_timer_callout;
static int dp_backlight_fd[DP_MAX_DISPLAYS];

static char dp_backlight_dev[32] = "intel_backlight";

static int
dp_write_sysfs(const char *path, const char *val)
{
	int fd, len;

	fd = open(path, O_WRONLY);
	if (fd < 0)
		return (errno);

	len = (int)strlen(val);
	if (write(fd, val, len) != len) {
		close(fd);
		return (EIO);
	}

	close(fd);
	return (0);
}

static int
dp_read_sysfs_uint(const char *path, uint32_t *val)
{
	char buf[32];
	int fd, len;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return (errno);

	len = read(fd, buf, sizeof(buf) - 1);
	close(fd);

	if (len <= 0)
		return (EIO);

	buf[len] = '\0';
	*val = (uint32_t)strtoul(buf, NULL, 10);
	return (0);
}

static void
dp_update_ambient(void)
{
	uint32_t lux;
	char path[128];
	int fd;

	snprintf(path, sizeof(path), "/sys/class/als0/lux");
	fd = open(path, O_RDONLY);
	if (fd < 0)
		return;

	lux = 0;
	read(fd, &lux, sizeof(lux));
	close(fd);

	if (lux < 10) {
		dp_set_brightness(0, DP_DEFAULT_DIM_LEVEL);
	} else if (lux < 100) {
		dp_set_brightness(0, 80);
	} else if (lux < 1000) {
		dp_set_brightness(0, 150);
	} else {
		dp_set_brightness(0, 255);
	}
}

static int
dp_set_backlight_ioctl(uint32_t display, uint8_t level)
{
	struct dp_display_info *info;
	uint32_t brightness, max_b;

	if (display >= dp_count)
		return (EINVAL);

	info = &dp_displays[display];

	if (dp_backlight_fd[display] < 0)
		dp_backlight_fd[display] = open("/dev/backlight0", O_RDWR);

	if (dp_backlight_fd[display] < 0) {
		DP_LOG(LOG_WARNING, "no backlight device for display %u",
		    display);
		return (ENODEV);
	}

	brightness = (uint32_t)((level / 255.0f) * info->brightness_range.max_cd_m2);
	max_b = info->brightness_range.max_cd_m2;

	return (ioctl(dp_backlight_fd[display], BACKLIGHT_SET_BRIGHTNESS,
	    &brightness));
}

static void
dp_timer_cb(void *arg)
{
	struct dp_display_info *info;

	if (!dp_polling_enabled || dp_count == 0)
		return;

	info = &dp_displays[0];
	if (dp_auto_brightness)
		dp_update_ambient();

	callout_reset(&dp_timer_callout, dp_dim_timeout * hz / 1000,
	    (void (*)(void))dp_timer_cb, NULL);
}

int
dp_init(void)
{
	char path[128], mbuf[32], buf[32];
	int fd, len, i;

	memset(dp_backlight_fd, -1, sizeof(dp_backlight_fd));

	for (i = 0; i < DP_MAX_DISPLAYS; i++) {
		snprintf(path, sizeof(path), "%s%d",
		    "/sys/class/backlight/backlight", i);
		fd = open(path, O_RDONLY);
		if (fd < 0)
			break;

		close(fd);
		dp_displays[dp_count].id = i;
		snprintf(dp_displays[dp_count].name, sizeof(dp_displays[dp_count].name),
		    "display%d", i);
		dp_displays[dp_count].max_brightness = 255;

		snprintf(mbuf, sizeof(mbuf), DP_SYSFS_MAX_BRIGHT,
		    dp_backlight_dev);
		if (dp_read_sysfs_uint(mbuf, &dp_displays[i].brightness_range.max_cd_m2) == 0)
			DP_LOG(LOG_INFO, "display%d brightness range %d cd/m2",
			    i, dp_displays[i].brightness_range.max_cd_m2);

		dp_brightness[i] = 128;
		dp_blank_state[i] = DP_BLANK_ON;
		dp_doze_mode[i] = DP_DOZE_NONE;
		dp_count++;
	}

	dp_dim_timeout = DP_DEFAULT_DIM_TIMEOUT;
	dp_dim_level = DP_DEFAULT_DIM_LEVEL;
	dp_auto_brightness = 0;
	dp_polling_enabled = 0;

	DP_LOG(LOG_INFO, "%u displays initialized", dp_count);
	return (0);
}

int
dp_set_brightness(uint32_t display, uint8_t level)
{
	char buf[32];
	struct dp_display_info *info;
	int fd;

	if (display >= dp_count)
		return (EINVAL);

	if (dp_blank_state[display] != DP_BLANK_ON)
		return (EPERM);

	if (dp_doze_mode[display] != DP_DOZE_NONE && level > DP_DEFAULT_DOZE_LEVEL)
		return (EINVAL);

	dp_brightness[display] = level;

	snprintf(buf, sizeof(buf), "%d", level);
	fd = open(DP_SYSFS_BRIGHTNESS, O_WRONLY);
	if (fd >= 0) {
		write(fd, buf, strlen(buf));
		close(fd);
	}

	info = &dp_displays[display];
	DP_LOG(LOG_DEBUG, "display%u brightness=%u", display, level);

	return (0);
}

int
dp_get_brightness(uint32_t display, uint8_t *level)
{
	struct dp_display_info *info;
	uint32_t val;
	int fd;

	if (display >= dp_count || level == NULL)
		return (EINVAL);

	info = &dp_displays[display];

	fd = open(DP_SYSFS_ACTUAL, O_RDONLY);
	if (fd >= 0) {
		/* parse actual brightness */
		close(fd);
	}

	*level = dp_brightness[display];
	return (0);
}

int
dp_set_blank(uint32_t display, enum dp_blank_state state)
{
	if (display >= dp_count)
		return (EINVAL);

	switch (state) {
	case DP_BLANK_ON:
	case DP_BLANK_STANDBY:
	case DP_BLANK_SUSPEND:
	case DP_BLANK_OFF:
		dp_blank_state[display] = state;
		DP_LOG(LOG_DEBUG, "display%u blank=%s", display,
		    dp_blank_state_name(state));
		return (0);
	default:
		return (EINVAL);
	}
}

int
dp_get_blank(uint32_t display, enum dp_blank_state *state)
{
	if (display >= dp_count || state == NULL)
		return (EINVAL);

	*state = dp_blank_state[display];
	return (0);
}

int
dp_get_display_info(uint32_t display, struct dp_display_info *info)
{
	if (display >= dp_count || info == NULL)
		return (EINVAL);

	memcpy(info, &dp_displays[display], sizeof(*info));
	return (0);
}

int
dp_enumerate_displays(struct dp_display_info infos[DP_MAX_DISPLAYS],
    uint32_t *count)
{
	uint32_t i;

	if (infos == NULL || count == NULL)
		return (EINVAL);

	for (i = 0; i < dp_count && i < DP_MAX_DISPLAYS; i++)
		infos[i] = dp_displays[i];

	*count = dp_count;
	return (0);
}

int
dp_set_ambient(const struct dp_ambient_info *ai)
{
	if (ai == NULL)
		return (EINVAL);

	dp_auto_brightness = 1;
	DP_LOG(LOG_DEBUG, "Ambient light sensor %u: %d lux", ai->sensor_id,
	    ai->light_lux);
	return (0);
}

int
dp_enable_auto_brightness(bool enable)
{
	dp_auto_brightness = enable ? 1 : 0;
	DP_LOG(LOG_DEBUG, "Auto-brightness %s", enable ? "enabled" :
	    "disabled");
	return (0);
}

int
dp_set_doze(uint32_t display, enum dp_doze_mode mode)
{
	uint8_t save_brightness;

	if (display >= dp_count)
		return (EINVAL);

	if (mode == dp_doze_mode[display])
		return (0);

	switch (mode) {
	case DP_DOZE_NONE:
		save_brightness = dp_brightness[display];
		dp_set_brightness(display, 0);
		dp_doze_mode[display] = DP_DOZE_LOW_POWER;
		sleep(1);
		dp_set_brightness(display, save_brightness);
		break;
	case DP_DOZE_LOW_POWER:
		dp_set_brightness(display, DP_DEFAULT_DOZE_LEVEL);
		break;
	}

	dp_doze_mode[display] = mode;
	DP_LOG(LOG_INFO, "display%u doze=%d", display, mode);
	return (0);
}

int
dp_get_doze(uint32_t display, enum dp_doze_mode *mode)
{
	if (display >= dp_count || mode == NULL)
		return (EINVAL);

	*mode = dp_doze_mode[display];
	return (0);
}

int
dp_set_dim_timeout(uint32_t display, uint32_t timeout_ms)
{
	if (display >= dp_count)
		return (EINVAL);

	dp_dim_timeout = timeout_ms;
	DP_LOG(LOG_DEBUG, "Dim timeout set to %ums", timeout_ms);
	return (0);
}

int
dp_set_dim_level(uint8_t level)
{
	dp_dim_level = level;
	return (0);
}

const char *
dp_blank_state_name(enum dp_blank_state st)
{
	static const char *names[] = {
		[DP_BLANK_ON]		= "on",
		[DP_BLANK_STANDBY]	= "standby",
		[DP_BLANK_SUSPEND]	= "suspend",
		[DP_BLANK_OFF]		= "off",
	};

	if (st >= DP_BLANK_OFF)
		return (NULL);
	return (names[st]);
}
