#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)ns_req.c	4.47 (Berkeley) 7/1/91";
static char rcsid[] = "$Id: ns_req.c,v 8.46 1998/03/27 00:21:03 halley Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1986, 1988, 1990
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
 * Portions Copyright (c) 1995 by International Business Machines, Inc.
 *
 * International Business Machines, Inc. (hereinafter called IBM) grants
 * permission under its copyrights to use, copy, modify, and distribute this
 * Software with or without fee, provided that the above copyright notice and
 * all paragraphs of this notice appear in all copies, and that the name of IBM
 * not be used in connection with the marketing of any product incorporating
 * the Software or modifications thereof, without specific, written prior
 * permission.
 *
 * To the extent it has a right to do so, IBM grants an immunity from suit
 * under its patents, if any, for the use, sale or manufacture of products to
 * the extent that such products are used for performing Domain Name System
 * dynamic updates in TCP/IP networks by means of the Software.  No immunity is
 * granted for any product per se or for any other function of any product.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", AND IBM DISCLAIMS ALL WARRANTIES,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  IN NO EVENT SHALL IBM BE LIABLE FOR ANY SPECIAL,
 * DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE, EVEN
 * IF IBM IS APPRISED OF THE POSSIBILITY OF SUCH DAMAGES.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include <isc/eventlib.h>
#include <isc/logging.h>
#include <isc/memcluster.h>

#include "port_after.h"

#include "named.h"

struct addinfo {
	char		*a_dname;		/* domain name */
	char		*a_rname;		/* referred by */
	u_int16_t	a_rtype;		/* referred by */
	u_int16_t	a_class;		/* class for address */
};

#ifndef BIND_UPDATE
enum req_action { Finish, Refuse, Return };
#endif

static struct addinfo	addinfo[NADDRECS];
static void		addname(const char *, const char *,
				u_int16_t, u_int16_t);
static void		copyCharString(u_char **, const char *);

static enum req_action	req_query(HEADER *hp, u_char **cpp, u_char *eom,
				  struct qstream *qsp,
				  int *buflenp, int *msglenp,
				  u_char *msg, int dfd,
				  struct sockaddr_in from);

static enum req_action	req_iquery(HEADER *hp, u_char **cpp, u_char *eom,
				   int *buflenp, u_char *msg,
				   struct sockaddr_in from);

#ifdef BIND_NOTIFY
static enum req_action	req_notify(HEADER *hp, u_char **cpp, u_char *eom,
				   u_char *msg,struct sockaddr_in from);
#endif

/*
 * Process request using database; assemble and send response.
 */
void
ns_req(u_char *msg, int msglen, int buflen, struct qstream *qsp,
       struct sockaddr_in from, int dfd)
{
	HEADER *hp = (HEADER *) msg;
	u_char *cp, *eom;
	enum req_action action;
	int n;

#ifdef DEBUG
	if (debug > 3) {
		ns_debug(ns_log_packet, 3, "ns_req(from %s)", sin_ntoa(from));
		fp_nquery(msg, msglen, log_get_stream(packet_channel));
	}
#endif

	/*
	 * It's not a response so these bits have no business
	 * being set. will later simplify work if we can
	 * safely assume these are always 0 when a query
	 * comes in.
	 */
	hp->aa = hp->ra = 0;

	hp->rcode = NOERROR;
	cp = msg + HFIXEDSZ;
	eom = msg + msglen;
	buflen -= HFIXEDSZ;

	free_addinfo();	/* sets addcount to zero */
	dnptrs[0] = NULL;

	switch (hp->opcode) {
	case ns_o_query:
		action = req_query(hp, &cp, eom, qsp,
				   &buflen, &msglen,
				   msg, dfd, from);
		break;

	case ns_o_iquery:
		action = req_iquery(hp, &cp, eom, &buflen, msg, from);
		break;

#ifdef BIND_NOTIFY
	case ns_o_notify:
		action = req_notify(hp, &cp, eom, msg, from);
		break;
#endif

#ifdef BIND_UPDATE
	case ns_o_update:
		action = req_update(hp, cp, eom, msg, qsp, dfd, from);
		break;
#endif /* BIND_UPDATE */

	default:
		ns_debug(ns_log_default, 1,
			 "ns_req: Opcode %d not implemented", hp->opcode);
		/* XXX - should syslog, limited by haveComplained */
		hp->qdcount = htons(0);
		hp->ancount = htons(0);
		hp->nscount = htons(0);
		hp->arcount = htons(0);
		hp->rcode = NOTIMP;
		action = Finish;
	}

	/*
	 * vector via internal opcode.  (yes, it was even uglier before.)
	 */
	switch (action) {
	case Return:
		return;
	case Refuse:
		hp->rcode = REFUSED;
		cp = eom;
		/*FALLTHROUGH*/
	case Finish:
		/* rest of the function handles this case */
		break;
	default:
		panic("ns_req: bad action variable", NULL);
		/*NOTREACHED*/
	}

	/*
	 * apply final polish
	 */
	hp->qr = 1;		/* set Response flag */
	hp->ra = (NS_OPTION_P(OPTION_NORECURSE) == 0);

	n = doaddinfo(hp, cp, buflen);
	cp += n;
	buflen -= n;

#ifdef DEBUG
	ns_debug(ns_log_default, 1,
		 "ns_req: answer -> %s fd=%d id=%d size=%d",
		 sin_ntoa(from), (qsp == NULL) ? dfd : qsp->s_rfd,
		 ntohs(hp->id), cp - msg);
	if (debug >= 10)
		fp_nquery(msg, cp - msg, log_get_stream(packet_channel));
#endif /*DEBUG*/
	if (qsp == NULL) {
		if (sendto(dfd, (char*)msg, cp - msg, 0,
			   (struct sockaddr *)&from,
			   sizeof(from)) < 0) {
			if (!haveComplained(ina_ulong(from.sin_addr),
					    (u_long)sendtoStr))
				ns_info(ns_log_default,
					"ns_req: sendto(%s): %s",
					sin_ntoa(from), strerror(errno));
			nameserIncr(from.sin_addr, nssSendtoErr);
		}
		nameserIncr(from.sin_addr, nssSentAns);
		if (hp->rcode == NXDOMAIN) 
			nameserIncr(from.sin_addr, nssSentNXD);
		if (!hp->aa) 
			nameserIncr(from.sin_addr, nssSentNaAns);
	} else
		writestream(qsp, msg, cp - msg);

