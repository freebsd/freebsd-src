/*	$FreeBSD$ */
/*	$OpenBSD: sensorsd.c,v 1.34 2007/08/14 17:10:02 cnst Exp $ */

/*-
 * Copyright (c) 2003 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2005 Matthew Gream <matthew.gream@pobox.com>
 * Copyright (c) 2006 Constantine A. Murenin <cnst+openbsd@bugmail.mojo.ru>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/sensors.h>

#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#define	RFBUFSIZ	28	/* buffer size for print_sensor */
#define	RFBUFCNT	4	/* ring buffers */
#define REPORT_PERIOD	60	/* report every n seconds */
#define CHECK_PERIOD	20	/* check every n seconds */

enum sensorsd_s_status {
	SENSORSD_S_UNSPEC,	/* status is unspecified */
	SENSORSD_S_INVALID,	/* status is invalid, per SENSOR_FINVALID */
	SENSORSD_S_WITHIN,	/* status is within limits */
	SENSORSD_S_OUTSIDE	/* status is outside limits */
};

struct limits_t {
	TAILQ_ENTRY(limits_t)	entries;
	enum sensor_type	type;		/* sensor type */
	int			numt;		/* sensor number */
	int64_t			last_val;
	int64_t			lower;		/* lower limit */
	int64_t			upper;		/* upper limit */
	char			*command;	/* failure command */
	time_t			astatus_changed;
	time_t			ustatus_changed;
	enum sensor_status	astatus;	/* last automatic status */
	enum sensor_status	astatus2;
	enum sensorsd_s_status	ustatus;	/* last user-limit status */
	enum sensorsd_s_status	ustatus2;
	int			acount;		/* stat change counter */
	int			ucount;		/* stat change counter */
	u_int8_t		flags;		/* sensorsd limit flags */
#define SENSORSD_L_USERLIMIT		0x0001	/* user specified limit */
#define SENSORSD_L_ISTATUS		0x0002	/* ignore automatic status */
};

struct sdlim_t {
	TAILQ_ENTRY(sdlim_t)	entries;
	char			dxname[16];	/* device unix name */
	int			dev;		/* device number */
	int			sensor_cnt;
	TAILQ_HEAD(, limits_t)	limits;
};

void		 usage(void);
struct sdlim_t	*create_sdlim(struct sensordev *);
void		 check(void);
void		 check_sdlim(struct sdlim_t *);
void		 execute(char *);
void		 report(time_t);
void		 report_sdlim(struct sdlim_t *, time_t);
static char	*print_sensor(enum sensor_type, int64_t);
void		 parse_config(char *);
void		 parse_config_sdlim(struct sdlim_t *, char **);
int64_t		 get_val(char *, int, enum sensor_type);
void		 reparse_cfg(int);

TAILQ_HEAD(, sdlim_t) sdlims = TAILQ_HEAD_INITIALIZER(sdlims);

char			 *configfile;
volatile sig_atomic_t	  reload = 0;
int			  debug = 0;

void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [-d]\n", __progname);
	exit(1);
}

