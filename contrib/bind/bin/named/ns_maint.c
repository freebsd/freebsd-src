#if !defined(lint) && !defined(SABER)
static const char sccsid[] = "@(#)ns_maint.c	4.39 (Berkeley) 3/2/91";
static const char rcsid[] = "$Id: ns_maint.c,v 8.137.8.1 2003/06/02 05:34:25 marka Exp $";
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
 * Portions Copyright (c) 1996-2000 by Internet Software Consortium.
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

/*
 * Portions Copyright (c) 1999 by Check Point Software Technologies, Inc.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Check Point Software Technologies Incorporated not be used 
 * in advertising or publicity pertaining to distribution of the document 
 * or software without specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND CHECK POINT SOFTWARE TECHNOLOGIES 
 * INCORPORATED DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.   
 * IN NO EVENT SHALL CHECK POINT SOFTWARE TECHNOLOGIES INCORPRATED
 * BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR 
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT 
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "port_before.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <assert.h>
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
#include <isc/dst.h>
#include <isc/misc.h>

#include "port_after.h"

#include "named.h"

static int		nxfers(struct zoneinfo *),
			bottom_of_zone(struct databuf *, int);

static void		startxfer(struct zoneinfo *),
			abortxfer(struct zoneinfo *),
			purge_z_2(struct hashbuf *, int);
static int		purge_nonglue_2(const char *, struct hashbuf *,
					int, int, int, int);

#ifndef HAVE_SPAWNXFER
static pid_t		spawnxfer(char **, struct zoneinfo *);
#endif

	/* State of all running zone transfers */
static struct {
	pid_t 		xfer_pid;
	int		xfer_state; /* see below */
	WAIT_T		xfer_status;
	struct in_addr	xfer_addr;
} xferstatus[MAX_XFERS_RUNNING];

#define XFER_IDLE	0
#define XFER_RUNNING	1
#define XFER_DONE	2


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
			if ((zp->z_flags & Z_NOTIFY) != 0)
				ns_stopnotify(zp->z_origin, zp->z_class);
			/* calls purge_zone */
			do_reload(zp, 0);
			/* reset zone state */
			if (!haveComplained((u_long)zp, (u_long)stale)) {
				ns_notice(ns_log_default,
					  "%s zone \"%s\" expired",
					  zoneTypeString(zp->z_type),
					  zp->z_origin);
			}
			zp->z_flags &= ~Z_AUTH;
			zp->z_flags |= Z_EXPIRED;
			zp->z_refresh = INIT_REFRESH;
			zp->z_retry = INIT_REFRESH;
			zp->z_serial = 0;
		}
		if ((zp->z_flags & (Z_NEED_RELOAD|Z_NEED_XFER|Z_QSERIAL)) != 0)
		{
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
		/*
		 * If we don't have the zone loaded or dialup is off 
		 * or we attempted a qserial_query before and the queue was
		 * full attempt to verify / load the zone.
		 */
		if ((zp->z_serial == 0) || (zp->z_flags & Z_NEED_QSERIAL) ||
		    (zp->z_dialup == zdialup_no) || 
		    (zp->z_dialup == zdialup_use_default && 
		     NS_OPTION_P(OPTION_NODIALUP)))
			qserial_query(zp);
		else {
			ns_info(ns_log_default, "Suppressed qserial_query(%s)",
				*(zp->z_origin) ? zp->z_origin : ".");
			ns_refreshtime(zp, tt.tv_sec);
		}
		break;

#ifdef BIND_UPDATE
	case Z_PRIMARY:
		if ((zp->z_flags & Z_DYNAMIC) == 0)
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
			if (zonedump(zp, ISNOTIXFR) < 0) {
				/* Try again later. */
				ns_error(ns_log_maint,
				 "zone dump for '%s' failed, rescheduling",
					 zp->z_origin);
				zp->z_dumptime = 0;
				(void)schedule_dump(zp);
			}
		} 
		if (zp->z_maintain_ixfr_base)
			ixfr_log_maint(zp);
		break;
#endif /* BIND_UPDATE */

	default:
		break;
	}

	if (zp->z_time != 0 && zp->z_time < tt.tv_sec)
		zp->z_time = tt.tv_sec;

	sched_zone_maint(zp);
}

