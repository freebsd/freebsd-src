/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Project.
 * All rights reserved.
 *
 * See thermal.h for full license text.
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

#include <string.h>
#include <errno.h>
#include <strings.h>
#include <stdio.h>

#include "thermal.h"
#include "../power/cpufreq.h"
#include "../sensors/sensor.h"

#define TH_LOG(level, fmt, ...)		do {		\
	if (bootverbose)				\
		printf("thermal: " fmt "\n", ##__VA_ARGS__);\
} while (0)

FEATURE(module_mobile_thermal, "Mobile thermal management subsystem");

static struct thermal_zone th_zones[TH_MAX_ZONES];
static uint32_t th_zone_count;
static struct cooling_dev th_cdevs[TH_MAX_CDEVS];
static uint32_t th_cdev_count;
static struct th_trip_handler th_trips[TH_MAX_TRIP_HANDLERS];
static uint32_t th_trip_count;
static uint32_t th_monitor_interval;
static void *th_monitor_callout;

static int
th_read_sysfs_uint(const char *path, uint32_t *val)
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

static int
th_write_sysfs(const char *path, const char *val)
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
th_read_sysfs_str(const char *path, char *buf, size_t buflen)
{
	int fd, len;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return (errno);

	len = read(fd, buf, buflen - 1);
	close(fd);

	if (len <= 0)
		return (EIO);

	buf[len] = '\0';
	return (0);
}

static int
th_parse_zones(void)
{
	uint32_t id;

	th_zone_count = 0;

	for (id = 0; id < TH_MAX_ZONES; id++) {
		char tpath[128], ppath[128], cpath[128];
		char type[32];
		uint32_t t;
		int ret;

		snprintf(tpath, sizeof(tpath), TH_ZONE_TEMP, id);
		ret = th_read_sysfs_uint(tpath, &t);
		if (ret)
			continue;

		snprintf(ppath, sizeof(ppath), TH_ZONE_TYPE, id);
		if (th_read_sysfs_str(ppath, type, sizeof(type)) != 0)
			strlcpy(type, "unknown", sizeof(type));

		th_zones[th_zone_count].id = id;
		th_zones[th_zone_count].temp_c = (int32_t)t / 1000;
		th_zones[th_zone_count].critical_c = 85000 / 1000;
		th_zones[th_zone_count].passive_c = 80000 / 1000;
		strlcpy(th_zones[th_zone_count].type, type,
		    sizeof(th_zones[th_zone_count].type));
		th_zones[th_zone_count].trip = TH_TRIP_PASSIVE;
		th_zones[th_zone_count].enabled = 1;

		TH_LOG(LOG_INFO, "Thermal zone %u: type=%s, temp=%dC", id,
		    type, th_zones[th_zone_count].temp_c);
		th_zone_count++;
	}

	return (0);
}

static int
th_parse_cdevs(void)
{
	uint32_t id;

	th_cdev_count = 0;

	for (id = 0; id < TH_MAX_CDEVS; id++) {
		char tpath[128], ppath[128];
		char type[32];
		uint32_t cs, ms;
		int ret;

		snprintf(ppath, sizeof(ppath), TH_CDEV_TYPE, id);
		if (th_read_sysfs_str(ppath, type, sizeof(type)) != 0)
			continue;

		snprintf(ppath, sizeof(ppath), TH_CDEV_MAX, id);
		if (th_read_sysfs_uint(ppath, &ms) != 0)
			continue;

		snprintf(tpath, sizeof(tpath), TH_CDEV_CUR, id);
		ret = th_read_sysfs_uint(tpath, &cs);
		if (ret)
			cs = 0;

		th_cdevs[th_cdev_count].id = id;
		if (strncmp(type, "fan", 3) == 0)
			th_cdevs[th_cdev_count].type = TH_CDEV_ACTIVE_FAN;
		else if (strncmp(type, "cpu", 3) == 0)
			th_cdevs[th_cdev_count].type = TH_CDEV_ACTIVE_CPU;
		else if (strncmp(type, "gpu", 3) == 0)
			th_cdevs[th_cdev_count].type = TH_CDEV_ACTIVE_GPU;
		else
			th_cdevs[th_cdev_count].type = TH_CDEV_PASSIVE;

		th_cdevs[th_cdev_count].state = cs;
		th_cdevs[th_cdev_count].max_state = ms;
		th_cdevs[th_cdev_count].online = 1;

		TH_LOG(LOG_INFO, "Cooling device %u: type=%s, state=%u/%u",
		    id, type, cs, ms);
		th_cdev_count++;
	}

	return (0);
}

