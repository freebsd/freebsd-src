#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)ns_maint.c	4.39 (Berkeley) 3/2/91";
static char rcsid[] = "$Id: ns_maint.c,v 8.38 1998/03/16 19:40:25 halley Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1986, 1988
 *    The Regents of the University of California.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1996, 1997 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include "port_before.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <errno.h>
#include <signal.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <isc/eventlib.h>
#include <isc/logging.h>
#include <isc/memcluster.h>

#include "port_after.h"

#include "named.h"

static int		xfers_running,	/* # of xfers running */
			xfers_deferred,	/* # of needed xfers not run yet */
			qserials_running,
			nxfers(struct zoneinfo *, int),
			bottom_of_zone(struct databuf *, int);

static void		startxfer(struct zoneinfo *),
			abortxfer(struct zoneinfo *),
			addxfer(struct zoneinfo *),
			tryxfer(void),
			purge_z_2(struct hashbuf *, int);

#define	qserial_qfull()	(qserials_running == MAXQSERIAL)

static time_t stats_time;

	/* State of all running zone transfers */
static struct {
	pid_t 	xfer_pid;
	int	xfer_state; /* see below */
	WAIT_T	xfer_status;
} xferstatus[MAX_XFERS_RUNNING];

#define XFER_IDLE	0
#define XFER_RUNNING	1
#define XFER_DONE	2


/*
 * Perform maintenance on all zones that need it.
 */
void
ns_maint() {
	struct zoneinfo *zp;
	int zonenum, deleted;

	gettime(&tt);

	ns_debug(ns_log_maint, 1, "ns_maint()");

	for (zp = zones, zonenum = 0; zp < &zones[nzones]; zp++, zonenum++)
		zone_maint(zp);
	ns_debug(ns_log_maint, 1, "exit ns_maint()");
}

/*
 * Perform routine zone maintenance.
 */
void
zone_maint(struct zoneinfo *zp) {
	gettime(&tt);

	ns_debug(ns_log_maint, 1, "zone_maint('%s'); now %lu",
		 zp->z_origin[0] == '\0' ? "." : zp->z_origin,
		 (u_long)tt.tv_sec);

#ifdef DEBUG
	if (debug >= 2)
		printzoneinfo((zp - zones), ns_log_maint, 2);
#endif

	switch (zp->z_type) {
			
	case Z_SECONDARY:
		/*FALLTHROUGH*/
#ifdef STUBS
	case Z_STUB:
#endif
		if (zp->z_serial != 0 &&
		    ((zp->z_lastupdate+zp->z_expire) < (u_int32_t)tt.tv_sec)) {
			zp->z_serial = 0;
		}
		if (zp->z_flags & (Z_NEED_RELOAD|Z_NEED_XFER|Z_QSERIAL)) {
			ns_retrytime(zp, tt.tv_sec);
			break;
		}
		if (zp->z_flags & Z_XFER_RUNNING) {
			abortxfer(zp);
			/*
			 * Check again in 30 seconds in case the first
			 * abort doesn't work.
			 */
			if (zp->z_time != 0 && zp->z_time <= tt.tv_sec)
				zp->z_time = tt.tv_sec + 30;
			break;
		}
		qserial_query(zp);
		break;

#ifdef BIND_UPDATE
	case Z_PRIMARY:
		if (! (zp->z_flags & Z_DYNAMIC))
			break;
		if (tt.tv_sec >= zp->z_soaincrtime &&
		    zp->z_soaincrintvl > 0 &&
		    zp->z_flags & Z_NEED_SOAUPDATE) {
			if (incr_serial(zp) < 0) {
				/* Try again later. */
				ns_error(ns_log_maint,
			      "error updating serial number for %s from %d",
					 zp->z_origin,
					 zp->z_serial);
				zp->z_soaincrtime = 0;
				(void)schedule_soa_update(zp, 0);
			}
			
		}
		if (tt.tv_sec >= zp->z_dumptime &&
		    zp->z_dumpintvl > 0 &&
		    zp->z_flags & Z_NEED_DUMP) {
			if (zonedump(zp) < 0) {
				/* Try again later. */
				ns_error(ns_log_maint,
				 "zone dump for '%s' failed, rescheduling",
					 zp->z_origin);
				zp->z_dumptime = 0;
				(void)schedule_dump(zp);
			}
		} 
		break;
#endif /* BIND_UPDATE */

	default:
		break;
	}

	/*
	 * It is essential that we never try to set a timer in the past
	 * or for now because doing so could cause an infinite loop.
	 */
	INSIST(zp->z_time == 0 || zp->z_time > tt.tv_sec);

	sched_zone_maint(zp);
}