	/* Is now a safe time? */
	if (needs_prime_cache)
		prime_cache();
}

#ifdef BIND_NOTIFY
int
findZonePri(const struct zoneinfo *zp, const struct sockaddr_in from) {
	struct in_addr ina;
	int i;

	ina = from.sin_addr;
	for (i = 0; (u_int)i < zp->z_addrcnt; i++)
		if (ina_equal(zp->z_addr[i], ina))
			return (i);
	return (-1);
}

static enum req_action
req_notify(HEADER *hp, u_char **cpp, u_char *eom, u_char *msg,
	   struct sockaddr_in from)
{
	int n, type, class, zn;
	char dnbuf[MAXDNAME];
	struct namebuf *np;
	const char *fname;
	struct hashbuf *htp = hashtab;		/* lookup relative to root */

	/* valid notify's have one question and zero answers */
	if ((ntohs(hp->qdcount) != 1)
	    || ntohs(hp->ancount) != 0
	    || ntohs(hp->nscount) != 0
	    || ntohs(hp->arcount) != 0) {
		ns_debug(ns_log_notify, 1,
			 "FORMERR Notify header counts wrong");
		hp->qdcount = htons(0);
		hp->ancount = htons(0);
		hp->nscount = htons(0);
		hp->arcount = htons(0);
		hp->rcode = FORMERR;
		return (Finish);
	}

	n = dn_expand(msg, eom, *cpp, dnbuf, sizeof dnbuf);
	if (n < 0) {
		ns_debug(ns_log_notify, 1,
			 "FORMERR Query expand name failed");
		hp->rcode = FORMERR;
		return (Finish);
	}
	*cpp += n;
	if (*cpp + 2 * INT16SZ > eom) {
		ns_debug(ns_log_notify, 1,
			 "FORMERR notify too short");
		hp->rcode = FORMERR;
		return (Finish);
	}
	GETSHORT(type, *cpp);
	GETSHORT(class, *cpp);
	ns_info(ns_log_notify, "rcvd NOTIFY(%s, %s, %s) from %s",
		dnbuf, p_class(class), p_type(type), sin_ntoa(from));
	/* XXX - when answers are allowed, we'll need to do compression
	 * correctly here, and we will need to check for packet underflow.
	 */
	np = nlookup(dnbuf, &htp, &fname, 0);
	if (!np) {
		ns_info(ns_log_notify,
			"rcvd NOTIFY for \"%s\", name not in cache",
			dnbuf);
		hp->rcode = SERVFAIL;
		return (Finish);
	}
	zn = findMyZone(np, class);
	if (zn == DB_Z_CACHE || zones[zn].z_type != z_slave) {
		/* this can come if a user did an AXFR of some zone somewhere
		 * and that zone's server now wants to tell us that the SOA
		 * has changed.  AXFR's always come from nonpriv ports so it
		 * isn't possible to know whether it was the server or just
		 * "dig".  this condition can be avoided by using secure zones
		 * since that way only real secondaries can AXFR from you.
		 */
		ns_info(ns_log_notify,
			"NOTIFY for non-secondary name (%s), from %s",
			dnbuf, sin_ntoa(from));
		goto refuse;
	}
	if (findZonePri(&zones[zn], from) == -1) {
		ns_info(ns_log_notify,
			"NOTIFY from non-master server (zone %s), from %s",
			zones[zn].z_origin, sin_ntoa(from));
		goto refuse;
	}
	switch (type) {
	case T_SOA:
		if (strcasecmp(dnbuf, zones[zn].z_origin) != 0) {
			ns_info(ns_log_notify,
				"NOTIFY(SOA) for non-origin (%s), from %s",
				dnbuf, sin_ntoa(from));
			goto refuse;
		}
		if (zones[zn].z_flags &
		    (Z_NEED_RELOAD|Z_NEED_XFER|Z_QSERIAL|Z_XFER_RUNNING)) {
			ns_info(ns_log_notify,
				"NOTIFY(SOA) for zone already xferring (%s)",
				dnbuf);
			goto noerror;
		}
		zones[zn].z_time = tt.tv_sec;
		qserial_query(&zones[zn]);
		sched_zone_maint(&zones[zn]);
		break;
	default:
		/* unimplemented, but it's not a protocol error, just
		 * something to be ignored.
		 */
		break;
	}
 noerror:
	hp->rcode = NOERROR;
	return (Finish);
 refuse:
	hp->rcode = REFUSED;
	return (Finish);
}
#endif /*BIND_NOTIFY*/

static enum req_action
req_query(HEADER *hp, u_char **cpp, u_char *eom, struct qstream *qsp,
	  int *buflenp, int *msglenp, u_char *msg, int dfd,
	  struct sockaddr_in from)
{
	int n, class, type, count, zone, foundname, founddata, omsglen, cname;
	u_int16_t id;
	u_char **dpp, *omsg, *answers;
	char dnbuf[MAXDNAME], *dname;
	const char *fname;
	struct hashbuf *htp;
	struct databuf *nsp[NSMAX];
	struct namebuf *np, *anp;
	struct qinfo *qp;
	struct zoneinfo *zp;
	struct databuf *dp;

	nameserIncr(from.sin_addr, nssRcvdQ);

	nsp[0] = NULL;
	dpp = dnptrs;
	*dpp++ = msg;
	*dpp = NULL;
	/*
	 * Make gcc happy.
	 */
	omsglen = 0;
	omsg = NULL;
	id = 0;

	/* valid queries have one question and zero answers */
	if ((ntohs(hp->qdcount) != 1)
	    || ntohs(hp->ancount) != 0
	    || ntohs(hp->nscount) != 0
	    || ntohs(hp->arcount) != 0) {
		ns_debug(ns_log_default, 1,
			 "FORMERR Query header counts wrong");
		hp->qdcount = htons(0);
		hp->ancount = htons(0);
		hp->nscount = htons(0);
		hp->arcount = htons(0);
		hp->rcode = FORMERR;
		return (Finish);
	}

