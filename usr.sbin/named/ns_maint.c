#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)ns_maint.c	4.39 (Berkeley) 3/2/91";
static char rcsid[] = "$Id: ns_maint.c,v 1.3 1995/08/20 21:18:49 peter Exp $";
#endif /* not lint */

/*
 * ++Copyright++ 1986, 1988
 * -
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
 * -
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
 * -
 * --Copyright--
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <sys/wait.h>
#include <stdio.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>

#include "named.h"

#ifdef USE_UTIME
# include <utime.h>
#endif

static int		xfers_running,	/* # of xfers running */
			xfers_deferred,	/* # of needed xfers not run yet */
			qserials_running,
			alarm_pending,	/* flag */
			nxfers __P((struct zoneinfo *, int));

static void		startxfer __P((struct zoneinfo *)),
			abortxfer __P((struct zoneinfo *)),
			addxfer __P((struct zoneinfo *)),
			tryxfer __P((void));

#define	qserial_qfull()	(qserials_running == MAXQSERIAL)

#ifdef CLEANCACHE
static time_t cache_time;
#endif
#ifdef XSTATS
static time_t stats_time;
#endif
/*
 * Invoked at regular intervals by signal interrupt; refresh all secondary
 * zones from primary name server and remove old cache entries.  Also,
 * ifdef'd ALLOW_UPDATES, dump database if it has changed since last
 * dump/bootup.
 */
void
ns_maint()
{
	register struct zoneinfo *zp;
	int zonenum;

	gettime(&tt);

	dprintf(1, (ddt, "\nns_maint(); now %s", ctimel(tt.tv_sec)));

	alarm_pending = 0;
	for (zp = zones, zonenum = 0; zp < &zones[nzones]; zp++, zonenum++) {
#ifdef DEBUG
		if (debug >= 2)
			printzoneinfo(zonenum);
#endif
		if (tt.tv_sec >= zp->z_time && zp->z_refresh > 0) {
			switch (zp->z_type) {

			case Z_CACHE:
				doachkpt();
				ns_refreshtime(zp, tt.tv_sec);
				break;

			case Z_SECONDARY:
#ifdef STUBS
			case Z_STUB:
#endif
				if (zp->z_serial != 0 &&
				    ((zp->z_lastupdate + zp->z_expire) <
				     tt.tv_sec)
				    ) {
					zp->z_serial = 0;
				}
				if (zp->z_flags &
				    (Z_NEED_RELOAD|Z_NEED_XFER|Z_QSERIAL)) {
					ns_refreshtime(zp, tt.tv_sec);
					break;
				}
				if (zp->z_flags & Z_XFER_RUNNING) {
					abortxfer(zp);
					ns_retrytime(zp, tt.tv_sec);
					break;
				}
				qserial_query(zp);
				break;
#ifdef ALLOW_UPDATES
			case Z_PRIMARY:
				/*
				 * Checkpoint the zone if it has changed
				 * since we last checkpointed
				 */
				if (zp->z_flags & Z_CHANGED) {
					zonedump(zp);
					ns_refreshtime(zp, tt.tv_sec);
				}
				break;
#endif /* ALLOW_UPDATES */
			}
			gettime(&tt);
		}
	}
#ifdef CLEANCACHE
	if ((cache_time + cache_interval) <= tt.tv_sec) {
		if (cache_time)
			remove_zone(hashtab, 0, 0);
		cache_time = tt.tv_sec;
	}
#endif
#ifdef XSTATS
	if (stats_time + stats_interval <= tt.tv_sec) {
		if (stats_time)
			ns_logstats();
		stats_time = tt.tv_sec;
	}
#endif
	if (!needmaint)
		sched_maint();
	dprintf(1, (ddt, "exit ns_maint()\n"));
}

/*
 * Find when the next refresh needs to be and set
 * interrupt time accordingly.
 */