static void
do_zone_maint(evContext ctx, void *uap, struct timespec due,
	      struct timespec inter) {
	ztimer_info zti = uap;
	struct zoneinfo *zp;

	INSIST(zti != NULL);
	
	ns_debug(ns_log_maint, 1, "do_zone_maint for zone %s (class %s)",
		 zti->name, p_class(zti->class));
	zp = find_zone(zti->name, zti->type, zti->class);
	if (zp == NULL) {
		ns_error(ns_log_maint,
		 "do_zone_maint: %s zone '%s' (class %s) is not authoritative",
			 zoneTypeString(zp), zti->name,
			 p_class(zti->class));
		return;
	}

	free_zone_timerinfo(zp);

	zp->z_flags &= ~Z_TIMER_SET;
	zone_maint(zp);
}

/*
 * Figure out the next maintenance time for the zone and set a timer.
 */
void
sched_zone_maint(struct zoneinfo *zp) {
	time_t next_maint = (time_t)0;
	char *zone_name;
	ztimer_info zti;

	if (zp->z_time != 0)
		next_maint = zp->z_time;
#ifdef BIND_UPDATE
	if (zp->z_type == z_master && (zp->z_flags & Z_DYNAMIC)) {
		if (zp->z_soaincrintvl > 0 && 
		    (next_maint == 0 || next_maint > zp->z_soaincrtime))
			next_maint = zp->z_soaincrtime;
		if (zp->z_dumpintvl > 0 &&
		    (next_maint == 0 || next_maint > zp->z_dumptime))
			next_maint = zp->z_dumptime;
	}
#endif

	if (next_maint != 0) {
		if (next_maint < tt.tv_sec)
			next_maint = tt.tv_sec;

		if (zp->z_flags & Z_TIMER_SET) {
			if (next_maint == zp->z_nextmaint) {
				ns_debug(ns_log_maint, 1,
					 "no schedule change for zone '%s'",
					 zp->z_origin[0] == '\0' ? "." :
					 zp->z_origin);
				return;
			}

			if (evResetTimer(ev, zp->z_timer,
					 do_zone_maint, zp->z_timerinfo,
					 evConsTime(next_maint, 0),
					 evConsTime(0, 0)) < 0) {
				ns_error(ns_log_maint,
		 "evChangeTimer failed in sched_zone_maint for zone '%s': %s",
					 zp->z_origin[0] == '\0' ? "." :
					 zp->z_origin,
					 strerror(errno));
				return;
			}
		} else {
			zti = (ztimer_info)memget(sizeof *zti);
			if (zti == NULL)
				ns_panic(ns_log_maint, 1,
				       "memget failed in sched_zone_maint");
			zti->name = savestr(zp->z_origin, 1);
			zti->class = zp->z_class;
			zti->type = zp->z_type;
			if (evSetTimer(ev, do_zone_maint, zti,
				       evConsTime(next_maint, 0),
				       evConsTime(0, 0), &zp->z_timer) < 0) {
				ns_error(ns_log_maint,
		 "evSetTimer failed in sched_zone_maint for zone '%s': %s",
					 zp->z_origin[0] == '\0' ? "." :
					 zp->z_origin,
					 strerror(errno));
				return;
			}
			zp->z_flags |= Z_TIMER_SET;
			zp->z_timerinfo = zti;
		}
		ns_debug(ns_log_maint, 1,
			 "next maintenance for zone '%s' in %lu sec",
			 zp->z_origin[0] == '\0' ? "." : zp->z_origin,
			 (u_long)(next_maint - tt.tv_sec));
	} else {
		if (zp->z_flags & Z_TIMER_SET) {
			free_zone_timerinfo(zp);
			if (evClearTimer(ev, zp->z_timer) < 0)
				ns_error(ns_log_maint,
		 "evClearTimer failed in sched_zone_maint for zone '%s': %s",
					 zp->z_origin[0] == '\0' ? "." :
					 zp->z_origin,
					 strerror(errno));
			zp->z_flags &= ~Z_TIMER_SET;
		}
		ns_debug(ns_log_maint, 1,
			 "no scheduled maintenance for zone '%s'",
			 zp->z_origin[0] == '\0' ? "." : zp->z_origin);
	}
	zp->z_nextmaint = next_maint;
}

void
ns_cleancache(evContext ctx, void *uap,
	      struct timespec due,
	      struct timespec inter)
{
	int deleted;

	gettime(&tt);
	INSIST(uap == NULL);
	deleted = clean_cache(hashtab, 0);
	ns_info(ns_log_maint, "Cleaned cache of %d RR%s",
		deleted, (deleted==1) ? "" : "s");
}

/*
 * Mark a zone "up to date" after named-xfer tells us this or we
 * discover it through the qserial_*() logic.
 * The caller is responsible for calling sched_zone_maint(zp).
 */
