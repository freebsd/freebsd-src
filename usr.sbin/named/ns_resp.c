#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)ns_resp.c	4.65 (Berkeley) 3/3/91";
static char rcsid[] = "$Id: ns_resp.c,v 8.18 1995/12/29 21:08:13 vixie Exp $";
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
 * --Copyright--
 */

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

static void		check_root __P((void)),
			check_ns __P((void));

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
			editFailed[] =		"edit of response failed";

static char *
learntFrom(qp, server)
	struct qinfo *qp;
	struct sockaddr_in *server;
{
	static char *buf = NULL;
	char *a, *ns, *na;
	struct databuf *db;
	char nsbuf[20];
	char abuf[20];
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
	register u_char *tempcp;
#ifdef VALIDATE
	struct sockaddr_in *server = &from_addr;
	struct { char *name; int type, class; u_int cred; } defer_rm[99];
	int defer_rm_count;
#endif
	struct sockaddr_in *nsa;
	struct databuf *nsp[NSMAX];
	int i, c, n, qdcount, ancount, aucount, nscount, arcount;
	int qtype, qclass, dbflags;
	int restart;	/* flag for processing cname response */
	int validanswer;
	int cname;
	int count, founddata, foundname;
	int buflen;
	int newmsglen;
	char name[MAXDNAME], qname[MAXDNAME];
	char *dname;
	const char *fname;
	const char *formerrmsg = "brain damage";
	u_char newmsg[PACKETSZ];
	u_char **dpp, *tp;
	time_t rtrip;
	struct hashbuf *htp;
	struct namebuf *np;
	struct netinfo *lp;
	struct fwdinfo *fwd;

