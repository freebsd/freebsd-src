#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)ns_resp.c	4.65 (Berkeley) 3/3/91";
static char rcsid[] = "$Id: ns_resp.c,v 8.41 1998/04/07 04:59:45 vixie Exp $";
#endif /* not lint */

/*
 * ++Copyright++ 1986, 1988, 1990
 * -
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
 * --Copyright--
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <errno.h>
#include <stdio.h>
#include <resolv.h>

#include "named.h"

static u_int8_t		norootlogged[MAXCLASS];	/* XXX- should be a bitmap */

static const char	skipnameFailedAnswer[] = "skipname failed in answer",
			skipnameFailedAuth[] =	"skipname failed in authority",
			skipnameFailedQuery[] =	"skipname failed in query",
			outofDataQuery[] =	"ran out of data in query",
			outofDataAnswer[] =	"ran out of data in answer",
			notSingleQuery[] =	"not exactly one query",
			expandFailedQuery[] =	"dn_expand failed in query",
			expandFailedAnswer[] =	"dn_expand failed in answer",
			expandFailedAuth[] =	"dn_expand failed in authority",
			outofDataAuth[] =	"ran out of data in authority",
			dlenOverrunAnswer[] =	"dlen overrun in answer",
			dlenOverrunAuth[] =	"dlen overrun in authority",
			dlenUnderrunAnswer[] =	"dlen underrun in answer",
			outofDataFinal[] =	"out of data in final pass",
			outofDataAFinal[] =	"out of data after final pass",
			badNameFound[] =	"found an invalid domain name",
			wrongQuestion[] =	"answer to wrong question",
			danglingCname[] =	"dangling CNAME pointer";

struct db_list {
	struct db_list *db_next;
	struct databuf *db_dp;
};

struct flush_set {
	char *		fs_name;
	int		fs_type;
	int		fs_class;
	u_int		fs_cred;
	struct db_list *fs_list;
	struct db_list *fs_last;
};

static void		rrsetadd __P((struct flush_set *, char *,
				      struct databuf *)),
			rrsetupdate __P((struct flush_set *, int flags)),
			flushrrset __P((struct flush_set *)),
			free_flushset __P((struct flush_set *));
static int		rrsetcmp __P((char *, struct db_list *)),
			check_root __P((void)),
			check_ns __P((void)),
			rrextract __P((u_char *, int, u_char *,
				       struct databuf **, char *, int,
				       char **));

static void		add_related_additional __P((char *));
static void		free_related_additional __P((void));
static int		related_additional __P((char *));
static void		maybe_free __P((char **));

#define MAX_RELATED 100

static int num_related = 0;
static char *related[MAX_RELATED];

#ifdef LAME_LOGGING
static char *
learntFrom(qp, server)
	struct qinfo *qp;
	struct sockaddr_in *server;
{
	static char *buf = NULL;
	char *a, *ns, *na;
	struct databuf *db;
#ifdef STATS
	char nsbuf[20];
	char abuf[20];
#endif
	int i;
	
	if (buf) {
		free(buf);
		buf = NULL;
	}

	a = ns = na = "<Not Available>";

	for (i = 0; i < (int)qp->q_naddr; i++) {
		if (qp->q_addr[i].ns_addr.sin_addr.s_addr ==
		    server->sin_addr.s_addr) {
			db = qp->q_addr[i].ns;
			if (db) {
#ifdef STATS
				if (db->d_ns) {
					strcpy(nsbuf,
					       inet_ntoa(db->d_ns->addr));
					ns = nsbuf;
				} else {
					ns = zones[db->d_zone].z_origin;
				}
#endif

#ifdef NCACHE
				if (!db->d_rcode)
#endif
					na = (char*)qp->q_addr[i].ns->d_data;
			}

#ifdef STATS
			db = qp->q_addr[i].nsdata;
			if (db) {
				if (db->d_ns) {
					strcpy(abuf,
					       inet_ntoa(db->d_ns->addr));
					a = abuf;
				} else {
					a = zones[db->d_zone].z_origin;
				}
			}
#endif
			break;
		}
	}

	if ((a == ns) && (ns == na))	/* all "UNKNOWN" */
		return ("");
	
#ifdef STATS
# define LEARNTFROM " '%s': learnt (A=%s,NS=%s)"
#else
# define LEARNTFROM " '%s'"
#endif
	buf = malloc(strlen(a = (*a ? a : "\".\"")) +
		     strlen(ns = (*ns ? ns : "\".\"")) +
		     strlen(na = (*na ? na : "\".\"")) +
		     sizeof(LEARNTFROM));
	if (!buf)
		return ("");
	sprintf(buf, LEARNTFROM, na, a, ns);
	return (buf);
}
#endif /*LAME_LOGGING*/