void
sched_maint()
{
	register struct zoneinfo *zp;
	struct itimerval ival;
#ifdef	CLEANCACHE
	time_t next_refresh = cache_time + cache_interval;
#else
	time_t next_refresh = 0;
#endif
	static time_t next_alarm;

	for (zp = zones; zp < &zones[nzones]; zp++)
		if (zp->z_time != 0 &&
		    (next_refresh == 0 || next_refresh > zp->z_time))
			next_refresh = zp->z_time;
        /*
	 *  Schedule the next call to ns_maint.
	 *  Don't visit any sooner than maint_interval.
	 */
	bzero((char *)&ival, sizeof (ival));
	if (next_refresh != 0) {
		if (next_refresh == next_alarm && alarm_pending) {
			dprintf(1, (ddt, "sched_maint: no schedule change\n"));
			return;
		}
		/*
		 *  tv_sec can be an unsigned long, so we can't let
		 *  it go negative.
		 */
		if (next_refresh < tt.tv_sec)
			next_refresh = tt.tv_sec;
		ival.it_value.tv_sec = next_refresh - tt.tv_sec;
		if ((long) ival.it_value.tv_sec < maint_interval)
			ival.it_value.tv_sec = maint_interval;
		next_alarm = next_refresh;
		alarm_pending = 1;
	}
	(void) setitimer(ITIMER_REAL, &ival, (struct itimerval *)NULL);
	dprintf(1, (ddt, "sched_maint: Next interrupt in %lu sec\n",
		    (u_long)ival.it_value.tv_sec));
}

/*
 * Mark a zone "up to date" after named-xfer tells us this or we
 * discover it through the qserial_*() logic.
 */
static void
markUpToDate(zp)
	struct zoneinfo *zp;
{
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
#if defined(USE_UTIME)
		struct utimbuf t;

		t.actime = tt.tv_sec;
		t.modtime = tt.tv_sec;
		(void) utime(zp->z_source, &t);
#else
		struct timeval t[2];

		t[0] = tt;
		t[1] = tt;
		(void) utimes(zp->z_source, t);
#endif /* USE_UTIME */
	}
	/* we use "stat" to set zp->z_ftime instead of just
	   setting it to tt.tv_sec in order to avoid any
	   possible rounding problems in utimes(). */
	if (stat(zp->z_source, &f_time) != -1)
	    zp->z_ftime = f_time.st_mtime;
	/* XXX log if stat fails? XXX */
}

/*
 * Query for the serial number of a zone, so that
 * we can check to see if we need to transfer it.
 */
void
qserial_query(zp)
	struct zoneinfo *zp;
{
	struct qinfo *qp;

	dprintf(1, (ddt, "qserial_query(%s)\n", zp->z_origin));

	if (qserial_qfull())
		return;

	qp = sysquery(zp->z_origin, zp->z_class, T_SOA,
		      zp->z_addr, zp->z_addrcnt, QUERY);
	if (!qp) {
		syslog(LOG_INFO, "qserial_query(%s): sysquery FAILED",
		       zp->z_origin);
		return;		/* XXX - this is bad, we should do something */
	}
	qp->q_flags |= Q_ZSERIAL;
	qp->q_zquery = zp;
	zp->z_flags |= Z_QSERIAL;
	ns_refreshtime(zp, tt.tv_sec);
	qserials_running++;
	dprintf(1, (ddt, "qserial_query(%s) QUEUED\n", zp->z_origin));
}