static void
markUpToDate(struct zoneinfo *zp) {
	struct stat f_time;

	zp->z_flags &= ~Z_SYSLOGGED;
	zp->z_lastupdate = tt.tv_sec;
	ns_refreshtime(zp, tt.tv_sec);
	/*
	 * Restore Z_AUTH in case expired,
	 * but only if there were no errors
	 * in the zone file.
	 */
	if ((zp->z_flags & Z_DB_BAD) == 0)
		zp->z_flags |= Z_AUTH;
	if (zp->z_source) {
		struct timeval t[2];

		t[0] = tt;
		t[1] = tt;
		(void) utimes(zp->z_source, t);
	}
	/* we use "stat" to set zp->z_ftime instead of just
	   setting it to tt.tv_sec in order to avoid any
	   possible rounding problems in utimes(). */
	if (stat(zp->z_source, &f_time) != -1)
		zp->z_ftime = f_time.st_mtime;
	/* XXX log if stat fails? */
}

void
qserial_retrytime(struct zoneinfo *zp, time_t timebase) {
	zp->z_time = timebase + 5 + (rand() % 25);
}

/*
 * Query for the serial number of a zone, so that we can check to see if
 * we need to transfer it.  If there are too many outstanding serial
 * number queries, we'll try again later.
 * The caller is responsible for calling sched_zone_maint(zp).
 */
void
qserial_query(struct zoneinfo *zp) {
	struct qinfo *qp;

	ns_debug(ns_log_default, 1, "qserial_query(%s)", zp->z_origin);

	if (qserial_qfull()) {
		qserial_retrytime(zp, tt.tv_sec);
		return;
	}

	qp = sysquery(zp->z_origin, zp->z_class, T_SOA,
		      zp->z_addr, zp->z_addrcnt, QUERY);
	if (!qp) {
		ns_info(ns_log_default, "qserial_query(%s): sysquery FAILED",
			zp->z_origin);
		/* XXX - this is bad, we should do something */
		qserial_retrytime(zp, tt.tv_sec);
		return;
	}
	qp->q_flags |= Q_ZSERIAL;
	qp->q_zquery = zp;
	zp->z_flags |= Z_QSERIAL;
	zp->z_xaddr = inaddr_any;
	ns_refreshtime(zp, tt.tv_sec);
	qserials_running++;
	ns_debug(ns_log_default, 1, "qserial_query(%s) QUEUED", zp->z_origin);
}

void
qserial_answer(struct qinfo *qp, u_int32_t serial, struct sockaddr_in from) {
	struct zoneinfo *zp = qp->q_zquery;

	ns_debug(ns_log_default, 1, "qserial_answer(%s, %u)", zp->z_origin,
		 serial);
	zp->z_flags &= ~Z_QSERIAL;
	qp->q_flags &= ~Q_ZSERIAL;	/* keeps us from being called twice */
	qserials_running--;
	if (serial == 0) {
		/* An error occurred, or the query timed out. */
		ns_info(ns_log_default, "Err/TO getting serial# for \"%s\"",
			zp->z_origin);
		addxfer(zp);
	} else if (SEQ_GT(serial, zp->z_serial) || !zp->z_serial) {
		ns_debug(ns_log_default, 1,
			 "qserial_answer: zone is out of date");
		zp->z_xaddr = from.sin_addr; /* don't use qp->q_from */
		addxfer(zp);
	} else if (SEQ_GT(zp->z_serial, serial)) {
		if (!haveComplained((u_long)zp, (u_long)"went backward")) {
			ns_notice(ns_log_default,
   "Zone \"%s\" (class %d) SOA serial# (%u) rcvd from [%s] is < ours (%u)",
				  zp->z_origin, zp->z_class, serial,
				  inet_ntoa(from.sin_addr), zp->z_serial);
		}
	} else {
		ns_debug(ns_log_default, 1,
			 "qserial_answer: zone serial is still OK");
		markUpToDate(zp);
		sched_zone_maint(zp);
	}
}

/*
 * Start an asynchronous zone transfer for a zone.
 * Depends on current time being in tt.
 * Caller must do sched_zone_maint(zp) after startxfer returns.
 */