static void
th_handle_trips(void)
{
	uint32_t z, h;
	int32_t temp;

	for (z = 0; z < th_zone_count; z++) {
		temp = th_zones[z].temp_c;

		for (h = 0; h < th_trip_count; h++) {
			if (!th_trips[h].active)
				continue;
			if (th_trips[h].zone_id != th_zones[z].id)
				continue;
			if (th_trips[h].trip != th_zones[z].trip)
				continue;

			th_trips[h].callback(th_zones[z].id, temp);
 		}
	}
}

static void
th_monitor(void *arg)
{
	uint32_t z;
	int error;

	for (z = 0; z < th_zone_count; z++) {
		uint32_t t;
		char tpath[128];

		snprintf(tpath, sizeof(tpath), TH_ZONE_TEMP,
		    th_zones[z].id);
		error = th_read_sysfs_uint(tpath, &t);
		if (error == 0) {
			th_zones[z].temp_c = (int32_t)t / 1000;
			if (th_zones[z].temp_c >= th_zones[z].critical_c) {
				printf("thermal: CRITICAL zone %u at %dC\n",
				    th_zones[z].id, th_zones[z].temp_c);
			} else if (th_zones[z].temp_c >= th_zones[z].passive_c) {
				printf("thermal: zone %u at %dC, passive "
				    "cooling\n", th_zones[z].id,
				    th_zones[z].temp_c);
			}
		}
	}

	th_handle_trips();
	callout_reset(&th_monitor_callout, th_monitor_interval,
	    (void (*)(void))th_monitor, NULL);
}

int
th_init(void)
{
	int error;

	th_zone_count = 0;
	th_cdev_count = 0;
	th_trip_count = 0;
	th_monitor_interval = hz;

	error = th_parse_zones();
	if (error)
		TH_LOG(LOG_WARNING, "No thermal zones found");

	error = th_parse_cdevs();
	if (error)
		TH_LOG(LOG_WARNING, "No cooling devices found");

	TH_LOG(LOG_INFO, "thermal subsystem: %u zones, %u cooling devices",
	    th_zone_count, th_cdev_count);
	return (0);
}

int
th_get_temps(struct thermal_zone zones[TH_MAX_ZONES], uint32_t *count)
{
	uint32_t z;

	if (zones == NULL || count == NULL)
		return (EINVAL);

	for (z = 0; z < th_zone_count && z < TH_MAX_ZONES; z++) {
		zones[z] = th_zones[z];
		uint32_t t;
		char tpath[128];

		snprintf(tpath, sizeof(tpath), TH_ZONE_TEMP, zones[z].id);
		if (th_read_sysfs_uint(tpath, &t) == 0)
			zones[z].temp_c = (int32_t)t / 1000;
	}

	*count = MIN(z, TH_MAX_ZONES);
	return (0);
}

int
th_get_cooling_devices(struct cooling_dev devs[TH_MAX_CDEVS], uint32_t *count)
{
	uint32_t id, j;

	if (devs == NULL || count == NULL)
		return (EINVAL);

	th_parse_cdevs();

	j = 0;
	for (id = 0; id < th_cdev_count && j < TH_MAX_CDEVS; id++) {
		uint32_t cs;
		char cpath[128];

		snprintf(cpath, sizeof(cpath), TH_CDEV_CUR, th_cdevs[id].id);
		if (th_read_sysfs_uint(cpath, &cs) == 0)
			th_cdevs[id].state = cs;

		devs[j++] = th_cdevs[id];
	}

	*count = MIN(j, TH_MAX_CDEVS);
	return (0);
}