void
ns_resp(msg, msglen)
	u_char *msg;
	int msglen;
{
	register struct qinfo *qp;
	register HEADER *hp;
	register struct qserv *qs;
	register struct databuf *ns, *ns2;
	register u_char *cp;
	u_char *eom = msg + msglen;
	struct flush_set *flushset = NULL;
	struct sockaddr_in *nsa;
	struct databuf *nsp[NSMAX];
	int i, c, n, qdcount, ancount, aucount, nscount, arcount, arfirst;
	int qtype, qclass, dbflags;
	int restart;	/* flag for processing cname response */
	int validanswer;
	int cname, lastwascname, externalcname;
	int count, founddata, foundname;
	int buflen;
	int newmsglen;
	char name[MAXDNAME], qname[MAXDNAME], aname[MAXDNAME];
	char msgbuf[MAXDNAME];
	char *dname, tmpdomain[MAXDNAME];
	const char *fname;
	const char *formerrmsg = "brain damage";
	u_char newmsg[PACKETSZ];
	u_char **dpp, *tp;
	time_t rtrip;
	struct hashbuf *htp;
	struct namebuf *np;
	struct netinfo *lp;
	struct fwdinfo *fwd;
	char *tname = NULL;

	nameserIncr(from_addr.sin_addr, nssRcvdR);
	nsp[0] = NULL;
	hp = (HEADER *) msg;
	if ((qp = qfindid(hp->id)) == NULL ) {
		dprintf(1, (ddt, "DUP? dropped (id %d)\n", ntohs(hp->id)));
		nameserIncr(from_addr.sin_addr, nssRcvdDupR);
		return;
	}

	dprintf(2, (ddt, "Response (%s %s %s) nsid=%d id=%d\n",
		    (qp->q_flags & Q_SYSTEM) ?"SYSTEM" :"USER",
		    (qp->q_flags & Q_PRIMING) ?"PRIMING" :"NORMAL",
		    (qp->q_flags & Q_ZSERIAL) ?"ZSERIAL" :"-",
		    ntohs(qp->q_nsid), ntohs(qp->q_id)));

	/*
	 * Here we handle high level formatting problems by parsing the header.
	 */
	qdcount = ntohs(hp->qdcount);
	ancount = ntohs(hp->ancount);
	aucount = ntohs(hp->nscount);	/* !!! */
	arcount = ntohs(hp->arcount);
	free_addinfo();		/* sets addcount to zero */
	cp = msg + HFIXEDSZ;
	dpp = dnptrs;
	*dpp++ = msg;
	if ((*cp & INDIR_MASK) == 0)
		*dpp++ = cp;
	*dpp = NULL;
	if (qdcount == 1) {
		n = dn_expand(msg, eom, cp, qname, sizeof(qname));
		if (n <= 0) {
			formerrmsg = expandFailedQuery;
			goto formerr;
		}
		cp += n;
		if (cp + 2 * INT16SZ > eom) {
			formerrmsg = outofDataQuery;
			goto formerr;
		}
		GETSHORT(qtype, cp);
		GETSHORT(qclass, cp);
		if (!ns_nameok(qname, qclass, response_trans,
			       ns_ownercontext(qtype, response_trans),
			       qname, from_addr.sin_addr)) {
			formerrmsg = badNameFound;
			goto formerr;
		}
		if (cp > eom) {
			formerrmsg = outofDataQuery;
			goto formerr;
		}
		if (qp->q_msg && qp->q_msglen &&
		    !res_nameinquery(qname, qtype, qclass,
				     qp->q_msg, qp->q_msg + qp->q_msglen)) {
			sprintf(msgbuf,
				"query section mismatch (%s %s %s)",
				qname, p_class(qclass), p_type(qtype));
			formerrmsg = msgbuf;
			goto formerr;
		}
		if (strcasecmp(qp->q_name, qname) != 0 ||
		    qp->q_class != qclass ||
		    qp->q_type != qtype) {
			formerrmsg = wrongQuestion;
			goto formerr;
		}
	} else {
		strcpy(qname, qp->q_name);
		qclass = qp->q_class;
		qtype = qp->q_type;
	}

	/* cp now points after the query section. */

	/*
	 *  Here we handle bad responses from servers.
	 *  Several possibilities come to mind:
	 *	The server is sick and returns SERVFAIL
	 *	The server returns some garbage opcode (it's sick)
	 *	The server can't understand our query and return FORMERR
	 *  In all these cases, we drop the packet, disable retries on
	 *  this server and immediately force a retry.
	 */
	if ((hp->rcode != NOERROR && hp->rcode != NXDOMAIN)
	    || (hp->opcode != QUERY
#ifdef BIND_NOTIFY
		&& hp->opcode != NS_NOTIFY_OP
#endif
		)) {
		dprintf(2, (ddt, "resp: error (ret %d, op %d), dropped\n",
			    hp->rcode, hp->opcode));
		switch (hp->rcode) {
		case SERVFAIL:
			nameserIncr(from_addr.sin_addr, nssRcvdFail);
			break;
		case FORMERR:
			nameserIncr(from_addr.sin_addr, nssRcvdFErr);
			break;
		default:
			nameserIncr(from_addr.sin_addr, nssRcvdErr);
			break;
		}
		/* mark server as bad */
		if (!qp->q_fwd)
			for (i = 0; i < (int)qp->q_naddr; i++)
				if (qp->q_addr[i].ns_addr.sin_addr.s_addr
				    == from_addr.sin_addr.s_addr)
					qp->q_addr[i].nretry = MAXRETRY;
		/*
		 * XXX:	doesn't handle responses sent from the wrong
		 *	interface on a multihomed server.
		 */
		if (qp->q_fwd ||
		    qp->q_addr[qp->q_curaddr].ns_addr.sin_addr.s_addr
		    == from_addr.sin_addr.s_addr)
			retry(qp);
		return;
	}

	if (qdcount != 1) {
		/* We don't generate or forward these (yet). */
		formerrmsg = notSingleQuery;
		goto formerr;
	}

	/*
	 * Determine if the response came from a forwarder.  Packets from
	 * anyplace not listed as a forwarder or as a server to whom we
	 * might have forwarded the query will be dropped.
	 */
	for (fwd = fwdtab;  fwd != (struct fwdinfo *)NULL;  fwd = fwd->next) {
		if (fwd->fwdaddr.sin_addr.s_addr ==
		    from_addr.sin_addr.s_addr) {
			/* XXX - should put this in STATS somewhere. */
			break;
		}
	}
	/*
	 * XXX:	note bad ambiguity here.  if one of our forwarders is also
	 *	a delegated server for some domain, then we will not update
	 *	the RTT information on any replies we get from those servers.
	 *	Workaround: disable recursion on authoritative servers so that
	 *	the ambiguity does not arise.
	 */
	/*
	 * If we weren't using a forwarder, find the qinfo pointer and update
	 * the rtt and fact that we have called on this server before.
	 */
	if (fwd == (struct fwdinfo *)NULL) {
		struct timeval *stp;

		for (n = 0, qs = qp->q_addr; (u_int)n < qp->q_naddr; n++, qs++)
			if (qs->ns_addr.sin_addr.s_addr ==
			    from_addr.sin_addr.s_addr)
				break;
		if ((u_int)n >= qp->q_naddr) {
			if (!haveComplained((char*)from_addr.sin_addr.s_addr,
					    "unexpected source")) {
				syslog(LOG_INFO,
				       "Response from unexpected source (%s)",
				       sin_ntoa(&from_addr));
			}
			/* 
			 * We don't know who this response came from so it
			 * gets dropped on the floor.
			 */
			return;
		}
		stp = &qs->stime;

		/* Handle response from different (untried) interface */
		if ((qs->ns != NULL) && (stp->tv_sec == 0)) {
			ns = qs->ns;
			while (qs > qp->q_addr
			       && (qs->stime.tv_sec == 0 || qs->ns != ns))
				qs--;
			*stp = qs->stime;
			/* XXX - sometimes stp still ends up pointing to
			 * a zero timeval, in spite of the above attempt.
			 * Why?  What should we do about it?
			 */
			dprintf(1, (ddt,
			      "Response from unused address %s, assuming %s\n",
				    sin_ntoa(&from_addr),
				    sin_ntoa(&qs->ns_addr)));
			/* XXX - catch aliases here */
		}

		/* compute query round trip time */
		/* XXX - avoid integer overflow, which is quite likely if stp
		 * points to a zero timeval (see above).
		 * rtrip is of type time_t, which we assume is at least
		 * as big as an int.
		 */
		if ((tt.tv_sec - stp->tv_sec) > (INT_MAX-999)/1000) {
			rtrip = INT_MAX;
		} else {
			rtrip = ((tt.tv_sec - stp->tv_sec) * 1000 +
				 (tt.tv_usec - stp->tv_usec) / 1000);
		}
		
		dprintf(3, (ddt, "stime %lu/%lu  now %lu/%lu rtt %ld\n",
			    (u_long)stp->tv_sec, (u_long)stp->tv_usec,
			    (u_long)tt.tv_sec, (u_long)tt.tv_usec,
			    (long)rtrip));

		/* prevent floating point overflow, limit to 1000 sec */
		if (rtrip > 1000000) {
			rtrip = 1000000;
		}
		ns = qs->nsdata;
		/*
		 * Don't update nstime if this doesn't look
		 * like an address databuf now.			XXX
		 */
		if (ns &&
		    ns->d_type == T_A &&
		    ns->d_class == qs->ns->d_class) {
			u_long t;

			if (ns->d_nstime == 0)
				t = rtrip;
			else
				t = ns->d_nstime * ALPHA
					+
				   (1 - ALPHA) * rtrip;
			if (t > 65535)
				t = 65535;
			ns->d_nstime = (u_int16_t)t;
		}

		/*
		 * Record the source so that we do not use this NS again.
		 */
		if (ns && qs->ns && (qp->q_nusedns < NSMAX)) {
			qp->q_usedns[qp->q_nusedns++] = qs->ns;
			dprintf(2, (ddt, "NS #%d addr %s used, rtt %d\n",
				    n, sin_ntoa(&qs->ns_addr),
				    ns->d_nstime));
		}

		/*
		 * Penalize those who had earlier chances but failed
		 * by multiplying round-trip times by BETA (>1).
		 * Improve nstime for unused addresses by applying GAMMA.
		 * The GAMMA factor makes unused entries slowly
		 * improve, so they eventually get tried again.
		 * GAMMA should be slightly less than 1.
		 * Watch out for records that may have timed out
		 * and are no longer the correct type.			XXX
		 */
		
		for (n = 0, qs = qp->q_addr;
		     (u_int)n < qp->q_naddr;
		     n++, qs++) {
			u_long t;

			ns2 = qs->nsdata;
			if ((!ns2) || (ns2 == ns))
				continue;
			if (ns2->d_type != T_A ||
			    ns2->d_class != qs->ns->d_class)	/* XXX */
				continue;
			if (qs->stime.tv_sec) {
			    if (ns2->d_nstime == 0)
				t = (rtrip * BETA);
			    else
				t = ns2->d_nstime * BETA
					+
				   (1 - ALPHA) * rtrip;
			} else
			    t = ns2->d_nstime * GAMMA;
			if (t > 65535)
				t = 65535;
			ns2->d_nstime = (u_int16_t)t;
			dprintf(2, (ddt, "NS #%d %s rtt now %d\n", n,
				    sin_ntoa(&qs->ns_addr),
				    ns2->d_nstime));
		}
	}

#ifdef BIND_NOTIFY
	/* for now, NOTIFY isn't defined for ANCOUNT!=0, AUCOUNT!=0,
	 * or ADCOUNT!=0.  therefore the only real work to be done for
	 * a NOTIFY-QR is to remove it from the query queue.
	 */
	if (hp->opcode == NS_NOTIFY_OP) {
		qremove(qp);
		return;
	}
#endif

#ifdef LAME_DELEGATION
	/*
	 *  Non-authoritative, no answer, no error, with referral.
	 */
	if (hp->rcode == NOERROR && !hp->aa && ancount == 0 && aucount > 0
#ifdef BIND_NOTIFY
	    && hp->opcode != NS_NOTIFY_OP
#endif
	    ) {
		u_char *tp;
		int type, class;
#ifdef DEBUG
		if (debug > 0)
			fp_nquery(msg, msglen, ddt);
#endif
		/*
		 * Since there is no answer section (ancount == 0),
		 * we must be pointing at the authority section (aucount > 0).
		 */
		tp = cp;
		n = dn_expand(msg, eom, tp, name, sizeof name);
		if (n < 0) {
			formerrmsg = expandFailedAuth;
			goto formerr;
		}
		tp += n;
		if (tp + 2 * INT16SZ > eom) {
			formerrmsg = outofDataAuth;
			goto formerr;
		}
		GETSHORT(type, tp);
		GETSHORT(class, tp);
		if (!ns_nameok(name, class, response_trans,
			       ns_ownercontext(type, response_trans),
			       name, from_addr.sin_addr)) {
			formerrmsg = badNameFound;
			goto formerr;
		}

		/*
		 * If the answer delegates us either to the same level in
		 * the hierarchy or closer to the root, we consider this
		 * server lame.  Note that for now we only log the message
		 * if the T_NS was C_IN, which is technically wrong (NS is
		 * visible in all classes) but necessary anyway (non-IN
		 * classes tend to not have good strong delegation graphs).
		 */

		if (type == T_NS && samedomain(qp->q_domain, name)) {
			nameserIncr(from_addr.sin_addr, nssRcvdLDel);
			/* mark server as bad */
			if (!qp->q_fwd)
			    for (i = 0; i < (int)qp->q_naddr; i++)
				if (qp->q_addr[i].ns_addr.sin_addr.s_addr
				    == from_addr.sin_addr.s_addr)
					qp->q_addr[i].nretry = MAXRETRY;
#ifdef LAME_LOGGING
			if (class == C_IN &&
			    !haveComplained((char*)nhash(sin_ntoa(&from_addr)),
					    (char*)nhash(qp->q_domain)))
				syslog(LAME_LOGGING,
				      "Lame server on '%s' (in '%s'?): %s%s\n",
				       qname, qp->q_domain, 
				       sin_ntoa(&from_addr),
				       learntFrom(qp, &from_addr));

#endif /* LAME_LOGGING */
			/* XXX - doesn't handle responses sent from the wrong
			 * interface on a multihomed server
			 */
			if (qp->q_fwd ||
			    qp->q_addr[qp->q_curaddr].ns_addr.sin_addr.s_addr
			    == from_addr.sin_addr.s_addr)
				retry(qp);
			return;
		}
	}
#endif /* LAME_DELEGATION */

	if (qp->q_flags & Q_ZSERIAL) {
		if (hp->aa && ancount > 0 && hp->rcode == NOERROR &&
		    qtype == T_SOA && ((qclass == C_IN) || (qclass == C_HS))) {
			int n;
			u_int16_t type, class, dlen;
			u_int32_t serial;
			u_char *tp = cp;
			u_char *rdatap;

			n = dn_expand(msg, eom, tp, name, sizeof name);
			if (n < 0) {
				formerrmsg = expandFailedAnswer;
				goto formerr;
			}
			tp += n;  		/* name */
			if (tp + 3 * INT16SZ + INT32SZ > eom) {
				formerrmsg = outofDataAnswer;
				goto formerr;
			}
			GETSHORT(type, tp);	/* type */
			GETSHORT(class, tp);	/* class */
			tp += INT32SZ;		/* ttl */
			GETSHORT(dlen, tp); 	/* dlen */
			rdatap = tp;		/* start of rdata */
			if (!ns_nameok(name, class, response_trans,
				       ns_ownercontext(type, response_trans),
				       name, from_addr.sin_addr)) {
				formerrmsg = badNameFound;
				goto formerr;
			}
			if (strcasecmp(qname, name) ||
			    qtype != type ||
			    qclass != class) {
				sprintf(msgbuf,
					"qserial answer mismatch (%s %s %s)",
					name, p_class(class), p_type(type));
				formerrmsg = msgbuf;
				goto formerr;
			}

			if (0 >= (n = dn_skipname(tp, eom))) {
				formerrmsg = skipnameFailedAnswer;
				goto formerr;
			}
			tp += n;  		/* mname */
			if (0 >= (n = dn_skipname(tp, eom))) {
				formerrmsg = skipnameFailedAnswer;
				goto formerr;
			}
			tp += n;  		/* rname */
			if (tp + 5 * INT32SZ > eom) {
				formerrmsg = dlenUnderrunAnswer;
				goto formerr;
			}
			GETLONG(serial, tp);
			tp += 4 * INT32SZ;	/* Skip rest of SOA. */
			if ((u_int)(tp - rdatap) != dlen) {
				formerrmsg = dlenOverrunAnswer;
				goto formerr;
			}

			qserial_answer(qp, serial);
			qremove(qp);
		} else {
			retry(qp);
		}
		return;
	}

	/*
	 * Add the info received in the response to the data base.
	 */
	arfirst = ancount + aucount;
	c = arfirst + arcount;

	/* -ve $ing non-existence of record, must handle non-authoritative
	 * NOERRORs with c == 0.
	 */
	if (!hp->aa && hp->rcode == NOERROR && c == 0)
		goto return_msg;

#ifdef notdef
	/*
	 * If the request was for a CNAME that doesn't exist,
	 * but the name is valid, fetch any other data for the name.
	 * DON'T do this now, as it will requery if data are already
	 * in the cache (maybe later with negative caching).
	 */
	if (type == T_CNAME && c == 0 && hp->rcode == NOERROR
	    && !(qp->q_flags & Q_SYSTEM)) {
		dprintf(4, (ddt, "resp: leaving, no CNAME\n"));

		/* Cause us to put it in the cache later */
		prime(class, T_ANY, qp);

		/* Nothing to store, just give user the answer */
		goto return_msg;
	}
#endif /* notdef */

	if (qp->q_flags & Q_SYSTEM)
		dbflags = DB_NOTAUTH | DB_NODATA;
	else
		dbflags = DB_NOTAUTH | DB_NODATA | DB_NOHINTS;
	count = c;
	if (qp->q_flags & Q_PRIMING)
		dbflags |= DB_PRIMING;
	if (hp->tc) {
		count -= arcount;	/* truncation had to affect this */
		if (!arcount) {
			count -= aucount;	/* guess it got this too */
		}
		if (!(arcount || aucount)) {
			count -= ancount;	/* things are pretty grim */
		}
		/* XXX - should retry this query with TCP */
		/*
		 * XXX - if this response is forwarded to the client
		 * the truncated section is included.  We will not
		 * validate it, and if it somehow corrupt, we won't
		 * notice.
		 *
		 * XXX - if the answer section is truncated and we got
		 * this response after being redirected by a CNAME, we
		 * will not include any part of the final answer in our
		 * response to the client.  This will make the client
		 * think that there are no RRs of the appropriate type.
		 */
	}

	tp = cp;

	restart = 0;
	validanswer = 0;
	nscount = 0;
	cname = 0;
	lastwascname = 0;
	externalcname = 0;
	strcpy(aname, qname);

	if (count) {
		/* allocate 1 extra record for end of set detection */
		flushset = (struct flush_set *)
				calloc(count+1, sizeof(struct flush_set));
		if (!flushset)
			panic(-1, "flushset: out of memory");
	} else
		flushset = NULL;

	for (i = 0; i < count; i++) {
		struct databuf *dp;
		int type;

		maybe_free(&tname);
		if (cp >= eom) {
			free_related_additional();
			if (flushset != NULL)
				free_flushset(flushset);
			formerrmsg = outofDataFinal;
			goto formerr;
		}
		n = rrextract(msg, msglen, cp, &dp, name, sizeof name, &tname);
		if (n < 0) {
			free_related_additional();
			maybe_free(&tname);
			if (flushset != NULL)
				free_flushset(flushset);
			formerrmsg = outofDataFinal;
			goto formerr;
		}
		cp += n;
		if (!dp)
			continue;
		type = dp->d_type;
		if (i < ancount) {
			/* Answer section. */
			if (externalcname || strcasecmp(name, aname) != 0) {
				if (!externalcname)
					syslog(LOG_DEBUG,
					       "wrong ans. name (%s != %s)",
					       name, aname);
				else
					dprintf(3, (ddt,
				 "ignoring answer '%s' after external cname\n",
						    name));
				db_free(dp);
				continue;
			}
			if (type == T_CNAME &&
			    qtype != T_CNAME && qtype != T_ANY) {
				strcpy(aname, (char *)dp->d_data);
				if (!samedomain(aname, qp->q_domain))
					externalcname = 1;
				cname = 1;
				lastwascname = 1;
			} else {
				validanswer = 1;
				lastwascname = 0;
			}

			if (tname != NULL) {
				add_related_additional(tname);
				tname = NULL;
			}

			dp->d_cred = (hp->aa && !strcasecmp(name, qname))
				? DB_C_AUTH
				: DB_C_ANSWER;
		} else {
			/* After answer section. */
			if (lastwascname) {
				dprintf(3, (ddt,
				 "last was cname, ignoring auth. and add.\n"));
				db_free(dp);
				break;
			}
			if (i < arfirst) {
				/* Authority section. */
				switch (type) {
				case T_NS:
				case T_SOA:
					if (!samedomain(aname, name)){
						syslog(LOG_DEBUG,
						    "bad referral (%s !< %s)",
						       aname[0] ? aname : ".",
						       name[0] ? name : ".");
						db_free(dp);
						continue;
					} else if (!samedomain(name,
							       qp->q_domain)) {
						if (!externalcname)
						    syslog(LOG_DEBUG,
  					            "bad referral (%s !< %s)",
							 name[0] ? name : ".",
							 qp->q_domain[0] ?
							 qp->q_domain : ".");
						db_free(dp);
						continue;
					}
					if (type == T_NS) {
						nscount++;
						add_related_additional(tname);
						tname = NULL;
					}
					break;
				case T_NXT:
				case T_SIG:
					break;
				default:
					syslog(LOG_DEBUG,
	"invalid RR type '%s' in authority section (name = '%s') from %s",
					       p_type(type), name,
					       sin_ntoa(&from_addr));
					db_free(dp);
					continue;
				}
			} else {
				/* Additional section. */
				switch (type) {
				case T_A:
				case T_AAAA:
					if (externalcname ||
					    !samedomain(name, qp->q_domain)) {
						dprintf(3, (ddt,
				     "ignoring additional info '%s' type %s\n",
						       name, p_type(type)));
						db_free(dp);
						continue;
					}
					if (!related_additional(name)) {
						syslog(LOG_DEBUG,
			     "unrelated additional info '%s' type %s from %s",
						       name, p_type(type),
						       sin_ntoa(&from_addr));
						db_free(dp);
						continue;
					}
					break;
				case T_KEY:
				case T_SIG:
					break;
				default:
					syslog(LOG_DEBUG,
	"invalid RR type '%s' in additional section (name = '%s') from %s",
					       p_type(type), name,
					       sin_ntoa(&from_addr));
					db_free(dp);
					continue;
				}
			}
			dp->d_cred = (qp->q_flags & Q_PRIMING)
				? DB_C_ANSWER
				: DB_C_ADDITIONAL;
		}
		rrsetadd(flushset, name, dp);
	}
	free_related_additional();
	maybe_free(&tname);
	if (flushset) {
		rrsetupdate(flushset, dbflags);
		free_flushset(flushset);
	}
	if (lastwascname && !externalcname)
		syslog(LOG_DEBUG, "%s (%s)", danglingCname, aname);

	if (cp > eom) {
		formerrmsg = outofDataAFinal;
		goto formerr;
	}

	if ((qp->q_flags & Q_SYSTEM) && ancount) {
		if ((qp->q_flags & Q_PRIMING) && !check_root()) {
			/* mark server as bad */
			if (!qp->q_fwd)
			    for (i = 0; i < (int)qp->q_naddr; i++)
				if (qp->q_addr[i].ns_addr.sin_addr.s_addr
				    == from_addr.sin_addr.s_addr)
					qp->q_addr[i].nretry = MAXRETRY;
			/* XXX - doesn't handle responses sent from
			* the wronginterface on a multihomed server
		 	*/
			if (qp->q_fwd ||
			    qp->q_addr[qp->q_curaddr].ns_addr.sin_addr.s_addr
			    == from_addr.sin_addr.s_addr)
				retry(qp);
			return;
		}
		dprintf(3, (ddt, "resp: leaving, SYSQUERY ancount %d\n",
			    ancount));
#ifdef BIND_NOTIFY
		if (qp->q_notifyzone != DB_Z_CACHE) {
			struct zoneinfo *zp = &zones[qp->q_notifyzone];

			/*
			 * Clear this first since sysnotify() might set it.
			 */
			qp->q_notifyzone = DB_Z_CACHE;
			sysnotify(zp->z_origin, zp->z_class, T_SOA);
		}
#endif
		qremove(qp);
		return;
	}

	if (ancount && count && !validanswer)
		/*
		 * Everything passed validation but we didn't get the
		 * final answer.  The response must have contained
		 * a dangling CNAME.  Force a restart of the query.
		 *
		 * Don't set restart if count==0, since this means
		 * the response was truncated in the answer section,
	         * causing us to set count to 0 which will cause
		 * validanswer to be 0 as well even though the answer
		 * section probably contained valid RRs (just not
		 * a complete set).
		 * XXX - this works right if we can just forward this
		 * response to the client, but not if we found a CNAME
		 * in a prior response and restarted the query.
		 */
		restart = 1;

	/*
	 *  If there are addresses and this is a local query,
	 *  sort them appropriately for the local context.
	 */
#ifdef SORT_RESPONSE
	if (!restart && ancount > 1 && (lp = local(&qp->q_from)) != NULL) 
		sort_response(tp, ancount, lp, eom);
#endif

	/*
	 * An answer to a T_ANY query or a successful answer to a
	 * regular query with no indirection, then just return answer.
	 */
	if (!restart && ancount && (qtype == T_ANY || !qp->q_cmsglen)) {
		dprintf(3, (ddt, "resp: got as much answer as there is\n"));
		goto return_msg;
	}

	/*
	 * We might want to cache this negative answer.
	 */
	if (!ancount &&
	    (!nscount || hp->rcode == NXDOMAIN) &&
	    (hp->aa || fwd || qclass == C_ANY)) {
		/* we have an authoritative NO */
		dprintf(3, (ddt, "resp: leaving auth NO\n"));
#ifdef NCACHE
		/* answer was NO */
		if (hp->aa &&
		    ((hp->rcode == NXDOMAIN) || (hp->rcode == NOERROR))) {
			cache_n_resp(msg, msglen);
		}
#endif /*NCACHE*/
		if (qp->q_cmsglen) {
			/* XXX - what about additional CNAMEs in the chain? */
			msg = qp->q_cmsg;
			msglen = qp->q_cmsglen;
			hp = (HEADER *)msg;
		}
		goto return_msg;
	}

	/*
	 * All messages in here need further processing.  i.e. they
	 * are either CNAMEs or we got referred again.
	 */
	count = 0;
	founddata = 0;
	foundname = 0;
	dname = name;
	/*
	 * Even with VALIDATE, if restart==0 and ancount > 0, we should
	 * have some valid data because because the data in the answer
	 * section is owned by the query name and that passes the
	 * validation test by definition
	 *
	 * XXX - the restart stuff doesn't work if any of the answer RRs
	 * is not cacheable (TTL==0 or unknown RR type), since all of the
	 * answer must pass through the cache and be re-assembled.
	 */
	if ((!restart || !cname) && qp->q_cmsglen && ancount) {
		dprintf(1, (ddt, "Cname second pass\n"));
		newmsglen = MIN(PACKETSZ, qp->q_cmsglen);
		bcopy(qp->q_cmsg, newmsg, newmsglen);
	} else {
		newmsglen = MIN(PACKETSZ, msglen);
		bcopy(msg, newmsg, newmsglen);
	}
	hp = (HEADER *) newmsg;
	hp->ancount = htons(0);
	hp->nscount = htons(0);
	hp->arcount = htons(0);
	hp->rcode = NOERROR;
	dnptrs[0] = newmsg;
	dnptrs[1] = NULL;
	cp = newmsg + HFIXEDSZ;
	/*
	 * Keep in mind that none of this code works when QDCOUNT>1.
	 * cp ends up pointed just past the query section in both cases.
	 */
	/*
	 * Arrange for dname to contain the query name. The query
	 * name can be either the original query name if restart==0
	 * or the target of the last CNAME if we are following a
	 * CNAME chain and were referred.
	 */
	n = dn_expand(newmsg, newmsg + newmsglen, cp, dname,
		      sizeof name);
	if (n < 0) {
		dprintf(1, (ddt, "dn_expand failed\n"));
		goto servfail;
	}
	if (!res_dnok(dname)) {
		dprintf(1, (ddt, "bad name (%s)\n", dname));
		goto servfail;
	}
	cp += n + QFIXEDSZ;
	buflen = sizeof(newmsg) - (cp - newmsg);

	cname = 0;
 try_again:
	dprintf(1, (ddt, "resp: nlookup(%s) qtype=%d\n", dname, qtype));
	fname = "";
	htp = hashtab;		/* lookup relative to root */
	np = nlookup(dname, &htp, &fname, 0);
	dprintf(1, (ddt, "resp: %s '%s' as '%s' (cname=%d)\n",
		    np == NULL ? "missed" : "found", dname, fname, cname));
	if (np == NULL || fname != dname)
		goto fetch_ns;

	foundname++;
	count = cp - newmsg;
	n = finddata(np, qclass, qtype, hp, &dname, &buflen, &count);
	if (n == 0)
		goto fetch_ns;		/* NO data available */
#ifdef NCACHE
	if (hp->rcode) {
		if (hp->rcode == NOERROR_NODATA)
			hp->rcode = NOERROR;
#ifdef RETURNSOA
		if (count) {
			cp += n;
			buflen -= n;
			hp->nscount = htons((u_int16_t)count);
		}
#endif
		goto return_newmsg;
	}
#endif
	cp += n;
	buflen -= n;
	hp->ancount = htons(ntohs(hp->ancount) + (u_int16_t)count);
	if (fname != dname && qtype != T_CNAME && qtype != T_ANY) {
		cname++;
		goto try_again;
	}
	founddata = 1;

	dprintf(3, (ddt,
		    "resp: foundname=%d, count=%d, founddata=%d, cname=%d\n",
		    foundname, count, founddata, cname));

 fetch_ns:

	if (hp->tc)
		goto return_newmsg;

	/*
 	 * Look for name servers to refer to and fill in the authority
 	 * section or record the address for forwarding the query
 	 * (recursion desired).
 	 */
	free_nsp(nsp);
	switch (findns(&np, qclass, nsp, &count, 0)) {
	case NXDOMAIN:		/* shouldn't happen */
		dprintf(3, (ddt, "req: leaving (%s, rcode %d)\n",
			    dname, hp->rcode));
		if (!foundname)
			hp->rcode = NXDOMAIN;
		if (qclass != C_ANY) {
			hp->aa = 1;
			if (np && (!foundname || !founddata)) {
				n = doaddauth(hp, cp, buflen, np, nsp[0]);
				cp += n;
				buflen -= n;
			}
		}
		goto return_newmsg;

	case SERVFAIL:
		goto servfail;
	}

	if (founddata) {
		hp = (HEADER *)newmsg;
		n = add_data(np, nsp, cp, buflen, &count);
		if (n < 0) {
			hp->tc = 1;
			n = (-n);
		}
		cp += n;
		buflen -= n;
		hp->nscount = htons((u_int16_t)count);
		goto return_newmsg;
	}

	/*
	 *  If we get here, we don't have the answer yet and are about
	 *  to iterate to try and get it.  First, infinite loop avoidance.
	 */
	if (qp->q_nqueries++ > MAXQUERIES) {
		dprintf(1, (ddt, "resp: MAXQUERIES exceeded (%s %s %s)\n",
			    dname, p_class(qclass), p_type(qtype)));
		syslog(LOG_INFO,
		       "MAXQUERIES exceeded, possible data loop in resolving (%s)",
		       dname);
		goto servfail;
	}

	/* Reset the query control structure */

	nsfree(qp, "ns_resp");
	qp->q_naddr = 0;
	qp->q_curaddr = 0;
	qp->q_fwd = fwdtab;

	getname(np, tmpdomain, sizeof tmpdomain);
	if (qp->q_domain != NULL)
		free(qp->q_domain);
	qp->q_domain = strdup(tmpdomain);
	if (qp->q_domain == NULL)
		panic(ENOMEM, "ns_resp: strdup failed");

	if ((n = nslookup(nsp, qp, dname, "ns_resp")) <= 0) {
		if (n < 0) {
			dprintf(3, (ddt, "resp: nslookup reports danger\n"));
			if (cname)  /* a remote CNAME that does not have data */
				goto return_newmsg;
			goto servfail;
		} else {
			dprintf(3, (ddt, "resp: no addrs found for NS's\n"));
			/*
			 * Timeout while sysquery looks up the NS addresses.
			 *
			 * Hopefully we'll have them when the client asks
			 * again.
			 *
			 * too bad we can't just wait for the sysquery
			 * response to restart this query (it's too hard).
			 *
			 * We could try to crawl back up the tree looking
			 * for reachable servers, but we may have just
			 * gotten delegated down here by a response with
			 * no A RRs for the servers.  If we blindly tried
			 * this strategy, we bang on the same server forever.
			 */
			goto timeout;
		}
	}
	for (n = 0; (u_int)n < qp->q_naddr; n++)
		qp->q_addr[n].stime.tv_sec = 0;
	if (!qp->q_fwd)
		qp->q_addr[0].stime = tt;
	if (cname) {
	 	if (qp->q_cname++ == MAXCNAMES) {
			dprintf(3, (ddt,
				    "resp: leaving, MAXCNAMES exceeded\n"));
			goto servfail;
	 	}
		dprintf(1, (ddt, "q_cname = %d\n", qp->q_cname));
		dprintf(3, (ddt,
			    "resp: building recursive query; nslookup\n"));
		if (!qp->q_cmsg) {
			qp->q_cmsg = qp->q_msg;
			qp->q_cmsglen = qp->q_msglen;
		} else if (qp->q_msg)
			(void) free(qp->q_msg);
		if ((qp->q_msg = (u_char *)malloc(PACKETSZ)) == NULL) {
			syslog(LOG_NOTICE, "resp: malloc error\n");
			goto servfail;
		}
		n = res_mkquery(QUERY, dname, qclass, qtype,
				NULL, 0, NULL, qp->q_msg, PACKETSZ);
		if (n < 0) {
			syslog(LOG_INFO, "resp: res_mkquery(%s) failed",
			       dname);
			goto servfail;
		}
		if (qp->q_name != NULL)
			free(qp->q_name);
		qp->q_name = savestr(dname);
		qp->q_msglen = n;
		hp = (HEADER *) qp->q_msg;
		hp->rd = 0;
	} else
		hp = (HEADER *) qp->q_msg;
	hp->id = qp->q_nsid = htons(nsid_next());
	if (qp->q_fwd)
		hp->rd = 1;
	unsched(qp);
	schedretry(qp, retrytime(qp));
	nsa = Q_NEXTADDR(qp, 0);
	dprintf(1, (ddt, "resp: forw -> %s ds=%d nsid=%d id=%d %dms\n",
		    sin_ntoa(nsa), ds,
		    ntohs(qp->q_nsid), ntohs(qp->q_id),
		    (qp->q_addr[0].nsdata != NULL)
			? qp->q_addr[0].nsdata->d_nstime
			: (-1)));
#ifdef DEBUG
	if (debug >= 10)
		fp_nquery(qp->q_msg, qp->q_msglen, ddt);
#endif
	if (sendto(ds, (char*)qp->q_msg, qp->q_msglen, 0,
		   (struct sockaddr *)nsa,
		   sizeof(struct sockaddr_in)) < 0) {
		if (!haveComplained((char*)nsa->sin_addr.s_addr, sendtoStr))
			syslog(LOG_INFO, "ns_resp: sendto(%s): %m",
			       sin_ntoa(nsa));
		nameserIncr(nsa->sin_addr, nssSendtoErr);
	}
	hp->rd = 0;	/* leave set to 0 for dup detection */
#ifdef XSTATS
	nameserIncr(nsa->sin_addr, nssSentFwdR);
#endif
	nameserIncr(qp->q_from.sin_addr, nssRcvdFwdR);
	dprintf(3, (ddt, "resp: Query sent.\n"));
	free_nsp(nsp);
	return;

 formerr:
	if (!haveComplained((char*)from_addr.sin_addr.s_addr,
			    (char*)nhash(formerrmsg)))
		syslog(LOG_INFO, "Malformed response from %s (%s)\n",
		       sin_ntoa(&from_addr), formerrmsg);
#ifdef XSTATS
	nameserIncr(from_addr.sin_addr, nssSentFErr);
#endif
	free_nsp(nsp);
	return;

 return_msg:
	nameserIncr(from_addr.sin_addr, nssRcvdFwdR);
#ifdef XSTATS
	nameserIncr(qp->q_from.sin_addr, nssSentFwdR);
#endif
	/* The "standard" return code */
	hp->qr = 1;
	hp->id = qp->q_id;
	hp->rd = 1;
	hp->ra = (NoRecurse == 0);
	(void) send_msg(msg, msglen, qp);
	qremove(qp);
	free_nsp(nsp);
	return;

 return_newmsg:
	nameserIncr(qp->q_from.sin_addr, nssSentAns);

#ifdef XSTATS
	if (!hp->aa)
		nameserIncr(qp->q_from.sin_addr, nssSentNaAns);
	if (hp->rcode == NXDOMAIN) 
		nameserIncr(qp->q_from.sin_addr, nssSentNXD);
#endif
	n = doaddinfo(hp, cp, buflen);
	cp += n;
	buflen -= n;
	hp->qr = 1;
	hp->id = qp->q_id;
	hp->rd = 1;
	hp->ra = (NoRecurse == 0);
	(void) send_msg(newmsg, cp - newmsg, qp);
	qremove(qp);
	free_nsp(nsp);
	return;

 servfail:
#ifdef XSTATS
	nameserIncr(qp->q_from.sin_addr, nssSentFail);
#endif
	hp = (HEADER *)(qp->q_cmsglen ? qp->q_cmsg : qp->q_msg);
	hp->rcode = SERVFAIL;
	hp->qr = 1;
	hp->id = qp->q_id;
	hp->rd = 1;
	hp->ra = (NoRecurse == 0);
	(void) send_msg((u_char *)hp, (qp->q_cmsglen ? qp->q_cmsglen : qp->q_msglen),
			qp);
 timeout:
	if (qp->q_stream != QSTREAM_NULL)
		sqrm(qp->q_stream);
	qremove(qp);
	free_nsp(nsp);
	return;
}

