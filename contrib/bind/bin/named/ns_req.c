#if !defined(lint) && !defined(SABER)
static const char sccsid[] = "@(#)ns_req.c	4.47 (Berkeley) 7/1/91";
static const char rcsid[] = "$Id: ns_req.c,v 8.118 2000/07/17 07:57:56 vixie Exp $";
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
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/un.h>

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

#include <isc/dst.h>

#include "port_after.h"

#include "named.h"

struct addinfo {
	char		*a_dname;		/* domain name */
	char		*a_rname;		/* referred by */
	u_int16_t	a_rtype;		/* referred by */
	u_int16_t	a_type;			/* type for data */
	u_int16_t	a_class;		/* class for data */
};


#ifndef BIND_UPDATE
enum req_action { Finish, Refuse, Return };
#endif

static struct addinfo	addinfo[NADDRECS];
static void		addname(const char *, const char *,
				u_int16_t, u_int16_t, u_int16_t);
static void		copyCharString(u_char **, const char *);

static enum req_action	req_query(HEADER *hp, u_char **cpp, u_char *eom,
				  struct qstream *qsp,
				  int *buflenp, int *msglenp,
				  u_char *msg, int dfd, int *ra,
				  struct sockaddr_in from,
				  struct tsig_record *in_tsig);

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
	int n, ra, has_tsig, msglen_orig, tsig_size, siglen, sig2len;
	u_char *tsigstart;
	u_char sig[TSIG_SIG_SIZE], sig2[TSIG_SIG_SIZE];
	struct tsig_record *in_tsig = NULL;
	int error = NOERROR;
	DST_KEY *key;
	time_t tsig_time;

#ifdef DEBUG
	if (debug > 3) {
		ns_debug(ns_log_packet, 3, "ns_req(from %s)", sin_ntoa(from));
		fp_nquery(msg, msglen, log_get_stream(packet_channel));
	}
