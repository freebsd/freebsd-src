#if !defined(lint) && !defined(SABER)
static const char rcsid[] = "$Id: ns_notify.c,v 8.20 2002/04/25 05:27:12 marka Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1994-2000 by Internet Software Consortium.
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

/* Import. */

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <errno.h>
#include <limits.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include <isc/eventlib.h>
#include <isc/logging.h>
#include <isc/memcluster.h>

#include <isc/dst.h>

#include "port_after.h"

#include "named.h"

#ifdef BIND_NOTIFY

/* Types. */

struct pnotify {
	char *			name;
	ns_class		class;
	ns_type			type;
	evTimerID		timer;
	LINK(struct pnotify)	link;
};

/* Forward. */

static void		sysnotify(const char *, ns_class, ns_type);
static void		sysnotify_slaves(const char *, const char *,
					 ns_class, ns_type, int, int *, int *);
static void		sysnotify_ns(const char *, const char *,
				     ns_class, ns_type, int, int *, int *);
static void		free_notify(struct pnotify *);
static void		notify_timer(evContext, void *,
				     struct timespec, struct timespec);

/* Local. */

static LIST(struct pnotify) pending_notifies;
static LIST(struct pnotify) loading_notifies;

/* Public. */

/*
 * ns_notify(dname, class, type)
 *	call this when a zone has changed and its slaves need to know.
 */
void
ns_notify(const char *dname, ns_class class, ns_type type) {
	static const char no_room[] = "%s failed, cannot notify for zone %s";
	int delay, max_delay;
	struct zoneinfo *zp;
	struct pnotify *ni;

	zp = find_auth_zone(dname, class);
	if (zp == NULL) {
		ns_warning(ns_log_notify,
			   "no zone found for notify (\"%s\" %s %s)",
			   (dname && *dname) ? dname : ".",
			   p_class(class), p_type(type));
		return;
	}
	if (ns_samename(dname, zp->z_origin) != 1) {
		ns_warning(ns_log_notify,
			   "notify not called with top of zone (\"%s\" %s %s)",
			   (dname && *dname) ? dname : ".",
			   p_class(class), p_type(type));
		return;
	}
	if ((zp->z_flags & Z_NOTIFY) != 0) {
		ns_info(ns_log_notify,
			"suppressing duplicate notify (\"%s\" %s %s)",
			(dname && *dname) ? dname : ".",
			p_class(class), p_type(type));
		return;
	}
	ni = memget(sizeof *ni);
	if (ni == NULL) {
		ns_info(ns_log_notify, no_room, "memget", dname);
		return;
	}
	ni->name = savestr(dname, 0);
	if (ni->name == NULL) {
		memput(ni, sizeof *ni);
		ni = NULL;
		ns_info(ns_log_notify, no_room, "memget", dname);
		return;
	}
	ni->class = class;
	ni->type = type;
	INIT_LINK(ni, link);
	evInitID(&ni->timer);

	if (loading != 0) {
		APPEND(loading_notifies, ni, link);
		return;
	}

	/* Delay notification for from five seconds up to fifteen minutes. */
	max_delay = MIN(nzones, 895);
	max_delay = MAX(max_delay, 25);
	delay = 5 + (rand() % max_delay);
	if (evSetTimer(ev, notify_timer, ni,
		       evAddTime(evNowTime(), evConsTime(delay, 0)),
		       evConsTime(0, 0), &ni->timer) < 0) {
		ns_error(ns_log_notify, "evSetTimer() failed: %s",
			 strerror(errno));
		ni->name = freestr(ni->name);
		memput(ni, sizeof *ni);
		return;
	}

	zp->z_flags |= Z_NOTIFY;
	APPEND(pending_notifies, ni, link);
	ns_debug(ns_log_notify, 3,
		 "ns_notify(%s, %s, %s): ni %p, zp %p, delay %d",
		 (dname && *dname) ? dname : ".",
		 p_class(class), p_type(type),
		 ni, zp, delay);
}

void
notify_afterload() {
	struct pnotify *ni;

	INSIST(loading == 0);
	while ((ni = HEAD(loading_notifies)) != NULL) {
		UNLINK(loading_notifies, ni, link);
		ns_notify(ni->name, ni->class, ni->type);
		ni->name = freestr(ni->name);
		memput(ni, sizeof *ni);
	}
}

/*
 * ns_unnotify()
 *	call this when all pending notifies are now considered junque.
 */