void
qserial_answer(qp, serial)
	struct qinfo *qp;
	u_int32_t serial;
{
	struct zoneinfo *zp = qp->q_zquery;
	int was_qfull = qserial_qfull();

	dprintf(1, (ddt, "qserial_answer(%s, %lu)\n",
		    zp->z_origin, (u_long)serial));
	zp->z_flags &= ~Z_QSERIAL;
	qp->q_flags &= ~Q_ZSERIAL;	/* keeps us from being called twice */
	qserials_running--;
	if (serial == 0) {
		/* an error occurred, or the query timed out.
		 */
#ifdef GETSER_LOGGING
		syslog(GETSER_LOGGING, "Err/TO getting serial# for \"%s\"",
		       zp->z_origin);
#endif /* GETSER_LOGGING */
		addxfer(zp);
	} else if (SEQ_GT(serial, zp->z_serial) || !zp->z_serial) {
		dprintf(1, (ddt, "qserial_answer: zone is out of date\n"));
		zp->z_xaddr = from_addr.sin_addr; /* don't use qp->q_from */
		addxfer(zp);
	} else if (SEQ_GT(zp->z_serial, serial)) {
		if (!haveComplained((char*)zp, "went backward")) {
			syslog(LOG_NOTICE,
   "Zone \"%s\" (class %d) SOA serial# (%lu) rcvd from [%s] is < ours (%lu)\n",
			       zp->z_origin, zp->z_class, serial,
			       inet_ntoa(from_addr.sin_addr),
			       zp->z_serial);
		}
	} else {
		dprintf(1, (ddt, "qserial_answer: zone serial is still OK\n"));
		markUpToDate(zp);
	}
	if (was_qfull)
		needmaint = 1;
}

/*
 * Start an asynchronous zone transfer for a zone.
 * Depends on current time being in tt.
 * The caller must call sched_maint after startxfer.
 */
static void
startxfer(zp)
	struct zoneinfo *zp;
{
	static char *argv[NSMAX + 20], argv_ns[NSMAX][MAXDNAME];
	int argc = 0, argc_ns = 0, pid, omask;
	unsigned int cnt;
	char debug_str[10];
	char serial_str[10];
	char port_str[10];
#ifdef GEN_AXFR
	char class_str[10];
#endif
#ifdef POSIX_SIGNALS
      sigset_t sset;
#endif

	dprintf(1, (ddt, "startxfer() %s\n", zp->z_origin));

	argv[argc++] = "named-xfer";
	argv[argc++] = "-z";
	argv[argc++] = zp->z_origin;
	argv[argc++] = "-f";
	argv[argc++] = zp->z_source;
	argv[argc++] = "-s";
	sprintf(serial_str, "%lu", (u_long)zp->z_serial);
	argv[argc++] = serial_str;
#ifdef GEN_AXFR
	argv[argc++] = "-C";
	sprintf(class_str, "%d", zp->z_class);
	argv[argc++] = class_str;
#endif
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

	if (zp->z_xaddr.s_addr != 0) {
		/* address was specified by the qserial logic, use it */
		argv[argc++] = strcpy(argv_ns[argc_ns++],
				      inet_ntoa(zp->z_xaddr));
	} else {
		/*
		 * Copy the server ip addresses into argv, after converting
		 * to ascii and saving the static inet_ntoa result
		 */
		for (cnt = 0;  cnt < zp->z_addrcnt;  cnt++) {
			struct in_addr a;

			a = zp->z_addr[cnt];
			if (aIsUs(a)
			    && !haveComplained(zp->z_origin,
					       (char*)startxfer)) {
				syslog(LOG_NOTICE,
				   "attempted to fetch zone %s from self (%s)",
				       zp->z_origin, inet_ntoa(a));
				continue;
			}
			argv[argc++] = strcpy(argv_ns[argc_ns++],
					      inet_ntoa(a));
		}
        }

	argv[argc] = 0;

#ifdef DEBUG
#ifdef ECHOARGS
	if (debug) {
		int i;
		for (i = 0; i < argc; i++)
			fprintf(ddt, "Arg %d=%s\n", i, argv[i]);
        }
#endif /* ECHOARGS */
#endif /* DEBUG */