#endif
	msglen_orig = msglen;
	siglen = sizeof(sig);
	
	tsigstart = ns_find_tsig(msg, msg + msglen);
	if (tsigstart == NULL)
		has_tsig = 0;
	else {
		char buf[MAXDNAME];

		has_tsig = 1;
		n = dn_expand(msg, msg + msglen, tsigstart, buf, sizeof(buf));
		if (n < 0) {
			ns_debug(ns_log_default, 1,
				 "ns_req: bad TSIG key name",
				 buf);
			key = NULL;
		}
		key = find_key(buf, NULL);
		if (key == NULL) {
			error = ns_r_badkey;
			hp->rcode = ns_r_notauth;
			ns_debug(ns_log_default, 1,
				 "ns_req: TSIG verify failed - unknown key %s",
				 buf);
		}
	}
	if (has_tsig && key != NULL) {
		n = ns_verify(msg, &msglen, key, NULL, 0, sig, &siglen, 
			      &tsig_time, 0);
		if (n != 0) {
			hp->rcode = ns_r_notauth;
			/* A query should never have an error code set */
			if (n == ns_r_badsig || n == ns_r_badkey ||
			    n == ns_r_badtime) {
				ns_debug(ns_log_default, 1,
		"ns_req: TSIG verify failed - query had error %s (%d) set",
				p_rcode(n), n);
				error = n;
				action = Return;
			}
			/* If there's a processing error just respond */
			else if (n == -ns_r_badsig || n == -ns_r_badkey ||
				 n == -ns_r_badtime) {
				n = -n;
				ns_debug(ns_log_default, 1,
					 "ns_req: TSIG verify failed - %s (%d)",
					 p_rcode(n), n);
				error = n;
			} else {
				ns_debug(ns_log_default, 1,
					"ns_req: TSIG verify failed - FORMERR");
				error = ns_r_formerr;
			}
			action = Finish;
		}
		in_tsig = memget(sizeof(struct tsig_record));
		if (in_tsig == NULL)
			ns_panic(ns_log_default, 1, "memget failed");
		in_tsig->key = key;
		in_tsig->siglen = siglen;
		memcpy(in_tsig->sig, sig, siglen);
		tsig_size = msglen_orig - msglen;
	} else if (has_tsig) {
		action = Finish;
		in_tsig = memget(sizeof(struct tsig_record));
		if (in_tsig == NULL)
			ns_panic(ns_log_default, 1, "memget failed");
		in_tsig->key = NULL;
		in_tsig->siglen = 0;
		tsig_size = msg + msglen - tsigstart;
		msglen = tsigstart - msg;
	}

	/* Hash some stuff so it's nice and random */
	nsid_hash((u_char *)&tt, sizeof(tt));
	nsid_hash(msg, (msglen > 512) ? 512 : msglen);

	/*
	 * It's not a response so these bits have no business
	 * being set. will later simplify work if we can
	 * safely assume these are always 0 when a query
	 * comes in.
	 */
	hp->aa = hp->ra = 0;
	ra = (NS_OPTION_P(OPTION_NORECURSE) == 0);

	if (error == NOERROR)
		hp->rcode = ns_r_noerror;
	cp = msg + HFIXEDSZ;
	eom = msg + msglen;
	buflen -= HFIXEDSZ;

	free_addinfo();	/* sets addcount to zero */
	dnptrs[0] = NULL;

	if (error == NOERROR) {
		switch (hp->opcode) {
		case ns_o_query:
			action = req_query(hp, &cp, eom, qsp,
					   &buflen, &msglen,
					   msg, dfd, &ra, from, in_tsig);
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
			action = req_update(hp, cp, eom, msg, qsp, dfd, from,
					    in_tsig);
			break;
#endif /* BIND_UPDATE */

		default:
			ns_debug(ns_log_default, 1,
				 "ns_req: Opcode %d not implemented",
				 hp->opcode);
			/* XXX - should syslog, limited by haveComplained */
			hp->qdcount = htons(0);
			hp->ancount = htons(0);
			hp->nscount = htons(0);
			hp->arcount = htons(0);
			hp->rcode = ns_r_notimpl;
			action = Finish;
		}
	}

	if (in_tsig != NULL) {
		memput(in_tsig, sizeof(struct tsig_record));
		in_tsig = NULL;
	}

	/*
	 * Vector via internal opcode.
	 */
	switch (action) {
	case Return:
		return;
	case Refuse:
		hp->rcode = ns_r_refused;
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
	 * Apply final polish.
	 */
	hp->qr = 1;		/* set Response flag */
	hp->ra = ra;		/* init above, may be modified by req_query */

	if (!hp->tc && has_tsig > 0 && buflen < tsig_size)
		hp->tc = 1;

	/*
	 * If there was a format error, then we don't know what the msg has.
	 */
	if (hp->rcode == ns_r_formerr) {
		hp->qdcount = htons(0);
		hp->ancount = htons(0);
		hp->nscount = htons(0);
		hp->arcount = htons(0);
		cp = msg + HFIXEDSZ;
	}

	/*
	 * If the query had a TSIG and the message is truncated or there was
	 * a TSIG error, build a new message with no data and a TSIG.
	 */
	if ((hp->tc || error != NOERROR) && has_tsig > 0) {
		hp->ancount = htons(0);
		hp->nscount = htons(0);
		hp->arcount = htons(0);
		cp = msg + HFIXEDSZ;
		cp += ns_skiprr(cp, msg + msglen, ns_s_qd, ntohs(hp->qdcount));
		sig2len = sizeof(sig2);
		buflen += (msglen - (cp - msg));
		msglen = cp - msg;
		n = ns_sign(msg, &msglen, msglen + buflen, error, key,
			    sig, siglen, sig2, &sig2len, tsig_time);
		if (n != 0) {
			INSIST(0);
		}
		cp = msg + msglen;
		
	}
	/* Either the message is not truncated or there was no TSIG */
	else {
		if (has_tsig > 0)
			buflen -= tsig_size;
		n = doaddinfo(hp, cp, buflen);
		cp += n;
		buflen -= n;
		if (has_tsig > 0) {
			buflen += tsig_size;
			sig2len = sizeof(sig2);
			msglen = cp - msg;
			n = ns_sign(msg, &msglen, msglen + buflen, error, key,
				    sig, siglen, sig2, &sig2len, tsig_time);
			if (n != 0) {
				INSIST(0);
			}
			cp = msg + msglen;
		}
	}

#ifdef DEBUG
	ns_debug(ns_log_default, 1,
		 "ns_req: answer -> %s fd=%d id=%d size=%d rc=%d",
		 sin_ntoa(from), (qsp == NULL) ? dfd : qsp->s_rfd,
		 ntohs(hp->id), cp - msg, hp->rcode);
	if (debug >= 10)
		res_pquery(&res, msg, cp - msg,
			   log_get_stream(packet_channel));
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
		if (hp->rcode == ns_r_nxdomain) 
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
	struct zoneinfo *zp;

	/* valid notify's have one question */
	if (ntohs(hp->qdcount) != 1) {
		ns_debug(ns_log_notify, 1,
			 "FORMERR Notify header counts wrong");
		hp->rcode = ns_r_formerr;
		return (Finish);
	}

	/* valid notify's are authoritative */
	if (!hp->aa) {
		ns_debug(ns_log_notify, 1,
			 "FORMERR Notify request without AA");
		hp->rcode = ns_r_formerr;
		return (Finish);
	}

	n = dn_expand(msg, eom, *cpp, dnbuf, sizeof dnbuf);
	if (n < 0) {
		ns_debug(ns_log_notify, 1,
			 "FORMERR Query expand name failed");
		hp->rcode = ns_r_formerr;
		return (Finish);
	}
	*cpp += n;
	if (*cpp + 2 * INT16SZ > eom) {
		ns_debug(ns_log_notify, 1,
			 "FORMERR notify too short");
		hp->rcode = ns_r_formerr;
		return (Finish);
	}
	GETSHORT(type, *cpp);
	GETSHORT(class, *cpp);
	ns_info(ns_log_notify, "rcvd NOTIFY(%s, %s, %s) from %s",
		dnbuf, p_class(class), p_type(type), sin_ntoa(from));
	/* XXX - when answers are allowed, we'll need to do compression
	 * correctly here, and we will need to check for packet underflow.
	 */
	/* Find the zone this NOTIFY refers to. */
	zp = find_auth_zone(dnbuf, class);
	if (zp == NULL) {
		ns_info(ns_log_notify,
			"rcvd NOTIFY for \"%s\", name not one of our zones",
			dnbuf);
		hp->rcode = ns_r_servfail;
		return (Finish);
	}
	/* Access control. */
	switch (type) {
	case T_SOA:
		if (zp->z_type != z_slave) {
			/*
			 * This can come if a user did an AXFR of some zone
			 * somewhere and that zone's server now wants to
			 * tell us that the SOA has changed.  AXFR's always
			 * come from nonpriv ports so it isn't possible to
			 * know whether it was the server or just "dig".
			 * This condition can be avoided by using secure
			 * zones since that way only real secondaries can
			 * AXFR from you.
			 */
			ns_info(ns_log_notify,
			    "NOTIFY(SOA) for non-secondary name (%s), from %s",
				dnbuf, sin_ntoa(from));
			goto refuse;
		}
		if (ns_samename(dnbuf, zp->z_origin) != 1) {
			ns_info(ns_log_notify,
				"NOTIFY(SOA) for non-origin (%s), from %s",
				dnbuf, sin_ntoa(from));
			goto refuse;
		}
		if (findZonePri(zp, from) == -1) {
			ns_debug(ns_log_notify, 1,
			"NOTIFY(SOA) from non-master server (zone %s), from %s",
				zp->z_origin, sin_ntoa(from));
			goto refuse;
		}
		break;
	default:
		/* No access requirements defined for other types. */
		break;
	}
	/* The work occurs here. */
	switch (type) {
	case T_SOA:
		if (zp->z_flags &
		    (Z_NEED_RELOAD|Z_NEED_XFER|Z_QSERIAL|Z_XFER_RUNNING)) {
			ns_info(ns_log_notify,
				"NOTIFY(SOA) for zone already xferring (%s)",
				dnbuf);
			goto noerror;
		}
		zp->z_time = tt.tv_sec;
		qserial_query(zp);
		sched_zone_maint(zp);
		break;
	default:
		/*
		 * Unimplemented, but it's not a protocol error, just
		 * something to be ignored.
		 */
		hp->rcode = ns_r_notimpl;
		return (Finish);
	}
 noerror:
	hp->rcode = ns_r_noerror;
	hp->aa = 1;
	return (Finish);
 refuse:
	hp->rcode = ns_r_refused;
	return (Finish);
}
#endif /*BIND_NOTIFY*/