	nameserIncr(from_addr.sin_addr, nssRcvdR);
#ifdef  DATUMREFCNT
	nsp[0] = NULL;
#endif
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
		GETSHORT(qtype, cp);
		GETSHORT(qclass, cp);
		if (cp > eom) {
			formerrmsg = outofDataQuery;
			goto formerr;
		}
		if (qp->q_msg && qp->q_msglen &&
		    !res_nameinquery(qname, qtype, qclass,
				     qp->q_msg, qp->q_msg + qp->q_msglen)) {
			char msgbuf[MAXDNAME*2];

			sprintf(msgbuf,
				"query section mismatch (%s %s %s)",
				qname, p_class(qclass), p_type(qtype));
			formerrmsg = msgbuf;
			goto formerr;
		}
	} else {
		/* Pedantic. */
		qname[0] = '\0';
		qtype = 0;
		qclass = 0;
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

#ifdef ALLOW_UPDATES
	if ( (hp->rcode == NOERROR) &&
	     (hp->opcode == UPDATEA || hp->opcode == UPDATED ||
	      hp->opcode == UPDATEDA || hp->opcode == UPDATEM ||
	      hp->opcode == UPDATEMA) ) {
		/*
		 * Update the secondary's copy, now that the primary
		 * successfully completed the update.  Zone doesn't matter
		 * for dyn. update -- doupdate calls findzone to find it
		 */
		/* XXX - DB_C_AUTH may be wrong */
		(void) doupdate(qp->q_msg, qp->q_msglen, qp->q_msg + HFIXEDSZ,
				0, (struct databuf *)0, 0, DB_C_AUTH);
		dprintf(3, (ddt, "resp: leaving, UPDATE*\n"));
		/* return code filled in by doupdate */
		goto return_msg;
	}
#endif /* ALLOW_UPDATES */

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
		if (ns && (ns->d_type==T_A) && (ns->d_class==qs->ns->d_class)){
			if (ns->d_nstime == 0)
				ns->d_nstime = (u_int32_t)rtrip;
			else
				ns->d_nstime = (u_int32_t)
						(ns->d_nstime * ALPHA 
						 +
						 (1-ALPHA) * (u_int32_t)rtrip);
			/* prevent floating point overflow,
			 * limit to 1000 sec
			 */
			if (ns->d_nstime > 1000000)
				ns->d_nstime = 1000000;
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
			ns2 = qs->nsdata;
			if ((!ns2) || (ns2 == ns))
				continue;
			if (ns2->d_type != T_A ||
			    ns2->d_class != qs->ns->d_class)	/* XXX */
				continue;
			if (qs->stime.tv_sec) {
			    if (ns2->d_nstime == 0)
				ns2->d_nstime = (u_int32_t)(rtrip * BETA);
			    else
				ns2->d_nstime = (u_int32_t)(
				    ns2->d_nstime * BETA + (1-ALPHA) * rtrip
				);
			    if (ns2->d_nstime > 1000000)
				ns2->d_nstime = 1000000;
			} else
			    ns2->d_nstime = (u_int32_t)(ns2->d_nstime * GAMMA);
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
	 *  Non-authoritative, no answer, no error
	 */
	if (qdcount == 1 && hp->rcode == NOERROR && !hp->aa && ancount == 0
	    && aucount > 0
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
		GETSHORT(type, tp);
		if (tp >= eom) {
			formerrmsg = outofDataAuth;
			goto formerr;
		}
		GETSHORT(class, tp);
		if (tp >= eom) {
			formerrmsg = outofDataAuth;
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

			n = dn_expand(msg, eom, tp, name, sizeof name);
			if (n < 0) {
				formerrmsg = expandFailedAnswer;
				goto formerr;
			}
			tp += n;  		/* name */
			GETSHORT(type, tp);	/* type */
			GETSHORT(class, tp);	/* class */
			tp += INT32SZ;		/* ttl */
			GETSHORT(dlen, tp); 	/* dlen */
			if (tp >= eom) {
				formerrmsg = outofDataAnswer;
				goto formerr;
			}
			if (strcasecmp(qname, name) ||
			    qtype != type ||
			    qclass != class) {
				char msgbuf[MAXDNAME*2];

				sprintf(msgbuf,
					"qserial answer mismatch (%s %s %s)",
					name, p_class(class), p_type(type));
				formerrmsg = msgbuf;
				goto formerr;
			}
			if ((u_int)dlen < (5 * INT32SZ)) {
				formerrmsg = dlenUnderrunAnswer;
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
			GETLONG(serial, tp);

			qserial_answer(qp, serial);
		}
		qremove(qp);
		return;
	}

	/*
	 * Add the info received in the response to the data base.
	 */
	c = ancount + aucount + arcount;

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
	}

	tp = cp;

	restart = 0;
	validanswer = 0;
	nscount = 0;
	cname = 0;
#ifdef VALIDATE
	defer_rm_count = 0;
#endif

	for (i = 0; i < count; i++) {
		struct databuf *ns3 = NULL;
		u_char cred;
		int VCode;
		u_int16_t type, class;

		if (cp >= eom) {
			formerrmsg = outofDataFinal;
			goto formerr;
		}

		/* Get the DNAME. */
		tempcp = cp;
		n = dn_expand(msg, eom, tempcp, name, sizeof name);
		if (n <= 0) {
			formerrmsg = outofDataFinal;
			goto formerr;
		}
		tempcp += n;
		GETSHORT(type, tempcp);
		GETSHORT(class, tempcp);

		/*
		 * See if there are any NS RRs in the authority section
		 * for the negative caching logic below.  We'll count
		 * these before validation.
		 */
		if (type == T_NS && i >= ancount && i < ancount + aucount)
			nscount++;

		/* Decide what credibility this ought to have in the cache. */
		if (i < ancount)
			cred = (hp->aa && !strcasecmp(name, qname))
				? DB_C_AUTH
				: DB_C_ANSWER;
		else
			cred = (qp->q_flags & Q_PRIMING)
				? DB_C_ANSWER
				: DB_C_ADDITIONAL;
#ifdef VALIDATE
		if ((n = dovalidate(msg, msglen, cp, 0,
				    dbflags, qp->q_domain, server,
				    &VCode)) < 0) {
			formerrmsg = outofDataFinal;
			goto formerr;
		}
		if (VCode == INVALID && !(qp->q_flags & Q_SYSTEM)) {
			/*
			 * If anything in the answer section fails
			 * validation this means that it definitely did
			 * not reside below the domain owning the NS RRs
			 * that we sent the query to.  This means either
			 * that it was the target of a CNAME early in the
			 * response, in which case we will treat this the
			 * same as if the answer was incomplete and restart
			 * the query on the CNAME target, or that someone
			 * was trying to spoof us.
			 */
			if (i < ancount)
				restart = 1;
			/*
			 * Restart or no, if we're here it means we are not
			 * going to cache this RR.  That being the case, we
			 * must burn down whatever partial RRset we've got
			 * in the cache now, lest we inadvertently answer
			 * with a truncated RRset in some future section.
			 */
			for (c = 0; c < defer_rm_count; c++)
				if (!strcasecmp(defer_rm[c].name, name) &&
				    defer_rm[c].class == class &&
				    defer_rm[c].type == type)
					break;
			if (c < defer_rm_count) {
				if (defer_rm[c].cred < cred)
					defer_rm[c].cred = cred;
			} else {
				if (defer_rm_count+1 >=
				    (sizeof defer_rm / sizeof defer_rm[0])) {
					formerrmsg = "too many RRs in ns_resp";
					goto formerr;
				}
				defer_rm[defer_rm_count].name = savestr(name);
				defer_rm[defer_rm_count].type = type;
				defer_rm[defer_rm_count].class = class;
				defer_rm[defer_rm_count].cred = cred;
				defer_rm_count++;
			}
		} else {
#endif
		if (i < ancount) {
			/*
			 * If there are any non-CNAME RRs (or
			 * CNAME RRs if they are an acceptable)
			 * then the query is complete unless an
			 * intermediate CNAME didn't pass validation,
			 * but that's OK.
			 */
			if (type != T_CNAME || qtype == T_CNAME ||
			    qtype == T_ANY)
				validanswer = 1;
			else
				cname = 1;
		}
		n = doupdate(msg, msglen, cp, 0, &ns3, dbflags, cred);
#ifdef VALIDATE
		}
#endif
		if (n < 0) {
			dprintf(1, (ddt, "resp: leaving, doupdate failed\n"));
			formerrmsg = outofDataFinal;
			goto formerr;
		}
		cp += n;
	}
#ifdef VALIDATE
	if (defer_rm_count > 0) {
		for (i = 0; i < defer_rm_count; i++) {
			register struct databuf *db = NULL;

			fname = "";
			htp = hashtab;		/* lookup relative to root */
			np = nlookup(defer_rm[i].name, &htp, &fname, 0);
			if (np && fname == defer_rm[i].name &&
			    defer_rm[i].class != C_ANY &&
			    defer_rm[i].type != T_ANY) {
				/*
				 * If doupdate() wouldn't have cached this
				 * RR anyway, there's no need to delete it.
				 */
				for (db = np->n_data;
				     db != NULL;
				     db = db->d_next) {
					if (!db->d_zone &&
					    match(db, defer_rm[i].class,
						  defer_rm[i].type) &&
					    db->d_cred >= defer_rm[i].cred) {
						break;
					}
				}
				if (db == NULL)
					delete_all(np, defer_rm[i].class,
						   defer_rm[i].type);
				/* XXX: should delete name node if empty? */
			}
			syslog(LOG_DEBUG, "defer_rm [%s %s %s] (np%#x, db%#x)",
			       defer_rm[i].name,
			       p_class(defer_rm[i].class),
			       p_type(defer_rm[i].type),
			       np, db);
			free(defer_rm[i].name);
		}
	}
#endif

	if (cp > eom) {
		formerrmsg = outofDataAFinal;
		goto formerr;
	}

	if ((qp->q_flags & Q_SYSTEM) && ancount) {
		if (qp->q_flags & Q_PRIMING)
			check_root();
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

	if (ancount && !validanswer)
		/*
		 * Everything passed validation but we didn't get the
		 * final answer.  The response must have contained
		 * a dangling CNAME.  Force a restart of the query.
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
		if (qp->q_cmsglen) {
			/* XXX - what about additional CNAMEs in the chain? */
			msg = qp->q_cmsg;
			msglen = qp->q_cmsglen;
			hp = (HEADER *)msg;
		}
#ifdef NCACHE
		/* answer was NO */
		if (hp->aa &&
		    ((hp->rcode == NXDOMAIN) || (hp->rcode == NOERROR))) {
			cache_n_resp(msg, msglen);
		}
#endif /*NCACHE*/
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
		newmsglen = qp->q_cmsglen;
		bcopy(qp->q_cmsg, newmsg, newmsglen);
	} else {
		newmsglen = msglen;
		bcopy(msg, newmsg, newmsglen);
	}
	hp = (HEADER *) newmsg;
	hp->ancount = htons(0);
	hp->nscount = htons(0);
	hp->arcount = htons(0);
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
#ifdef	DATUMREFCNT
	free_nsp(nsp);
#endif
	switch (findns(&np, qclass, nsp, &count, 0)) {
	case NXDOMAIN:		/* shouldn't happen */
		dprintf(3, (ddt, "req: leaving (%s, rcode %d)\n",
			    dname, hp->rcode));
		if (!foundname)
			hp->rcode = NXDOMAIN;
		if (qclass != C_ANY) {
			hp->aa = 1;
			/* XXX:	should return SOA if founddata == 0,
			 *	but old named's are confused by an SOA
			 *	in the auth. section if there's no error.
			 */
			if (foundname == 0 && np) {
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
#ifdef	DATUMREFCNT
	/* XXX - this code should be shared with qfree()'s similar logic. */
	for (i = 0; (u_int)i < qp->q_naddr; i++) {
		static const char freed[] = "freed", busy[] = "busy";
		const char *result;

		if (qp->q_addr[i].ns != NULL) {
			if ((--(qp->q_addr[i].ns->d_rcnt)))
				result = busy;
			else
				result = freed;
			dprintf(1, (ddt, "ns_resp: ns %s rcnt %d (%s)\n",
				    qp->q_addr[i].ns->d_data,
				    qp->q_addr[i].ns->d_rcnt,
				    result));
			if (result == freed)
				free((char*)qp->q_addr[i].ns);
		}
		if (qp->q_addr[i].nsdata != NULL) {
			if ((--(qp->q_addr[i].nsdata->d_rcnt)))
				result = busy;
			else
				result = freed;
			dprintf(1, (ddt,
				    "ns_resp: nsdata %08.8X rcnt %d (%s)\n",
				    *(int32_t *)(qp->q_addr[i].nsdata->d_data),
				    qp->q_addr[i].nsdata->d_rcnt,
				    result));
			if (result == freed)
				free((char*)qp->q_addr[i].nsdata);
		}
	}
#endif
	qp->q_naddr = 0;
	qp->q_curaddr = 0;
	qp->q_fwd = fwdtab;
#if defined(LAME_DELEGATION) || defined(VALIDATE)
	getname(np, qp->q_domain, sizeof(qp->q_domain));
#endif /* LAME_DELEGATION */
	if ((n = nslookup(nsp, qp, dname, "ns_resp")) <= 0) {
		if (n < 0) {
			dprintf(3, (ddt, "resp: nslookup reports danger\n"));
		} else {
			dprintf(3, (ddt, "resp: no addrs found for NS's\n"));
		}
		if (cname)	/* a remote CNAME that does not have data */
			goto return_newmsg;
		goto servfail;
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
		if ((qp->q_msg = (u_char *)malloc(BUFSIZ)) == NULL) {
			syslog(LOG_NOTICE, "resp: malloc error\n");
			goto servfail;
		}
		n = res_mkquery(QUERY, dname, qclass, qtype,
				NULL, 0, NULL, qp->q_msg, BUFSIZ);
		if (n < 0) {
			syslog(LOG_INFO, "resp: res_mkquery(%s) failed",
			       dname);
			goto servfail;
		}
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
	nameserIncr(nsa->sin_addr, nssSentFwdR);
	nameserIncr(qp->q_from.sin_addr, nssRcvdFwdR);
	dprintf(3, (ddt, "resp: Query sent.\n"));
#ifdef	DATUMREFCNT
	free_nsp(nsp);
#endif
	return;

 formerr:
	if (!haveComplained((char*)from_addr.sin_addr.s_addr,
			    (char*)nhash(formerrmsg)))
		syslog(LOG_INFO, "Malformed response from %s (%s)\n",
		       sin_ntoa(&from_addr), formerrmsg);
	nameserIncr(from_addr.sin_addr, nssSentFErr);
#ifdef	DATUMREFCNT
	free_nsp(nsp);
#endif
	return;

 return_msg:
	nameserIncr(from_addr.sin_addr, nssRcvdFwdR);
	nameserIncr(qp->q_from.sin_addr, nssSentFwdR);
	/* The "standard" return code */
	hp->qr = 1;
	hp->id = qp->q_id;
	hp->rd = 1;
	hp->ra = (NoRecurse == 0);
	(void) send_msg(msg, msglen, qp);
	qremove(qp);
#ifdef	DATUMREFCNT
	free_nsp(nsp);
#endif
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
#ifdef	DATUMREFCNT
	free_nsp(nsp);
#endif
	return;

 servfail:
	nameserIncr(qp->q_from.sin_addr, nssSentFail);
	hp = (HEADER *)(cname ? qp->q_cmsg : qp->q_msg);
	hp->rcode = SERVFAIL;
	hp->qr = 1;
	hp->id = qp->q_id;
	hp->rd = 1;
	hp->ra = (NoRecurse == 0);
	(void) send_msg((u_char *)hp, (cname ? qp->q_cmsglen : qp->q_msglen),
			qp);
	qremove(qp);
#ifdef	DATUMREFCNT
	free_nsp(nsp);
#endif
	return;
}

/*
 * Decode the resource record 'rrp' and update the database.
 * If savens is non-nil, record pointer for forwarding queries a second time.
 */
int
doupdate(msg, msglen, rrp, zone, savens, flags, cred)
	u_char *msg, *rrp;
	struct databuf **savens;
	int msglen, zone, flags;
	u_int cred;
{
	register u_char *cp;
	register int n;
	int class, type, dlen, n1;
	u_int32_t ttl;
	struct databuf *dp;
	char dname[MAXDNAME];
	u_char *cp1;
	u_char data[BUFSIZ];
	register HEADER *hp = (HEADER *)msg;
#ifdef ALLOW_UPDATES
	int zonenum;
#endif

	dprintf(3, (ddt, "doupdate(zone %d, savens %#lx, flags %#lx)\n",
		    zone, (u_long)savens, (u_long)flags));

	cp = rrp;
	if ((n = dn_expand(msg, msg + msglen, cp, dname, sizeof dname)) < 0) {
		hp->rcode = FORMERR;
		return (-1);
	}
	cp += n;
	GETSHORT(type, cp);
	GETSHORT(class, cp);
	GETLONG(ttl, cp);
	GETSHORT(dlen, cp);
	dprintf(3, (ddt, "doupdate: dname %s type %d class %d ttl %d\n",
		    dname, type, class, ttl));
	/*
	 * Convert the resource record data into the internal
	 * database format.
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
	case T_LOC:
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
		n = dn_expand(msg, msg + msglen, cp,
			      (char *)data, sizeof data);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 = data;
		n = strlen((char *)data) + 1;
		break;

	case T_MINFO:
	case T_SOA:
	case T_RP:
		n = dn_expand(msg, msg + msglen, cp,
			      (char *)data, sizeof data);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 = data + (n = strlen((char *)data) + 1);
		n1 = sizeof(data) - n;
		if (type == T_SOA)
			n1 -= 5 * INT32SZ;
		n = dn_expand(msg, msg + msglen, cp, (char *)cp1, n1);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 += strlen((char *)cp1) + 1;
		if (type == T_SOA) {
			bcopy(cp, cp1, n = 5 * INT32SZ);
			cp += n;
			cp1 += n;
		}
		n = cp1 - data;
		cp1 = data;
		break;

	case T_MX:
	case T_AFSDB:
	case T_RT:
		/* grab preference */
		bcopy(cp, data, INT16SZ);
		cp1 = data + INT16SZ;
		cp += INT16SZ;

		/* get name */
		n = dn_expand(msg, msg + msglen, cp, (char *)cp1,
			      sizeof data - INT16SZ);
		if (n < 0) {
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

	case T_PX:
		/* grab preference */
		bcopy(cp, data, INT16SZ);
		cp1 = data + INT16SZ;
		cp += INT16SZ;

		/* get MAP822 name */
		n = dn_expand(msg, msg + msglen, cp, (char *)cp1,
				sizeof data - INT16SZ);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 += (n = strlen((char *)cp1) + 1);
		n1 = sizeof(data) - n;
		n = dn_expand(msg, msg + msglen, cp, (char *)cp1, n1);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 += strlen((char *)cp1) + 1;
		n = cp1 - data;
		cp1 = data;
		break;

	default:
		dprintf(3, (ddt, "unknown type %d\n", type));
		return ((cp - rrp) + dlen);
	}
	if (n > MAXDATA) {
		dprintf(1, (ddt,
			    "update type %d: %d bytes is too much data\n",
			    type, n));
		hp->rcode = FORMERR;
		return (-1);
	}

#ifdef ALLOW_UPDATES
	/*
	 * If this is a dynamic update request, process it specially; else,
	 * execute normal update code.
	 */
	switch(hp->opcode) {

	/* For UPDATEM and UPDATEMA, do UPDATED/UPDATEDA followed by UPDATEA */
	case UPDATEM:
	case UPDATEMA:

	/*
	 * The named code for UPDATED and UPDATEDA is the same except that for
	 * UPDATEDA we we ignore any data that was passed: we just delete all
	 * RRs whose name, type, and class matches
	 */
	case UPDATED:
	case UPDATEDA:
		if (type == T_SOA) {	/* Not allowed */
			dprintf(1, (ddt, "UDPATE: REFUSED - SOA delete\n"));
			hp->rcode = REFUSED;
			return (-1);
		}
		/*
		 * Don't check message length if doing UPDATEM/UPDATEMA,
		 * since the whole message wont have been demarshalled until
		 * we reach the code for UPDATEA
		 */
		if ( (hp->opcode == UPDATED) || (hp->opcode == UPDATEDA) ) {
			if (cp != (u_char *)(msg + msglen)) {
				dprintf(1, (ddt, 
					  "FORMERR UPDATE message length off\n"
					    ));
				hp->rcode = FORMERR;
				return (-1);
			}
		}
		if ((zonenum = findzone(dname, class)) == 0) { 
			hp->rcode = NXDOMAIN;
			return (-1);
		}
		if (zones[zonenum].z_flags & Z_DYNADDONLY) {
			hp->rcode = NXDOMAIN;
			return (-1);
		}
		if ( (hp->opcode == UPDATED) || (hp->opcode == UPDATEM) ) {
			/* Make a dp for use in db_update, as old dp */
			dp = savedata(class, type, 0, cp1, n);
			dp->d_zone = zonenum;
			dp->d_cred = cred;
			dp->d_clev = db_getclev(zones[zonenum].z_origin);
			n = db_update(dname, dp, NULL, DB_MEXIST | DB_DELETE,
				      hashtab);
			if (n != OK) {
				dprintf(1, (ddt,
					    "UPDATE: db_update failed\n"));
				free((char*) dp);
				hp->rcode = NOCHANGE;
				return (-1);
			}
		} else {	/* UPDATEDA or UPDATEMA */
			int DeletedOne = 0;
			/* Make a dp for use in db_update, as old dp */
			dp = savedata(class, type, 0, NULL, 0);
			dp->d_zone = zonenum;
			dp->d_cred = cred;
			dp->d_clev = db_getclev(zones[zonenum].z_origin);
			do {	/* Loop and delete all matching RR(s) */
				n = db_update(dname, dp, NULL, DB_DELETE,
					      hashtab);
				if (n != OK)
					break;
				DeletedOne++;
			} while (1);
			free((char*) dp);
			/* Ok for UPDATEMA not to have deleted any RRs */
			if (!DeletedOne && hp->opcode == UPDATEDA) {
				dprintf(1, (ddt,
					    "UPDATE: db_update failed\n"));
				hp->rcode = NOCHANGE;
				return (-1);
			}
		}
		if ( (hp->opcode == UPDATED) || (hp->opcode == UPDATEDA) )
			return (cp - rrp);;
		/*
		 * Else unmarshal the RR to be added and continue on to
		 * UPDATEA code for UPDATEM/UPDATEMA
		 */
		if ((n =
		   dn_expand(msg, msg+msglen, cp, dname, sizeof(dname))) < 0) {
			dprintf(1, (ddt,
				    "FORMERR UPDATE expand name failed\n"));
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		GETSHORT(type, cp);
		GETSHORT(class, cp);
		GETLONG(ttl, cp);
		GETSHORT(n, cp);
		cp1 = cp;
/**** XXX - need bounds checking here ****/
		cp += n;

	case UPDATEA:
		if (n > MAXDATA) {
			dprintf(1, (ddt, "UPDATE: too much data\n"));
			hp->rcode = NOCHANGE;
			return (-1);
		}
		if (cp != (u_char *)(msg + msglen)) {
			dprintf(1, (ddt,
				    "FORMERR UPDATE message length off\n"));
			hp->rcode = FORMERR;
			return (-1);
		}
		if ((zonenum = findzone(dname, class)) == 0) { 
			hp->rcode = NXDOMAIN;
			return (-1);
		}
		if (zones[zonenum].z_flags & Z_DYNADDONLY) {
			struct hashbuf *htp = hashtab;
			char *fname;
			if (nlookup(dname, &htp, &fname, 0) &&
			    !strcasecmp(dname, fname)) {
				dprintf(1, (ddt,
					    "refusing add of existing name\n"
					    ));
				hp->rcode = REFUSED;
				return (-1);
			}
		}
		dp = savedata(class, type, ttl, cp1, n);
		dp->d_zone = zonenum;
		dp->d_cred = cred;
		dp->d_clev = db_getclev(zones[zonenum].z_origin);
		if ((n = db_update(dname, NULL, dp, DB_NODATA,
				   hashtab)) != OK) {
			dprintf(1, (ddt, "UPDATE: db_update failed\n"));
			hp->rcode = NOCHANGE;
			free((char*) dp);
			return (-1);
		}
		else
			return (cp - rrp);
	}
#endif /* ALLOW_UPDATES */

	if (zone == 0)
		ttl += tt.tv_sec;
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
		    ((temp = strrchr((char *)data, '.')) != NULL) &&
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
				       data, sin_ntoa(&from_addr), qname);
			return (cp - rrp);
		}
#ifdef BOGUSNS
		if (bogusns) {
			if (!haveComplained((char*)from_addr.sin_addr.s_addr,
					    "bogus nonroot NS"))
				syslog(LOG_INFO,
			"bogus nonroot NS %s rcvd from %s on query for \"%s\"",
				       data, sin_ntoa(&from_addr), qname);
			return (cp - rrp);
		}
#endif
	}
#endif /*TRACEROOT || BOGUSNS*/

	dp = savedata(class, type, ttl, cp1, n);
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
		free((char *)dp);
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
		char tmp[MAXDNAME*2];

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
	struct namebuf *np;
	struct databuf *nsp[NSMAX];
	struct hashbuf *htp;
	struct sockaddr_in *nsa;
	const char *fname;
	int n, count;

#ifdef	DATUMREFCNT
	nsp[0] = NULL;
#endif
	dprintf(3, (ddt, "sysquery(%s, %d, %d, %#lx, %d)\n",
		    dname, class, type, (u_long)nss, nsc));
	qp = qnew();

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
			syslog(LOG_DEBUG, "sysquery: findns error (%d) on %s?",
			       n, dname);
 err2:
#ifdef	DATUMREFCNT
			free_nsp(nsp);
#endif
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
#if defined(LAME_DELEGATION) || defined(VALIDATE)
	getname(np, qp->q_domain, sizeof(qp->q_domain));
#endif /* LAME_DELEGATION */

	if ((qp->q_msg = (u_char *)malloc(BUFSIZ)) == NULL) {
		syslog(LOG_NOTICE, "sysquery: malloc failed");
		goto err2;
	}
	n = res_mkquery(opcode, dname, class,
			type, NULL, 0, NULL,
			qp->q_msg, BUFSIZ);
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
		count = nslookup(nsp, qp, dname, "sysquery");
		if (count <= 0) {
			if (count < 0)
				syslog(LOG_INFO,
				      "sysquery: nslookup reports danger (%s)",
				       dname);
			else
				/* "." domain gets LOG_WARNING here. */
				syslog(dname[0] ? LOG_INFO : LOG_WARNING,
				       "sysquery: no addrs found for NS (%s)",
				       dname);
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
		    qp->q_time));
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
#ifdef	DATUMREFCNT
	free_nsp(nsp);
#endif
	return (qp);
}

/*
 * Check the list of root servers after receiving a response
 * to a query for the root servers.
 */
static void
check_root()
{
	register struct databuf *dp, *pdp;
	register struct namebuf *np;
	int count = 0;

	priming = 0;
	for (np = hashtab->h_tab[0]; np != NULL; np = np->n_next)
		if (np->n_dname[0] == '\0')
			break;
	if (np == NULL) {
		syslog(LOG_NOTICE, "check_root: Can't find root!\n");
		return;
	}
	for (dp = np->n_data; dp != NULL; dp = dp->d_next)
		if (dp->d_type == T_NS)
			count++;
	dprintf(1, (ddt, "%d root servers\n", count));
	if (count < MINROOTS) {
		syslog(LOG_NOTICE,
		"check_root: %d root servers after query to root server < min",
		       count);
		return;
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
	check_ns();
}

/* 
 * Check the root to make sure that for each NS record we have a A RR
 */
static void
check_ns()
{
	register struct databuf *dp, *tdp;
	register struct namebuf *np, *tnp;
	struct hashbuf *htp;
	char *dname;
	int found_arr;
	const char *fname;
	time_t curtime;

	dprintf(2, (ddt, "check_ns()\n"));

	curtime = (u_int32_t) tt.tv_sec;
	for (np = hashtab->h_tab[0]; np != NULL; np = np->n_next) {
		if (np->n_dname[0] != 0)
			continue;
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			if (dp->d_type != T_NS)
				continue;

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
			for (tdp=tnp->n_data; tdp != NULL; tdp=tdp->d_next) {
				if (tdp->d_type != T_A ||
				    tdp->d_class != dp->d_class)
					continue;
				if ((tdp->d_zone == 0) &&
				    (tdp->d_ttl < curtime)) {
					dprintf(3, (ddt,
						"check_ns: stale entry '%s'\n",
						    tnp->n_dname));
					/* Cache invalidate the address RR's */
					delete_all(tnp, dp->d_class, T_A);
					found_arr = 0;
					break;
				}
				found_arr++;
			}
			if (!found_arr)
				sysquery(dname, dp->d_class, T_A, NULL,
					 0, QUERY);
	        }
	}
}

/* int findns(npp, class, nsp, countp, flag)
 *	Find NS' or an SOA
 * npp, class:
 *	dname whose most enclosing NS is wanted
 * nsp, countp:
 *	result array and count; array will also be NULL terminated
 * flag:
 *	boolean: we're being called from ADDAUTH, bypass authority checks
 * return value:
 *	NXDOMAIN: we are authoritative for this {dname,class}
 *	SERVFAIL: we are auth but zone isn't loaded; or, no root servers found
 *	OK: success (this is the only case where *countp and nsp[] are valid)
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
	
#ifdef DATUMREFCNT
	nsp[0] = NULL;
#endif

	if (priming && (np == NULL || np->n_dname[0] == '\0'))
		htp = fcachetab;
	else
		htp = hashtab;

 try_again:
	if (htp == fcachetab)
		needs_prime_cache = 1;
	while (np == NULL && htp != NULL) {
		dprintf(3, (ddt, "findns: using %s\n",
			    htp == hashtab ? "cache" : "hints"));
		for (np = htp->h_tab[0]; np != NULL; np = np->n_next)
			if (np->n_dname[0] == '\0')
				break;
		htp = (htp == hashtab ? fcachetab : NULL);	/* Fallback */
	}
	while (np != NULL) {
		dprintf(5, (ddt, "findns: np %#lx '%s'\n",
			    (u_long)np, np->n_dname));
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
#ifdef DATUMREFCNT
					nsp[1] = NULL;
					dp->d_rcnt++;
#endif
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
#ifdef DATUMREFCNT
			    (dp->d_ttl < tt.tv_sec) &&
#else
			    (dp->d_ttl < (tt.tv_sec+900)) &&
#endif
			    !(dp->d_flags & DB_F_HINT)) {
				dprintf(1, (ddt, "findns: stale entry '%s'\n",
					    np->n_dname));
				/* Cache invalidate the NS RR's. */
#ifndef DATUMREFCNT
				if (dp->d_ttl < tt.tv_sec)
#endif
					delete_all(np, class, T_NS);
				goto try_parent;
			}
			if (nspp < &nsp[NSMAX-1]) {
				*nspp++ = dp;
#ifdef DATUMREFCNT
				dp->d_rcnt++;
#endif
			}
		}

		*countp = nspp - nsp;
		if (*countp > 0) {
			dprintf(3, (ddt, "findns: %d NS's added for '%s'\n",
				    *countp, np->n_dname));
			*nspp = NULL;
			*npp = np;
			return (OK);	/* Success, got some NS's */
		}