#define BOUNDS_CHECK(ptr, count) \
	do { \
		if ((ptr) + (count) > eom) { \
			hp->rcode = FORMERR; \
			return (-1); \
		} \
	} while (0)

static int
rrextract(msg, msglen, rrp, dpp, dname, namelen, tnamep)
	u_char *msg;
	int msglen;
	u_char *rrp;
	struct databuf **dpp;
	char *dname;
	int namelen;
	char **tnamep;
{
	register u_char *cp, *eom, *rdatap;
	register int n;
	int class, type, dlen, n1;
	u_int32_t ttl;
	u_char *cp1;
	u_char data[MAXDNAME*2 + INT32SZ*5];
	register HEADER *hp = (HEADER *)msg;
	enum context context;

	if (tnamep != NULL)
		*tnamep = NULL;

	*dpp = NULL;
	cp = rrp;
	eom = msg + msglen;
	if ((n = dn_expand(msg, eom, cp, dname, namelen)) < 0) {
		hp->rcode = FORMERR;
		return (-1);
	}
	cp += n;
	BOUNDS_CHECK(cp, 2*INT16SZ + INT32SZ + INT16SZ);
	GETSHORT(type, cp);
	GETSHORT(class, cp);
	GETLONG(ttl, cp);
	GETSHORT(dlen, cp);
	BOUNDS_CHECK(cp, dlen);
	rdatap = cp;
	if (!ns_nameok(dname, class, response_trans,
		       ns_ownercontext(type, response_trans),
		       dname, from_addr.sin_addr)) {
		hp->rcode = FORMERR;
		return (-1);
	}
	dprintf(3, (ddt, "rrextract: dname %s type %d class %d ttl %d\n",
		    dname, type, class, ttl));
	/*
	 * Convert the resource record data into the internal
	 * database format.
	 *
	 * On entry to the switch:
	 *   CP points to the RDATA section of the wire-format RR.
	 *   DLEN is its length.
	 *   The memory area at DATA is available for processing.
	 * 
	 * On exit from the switch:
	 *   CP has been incremented past the RR.
	 *   CP1 points to the RDATA section of the database-format RR.
	 *   N contains the length of the RDATA section of the dbase-format RR.
	 *
	 * The new data at CP1 for length N will be copied into the database,
	 * so it need not be in any particular storage location.
	 */
	switch (type) {
	case T_A:
		if (dlen != INT32SZ) {
			hp->rcode = FORMERR;
			return (-1);
		}
		/*FALLTHROUGH*/
	case T_WKS:
	case T_HINFO:
	case T_UINFO:
	case T_UID:
	case T_GID:
	case T_TXT:
	case T_X25:
	case T_ISDN:
	case T_NSAP:
	case T_AAAA:
	case T_LOC:
	case T_KEY:
#ifdef ALLOW_T_UNSPEC
	case T_UNSPEC:
#endif
		cp1 = cp;
		n = dlen;
		cp += n;
		break;

	case T_CNAME:
	case T_MB:
	case T_MG:
	case T_MR:
	case T_NS:
	case T_PTR:
		n = dn_expand(msg, eom, cp, (char *)data, sizeof data);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		if (!ns_nameok((char *)data, class, response_trans,
			       type == T_PTR ?ns_ptrcontext(dname) :domain_ctx,
			       dname, from_addr.sin_addr)) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 = data;
		n = strlen((char *)data) + 1;
		if (tnamep != NULL && (type == T_NS || type == T_MB))
			*tnamep = strdup((char *)cp1);
		break;

	case T_SOA:
		context = hostname_ctx;
		goto soa_rp_minfo;
	case T_RP:
	case T_MINFO:
		context = mailname_ctx;
		/* FALLTHROUGH */
	soa_rp_minfo:
		n = dn_expand(msg, eom, cp, (char *)data, sizeof data);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		if (!ns_nameok((char *)data, class, response_trans, context,
			       dname, from_addr.sin_addr)) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		/*
		 * The next use of 'cp' is dn_expand(), so we don't have
		 * to BOUNDS_CHECK() here.
		 */
		cp1 = data + (n = strlen((char *)data) + 1);
		n1 = sizeof(data) - n;
		if (type == T_SOA)
			n1 -= 5 * INT32SZ;
		n = dn_expand(msg, eom, cp, (char *)cp1, n1);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		if (type == T_RP)
			context = domain_ctx;
		else
			context = mailname_ctx;
		if (!ns_nameok((char *)cp1, class, response_trans, context,
			       dname, from_addr.sin_addr)) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 += strlen((char *)cp1) + 1;
		if (type == T_SOA) {
			n = 5 * INT32SZ;
			BOUNDS_CHECK(cp, n);
			bcopy(cp, cp1, n);
			cp += n;
			cp1 += n;
		}
		n = cp1 - data;
		cp1 = data;
		break;

	case T_NAPTR:
		/* Grab weight and port. */
		BOUNDS_CHECK(cp, INT16SZ*2);
		bcopy(cp, data, INT16SZ*2);
		cp1 = data + INT16SZ*2;
		cp += INT16SZ*2;

		/* Flags */
		BOUNDS_CHECK(cp, 1);
		n = *cp++;
		BOUNDS_CHECK(cp, n);
		*cp1++ = n;
		bcopy(cp, cp1, n);
		cp += n; cp1 += n;

		/* Service */
		BOUNDS_CHECK(cp, 1);
		n = *cp++;
		BOUNDS_CHECK(cp, n);
		*cp1++ = n;
		bcopy(cp, cp1, n);
		cp += n; cp1 += n;

		/* Regexp */
		BOUNDS_CHECK(cp, 1);
		n = *cp++;
		BOUNDS_CHECK(cp, n);
		*cp1++ = n;
		bcopy(cp, cp1, n);
		cp += n; cp1 += n;

		/* Replacement */
		n = dn_expand(msg, eom, cp, (char *)cp1,
			      sizeof data - (cp1 - data));
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		if (!ns_nameok((char *)cp1, class, response_trans,
			       hostname_ctx, dname, from_addr.sin_addr)) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;

		/* compute end of data */
		cp1 += strlen((char *)cp1) + 1;
		/* compute size of data */
		n = cp1 - data;
		cp1 = data;
		break;

	case T_MX:
	case T_AFSDB:
	case T_RT:
	case T_SRV:
		/* grab preference */
		BOUNDS_CHECK(cp, INT16SZ);
		bcopy(cp, data, INT16SZ);
		cp1 = data + INT16SZ;
		cp += INT16SZ;

		if (type == T_SRV) {
			/* Grab weight and port. */
			BOUNDS_CHECK(cp, INT16SZ*2);
			bcopy(cp, cp1, INT16SZ*2);
			cp1 += INT16SZ*2;
			cp += INT16SZ*2;
		}

		/* get name */
		n = dn_expand(msg, eom, cp, (char *)cp1,
			      sizeof data - (cp1 - data));
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		if (!ns_nameok((char *)cp1, class, response_trans,
			       hostname_ctx, dname, from_addr.sin_addr)) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;

		if (tnamep != NULL)
			*tnamep = strdup((char *)cp1);

		/* compute end of data */
		cp1 += strlen((char *)cp1) + 1;
		/* compute size of data */
		n = cp1 - data;
		cp1 = data;
		break;

	case T_PX:
		/* grab preference */
		BOUNDS_CHECK(cp, INT16SZ);
		bcopy(cp, data, INT16SZ);
		cp1 = data + INT16SZ;
		cp += INT16SZ;

		/* get MAP822 name */
		n = dn_expand(msg, eom, cp, (char *)cp1,
			      sizeof data - INT16SZ);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		if (!ns_nameok((char *)cp1, class, response_trans,
			       domain_ctx, dname, from_addr.sin_addr)) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		/*
		 * The next use of 'cp' is dn_expand(), so we don't have
		 * to BOUNDS_CHECK() here.
		 */
		cp1 += (n = strlen((char *)cp1) + 1);
		n1 = sizeof(data) - n;
		n = dn_expand(msg, eom, cp, (char *)cp1, n1);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		if (!ns_nameok((char *)cp1, class, response_trans,
			       domain_ctx, dname, from_addr.sin_addr)) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 += strlen((char *)cp1) + 1;
		n = cp1 - data;
		cp1 = data;
		break;

	case T_SIG: {
		u_long origTTL, exptime, signtime, timetilexp, now;

		/* Check signature time, expiration, and adjust TTL.  */
		/* This code is similar to that in db_load.c.  */

		/* Skip coveredType, alg, labels */
		BOUNDS_CHECK(cp, INT16SZ + 1 + 1 + 3*INT32SZ);
		cp1 = cp + INT16SZ + 1 + 1;
		GETLONG(origTTL, cp1);
		GETLONG(exptime, cp1);
		GETLONG(signtime, cp1);
		now = time(NULL);	/* Get current time in GMT/UTC */

		/* Don't let bogus name servers increase the signed TTL */
		if (ttl > origTTL) {
			dprintf(3, (ddt,
				 "shrinking SIG TTL from %d to origTTL %d\n",
				ttl, origTTL));
			ttl = origTTL;
		}

		/* Don't let bogus signers "sign" in the future.  */
		if (signtime > now) {
			dprintf(3, (ddt,
			  "ignoring SIG: signature date %s is in the future\n",
				    p_secstodate (signtime)));
			return ((cp - rrp) + dlen);
		}
		
		/* Ignore received SIG RR's that are already expired.  */
		if (exptime <= now) {
			dprintf(3, (ddt,
				 "ignoring SIG: expiration %s is in the past\n",
				    p_secstodate (exptime)));
			return ((cp - rrp) + dlen);
		}

		/* Lop off the TTL at the expiration time.  */
		timetilexp = exptime - now;
		if (timetilexp < ttl) {
			dprintf(3, (ddt,
				"shrinking expiring %s SIG TTL from %d to %d\n",
				    p_secstodate (exptime), ttl, timetilexp));
			ttl = timetilexp;
		}

		/* The following code is copied from named-xfer.c.  */
		cp1 = (u_char *)data;

		/* first just copy over the type_covered, algorithm, */
		/* labels, orig ttl, two timestamps, and the footprint */
		BOUNDS_CHECK(cp, 18);
		bcopy(cp, cp1, 18);
		cp  += 18;
		cp1 += 18;

		/* then the signer's name */
		n = dn_expand(msg, eom, cp, (char *)cp1, (sizeof data) - 18);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 += strlen((char*)cp1)+1;

		/* finally, we copy over the variable-length signature.
		   Its size is the total data length, minus what we copied. */
		if (18 + (u_int)n > dlen) {
			hp->rcode = FORMERR;
			return (-1);
		}
		n = dlen - (18 + n);
		if (n > ((int)(sizeof data) - (int)(cp1 - (u_char *)data))) {
			hp->rcode = FORMERR;
			return (-1);  /* out of room! */
		}
		bcopy(cp, cp1, n);
		cp += n;
		cp1 += n;
		
		/* compute size of data */
		n = cp1 - (u_char *)data;
		cp1 = (u_char *)data;
		break;
	    }

	default:
		dprintf(3, (ddt, "unknown type %d\n", type));
		return ((cp - rrp) + dlen);
	}

	if (cp > eom) {
		hp->rcode = FORMERR;
		return (-1);
	}
	if ((u_int)(cp - rdatap) != dlen) {
		dprintf(3, (ddt,
		      "encoded rdata length is %u, but actual length was %u",
			    dlen, (u_int)(cp - rdatap)));
		hp->rcode = FORMERR;
		return (-1);
	}
	if (n > MAXDATA) {
		dprintf(1, (ddt,
			    "update type %d: %d bytes is too much data\n",
			    type, n));
		hp->rcode = FORMERR;
		return (-1);
	}

	ttl += tt.tv_sec;

	*dpp = savedata(class, type, ttl, cp1, n);
	return (cp - rrp);
}