int
main(int argc, char *argv[])
{
	struct sensordev sensordev;
	struct sdlim_t	*sdlim;
	size_t		 sdlen = sizeof(sensordev);
	time_t		 next_report, last_report = 0, next_check;
	int		 mib[3], dev;
	int		 sleeptime, sensor_cnt = 0, ch;

	while ((ch = getopt(argc, argv, "d")) != -1) {
		switch (ch) {
		case 'd':
			debug = 1;
			break;
		default:
			usage();
		}
	}

	mib[0] = CTL_HW;
	mib[1] = HW_SENSORS;

	for (dev = 0; dev < MAXSENSORDEVICES; dev++) {
		mib[2] = dev;
		if (sysctl(mib, 3, &sensordev, &sdlen, NULL, 0) == -1) {
			if (errno != ENOENT)
				warn("sysctl");
			continue;
		}
		sdlim = create_sdlim(&sensordev);
		TAILQ_INSERT_TAIL(&sdlims, sdlim, entries);
		sensor_cnt += sdlim->sensor_cnt;
	}

	if (sensor_cnt == 0)
		errx(1, "no sensors found");

	openlog("sensorsd", LOG_PID | LOG_NDELAY, LOG_DAEMON);

	if (configfile == NULL)
		if (asprintf(&configfile, "/etc/sensorsd.conf") == -1)
			err(1, "out of memory");
	parse_config(configfile);

	if (debug == 0 && daemon(0, 0) == -1)
		err(1, "unable to fork");

	signal(SIGHUP, reparse_cfg);
	signal(SIGCHLD, SIG_IGN);

	syslog(LOG_INFO, "startup, system has %d sensors", sensor_cnt);

	next_check = next_report = time(NULL);

	for (;;) {
		if (reload) {
			parse_config(configfile);
			syslog(LOG_INFO, "configuration reloaded");
			reload = 0;
		}
		if (next_check <= time(NULL)) {
			check();
			next_check = time(NULL) + CHECK_PERIOD;
		}
		if (next_report <= time(NULL)) {
			report(last_report);
			last_report = next_report;
			next_report = time(NULL) + REPORT_PERIOD;
		}
		if (next_report < next_check)
			sleeptime = next_report - time(NULL);
		else
			sleeptime = next_check - time(NULL);
		if (sleeptime > 0)
			sleep(sleeptime);
	}
}

struct sdlim_t *
create_sdlim(struct sensordev *snsrdev)
{
	struct sensor	 sensor;
	struct sdlim_t	*sdlim;
	struct limits_t	*limit;
	size_t		 slen = sizeof(sensor);
	int		 mib[5], numt;
	enum sensor_type type;

	if ((sdlim = calloc(1, sizeof(struct sdlim_t))) == NULL)
		err(1, "calloc");

	strlcpy(sdlim->dxname, snsrdev->xname, sizeof(sdlim->dxname));

	mib[0] = CTL_HW;
	mib[1] = HW_SENSORS;
	mib[2] = sdlim->dev = snsrdev->num;

	TAILQ_INIT(&sdlim->limits);

	for (type = 0; type < SENSOR_MAX_TYPES; type++) {
		mib[3] = type;
		for (numt = 0; numt < snsrdev->maxnumt[type]; numt++) {
			mib[4] = numt;
			if (sysctl(mib, 5, &sensor, &slen, NULL, 0) == -1) {
				if (errno != ENOENT)
					warn("sysctl");
				continue;
			}
			if ((limit = calloc(1, sizeof(struct limits_t))) ==
			    NULL)
				err(1, "calloc");
			limit->type = type;
			limit->numt = numt;
			TAILQ_INSERT_TAIL(&sdlim->limits, limit, entries);
			sdlim->sensor_cnt++;
		}
	}

	return (sdlim);
}

void
check(void)
{
	struct sdlim_t	*sdlim;

	TAILQ_FOREACH(sdlim, &sdlims, entries)
		check_sdlim(sdlim);
}