int
th_cooling_set_state(uint32_t dev_id, uint32_t state)
{
	uint32_t d;
	char path[128];
	char buf[32];

	for (d = 0; d < th_cdev_count; d++) {
		if (th_cdevs[d].id == dev_id) {
			if (state > th_cdevs[d].max_state)
				return (EINVAL);

			snprintf(path, sizeof(path), TH_CDEV_CUR, dev_id);
			snprintf(buf, sizeof(buf), "%u", state);
			return (th_write_sysfs(path, buf));
		}
	}

	return (ENOENT);
}

int
th_cooling_get_state(uint32_t dev_id, uint32_t *state)
{
	uint32_t d;
	char path[128];

	if (state == NULL)
		return (EINVAL);

	for (d = 0; d < th_cdev_count; d++) {
		if (th_cdevs[d].id == dev_id) {
			snprintf(path, sizeof(path), TH_CDEV_CUR, dev_id);
			return (th_read_sysfs_uint(path, state));
		}
	}

	return (ENOENT);
}

int
th_register_trip_handler(uint32_t zone_id, enum th_trip_type trip,
    void (*callback)(uint32_t, int32_t), void *arg)
{
	uint32_t h;

	if (callback == NULL || th_trip_count >= TH_MAX_TRIP_HANDLERS)
		return (EINVAL);

	h = th_trip_count++;
	th_trips[h].zone_id = zone_id;
	th_trips[h].trip = trip;
	th_trips[h].callback = callback;
	th_trips[h].arg = arg;
	th_trips[h].active = 1;

	TH_LOG(LOG_INFO, "Trip handler registered: zone=%u, trip=%d",
	    zone_id, trip);
	return (0);
}

int
th_unregister_trip_handler(uint32_t id)
{
	uint32_t h;

	for (h = 0; h < th_trip_count; h++) {
		if (th_trips[h].zone_id == id) {
			th_trips[h].active = 0;
			return (0);
		}
	}

	return (ENOENT);
}

int
th_monitor_start(void)
{
	if (th_monitor_callout != NULL) {
		printf("thermal: monitor already running\n");
		return (0);
	}

	callout_init(&th_monitor_callout, 1);
	callout_reset(&th_monitor_callout, th_monitor_interval,
	    (void (*)(void))th_monitor, NULL);
	TH_LOG(LOG_INFO, "thermal monitor started, interval=%ums",
	    (unsigned)(th_monitor_interval * 1000 / hz));
	return (0);
}

int
th_monitor_stop(void)
{
	if (th_monitor_callout != NULL) {
		callout_stop(&th_monitor_callout);
		th_monitor_callout = NULL;
		TH_LOG(LOG_INFO, "thermal monitor stopped");
	}
	return (0);
}

bool
th_is_critical(uint32_t zone_id)
{
	uint32_t z;

	for (z = 0; z < th_zone_count; z++) {
		if (th_zones[z].id == zone_id)
			return (th_zones[z].temp_c >= th_zones[z].critical_c);
	}

	return (false);
}

int32_t
th_get_zone_temp(uint32_t zone_id)
{
	uint32_t z;

	for (z = 0; z < th_zone_count; z++) {
		if (th_zones[z].id == zone_id)
			return (th_zones[z].temp_c);
	}

	return (-1);
}

const char *
th_trip_type_name(enum th_trip_type trip)
{
	static const char *names[] = {
		[TH_TRIP_PASSIVE]	= "passive",
		[TH_TRIP_ACTIVE]	= "active",
		[TH_TRIP_CRITICAL]	= "critical",
		[TH_TRIP_HOT]		= "hot",
	};

	if (trip >= TH_TRIP_MAX)
		return (NULL);
	return (names[trip]);
}