try_parent:
		np = np->n_parent;
	}
	if (htp)
		goto try_again;
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
	int buflen, n, count = 0, foundstale = 0;

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
		if (stale(dp)) {
			/*
			 * Don't use stale data.
			 * Would like to call delete_all here
			 * and continue, but the data chain would get
			 * munged; can't restart, as make_rr has side
			 * effects (leaving pointers in dnptr).
			 * Just skip this entry for now
			 * and call delete_all at the end.
			 */
			dprintf(3, (ddt,
				    "finddata: stale entry '%s'\n",
				    np->n_dname));
			if (dp->d_zone == 0)
				foundstale++;
			continue;
		}
		if (dp->d_cred == DB_C_ADDITIONAL) {
			/* we want to expire additional data very
			 * quickly.  current strategy is to cut 5%
			 * off each time it is accessed.  this makes
			 * stale(dp) true faster when this datum is
			 * used often.
			 */
			dp->d_ttl = tt.tv_sec
					+
				0.95 * (int) (dp->d_ttl - tt.tv_sec);
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
			if (type != T_ANY) {
				hp->rcode = NOERROR_NODATA;
				*countp = 0;
				return 1; /* XXX - we have to report success */
			}
			/* don't satisfy T_ANY queries from -$ info */
			continue;
		}