static enum req_action
req_query(HEADER *hp, u_char **cpp, u_char *eom, struct qstream *qsp,
	  int *buflenp, int *msglenp, u_char *msg, int dfd, int *ra,
	  struct sockaddr_in from, struct tsig_record *in_tsig)
{
	int n, class, type, count, zone, foundname, founddata, omsglen, cname;
	int recursion_blocked_by_acl;
	u_int16_t id;
	u_int32_t serial_ixfr;
	int ixfr_found;
	int ixfr_error = 0;
	char dnbuf2[MAXDNAME];
	u_char **dpp, *omsg, *answers, *afterq;
	char dnbuf[MAXDNAME], *dname;
	const char *fname;
	struct hashbuf *htp;
	struct databuf *nsp[NSMAX];
	struct namebuf *np, *anp;
	struct qinfo *qp;
	struct zoneinfo *zp;
	struct databuf *dp;
	DST_KEY *in_key = (in_tsig != NULL) ? in_tsig->key : NULL;

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
	recursion_blocked_by_acl = 0;

	/* valid queries have one question and zero answers */
	if ((ntohs(hp->qdcount) != 1)
	    || ntohs(hp->ancount) != 0
	    || ntohs(hp->arcount) != 0) {
		ns_debug(ns_log_default, 1,
			 "FORMERR Query header counts wrong");
		hp->rcode = ns_r_formerr;
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
		hp->rcode = ns_r_formerr;
		return (Finish);
	}
	*cpp += n;
	answers = *cpp;
	if (*cpp + 2 * INT16SZ > eom) {
		ns_debug(ns_log_default, 1,
			 "FORMERR Query message length short");
		hp->rcode = ns_r_formerr;
		return (Finish);
	}
	GETSHORT(type, *cpp);
	GETSHORT(class, *cpp);
	if (*cpp < eom && type != ns_t_ixfr) {
		ns_debug(ns_log_default, 6,
			 "message length > received message");
		*msglenp = *cpp - msg;
	}

	if (((ntohs(hp->nscount) != 0) && (type != ns_t_ixfr)) ||
	    ((ntohs(hp->nscount) != 1) && (type == ns_t_ixfr)))
	{
		ns_debug(ns_log_default, 1, "FORMERR Query nscount wrong"); 
		hp->rcode = ns_r_formerr;
		return (Finish);
	} 

	afterq = *cpp;
	qtypeIncr(type);

	/*
	 * Process query.
	 */
	if (type == ns_t_ixfr) {
		ns_info(ns_log_security, "Request %s from %s",
			p_type(type), sin_ntoa(from));
		hp->nscount = htons(0);
		hp->rd = 0; /* Force IXFR queries to be non recursive. */
		n = dn_expand(msg, eom, *cpp, dnbuf2, sizeof dnbuf2);
		if (n < 0) {
			ns_debug(ns_log_default, 1,
				 "FORMERR Query expand name failed");
			hp->rcode = ns_r_formerr;
			return (Finish);
		}
		*cpp += n;
		if (*cpp + 3 * INT16SZ + INT32SZ > eom) {
			ns_debug(ns_log_default, 1,
				 "ran out of data in IXFR query");
			hp->rcode = ns_r_formerr;
			return (Finish);
		}
		GETSHORT(n, *cpp);
		if (n != ns_t_soa || ns_samename(dnbuf, dnbuf2) != 1) {
			ns_debug(ns_log_default, 1,
				 "FORMERR SOA record expected");
			hp->rcode = ns_r_formerr;
			return (Finish);
		}
		*cpp += INT32SZ + INT16SZ * 2; /* skip class, ttl, dlen */
		if (0 >= (n = dn_skipname(*cpp, eom))) {
			ns_debug(ns_log_default, 1,
				 "FORMERR Query expand name failed");
			hp->rcode = ns_r_formerr;
			return (Finish);
		}
		*cpp += n;  /* mname */
		if (0 >= (n = dn_skipname(*cpp, eom))) {
			ns_debug(ns_log_default, 1,
				 "FORMERR Query expand name failed");
			hp->rcode = ns_r_formerr;
			return (Finish);
		}
		*cpp += n;  /* rname */
		if (*cpp + 5 * INT32SZ > eom) {
			ns_debug(ns_log_default, 1,
				 "ran out of data in IXFR query");
			hp->rcode = ns_r_formerr;
			return (Finish);
		}
		GETLONG(serial_ixfr, *cpp);
		/* ignore other soa counters */
		if ((*cpp + (4 * INT32SZ)) < eom)
			ns_debug(ns_log_default, 6,
				 "ixfr: message length > received message");
		/* Reset msglenp to cover just the question. */
		*msglenp = afterq - msg;
	}
	*cpp = afterq;