	/*
	 * Get domain name, class, and type.
	 */
	if ((**cpp & INDIR_MASK) == 0)
		*dpp++ = *cpp;	/* remember name for compression */
	*dpp = NULL;
	n = dn_expand(msg, eom, *cpp, dnbuf, sizeof dnbuf);
	if (n < 0) {
		ns_debug(ns_log_default, 1,
			 "FORMERR Query expand name failed");
		hp->rcode = FORMERR;
		return (Finish);
	}
	*cpp += n;
	if (*cpp + 2 * INT16SZ > eom) {
		ns_debug(ns_log_default, 1,
			 "FORMERR Query message length short");
		hp->rcode = FORMERR;
		return (Finish);
	}
	GETSHORT(type, *cpp);
	GETSHORT(class, *cpp);
	if (*cpp < eom) {
		ns_debug(ns_log_default, 6,
			 "message length > received message");
		*msglenp = *cpp - msg;
	}

	qtypeIncr(type);

	/*
	 * Process query.
	 */
	if (type == T_AXFR) {
		/* Refuse request if not a TCP connection. */
		if (qsp == NULL) {
			ns_info(ns_log_default,
				"rejected UDP AXFR from %s for \"%s\"",
				sin_ntoa(from), *dnbuf ? dnbuf : ".");
			return (Refuse);
		}
		/* The position of this is subtle. */
		nameserIncr(from.sin_addr, nssRcvdAXFR);
		hp->rd = 0;		/* Recursion not possible. */
	}
	*buflenp -= *msglenp;
	count = 0;
	founddata = 0;
	dname = dnbuf;
	cname = 0;

#ifdef QRYLOG
	if (qrylog) {
		ns_info(ns_log_queries, "XX /%s/%s/%s",
			inet_ntoa(from.sin_addr), 
			(dname[0] == '\0') ? "." : dname, 
			p_type(type));
	}
#endif /*QRYLOG*/

 try_again:
	foundname = 0;
	ns_debug(ns_log_default, 1, "req: nlookup(%s) id %d type=%d class=%d",
		 dname, ntohs(hp->id), type, class);
	htp = hashtab;		/* lookup relative to root */
	if ((anp = np = nlookup(dname, &htp, &fname, 0)) == NULL)
		fname = "";
	ns_debug(ns_log_default, 1, "req: %s '%s' as '%s' (cname=%d)",
		 np == NULL ? "missed" : "found",
		 dname, fname, cname);

#ifdef YPKLUDGE
	/* Some braindamaged resolver software will not 
	   recognize internet addresses in dot notation and 
	   send out address  queries for "names" such as 
	   128.93.8.1.  This kludge will prevent those 
	   from flooding higher level servers.
	   We simply claim to be authoritative and that
	   the domain doesn't exist.
	   Note that we could return the address but we
	   don't do that in order to encourage that broken
	   software is fixed.
	*/

	if (!np && type == T_A && class == C_IN && dname) {
		struct in_addr ina;

		if (inet_aton(dname, &ina)) {
			hp->rcode = NXDOMAIN;
			hp->aa = 1;
			ns_debug(ns_log_default, 3,
				 "ypkludge: hit as '%s'", dname);
			return (Finish);
		}
	}
#endif /*YPKLUDGE*/

	/*
	 * Begin Access Control Point
	 */

	zone = DB_Z_CACHE;
	if (np) {
		struct namebuf *access_np;

		/*
		 * Find out which zone this will be answered from.  Note
		 * that we look for a zone with the same class as ours.
		 * The np that we found in the database might not be the
		 * one we asked for (i.e. dname might not equal fname).  This
		 * is OK, since if a name doesn't exist, we need to go up
		 * the tree until we find the closest enclosing zone that
		 * is of the same class.
		 */
		for (access_np = np; access_np != NULL;
		     access_np = np_parent(access_np)) {
			dp = access_np->n_data;
			while (dp && dp->d_class != class)
				dp = dp->d_next;
			if (dp != NULL) {
				zone = dp->d_zone;
				break;
			}
		}
	}

	zp = &zones[zone];

	/*
	 * Are queries allowed from this host?
	 */
	if (type != T_AXFR) {
		ip_match_list query_acl;

		if (zp->z_query_acl != NULL)
			query_acl = zp->z_query_acl;
		else
			query_acl = server_options->query_acl;

		if (query_acl != NULL
		    && !ip_address_allowed(query_acl, from.sin_addr)) {
			ns_notice(ns_log_security,
				  "unapproved query from %s for \"%s\"",
				  sin_ntoa(from), *dname ? dname : ".");
			return (Refuse);
		}
	} else {
		ip_match_list transfer_acl;

		/* Do they have permission to do a zone transfer? */

		if (zp->z_transfer_acl != NULL)
			transfer_acl = zp->z_transfer_acl;
		else
			transfer_acl = server_options->transfer_acl;

		if (transfer_acl != NULL
		    && !ip_address_allowed(transfer_acl, from.sin_addr)) {
			ns_notice(ns_log_security,
				  "unapproved AXFR from %s for \"%s\" (acl)",
				  sin_ntoa(from), *dname ? dname : ".");
			return (Refuse);
		}

		/* Are we authoritative? */

		if ((zp->z_flags & Z_AUTH) == 0) {
			ns_notice(ns_log_security,
			  "unapproved AXFR from %s for \"%s\" (not auth)",
				  sin_ntoa(from), *dname ? dname : ".");
			return (Refuse);
		}

		/* Is the name at a zone cut? */

		if (strcasecmp(zp->z_origin, dname) != 0) {
			ns_notice(ns_log_security,
			  "unapproved AXFR from %s for \"%s\" (not zone top)",
				  sin_ntoa(from), *dname ? dname : ".");
			return (Refuse);
		}

		ns_info(ns_log_security, "approved AXFR from %s for \"%s\"",
			sin_ntoa(from), *dname ? dname : ".");
	}

	/*
	 * End Access Control Point
	 */

	/*
	 * Yow!
	 */
	if (!strcasecmp(dnbuf, "VERSION.BIND") &&
	    class == C_CHAOS && type == T_TXT) {
		u_char *tp;

		hp->ancount = htons(1);
		hp->nscount = htons(0);
		hp->arcount = htons(0);
		hp->rcode = NOERROR;
		hp->aa = 1;
		hp->ra = 0;
		copyCharString(cpp, "VERSION");	/* Name */
		copyCharString(cpp, "BIND");
		*(*cpp)++ = 0x00;
		PUTSHORT(T_TXT, *cpp);		/* Type */
		PUTSHORT(C_CHAOS, *cpp);	/* Class */
		PUTLONG(0, *cpp);		/* TTL */
		tp = *cpp;			/* Temp RdLength */
		PUTSHORT(0, *cpp);
		copyCharString(cpp, ShortVersion);
		PUTSHORT((*cpp) - (tp + INT16SZ), tp);	/* Real RdLength */
		*msglenp = *cpp - msg;		/* Total message length */
		return (Finish);
	}