/*
 * Decode the resource record 'rrp' and update the database.
 * If savens is non-nil, record pointer for forwarding queries a second time.
 */
int
doupdate(msg, msglen, rrp, zone, savens, flags, cred)
	u_char *msg;
	int msglen;
	u_char *rrp;
	int zone;
	struct databuf **savens;
	int flags;
	u_int cred;
{
	register u_char *cp;
	register int n;
	int class, type;
	struct databuf *dp;
	char dname[MAXDNAME];

	dprintf(3, (ddt, "doupdate(zone %d, savens %#lx, flags %#lx)\n",
		    zone, (u_long)savens, (u_long)flags));

	if ((n = rrextract(msg, msglen, rrp, &dp, dname, sizeof(dname), NULL))
	    == -1)
		return (-1);
	if (!dp)
		return (-1);

	type = dp->d_type;
	class = dp->d_class;
	cp = rrp + n;

#if defined(TRACEROOT) || defined(BOGUSNS)
	if ((type == T_NS) && (savens != NULL)) {
		char *temp, qname[MAXDNAME];
		register int bogus = 0;
		int bogusns = 0;
#ifdef BOGUSNS
		if (addr_on_netlist(from_addr.sin_addr, boglist)) {
			bogusns++;
			bogus++;
		}
#endif
		if (!bogus &&
		    ((temp = strrchr((char *)dp->d_data, '.')) != NULL) &&
		     !strcasecmp(temp, ".arpa")
		     )
			bogus++;
		qname[0] = qname[1] = '\0';
		if (dn_expand(msg, msg + msglen, msg + HFIXEDSZ,
			      qname, sizeof(qname)) < 0)
			qname[0] = '?';
		else if (qname[0] == '\0')
			qname[0] = '.';
		if (bogus && ((dname[0] == '\0') && (zone == 0))) {
			if (!haveComplained((char*)from_addr.sin_addr.s_addr,
					    "bogus root NS"))
				syslog(LOG_NOTICE,
			 "bogus root NS %s rcvd from %s on query for \"%s\"",
				       dp->d_data, sin_ntoa(&from_addr), qname);
			db_free(dp);
			return (cp - rrp);
		}
#ifdef BOGUSNS
		if (bogusns) {
			if (!haveComplained((char*)from_addr.sin_addr.s_addr,
					    "bogus nonroot NS"))
				syslog(LOG_INFO,
			"bogus nonroot NS %s rcvd from %s on query for \"%s\"",
				       dp->d_data, sin_ntoa(&from_addr), qname);
			db_free(dp);
			return (cp - rrp);
		}
#endif
	}
#endif /*TRACEROOT || BOGUSNS*/

	dp->d_zone = zone;
	dp->d_cred = cred;
	dp->d_clev = 0;	/* We trust what is on disk more, except root srvrs */
	if ((n = db_update(dname, dp, dp, flags, hashtab)) != OK) {
#ifdef DEBUG
		if (debug && (n != DATAEXISTS))
			fprintf(ddt, "update failed (%d)\n", n);
		else if (debug >= 3)
			fprintf(ddt, "update failed (DATAEXISTS)\n");
#endif
		db_free(dp);
	} else if (type == T_NS && savens != NULL)
		*savens = dp;
	return (cp - rrp);
}

