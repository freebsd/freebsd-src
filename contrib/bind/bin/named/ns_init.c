#if !defined(lint) && !defined(SABER)
static const char sccsid[] = "@(#)ns_init.c	4.38 (Berkeley) 3/21/91";
static const char rcsid[] = "$Id: ns_init.c,v 8.77 2002/08/20 04:27:23 marka Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1986, 1990
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

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
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

#ifdef DEBUG
static void		content_zone(int, int);
#endif
static void		purgeandload(struct zoneinfo *zp);

/*
 * Set new refresh time for zone.  Use a random number in the last half of
 * the refresh limit; we want it to be substantially correct while still
 * preventing slave synchronization.
 */
void
ns_refreshtime(struct zoneinfo *zp, time_t timebase) {
	u_long refresh = (zp->z_refresh > 0) ? zp->z_refresh : INIT_REFRESH;
	time_t half = (refresh + 1) / 2;

	if (zp->z_flags & Z_NEEDREFRESH) {
		zp->z_flags &= ~Z_NEEDREFRESH;
		zp->z_time = timebase;
	} else
		zp->z_time = timebase + half + (rand() % half);
}

/*
 * Set new retry time for zone.
 */
void
ns_retrytime(struct zoneinfo *zp, time_t timebase) {

	zp->z_flags &= ~Z_NEEDREFRESH;
	zp->z_time = timebase + zp->z_retry;
}

/*
 * Read configuration file and save it as internal state.
 */
time_t
ns_init(const char *conffile) {
	struct zoneinfo *zp;
	static int loads = 0;			/* number of times loaded */
	time_t mtime;

	ns_debug(ns_log_config, 1, "ns_init(%s)", conffile);
	gettime(&tt);

	if (loads == 0) {
		/* Init zone data. */
		zones = NULL;
		INIT_LIST(freezones);
		INIT_LIST(reloadingzones);
		nzones = 0;
		make_new_zones();

		/* Init cache. */
		zones[0].z_type = z_cache;
		zones[0].z_origin = savestr("", 1);

		/* Allocate cache hash table, formerly the root hash table. */
		hashtab = savehash((struct hashbuf *)NULL);

		/* Allocate root-hints/file-cache hash table. */
		fcachetab = savehash((struct hashbuf *)NULL);

		/* Init other misc stuff. */
		dst_init();
		init_configuration();
	} else {
		/* Mark previous zones as not yet found in boot file. */
		block_signals();
		for (zp = &zones[1]; zp < &zones[nzones]; zp++)
			if (zp->z_type != z_nil) {
				zp->z_flags &= ~Z_FOUND;
				if (LINKED(zp, z_reloadlink))
					UNLINK(reloadingzones, zp,
					       z_reloadlink);
			}
		unblock_signals();
	}

#ifdef DEBUG
	if (debug >= 3) {
		ns_debug(ns_log_config, 3, "content of zones before loading");
		content_zone(nzones - 1, 3);
	}
#endif

	mtime = load_configuration(conffile);

	/* Erase all old zones that were not found. */
	for (zp = &zones[0]; zp < &zones[nzones]; zp++) {
		if (zp->z_type == z_cache)
			continue;
		if (zp->z_type != z_nil && (zp->z_flags & Z_FOUND) == 0)
			remove_zone(zp, "removed");
	}
	/* Reload parent zones of zones removed */
	for (zp = &zones[0]; zp < &zones[nzones]; zp++) {
		if (zp->z_type == z_cache)
			continue;
		if (zp->z_type != z_nil &&
		    (zp->z_flags & Z_PARENT_RELOAD) != 0) {
			zp->z_flags &= ~Z_PARENT_RELOAD;
			purgeandload(zp);
		}
	}

#ifdef DEBUG
	if (debug >= 2) {
		ns_debug(ns_log_config, 2, "content of zones after loading");
		content_zone(nzones-1, 2);
	}
#endif

	ns_debug(ns_log_config, 1, "exit ns_init()");
	loads++;
	return (mtime);
}

void
zoneinit(struct zoneinfo *zp) {
	struct stat sb;
	int result;

	/*
	 * Try to load zone from backup file,
	 * if one was specified and it exists.
	 * If not, or if the data are out of date,
	 * we will refresh the zone from a primary
	 * immediately.
	 */
	if (zp->z_source == NULL)
		return;
	result = stat(zp->z_source, &sb);
	if (result != -1) {
		ns_stopxfrs(zp);
		purge_zone(zp, hashtab);
	}
	if (result == -1 ||
	    db_load(zp->z_source, zp->z_origin, zp, NULL, ISNOTIXFR))
	{
		/*
		 * Set zone to be refreshed immediately.
		 */
		zp->z_refresh = INIT_REFRESH;
		zp->z_retry = INIT_REFRESH;
		if ((zp->z_flags & (Z_QSERIAL|Z_XFER_RUNNING)) == 0) {
			zp->z_time = tt.tv_sec;
			sched_zone_maint(zp);
		}
	} else {
		zp->z_flags |= Z_AUTH;
		zp->z_flags &= ~(Z_NEED_RELOAD|Z_EXPIRED);
		ns_refreshtime(zp, tt.tv_sec);
		sched_zone_maint(zp);
	}
}