	/*
	 * If we don't know anything about the requested name,
	 * go look for nameservers.
	 */
	if (!np || fname != dname)
		goto fetchns;

	foundname++;
	answers = *cpp;
	count = *cpp - msg;

	/* Look for NXDOMAIN record with appropriate class
	 * if found return immediately
	 */
	for (dp = np->n_data; dp; dp = dp->d_next) {
		if (!stale(dp) && (dp->d_rcode == NXDOMAIN) &&
		    (dp->d_class == class)) {
#ifdef RETURNSOA
			n = finddata(np, class, T_SOA, hp, &dname,
				     buflenp, &count);
			if (n != 0) {
				if (count) {
					*cpp += n;
					*buflenp -= n;
					*msglenp += n;
					hp->nscount = htons((u_int16_t)count);
				}
				if (hp->rcode == NOERROR_NODATA) {
					/* this should not occur */
					hp->rcode = NOERROR;
					return (Finish);
				}
			}
#endif
			hp->rcode = NXDOMAIN;
			/* 
			 * XXX forcing AA all the time isn't right, but
			 * we have to work that way by default
			 * for compatibility with older servers.
			 */
			if (!NS_OPTION_P(OPTION_NONAUTH_NXDOMAIN))
			    hp->aa = 1;
			ns_debug(ns_log_default, 3, "NXDOMAIN aa = %d",
				 hp->aa);
			return (Finish);
		}
	}

	/*
	 * If not NXDOMAIN, the NOERROR_NODATA record might be
	 * anywhere in the chain.  Have to go through the grind.
	 */

	n = finddata(np, class, type, hp, &dname, buflenp, &count);
	if (n == 0) {
		/*
		 * NO data available.  Refuse AXFR requests, or
		 * look for better servers for other requests.
		 */
		if (type == T_AXFR) {
			ns_debug(ns_log_default, 1,	
				 "T_AXFR refused: no data");
			return (Refuse);
		}
		goto fetchns;
	}

	if (hp->rcode == NOERROR_NODATA) {
		hp->rcode = NOERROR;
#ifdef RETURNSOA
		if (count) {
		        *cpp += n;
			*buflenp -= n;
			*msglenp += n;
			hp->nscount = htons(count);
		}
#endif
		founddata = 1;
		return (Finish);
	}

	*cpp += n;
	*buflenp -= n;
	*msglenp += n;
	hp->ancount = htons(ntohs(hp->ancount) + (u_int16_t)count);
	if (fname != dname && type != T_CNAME && type != T_ANY) {
		if (cname++ >= MAXCNAMES) {
			ns_debug(ns_log_default, 3,
				 "resp: leaving, MAXCNAMES exceeded");
			hp->rcode = SERVFAIL;
			return (Finish);
		}
		goto try_again;
	}
	founddata = 1;
	ns_debug(ns_log_default, 3,
		 "req: foundname=%d, count=%d, founddata=%d, cname=%d",
		 foundname, count, founddata, cname);

	if (type == T_AXFR) {
		ns_xfr(qsp, np, zone, class, type, hp->opcode, ntohs(hp->id));
		return (Return);
	}

 fetchns:
	/*
	 * If we're already out of room in the response, we're done.
	 */
	if (hp->tc)
		return (Finish);

	/*
 	 * Look for name servers to refer to and fill in the authority
 	 * section or record the address for forwarding the query
 	 * (recursion desired).
 	 */
	free_nsp(nsp);
	nsp[0] = NULL;
	count = 0;
	switch (findns(&np, class, nsp, &count, 0)) {
	case NXDOMAIN:
		/* We are authoritative for this np. */
		if (!foundname)
			hp->rcode = NXDOMAIN;
		ns_debug(ns_log_default, 3, "req: leaving (%s, rcode %d)",
			 dname, hp->rcode);
		if (class != C_ANY) {
			hp->aa = 1;
			if (np && (!foundname || !founddata)) {
				n = doaddauth(hp, *cpp, *buflenp, np, nsp[0]);
				*cpp += n;
				*buflenp -= n;
#ifdef ADDAUTH
			} else if (ntohs(hp->ancount) != 0) {
				/* don't add NS records for NOERROR NODATA
				   as some servers can get confused */
				free_nsp(nsp);
				switch (findns(&np, class, nsp, &count, 1)) {
				case NXDOMAIN:
				case SERVFAIL:
					break;
				default:
					if (np &&
					    (type != T_NS || np != anp)
					    ) {
						n = add_data(np, nsp, *cpp,
							     *buflenp, &count);
						if (n < 0) {
							hp->tc = 1;
							n = (-n);
						}
						*cpp += n;
						*buflenp -= n;
						hp->nscount = 
							htons((u_int16_t)
							      count);
					}
				}
#endif /*ADDAUTH*/
			}
		}
		free_nsp(nsp);
		return (Finish);

	case SERVFAIL:
		/* We're authoritative but the zone isn't loaded. */
		if (!founddata &&
		    !(NS_OPTION_P(OPTION_FORWARD_ONLY) && 
		      server_options->fwdtab)) {
			hp->rcode = SERVFAIL;
			free_nsp(nsp);
			return (Finish);
		}
	}

	/*
	 *  If we successfully found the answer in the cache,
	 *  or this is not a recursive query, or we are purposely
	 *  never recursing, then add the nameserver references
	 *  ("authority section") here and we're done.
	 */
	if (founddata || !hp->rd || NS_OPTION_P(OPTION_NORECURSE)) {
		/*
		 * If the qtype was NS, and the np of the authority is
		 * the same as the np of the data, we don't need to add
		 * another copy of the answer here in the authority
		 * section.
		 */
		if (!founddata || type != T_NS || anp != np) {
			n = add_data(np, nsp, *cpp, *buflenp, &count);
			if (n < 0) {
				hp->tc = 1;
				n = (-n);
			}
			*cpp += n;
			*buflenp -= n;
			hp->nscount = htons((u_int16_t)count);
		}
		free_nsp(nsp);

		/* Our caller will handle the Additional section. */
		return (Finish);
	}