int
send_msg(msg, msglen, qp)
	u_char *msg;
	int msglen;
	struct qinfo *qp;
{
	if (qp->q_flags & Q_SYSTEM)
		return (1);
#ifdef DEBUG
	if (debug) {
		fprintf(ddt,"send_msg -> %s (%s %d) id=%d\n",
			sin_ntoa(&qp->q_from), 
			qp->q_stream == QSTREAM_NULL ? "UDP" : "TCP",
			qp->q_stream == QSTREAM_NULL ? qp->q_dfd
						     : qp->q_stream->s_rfd,
			ntohs(qp->q_id));
	}
	if (debug > 4) {
		struct qinfo *tqp;

		for (tqp = nsqhead; tqp!=QINFO_NULL; tqp = tqp->q_link) {
			fprintf(ddt,
				"qp %#lx q_id: %d  q_nsid: %d q_msglen: %d ",
				(u_long)tqp, tqp->q_id,
				tqp->q_nsid, tqp->q_msglen);
			fprintf(ddt,
				"q_naddr: %d q_curaddr: %d\n",
				tqp->q_naddr, tqp->q_curaddr);
			fprintf(ddt, "q_next: %#lx q_link: %#lx\n",
				(u_long)qp->q_next, (u_long)qp->q_link);
		}
	}
	if (debug > 5)
		fp_nquery(msg, msglen, ddt);
#endif /* DEBUG */
	if (qp->q_stream == QSTREAM_NULL) {
		if (sendto(qp->q_dfd, (char*)msg, msglen, 0,
			   (struct sockaddr *)&qp->q_from,
			   sizeof(qp->q_from)) < 0) {
			if (!haveComplained((char*)qp->q_from.sin_addr.s_addr,
					    sendtoStr))
#if defined(SPURIOUS_ECONNREFUSED)
                           if (errno != ECONNREFUSED)
#endif
				syslog(LOG_INFO,
				       "send_msg: sendto(%s): %m",
				       sin_ntoa(&qp->q_from));
			nameserIncr(qp->q_from.sin_addr, nssSendtoErr);
			return (1);
		}
	} else {
		(void) writemsg(qp->q_stream->s_rfd, (u_char*)msg, msglen);
		sq_done(qp->q_stream);
	}
	return (0);
}

#ifdef notdef
/* i don't quite understand this but the only ref to it is notdef'd --vix */
prime(class, type, oqp)
	int class, type;
	register struct qinfo *oqp;
{
	char	dname[BUFSIZ];

	if (oqp->q_msg == NULL)
		return;
	if (dn_expand((u_char *)oqp->q_msg,
	    (u_char *)oqp->q_msg + oqp->q_msglen,
	    (u_char *)oqp->q_msg + HFIXEDSZ, (u_char *)dname,
	    sizeof(dname)) < 0)
		return;
	dprintf(2, (ddt, "prime: %s\n", dname));
	(void) sysquery(dname, class, type, NULL, 0, QUERY);
}
#endif