	gettime(&tt);
#ifndef SYSV
#if defined(POSIX_SIGNALS)
      sigemptyset(&sset);
      sigaddset(&sset,SIGCHLD);
      sigprocmask(SIG_BLOCK,&sset,NULL);
#else
	omask = sigblock(sigmask(SIGCHLD));
#endif
#endif
	if ((pid = vfork()) == -1) {
		syslog(LOG_ERR, "xfer vfork: %m");
#ifndef SYSV
#if defined(POSIX_SIGNALS)
              sigprocmask(SIG_UNBLOCK,&sset,NULL);
#else
		(void) sigsetmask(omask);
#endif
#endif
		zp->z_time = tt.tv_sec + 10;
		return;
	}

	if (pid == 0) {
		/* child */
		execv(_PATH_XFER, argv);
		syslog(LOG_ERR, "can't exec %s: %m", _PATH_XFER);
		_exit(XFER_FAIL);	/* avoid duplicate buffer flushes */
	}
	/* parent */
	dprintf(1, (ddt, "started xfer child %d\n", pid));
	zp->z_flags &= ~Z_NEED_XFER;
	zp->z_flags |= Z_XFER_RUNNING;
	zp->z_xferpid = pid;
	xfers_running++;
	zp->z_time = tt.tv_sec + MAX_XFER_TIME;
#ifndef SYSV
#if defined(POSIX_SIGNALS)
      sigprocmask(SIG_UNBLOCK,&sset,NULL);
#else
	(void) sigsetmask(omask);
#endif
#endif
}

const char *
zoneTypeString(zp)
	const struct zoneinfo *zp;
{
	static char ret[sizeof "(4294967296?)"];	/* 2^32 */

	switch (zp->z_type) {
	case Z_PRIMARY:		return ("primary");
	case Z_SECONDARY:	return ("secondary");
#ifdef STUBS
	case Z_STUB:		return ("stub");
#endif
	case Z_CACHE:		return ("cache");
	default:
		sprintf(ret, "(%lu?)", (u_long)zp->z_type);
		return (ret);
	}
}

#ifdef DEBUG
void
printzoneinfo(zonenum)
int zonenum;
{
	struct timeval  tt;
	struct zoneinfo *zp = &zones[zonenum];

	if (!debug)
		return;

	fprintf(ddt, "printzoneinfo(%d):\n", zonenum);

	gettime(&tt);
	if (zp->z_origin != NULL && (zp->z_origin[0] == '\0'))
		fprintf(ddt, "origin ='.'");
	else
		fprintf(ddt, "origin ='%s'", zp->z_origin);
#ifdef GEN_AXFR
	fprintf(ddt, ", class = %d", zp->z_class);
#endif
 	fprintf(ddt, ", type = %s", zoneTypeString(zp));
	if (zp->z_source)
		fprintf(ddt,", source = %s\n", zp->z_source);
	fprintf(ddt, "z_refresh = %lu", (u_long)zp->z_refresh);
	fprintf(ddt, ", retry = %lu", (u_long)zp->z_retry);
	fprintf(ddt, ", expire = %lu", (u_long)zp->z_expire);
	fprintf(ddt, ", minimum = %lu", (u_long)zp->z_minimum);
	fprintf(ddt, ", serial = %lu\n", (u_long)zp->z_serial);
	fprintf(ddt, "z_time = %lu", (u_long)zp->z_time);
	if (zp->z_time) {
		fprintf(ddt, ", now time : %lu sec", (u_long)tt.tv_sec);
		fprintf(ddt, ", time left: %lu sec",
			(int)(zp->z_time - tt.tv_sec));
	}
	fprintf(ddt, "; flags %lx\n", (u_long)zp->z_flags);
}
#endif /* DEBUG */

/*
 * remove_zone (htp, zone) --
 *     Delete all RR's in the zone "zone" under specified hash table.
 */
void
#ifdef CLEANCACHE
remove_zone(htp, zone, all)
#else
remove_zone(htp, zone)
#endif
	register struct hashbuf *htp;
	register int zone;