	if (!ns_t_udp_p(type)) {
		/* Refuse request if not a TCP connection. */
		if (qsp == NULL) {
			ns_info(ns_log_default,
				"rejected UDP %s from %s for \"%s\"",
				p_type(type), sin_ntoa(from),
				*dnbuf ? dnbuf : ".");
			return (Refuse);
		}
		/* The position of this is subtle. */
		nameserIncr(from.sin_addr, nssRcvdAXFR);
		hp->rd = 0;		/* Recursion not possible. */
	}
	*buflenp -= (*msglenp - HFIXEDSZ);
	count = 0;
	founddata = 0;
	dname = dnbuf;
	cname = 0;

#ifdef QRYLOG
	if (qrylog) {
		ns_info(ns_log_queries, "%s/%s/%s/%s/%s",
			(hp->rd) ? "XX+" : "XX ",
			inet_ntoa(from.sin_addr), 
			(dname[0] == '\0') ? "." : dname, 
			p_type(type), p_class(class));
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
			hp->rcode = ns_r_nxdomain;
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

	ixfr_found = 0;
	if (type == ns_t_ixfr && zone != DB_Z_CACHE) {
		if (SEQ_GT(serial_ixfr, zp->z_serial))
			ixfr_found = 0;
		else {
			ixfr_error = ixfr_have_log(zp, serial_ixfr,
						   zp->z_serial);
			if (ixfr_error < 0) {
				ns_info(ns_log_security, "No %s log from %d for \"%s\"",
					p_type(type), serial_ixfr, *dname ? dname : ".");
				ns_debug(ns_log_default,
					 	1, "ixfr_have_log(%d %d) failed %d", 
			 			serial_ixfr, zp->z_serial, ixfr_error);
				ixfr_found = 0; /* Refuse IXFR and send AXFR */
			} else if (ixfr_error == 1) {
				ixfr_found = 1;
			}
		}   
		ns_debug(ns_log_default, 1, "IXFR log lowest serial: %d", 
			 zp->z_serial_ixfr_start);
	}
	/*
	 * If recursion is turned on, we need to check recursion ACL
	 * if it exists - and return result to caller.
	 */
	{
		ip_match_list recursion_acl;

		recursion_acl = server_options->recursion_acl;
		if (!NS_OPTION_P(OPTION_NORECURSE) && recursion_acl != NULL
		    && !ip_address_allowed(recursion_acl, from.sin_addr)) {
			recursion_blocked_by_acl = 1;
			*ra = 0;
		}
	}

	/*
	 * Are queries allowed from this host?
	 */
	if (!ns_t_xfr_p(type)) {
		ip_match_list query_acl;

		if (zp->z_query_acl != NULL)
			query_acl = zp->z_query_acl;
		else
			query_acl = server_options->query_acl;

		if (query_acl != NULL
		    && !ip_addr_or_key_allowed(query_acl, from.sin_addr,
					       in_key))
		{
			/*
			 * If this is *not* a zone acl and we would not
			 * have recursed and we have some answer return
			 * what we have with a referral.
			 */
			if ((zp->z_query_acl == NULL) &&
			    (!hp->rd || NS_OPTION_P(OPTION_NORECURSE) ||
			     recursion_blocked_by_acl) &&
			    (ntohs(hp->ancount) != 0)) {
				goto fetchns;
			}

			/*
			 * See if we would have made a referral from 
			 * an enclosing zone if we are actually in the
			 * cache.
			 */
			if (zp->z_type == z_cache && np != NULL) {
				struct namebuf *access_np;

				zone = DB_Z_CACHE;
				for (access_np = np; access_np != NULL;
				     access_np = np_parent(access_np)) {
					dp = access_np->n_data;
					while (dp && (dp->d_class != class ||
					       dp->d_zone == DB_Z_CACHE))
						dp = dp->d_next;
					if (dp != NULL) {
						zone = dp->d_zone;
						np = access_np;
						break;
					}
				}
				zp = &zones[zone];
				if (zp->z_type != z_cache &&
				    zp->z_query_acl != NULL &&
				    ip_addr_or_key_allowed(zp->z_query_acl,
						   from.sin_addr, in_key) &&
				    (!hp->rd || recursion_blocked_by_acl ||
				     NS_OPTION_P(OPTION_NORECURSE))) {
					goto fetchns;
				}
			}
			ns_notice(ns_log_security,
				  "denied query from %s for \"%s\"",
				  sin_ntoa(from), *dname ? dname : ".");
			nameserIncr(from.sin_addr, nssRcvdUQ);
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
		    && !ip_addr_or_key_allowed(transfer_acl, from.sin_addr,
					       in_key))
		{
			ns_notice(ns_log_security,
				  "denied %s from %s for \"%s\" (acl)",
				  p_type(type), sin_ntoa(from),
				  *dname ? dname : ".");
			nameserIncr(from.sin_addr, nssRcvdUXFR);
			return (Refuse);
		}

		/* Are we master or slave? */

		if (zp->z_type != z_master && zp->z_type != z_slave) {
			ns_notice(ns_log_security,
			 "denied %s from %s for \"%s\" (not master/slave)",
				  p_type(type), sin_ntoa(from),
				  *dname ? dname : ".");
			nameserIncr(from.sin_addr, nssRcvdUXFR);
			return (Refuse);
		}

		/* Are we authoritative? */

		if ((zp->z_flags & Z_AUTH) == 0) {
			ns_notice(ns_log_security,
			 "denied %s from %s for \"%s\" (not authoritative)",
				  p_type(type), sin_ntoa(from),
				  *dname ? dname : ".");
			nameserIncr(from.sin_addr, nssRcvdUXFR);
			return (Refuse);
		}

		/* Is the name at a zone cut? */

		if (ns_samename(zp->z_origin, dname) != 1) {
			ns_notice(ns_log_security,
			  "denied %s from %s for \"%s\" (not zone top)",
				  p_type(type), sin_ntoa(from),
				  *dname ? dname : ".");
			nameserIncr(from.sin_addr, nssRcvdUXFR);
			return (Refuse);
		}

		if (type == ns_t_ixfr) { 
		    ns_info(ns_log_security, "approved %s from %s for \"%s\"",
		    	(ixfr_found) ? p_type(type) : "IXFR/AXFR", 
			sin_ntoa(from), *dname ? dname : ".");
		} else
		    ns_info(ns_log_security, "approved %s from %s for \"%s\"",
		    	p_type(type), sin_ntoa(from), *dname ? dname : ".");
	}

	/*
	 * End Access Control Point
	 */
	/*
	 * Yow!
	 */
	if (class == ns_c_chaos && type == ns_t_txt &&
	    ns_samename(dnbuf, "VERSION.BIND") == 1) {
		u_char *tp;

		hp->ancount = htons(1);
		hp->nscount = htons(0);
		hp->arcount = htons(0);
		hp->rcode = ns_r_noerror;
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
		copyCharString(cpp, server_options->version);
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

	/* The response is authoritative until we add insecure data */
	hp->ad = 1;

	/* Look for NXDOMAIN record with appropriate class
	 * if found return immediately
	 */
	for (dp = np->n_data; dp; dp = dp->d_next) {
		if (!stale(dp) && (dp->d_rcode == ns_r_nxdomain) &&
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
					hp->rcode = ns_r_noerror;
					return (Finish);
				}
			}
#else
			count = 0;
#endif
			hp->rcode = ns_r_nxdomain;
			/* 
			 * XXX forcing AA all the time isn't right, but
			 * we have to work that way by default
			 * for compatibility with older servers.
			 */
			if (!NS_OPTION_P(OPTION_NONAUTH_NXDOMAIN))
			    hp->aa = 1;
			ns_debug(ns_log_default, 3, "NXDOMAIN aa = %d",
				 hp->aa);
			if ((count == 0) || NS_OPTION_P(OPTION_NORFC2308_TYPE1))
				return (Finish);
			founddata = 1;
			goto fetchns;
		}
	}