static void
startxfer(struct zoneinfo *zp) {
	char *argv[NSMAX + 20], argv_ns[NSMAX][MAXDNAME];
	int argc = 0, argc_ns = 0, pid, i;
	u_int cnt;
	char debug_str[10];
	char serial_str[10];
	char port_str[10];
	char class_str[10];
	char src_str[20];

	ns_debug(ns_log_default, 1, "startxfer() %s", zp->z_origin);

	argv[argc++] = server_options->named_xfer;
	argv[argc++] = "-z";
	argv[argc++] = zp->z_origin;
	argv[argc++] = "-f";
	argv[argc++] = zp->z_source;
	argv[argc++] = "-s";
	sprintf(serial_str, "%u", zp->z_serial);
	argv[argc++] = serial_str;
	if (zp->z_axfr_src.s_addr != 0) {
		argv[argc++] = "-x";
		argv[argc++] = strcpy(src_str, inet_ntoa(zp->z_axfr_src));
	}
	argv[argc++] = "-C";
	sprintf(class_str, "%d", zp->z_class);
	argv[argc++] = class_str;
 	if (zp->z_flags & Z_SYSLOGGED)
		argv[argc++] = "-q";
	argv[argc++] = "-P";
	sprintf(port_str, "%d", ns_port);
	argv[argc++] = port_str;
#ifdef STUBS
	if (zp->z_type == Z_STUB)
		argv[argc++] = "-S";
#endif
#ifdef DEBUG
	if (debug) {
		argv[argc++] = "-d";
		sprintf(debug_str, "%d", debug);
		argv[argc++] = debug_str;
		argv[argc++] = "-l";
		argv[argc++] = _PATH_XFERDDT;
		if (debug > 5) {
			argv[argc++] = "-t";
			argv[argc++] = _PATH_XFERTRACE;
		}
	}
#endif
	
	if (ina_hlong(zp->z_xaddr) != INADDR_ANY) {
		/*
		 * Address was specified by the qserial logic, use it
		 * first.
		 */
		if (aIsUs(zp->z_xaddr) &&
		    !haveComplained((u_long)zp, (u_long)startxfer)) {
			ns_notice(ns_log_default,
				  "attempted to fetch zone %s from self (%s)",
				  zp->z_origin, inet_ntoa(zp->z_xaddr));
		} else
			argv[argc++] = strcpy(argv_ns[argc_ns++],
					      inet_ntoa(zp->z_xaddr));
	}
	/*
	 * Copy the server ip addresses into argv, after converting
	 * to ascii and saving the static inet_ntoa result.  Skip zp->z_xaddr
	 * if seen.
	 */
	for (cnt = 0;  cnt < zp->z_addrcnt;  cnt++) {
		struct in_addr a;

		a = zp->z_addr[cnt];
		if (ina_equal(a, zp->z_xaddr))
			continue;
		if (aIsUs(a) &&
		    !haveComplained((u_long)zp, (u_long)startxfer)) {
			ns_notice(ns_log_default,
				  "attempted to fetch zone %s from self (%s)",
				  zp->z_origin, inet_ntoa(a));
			continue;
		}
		argv[argc++] = strcpy(argv_ns[argc_ns++], inet_ntoa(a));
	}

	argv[argc] = NULL;

#ifdef DEBUG
	if (debug >= 1) {
		char buffer[1024];
		char *curr, *last;
		int len;
		
		curr = buffer;
		last = &buffer[sizeof buffer - 1]; /* leave room for \0 */
		for (i = 0; i < argc; i++) {
			len = strlen(argv[i]);
			if (curr + len + 1 >= last) {
				ns_debug(ns_log_xfer_in, 1,
					 "xfer args debug printout truncated");
				break;
			}
			strncpy(curr, argv[i], len);
			curr += len;
			*curr = ' ';
			curr++;
		}
		*curr = '\0';
		ns_debug(ns_log_xfer_in, 1, buffer);
        }
#endif /* DEBUG */

	gettime(&tt);
	for (i = 0; i < MAX_XFERS_RUNNING; i++) {
		if (xferstatus[i].xfer_pid == 0) {
			xferstatus[i].xfer_state = XFER_RUNNING;
			break;
		}
	}
	if ((pid = vfork()) == -1) {
		ns_error(ns_log_default, "xfer vfork: %s", strerror(errno));
		zp->z_time = tt.tv_sec + 10;
		return;
	}

	if (pid == 0) {
		/* Child. */
		execv(server_options->named_xfer, argv);
		ns_error(ns_log_default, "can't exec %s: %s",
			 server_options->named_xfer, strerror(errno));
		_exit(XFER_FAIL);	/* Avoid duplicate buffer flushes. */
	}
	/* Parent. */
	xferstatus[i].xfer_pid = pid;  /* XXX - small race condition here if we
					* can't hold signals */
	ns_debug(ns_log_default, 1, "started xfer child %d", pid);
	zp->z_flags &= ~Z_NEED_XFER;
	zp->z_flags |= Z_XFER_RUNNING;
	zp->z_xferpid = pid;
	xfers_running++;
	if (zp->z_max_transfer_time_in)
		zp->z_time = tt.tv_sec + zp->z_max_transfer_time_in;
	else
		zp->z_time = tt.tv_sec + server_options->max_transfer_time_in;
}