#ifndef RETURNSOA
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
			if (type != T_ANY) {
				hp->rcode = NXDOMAIN;
				*countp = 0;
				return 1; /* XXX - we have to report success */
			}
			/* don't satisfy T_ANY queries from -$ info */
			continue;
		}
#endif
#endif /*NCACHE*/

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
			if (type != T_ANY) {	/* or T_NS? */
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
	if (foundstale) {
		delete_all(np, class, type);
		/* XXX this isn't right if 'type' is something special
		 * such as T_AXFR or T_MAILB, since the matching done
		 * by match() in delete_all() is different from that
		 * done by wanted() above.
		 */
	}
	dprintf(3, (ddt, "finddata: added %d class %d type %d RRs\n",
		    count, class, type));
	*countp = count;
	return (*lenp - buflen);
}

/*
 * Do we want this data record based on the class and type?
 */
int
wanted(dp, class, type)
	struct databuf *dp;
	int class, type;
{
	dprintf(3, (ddt, "wanted(%#lx, %d, %d) [%s %s]\n",
		    (u_long)dp, class, type,
		    p_class(dp->d_class), p_type(dp->d_type)));

	if (dp->d_class != class && class != C_ANY)
		return (0);
	if (type == dp->d_type)
		return (1);
#ifdef NCACHE
	/*-ve $ing stuff, for a T_ANY query, we do not want to return
	 * -ve $ed RRs.
	 */
	if (type == T_ANY && dp->d_rcode == NOERROR_NODATA)
		return (0);
#endif

	switch (dp->d_type) {
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
		    (u_long)np, np->n_dname, p_class(class), p_type(type)));
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