	/*
	 *  At this point, we don't have the answer, but we do
	 *  have some NS's to try.  If the user would like us
	 *  to recurse, create the initial query.  If a cname
	 *  is involved, we need to build a new query and save
	 *  the old one in cmsg/cmsglen.
	 */
	if (cname) {
		omsg = (u_char *)memget((unsigned) *msglenp);
		if (omsg == NULL) {
			ns_info(ns_log_default, "ns_req: Out Of Memory");
			hp->rcode = SERVFAIL;
			free_nsp(nsp);
			return (Finish);
		}
		id = hp->id;
		omsglen = *msglenp;
		memcpy(omsg, msg, omsglen);
		n = res_mkquery(QUERY, dname, class, type,
				NULL, 0, NULL, msg,
				*msglenp + *buflenp);
		if (n < 0) {
			ns_info(ns_log_default, "res_mkquery(%s) failed",
				dname);
			hp->rcode = SERVFAIL;
			free_nsp(nsp);
			return (Finish);
		}
		*msglenp = n;
	}
	n = ns_forw(nsp, msg, *msglenp, from, qsp, dfd, &qp,
		    dname, class, type, np, 0);
	if (n != FW_OK && cname)
		memput(omsg, omsglen);
	switch (n) {
	case FW_OK:
		if (cname) {
			qp->q_cname = cname;
			qp->q_cmsg = omsg;
			qp->q_cmsglen = omsglen;
			qp->q_id = id;
		}
		break;
	case FW_DUP:
		break;		/* Duplicate request dropped */
	case FW_NOSERVER:
		/* 
		 * Don't go into an infinite loop if 
		 * the admin gave root NS records in the cache
		 * file without giving address records
		 * for the root servers.
		 */
		if (np) {
			if (NAME(*np)[0] == '\0') {
				ns_notice(ns_log_default,
					"ns_req: no address for root server");
				hp->rcode = SERVFAIL;
				free_nsp(nsp);
				return (Finish);
			}
			for (dp = np->n_data; dp ; dp = dp->d_next)
				if (dp->d_zone && match(dp, class, T_NS))
					break;
			if (dp) {
				/*
				 * we know the child zone exists but are
				 * missing glue.
				 *
				 * nslookup has called sysquery() to get the
				 * missing glue.
				 *
				 * for UDP, drop the response and let the
				 * client retry.  for TCP, we should probably
				 * (XXX) hold open the TCP connection for a
				 * while in case the sysquery() comes back
				 * soon.  meanwhile we SERVFAIL.
				 */
				if (qsp)
					goto do_servfail;
				break;
			}
			np = np_parent(np);
		}
		goto fetchns;	/* Try again. */
	case FW_SERVFAIL:
 do_servfail:
		hp->rcode = SERVFAIL;
		free_nsp(nsp);
		return (Finish);
	}
	free_nsp(nsp);
	return (Return);
}

static enum req_action
req_iquery(HEADER *hp, u_char **cpp, u_char *eom, int *buflenp,
	   u_char *msg, struct sockaddr_in from)
{
	int dlen, alen, n, type, class, count;
	char dnbuf[MAXDNAME], anbuf[PACKETSZ], *data, *fname;

	nameserIncr(from.sin_addr, nssRcvdIQ);

	if (ntohs(hp->ancount) != 1
	    || ntohs(hp->qdcount) != 0
	    || ntohs(hp->nscount) != 0
	    || ntohs(hp->arcount) != 0) {
		ns_debug(ns_log_default, 1,
			 "FORMERR IQuery header counts wrong");
		hp->qdcount = htons(0);
		hp->ancount = htons(0);
		hp->nscount = htons(0);
		hp->arcount = htons(0);
		hp->rcode = FORMERR;
		return (Finish);
	}

	/*
	 * Skip domain name, get class, and type.
	 */
	if ((n = dn_skipname(*cpp, eom)) < 0) {
		ns_debug(ns_log_default, 1,
			 "FORMERR IQuery packet name problem");
		hp->rcode = FORMERR;
		return (Finish);
	}
	*cpp += n;
	if (*cpp + 3 * INT16SZ + INT32SZ > eom) {
		ns_debug(ns_log_default, 1,
			 "FORMERR IQuery message too short");
		hp->rcode = FORMERR;
		return (Finish);
	}
	GETSHORT(type, *cpp);
	GETSHORT(class, *cpp);
	*cpp += INT32SZ;	/* ttl */
	GETSHORT(dlen, *cpp);
	*cpp += dlen;
	if (*cpp != eom) {
		ns_debug(ns_log_default, 1,
			 "FORMERR IQuery message length off");
		hp->rcode = FORMERR;
		return (Finish);
	}

	/*
	 * not all inverse queries are handled.
	 */
	switch (type) {
	case T_A:
		if (!NS_OPTION_P(OPTION_FAKE_IQUERY) || dlen != INT32SZ)
			return (Refuse);
		break;
	default:
		return (Refuse);
	}
	ns_debug(ns_log_default, 1,
		 "req: IQuery class %d type %d", class, type);

	fname = (char *)msg + HFIXEDSZ;
	alen = (char *)*cpp - fname;
	if ((size_t)alen > sizeof anbuf)
		return (Refuse);
	memcpy(anbuf, fname, alen);
	data = anbuf + alen - dlen;
	*cpp = (u_char *)fname;
	*buflenp -= HFIXEDSZ;
	count = 0;

#ifdef QRYLOG
	if (qrylog) {
		char tmp[sizeof "255.255.255.255"];

		strcpy(tmp, inet_ntoa(from.sin_addr));
		ns_info(ns_log_queries, "XX /%s/%s/-%s",
			tmp, inet_ntoa(ina_get((u_char *)data)),
			p_type(type));
	}
#endif /*QRYLOG*/

	/*
	 * We can only get here if the option "fake-iquery" is on in the boot
	 * file.
	 *
	 * What we do here is send back a bogus response of "[dottedquad]".
	 * A better strategy would be to turn this into a PTR query, but that
	 * would legitimize inverse queries in a way they do not deserve.
	 */
	sprintf(dnbuf, "[%s]", inet_ntoa(ina_get((u_char *)data)));
	*buflenp -= QFIXEDSZ;
	n = dn_comp(dnbuf, *cpp, *buflenp, NULL, NULL);
	if (n < 0) {
		hp->tc = 1;
		return (Finish);
	}
	*cpp += n;
	PUTSHORT((u_int16_t)type, *cpp);
	PUTSHORT((u_int16_t)class, *cpp);
	*buflenp -= n;
	count++;

	ns_debug(ns_log_default, 1, "req: IQuery %d records", count);
	hp->qdcount = htons((u_int16_t)count);
	if (alen > *buflenp) {
		hp->tc = 1;
		return (Finish);
	}
	memcpy(*cpp, anbuf, alen);
	*cpp += alen;
	return (Finish);
}