	/*
	 * If not NXDOMAIN, the NOERROR_NODATA record might be
	 * anywhere in the chain.  Have to go through the grind.
	 */

	n = finddata(np, class, type, hp, &dname, buflenp, &count);
	if (n == 0) {
		/*
		 * NO data available.  Refuse transfer requests, or
		 * look for better servers for other requests.
		 */
		if (ns_t_xfr_p(type)) {
			ns_debug(ns_log_default, 1,	
				 "transfer refused: no data");
			return (Refuse);
		}
		goto fetchns;
	}

	if (hp->rcode == NOERROR_NODATA) {
		hp->rcode = ns_r_noerror;
#ifdef RETURNSOA
		if (count) {
		        *cpp += n;
			*buflenp -= n;
			*msglenp += n;
			hp->nscount = htons(count);
		}
#endif
		founddata = 1;
		ns_debug(ns_log_default, 1, "count = %d", count);
		if ((count == 0) || NS_OPTION_P(OPTION_NORFC2308_TYPE1))
			return (Finish);
		goto fetchns;
	}

	*cpp += n;
	*buflenp -= n;
	*msglenp += n;
	hp->ancount = htons(ntohs(hp->ancount) + (u_int16_t)count);
	if (fname != dname && type != T_CNAME && type != T_ANY) {
		if (cname++ >= MAXCNAMES) {
			ns_debug(ns_log_default, 3,
				 "resp: leaving, MAXCNAMES exceeded");
			hp->rcode = ns_r_servfail;
			return (Finish);
		}
		goto try_again;
	}
	founddata = 1;
	ns_debug(ns_log_default, 3,
		 "req: foundname=%d, count=%d, founddata=%d, cname=%d",
		 foundname, count, founddata, cname);

	if (ns_t_xfr_p(type)) {
#ifdef BIND_UPDATE
		if ((zp->z_flags & Z_NEED_SOAUPDATE) != 0)
			if (incr_serial(zp) < 0)
				ns_error(ns_log_default,
			   "error updating serial number for %s from %d",
					 zp->z_origin, zp->z_serial);
#endif
		/*
		 * Just return SOA if "up to date".
		 */
 		if (type == ns_t_ixfr) {
 			hp->aa = 1;
 			if ((SEQ_GT(serial_ixfr, zp->z_serial) ||
			     serial_ixfr == zp->z_serial)) {
 				return (Finish);
			}
 		}

		/*
		 * We don't handle UDP based IXFR queries (yet).
		 * Tell client to retry with TCP by returning SOA.
		 */
		if (qsp == NULL)
			return (Finish);
		else {
			if (!ixfr_found && type == ns_t_ixfr) {
				qsp->flags |= STREAM_AXFRIXFR;
				hp->qdcount = htons(1);
			}
			ns_xfr(qsp, np, zone, class, type,
				       hp->opcode, ntohs(hp->id),
				       serial_ixfr, in_tsig);
		}
		return (Return);
	}