void
prime_cache()
{
	register struct qinfo *qp;

	dprintf(1, (ddt, "prime_cache: priming = %d\n", priming));
	if (!priming && fcachetab->h_tab[0] != NULL && !forward_only) {
		priming++;
		if (!(qp = sysquery("", C_IN, T_NS, NULL, 0, QUERY)))
			priming = 0;
		else
			qp->q_flags |= (Q_SYSTEM | Q_PRIMING);
	}
	needs_prime_cache = 0;
	return;
}

#ifdef BIND_NOTIFY
struct notify *
findNotifyPeer(zp, ina)
	const struct zoneinfo *zp;
	struct in_addr ina;
{
	register struct notify *ap;

	for (ap = zp->z_notifylist; ap; ap = ap->next)
		if (ap->addr.s_addr == ina.s_addr)
			break;
	return (ap);
}

/* sysnotify(dname, class, type)
 *	cause a NOTIFY request to be sysquery()'d to each secondary server
 *	of the zone that "dname" is within.
 */
void
sysnotify(dname, class, type)
	const char *dname;
	int class, type;
{
	char *soaname, *zname;
	const char *fname;
	register struct databuf *dp;
	struct in_addr nss[NSMAX];
	int nns, na, zn, nsc;
	struct hashbuf *htp;
	struct zoneinfo *zp;
	struct notify *ap;
	struct namebuf *np;

	htp = hashtab;
	np = nlookup(dname, &htp, &fname, 0);
	if (!np)
		panic(-1, "sysnotify: can't find name");
	zn = findMyZone(np, class);
	if (zn == DB_Z_CACHE)
		panic(-1, "sysnotify: not auth zone");
	zp = &zones[zn];
	if (zp->z_type != Z_PRIMARY && zp->z_type != Z_SECONDARY)
		panic(-1, "sysnotify: not pri/sec");
	zname = zp->z_origin;
/*
**DBG**	syslog(LOG_INFO, "sysnotify: found \"%s\" in \"%s\" (%s)",
**DBG**	       dname, zname, zoneTypeString(zp));
*/
	nns = na = 0;
	/*
	 * Send to recent AXFR peers.
	 */
	for (ap = zp->z_notifylist; ap; ap = ap->next) {
		if (tt.tv_sec - ap->last >= zp->z_refresh) {
			/* XXX - probably should do GC here. */
			continue;
		}
		nss[0] = ap->addr;
		nsc = 1;
		nns++;
		na++;
		sysquery(dname, class, T_SOA, nss, nsc, NS_NOTIFY_OP);
	}
	if (zp->z_type != Z_PRIMARY)
		goto done;
	/*
	 * Master.
	 */
	htp = hashtab;
	np = nlookup(zname, &htp, &fname, 0);
	if (!np)
		panic(-1, "sysnotify: found name but not zone");
	soaname = NULL;
	for (dp = np->n_data; dp; dp = dp->d_next) {
		if (!dp->d_zone || !match(dp, class, T_SOA))
			continue;
		if (soaname) {
			syslog(LOG_NOTICE, "multiple SOA's for zone \"%s\"?",
			       zname);
			return;
		}
		soaname = (char *) dp->d_data;
	}
	if (!soaname) {
		syslog(LOG_NOTICE, "no SOA found for zone \"%s\"", zname);
		return;
	}

	for (dp = np->n_data; dp; dp = dp->d_next) {
		register struct databuf *adp;
		struct namebuf *anp;

		if (!dp->d_zone || !match(dp, class, T_NS))
			continue;
		/* NS RDATA is server name. */
		if (strcasecmp((char*)dp->d_data, soaname) == 0)
			continue;
		htp = hashtab;
		anp = nlookup((char*)dp->d_data, &htp, &fname, 0);
		if (!anp) {
			syslog(LOG_INFO, "sysnotify: can't nlookup(%s)?",
			       (char*)dp->d_data);
			continue;
		}
		nsc = 0;
		for (adp = anp->n_data; adp; adp = adp->d_next) {
			struct in_addr ina;
			if (!match(adp, class, T_A))
				continue;
			ina = data_inaddr(adp->d_data);
			/* Don't send to things we handled above. */
			ap = findNotifyPeer(zp, ina);
			if (ap && tt.tv_sec - ap->last < zp->z_refresh)
				goto nextns;
			if (nsc < NSMAX)
				nss[nsc++] = ina;
		} /*next A*/
		if (nsc == 0) {
			struct qinfo *qp;

			qp = sysquery((char*)dp->d_data,	/*NS name*/
				      class,			/*XXX: C_IN?*/
				      T_A, 0, 0, QUERY);
			if (qp)
				qp->q_notifyzone = zn;
			continue;
		}
		(void) sysquery(dname, class, T_SOA, nss, nsc, NS_NOTIFY_OP);
		nns++;
		na += nsc;
 nextns:;
	} /*next NS*/
 done:
	if (nns || na) {
		char tmp[MAXDNAME];

		/* Many syslog()'s only take 5 args. */
		sprintf(tmp, "%s %s %s", dname, p_class(class), p_type(type));
		syslog(LOG_INFO, "Sent NOTIFY for \"%s\" (%s); %d NS, %d A",
		       tmp, zname, nns, na);
	}
}
#endif /*BIND_NOTIFY*/

struct qinfo *
sysquery(dname, class, type, nss, nsc, opcode)
	const char *dname;
	int class, type;
	struct in_addr *nss;
	int nsc, opcode;
{
	register struct qinfo *qp, *oqp;
	register HEADER *hp;
	char tmpdomain[MAXDNAME];
	struct namebuf *np;
	struct databuf *nsp[NSMAX];
	struct hashbuf *htp;
	struct sockaddr_in *nsa;
	const char *fname;
	int n, count;

	nsp[0] = NULL;
	dprintf(3, (ddt, "sysquery(%s, %d, %d, %#lx, %d)\n",
		    dname, class, type, (u_long)nss, nsc));
	qp = qnew(dname, class, type);

	if (nss && nsc) {
		np = NULL;
	} else {
		htp = hashtab;
		if (priming && dname[0] == '\0') {
			np = NULL;
		} else if ((np = nlookup(dname, &htp, &fname, 1)) == NULL) {
			syslog(LOG_INFO, "sysquery: nlookup error on %s?",
			       dname);
 err1:
			qfree(qp);
			return (NULL);
		}

		n = findns(&np, class, nsp, &count, 0);
		switch (n) {
		case NXDOMAIN:
		case SERVFAIL:
			syslog(LOG_DEBUG, "sysquery: findns error (%s) on %s?",
			       n == NXDOMAIN ? "NXDOMAIN" : "SERVFAIL", dname);
 err2:
			free_nsp(nsp);
			goto err1;
		}
	}

	/* build new qinfo struct */
	qp->q_cmsg = qp->q_msg = NULL;
	qp->q_dfd = ds;
	if (nss && nsc)
		qp->q_fwd = NULL;
	else
		qp->q_fwd = fwdtab;
	qp->q_expire = tt.tv_sec + RETRY_TIMEOUT*2;
	qp->q_flags |= Q_SYSTEM;

	getname(np, tmpdomain, sizeof tmpdomain);
	if (qp->q_domain != NULL)
		free(qp->q_domain);
	qp->q_domain = strdup(tmpdomain);
	if (qp->q_domain == NULL)
		panic(ENOMEM, "ns_resp: strdup failed");

	if ((qp->q_msg = (u_char *)malloc(PACKETSZ)) == NULL) {
		syslog(LOG_NOTICE, "sysquery: malloc failed");
		goto err2;
	}
	n = res_mkquery(opcode, dname, class,
			type, NULL, 0, NULL,
			qp->q_msg, PACKETSZ);
	if (n < 0) {
		syslog(LOG_INFO, "sysquery: res_mkquery(%s) failed", dname);
		goto err2;
	}
	qp->q_msglen = n;
	hp = (HEADER *) qp->q_msg;
	hp->id = qp->q_nsid = htons(nsid_next());
	hp->rd = (qp->q_fwd ? 1 : 0);

	/* First check for an already pending query for this data */
	for (oqp = nsqhead; oqp != QINFO_NULL; oqp = oqp->q_link) {
		if ((oqp != qp)
		    && (oqp->q_msglen == qp->q_msglen)
		    && bcmp((char *)oqp->q_msg+2,
			    qp->q_msg+2,
			    qp->q_msglen-2) == 0
		    ) {
#ifdef BIND_NOTIFY
			/* XXX - need fancier test to suppress duplicate
			 *       NOTIFYs to the same server (compare nss?)
			 */
			if (opcode != NS_NOTIFY_OP)
#endif /*BIND_NOTIFY*/
			{
			    dprintf(3, (ddt, "sysquery: duplicate\n"));
			    goto err2;
			}
		}
	}

	if (nss && nsc) {
		int i;
		struct qserv *qs;

		for (i = 0, qs = qp->q_addr;
		     i < nsc;
		     i++, qs++) {
			qs->ns_addr.sin_family = AF_INET;
			qs->ns_addr.sin_addr = nss[i];
			qs->ns_addr.sin_port = ns_port;
			qs->ns = NULL;
			qs->nsdata = NULL;
			qs->stime = tt;
			qs->nretry = 0;
		}
		qp->q_naddr = nsc;
	} else {
 fetch_a:
		count = nslookup(nsp, qp, dname, "sysquery");
		if (count <= 0) {
			if (count < 0) {
				syslog(LOG_INFO,
				      "sysquery: nslookup reports danger (%s)",
				       dname);
				goto err2;
			} else if (np && NAME(*np)[0] == '\0') {
				syslog(LOG_WARNING,
				   "sysquery: no addrs found for root NS (%s)",
				       dname);
				if (class == C_IN && !priming)
					needs_prime_cache = 1;
				goto err2;
			}
			if (np) {
				free_nsp(nsp);
				nsp[0] = NULL;
				np = np_parent(np);
				n = findns(&np, class, nsp, &count, 0);
				switch (n) {
				case NXDOMAIN: /*FALLTHROUGH*/
				case SERVFAIL:
					syslog(LOG_DEBUG,
					  "sysquery: findns error (%d) on %s?",
					       n, dname);
					goto err2;
				}
				goto fetch_a;
			}
			goto err2;
		}
	}

	schedretry(qp, retrytime(qp));
	if (qp->q_fwd == NULL)
		qp->q_addr[0].stime = tt;	/* XXX - why not every? */
	nsa = Q_NEXTADDR(qp, 0);

	dprintf(1, (ddt,
		    "sysquery: send -> %s dfd=%d nsid=%d id=%d retry=%ld\n",
		    sin_ntoa(nsa), qp->q_dfd, 
		    ntohs(qp->q_nsid), ntohs(qp->q_id),
		    (long)qp->q_time));
#ifdef DEBUG
	if (debug >= 10)
		fp_nquery(qp->q_msg, qp->q_msglen, ddt);
#endif
	if (sendto(qp->q_dfd, (char*)qp->q_msg, qp->q_msglen, 0,
		   (struct sockaddr *)nsa,
		   sizeof(struct sockaddr_in)) < 0) {
		if (!haveComplained((char*)nsa->sin_addr.s_addr, sendtoStr))
			syslog(LOG_INFO, "sysquery: sendto(%s): %m",
			       sin_ntoa(nsa));
		nameserIncr(nsa->sin_addr, nssSendtoErr);
	}
	nameserIncr(nsa->sin_addr, nssSentSysQ);
	free_nsp(nsp);
	return (qp);
}

/*
 * Check the list of root servers after receiving a response
 * to a query for the root servers.
 */
static int
check_root()
{
	register struct databuf *dp, *pdp;
	register struct namebuf *np;
	int count = 0;

	priming = 0;
	for (np = hashtab->h_tab[0]; np != NULL; np = np->n_next)
		if (NAME(*np)[0] == '\0')
			break;
	if (np == NULL) {
		syslog(LOG_NOTICE, "check_root: Can't find root!\n");
		return (0);
	}
	for (dp = np->n_data; dp != NULL; dp = dp->d_next)
		if (dp->d_type == T_NS)
			count++;
	dprintf(1, (ddt, "%d root servers\n", count));
	if (count < MINROOTS) {
		syslog(LOG_NOTICE,
		"check_root: %d root servers after query to root server < min",
		       count);
		return (0);
	}
	pdp = NULL;
	dp = np->n_data;
	while (dp != NULL) {
		if (dp->d_type == T_NS && dp->d_zone == 0 &&
		    dp->d_ttl < tt.tv_sec) {
			dprintf(1, (ddt, "deleting old root server '%s'\n",
				    dp->d_data));
			dp = rm_datum(dp, np, pdp);
			/* SHOULD DELETE FROM HINTS ALSO */
			continue;
		}
		pdp = dp;
		dp = dp->d_next;
	}
	if (check_ns())
		return (1);
	else {
		priming = 1;
		return (0);
	}
}