#ifdef	CLEANCACHE
	register int all;
#endif
{
	register struct databuf *dp, *pdp;
	register struct namebuf *np, *pnp, *npn;
	struct namebuf **npp, **nppend;

	nppend = htp->h_tab + htp->h_size;
	for (npp = htp->h_tab; npp < nppend; npp++)
	    for (pnp = NULL, np = *npp; np != NULL; np = npn) {
		for (pdp = NULL, dp = np->n_data; dp != NULL; ) {
#ifdef	CLEANCACHE
			if (dp->d_zone == zone && (all || stale(dp)))
#else
			if (dp->d_zone == zone)
#endif
				dp = rm_datum(dp, np, pdp);
			else {
				pdp = dp;
				dp = dp->d_next;
			}
		}

		if (np->n_hash) {
			/* call recursively to remove subdomains. */
#ifdef CLEANCACHE
			remove_zone(np->n_hash, zone, all);
#else
			remove_zone(np->n_hash, zone);
#endif

			/* if now empty, free it */
			if (np->n_hash->h_cnt == 0) {
				free((char*)np->n_hash);
				np->n_hash = NULL;
			}
		}

		if ((np->n_hash == NULL) && (np->n_data == NULL)) {
			npn = rm_name(np, npp, pnp);
			htp->h_cnt--;
		} else {
			npn = np->n_next;
			pnp = np;
		}
	    }
}

#ifdef PURGE_ZONE
static void purge_z_2();
static bottom_of_zone();