const char *
zoneTypeString(const struct zoneinfo *zp) {
	static char ret[sizeof "(4294967296?)"];	/* 2^32 */

	switch (zp->z_type) {
	case Z_MASTER:		return ("master");
	case Z_SLAVE:		return ("slave");
#ifdef STUBS
	case Z_STUB:		return ("stub");
#endif
	case Z_CACHE:		return ("cache");
	default:
		sprintf(ret, "(%u?)", (u_int32_t)zp->z_type);
		return (ret);
	}
}

#ifdef DEBUG
void
printzoneinfo(int zonenum, int category, int level) {
	struct timeval  tt;
	struct zoneinfo *zp = &zones[zonenum];

	if (debug == 0)
		return;

	if (!zp->z_origin)
		return;

	gettime(&tt);

	ns_debug(category, level, "zone %d: %s, class %s, type %s", zonenum,
		 zp->z_origin[0] ? zp->z_origin : ".",
		 p_class(zp->z_class), zoneTypeString(zp));
	if (zp->z_source)
		ns_debug(category, level, "\tsource %s", zp->z_source);
	ns_debug(category, level, "\tflags %lx, serial %u, minimum %u",
		 (u_long)zp->z_flags, zp->z_serial, zp->z_minimum);
	ns_debug(category, level, "\trefresh %u, retry %u, expire %u",
		 zp->z_refresh, zp->z_retry, zp->z_expire);
	if (zp->z_time)
		ns_debug(category, level, "\tz_time %lu (now %lu, left: %lu)",
			 zp->z_time, (u_long)tt.tv_sec,
			 (u_long)(zp->z_time - tt.tv_sec));
	else
		ns_debug(category, level, "\tz_time %lu", zp->z_time);
#ifdef BIND_UPDATE
	if (zp->z_type == z_master && zp->z_flags & Z_DYNAMIC) {
		ns_debug(category, level,
			 "\tdumpintvl %lu, soaincrintvl %lu deferupdcnt %lu",
			 zp->z_dumpintvl, zp->z_soaincrintvl,
			 zp->z_deferupdcnt);
		if (zp->z_soaincrtime)
			ns_debug(category, level,
				 "\tz_soaincrtime %lu (now %lu, left: %lu)",
				 zp->z_soaincrtime, (u_long)tt.tv_sec,
				 (u_long)(zp->z_soaincrtime - tt.tv_sec));
		else
			ns_debug(category, level, "\tz_soaincrtime %lu",
				 zp->z_soaincrtime);
		if (zp->z_dumptime)
			ns_debug(category, level,
				 "\tz_dumptime %lu (now %lu, left: %lu)",
				 zp->z_dumptime, (u_long)tt.tv_sec,
				 (u_long)(zp->z_dumptime - tt.tv_sec));
		else
			ns_debug(category, level, "\tz_dumptime %lu",
				 zp->z_dumptime);
	}
#endif
}
#endif /* DEBUG */

/* clean_cache(htp, all)
 *	Scan the entire cache looking for expired TTL's on nonauthoritative
 *	data, and remove it.  if `all' is true, ignore TTL and rm everything.
 * notes:
 *	this should be lazy and eventlib driven.
 * return:
 *	number of deleted RRs.
 */
int
clean_cache(struct hashbuf *htp, int all) {
	struct databuf *dp, *pdp;
	struct namebuf *np, *pnp, *npn;
	struct namebuf **npp, **nppend;
	int deleted = 0;

	nppend = htp->h_tab + htp->h_size;
	for (npp = htp->h_tab; npp < nppend; npp++) {
		for (pnp = NULL, np = *npp; np != NULL; np = npn) {
			for (pdp = NULL, dp = np->n_data; dp != NULL;
			     (void)NULL) {
				if (dp->d_zone == DB_Z_CACHE &&
				    (stale(dp) || all)) {
					dp = rm_datum(dp, np, pdp, NULL);
					deleted++;
				} else {
					pdp = dp;
					dp = dp->d_next;
				}
			} /*for(pdp)*/

			if (np->n_hash) {
				/* Call recursively to remove subdomains. */
				deleted += clean_cache(np->n_hash, all);

				/* If now empty, free it */
				if (np->n_hash->h_cnt == 0) {
					rm_hash(np->n_hash);
					np->n_hash = NULL;
				}
			}

			if (np->n_hash == NULL && np->n_data == NULL) {
				npn = rm_name(np, npp, pnp);
				htp->h_cnt--;
			} else {
				npn = np->n_next;
				pnp = np;
			}
		} /*for(pnp)*/
	} /*for(npp)*/
	return (deleted);
}