void
ns_unnotify(void) {
	while (!EMPTY(pending_notifies)) {
		struct pnotify *ni = HEAD(pending_notifies);

		INSIST(LINKED(ni, link));
		UNLINK(pending_notifies, ni, link);
		free_notify(ni);
	}
}

/*
 * ns_stopnotify(const char *dname, ns_class class)
 *	stop notifies for this particular zone.
 */
void
ns_stopnotify(const char *dname, ns_class class) {
	struct pnotify *ni;

	ni = HEAD(pending_notifies);
	while (ni != NULL &&
	       (ni->class != class || ns_samename(ni->name, dname) != 1))
		ni = NEXT(ni, link);

	if (ni != NULL) {
		UNLINK(pending_notifies, ni, link);
		free_notify(ni);
	}
}

/* Private. */

/*
 * sysnotify(dname, class, type)
 *	cause a NOTIFY request to be sysquery()'d to each slave server
 *	of the zone that "dname" is within.
 */
static void
sysnotify(const char *dname, ns_class class, ns_type type) {
	const char *zname;
	u_int32_t zserial;
	int nns, na, i;
	struct zoneinfo *zp;
	struct in_addr *also_addr;

	ns_debug(ns_log_notify, 3, "sysnotify(%s, %s, %s)",
		 dname, p_class(class), p_type(type));
	zp = find_auth_zone(dname, class);
	if (zp == NULL) {
		ns_warning(ns_log_notify, "sysnotify: can't find \"%s\" (%s)",
			   dname, p_class(class));
		return;
	}
	if (ns_samename(dname, zp->z_origin) != 1) {
		ns_warning(ns_log_notify, "sysnotify: not auth for zone %s",
			   dname);
		return;
	}
	if (zp->z_notify == notify_no ||
	    (zp->z_notify == notify_use_default &&    
	     server_options->notify == notify_no))
		return;
	if (zp->z_type != z_master && zp->z_type != z_slave) {
		ns_warning(ns_log_notify, "sysnotify: %s not master or slave",
			   dname);
		return;
	}
	zname = zp->z_origin;
	zserial = zp->z_serial;
	nns = na = 0;
	if (zp->z_notify == notify_yes ||
	    (zp->z_notify == notify_use_default &&
	     server_options->notify == notify_yes))
		sysnotify_slaves(dname, zname, class, type,
				 zp - zones, &nns, &na);

	/*
	 * Handle any global or zone-specific also-notify clauses
	 */
	if (zp->z_notify_count != 0) {
		/* zone-specific also notify */
		
		ns_debug(ns_log_notify, 3, "zone notify ns = %d",
			 zp->z_notify_count);

		also_addr = zp->z_also_notify;
		for (i = 0; i < zp->z_notify_count; i++) {
			ns_debug(ns_log_notify, 4, "notifying %s",
				 inet_ntoa(*also_addr));
			sysquery(dname, class, type, also_addr, NULL, 1,
				 ns_port, NS_NOTIFY_OP, 0);
			also_addr++;
		}
		nns += zp->z_notify_count;
		na += zp->z_notify_count;
	} else if (server_options->notify_count != 0) {
		ns_debug(ns_log_notify, 4, "global notify ns = %d",
			 server_options->notify_count);
		also_addr = server_options->also_notify;
		for (i = 0; i < server_options->notify_count; i++) {
			ns_debug(ns_log_notify, 3, "notifying %s",
				 inet_ntoa(*also_addr));
			sysquery(dname, class, type, also_addr, NULL, 1,
				 ns_port, ns_o_notify, 0);
			also_addr++;
		}
		nns += server_options->notify_count;
		na += server_options->notify_count;
	}

	if (nns != 0 || na != 0)
		ns_info(ns_log_notify,
			"Sent NOTIFY for \"%s %s %s %u\" (%s); %d NS, %d A",
			dname, p_class(class), p_type(type), zserial, zname, nns, na);
}