void
check_sdlim(struct sdlim_t *sdlim)
{
	struct sensor		 sensor;
	struct limits_t		*limit;
	size_t		 	 len;
	int		 	 mib[5];

	mib[0] = CTL_HW;
	mib[1] = HW_SENSORS;
	mib[2] = sdlim->dev;
	len = sizeof(sensor);

	TAILQ_FOREACH(limit, &sdlim->limits, entries) {
		if ((limit->flags & SENSORSD_L_ISTATUS) &&
		    !(limit->flags & SENSORSD_L_USERLIMIT)) 
			continue;

		mib[3] = limit->type;
		mib[4] = limit->numt;
		if (sysctl(mib, 5, &sensor, &len, NULL, 0) == -1)
			err(1, "sysctl");

		if (!(limit->flags & SENSORSD_L_ISTATUS)) {
			enum sensor_status	newastatus = sensor.status;

			if (limit->astatus != newastatus) {
				if (limit->astatus2 != newastatus) {
					limit->astatus2 = newastatus;
					limit->acount = 0;
				} else if (++limit->acount >= 3) {
					limit->last_val = sensor.value;
					limit->astatus2 =
					    limit->astatus = newastatus;
					limit->astatus_changed = time(NULL);
				}
			}
		}
	
		if (limit->flags & SENSORSD_L_USERLIMIT) {
			enum sensorsd_s_status 	 newustatus;

			if (sensor.flags & SENSOR_FINVALID)
				newustatus = SENSORSD_S_INVALID;
			else if (sensor.value > limit->upper ||
				sensor.value < limit->lower)
				newustatus = SENSORSD_S_OUTSIDE;
			else
				newustatus = SENSORSD_S_WITHIN;

			if (limit->ustatus != newustatus) {
				if (limit->ustatus2 != newustatus) {
					limit->ustatus2 = newustatus;
					limit->ucount = 0;
				} else if (++limit->ucount >= 3) {
					limit->last_val = sensor.value;
					limit->ustatus2 =
					    limit->ustatus = newustatus;
					limit->ustatus_changed = time(NULL);
				}
			}
		}
	}
}

void
execute(char *command)
{
	char *argp[] = {"sh", "-c", command, NULL};

	switch (fork()) {
	case -1:
		syslog(LOG_CRIT, "execute: fork() failed");
		break;
	case 0:
		execv("/bin/sh", argp);
		_exit(1);
		/* NOTREACHED */
	default:
		break;
	}
}

void
report(time_t last_report)
{
	struct sdlim_t	*sdlim;
 
	TAILQ_FOREACH(sdlim, &sdlims, entries)
		report_sdlim(sdlim, last_report);
}

void
report_sdlim(struct sdlim_t *sdlim, time_t last_report)
{
	struct limits_t	*limit;

	TAILQ_FOREACH(limit, &sdlim->limits, entries) {
		if ((limit->astatus_changed <= last_report) &&
		    (limit->ustatus_changed <= last_report))
			continue;

		if (limit->astatus_changed > last_report) {
			const char *as = NULL;

			switch (limit->astatus) {
			case SENSOR_S_UNSPEC:
				as = "";
				break;
			case SENSOR_S_OK:
				as = ", OK";
				break;
			case SENSOR_S_WARN:
				as = ", WARN";
				break;
			case SENSOR_S_CRIT:
				as = ", CRITICAL";
				break;
			case SENSOR_S_UNKNOWN:
				as = ", UNKNOWN";
				break;
			}
			syslog(LOG_ALERT, "%s.%s%d: %s%s",
			    sdlim->dxname, sensor_type_s[limit->type],
			    limit->numt,
			    print_sensor(limit->type, limit->last_val), as);
		}

		if (limit->ustatus_changed > last_report) {
			char us[BUFSIZ];

			switch (limit->ustatus) {
			case SENSORSD_S_UNSPEC:
				snprintf(us, sizeof(us),
				    "ustatus uninitialised");
				break;
			case SENSORSD_S_INVALID:
				snprintf(us, sizeof(us), "marked invalid");
				break;
			case SENSORSD_S_WITHIN:
				snprintf(us, sizeof(us), "within limits: %s",
				    print_sensor(limit->type, limit->last_val));
				break;
			case SENSORSD_S_OUTSIDE:
				snprintf(us, sizeof(us), "exceeds limits: %s",
				    print_sensor(limit->type, limit->last_val));
				break;
			}
			syslog(LOG_ALERT, "%s.%s%d: %s",
			    sdlim->dxname, sensor_type_s[limit->type],
			    limit->numt, us);
		}

		if (limit->command) {
			int i = 0, n = 0, r;
			char *cmd = limit->command;
			char buf[BUFSIZ];
			int len = sizeof(buf);

			buf[0] = '\0';
			for (i = n = 0; n < len; ++i) {
				if (cmd[i] == '\0') {
					buf[n++] = '\0';
					break;
				}
				if (cmd[i] != '%') {
					buf[n++] = limit->command[i];
					continue;
				}
				i++;
				if (cmd[i] == '\0') {
					buf[n++] = '\0';
					break;
				}

				switch (cmd[i]) {
				case 'x':
					r = snprintf(&buf[n], len - n, "%s",
					    sdlim->dxname);
					break;
				case 't':
					r = snprintf(&buf[n], len - n, "%s",
					    sensor_type_s[limit->type]);
					break;
				case 'n':
					r = snprintf(&buf[n], len - n, "%d",
					    limit->numt);
					break;
				case '2':
					r = snprintf(&buf[n], len - n, "%s",
					    print_sensor(limit->type,
					    limit->last_val));
					break;
				case '3':
					r = snprintf(&buf[n], len - n, "%s",
					    print_sensor(limit->type,
					    limit->lower));
					break;
				case '4':
					r = snprintf(&buf[n], len - n, "%s",
					    print_sensor(limit->type,
					    limit->upper));
					break;
				default:
					r = snprintf(&buf[n], len - n, "%%%c",
					    cmd[i]);
					break;
				}
				if (r < 0 || (r >= len - n)) {
					syslog(LOG_CRIT, "could not parse "
					    "command");
					return;
				}
				if (r > 0)
					n += r;
			}
			if (buf[0])
				execute(buf);
		}
	}
}