void
purge_zone(const char *dname, struct hashbuf *htp, int class) {
	const char *fname;
	struct databuf *dp, *pdp;
	struct namebuf *np;
	struct hashbuf *phtp = htp;
	int root_zone = 0;

	ns_debug(ns_log_default, 1, "purge_zone(%s,%d)", dname, class);
	if ((np = nlookup(dname, &phtp, &fname, 0)) && dname == fname &&
	    !ns_wildcard(NAME(*np))) {
		for (pdp = NULL, dp = np->n_data; dp != NULL; (void)NULL) {
			if (dp->d_class == class)
				dp = rm_datum(dp, np, pdp, NULL);
			else {
				pdp = dp;
				dp = dp->d_next;
			}
		}

		if (*dname == '\0')
			root_zone = 1;

		if (np->n_hash != NULL || root_zone) {
			struct hashbuf *h;

			if (root_zone)
				h = htp;
			else
				h = np->n_hash;

			purge_z_2(h, class);
			if (h->h_cnt == 0 && !root_zone) {
				rm_hash(np->n_hash);
				np->n_hash = NULL;
			}
		}
		
		/* remove entry from cache, if required */
		if (np->n_hash == NULL && np->n_data == NULL) {
			struct namebuf **npp, **nppend;
			struct namebuf *npn, *pnp, *nnp;

			ns_debug(ns_log_default, 3,
				 "purge_zone: cleaning cache");

			/* Walk parent hashtable looking for ourself. */
			if (np->n_parent)
				phtp = np->n_parent->n_hash;
			else 
				phtp = htp;	/* top / root zone */

			if (phtp) {
				nppend = phtp->h_tab + phtp->h_size;
				for (npp = phtp->h_tab; npp < nppend; npp++) {
				    for (pnp = NULL, nnp = *npp;
					 nnp != NULL;
					 nnp = npn) {
					    if (nnp == np) {
						    ns_debug(ns_log_default, 3,
					     "purge_zone: found our selves");
						    npn = rm_name(nnp,npp,pnp);
						    phtp->h_cnt--;
					    } else {
						    npn = nnp->n_next;
						    pnp = nnp;
					    }
				    }
				}
			}
		}
	}
}

static void
purge_z_2(htp, class)
	struct hashbuf *htp;
	int class;
{
	struct databuf *dp, *pdp;
	struct namebuf *np, *pnp, *npn;
	struct namebuf **npp, **nppend;

	nppend = htp->h_tab + htp->h_size;
	for (npp = htp->h_tab; npp < nppend; npp++) {
		for (pnp = NULL, np = *npp; np != NULL; np = npn) {
			if (!bottom_of_zone(np->n_data, class)) {
				for (pdp = NULL, dp = np->n_data;
				     dp != NULL;
				     (void)NULL) {
					if (dp->d_class == class)
						dp = rm_datum(dp, np, pdp, 
							      NULL);
					else {
						pdp = dp;
						dp = dp->d_next;
					}
				}
				if (np->n_hash) {
					/* call recursively to rm subdomains */
					purge_z_2(np->n_hash, class);

					/* if now empty, free it */
					if (np->n_hash->h_cnt == 0) {
						rm_hash(np->n_hash);
						np->n_hash = NULL;
					}
				}
			}

			if (np->n_hash == NULL && np->n_data == NULL) {
				npn = rm_name(np, npp, pnp);
				htp->h_cnt--;
			} else {
				npn = np->n_next;
				pnp = np;
			}
		}
	}
}

static int
bottom_of_zone(struct databuf *dp, int class) {
	int ret = 0;

	for ((void)NULL; dp; dp = dp->d_next) {
		if (dp->d_class != class)
			continue;
		if (dp->d_zone == DB_Z_CACHE)
			continue;
		if (dp->d_rcode)	/* This should not occur. */
			continue;
		if (dp->d_type != T_SOA)
			continue;
		ret = 1;
		break;
	}
	ns_debug(ns_log_default, 3, "bottom_of_zone() == %d", ret);
	return (ret);
}
   
/*
 * Handle XFER limit for a nameserver.
 */
static int
nxfers(struct zoneinfo *zp, int delta) {
	struct in_addr nsa;
	struct nameser *nsp;
	int ret;

	if (ina_hlong(zp->z_xaddr) != INADDR_ANY)
		nsa = zp->z_xaddr;	/* qserial overrode address */
	else if (!zp->z_addrcnt)
		return (-1);
	else
		nsa = zp->z_addr[0];	/* first ns holds zone's xfer limit */

	if (!(nsp = nameserFind(nsa, NS_F_INSERT)))
		return (-1);		/* probably ENOMEM */

	ret = nsp->xfers;
	if (delta < 0 && -delta > ret)
		return (-1);		/* taking more than we have */

	nsp->xfers += delta;
	return (ret);
}

/*
 * Abort an xfer that has taken too long.
 */