/* 
 * Check the root to make sure that for each NS record we have a A RR
 */
static int
check_ns()
{
	register struct databuf *dp, *tdp;
	register struct namebuf *np, *tnp;
	struct hashbuf *htp;
	char *dname;
	int found_arr;
	const char *fname;
	time_t curtime;
	int servers = 0, rrsets = 0;

	dprintf(2, (ddt, "check_ns()\n"));

	curtime = (u_int32_t) tt.tv_sec;
	for (np = hashtab->h_tab[0]; np != NULL; np = np->n_next) {
		if (NAME(*np)[0] != '\0')
			continue;
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			int cnames = 0;

#ifdef NCACHE
			if (dp->d_rcode)
				continue;
#endif
			if (dp->d_type != T_NS)
				continue;

			servers++;

	        	/* look for A records */
			dname = (caddr_t) dp->d_data;
			htp = hashtab;
			tnp = nlookup(dname, &htp, &fname, 0);
			if (tnp == NULL || fname != dname) {
				dprintf(3, (ddt,
					    "check_ns: %s: not found %s %#lx\n",
					    dname, fname, (u_long)tnp));
				sysquery(dname, dp->d_class, T_A, NULL,
					 0, QUERY);
				continue;
			}
			/* look for name server addresses */
			found_arr = 0;
			delete_stale(tnp);
			for (tdp = tnp->n_data;
			     tdp != NULL;
			     tdp = tdp->d_next) {
#ifdef NCACHE
				if (tdp->d_rcode)
					continue;
#endif
				if (tdp->d_type == T_CNAME)
					cnames++;
				if (tdp->d_type != T_A ||
				    tdp->d_class != dp->d_class)
					continue;
				if ((tdp->d_zone == 0) &&
				    (tdp->d_ttl < curtime)) {
					dprintf(3, (ddt,
						"check_ns: stale entry '%s'\n",
						    NAME(*tnp)));
					found_arr = 0;
					break;
				}
				found_arr++;
			}
			if (found_arr)
				rrsets++;
			else if (cnames > 0)
				syslog(LOG_INFO, "Root NS %s -> CNAME %s",
				       NAME(*np), NAME(*tnp));
			else
				sysquery(dname, dp->d_class, T_A, NULL,
					 0, QUERY);
	        }
	}

	dprintf(2, (ddt, "check_ns: %d %d\n", servers, rrsets));
	return ((servers<=2)?(rrsets==servers):((rrsets*2)>=servers));
}

/* int findns(npp, class, nsp, countp, flag)
 *	Find NS's or an SOA
 * npp, class:
 *	dname whose most enclosing NS is wanted
 * nsp, countp:
 *	result array and count; array will also be NULL terminated
 * flag:
 *	boolean: we're being called from ADDAUTH, bypass authority checks
 * return value:
 *	NXDOMAIN: we are authoritative for this {dname,class}
 *		  *countp is bogus, but nsp[] has a single SOA returned in it.
 *	SERVFAIL: we are auth but zone isn't loaded; or, no root servers found
 *		  *countp and nsp[] are bogus.
 *	OK: we are not authoritative, and here are the NS records we found.
 *		  *countp and nsp[] return NS records of interest.
 */
int
findns(npp, class, nsp, countp, flag)
	register struct namebuf **npp;
	int class;
	struct databuf **nsp;
	int *countp;
	int flag;
{
	register struct namebuf *np = *npp;
	register struct databuf *dp;
	register struct	databuf **nspp;
	struct hashbuf *htp;
	
	nsp[0] = NULL;

	if (priming && (np == NULL || NAME(*np)[0] == '\0'))
		htp = fcachetab;
	else
		htp = hashtab;

 try_again:
	if (htp == fcachetab && class == C_IN && !priming)
		needs_prime_cache = 1;
	if (np == NULL) {
		/* find the root */
		for (np = htp->h_tab[0]; np != NULL; np = np->n_next)
			if (NAME(*np)[0] == '\0')
				break;
	}
	while (np != NULL) {
		dprintf(5, (ddt, "findns: np %#lx '%s'\n",
			    (u_long)np, NAME(*np)));
		/* Look first for SOA records. */
#ifdef ADDAUTH
		if (!flag)
#endif
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			if (dp->d_zone != 0 &&
#ifdef PURGE_ZONE
				((zones[dp->d_zone].z_type == Z_PRIMARY) ||
				(zones[dp->d_zone].z_type == Z_SECONDARY)) &&
#endif
				match(dp, class, T_SOA)) {
				dprintf(3, (ddt, "findns: SOA found\n"));
				if (zones[dp->d_zone].z_flags & Z_AUTH) {
					*npp = np;
					nsp[0] = dp;
					nsp[1] = NULL;
					dp->d_rcnt++;
					return (NXDOMAIN);
				} else {
					/* XXX:	zone isn't loaded but we're
					 *	primary or secondary for it.
					 *	should we fwd this?
					 */
					return (SERVFAIL);
				}
			}
		}

		/* If no SOA records, look for NS records. */
		nspp = &nsp[0];
		*nspp = NULL;
		delete_stale(np);
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			if (!match(dp, class, T_NS))
				continue;
#ifdef NCACHE
			if (dp->d_rcode)
				continue;
#endif
			/*
			 * Don't use records that may become invalid to
			 * reference later when we do the rtt computation.
			 * Never delete our safety-belt information!
			 *
			 * XXX:	this is horribly bogus.
			 */
			if ((dp->d_zone == 0) &&
			    (dp->d_ttl < tt.tv_sec) &&
			    !(dp->d_flags & DB_F_HINT)) {
				dprintf(1, (ddt, "findns: stale entry '%s'\n",
					    NAME(*np)));
				/*
				 * We may have already added NS databufs
				 * and are going to throw them away. Fix
				 * fix reference counts. We don't need
				 * free() them here as we just got them
				 * from the cache.
				 */
				while (nspp > &nsp[0]) 
					(*--nspp)->d_rcnt--;
				nsp[0] = NULL;
				goto try_parent;
			}
			if (nspp < &nsp[NSMAX-1]) {
				*nspp++ = dp;
				dp->d_rcnt++;
			}
		}

		*countp = nspp - nsp;
		if (*countp > 0) {
			dprintf(3, (ddt, "findns: %d NS's added for '%s'\n",
				    *countp, NAME(*np)));
			*nspp = NULL;
			*npp = np;
			return (OK);	/* Success, got some NS's */
		}
 try_parent:
		np = np_parent(np);
	}
	if (htp == hashtab) {
		htp = fcachetab;
		goto try_again;
	}
	dprintf(1, (ddt, "findns: No root nameservers for class %s?\n",
		    p_class(class)));
	if ((unsigned)class < MAXCLASS && norootlogged[class] == 0) {
		norootlogged[class] = 1;
		syslog(LOG_INFO, "No root nameservers for class %s\n",
		       p_class(class));
	}
	return (SERVFAIL);
}


/*
 * Extract RR's from the given node that match class and type.
 * Return number of bytes added to response.
 * If no matching data is found, then 0 is returned.
 */
int
finddata(np, class, type, hp, dnamep, lenp, countp)
	struct namebuf *np;
	int class, type;
	register HEADER *hp;
	char **dnamep;
	int *lenp, *countp;
{
	register struct databuf *dp;
	register char *cp;
	int buflen, n, count = 0;

	delete_stale(np);

#ifdef ROUND_ROBIN
	if (type != T_ANY && type != T_PTR) {
		/* cycle order of RRs, for a load balancing effect... */

		register struct databuf **dpp;
 
		for (dpp = &np->n_data; dp = *dpp; dpp = &dp->d_next) {
			if (dp->d_next && wanted(dp, class, type)) {
				register struct databuf *lp;

				*dpp = lp = dp->d_next;
				dp->d_next = NULL;

				for (dpp = &lp->d_next;
				     *dpp;
				     dpp = &lp->d_next)
					lp = *dpp;
				*dpp = dp;
				break;
			}
		}
	}
#endif /*ROUND_ROBIN*/

	buflen = *lenp;
#ifdef DEBUG
	if (buflen > PACKETSZ)
		dprintf(1, (ddt, "finddata(): buflen=%d\n", buflen));
#endif
	cp = ((char *)hp) + *countp;
	for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
		if (!wanted(dp, class, type)) {
#ifndef NCACHE /*if no negative caching then cname => nothing else*/
			if (type == T_CNAME && class == dp->d_class) {
				/* any data means no CNAME exists */
				*countp = 0;
				return 0;
			}
#endif /*NCACHE*/
			continue;
		}
		if (dp->d_cred == DB_C_ADDITIONAL) {
#ifdef NOADDITIONAL
			continue;
#else
			/* we want to expire additional data very
			 * quickly.  current strategy is to cut 5%
			 * off each time it is accessed.  this makes
			 * stale(dp) true earlier when this datum is
			 * used often.
			 */
			dp->d_ttl = tt.tv_sec
					+
				0.95 * (int) (dp->d_ttl - tt.tv_sec);
#endif
		}
#ifdef NCACHE
		/* -ve $ing stuff, anant@isi.edu
		 * if we have a -ve $ed record, change the rcode on the
		 * header to reflect that
		 */
		if (dp->d_rcode == NOERROR_NODATA) {
			if (count != 0) {
				/*
				 * This should not happen, yet it does...
				 */
				syslog(LOG_INFO,
				   "NODATA & data for \"%s\" type %d class %d",
				       *dnamep, type, class);
				continue;
			}
			if (type == T_ANY)
				continue;
			hp->rcode = NOERROR_NODATA;
			if (dp->d_size == 0) { /* !RETURNSOA */
				*countp = 0;
				return 1; /* XXX - we have to report success */
			}
		}
		if (dp->d_rcode == NXDOMAIN) {
			if (count != 0) {
				/*
				 * This should not happen, yet it might...
				 */
				syslog(LOG_INFO,
				  "NXDOMAIN & data for \"%s\" type %d class %d",
				       *dnamep, type, class);
				continue;
			}
			hp->rcode = NXDOMAIN;
			if (dp->d_size == 0) { /* !RETURNSOA */
				*countp = 0;
				return 1; /* XXX - we have to report success */
			}
		}
#endif /*NCACHE*/

		/* Don't put anything but key or sig RR's in response to
			     requests for key or sig */
		if (((type == T_SIG) || (type == T_KEY)) &&
		    (!((dp->d_type == T_SIG) || (dp->d_type == T_KEY))) )
			continue;

		if ((n = make_rr(*dnamep, dp, (u_char *)cp, buflen, 1)) < 0) {
			hp->tc = 1;
			*countp = count;
			return (*lenp - buflen);
		}

		cp += n;
		buflen -= n;
		count++;
#ifdef notdef
		/* this isn't right for glue records, aa is set in ns_req */
		if (dp->d_zone &&
		    (zones[dp->d_zone].z_flags & Z_AUTH) &&
		    class != C_ANY)
			hp->aa = 1;			/* XXX */
#endif
		if (dp->d_type == T_CNAME) {
			/* don't alias if querying for key, sig, nxt, or any */
			if ((type != T_KEY) && 
			    (type != T_SIG) &&
			    (type != T_NXT) &&
			    (type != T_ANY)) {	/* or T_NS? */
				*dnamep = (caddr_t) dp->d_data;
				if (dp->d_zone != DB_Z_CACHE &&
				    (zones[dp->d_zone].z_flags & Z_AUTH) &&
				    class != C_ANY)		/* XXX */
					hp->aa = 1;		/* XXX */
			}
			break;
		}
	}
	/*
	 * Cache invalidate the other RR's of same type
	 * if some have timed out
	 */
	dprintf(3, (ddt, "finddata: added %d class %d type %d RRs\n",
		    count, class, type));
	*countp = count;
	return (*lenp - buflen);
}

/*
 * Do we want this data record based on the class and type?
 * (We always return found unexpired SIG RR's that cover the wanted rrtype.)
 */