void
purge_zone(dname, htp, class)
	const char *dname;
	register struct hashbuf *htp;
	int class;
{
	const char *fname;
	struct databuf *dp, *pdp;
	struct namebuf *np;
	struct hashbuf *phtp = htp;

	dprintf(1, (ddt, "purge_zone(%s,%d)\n", dname, class));
	if ((np = nlookup(dname, &phtp, &fname, 0)) && dname == fname) {
		for (pdp = NULL, dp = np->n_data; dp != NULL; ) {
			if (dp->d_class == class)
			    dp = rm_datum(dp, np, pdp);
			else {
				pdp = dp;
				dp = dp->d_next;
			}
		}

		if (np->n_hash) {

		    purge_z_2(np->n_hash, class);

		    if (np->n_hash->h_cnt == 0) {
			    free((char*)np->n_hash);
			    np->n_hash = NULL;
		    }
		}

		/* remove entry from cache, if required */
		if ((np->n_hash == NULL) && (np->n_data == NULL)) {
		    struct namebuf **npp, **nppend;
		    struct namebuf *npn, *pnp, *nnp;

		    dprintf(3,(ddt, "purge_zone: cleaning cache\n"));

		    /* walk parent hashtable looking for ourself */
		    if (np->n_parent)
			    phtp = np->n_parent->n_hash;
		    else
			    phtp = htp; /* top / root zone */

		    if (phtp) {
			nppend = phtp->h_tab + phtp->h_size;

			for (npp = phtp->h_tab; npp < nppend; npp++) {
			    for (pnp = NULL, nnp = *npp;
				 nnp != NULL;
				 nnp = npn
				 ) {
				if (nnp == np) {
			    dprintf(3,(ddt, "purge_zone: found our selves\n"));
				    npn = rm_name(nnp, npp, pnp);
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
	register struct hashbuf *htp;
	register int class;
{
	register struct databuf *dp, *pdp;
	register struct namebuf *np, *pnp, *npn;
	struct namebuf **npp, **nppend;

	nppend = htp->h_tab + htp->h_size;
	for (npp = htp->h_tab; npp < nppend; npp++)
	    for (pnp = NULL, np = *npp; np != NULL; np = npn) {
		if (!bottom_of_zone(np->n_data, class)) {
		    for (pdp = NULL, dp = np->n_data; dp != NULL; ) {
			if (dp->d_class == class)
				dp = rm_datum(dp, np, pdp);
			else {
				pdp = dp;
				dp = dp->d_next;
			}
		    }

		    if (np->n_hash) {
			/* call recursively to remove subdomains. */
			purge_z_2(np->n_hash, class);

			/* if now empty, free it */
			if (np->n_hash->h_cnt == 0) {
				free((char*)np->n_hash);
				np->n_hash = NULL;
			}
		    }

		}

		if ((np->n_hash == NULL) && (np->n_data == NULL)) {
		    npn = rm_name(np, npp, pnp);
		    htp->h_cnt--;
		} else {
		    npn = np->n_next;
		    pnp = np;
		}
	    }
}

static int
bottom_of_zone(dp, class)
	struct databuf *dp;
	int class;
{
	for ( ; dp ; dp = dp->d_next) {
		if (dp->d_class != class)
			continue;
		if (dp->d_zone == 0)
			continue;
#ifdef NCACHE
		if (dp->d_rcode)	/* this should not occur */
			continue;
#endif
		if (dp->d_type == T_SOA)
			return (1);
	}
	dprintf(3, (ddt, "bottom_of_zone() == 0\n"));
	return (0);
}
#endif

/*
 * Handle XFER limit for a nameserver.
 */
static int
nxfers(zp, delta)
	struct zoneinfo *zp;
	int delta;
{
	struct in_addr nsa;
	struct nameser *nsp;
	int ret;

	if (zp->z_xaddr.s_addr)
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
abortxfer(zp)
	struct zoneinfo *zp;
{
	kill(zp->z_xferpid, SIGKILL);
	syslog(LOG_NOTICE, "zone transfer timeout for \"%s\"; pid %lu killed",
	       zp->z_origin, (u_long)zp->z_xferpid);
	ns_retrytime(zp, tt.tv_sec);
	(void) nxfers(zp, -1);
	xfers_running--;
}

/*
 * SIGCHLD signal handler: process exit of xfer's.
 * (Note: also called when outgoing transfer completes.)
 */
SIG_FN
endxfer()
{
    	register struct zoneinfo *zp;
	int exitstatus, pid, xfers, save_errno;
#if defined(sequent)
	union wait status;
#else
	int status;
#endif /* sequent */

	save_errno = errno;
	xfers = 0;
	gettime(&tt);
#if defined(USE_WAITPID)
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
#else /* USE_WAITPID */
	{
		pid = wait(&status);
#endif /* USE_WAITPID */
		exitstatus = WIFEXITED(status) ?WEXITSTATUS(status) :0;

		for (zp = zones; zp < &zones[nzones]; zp++) {
			if (zp->z_xferpid != pid)
				continue;
			xfers++;
			xfers_running--;
			(void) nxfers(zp, -1);
			zp->z_xferpid = 0;
			zp->z_flags &= ~Z_XFER_RUNNING;
			dprintf(1, (ddt,
		"\nendxfer: child %d zone %s returned status=%d termsig=%d\n",
				    pid, zp->z_origin, exitstatus,
				    WIFSIGNALED(status) ?WTERMSIG(status) :-1
				    )
				);
			if (WIFSIGNALED(status)) {
				if (WTERMSIG(status) != SIGKILL) {
					syslog(LOG_NOTICE,
					  "named-xfer exited with signal %d\n",
					  WTERMSIG(status));
				}
				ns_retrytime(zp, tt.tv_sec);
			} else {
				switch (exitstatus) {
				case XFER_UPTODATE:
					markUpToDate(zp);
					break;

				case XFER_SUCCESS:
					zp->z_flags |= Z_NEED_RELOAD;
					zp->z_flags &= ~Z_SYSLOGGED;
					needzoneload++;
					break;

				case XFER_TIMEOUT:
					if (!(zp->z_flags & Z_SYSLOGGED)) {
						zp->z_flags |= Z_SYSLOGGED;
						syslog(LOG_NOTICE,
		      "zoneref: Masters for secondary zone \"%s\" unreachable",
						    zp->z_origin);
					}
					ns_retrytime(zp, tt.tv_sec);
					break;

				default:
					if (!(zp->z_flags & Z_SYSLOGGED)) {
						zp->z_flags |= Z_SYSLOGGED;
						syslog(LOG_NOTICE,
					     "named-xfer for \"%s\" exited %d",
						    zp->z_origin, exitstatus);
					}
					/* FALLTHROUGH */
				case XFER_FAIL:
					zp->z_flags |= Z_SYSLOGGED;
					ns_retrytime(zp, tt.tv_sec);
					break;
				} /*switch*/
				break;
			} /*if/else*/
		} /*for*/
	} /*while*/
	tryxfer();
#if defined(SYSV)
	(void)signal(SIGCLD, (SIG_FN (*)()) endxfer);
#endif
	errno = save_errno;
}

/*
 * Try to start some xfers - new "fair scheduler" by Bob Heiney @DEC (1995)
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
			syslog(LOG_INFO, "zones changed: %p != %p",
			       lastzones, zones);
		lastzones = zones;
		zp = zones;
	}

	/* did zones shrink? */
	if (lastnzones > nzones) {
		syslog(LOG_INFO, "zones shrunk");
		zp = zones;
	}
	lastnzones = nzones;

	if (zp == zones)
		stopzp = &zones[nzones-1];
	else
		stopzp = zp - 1;

	dprintf(3, (ddt, "tryxfer start zp=%p stopzp=%p def=%d running=%d\n",
		    zp, stopzp, xfers_deferred, xfers_running));

	startzp = zp;
	for (;;) {
		int xfers;

		if (!xfers_deferred || xfers_running >= max_xfers_running)
			break;

		if ((xfers = nxfers(zp, 0)) != -1 &&
		    xfers < max_xfers_per_ns &&
		    (zp->z_flags & Z_NEED_XFER)) {
			nxfers(zp, 1);
			xfers_deferred--;
			startxfer(zp);
		}

		if (zp == stopzp) {
			dprintf(3, (ddt, "tryxfer stop mark\n"));
			zp = startzp;
			break;
		}

		zp++;
		/* wrap around? */
		if (zp == &zones[nzones])
			zp = zones;
	}
	dprintf(3, (ddt, "tryxfer stop zp=%p\n", zp));

	if (!needmaint)
		sched_maint();
}

/*
 * Reload zones whose transfers have completed.
 */
void
loadxfer()
{
    	register struct zoneinfo *zp;

	gettime(&tt);
	for (zp = zones; zp < &zones[nzones]; zp++) {
		if (zp->z_flags & Z_NEED_RELOAD) {
			dprintf(1, (ddt, "loadxfer() \"%s\"\n",
				    zp->z_origin[0] ? zp->z_origin : "."));
			zp->z_flags &= ~(Z_NEED_RELOAD|Z_AUTH);
#ifdef CLEANCACHE
			remove_zone(hashtab, zp - zones, 1);
#else
			remove_zone(hashtab, zp - zones);
#endif
#ifdef PURGE_ZONE
			purge_zone(zp->z_origin, hashtab, zp->z_class);
#endif
			if (!db_load(zp->z_source, zp->z_origin, zp, NULL))
				zp->z_flags |= Z_AUTH;
			if (zp->z_flags & Z_TMP_FILE)
				(void) unlink(zp->z_source);
		}
	}
	if (!needmaint)
		sched_maint();
}

/*
 * Add this zone to the set of those needing transfers.
 */
static void
addxfer(zp)
	struct zoneinfo *zp;
{
	if (!(zp->z_flags & Z_NEED_XFER)) {
		zp->z_flags |= Z_NEED_XFER;
		xfers_deferred++;
		tryxfer();
	}
}