static void
abortxfer(struct zoneinfo *zp) {
	if (zp->z_flags & (Z_XFER_GONE|Z_XFER_ABORTED)) {
		int i;

		for (i = 0; i < MAX_XFERS_RUNNING; i++) {
			if (xferstatus[i].xfer_pid == zp->z_xferpid) {
				xferstatus[i].xfer_pid = 0;
				xferstatus[i].xfer_state = XFER_IDLE;
				break;
			}
		}

		if (zp->z_flags & Z_XFER_GONE)
			ns_warning(ns_log_default,
			   "zone transfer timeout for \"%s\"; pid %lu missing",
				   zp->z_origin, (u_long)zp->z_xferpid);
		else if (kill(zp->z_xferpid, SIGKILL) == -1)
			ns_warning(ns_log_default,
			  "zone transfer timeout for \"%s\"; kill pid %lu: %s",
				   zp->z_origin, (u_long)zp->z_xferpid,
				   strerror(errno));
		else
			ns_warning(ns_log_default,
"zone transfer timeout for \"%s\"; second kill \
pid %lu - forgetting, processes may accumulate",
				   zp->z_origin, (u_long)zp->z_xferpid);

		zp->z_xferpid = 0;
		xfers_running--;
		(void)nxfers(zp, -1);
		zp->z_flags &= ~(Z_XFER_RUNNING|Z_XFER_ABORTED|Z_XFER_GONE);
	} else if (kill(zp->z_xferpid, SIGTERM) == -1) {
		if (errno == ESRCH)
			/* No warning on first time, it may have just exited */
			zp->z_flags |= Z_XFER_GONE;
		else {
			ns_warning(ns_log_default,
		    "zone transfer timeout for \"%s\"; pid %lu kill failed %s",
				   zp->z_origin, (u_long)zp->z_xferpid,
				   strerror(errno));
			zp->z_flags |= Z_XFER_ABORTED;
		}
	} else {
		ns_notice(ns_log_default,
			  "zone transfer timeout for \"%s\"; pid %lu killed",
			  zp->z_origin, (u_long)zp->z_xferpid);
		zp->z_flags |= Z_XFER_ABORTED;
	}
}

/*
 * SIGCHLD signal handler: process exit of xfer's.
 */
void
reapchild(evContext ctx, void *uap, int sig) {
	int pid, i;
	WAIT_T status;
	int saved_errno = errno;

	gettime(&tt);
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
		for (i = 0; i < MAX_XFERS_RUNNING; i++) {
			if (xferstatus[i].xfer_pid == pid) {
				xferstatus[i].xfer_status = status;
				xferstatus[i].xfer_state = XFER_DONE;
				ns_need(MAIN_NEED_ENDXFER);
				break;
			}
		}
	}

	errno = saved_errno;
}

/*
 * Finish processing of of finished xfers
 */
void
endxfer() {
    	struct zoneinfo *zp;   
	int exitstatus, pid, i;
	WAIT_T status;

	gettime(&tt);

	for (i = 0; i < MAX_XFERS_RUNNING; i++) {
		if (xferstatus[i].xfer_state != XFER_DONE)
			continue;
		pid = xferstatus[i].xfer_pid;
		status = xferstatus[i].xfer_status;
		exitstatus = WIFEXITED(status) ? WEXITSTATUS(status) : 0;

		for (zp = zones; zp < &zones[nzones]; zp++) {
			if (zp->z_xferpid != pid)
				continue;
			xfers_running--;
			(void) nxfers(zp, -1);
			zp->z_xferpid = 0;
			zp->z_flags &=
				~(Z_XFER_RUNNING|Z_XFER_ABORTED|Z_XFER_GONE);
			ns_debug(ns_log_default, 1,
		 "\nendxfer: child %d zone %s returned status=%d termsig=%d",
				 pid, zp->z_origin, exitstatus,
				 WIFSIGNALED(status) ? WTERMSIG(status) : -1);
			if (WIFSIGNALED(status)) {
				if (WTERMSIG(status) != SIGKILL) {
					ns_notice(ns_log_default,
                                    "named-xfer \"%s\" exited with signal %d",
                                    zp->z_origin[0]?zp->z_origin:".",
						  WTERMSIG(status));
				}
				ns_retrytime(zp, tt.tv_sec);
				sched_zone_maint(zp);
			} else {
				switch (exitstatus) {
				case XFER_UPTODATE:
					markUpToDate(zp);
					sched_zone_maint(zp);
					break;

				case XFER_SUCCESS:
					/* XXX should incorporate loadxfer() */
					zp->z_flags |= Z_NEED_RELOAD;
					zp->z_flags &= ~Z_SYSLOGGED;
					ns_need(MAIN_NEED_ZONELOAD);
					break;

				case XFER_TIMEOUT:
					if (!(zp->z_flags & Z_SYSLOGGED)) {
						zp->z_flags |= Z_SYSLOGGED;
						ns_notice(ns_log_default,
		      "zoneref: Masters for secondary zone \"%s\" unreachable",
							  zp->z_origin);
					}
					ns_retrytime(zp, tt.tv_sec);
					sched_zone_maint(zp);
					break;

				default:
					if (!(zp->z_flags & Z_SYSLOGGED)) {
						zp->z_flags |= Z_SYSLOGGED;
						ns_notice(ns_log_default,
					     "named-xfer for \"%s\" exited %d",
							  zp->z_origin,
							  exitstatus);
					}
					/* FALLTHROUGH */
				case XFER_FAIL:
					zp->z_flags |= Z_SYSLOGGED;
					ns_retrytime(zp, tt.tv_sec);
					sched_zone_maint(zp);
					break;
				}
				break;
			}
		}
		xferstatus[i].xfer_state = XFER_IDLE;
		xferstatus[i].xfer_pid = 0;
	}
	tryxfer();
}