static void
sysnotify_slaves(const char *dname, const char *zname,
		 ns_class class, ns_type type,
		 int zn, int *nns, int *na)
{
	const char *mname, *fname;
	struct hashbuf *htp;
	struct namebuf *np;
	struct databuf *dp;

	/*
	 * Master.
	 */
	htp = hashtab;
	np = nlookup(zname, &htp, &fname, 0);
	if (np == NULL) {
		ns_warning(ns_log_notify,
			   "sysnotify: found name \"%s\" but not zone",
			   dname);
		return;
	}
	mname = NULL;
	for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
		if (dp->d_zone == DB_Z_CACHE || !match(dp, class, ns_t_soa))
			continue;
		if (dp->d_type == ns_t_sig)
			continue;
		if (mname) {
			ns_notice(ns_log_notify,
				  "multiple SOA's for zone \"%s\"?",
				  zname);
			return;
		}
		mname = (char *) dp->d_data;
	}
	if (mname == NULL) {
		ns_notice(ns_log_notify, "no SOA found for zone \"%s\"",
			  zname);
		return;
	}
	for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
		if (dp->d_zone == DB_Z_CACHE || !match(dp, class, ns_t_ns))
			continue;
		if (dp->d_type == ns_t_sig)
			continue;
		if (ns_samename((char*)dp->d_data, mname) == 1)
			continue;
		sysnotify_ns(dname, (char *)dp->d_data, class, type,
			     zn, nns, na);
	}
}

static void
sysnotify_ns(const char *dname, const char *aname,
	     ns_class class, ns_type type,
	     int zn, int *nns, int *na)
{
	struct databuf *adp;
	struct namebuf *anp;
	const char *fname;
	struct in_addr nss[NSMAX];
	struct hashbuf *htp;
	int is_us, nsc, auth6, neg;
	int cname = 0;

	htp = hashtab;
	anp = nlookup(aname, &htp, &fname, 0);
	nsc = 0;
	is_us = 0;
	auth6 = 0;
	neg = 0;
	if (anp != NULL)
		for (adp = anp->n_data; adp; adp = adp->d_next) {
			struct in_addr ina;

			if (adp->d_class != class)
				continue;
			if (adp->d_rcode == NXDOMAIN) {
				neg = 1;
				break;
			}
			if (adp->d_type == T_CNAME && adp->d_rcode == 0) {
				cname = 1;
				ns_error(ns_log_notify,
					 "NS '%s' for '%s/%s' is a CNAME",
					 *aname ? aname : ".",
					 *dname ? dname : ".",
					 p_class(class));
				break;
			}
			if ((adp->d_type == T_AAAA || adp->d_type == ns_t_a6) &&
			    (zones[adp->d_class].z_type == z_master ||
			     zones[adp->d_class].z_type == z_slave)) {
				auth6 = 1;
				continue;
			}
			if (!match(adp, class, T_A))
				continue;
			if (adp->d_rcode) {
				neg = 1;
				continue;
			}
			if (adp->d_type == ns_t_sig)
				continue;
			ina = ina_get(adp->d_data);
			if (aIsUs(ina)) {
				is_us = 1;
				continue;
			}
			if (nsc < NSMAX)
				nss[nsc++] = ina;
		} /*next A*/
	if (nsc == 0) {
		if (!is_us && !cname && !auth6 && !neg &&
		    !NS_OPTION_P(OPTION_NOFETCHGLUE)) {
			struct qinfo *qp;

			qp = sysquery(aname, class, ns_t_a, NULL, NULL, 0,
				      ns_port, ns_o_query, 0);
			if (qp != NULL)
				qp->q_notifyzone = zn;
		}
		return;
	}
	sysquery(dname, class, type, nss, NULL, nsc, ns_port, ns_o_notify, 0);
	(*nns)++;
	*na += nsc;
}

static void
free_notify(struct pnotify *ni) {
	struct zoneinfo *zp;

	INSIST(!LINKED(ni, link));
	zp = find_auth_zone(ni->name, ni->class);
	if (zp != NULL && ns_samename(ni->name, zp->z_origin) == 1) {
		INSIST((zp->z_flags & Z_NOTIFY) != 0);
		zp->z_flags &= ~Z_NOTIFY;
	}
	if (evTestID(ni->timer)) {
		evClearTimer(ev, ni->timer);
		evInitID(&ni->timer);
	}
	ni->name = freestr(ni->name);
	memput(ni, sizeof *ni);
}

static void
notify_timer(evContext ctx, void *uap,
	     struct timespec due,
	     struct timespec inter)
{
	struct pnotify *ni = uap;

	UNUSED(ctx);
	UNUSED(due);
	UNUSED(inter);

	INSIST(evTestID(ni->timer));
	evInitID(&ni->timer);
	INSIST(LINKED(ni, link));
	UNLINK(pending_notifies, ni, link);
	sysnotify(ni->name, ni->class, ni->type);
	free_notify(ni);
}

#endif /*BIND_NOTIFY*/