static void
do_zone_maint(evContext ctx, void *uap, struct timespec due,
	      struct timespec inter) {
	ztimer_info zti = uap;
	struct zoneinfo *zp;

	UNUSED(ctx);
	UNUSED(due);
	UNUSED(inter);

	INSIST(zti != NULL);
	
	ns_debug(ns_log_maint, 1, "do_zone_maint for zone %s (class %s)",
		 zti->name, p_class(zti->class));
	zp = find_zone(zti->name, zti->class);
	if (zp == NULL) {
		ns_error(ns_log_maint,
		 "do_zone_maint: %s zone '%s' (class %s) is not authoritative",
			 zoneTypeString(zti->type), zti->name,
			 p_class(zti->class));
		return;
	}
	if (zp->z_type != zti->type) {
		ns_error(ns_log_maint,
		 "do_zone_maint: %s zone '%s' (class %s) has changed its type",
			 zoneTypeString(zti->type), zti->name,
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
	ztimer_info zti;

	if (zp->z_time != 0)
		next_maint = zp->z_time;
#ifdef BIND_UPDATE
	if (zp->z_type == z_master && (zp->z_flags & Z_DYNAMIC) != 0) {
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

	UNUSED(ctx);
	UNUSED(due);
	UNUSED(inter);

	gettime(&tt);
	INSIST(uap == NULL);
	deleted = clean_cache(hashtab, 0);
	ns_info(ns_log_maint, "Cleaned cache of %d RRset%s",
		deleted, (deleted==1) ? "" : "s");
}

void
ns_heartbeat(evContext ctx, void *uap, struct timespec due,
	     struct timespec inter)
{
	struct zoneinfo *zp;

	UNUSED(ctx);
	UNUSED(due);
	UNUSED(inter);

	gettime(&tt);
	INSIST(uap == NULL);

	for (zp = zones; zp < &zones[nzones]; zp++) {
		enum zonetype zt = zp->z_type;

		if ((zt == z_nil) ||
		    (zp->z_dialup == zdialup_no) ||
		    (zp->z_dialup == zdialup_use_default && 
		     NS_OPTION_P(OPTION_NODIALUP)))
			continue;
		/*
		 * Perform the refresh query that was suppressed.
		 */
		if ((zt == z_slave || zt == z_stub) &&
		    (zp->z_flags &
		     (Z_NEED_RELOAD|Z_NEED_XFER|Z_QSERIAL|Z_XFER_RUNNING)
		     ) == 0) {
			ns_info(ns_log_default,
				"Heartbeat: qserial \"%s\"",
				*(zp->z_origin) ? zp->z_origin : ".");
			qserial_query(zp);
		}
#ifdef BIND_NOTIFY
		/*
		 * Trigger a refresh query while the link is up by
		 * sending a notify.
		 */
		if (((zp->z_notify == notify_yes) ||
		     (zp->z_notify == notify_explicit) ||
		     ((zp->z_notify == notify_use_default) &&
		      server_options->notify != notify_no)) &&
		    (zt == z_master || zt == z_slave) && !loading &&
		    ((zp->z_flags & Z_AUTH) != 0))
			ns_notify(zp->z_origin, zp->z_class, ns_t_soa);
#endif
	}
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
	if ((zp->z_flags & Z_DB_BAD) == 0) {
		zp->z_flags |= Z_AUTH;
		zp->z_flags &= ~Z_EXPIRED;
	}
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

	if (qserials_running >= server_options->serial_queries) {
		qserial_retrytime(zp, tt.tv_sec);
		zp->z_flags |= Z_NEED_QSERIAL;
		return;
	}

	qp = sysquery(zp->z_origin, zp->z_class, T_SOA,
		      zp->z_addr, zp->z_keys, zp->z_addrcnt,
		      ntohs(zp->z_port) ? zp->z_port : ns_port,
		      QUERY, 0);
	if (qp == NULL) {
		ns_debug(ns_log_default, 1,
			 "qserial_query(%s): sysquery FAILED",
			 zp->z_origin);
		/* XXX - this is bad, we should do something */
		qserial_retrytime(zp, tt.tv_sec);
		zp->z_flags |= Z_NEED_QSERIAL;
		return;
	}
	qp->q_flags |= Q_ZSERIAL;
	qp->q_zquery = zp;
	zp->z_flags |= Z_QSERIAL;
	zp->z_flags &= ~Z_NEED_QSERIAL;
	zp->z_xaddrcnt = 0;
	ns_refreshtime(zp, tt.tv_sec);
	qserials_running++;
	ns_debug(ns_log_default, 1, "qserial_query(%s) QUEUED", zp->z_origin);
}

static int
qserv_compare(const void *a, const void *b) {
	const struct qserv *qs1 = a, *qs2 = b;
	u_int32_t s1 = qs1->serial, s2 = qs2->serial;

	/* Note that we sort the "best" serial numbers to the front. */
	if (s1 == s2)
		return (0);
	if (s1 == 0)
		return (-1);
	if (s2 == 0)
		return (1);
	if (!SEQ_GT(s1, s2))
		return (1);
	assert(SEQ_GT(s1, s2));
	return (-1);
}

void
qserial_answer(struct qinfo *qp) {
	struct zoneinfo *zp = qp->q_zquery;
	struct qserv *qs = NULL;
	u_int32_t serial = 0;
	int n, cnt = 0;

	/* Take this query out of the global quotas. */
	zp->z_flags &= ~Z_QSERIAL;
	qp->q_flags &= ~Q_ZSERIAL;	/* keeps us from being called twice */
	qserials_running--;

	/* Find best serial among those returned. */
	for (n = 0; n < qp->q_naddr; n++) {
		qs = &qp->q_addr[n];
		ns_debug(ns_log_default, 1, "qserial_answer(%s): [%s] -> %lu",
			 zp->z_origin, inet_ntoa(qs->ns_addr.sin_addr),
			 (unsigned long)qs->serial);
		/* Don't consider serials which weren't set by a response. */
		if (qs->serial == 0)
			continue;
		/* Count valid answers. */
		cnt++;
		/* Remove from consideration serials which aren't "better." */
		if (zp->z_serial != 0 && !SEQ_GT(qs->serial, zp->z_serial)) {
			if (serial == 0 && qs->serial == zp->z_serial)
				serial = qs->serial;

			if (qs->serial != zp->z_serial)
				ns_notice(ns_log_xfer_in,
	 "Zone \"%s\" (%s) SOA serial# (%lu) rcvd from [%s] is < ours (%lu)%s",
					  zp->z_origin, p_class(zp->z_class),
					  (u_long) qs->serial,
					  inet_ntoa(qs->ns_addr.sin_addr),
					  (u_long) zp->z_serial,
					  qp->q_naddr!=1 ? ": skipping" : "");
			qs->serial = 0;
			continue;
		}
		if (serial == 0 || SEQ_GT(qs->serial, serial))
			serial = qs->serial;
	}

	/* If we have an existing serial number, then sort by "better." */
	if (zp->z_serial != 0) {
		qsort(qp->q_addr, qp->q_naddr, sizeof(struct qserv),
		      qserv_compare);
		for (n = 0; n < qp->q_naddr; n++) {
			qs = &qp->q_addr[n];
			ns_debug(ns_log_default, 1,
				 "qserial_answer after sort: [%s] -> %lu",
				 inet_ntoa(qs->ns_addr.sin_addr),
				 (unsigned long)qs->serial);
		}
	}

	/* Now see about kicking off an inbound transfer. */
	if (serial == 0) {
		/* An error occurred, or the all queries timed out. */
		if (qp->q_naddr != cnt)
			ns_info(ns_log_xfer_in,
				"Err/TO getting serial# for \"%s\"",
				zp->z_origin);
		addxfer(zp);
	} else if (zp->z_serial == 0 || SEQ_GT(serial, zp->z_serial)) {
		ns_debug(ns_log_xfer_in, 1,
			 "qserial_answer: zone is out of date");
		/* Use all servers whose serials are better than ours. */
		zp->z_xaddrcnt = 0;
		for (n = 0; n < qp->q_naddr; n++) {
			qs = &qp->q_addr[n];
			if (qs->serial != 0)
				zp->z_xaddr[zp->z_xaddrcnt++] =
					qs->ns_addr.sin_addr;
		}
		addxfer(zp);
	} else if (zp->z_serial == serial) {
		ns_debug(ns_log_xfer_in, 1,
			 "qserial_answer: zone serial is still OK");
		markUpToDate(zp);
		sched_zone_maint(zp);
	}
}

/*
 * Writes TSIG key info for an address to a file, optionally opening it first.
 * Returns:
 *	-1:	Error.
 *	 0:	No action taken.
 *	 1:	Tsig info successfully written.
 */
static int
write_tsig_info(struct zoneinfo *zp, struct in_addr addr, char *name, int *fd) {
	server_info si;
	DST_KEY *dst_key = NULL;
	int tsig_fd = *fd;
	char tsig_str[1024], secret_buf64[172];
	u_char secret_buf[128];
	int secret_len, len;
	int i;

	for (i = 0; i < zp->z_addrcnt ; i++)
		if (memcmp(&addr, &zp->z_addr[i], sizeof(addr)) == 0) {
			dst_key = zp->z_keys[i];
			break;
		}

	if (dst_key == NULL) {
		si = find_server(addr);
		if (si == NULL || si->key_list == NULL ||
		    si->key_list->first == NULL)
			return(0);
		dst_key = si->key_list->first->key;
	}
	if (tsig_fd == -1) {
		*fd = tsig_fd = mkstemp(name);
		if (tsig_fd < 0) {
			ns_warning(ns_log_default,
			   "write_tsig_info: mkstemp(%s) for TSIG info failed",
				   name);
			return(-1);
		}
		(void) fchown(tsig_fd, user_id, group_id);
	}

	memset(secret_buf, 0, sizeof(secret_buf));
	secret_len = dst_key_to_buffer(dst_key, secret_buf, sizeof(secret_buf));
	if (secret_len == 0)
		return (-1);
	len = b64_ntop(secret_buf, secret_len, secret_buf64,
		       sizeof(secret_buf64));
	if (len == -1)
		return (-1);
	/* We need snprintf! */
	if (strlen(dst_key->dk_key_name) + len + sizeof("XXX.XXX.XXX.XXX") +
	    sizeof("123") + 5 > sizeof(tsig_str))
		return (-1);
	sprintf(tsig_str, "%s\n%s\n%d\n%s\n",
		inet_ntoa(addr), dst_key->dk_key_name, dst_key->dk_alg,
			secret_buf64);
	len = strlen(tsig_str);
	if (write(tsig_fd, tsig_str, strlen(tsig_str)) != len)
		return (-1);
	return (1);
}

/*
 * Returns number of tsigs written or -1.
 */
static int
write_tsigs(struct zoneinfo *zp, char *tsig_name) {
	struct in_addr a;
	int tsig_ret;
	int tsig_fd = -1;
	int cnt;
	int records = 0;

	for (cnt = 0; cnt < zp->z_xaddrcnt; cnt++) {
		a = zp->z_xaddr[cnt];
		if (aIsUs(a) && ns_port == zp->z_port)
			continue;

		tsig_ret = write_tsig_info(zp, a, tsig_name, &tsig_fd);
		switch (tsig_ret) {
		case -1:
			goto error;
		case 0:
			break;
		case 1:
			records++;
			break;
		}
	}

	if (tsig_fd != -1)
		close(tsig_fd);
	return (records);

 error:
	if (tsig_fd != -1) {
		unlink(tsig_name);
		close(tsig_fd);
	}
	return (-1);
}

#ifdef BIND_IXFR
static int
supports_ixfr(struct zoneinfo *zp) {
	int cnt = 0;
	for (cnt = 0; cnt < zp->z_xaddrcnt; cnt++) {
		struct in_addr a;
		server_info si;

		a = zp->z_xaddr[cnt];
		if (aIsUs(a) && ns_port == zp->z_port)
			continue;
		si = find_server(a);

		if (si != NULL && (si->flags & SERVER_INFO_SUPPORT_IXFR) != 0)
			return(1);
	}
	return(0);
}
#endif

/*
 * Start an asynchronous zone transfer for a zone.  Depends on current time
 * being in tt.  Caller must do a sched_zone_maint(zp) after we return.
 */
static void
startxfer(struct zoneinfo *zp) {
	char *argv[NSMAX*2 + 20];
	char argv_ns[NSMAX][MAXDNAME];
	int argc = 0, argc_ns = 0, i;
	pid_t pid;
	u_int cnt;
	char debug_str[10];
	char serial_str[10];
	char port_str[10];
	char class_str[10];
	char src_str[20];
	char tsig_name[MAXPATHLEN+1];
	int tsig_ret = 0;

	ns_debug(ns_log_default, 1, "startxfer() %s",
		 zp->z_origin[0] != '\0' ? zp->z_origin : ".");

	argv[argc++] = server_options->named_xfer;
	DE_CONST("-z", argv[argc++]);
	DE_CONST(*zp->z_origin ? zp->z_origin : ".", argv[argc++]);
	DE_CONST("-f", argv[argc++]);
	argv[argc++] = zp->z_source;
#ifdef BIND_IXFR
	if (supports_ixfr(zp) && zp->z_ixfr_tmp != NULL) {
		DE_CONST("-i", argv[argc++]);
		argv[argc++] = zp->z_ixfr_tmp;
	}
#endif
	if (zp->z_serial != 0) {
		DE_CONST("-s", argv[argc++]);
		sprintf(serial_str, "%u", zp->z_serial);
		argv[argc++] = serial_str;
	}
	if (zp->z_axfr_src.s_addr != 0 ||
	    server_options->axfr_src.s_addr != 0) {
		DE_CONST("-x", argv[argc++]);
		argv[argc++] = strcpy(src_str, inet_ntoa(
			(zp->z_axfr_src.s_addr != 0) ? zp->z_axfr_src :
			server_options->axfr_src));
	}
	DE_CONST("-C", argv[argc++]);
	sprintf(class_str, "%d", zp->z_class);
	argv[argc++] = class_str;
 	if (zp->z_flags & Z_SYSLOGGED)
		DE_CONST("-q", argv[argc++]);
	DE_CONST("-P", argv[argc++]);
	sprintf(port_str, "%d", ntohs(zp->z_port) != 0 ? zp->z_port : ns_port);
	argv[argc++] = port_str;
#ifdef STUBS
	if (zp->z_type == Z_STUB)
		DE_CONST("-S", argv[argc++]);
#endif
#ifdef DEBUG
	if (debug) {
		DE_CONST("-d", argv[argc++]);
		sprintf(debug_str, "%d", debug);
		argv[argc++] = debug_str;
		DE_CONST("-l", argv[argc++]);
		DE_CONST(_PATH_XFERDDT, argv[argc++]);
		if (debug > 5) {
			DE_CONST("-t", argv[argc++]);
			DE_CONST(_PATH_XFERTRACE, argv[argc++]);
		}
	}
#endif
	
	if (zp->z_xaddrcnt == 0) {
		for (zp->z_xaddrcnt = 0;
		     zp->z_xaddrcnt < zp->z_addrcnt;
		     zp->z_xaddrcnt++)
			zp->z_xaddr[zp->z_xaddrcnt] =
				zp->z_addr[zp->z_xaddrcnt];
	}

	/*
	 * Store TSIG keys if we have them.
	 */
	strcpy(tsig_name, "tsigs.XXXXXX");
	tsig_ret = write_tsigs(zp, tsig_name);
	if (tsig_ret == -1) {
		ns_error(ns_log_xfer_in, "unable to write tsig info: '%s'",
			 zp->z_origin);
		return;
	}
	if (tsig_ret != 0) {
		DE_CONST("-T", argv[argc++]);
		argv[argc++] = tsig_name;
	}

	/*
	 * Copy the server ip addresses into argv, after converting
	 * to ascii and saving the static inet_ntoa result.
	 * Also, send TSIG key info into a file for the child.
	 */
	for (cnt = 0; cnt < zp->z_xaddrcnt; cnt++) {
		struct in_addr a;

		a = zp->z_xaddr[cnt];
		if (aIsUs(a) && ns_port == zp->z_port) {
			if (!haveComplained((u_long)zp, (u_long)startxfer))
				ns_notice(ns_log_default,
				   "attempted to fetch zone %s from self (%s)",
					  zp->z_origin, inet_ntoa(a));
			continue;
		}
		argv[argc++] = strcpy(argv_ns[argc_ns++], inet_ntoa(a));
#ifdef BIND_IXFR
		if (zp->z_ixfr_tmp != NULL) {
			server_info si = find_server(a);

			if (si != NULL &&
			    (si->flags & SERVER_INFO_SUPPORT_IXFR) != 0)
				DE_CONST("ixfr", argv[argc++]);
			else
				DE_CONST("axfr", argv[argc++]);
		}
#endif
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
			if (len + 1 >= last - curr) {
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
		ns_debug(ns_log_xfer_in, 1, "%s", buffer);
	}
#endif /* DEBUG */

	gettime(&tt);
	for (i = 0; i < MAX_XFERS_RUNNING; i++)
		if (xferstatus[i].xfer_pid == 0)
			break;
	if (i == MAX_XFERS_RUNNING) {
		ns_warning(ns_log_default,
			   "startxfer: too many xfers running");
		zp->z_time = tt.tv_sec + 10;
		return;
	}
	
	if ((pid = spawnxfer(argv, zp)) == -1) {
		unlink(tsig_name);
		return;
	}
	
	xferstatus[i].xfer_state = XFER_RUNNING;
	xferstatus[i].xfer_pid = pid;  /* XXX - small race condition here if we
					* can't hold signals */
	xferstatus[i].xfer_addr = zp->z_xaddr[0];
	ns_debug(ns_log_default, 1, "started xfer child %d", pid);
	zp->z_flags &= ~Z_NEED_XFER;
	zp->z_flags |= Z_XFER_RUNNING;
	zp->z_xferpid = pid;
	xfers_running++;
	xfers_deferred--;
	if (zp->z_max_transfer_time_in)
		zp->z_time = tt.tv_sec + zp->z_max_transfer_time_in;
	else
		zp->z_time = tt.tv_sec + server_options->max_transfer_time_in;
}

const char *
zoneTypeString(u_int type) {
	static char ret[sizeof "(4294967296?)"];	/* 2^32 */

	switch (type) {
	case Z_MASTER:		return ("master");
	case Z_SLAVE:		return ("slave");
#ifdef STUBS
	case Z_STUB:		return ("stub");
#endif
	case Z_HINT:		return ("hint");
	case Z_CACHE:		return ("cache");
	case Z_FORWARD:		return ("forward");
	default:
		sprintf(ret, "(%u?)", type);
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
		 p_class(zp->z_class), zoneTypeString(zp->z_type));
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
	if (zp->z_type == z_master && (zp->z_flags & Z_DYNAMIC) != 0) {
		ns_debug(category, level,
			 "\tdumpintvl %lu, soaincrintvl %lu deferupdcnt %lu",
			 (unsigned long)zp->z_dumpintvl,
			 (unsigned long)zp->z_soaincrintvl,
			 (unsigned long)zp->z_deferupdcnt);
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

/*
 * Remove all cached data below dname, class independent.
 */
void
clean_cache_from(char *dname, struct hashbuf *htp) {
	const char *fname;
	struct databuf *dp, *pdp;
	struct namebuf *np;
	struct hashbuf *phtp = htp;
	int root_zone = 0;

	ns_debug(ns_log_default, 1, "clean_cache_from(%s)", dname);
	if ((np = nlookup(dname, &phtp, &fname, 0)) && dname == fname &&
	    !ns_wildcard(NAME(*np))) {
		for (pdp = NULL, dp = np->n_data; dp != NULL; (void)NULL) {
			if (dp->d_zone == DB_Z_CACHE)
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
			(void)clean_cache(h, 1);
			if (h->h_cnt == 0 && !root_zone) {
				rm_hash(np->n_hash);
				np->n_hash = NULL;
			}
		}
		
		if (!root_zone && np->n_hash == NULL && np->n_data == NULL)
			(void) purge_node(htp, np);
	}
}

/* clean_cache(htp, all)
 *	Scan the entire cache looking for expired TTL's on nonauthoritative
 *	data, and remove it.  if `all' is true, ignore TTL and rm everything.
 * notes:
 *	this should be lazy and eventlib driven.
 * return:
 *	number of deleted RRs (all=1) or RRsets (all=0).
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
 again:
			for (pdp = NULL, dp = np->n_data; dp != NULL;
			     (void)NULL) {
				if (all && dp->d_zone == DB_Z_CACHE) {
					dp = rm_datum(dp, np, pdp, NULL);
					deleted++;
				} else if (dp->d_zone == DB_Z_CACHE && 
					   stale(dp)) {
					delete_all(np, dp->d_class, dp->d_type);
					deleted++;
					goto again;
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

/* struct namebuf *
 * purge_node(htp, np)
 *	Remove entry from cache.
 * Prerequisites:
 *	Node is empty and has no children.
 * Paramters:
 *	htp - root of recursive hash table this node is part of.
 *	np - the node to be deleted.
 * Return:
 *	pointer to parent.
 */
struct namebuf *
purge_node(struct hashbuf *htp, struct namebuf *np) {
	struct namebuf **npp, **nppend;
	struct namebuf *npn, *pnp, *nnp, *parent;
	struct hashbuf *phtp;

	ns_debug(ns_log_default, 3, "purge_node: cleaning cache");
	INSIST(np->n_hash == NULL && np->n_data == NULL);

	/* Walk parent hashtable looking for ourself. */
	parent = np->n_parent; 
	if (parent != NULL)
		phtp = parent->n_hash;
	else 
		phtp = htp;

	if (phtp == NULL) {
		/* XXX why shouldn't we panic? */
	} else {
		nppend = phtp->h_tab + phtp->h_size;
		for (npp = phtp->h_tab; npp < nppend; npp++) {
			for (pnp = NULL, nnp = *npp; nnp != NULL; nnp = npn) {
				if (nnp == np) {
					ns_debug(ns_log_default, 3,
						 "purge_node: found ourself");
					npn = rm_name(nnp, npp, pnp);
					phtp->h_cnt--;
				} else {
					npn = nnp->n_next;
					pnp = nnp;
				}
			}
		}
	}
	return (parent);
}

void
remove_zone(struct zoneinfo *zp, const char *verb) {
#ifdef BIND_UPDATE
	/*
	 * A dynamic zone might have changed, so we
	 * need to dump it before removing it.
	 */
	if ((zp->z_flags & Z_DYNAMIC) != 0 &&
	    ((zp->z_flags & Z_NEED_SOAUPDATE) != 0 ||
	     (zp->z_flags & Z_NEED_DUMP) != 0))
		(void) zonedump(zp, ISNOTIXFR);
#endif
	if ((zp->z_flags & Z_NOTIFY) != 0)
		ns_stopnotify(zp->z_origin, zp->z_class);
	if ((zp->z_flags & Z_NEED_XFER) != 0) {
		zp->z_flags &= ~Z_NEED_XFER;
		xfers_deferred--;
	}
	ns_stopxfrs(zp);
	if ((zp->z_flags & Z_XFER_RUNNING) != 0) {
		int i;
		/* Kill and abandon the current transfer. */
		for (i = 0; i < MAX_XFERS_RUNNING; i++) {
			if (xferstatus[i].xfer_pid == zp->z_xferpid) {
				xferstatus[i].xfer_pid = 0;
				xferstatus[i].xfer_state = XFER_IDLE;
				xfers_running--;
				break;
			}
		}
		(void)kill(zp->z_xferpid, SIGTERM);
		zp->z_flags &= ~(Z_XFER_RUNNING|Z_XFER_ABORTED|Z_XFER_GONE);
		zp->z_xferpid = 0;
		ns_need(main_need_tryxfer);
	}
	do_reload(zp, 1);
	ns_notice(ns_log_config, "%s zone \"%s\" (%s) %s",
		  zoneTypeString(zp->z_type), zp->z_origin,
		  p_class(zp->z_class), verb);
	free_zone_contents(zp, 1);
	memset(zp, 0, sizeof(*zp));
	zp->z_type = z_nil;  /* Pedantic; memset() did it. */
	INIT_LINK(zp, z_reloadlink);
	INIT_LINK(zp, z_freelink);
	free_zone(zp);
}

int
purge_nonglue(struct zoneinfo *zp, struct hashbuf *htp, int log) {
	const char *dname = zp->z_origin;
	const char *fname;
	struct namebuf *np;
	struct hashbuf *phtp = htp;
	int root_zone = 0;
	int errs = 0;
	int zone = zp - zones;
	struct databuf *pdp, *dp;
	int class = zp->z_class;

	ns_debug(ns_log_default, 1, "purge_nonglue(%s/%d)", dname, class);
	if ((np = nlookup(dname, &phtp, &fname, 0)) && dname == fname &&
	    !ns_wildcard(NAME(*np))) {

		for (pdp = NULL, dp = np->n_data; dp != NULL; (void)NULL) {
			if (dp->d_class == class && dp->d_zone != zone)
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
			errs += purge_nonglue_2(dname, h, class, 0, log, zone);
			if (h->h_cnt == 0 && !root_zone) {
				rm_hash(np->n_hash);
				np->n_hash = NULL;
			}
		}
	}
	return (errs);
}

static int
valid_glue(struct databuf *dp, char *name, int belowcut) {

	/* NS records are only valid glue at the zone cut */
	if (belowcut && dp->d_type == T_NS)
		return(0);

	if (ISVALIDGLUE(dp))	/* T_NS/T_A/T_AAAA/T_A6 */
		return (1);

	if (belowcut)
		return (0);

	/* Parent NXT record? */
	if (dp->d_type == T_NXT && !ns_samedomain((char*)dp->d_data, name) &&
	    ns_samedomain((char*)dp->d_data, zones[dp->d_zone].z_origin))
		return (1);

	/* KEY RRset may be in the parent */
	if (dp->d_type == T_KEY)
		return (1);

	/* NXT & KEY records may be signed */
	if (!belowcut && dp->d_type == T_SIG &&
	    (SIG_COVERS(dp) == T_NXT || SIG_COVERS(dp) == T_KEY))
		return (1);
	return (0);
}

static int
purge_nonglue_2(const char *dname, struct hashbuf *htp, int class,
	        int belowcut, int log, int zone)
{
	struct databuf *dp, *pdp;
	struct namebuf *np, *pnp, *npn;
	struct namebuf **npp, **nppend;
	int errs = 0;
	int zonecut;
	char name[MAXDNAME];

	nppend = htp->h_tab + htp->h_size;
	for (npp = htp->h_tab; npp < nppend; npp++) {
		for (pnp = NULL, np = *npp; np != NULL; np = npn) {
			if (!bottom_of_zone(np->n_data, class)) {
				zonecut = belowcut;
				for (dp = np->n_data; dp != NULL;
				     dp = dp->d_next) {
					if (match(dp, class, ns_t_ns)) {
						zonecut = 1;
						break;
					}
				}
				getname(np, name, sizeof name);
				for (pdp = NULL, dp = np->n_data;
				     dp != NULL;
				     (void)NULL) {
					int delete = 0;
					if (!zonecut &&
					    dp->d_class == class &&
					    dp->d_zone != zone)
						delete = 1;
					if (zonecut &&
					    dp->d_class == class &&
					    !valid_glue(dp, name, belowcut)) {
						if (log &&
						    dp->d_zone == zone) {
						    ns_error(ns_log_load,
	        "zone: %s/%s: non-glue record %s bottom of zone: %s/%s",
							 *dname ? dname : ".",
						         p_class(dp->d_class),
							 belowcut ? "below" :
								    "at",
							 *name ? name : ".",
							 p_type(dp->d_type));
							errs++;
						}
						delete = 1;
					}
					if (delete)
						dp = rm_datum(dp, np, pdp, 
							      NULL);
					else {
						pdp = dp;
						dp = dp->d_next;
					}
				}
				if (np->n_hash) {
					/*
					 * call recursively to clean
					 * subdomains
					 */
					errs += purge_nonglue_2(dname,
								np->n_hash,
								class,
								zonecut || 
								belowcut,
								log, zone);

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
	return (errs);
}

void
purge_zone(struct zoneinfo *zp, struct hashbuf *htp) {
	const char *fname;
	struct databuf *dp, *pdp;
	struct namebuf *np;
	struct hashbuf *phtp = htp;
	int root_zone = 0;
	int zone = zp - zones;
	char *dname = zp->z_origin;

	ns_debug(ns_log_default, 1, "purge_zone(%s)", dname);
	if ((np = nlookup(dname, &phtp, &fname, 0)) && dname == fname &&
	    !ns_wildcard(NAME(*np))) {
		for (pdp = NULL, dp = np->n_data; dp != NULL; (void)NULL) {
			if (dp->d_zone == zone)
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
			purge_z_2(h, zone);
			if (h->h_cnt == 0 && !root_zone) {
				rm_hash(np->n_hash);
				np->n_hash = NULL;
			}
		}
		
		if (!root_zone && np->n_hash == NULL && np->n_data == NULL)
			(void) purge_node(htp, np);
	}
}

static void
purge_z_2(struct hashbuf *htp, int zone) {
	struct databuf *dp, *pdp;
	struct namebuf *np, *pnp, *npn;
	struct namebuf **npp, **nppend;

	nppend = htp->h_tab + htp->h_size;
	for (npp = htp->h_tab; npp < nppend; npp++) {
		for (pnp = NULL, np = *npp; np != NULL; np = npn) {
			for (pdp = NULL, dp = np->n_data;
			     dp != NULL;
			     (void)NULL) {
				if (dp->d_zone == zone)
					dp = rm_datum(dp, np, pdp, 
						      NULL);
				else {
					pdp = dp;
					dp = dp->d_next;
				}
			}
			if (np->n_hash) {
				/* call recursively to rm subdomains */
				purge_z_2(np->n_hash, zone);

				/* if now empty, free it */
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
nxfers(struct zoneinfo *zp) {
	struct in_addr nsa;
	int ret;
	int i;

	if (zp->z_xaddrcnt != 0)
		nsa = zp->z_xaddr[0];	/* first ns holds zone's xfer limit */
	else if (zp->z_addrcnt != 0)
		nsa = zp->z_addr[0];	/* first ns holds zone's xfer limit */
	else
		return (-1);
	
	ret = 0;
	for (i = 0; i < MAX_XFERS_RUNNING; i++)
		if (xferstatus[i].xfer_status == XFER_RUNNING &&
		    xferstatus[i].xfer_addr.s_addr == nsa.s_addr)
			ret++;
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
 * Process exit of xfer's.
 */
void
reapchild(void) {
	int i;
	pid_t pid;
	WAIT_T status;

	gettime(&tt);
	while ((pid = (pid_t)waitpid(-1, &status, WNOHANG)) > 0) {
		for (i = 0; i < MAX_XFERS_RUNNING; i++) {
			if (xferstatus[i].xfer_pid == pid) {
				xferstatus[i].xfer_status = status;
				xferstatus[i].xfer_state = XFER_DONE;
				ns_need(main_need_endxfer);
				break;
			}
		}
	}
}

/*
 * Finish processing of of finished xfers
 */
void
endxfer() {
	struct zoneinfo *zp;   
	int exitstatus, i;
	pid_t pid;
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

				case XFER_SUCCESSAXFR:
				case XFER_SUCCESSAXFRIXFRFILE:
					zp->z_xferpid = XFER_ISAXFR;
					if (exitstatus == XFER_SUCCESSAXFRIXFRFILE) {
						zp->z_xferpid = XFER_ISAXFRIXFR;
						if (zp->z_ixfr_tmp != NULL)
							isc_movefile(
								zp->z_ixfr_tmp,
								zp->z_source);
					} 
					/* XXX should incorporate loadxfer() */
					zp->z_flags |= Z_NEED_RELOAD;
					zp->z_flags &= ~Z_SYSLOGGED;
					ns_need(main_need_zoneload);
					break;

				case XFER_SUCCESSIXFR:
					zp->z_xferpid = XFER_ISIXFR;
					ns_notice(ns_log_default,
						  "IXFR Success %s",
						  zp->z_ixfr_tmp);
					if (merge_logs(zp, zp->z_ixfr_tmp) >= 0) {
						ns_notice(ns_log_default,
							"IXFR Merge success %s",
							  zp->z_ixfr_tmp);
							
						(void)unlink(zp->z_updatelog);
						(void)unlink(zp->z_ixfr_base);
						isc_movefile(zp->z_ixfr_tmp,
							     zp->z_ixfr_base);
						(void)unlink(zp->z_ixfr_tmp);
						if (zonedump(zp, ISIXFR) < 0) 
							ns_warning(ns_log_db,
				"error in write ixfr updates to zone file %s",
								zp ->z_source); 
						ns_refreshtime(zp, tt.tv_sec);
						sched_zone_maint(zp);
					} else {
						ns_notice(ns_log_default,
							"IXFR Merge failed %s",
							  zp->z_ixfr_tmp);
						ns_retrytime(zp, tt.tv_sec);
						sched_zone_maint(zp);
					}
					break;

				case XFER_TIMEOUT:
					if (!(zp->z_flags & Z_SYSLOGGED)) {
						zp->z_flags |= Z_SYSLOGGED;
						ns_notice(ns_log_default,
		      "zoneref: Masters for slave zone \"%s\" unreachable",
							  zp->z_origin);
					}
					ns_retrytime(zp, tt.tv_sec);
					sched_zone_maint(zp);
					break;

				case XFER_REFUSED:
					if (!(zp->z_flags & Z_SYSLOGGED)) {
						zp->z_flags |= Z_SYSLOGGED;
						ns_error(ns_log_xfer_in,
		      "zoneref: Masters for slave zone \"%s\" REFUSED transfer",
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
void
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
		
		if ((xfers = nxfers(zp)) != -1 &&
		    xfers < server_options->transfers_per_ns &&
		    (zp->z_flags & Z_NEED_XFER)) {
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
loadxfer(void) {
	struct zoneinfo *zp;   
	u_int32_t old_serial,new_serial;
	char *tmpnom;
	int isixfr;

	gettime(&tt);
	for (zp = zones; zp < &zones[nzones]; zp++) {
		if (zp->z_flags & Z_NEED_RELOAD) {
			ns_debug(ns_log_default, 1, "loadxfer() \"%s\"",
				 zp->z_origin[0] ? zp->z_origin : ".");
			zp->z_flags &= ~(Z_NEED_RELOAD|Z_AUTH);
/* XXX this is bad, should be done in ns_zreload() for primary changes. */
			ns_stopxfrs(zp);
			old_serial = zp->z_serial;
			if (zp->z_xferpid == XFER_ISIXFR) {
				tmpnom = zp->z_ixfr_tmp;
				isixfr = ISIXFR;
			} else {
				tmpnom = zp->z_source;
				purge_zone(zp, hashtab);
				isixfr = ISNOTIXFR;
			}
			if (zp->z_xferpid == XFER_ISAXFRIXFR) {
				tmpnom= zp->z_source;
				purge_zone(zp, hashtab);
				isixfr = ISNOTIXFR;
			}

			if (!db_load(tmpnom, zp->z_origin, zp, NULL, isixfr)) {
				zp->z_flags |= Z_AUTH;
				zp->z_flags &= ~Z_EXPIRED;
				if (isixfr == ISIXFR) {
					new_serial= zp ->z_serial;
						ns_warning(ns_log_db, "ISIXFR");
						ns_warning(ns_log_db, "error in updating ixfr data base file %s from %s", zp -> z_ixfr_base, zp ->z_ixfr_tmp);
					if (zonedump(zp,ISIXFR)<0) 
						ns_warning(ns_log_db, "error in write ixfr updates to zone file %s", zp ->z_source); 
			
				}
			}
			zp->z_xferpid = 0;
			if (zp->z_flags & Z_TMP_FILE)
				(void) unlink(zp->z_source);
			sched_zone_maint(zp);
		}
	}
}

/*
 * Add this zone to the set of those needing transfers.
 */
void
addxfer(struct zoneinfo *zp) {
	if (!(zp->z_flags & Z_NEED_XFER)) {
		zp->z_flags |= Z_NEED_XFER;
		xfers_deferred++;
		tryxfer();
	}
}

/*
 * Mark one zone as requiring a reload.
 * Note that it should be called with signals blocked,
 * and should not allocate memory (since it can be called from a sighandler).
 */
const char *
deferred_reload_unsafe(struct zoneinfo *zp) {
	INSIST(zp->z_type != z_nil);
	if (!zonefile_changed_p(zp))
		return ("Zone file has not changed.");
	if (LINKED(zp, z_reloadlink))
		return ("Zone is already scheduled for reloading.");
	APPEND(reloadingzones, zp, z_reloadlink);
	ns_need_unsafe(main_need_zreload);
	return ("Zone is now scheduled for reloading.");
}

/*
 * If we've loaded this file, and the file has not been modified and contains
 * no $INCLUDE, then there's no need to reload.
 */
int
zonefile_changed_p(struct zoneinfo *zp) {
	struct stat sb;

	INSIST(zp->z_type != z_nil);
	return ((zp->z_flags & Z_INCLUDE) != 0 ||
		stat(zp->z_source, &sb) == -1 ||
		zp->z_ftime != sb.st_mtime);
}

int
reload_master(struct zoneinfo *zp) {
	INSIST(zp->z_type == z_master);
	zp->z_flags &= ~Z_AUTH;
	ns_stopxfrs(zp);
	/* XXX what about parent zones? */
#ifdef BIND_UPDATE
	/*
	 * A dynamic zone might have changed, so we
	 * need to dump it before reloading it.
	 */
	if ((zp->z_flags & Z_DYNAMIC) != 0 &&
	    ((zp->z_flags & Z_NEED_SOAUPDATE) != 0 ||
	     (zp->z_flags & Z_NEED_DUMP) != 0))
		(void) zonedump(zp, ISNOTIXFR);
#endif
	purge_zone(zp, hashtab);
	ns_debug(ns_log_config, 1, "reloading zone");
#ifdef BIND_UPDATE
	if ((zp->z_flags & Z_DYNAMIC) != 0) {
		struct stat sb;

		if (stat(zp->z_source, &sb) < 0)
			ns_error(ns_log_config, "stat(%s) failed: %s",
				 zp->z_source, strerror(errno));
		else {
			if ((sb.st_mode & (S_IWUSR|S_IWGRP|S_IWOTH)) != 0)
				ns_warning(ns_log_config,
					 "dynamic zone file '%s' is writable",
					   zp->z_source);
		}
	}
#endif
	if (!db_load(zp->z_source, zp->z_origin, zp, NULL, ISNOTIXFR))
		zp->z_flags |= Z_AUTH;
	zp->z_refresh = 0;	/* no maintenance needed */
	zp->z_time = 0;
#ifdef BIND_UPDATE
	zp->z_lastupdate = 0;
	if ((zp->z_flags & Z_DYNAMIC) != 0)
		if (merge_logs(zp, zp->z_updatelog) == 1)
			return (1);
#endif
	return (0);
}

/*
 * Called by main() when main_need_zreload has been set.  Should pull one
 * zone off of the reloadingzones list and reload it, then if the list is
 * not then empty, should turn main_need_zreload on again for the next call.
 * It is not an error to call this when the reloadingzones list is empty.
 */
void
ns_zreload(void) {
	struct zoneinfo *zp;

	block_signals();
	if (EMPTY(reloadingzones)) {
		unblock_signals();
		return;
	}
	zp = HEAD(reloadingzones);
	UNLINK(reloadingzones, zp, z_reloadlink);
	unblock_signals();

	reload_master(zp);

	block_signals();
	if (!EMPTY(reloadingzones))
		ns_need_unsafe(main_need_zreload);
	unblock_signals();
}

/*
 * Flush and reload configuration file and data base.
 */
void
ns_reload(void) {
	ns_notice(ns_log_default, "%s %snameserver",
		  (reconfiging != 0) ? "reconfiguring" : "reloading",
		  (noexpired == 1) ? "(-noexpired) " : "");

	INSIST(reloading == 0);
	qflush();
	sq_flush(NULL);
	reloading++;	/* To force transfer if slave and backing up. */
	confmtime = ns_init(conffile);
	time(&resettime);
	reloading--;
	ns_notice(ns_log_default, "Ready to answer queries.");
}

/*
 * Reload configuration, look for new or deleted zones, not changed ones
 * also ignore expired zones.
 */
void
ns_noexpired(void) {
	INSIST(noexpired == 0);
	noexpired++;	/* To ignore zones which are expired */
	ns_reconfig();
	noexpired--;
}

/*
 * Reload configuration, look for new or deleted zones, not changed ones.
 */
void
ns_reconfig(void) {
	INSIST(reconfiging == 0);
	reconfiging++;	/* To ignore zones which aren't new or deleted. */
	ns_reload();
	reconfiging--;
}

void
make_new_zones(void) {
	struct zoneinfo *zp;
	int n;
	int newzones = (nzones == 0) ? INITIALZONES : NEWZONES;

	ns_debug(ns_log_config, 1, "Adding %d template zones", NEWZONES);
	zp = (struct zoneinfo *)
		memget((nzones + newzones) * sizeof(struct zoneinfo));
	if (zp == NULL)
		panic("no memory for more zones", NULL);
	memset(zp, 0, (nzones + newzones) * sizeof(struct zoneinfo));
	if (zones != NULL) {
		memcpy(zp, zones, nzones * sizeof(struct zoneinfo));
		memput(zones, nzones * sizeof(struct zoneinfo));
	}
	zones = zp;
	block_signals();
	for (n = 0; n < newzones; n++) {
		INIT_LINK(&zones[nzones], z_reloadlink);
		INIT_LINK(&zones[nzones], z_freelink);
		if (nzones != 0)
			free_zone(&zones[nzones]);
		nzones++;
	}
	unblock_signals();
}

void
free_zone(struct zoneinfo *zp) {
	if (LINKED(zp, z_reloadlink))
		panic("freeing reloading zone", NULL);
	if (zp->z_type != z_nil)
		panic("freeing unfree zone", NULL);
	APPEND(freezones, zp, z_freelink);
}

#ifndef HAVE_SPAWNXFER
static pid_t
spawnxfer(char **argv, struct zoneinfo *zp) {
	pid_t pid = (pid_t)vfork();

	if (pid == -1) {
		ns_error(ns_log_default, "xfer vfork: %s", strerror(errno));
		zp->z_time = tt.tv_sec + 10;
		return (pid);
	}
	if (pid == 0) {
		/* Child. */
		execv(server_options->named_xfer, argv);
		ns_error(ns_log_default, "can't exec %s: %s",
		server_options->named_xfer, strerror(errno));
		_exit(XFER_FAIL);	/* Avoid duplicate buffer flushes. */
	}
	return (pid);
}
#endif

struct zoneinfo *
find_auth_zone(const char *zname, ns_class zclass) {
	struct zoneinfo *zp;
	struct hashbuf *htp;
	struct namebuf *np;
	const char *fname;
	int zn;

	zp = find_zone(zname, zclass);
	if (zp != NULL &&
	    (zp->z_type == z_slave ||
	     zp->z_type == z_master ||
	     zp->z_type == z_stub))
		return (zp);

	htp = hashtab;
	np = nlookup(zname, &htp, &fname, 0);
	if (np != NULL && (zn = findMyZone(np, zclass)) != DB_Z_CACHE)
		return (&zones[zn]);

	return (NULL);
}