/*
 * Try to start some xfers - new "fair scheduler" by Bob Halley @DEC (1995)
 */
static void
tryxfer() {
	static struct zoneinfo *zp = NULL;
	static struct zoneinfo *lastzones = NULL;
	static int lastnzones = 0;
	struct zoneinfo *startzp, *stopzp;

	/* initialize, and watch out for changes in zones! */
	if (lastzones != zones) {
		if (lastzones != NULL)
			ns_debug(ns_log_default, 3, "zones changed: %p != %p",
				 lastzones, zones);
		lastzones = zones;
		zp = zones;
	}

	/* did zones shrink? */
	if (lastnzones > nzones) {
		ns_debug(ns_log_default, 3, "zones shrunk");
		zp = zones;
	}
	lastnzones = nzones;
    
	if (zp == zones)
		stopzp = &zones[nzones-1];
	else
		stopzp = zp - 1;

	ns_debug(ns_log_default, 3,
		 "tryxfer start zp=%p stopzp=%p def=%d running=%d",
		zp, stopzp, xfers_deferred, xfers_running);

	startzp = zp;
	for (;;) {
		int xfers;

		if (!xfers_deferred ||
		    xfers_running >= server_options->transfers_in)
			break;
		
		if ((xfers = nxfers(zp, 0)) != -1 &&
		    xfers < server_options->transfers_per_ns &&
		    (zp->z_flags & Z_NEED_XFER)) {
			nxfers(zp, 1);
			xfers_deferred--;
			startxfer(zp);
			sched_zone_maint(zp);
		}

		if (zp == stopzp) {
			ns_debug(ns_log_default, 3, "tryxfer stop mark");
			zp = startzp;
			break;
		}

		zp++;
		/* wrap around? */
		if (zp == &zones[nzones])
			zp = zones;
	}
	ns_debug(ns_log_default, 3, "tryxfer stop zp=%p", zp);
}

/*
 * Reload zones whose transfers have completed.
 */
void
loadxfer() {
    	struct zoneinfo *zp;   

	gettime(&tt);
	for (zp = zones; zp < &zones[nzones]; zp++) {
		if (zp->z_flags & Z_NEED_RELOAD) {
			ns_debug(ns_log_default, 1, "loadxfer() \"%s\"",
				 zp->z_origin[0] ? zp->z_origin : ".");
			zp->z_flags &= ~(Z_NEED_RELOAD|Z_AUTH);
/* XXX this is bad, should be done in ns_reload() for primary changes. */
			ns_stopxfrs(zp);
			purge_zone(zp->z_origin, hashtab, zp->z_class);
			if (!db_load(zp->z_source, zp->z_origin, zp, NULL))
				zp->z_flags |= Z_AUTH;
			if (zp->z_flags & Z_TMP_FILE)
				(void) unlink(zp->z_source);
			sched_zone_maint(zp);
		}
	}
}

/*
 * Add this zone to the set of those needing transfers.
 */
static void
addxfer(struct zoneinfo *zp) {
	if (!(zp->z_flags & Z_NEED_XFER)) {
		zp->z_flags |= Z_NEED_XFER;
		xfers_deferred++;
		tryxfer();
	}
}

/*
 * Flush and reload data base.
 */
void
ns_reload() {
	ns_notice(ns_log_default, "reloading nameserver");

	qflush();
	sq_flush(NULL);
#ifdef FORCED_RELOAD
	reloading = 1;     /* to force transfer if secondary and backing up */
#endif
	ns_init(conffile);
	time(&resettime);
#ifdef FORCED_RELOAD
	reloading = 0;
#endif /* FORCED_RELOAD */
	ns_notice(ns_log_default, "Ready to answer queries.");
}