int
wanted(dp, class, type)
	struct databuf *dp;
	int class, type;
{
	u_char *cp;
	u_int16_t coveredType;
	time_t expiration;

	dprintf(3, (ddt, "wanted(%#lx, %d, %d) [%s %s]\n",
		    (u_long)dp, class, type,
		    p_class(dp->d_class), p_type(dp->d_type)));

	if (dp->d_class != class && class != C_ANY)
		return (0);
	/* Must check SIG for expiration below, other matches return OK here. */
	if (type == dp->d_type && (type != T_SIG))
		return (1);
#ifdef NCACHE
	/*-ve $ing stuff, for a T_ANY query, we do not want to return
	 * -ve $ed RRs.
	 */
	if (type == T_ANY && dp->d_rcode == NOERROR_NODATA)
		return (0);
#endif

	/* First, look at the type of RR.  */
	switch (dp->d_type) {

		/* Cases to deal with:
			T_ANY search, return all unexpired SIGs.
			T_SIG search, return all unexpired SIGs.
			T_<foo> search, return all unexp SIG <FOO>s.
		 */
	case T_SIG:
		cp = dp->d_data;
		GETSHORT(coveredType,cp);
		cp += INT16SZ + INT32SZ; /* skip alg, labels, & orig TTL */
		GETLONG(expiration,cp);

		if (type == T_ANY || type == T_SIG || type == coveredType) {
			if (expiration > time(0))
				return (1);	/* Unexpired matching SIG */
		}
		return (0);		/* We don't return this SIG. */

	case T_ANY:
		return (1);
	case T_CNAME:
#ifdef NCACHE
		if (dp->d_rcode != NOERROR_NODATA)
#endif
			return (1);
#ifdef NCACHE
		else
			break;
#endif
	}
	/* OK, now look at the type of query.  */
	switch (type) {
	case T_ANY:
		return (1);

	case T_MAILB:
		switch (dp->d_type) {
		case T_MR:
		case T_MB:
		case T_MG:
		case T_MINFO:
			return (1);
		}
		break;

	case T_AXFR:
		/* T_AXFR needs an authoritative SOA */
		if (dp->d_type == T_SOA && dp->d_zone != 0
		    && (zones[dp->d_zone].z_flags & Z_AUTH))
			return (1);
		break;
	}
	return (0);
}

/*
 *  Add RR entries from dpp array to a query/response.
 *  Return the number of bytes added or negative the amount
 *  added if truncation occured.  Typically you are
 *  adding NS records to a response.
 */
int
add_data(np, dpp, cp, buflen, countp)
	struct namebuf *np;
	struct databuf **dpp;
	register u_char *cp;
	int buflen, *countp;
{
	register struct databuf *dp;
	char dname[MAXDNAME];
	register int n, bytes;

	bytes = *countp = 0;
	getname(np, dname, sizeof(dname));
	for (dp = *dpp++; dp != NULL; dp = *dpp++) {
		if (stale(dp))
			continue;	/* ignore old cache entry */
#ifdef NCACHE
		if (dp->d_rcode)
			continue;
#endif
		if ((n = make_rr(dname, dp, cp, buflen, 1)) < 0)
			return (-bytes);	/* Truncation */
		cp += n;
		buflen -= n;
		bytes += n;
		(*countp)++;
	}
	return (bytes);
}

static void
rrsetadd(flushset, name, dp)
	struct flush_set *flushset;
	char *name;
	struct databuf *dp;
{
	struct flush_set *fs = flushset;
	struct db_list *dbl;

	while (fs->fs_name && (
		strcasecmp(fs->fs_name,name) ||
		(fs->fs_class != dp->d_class) ||
		(fs->fs_type != dp->d_type) ||
		(fs->fs_cred != dp->d_cred))) {
		fs++;
	}
	if (!fs->fs_name) {
		fs->fs_name = strdup(name);
		if (!fs->fs_name)
			panic(-1, "rrsetadd: out of memory");
		fs->fs_class = dp->d_class;
		fs->fs_type = dp->d_type;
		fs->fs_cred = dp->d_cred;
		fs->fs_list = NULL;
		fs->fs_last = NULL;
	}
	dbl = (struct db_list *)malloc(sizeof(struct db_list));
	if (!dbl)
		panic(-1, "rrsetadd: out of memory");
	dbl->db_next = NULL;
	dbl->db_dp = dp;
	if (fs->fs_last == NULL)
		fs->fs_list = dbl;
	else
		fs->fs_last->db_next = dbl;
	fs->fs_last = dbl;
}

static int
ttlcheck(name,dbl,update)
	char *name;
	struct db_list *dbl;
	int update;
{
	int type = dbl->db_dp->d_type;
	int class = dbl->db_dp->d_class;
	struct hashbuf *htp = hashtab;
	const char *fname;
	register struct namebuf *np;
	struct db_list *dbp = dbl;
	struct databuf *dp;
	u_int32_t ttl;
	int first;


	np = nlookup(name, &htp, &fname, 0);
	if (np == NULL || fname != name || ns_wildcard(NAME(*np))) {
		return(1);
	}

	/* check that all the ttl's we have are the same, if not return 1 */
	first = 1;
	for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
		if (!match(dp, class, type))
			continue;
		if (first) {
			/* we can't update zone data so return early */
			if (dp->d_zone != 0)
				return(0);
			ttl = dp->d_ttl;
			first = 0;
		} else if (ttl != dp->d_ttl) {
			return(1);
		}
	}

	/* there are no records of this type in the cache */
	if (first)
		return(1);

	/*
	 * the ttls of all records we have in the cache are the same
	 * if the ttls differ in the new set we don't want it.
	 */

	/* check that all the ttl's we have are the same, if not return 0 */
	first = 1;
	while (dbp) {
		if (first) {
			ttl = dbp->db_dp->d_ttl;
			first = 0;
		} else if (ttl != dbp->db_dp->d_ttl) {
			return(0);
		}
		dbp = dbp->db_next;
	}

	/* update ttl if required */
	if (update) {
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			if (!match(dp, class, type))
				continue;
			if (dp->d_ttl > ttl)
				break;
			dp->d_ttl = ttl;
			fixttl(dp);
		}
	}

	return(1);
}

static int
rrsetcmp(name, dbl)
	char *name;
	struct db_list *dbl;
{
	int type = dbl->db_dp->d_type;
	int class = dbl->db_dp->d_class;
	struct hashbuf *htp = hashtab;
	const char *fname;
	register struct namebuf *np;
	struct db_list *dbp = dbl;
	struct databuf *dp;
	int exists = 0;


	np = nlookup(name, &htp, &fname, 0);
	if (np == NULL || fname != name || ns_wildcard(NAME(*np))) {
		dprintf(1, (ddt, "rrsetcmp: name not in database\n"));
		return(-1);
	}

	/* check that all entries in dbl are in the cache */
	while (dbp) {
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			if (match(dp, class, type))
				exists++;
			if (!db_cmp(dp, dbp->db_dp) 
#ifdef NOADDITIONAL
			&& ((dp->d_cred == dbp->db_dp->d_cred) ||
				 (dp->d_cred != DB_C_ADDITIONAL))
#endif
				 )
				break;
		}
		if (!dp) {
			dprintf(1, (ddt, "rrsetcmp: %srecord%s in database\n",
				exists ? "" : "no ", exists ? " not" : "s"));
			return(exists? 1 : -1);
		}
		dbp = dbp->db_next;
	}

	/* Check that all cache entries are in the list. */
	for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
		if (!match(dp, class, type))
			continue;
#ifdef NCACHE
		if (dp->d_rcode)
			return(1);
#endif
		dbp = dbl;
		while (dbp) {
			if (!db_cmp(dp, dbp->db_dp))
				break;
			dbp = dbp->db_next;
		}
		if (!dbp) {
			dprintf(1, (ddt, "rrsetcmp: record not in rrset\n"));
			return(1);
		}
	}
	dprintf(1, (ddt, "rrsetcmp: rrsets matched\n"));
	return(0);
}

static void
rrsetupdate(flushset, flags)
	struct flush_set * flushset;
	int flags;
{
	struct flush_set *fs = flushset;
	struct db_list *dbp, *odbp;
	int n;

	while (fs->fs_name) {
		dprintf(1,(ddt, "rrsetupdate: %s\n",
			    fs->fs_name[0] ? fs->fs_name : "."));
		if ((n = rrsetcmp(fs->fs_name, fs->fs_list)) &&
		    ttlcheck(fs->fs_name, fs->fs_list, 0)) {
			if (n > 0)
				flushrrset(fs);

			dbp = fs->fs_list;
			while (dbp) {
				n = db_update(fs->fs_name, dbp->db_dp,
						dbp->db_dp, flags, hashtab);
				dprintf(1,(ddt, "rrsetupdate: %s %d\n",
					fs->fs_name[0] ? fs->fs_name : ".", n));
				if (n != OK)
					db_free(dbp->db_dp);
				odbp = dbp;
				dbp = dbp->db_next;
				free((char *)odbp);
			}    
		} else {
			if (n == 0)
				(void)ttlcheck(fs->fs_name, fs->fs_list, 1);
			dbp = fs->fs_list;
			while (dbp) {
				db_free(dbp->db_dp);
				odbp = dbp;
				dbp = dbp->db_next;
				free((char *)odbp);
			}
		}
		fs->fs_list = NULL;
		fs++;
	}
}

static void
flushrrset(fs)
	struct flush_set * fs;
{
	struct databuf *dp;
	int n;

	dprintf(1, (ddt, "flushrrset(%s, %s, %s, %d)\n",
		fs->fs_name[0]?fs->fs_name:".", p_type(fs->fs_type),
		p_class(fs->fs_class), fs->fs_cred));
	dp = savedata(fs->fs_class, fs->fs_type, 0, NULL, 0);
	dp->d_zone = 0;
	dp->d_cred = fs->fs_cred;
	dp->d_clev = 0;	
	do {
		n = db_update(fs->fs_name, dp, NULL, DB_DELETE, hashtab);
		dprintf(1, (ddt, "flushrrset: %d\n", n));
	} while (n == OK);
	db_free(dp);
}

static void
free_flushset(flushset)
	struct flush_set *flushset;
{
	struct flush_set *fs;

	for (fs = flushset; fs->fs_name != NULL; fs++)
		free(fs->fs_name);
	free((char *)flushset);
}

/*
 *  This is best thought of as a "cache invalidate" function.
 *  It is called whenever a piece of data is determined to have
 *  become invalid either through a timeout or a validation
 *  failure.  It is better to have no information, than to
 *  have partial information you pass off as complete.
 */
void
delete_all(np, class, type)
	register struct namebuf *np;
	int class, type;
{
	register struct databuf *dp, *pdp;

	dprintf(3, (ddt, "delete_all(%#lx:\"%s\" %s %s)\n",
		    (u_long)np, NAME(*np), p_class(class), p_type(type)));
	pdp = NULL;
	dp = np->n_data;
	while (dp != NULL) {
		if ((dp->d_zone == 0) && !(dp->d_flags & DB_F_HINT)
		    && match(dp, class, type)) {
			dp = rm_datum(dp, np, pdp);
			continue;
		}
		pdp = dp;
		dp = dp->d_next;
	}
}

/* delete_stale(np)
 *	for all RRs associated with this name, check for staleness (& delete)
 * arguments:
 *	np = pointer to namebuf to be cleaned.
 * returns:
 *	void.
 * side effects:
 *	delete_all() can be called, freeing memory and relinking chains.
 */
void 
delete_stale(np)
	struct namebuf *np;
{
	struct databuf *dp;
 again:  
        for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
                if ((dp->d_zone == 0) && stale(dp)) {
                        delete_all(np, dp->d_class, dp->d_type);
                        goto again;
		}
	}
}


static void
add_related_additional(name)
	char *name;
{
	int i;

	if (num_related >= MAX_RELATED - 1)
		return;
	for (i = 0; i < num_related; i++)
		if (strcasecmp(name, related[i]) == 0) {
			free(name);
			return;
		}
	related[num_related++] = name;
}

static void
free_related_additional() {
	int i;

	for (i = 0; i < num_related; i++)
		free(related[i]);
	num_related = 0;
}

static int
related_additional(name)
	char *name;
{
	int i;

	for (i = 0; i < num_related; i++)
		if (strcasecmp(name, related[i]) == 0)
			return (1);
	return (0);
}

static void
maybe_free(tname)
	char **tname;
{
	if (tname == NULL || *tname == NULL)
		return;
	free(*tname);
	*tname = NULL;
}