/*
 * Purge the zone and reload all parent zones.  This needs to be done when
 * we unload a zone, since the child zone will have stomped the parent's
 * delegation to that child when it was first loaded.
 */
void
do_reload(struct zoneinfo *ozp, int mark) {
	struct zoneinfo *zp;
	const char *domain = ozp->z_origin;
	int type = ozp->z_type;
	int class = ozp->z_class;

	ns_debug(ns_log_config, 1, "do_reload: %s %d %d %d", 
		 *domain ? domain : ".", type, class, mark);

	/*
	 * Check if the zone has changed type.  If so, we might not need to
	 * do any purging or parent reloading.
	 *
	 * If the new zone is a master zone, then it will have purged the
	 * old data and loaded, so we don't need to do anything.
	 *
	 * If the new zone is a slave or stub zone and has successfully loaded,
	 * then we don't need to do anything either.
	 *
	 * NOTE: we take care not to match ourselves.
	 */
	zp = find_zone(domain, class);
	if (zp != NULL &&
	    ((type != z_master && zp->z_type == z_master) ||
	     (type != z_slave && zp->z_type == z_slave && zp->z_serial != 0) ||
	     (type != z_stub && zp->z_type == z_stub && zp->z_serial != 0)))
		return;

	/*
	 * Clean up any leftover data.
	 */
	ns_stopxfrs(zp);
	if (type == z_hint || (type == z_stub && *domain == 0))
		purge_zone(ozp, fcachetab);
	else
		purge_zone(ozp, hashtab);

	/*
	 * Reload
	 */
	while (*domain) {
		const char *s;
		int escaped;

		/*
		 * XXX this is presentation level hair and belongs elsewhere.
		 */
		escaped = 0;
		for (s = domain; *s != '\0'; s++) {
			if (!escaped) {
				if (*s == '.')
					break;
				else if (*s == '\\')
					escaped = 1;
			} else
				escaped = 0;
		}

		if (*s != '\0')
			domain = s + 1;	/* skip label and its separator */
		else
			domain = "";	/* root zone */

		zp = find_zone(domain, class);
		if (zp != NULL && zp->z_type != Z_HINT) {
			ns_debug(ns_log_config, 1, "do_reload: matched %s",
				 *domain ? domain : ".");
			if (mark)
				zp->z_flags |= Z_PARENT_RELOAD;
			else
				purgeandload(zp);
			break;
		}
	}
}

static void
purgeandload(struct zoneinfo *zp) {

#ifdef BIND_UPDATE
	/*
	 * A dynamic zone might have changed, so we
	 * need to dump it before removing it.
	 */
	if (zp->z_type == Z_PRIMARY &&
	    (zp->z_flags & Z_DYNAMIC) != 0 &&
	    ((zp->z_flags & Z_NEED_SOAUPDATE) != 0 ||
	     (zp->z_flags & Z_NEED_DUMP) != 0))
		(void) zonedump(zp, ISNOTIXFR);
#endif
	ns_stopxfrs(zp);

	if (zp->z_type == Z_HINT)
		purge_zone(zp, fcachetab);
	else
		purge_zone(zp, hashtab);

	zp->z_flags &= ~Z_AUTH;

	switch (zp->z_type) {
	case Z_SECONDARY:
	case Z_STUB:
		zoneinit(zp);
		break;
	case Z_PRIMARY:
		if (db_load(zp->z_source, zp->z_origin, zp, 0, ISNOTIXFR) == 0)
			zp->z_flags |= Z_AUTH;
		break;
	case Z_HINT:
	case Z_CACHE:
		(void)db_load(zp->z_source, zp->z_origin, zp, 0, ISNOTIXFR);
		break;
	}
}

#ifdef DEBUG
/* prints out the content of zones */
static void
content_zone(int end, int level) {
	int i;

	for (i = 0;  i <= end;  i++) {
		printzoneinfo(i, ns_log_config, level);
	}
}
#endif

enum context
ns_ptrcontext(owner)
	const char *owner;
{
	if (ns_samedomain(owner, "in-addr.arpa") ||
	    ns_samedomain(owner, "ip6.int"))
		return (hostname_ctx);
	return (domain_ctx);
}

enum context
ns_ownercontext(type, transport)
	int type;
	enum transport transport;
{
	enum context context = domain_ctx;

	switch (type) {
	case T_A:
	case T_WKS:
	case T_MX:
		switch (transport) {
		case update_trans:
		case primary_trans:
		case secondary_trans:
			context = owner_ctx;
			break;
		case response_trans:
			context = hostname_ctx;
			break;
		default:
			panic("impossible condition in ns_ownercontext()", 
			      NULL);
		}
		break;
	case T_MB:
	case T_MG:
		context = mailname_ctx;
		break;
	default:
		/* Nothing to do. */
		break;
	}
	return (context);
}