const char *drvstat[] = {
	NULL, "empty", "ready", "powerup", "online", "idle", "active",
	"rebuild", "powerdown", "fail", "pfail"
};

static char *
print_sensor(enum sensor_type type, int64_t value)
{
	static char	 rfbuf[RFBUFCNT][RFBUFSIZ];	/* ring buffer */
	static int	 idx;
	char		*fbuf;

	fbuf = rfbuf[idx++];
	if (idx == RFBUFCNT)
		idx = 0;

	switch (type) {
	case SENSOR_TEMP:
		snprintf(fbuf, RFBUFSIZ, "%.2f degC",
		    (value - 273150000) / 1000000.0);
		break;
	case SENSOR_FANRPM:
		snprintf(fbuf, RFBUFSIZ, "%lld RPM", value);
		break;
	case SENSOR_VOLTS_DC:
		snprintf(fbuf, RFBUFSIZ, "%.2f V DC", value / 1000000.0);
		break;
	case SENSOR_AMPS:
		snprintf(fbuf, RFBUFSIZ, "%.2f A", value / 1000000.0);
		break;
	case SENSOR_WATTHOUR:
		snprintf(fbuf, RFBUFSIZ, "%.2f Wh", value / 1000000.0);
		break;
	case SENSOR_AMPHOUR:
		snprintf(fbuf, RFBUFSIZ, "%.2f Ah", value / 1000000.0);
		break;
	case SENSOR_INDICATOR:
		snprintf(fbuf, RFBUFSIZ, "%s", value? "On" : "Off");
		break;
	case SENSOR_INTEGER:
		snprintf(fbuf, RFBUFSIZ, "%lld", value);
		break;
	case SENSOR_PERCENT:
		snprintf(fbuf, RFBUFSIZ, "%.2f%%", value / 1000.0);
		break;
	case SENSOR_LUX:
		snprintf(fbuf, RFBUFSIZ, "%.2f lx", value / 1000000.0);
		break;
	case SENSOR_DRIVE:
		if (0 < value && value < sizeof(drvstat)/sizeof(drvstat[0]))
			snprintf(fbuf, RFBUFSIZ, "%s", drvstat[value]);
		else
			snprintf(fbuf, RFBUFSIZ, "%lld ???", value);
		break;
	case SENSOR_TIMEDELTA:
		snprintf(fbuf, RFBUFSIZ, "%.6f secs", value / 1000000000.0);
		break;
	default:
		snprintf(fbuf, RFBUFSIZ, "%lld ???", value);
	}

	return (fbuf);
}