/*
 *  Test a datum for validity and return non-zero if it is out of date.
 */
int
stale(struct databuf *dp) {
	struct zoneinfo *zp = &zones[dp->d_zone];

	switch (zp->z_type) {

	case z_master:
		return (0);

#ifdef STUBS
	case z_stub:
		/* root stub zones have DB_F_HINT set */
		if (dp->d_flags & DB_F_HINT)
			return (0);
		/* FALLTROUGH */
#endif
	case z_slave:
		/*
		 * Check to see whether a secondary zone has expired or
		 * time warped; if so clear authority flag for zone,
		 * schedule the zone for immediate maintenance, and
		 * return true.
		 */
		if ((int32_t)(tt.tv_sec - zp->z_lastupdate)
		    > (int32_t)zp->z_expire) {
			ns_debug(ns_log_default, 1,
				 "stale: secondary zone %s expired",
				zp->z_origin);
			if (!haveComplained((u_long)zp, (u_long)stale)) {
				ns_notice(ns_log_default,
					  "secondary zone \"%s\" expired",
					  zp->z_origin);
			}
			zp->z_flags &= ~Z_AUTH;
			if (!(zp->z_flags & (Z_QSERIAL|Z_XFER_RUNNING))) {
				zp->z_time = tt.tv_sec;
				sched_zone_maint(zp);
			}
			return (1);
		}
		if (zp->z_lastupdate > tt.tv_sec) {
			if (!haveComplained((u_long)zp, (u_long)stale)) {
				ns_notice(ns_log_default,
					  "secondary zone \"%s\" time warp",
					  zp->z_origin);
			}
			zp->z_flags &= ~Z_AUTH;
			if (!(zp->z_flags & (Z_QSERIAL|Z_XFER_RUNNING))) {
				zp->z_time = tt.tv_sec;
				sched_zone_maint(zp);
			}
			return (1);
		}
		return (0);

	case z_hint:
		if (dp->d_flags & DB_F_HINT ||
		    dp->d_ttl >= (u_int32_t)tt.tv_sec)
			return (0);
		ns_debug(ns_log_default, 3, "stale: ttl %d %ld (x%lx)",
			 dp->d_ttl, (long)(dp->d_ttl - tt.tv_sec),
			 (u_long)dp->d_flags);
		return (1);

	default:
		/* FALLTHROUGH */ ;

	}
	panic("stale: impossible condition", NULL);
	/* NOTREACHED */
	return (0);	/* Make gcc happy. */
}

/*
 * Copy databuf into a resource record for replies.
 * Return size of RR if OK, -1 if buffer is full.
 */