	if (count > 1 && type == T_A && !NS_OPTION_P(OPTION_NORECURSE) &&
	    hp->rd)
		sort_response(answers, *cpp, count, &from);

 fetchns:
	/*
	 * If we're already out of room in the response, we're done.
	 */
	if (hp->tc)
		return (Finish);

	if (hp->ancount == 0)
		hp->ad = 0;

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
			hp->rcode = ns_r_nxdomain;
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
		    !(NS_ZOPTION_P(zp, OPTION_FORWARD_ONLY) && 
		      NS_ZFWDTAB(zp))) {
			hp->rcode = ns_r_servfail;
			free_nsp(nsp);
			return (Finish);
		}
	}

	if (!founddata && hp->rd && recursion_blocked_by_acl) {
		ns_notice(ns_log_security,
			  "denied recursion for query from %s for %s",
			  sin_ntoa(from), *dname ? dname : ".");
		nameserIncr(from.sin_addr, nssRcvdURQ);
	}

	/*
	 *  If we successfully found the answer in the cache,
	 *  or this is not a recursive query, or we are purposely
	 *  never recursing, or recursion is prohibited by ACL, then
	 *  add the nameserver references("authority section") here
	 *  and we're done.
	 */
	if (founddata || !hp->rd || NS_OPTION_P(OPTION_NORECURSE)
	    || recursion_blocked_by_acl) {
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
			hp->nscount = htons(ntohs(hp->nscount) +
						  (u_int16_t)count);
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
			hp->rcode = ns_r_servfail;
			free_nsp(nsp);
			return (Finish);
		}
		id = hp->id;
		omsglen = *msglenp;
		memcpy(omsg, msg, omsglen);
		n = res_nmkquery(&res, QUERY, dname, class, type,
				 NULL, 0, NULL, msg,
				 *msglenp + *buflenp);
		if (n < 0) {
			ns_info(ns_log_default, "res_mkquery(%s) failed",
				dname);
			hp->rcode = ns_r_servfail;
			free_nsp(nsp);
			return (Finish);
		}
		*msglenp = n;
	}
	n = ns_forw(nsp, msg, *msglenp, from, qsp, dfd, &qp,
		    dname, class, type, np, 0, in_tsig);
	if (n != FW_OK && cname) {
		memput(omsg, omsglen);
		omsg = NULL;
	}
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
				hp->rcode = ns_r_servfail;
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
		hp->rcode = ns_r_servfail;
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
		hp->rcode = ns_r_formerr;
		return (Finish);
	}

	/*
	 * Skip domain name, get class, and type.
	 */
	if ((n = dn_skipname(*cpp, eom)) < 0) {
		ns_debug(ns_log_default, 1,
			 "FORMERR IQuery packet name problem");
		hp->rcode = ns_r_formerr;
		return (Finish);
	}
	*cpp += n;
	if (*cpp + 3 * INT16SZ + INT32SZ > eom) {
		ns_debug(ns_log_default, 1,
			 "FORMERR IQuery message too short");
		hp->rcode = ns_r_formerr;
		return (Finish);
	}
	GETSHORT(type, *cpp);
	GETSHORT(class, *cpp);
	*cpp += INT32SZ;	/* ttl */
	GETSHORT(dlen, *cpp);
	if (*cpp + dlen != eom) {
		ns_debug(ns_log_default, 1,
			 "FORMERR IQuery message length off");
		hp->rcode = ns_r_formerr;
		return (Finish);
	}
	*cpp += dlen;

	/*
	 * not all inverse queries are handled.
	 */
	switch (type) {
	case T_A:
		if (!NS_OPTION_P(OPTION_FAKE_IQUERY) || dlen != INT32SZ) {
			if (dlen != INT32SZ)
				ns_warning(ns_log_security,
					   "bad iquery from %s",
					   inet_ntoa(from.sin_addr));
			return (Refuse);
		}
		break;
	default:
		ns_warning(ns_log_security,
			   "unsupported iquery type from %s",
			   inet_ntoa(from.sin_addr));
		return (Refuse);
	}
	ns_debug(ns_log_default, 1,
		 "req: IQuery class %d type %d", class, type);

	fname = (char *)msg + HFIXEDSZ;
	alen = (char *)*cpp - fname;
	if ((size_t)alen > sizeof anbuf) {
		ns_warning(ns_log_security,
			   "bad iquery from %s",
			   inet_ntoa(from.sin_addr));
		return (Refuse);
	}
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
			if ((zp->z_flags & (Z_QSERIAL|Z_XFER_RUNNING)) == 0) {
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
			if ((zp->z_flags & (Z_QSERIAL|Z_XFER_RUNNING)) == 0) {
				zp->z_time = tt.tv_sec;
				sched_zone_maint(zp);
			}
			return (1);
		}
		return (0);

	case z_hint:
	case z_cache:
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
	int buflen, int doadd, u_char **comp_ptrs, u_char **edp,
	int use_minimum)
{
	u_char *cp;
	u_char *cp1, *sp;
	struct zoneinfo *zp;
	int32_t n;
	int16_t type = dp->d_type;
	u_int32_t ttl;

	ns_debug(ns_log_default, 5,
		 "make_rr(%s, %lx, %lx, %d, %d) %d zone %d ttl %lu",
		 name, (u_long)dp, (u_long)buf,
		 buflen, doadd, dp->d_size, dp->d_zone, (u_long)dp->d_ttl);

	if (dp->d_rcode && dp->d_size == 0)
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
		if (dp->d_ttl != USE_MINIMUM && !use_minimum)
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
		goto cleanup;
	cp = buf + n;
	buflen -= n;
	if (buflen < 0)
		goto cleanup;
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
			goto cleanup;
		PUTSHORT((u_int16_t)n, sp);
		cp += n;
		break;

	case T_MB:
	case T_NS:
		/* Store domain name in answer */
		n = dn_comp((char *)dp->d_data, cp, buflen, comp_ptrs, edp);
		if (n < 0)
			goto cleanup;
		PUTSHORT((u_int16_t)n, sp);
		cp += n;
		if (doadd) {
			addname((char*)dp->d_data, name,
				type, T_A, dp->d_class);
			addname(name, name, type, T_KEY, dp->d_class);
		}
		break;

	case T_SOA:
	case T_MINFO:
	case T_RP:
		cp1 = dp->d_data;
		n = dn_comp((char *)cp1, cp, buflen, comp_ptrs, edp);
		if (n < 0)
			goto cleanup;
		cp += n;
		buflen -= type == T_SOA ? n + 5 * INT32SZ : n;
		if (buflen < 0)
			goto cleanup;
		cp1 += strlen((char *)cp1) + 1;
		n = dn_comp((char *)cp1, cp, buflen, comp_ptrs, edp);
		if (n < 0)
			goto cleanup;
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
			if (doadd)
				addname(name, name, type, T_KEY, dp->d_class);
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
			goto cleanup;
 		memcpy(cp, cp1, INT16SZ);
 		cp += INT16SZ;
 		cp1 += INT16SZ;
		n = (u_int16_t)((cp - sp) - INT16SZ);
		ns_debug(ns_log_default, 1, "current size n = %u", n);

		/* copy preference */
		buflen -= INT16SZ;
		if (buflen < 0)
			goto cleanup;
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
			goto cleanup;
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
			goto cleanup;
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
			goto cleanup;
		*cp++ = n;
		memcpy(cp, cp1, n);
		cp += n;
		cp1 += n;
		n = (u_int16_t)((cp - sp) - INT16SZ);
		ns_debug(ns_log_default, 1, "current size n = %u", n);

		/* Replacement */
		ns_debug(ns_log_default, 1, "Replacement = %s", cp1);
		n = dn_comp((char *)cp1, cp, buflen, comp_ptrs, edp);
		ns_debug(ns_log_default, 1, "dn_comp's n = %u", n);
		if (n < 0)
			goto cleanup;
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
			goto cleanup;

 		/* copy preference */
 		memcpy(cp, cp1, INT16SZ);
 		cp += INT16SZ;
 		cp1 += INT16SZ;

		if (type == T_SRV) {
			buflen -= INT16SZ*2;
			if (buflen < 0)
				goto cleanup;
			memcpy(cp, cp1, INT16SZ*2);
			cp += INT16SZ*2;
			cp1 += INT16SZ*2;
		}

		n = dn_comp((char *)cp1, cp, buflen,
			    (type == ns_t_mx) ? comp_ptrs : NULL,
			    (type == ns_t_mx) ? edp : NULL);
		if (n < 0)
			goto cleanup;
		cp += n;

		/* save data length */
		n = (u_int16_t)((cp - sp) - INT16SZ);
		PUTSHORT((u_int16_t)n, sp);
		if (doadd)
			addname((char*)cp1, name, type, T_A, dp->d_class);
		break;

	case T_PX:
		cp1 = dp->d_data;

		if ((buflen -= INT16SZ) < 0)
			goto cleanup;

		/* copy preference */
		memcpy(cp, cp1, INT16SZ);
		cp += INT16SZ;
		cp1 += INT16SZ;

		n = dn_comp((char *)cp1, cp, buflen, comp_ptrs, edp);
		if (n < 0)
			goto cleanup;
		cp += n;
		buflen -= n;
		cp1 += strlen((char *)cp1) + 1;
		n = dn_comp((char *)cp1, cp, buflen, comp_ptrs, edp);
		if (n < 0)
			goto cleanup;
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
			goto cleanup;  /* out of room! */
		memcpy(cp, cp1, 18);
		cp  += 18;
		cp1 += 18;
		buflen -= 18;

		/* then the signer's name */
		n = dn_comp((char *)cp1, cp, buflen, NULL, NULL);
		if (n < 0)
			goto cleanup;
		cp += n;
		buflen -= n;
		cp1 += strlen((char*)cp1)+1;

		/* finally, we copy over the variable-length signature */
		n = dp->d_size - (u_int16_t)((cp1 - dp->d_data));
		if (n > buflen)
			goto cleanup;  /* out of room! */
		memcpy(cp, cp1, n);
		cp += n;
		
		/* save data length & return */
		n = (u_int16_t)((cp - sp) - INT16SZ);
		PUTSHORT((u_int16_t)n, sp);
		break;

	case T_NXT:
		cp1 = dp->d_data;
		n = dn_comp((char *)cp1, cp, buflen, NULL, NULL);
		if (n < 0)
			goto cleanup;

		cp += n;
		buflen -=n;
		cp1 += strlen((char *)cp1) + 1; 

		/* copy nxt bit map */
		n = dp->d_size - (u_int16_t)((cp1 - dp->d_data));
		if (n > buflen)
			goto cleanup;  /* out of room! */
		memcpy(cp, cp1, n);
		cp += n;
		buflen -= n;

		n = (u_int16_t)((cp - sp) - INT16SZ);
		PUTSHORT((u_int16_t)n, sp);

		break;

	default:
		if ((type == T_A || type == T_AAAA) && doadd)
			addname(name, name, type, T_KEY, dp->d_class);
		if (dp->d_size > buflen)
			goto cleanup;
		memcpy(cp, dp->d_data, dp->d_size);
		PUTSHORT((u_int16_t)dp->d_size, sp);
		cp += dp->d_size;
	}
	return (cp - buf);

 cleanup:
	/* Rollback RR. */
	ns_name_rollback(buf, (const u_char **)comp_ptrs,
			 (const u_char **)edp);
	return (-1);
}