void
parse_config(char *cf)
{
	struct sdlim_t	 *sdlim;
	char		**cfa;

	if ((cfa = calloc(2, sizeof(char *))) == NULL)
		err(1, "calloc");
	cfa[0] = cf;
	cfa[1] = NULL;

	TAILQ_FOREACH(sdlim, &sdlims, entries)
		parse_config_sdlim(sdlim, cfa);
	free(cfa);
}

void
parse_config_sdlim(struct sdlim_t *sdlim, char **cfa)
{
	struct limits_t	 *p;
	char		 *buf = NULL, *ebuf = NULL;
	char		  node[48];

	TAILQ_FOREACH(p, &sdlim->limits, entries) {
		snprintf(node, sizeof(node), "hw.sensors.%s.%s%d", 
		    sdlim->dxname, sensor_type_s[p->type], p->numt);
		p->flags = 0;
		if (cgetent(&buf, cfa, node) != 0)
			if (cgetent(&buf, cfa, sensor_type_s[p->type]) != 0)
				continue;
		if (cgetcap(buf, "istatus", ':'))
			p->flags |= SENSORSD_L_ISTATUS;
		if (cgetstr(buf, "low", &ebuf) < 0)
			ebuf = NULL;
		p->lower = get_val(ebuf, 0, p->type);
		if (cgetstr(buf, "high", &ebuf) < 0)
			ebuf = NULL;
		p->upper = get_val(ebuf, 1, p->type);
		if (cgetstr(buf, "command", &ebuf) < 0)
			ebuf = NULL;
		if (ebuf)
			asprintf(&(p->command), "%s", ebuf);
		free(buf);
		buf = NULL;
		if (p->lower != LLONG_MIN || p->upper != LLONG_MAX)
			p->flags |= SENSORSD_L_USERLIMIT;
	}
}

int64_t
get_val(char *buf, int upper, enum sensor_type type)
{
	double	 val;
	int64_t	 rval = 0;
	char	*p;

	if (buf == NULL) {
		if (upper)
			return (LLONG_MAX);
		else
			return (LLONG_MIN);
	}

	val = strtod(buf, &p);
	if (buf == p)
		err(1, "incorrect value: %s", buf);

	switch(type) {
	case SENSOR_TEMP:
		switch(*p) {
		case 'C':
			printf("C");
			rval = (val + 273.16) * 1000 * 1000;
			break;
		case 'F':
			printf("F");
			rval = ((val - 32.0) / 9 * 5 + 273.16) * 1000 * 1000;
			break;
		default:
			errx(1, "unknown unit %s for temp sensor", p);
		}
		break;
	case SENSOR_FANRPM:
		rval = val;
		break;
	case SENSOR_VOLTS_DC:
		if (*p != 'V')
			errx(1, "unknown unit %s for voltage sensor", p);
		rval = val * 1000 * 1000;
		break;
	case SENSOR_PERCENT:
		rval = val * 1000.0;
		break;
	case SENSOR_INDICATOR:
	case SENSOR_INTEGER:
	case SENSOR_DRIVE:
		rval = val;
		break;
	case SENSOR_AMPS:
	case SENSOR_WATTHOUR:
	case SENSOR_AMPHOUR:
	case SENSOR_LUX:
		rval = val * 1000 * 1000;
		break;
	case SENSOR_TIMEDELTA:
		rval = val * 1000 * 1000 * 1000;
		break;
	default:
		errx(1, "unsupported sensor type");
		/* not reached */
	}
	free(buf);
	return (rval);
}

/* ARGSUSED */
void
reparse_cfg(int signo)
{
	reload = 1;
}