int
make_rr(const char *name, struct databuf *dp, u_char *buf,
	int buflen, int doadd, u_char **comp_ptrs, u_char **edp)
{
	u_char *cp;
	u_char *cp1, *sp;
	struct zoneinfo *zp;
	int32_t n;
	int16_t type = dp->d_type;
	u_int32_t ttl;
#ifdef BIND_UPDATE
	u_int32_t serial;
#endif

	ns_debug(ns_log_default, 5,
		 "make_rr(%s, %lx, %lx, %d, %d) %d zone %d ttl %lu",
		 name, (u_long)dp, (u_long)buf,
		 buflen, doadd, dp->d_size, dp->d_zone, (u_long)dp->d_ttl);

	if (dp->d_rcode
#ifdef RETURNSOA
	    && dp->d_size == 0
#endif
	    )
		panic("make_rr: impossible d_rcode value", NULL);

	zp = &zones[dp->d_zone];
	/* check for outdated RR before updating comp_ptrs[] by dn_comp() */
	if (zp->z_type == Z_CACHE) {
		if ((dp->d_flags & DB_F_HINT) != 0
		    || dp->d_ttl < (u_int32_t)tt.tv_sec) {
			ttl = 0;
		} else
			ttl = dp->d_ttl - (u_int32_t) tt.tv_sec;
	} else {
		if (dp->d_ttl != USE_MINIMUM)
			ttl = dp->d_ttl;
		else
			ttl = zp->z_minimum;		/* really default */
	}

	buflen -= RRFIXEDSZ;
	if (buflen < 0)
		return (-1);
#ifdef RETURNSOA
	if (dp->d_rcode) {
		name = (char *)dp->d_data;
		name += strlen(name) +1;
		name += strlen(name) +1;
		name += 5 * INT32SZ;
		type = T_SOA;
	}
#endif
	if ((n = dn_comp(name, buf, buflen, comp_ptrs, edp)) < 0)
		return (-1);
	cp = buf + n;
	buflen -= n;
	if (buflen < 0)
		return (-1);
	PUTSHORT((u_int16_t)type, cp);
	PUTSHORT((u_int16_t)dp->d_class, cp);
	PUTLONG(ttl, cp);
	sp = cp;
	cp += INT16SZ;
	switch (type) {
	case T_CNAME:
	case T_MG:
	case T_MR:
	case T_PTR:
		n = dn_comp((char *)dp->d_data, cp, buflen, comp_ptrs, edp);
		if (n < 0)
			return (-1);
		PUTSHORT((u_int16_t)n, sp);
		cp += n;
		break;

	case T_MB:
	case T_NS:
		/* Store domain name in answer */
		n = dn_comp((char *)dp->d_data, cp, buflen, comp_ptrs, edp);
		if (n < 0)
			return (-1);
		PUTSHORT((u_int16_t)n, sp);
		cp += n;
		if (doadd)
			addname((char*)dp->d_data, name,
				type, dp->d_class);
		break;

	case T_SOA:
	case T_MINFO:
	case T_RP:
		cp1 = dp->d_data;
		n = dn_comp((char *)cp1, cp, buflen, comp_ptrs, edp);
		if (n < 0)
			return (-1);
		cp += n;
		buflen -= type == T_SOA ? n + 5 * INT32SZ : n;
		if (buflen < 0)
			return (-1);
		cp1 += strlen((char *)cp1) + 1;
		n = dn_comp((char *)cp1, cp, buflen, comp_ptrs, edp);
		if (n < 0)
			return (-1);
		cp += n;
		if (type == T_SOA) {
			cp1 += strlen((char *)cp1) + 1;
#ifdef BIND_UPDATE
			if (zp->z_flags & Z_NEED_SOAUPDATE)
				if (incr_serial(zp) < 0)
					ns_error(ns_log_default,
			   "error updating serial number for %s from %d",
						 zp->z_origin, zp->z_serial);
#endif
			n = 5 * INT32SZ;
			memcpy(cp, cp1, n);
			cp += n;
		}
		n = (u_int16_t)((cp - sp) - INT16SZ);
		PUTSHORT((u_int16_t)n, sp);
		break;

	case T_NAPTR:
		/* cp1 == our data/ cp == data of RR */
		cp1 = dp->d_data;

 		/* copy order */
		buflen -= INT16SZ;
		if (buflen < 0)
			return (-1);
 		memcpy(cp, cp1, INT16SZ);
 		cp += INT16SZ;
 		cp1 += INT16SZ;
		n = (u_int16_t)((cp - sp) - INT16SZ);
		ns_debug(ns_log_default, 1, "current size n = %u", n);

		/* copy preference */
		buflen -= INT16SZ;
		if (buflen < 0)
			return (-1);
		memcpy(cp, cp1, INT16SZ);
		cp += INT16SZ;
		cp1 += INT16SZ;
		n = (u_int16_t)((cp - sp) - INT16SZ);
		ns_debug(ns_log_default, 1, "current size n = %u", n);

		/* Flags */
		n = *cp1++;
		ns_debug(ns_log_default, 1, "size of n at flags = %d", n);
		buflen -= n + 1;
		if (buflen < 0)
			return (-1);
		*cp++ = n;
		memcpy(cp, cp1, n);
		cp += n;
		cp1 += n;
		n = (u_int16_t)((cp - sp) - INT16SZ);
		ns_debug(ns_log_default, 1, "current size n = %u", n);
		
		/* Service */
		n = *cp1++;
		buflen -= n + 1;
		if (buflen < 0)
			return (-1);
		*cp++ = n;
		memcpy(cp, cp1, n);
		cp += n;
		cp1 += n;
		n = (u_int16_t)((cp - sp) - INT16SZ);
		ns_debug(ns_log_default, 1, "current size n = %u", n);

		/* Regexp */
		n = *cp1++;
		buflen -= n + 1;
		if (buflen < 0)
			return (-1);
		*cp++ = n;
		memcpy(cp, cp1, n);
		cp += n;
		cp1 += n;
		n = (u_int16_t)((cp - sp) - INT16SZ);
		ns_debug(ns_log_default, 1, "current size n = %u", n);

		/* Replacement */
		ns_debug(ns_log_default, 1, "Replacement = %s", cp1);
		n = dn_comp((char *)cp1, cp, buflen, dnptrs, edp);
		ns_debug(ns_log_default, 1, "dn_comp's n = %u", n);
		if (n < 0)
			return (-1);
		cp += n;

		/* save data length */
		n = (u_int16_t)((cp - sp) - INT16SZ);
		ns_debug(ns_log_default, 1, "saved size n = %u", n);
		PUTSHORT((u_int16_t)n, sp);

		break;

	case T_MX:
	case T_AFSDB:
	case T_RT:
	case T_SRV:
		/* cp1 == our data/ cp == data of RR */
		cp1 = dp->d_data;

 		if ((buflen -= INT16SZ) < 0)
			return (-1);

 		/* copy preference */
 		memcpy(cp, cp1, INT16SZ);
 		cp += INT16SZ;
 		cp1 += INT16SZ;

		if (type == T_SRV) {
			buflen -= INT16SZ*2;
			if (buflen < 0)
				return (-1);
			memcpy(cp, cp1, INT16SZ*2);
			cp += INT16SZ*2;
			cp1 += INT16SZ*2;
		}

		n = dn_comp((char *)cp1, cp, buflen, comp_ptrs, edp);
		if (n < 0)
			return (-1);
		cp += n;

		/* save data length */
		n = (u_int16_t)((cp - sp) - INT16SZ);
		PUTSHORT((u_int16_t)n, sp);
		if (doadd)
			addname((char*)cp1, name, type, dp->d_class);
		break;

	case T_PX:
		cp1 = dp->d_data;

		if ((buflen -= INT16SZ) < 0)
			return (-1);

		/* copy preference */
		memcpy(cp, cp1, INT16SZ);
		cp += INT16SZ;
		cp1 += INT16SZ;

		n = dn_comp((char *)cp1, cp, buflen, comp_ptrs, edp);
		if (n < 0)
			return (-1);
		cp += n;
		buflen -= n;
		cp1 += strlen((char *)cp1) + 1;
		n = dn_comp((char *)cp1, cp, buflen, comp_ptrs, edp);
		if (n < 0)
			return (-1);
		cp += n;

		/* save data length */
		n = (u_int16_t)((cp - sp) - INT16SZ);
		PUTSHORT((u_int16_t)n, sp);
		break;

	case T_SIG:
		/* cp1 == our data; cp == data of target RR */
		cp1 = dp->d_data;

		/* first just copy over the type_covered, algorithm, */
		/* labels, orig ttl, two timestamps, and the footprint */
		if ((dp->d_size - 18) > buflen)
			return (-1);  /* out of room! */
		memcpy(cp, cp1, 18);
		cp  += 18;
		cp1 += 18;
		buflen -= 18;

		/* then the signer's name */
		n = dn_comp((char *)cp1, cp, buflen, NULL, NULL);
		if (n < 0)
			return (-1);
		cp += n;
		buflen -= n;
		cp1 += strlen((char*)cp1)+1;

		/* finally, we copy over the variable-length signature */
		n = dp->d_size - (u_int16_t)((cp1 - dp->d_data));
		if (n > buflen)
			return (-1);  /* out of room! */
		memcpy(cp, cp1, n);
		cp += n;
		
  		/* save data length & return */
		n = (u_int16_t)((cp - sp) - INT16SZ);
  		PUTSHORT((u_int16_t)n, sp);
		break;

	default:
		if (dp->d_size > buflen)
			return (-1);
		memcpy(cp, dp->d_data, dp->d_size);
		PUTSHORT((u_int16_t)dp->d_size, sp);
		cp += dp->d_size;
	}
	return (cp - buf);
}