int
ns_nameok(const struct qinfo *qry, const char *name, int class,
	  struct zoneinfo *zp, enum transport transport,
	  enum context context,
	  const char *owner,
	  struct in_addr source)
{
	enum severity severity = not_set;
	int ok = 1;

	if (zp != NULL)
		severity = zp->z_checknames;
	if (severity == not_set)
		severity = server_options->check_names[transport];

	if (severity == ignore)
		return (1);
	switch (context) {
	case domain_ctx:
		ok = (class != C_IN) || res_dnok(name);
		break;
	case owner_ctx:
		ok = (class != C_IN) || res_ownok(name);
		break;
	case mailname_ctx:
		ok = res_mailok(name);
		break;
	case hostname_ctx:
		ok = res_hnok(name);
		break;
	default:
		ns_panic(ns_log_default, 1,
			 "unexpected context %d in ns_nameok", (int)context);
	}
	if (!ok) {
		char *q, *s, *o;

		if (source.s_addr == INADDR_ANY)
			s = savestr(transport_strings[transport], 0);
		else {
			s = newstr(strlen(transport_strings[transport]) +
				   sizeof " from [000.000.000.000] for [000.000.000.000]", 0);
			if (s != NULL) {
			    if (transport == response_trans && qry != NULL) {
				if ((qry->q_flags & Q_PRIMING) != 0) {
				    sprintf(s, "%s from [%s] for priming",
					    transport_strings[transport],
					    inet_ntoa(source));
				} else if ((qry->q_flags & Q_ZSERIAL) != 0) {
				    sprintf(s, "%s from [%s] for soacheck",
					    transport_strings[transport],
					    inet_ntoa(source));
				} else if ((qry->q_flags & Q_SYSTEM) != 0) {
				    sprintf(s, "%s from [%s] for sysquery",
					    transport_strings[transport],
					    inet_ntoa(source));
				} else {
				    q=strdup(inet_ntoa(qry->q_from.sin_addr));
				    sprintf(s, "%s from [%s] for [%s]",
					    transport_strings[transport],
					    inet_ntoa(source),
					    q != NULL ? q : "memget failed");
				    free(q);
				}
			    } else {
				sprintf(s, "%s from [%s]",
					(transport == response_trans)
						? "query"
						: transport_strings[transport],
					inet_ntoa(source));
			    }
			}
		}
		if (ns_samename(owner, name) == 1)
			o = savestr("", 0);
		else {
			const char *t = (*owner == '\0') ? "." : owner;

			o = newstr(strlen(t) + sizeof " (owner \"\")", 0);
			if (o)
				sprintf(o, " (owner \"%s\")", t);
		}
		/*
		 * We use log_write directly here to avoid duplicating
		 * the message formatting and arguments.
		 */
		log_write(log_ctx, ns_log_default, 
			  (transport != response_trans) ||
			  (o == NULL) || (s == NULL) ||
			  ( (qry != NULL) &&
				(qry->q_flags & (Q_PRIMING|Q_ZSERIAL)) ) ?
			  log_warning : log_info,
			  "%s name \"%s\"%s %s (%s) is invalid - %s",
			  context_strings[context],
			  name, o != NULL ? o : "[memget failed]",
			  p_class(class),
			  s != NULL ? s : "[memget failed]",
			  (severity == fail) ?
			  "rejecting" : "proceeding anyway");
		if (severity == warn)
			ok = 1;
		if (s != NULL)
			(void)freestr(s);
		if (o != NULL)
			(void)freestr(o);
	}
	return (ok);
}

int
ns_wildcard(const char *name) {
	if (*name != '*')
		return (0);
	return (*++name == '\0');
}

void
ns_shutdown() {
	struct zoneinfo *zp;

#ifdef BIND_NOTIFY
	ns_unnotify();
#endif
	/* Erase zones. */
	for (zp = &zones[0]; zp < &zones[nzones]; zp++) {
		if (zp->z_type) {
			if (zp->z_type != z_hint && zp->z_type != z_cache) {
				ns_stopxfrs(zp);
				purge_zone(zp, hashtab);
			} else if (zp->z_type == z_hint)
				purge_zone(zp, fcachetab);
			free_zone_contents(zp, 1);
		}
	}

	/* Erase the cache. */
	clean_cache(hashtab, 1);
	hashtab->h_cnt = 0;		/* ??? */
	rm_hash(hashtab);
	hashtab = NULL;
	clean_cache(fcachetab, 1);
	fcachetab->h_cnt = 0;		/* ??? */
	rm_hash(fcachetab);
	fcachetab = NULL;

	if (zones != NULL)
		memput(zones, nzones * sizeof *zones);
	zones = NULL;

	freeComplaints();
	shutdown_configuration();
}
