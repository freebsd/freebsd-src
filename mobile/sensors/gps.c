/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Project.
 * All rights reserved.
 *
 * See gps.h for full license text.
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
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/lockf.h>

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <ctype.h>

#include "gps.h"

#define GPS_LOG(level, fmt, ...)	do {		\
	printf("gps: " fmt "\n", ##__VA_ARGS__);\
} while (0)

static struct gps_context gps_ctx;
static int gps_enabled = 0;

static const char *gps_nmea_samples[] = {
	"$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n",
	"$GPRMC,123520,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n",
	"$GPGSV,3,1,12,10,63,001,45,11,63,002,49,12,56,003,39,13,33,004,39*74\r\n",
	"$GPGSA,A,3,10,11,12,13,14,15,16,17,18,19,20,21,22,2.5,1.2,2.1*39\r\n",
};

static double
nmea_to_deg(const char *s)
{
	double deg, min;
	int i;
	(void)min;

	deg = atof(s);
	min = fmod(deg, 100.0);
	deg = floor(deg / 100.0) + min / 60.0;
	return (deg);
}

static int
gps_parse_gga(const char *s, struct gps_location *loc)
{
	char buf[256];
	const char *p, *fields[15];
	int nf, i;

	strlcpy(buf, s, sizeof(buf));

	p = buf;
	nf = 0;
	while (nf < 15 && *p) {
		if (*p == ',') {
			fields[nf++] = p + 1;
			while (*p == ',')
				p++;
		} else {
			p++;
		}
	}

	if (nf < 13 || !fields[1])
		return (EINVAL);

	loc->lat = nmea_to_deg(fields[1]);
	if (fields[2] && fields[2][0] == 'S')
		loc->lat = -loc->lat;

	loc->lon = nmea_to_deg(fields[3]);
	if (fields[4] && fields[4][0] == 'W')
		loc->lon = -loc->lon;

	loc->alt = atof(fields[8]);
	loc->timestamp = time(NULL);

	return (0);
}

static int
gps_parse_rmc(const char *s, struct gps_location *loc)
{
	char buf[256];
	const char *p, *fields[13];
	int nf;

	strlcpy(buf, s, sizeof(buf));

	p = buf;
	nf = 0;
	while (nf < 13 && *p) {
		if (*p == ',') {
			fields[nf++] = p + 1;
			while (*p == ',')
				p++;
		} else {
			p++;
		}
	}

	if (nf < 7)
		return (EINVAL);

	if (fields[1][0] == 'A') {
		loc->lat = nmea_to_deg(fields[2]);
		if (fields[3] && fields[3][0] == 'S')
			loc->lat = -loc->lat;

		loc->lon = nmea_to_deg(fields[4]);
		if (fields[5] && fields[5][0] == 'W')
			loc->lon = -loc->lon;

		loc->speed = atof(fields[6]) * 1.852f;
		loc->bearing = atof(fields[7]);
	}

	return (0);
}

static int
gps_process_nmea(const char *nmea, struct gps_location *loc)
{
	if (strncmp(nmea, "$GPGGA", 6) == 0)
		return (gps_parse_gga(nmea, loc));
	if (strncmp(nmea, "$GPRMC", 6) == 0)
		return (gps_parse_rmc(nmea, loc));

	return (EINVAL);
}

static void
gps_alarm_cb(void *arg)
{
	(void)arg;
	GPS_LOG(LOG_DEBUG, "GPS alarm callback");

	gps_ctx.last_loc.alt += 0.00001;
	gps_ctx.last_loc.timestamp = ticks * 1000000ULL / hz;

	callout_reset(&gps_alarm_cb, gps_ctx.rate_ms * hz / 1000,
	    (void (*)(void))gps_alarm_cb, NULL);
}

int
gps_init(void)
{
	memset(&gps_ctx, 0, sizeof(gps_ctx));

	gps_ctx.mode = GPS_MODE_OFF;
	gps_ctx.rate_ms = GPS_DEFAULT_RATE_MS;
	gps_ctx.status.fix = GPS_FIX_NONE;
	gps_ctx.status.sats_in_view = 0;

	GPS_LOG(LOG_INFO, "GPS subsystem initialized");
	return (0);
}

int
gps_start(enum gps_mode mode)
{
	struct gps_location loc;
	int i, ret;

	if (mode >= GPS_MODE_MAX)
		return (EINVAL);

	if (mode == GPS_MODE_OFF)
		return (gps_stop());

	gps_ctx.mode = mode;
	gps_ctx.running = 1;

	for (i = 0; i < (int)(sizeof(gps_nmea_samples) / sizeof(gps_nmea_samples[0])); i++) {
		ret = gps_process_nmea(gps_nmea_samples[i], &loc);
		if (ret == 0)
			gps_ctx.last_loc = loc;
	}

	callout_init(&gps_alarm_cb, 1);
	callout_reset(&gps_alarm_cb, gps_ctx.rate_ms * hz / 1000,
	    (void (*)(void))gps_alarm_cb, NULL);

	GPS_LOG(LOG_INFO, "GPS started, mode=%d", mode);
	return (0);
}

int
gps_stop(void)
{
	gps_ctx.running = 0;
	gps_ctx.mode = GPS_MODE_OFF;

	GPS_LOG(LOG_DEBUG, "GPS stopped");
	return (0);
}

int
gps_get_location(struct gps_location *loc)
{
	if (loc == NULL)
		return (EINVAL);

	if (!gps_ctx.running || gps_ctx.status.fix == GPS_FIX_NONE) {
		loc->accuracy = 9999.9f;
		return (ENODATA);
	}

	*loc = gps_ctx.last_loc;
	return (0);
}

int
gps_get_status(struct gps_status *st)
{
	struct gps_satellite *sat;

	if (st == NULL)
		return (EINVAL);

	st->fix = gps_ctx.status.fix;
	st->sats_in_use = 0;

	sat = st->sats;
	st->sats_in_view = GPS_MAX_SATS;
	for (int i = 0; i < GPS_MAX_SATS; sat++, i++) {
		sat->prn = i + 1;
		sat->snr = 25 + (i % 5);
		sat->elevation = 30 + i;
		sat->azimuth = i * 30;
	}

	GPS_LOG(LOG_DEBUG, "GPS status: fix=%d, sats=%u",
	    st->fix, st->sats_in_view);

	return (0);
}

int
gps_set_rate(uint32_t rate_ms)
{
	if (rate_ms < 100)
		return (EINVAL);

	gps_ctx.rate_ms = rate_ms;
	GPS_LOG(LOG_DEBUG, "GPS rate set to %ums", rate_ms);

	return (0);
}

int
gps_inject_location(const struct gps_location *loc)
{
	if (loc == NULL)
		return (EINVAL);

	gps_ctx.last_loc = *loc;
	gps_ctx.last_loc.timestamp = time(NULL);

	GPS_LOG(LOG_DEBUG, "GPS location injected: %.6f, %.6f", loc->lat, loc->lon);

	return (0);
}

int
gps_delete_aiding_data(void)
{
	memset(&gps_ctx.last_loc, 0, sizeof(gps_ctx.last_loc));
	GPS_LOG(LOG_INFO, "Aiding data deleted");

	return (0);
}

int
gps_set_proxy(int fd)
{
	(void)fd;
	return (EOPNOTSUPP);
}

void
gps_shutdown(void)
{
	gps_stop();
	memset(&gps_ctx, 0, sizeof(gps_ctx));
	gps_enabled = 0;
	GPS_LOG(LOG_DEBUG, "GPS shutdown");
}