static void
addname(const char *dname, const char *rname,
	u_int16_t rtype, u_int16_t class)
{
	struct addinfo *ap;
	int n;

	for (ap = addinfo, n = addcount; --n >= 0; ap++)
		if (strcasecmp(ap->a_dname, dname) == 0)
			return;

	/* add domain name to additional section */
	if (addcount < NADDRECS) {
		addcount++;
		ap->a_dname = savestr(dname, 1);
		ap->a_rname = savestr(rname, 1);
		ap->a_rtype = rtype;
		ap->a_class = class;
	}
}

/*
 * Lookup addresses for names in addinfo and put into the message's
 * additional section.
 */
int
doaddinfo(HEADER *hp, u_char *msg, int msglen) {
	struct namebuf *np;
	struct databuf *dp;
	struct addinfo *ap;
	u_char *cp;
	struct hashbuf *htp;
	const char *fname;
	int n, count;

	if (!addcount)
		return (0);

	ns_debug(ns_log_default, 3, "doaddinfo() addcount = %d", addcount);

	if (hp->tc) {
		ns_debug(ns_log_default, 4,
			 "doaddinfo(): tc already set, bailing");
		return (0);
	}

	count = 0;
	cp = msg;
	for (ap = addinfo; --addcount >= 0; ap++) {
		int     foundany = 0,
			foundcname = 0,
			save_count = count,
			save_msglen = msglen;
		u_char	*save_cp = cp;

		ns_debug(ns_log_default, 3,
			 "do additional \"%s\" (from \"%s\")",
			 ap->a_dname, ap->a_rname);
		htp = hashtab;	/* because "nlookup" stomps on arg. */
		np = nlookup(ap->a_dname, &htp, &fname, 0);
		if (np == NULL || fname != ap->a_dname)
			goto next_rr;
		ns_debug(ns_log_default, 3, "found it");
		/* look for the data */
		delete_stale(np);
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			if (dp->d_rcode)
				continue;
			if (match(dp, (int)ap->a_class, T_CNAME) ||
			    match(dp, C_IN, T_CNAME)) {
				foundcname++;
				break;
			}
			if (!match(dp, (int)ap->a_class, T_A) &&
			    !match(dp, C_IN, T_A) &&
			    !match(dp, (int)ap->a_class, T_AAAA) &&
			    !match(dp, C_IN, T_AAAA)) {
				continue;
			}
			foundany++;
			/*
			 *  Should be smart and eliminate duplicate
			 *  data here.	XXX
			 */
			if ((n = make_rr(ap->a_dname, dp, cp, msglen, 0,
					 dnptrs, dnptrs_end)) < 0) {
				/* truncation in the additional-data section
				 * is not all that serious.  we do not set TC,
				 * since the answer and authority sections are
				 * OK; however, since we're not setting TC we
				 * have to make sure that none of the RR's for
				 * this name go out (!TC implies that all
				 * {name,type} appearances are complete -- and
				 * since we only do A RR's here, the name is
				 * the key).	vixie, 23apr93
				 */
				ns_debug(ns_log_default, 5,
			  "addinfo: not enough room, remaining msglen = %d",
					 save_msglen);
				cp = save_cp;
				msglen = save_msglen;
				count = save_count;
				break;
			}
			ns_debug(ns_log_default, 5,
				 "addinfo: adding address data n = %d", n);
			cp += n;
			msglen -= n;
			count++;
		}
 next_rr:
		if (!NS_OPTION_P(OPTION_NOFETCHGLUE) && 
		    !foundcname && !foundany) {
			/* ask a real server for this info */
			(void) sysquery(ap->a_dname, (int)ap->a_class, T_A,
					NULL, 0, QUERY);
		}
		if (foundcname) {
			if (!haveComplained(nhash(ap->a_dname),
					    nhash(ap->a_rname))) {
				ns_info(ns_log_cname,
					"\"%s %s %s\" points to a CNAME (%s)",
					ap->a_rname, p_class(ap->a_class),
					p_type(ap->a_rtype), ap->a_dname);
			}
		}
		freestr(ap->a_dname);
		freestr(ap->a_rname);
	}
	hp->arcount = htons((u_int16_t)count);
	return (cp - msg);
}

int
doaddauth(HEADER *hp, u_char *cp, int buflen,
	  struct namebuf *np, struct databuf *dp)
{
	char dnbuf[MAXDNAME];
	int n;

	getname(np, dnbuf, sizeof dnbuf);
	if (stale(dp)) {
		ns_debug(ns_log_default, 1,
			 "doaddauth: can't add stale '%s' (%d)",
			dnbuf, buflen);
		return (0);
	}
	n = make_rr(dnbuf, dp, cp, buflen, 1, dnptrs, dnptrs_end);
	if (n <= 0) {
		ns_debug(ns_log_default, 1,
			 "doaddauth: can't add oversize '%s' (%d) (n=%d)",
			 dnbuf, buflen, n);
		if (n < 0) {
			hp->tc = 1;
		}
		return (0);
	}
	hp->nscount = htons(ntohs(hp->nscount) + 1);
	return (n);
}

void
free_addinfo() {
	struct addinfo *ap;

	for (ap = addinfo; --addcount >= 0; ap++) {
		freestr(ap->a_dname);
		freestr(ap->a_rname);
	}
	addcount = 0;
}

void
free_nsp(struct databuf **nsp) {
	while (*nsp) {
		DRCNTDEC(*nsp);
		if ((*nsp)->d_rcnt)
			ns_debug(ns_log_default, 3, "free_nsp: %s rcnt %d",
				 (*nsp)->d_data, (*nsp)->d_rcnt);
		else {
			ns_debug(ns_log_default, 3,
				 "free_nsp: %s rcnt %d delayed",
				 (*nsp)->d_data, (*nsp)->d_rcnt);
			db_freedata(*nsp);	/* delayed free */
		}
		*nsp++ = NULL;
	}
}

static void
copyCharString(u_char **dst, const char *src) {
	size_t len = strlen(src) & 0xff;
	*(*dst)++ = (u_char) len;
	memcpy(*dst, src, len);
	*dst += len;
}