static void
addname(const char *dname, const char *rname,
	u_int16_t rtype, u_int16_t type, u_int16_t class)
{
	struct addinfo *ap;
	int n;

	for (ap = addinfo, n = addcount; --n >= 0; ap++)
		if (ns_samename(ap->a_dname, dname) == 1 && ap->a_type == type)
			return;

	/* add domain name to additional section */
	if (addcount < NADDRECS) {
		addcount++;
		ap->a_dname = savestr(dname, 1);
		ap->a_rname = savestr(rname, 1);
		ap->a_rtype = rtype;
		ap->a_type = type;
		ap->a_class = class;
	}
}

/*
 * Lookup addresses/keys for names in addinfo and put into the message's
 * additional section.
 */
int
doaddinfo(HEADER *hp, u_char *msg, int msglen) {
	register struct namebuf *np;
	register struct databuf *dp;
	register struct addinfo *ap;
	register u_char *cp;
	struct hashbuf *htp;
	const char *fname;
	register int n, count;
	register int ns_logging;
	int finishedA = 0;
	int save_addcount = addcount;

	if (!addcount)
		return (0);

	ns_logging = ns_wouldlog(ns_log_default, 3);

	if (ns_logging)
		ns_debug(ns_log_default, 3,
			"doaddinfo() addcount = %d", addcount);

	if (hp->tc) {
		ns_debug(ns_log_default, 4,
			 "doaddinfo(): tc already set, bailing");
		return (0);
	}

	count = 0;
	cp = msg;
loop:
	for (ap = addinfo; --addcount >= 0; ap++) {
		int     foundany = 0,
			foundcname = 0,
			save_count = count,
			save_msglen = msglen;
		u_char	*save_cp = cp;

		if ((finishedA == 1 && ap->a_type == T_A) ||
		    (finishedA == 0 && ap->a_type == T_KEY))
			continue;
		if (ns_logging)
			ns_debug(ns_log_default, 3,
				 "do additional \"%s\" (from \"%s\")",
				 ap->a_dname, ap->a_rname);
		htp = hashtab;	/* because "nlookup" stomps on arg. */
		np = nlookup(ap->a_dname, &htp, &fname, 0);
		if (np == NULL || fname != ap->a_dname)
			goto next_rr;
		if (ns_logging)
		    ns_debug(ns_log_default, 3, "found it");
		/* look for the data */
		(void)delete_stale(np);
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			if (dp->d_rcode)
				continue;
			if ((match(dp, (int)ap->a_class, T_CNAME) &&
			     dp->d_type == T_CNAME) ||
			    (match(dp, C_IN, T_CNAME) &&
			     dp->d_type == T_CNAME)) {
				foundcname++;
				break;
			}
			if (ap->a_type == T_A &&
			    !match(dp, (int)ap->a_class, T_A) &&
			    !match(dp, C_IN, T_A) &&
			    !match(dp, (int)ap->a_class, T_AAAA) &&
			    !match(dp, C_IN, T_AAAA) &&
			    !match(dp, (int)ap->a_class, ns_t_a6) &&
			    !match(dp, C_IN, ns_t_a6)) {
				continue;
			}
			if (ap->a_type == T_KEY &&
			    !match(dp, (int)ap->a_class, T_KEY) &&
			    !match(dp, C_IN, T_KEY))
				continue;

			foundany++;
			/*
			 *  Should be smart and eliminate duplicate
			 *  data here.	XXX
			 */
			if ((n = make_rr(ap->a_dname, dp, cp, msglen, 0,
					 dnptrs, dnptrs_end, 0)) < 0) {
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
				/* Rollback RRset. */
				ns_name_rollback(save_cp,
						 (const u_char **)dnptrs,
						 (const u_char **)dnptrs_end);
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
		    !foundcname && !foundany &&
		    (ap->a_type == T_A || ap->a_type == T_AAAA)) {
			/* ask a real server for this info */
			(void) sysquery(ap->a_dname, (int)ap->a_class,
					ap->a_type, NULL, 0, ns_port, QUERY);
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
	if (finishedA == 0) {
		finishedA = 1;
		addcount = save_addcount;
		goto loop; /* now do the KEYs... */
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
	n = make_rr(dnbuf, dp, cp, buflen, 1, dnptrs, dnptrs_end, 1);
	if (n <= 0) {
		ns_debug(ns_log_default, 1,
			 "doaddauth: can't add oversize '%s' (%d) (n=%d)",
			 dnbuf, buflen, n);
		if (n < 0) {
			hp->tc = 1;
		}
		return (0);
	}
	if (dp->d_secure != DB_S_SECURE)
		hp->ad = 0;
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
