#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)ns_resp.c	4.65 (Berkeley) 3/3/91";
static char rcsid[] = "$Id: ns_resp.c,v 1.1.1.1 1994/09/22 19:46:13 pst Exp $";
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

static char		skipnameFailedAnswer[] = "skipname failed in answer",
			skipnameFailedQuery[] =	"skipname failed in query",
			outofDataQuery[] =	"ran out of data in query",
			outofDataAnswer[] =	"ran out of data in answer",
#ifdef LAME_DELEGATION
			expandFailedQuery[] =	"dn_expand failed in query",
			expandFailedAuth[] =	"dn_expand failed in authority",
			outofDataAuth[] =	"ran out of data in authority",
#endif /* LAME_DELEGATION */
			dlenOverrunAnswer[] =	"dlen overrun in answer",
			dlenUnderrunAnswer[] =	"dlen underrun in answer",
			outofDataFinal[] =	"out of data in final pass",
			outofDataAFinal[] =	"out of data after final pass";

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
#ifdef VALIDATE
	register u_char *tempcp;
	struct sockaddr_in *server = &from_addr;
	int *validatelist;
	int lesscount;
#endif
	struct sockaddr_in *nsa;
	struct databuf *nsp[NSMAX], **nspp;
	int i, c, n, ancount, aucount, nscount, arcount;
	int old_ancount;
	int type, class, dbflags;
	int cname = 0; /* flag for processing cname response */
	int count, founddata, foundname;
	int buflen;
	int newmsglen;
	char name[MAXDNAME], *dname;
	char *fname;
	char *formerrmsg = "brain damage";
	u_char newmsg[BUFSIZ];
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
	 *  Here we handle bad responses from servers.
	 *  Several possibilities come to mind:
	 *	The server is sick and returns SERVFAIL
	 *	The server returns some garbage opcode (its sick)
	 *	The server can't understand our query and return FORMERR
	 *  In all these cases, we simply drop the packet and force
	 *  a retry.  This will make him look bad due to unresponsiveness.
	 */
	if ((hp->rcode != NOERROR && hp->rcode != NXDOMAIN)
#ifndef NCACHE
	    || (hp->rcode == NXDOMAIN && !hp->aa) /* must accept this one if
						   * we allow negative caching
						   */
#endif /*NCACHE*/
	    || hp->opcode != QUERY) {
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
		return;
	}
#ifdef LAME_DELEGATION
	/*
	 *  Non-authoritative, no answer, no error
	 */
	if (hp->rcode == NOERROR && !hp->aa && ntohs(hp->ancount) == 0 &&
		ntohs(hp->nscount) > 0) {

#ifdef LAME_LOGGING
		char qname[MAXDNAME];
#endif /* LAME_LOGGING */

#ifdef DEBUG
		if (debug > 0)
			fp_query(msg, ddt);
#endif

		cp = msg + HFIXEDSZ;
		dpp = dnptrs;
		*dpp++ = msg;
		if ((*cp & INDIR_MASK) == 0)
			*dpp++ = cp;
		*dpp = NULL;
		if (hp->qdcount) {
#ifdef LAME_LOGGING
			n = dn_expand(msg, msg + msglen, cp, qname,
				      sizeof(qname));
	    		if (n <= 0) {
				formerrmsg = expandFailedQuery;
				goto formerr;
			}
#else /* LAME_LOGGING */
			n = dn_skipname(cp, msg + msglen);
	    		if (n <= 0) {
				formerrmsg = skipnameFailedQuery;
				goto formerr;
			}
#endif /* LAME_LOGGING */
			cp += n;
			GETSHORT(type, cp);
			GETSHORT(class, cp);
			if (cp - msg > msglen) {
				formerrmsg = outofDataQuery;
				goto formerr;
			}
#ifdef LAME_LOGGING
		} else {
			strcpy(qname, "[No query name!]");
#endif /* LAME_LOGGING */
		}
		n = dn_expand(msg, msg + msglen, cp, name, sizeof name);
		if (n < 0) {
			formerrmsg = expandFailedAuth;
			goto formerr;
		}
		cp += n;
		GETSHORT(type, cp);
		if (cp - msg > msglen) {
			formerrmsg = outofDataAuth;
			goto formerr;
		}

		/*
		 * If the answer delegates us either to the same level in
		 * the hierarchy or closer to the root, we consider this
		 * server lame.
		 */

		if (type == T_NS && samedomain(qp->q_domain, name)) {
			nameserIncr(from_addr.sin_addr, nssRcvdLDel);
#ifdef LAME_LOGGING
			if (!haveComplained((char*)dhash((u_char*)name,
							 strlen(name)),
					    (char*)dhash((u_char*)qp->q_domain,
							 strlen(qp->q_domain)
							 )
					    )
			    ) {
				syslog(LAME_LOGGING,
"Lame delegation to '%s' from [%s] (server for '%s'?) on query on name '%s'\n",
				       name, inet_ntoa(from_addr.sin_addr),
				       qp->q_domain, qname);
			}
#endif /* LAME_LOGGING */
			return;
		}
	}
#endif /* LAME_DELEGATION */


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
			/* XXX - should put this in STATS somewhere */
			break;
		}
	}
	/* XXX:	note bad ambiguity here.  if one of our forwarders is also
	 *	a delegated server for some domain, then we will not update
	 *	the RTT information on any replies we get from those servers.
	 */
	/*
	 * If we were using nameservers, find the qinfo pointer and update
	 * the rtt and fact that we have called on this server before.
	 */
	if (fwd == (struct fwdinfo *)NULL) {
		struct timeval *stp;

		for (n = 0, qs = qp->q_addr; n < qp->q_naddr; n++, qs++)
			if (qs->ns_addr.sin_addr.s_addr ==
			    from_addr.sin_addr.s_addr)
				break;
		if (n >= qp->q_naddr) {
			if (!haveComplained((char*)from_addr.sin_addr.s_addr,
					    "unexpected source")) {
				syslog(LOG_NOTICE,
				     "Response from unexpected source [%s].%d",
				       inet_ntoa(from_addr.sin_addr),
				       ntohs(from_addr.sin_port));
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
			while (qs > qp->q_addr &&
			    (qs->stime.tv_sec == 0 || qs->ns != ns))
				qs--;
			*stp = qs->stime;
			/* XXX - sometimes stp still ends up pointing to
			 * a zero timeval, in spite of the above attempt.
			 * Why?  What should we do about it?
			 */
			dprintf(1, (ddt,
			    "Response from unused address %s, assuming %s\n",
				    inet_ntoa(from_addr.sin_addr),
				    inet_ntoa(qs->ns_addr.sin_addr)));
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

		dprintf(3, (ddt, "stime %d/%d  now %d/%d rtt %d\n",
			    stp->tv_sec, stp->tv_usec,
			    tt.tv_sec, tt.tv_usec, rtrip));

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
			dprintf(2, (ddt, "NS #%d addr [%s] used, rtt %d\n",
				    n, inet_ntoa(qs->ns_addr.sin_addr),
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

		for (n = 0, qs = qp->q_addr; n < qp->q_naddr; n++, qs++) {
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
			dprintf(2, (ddt, "NS #%d [%s] rtt now %d\n", n,
				    inet_ntoa(qs->ns_addr.sin_addr),
				    ns2->d_nstime));
		}
	}

	/*************************************************************/

	/*
	 * Skip query section
	 */
	free_addinfo();		/* sets addcount to zero */
	cp = msg + HFIXEDSZ;
	dpp = dnptrs;
	*dpp++ = msg;
	if ((*cp & INDIR_MASK) == 0)
		*dpp++ = cp;
	*dpp = NULL;
	type = class = 0;
	if (hp->qdcount) {
		n = dn_skipname(cp, msg + msglen);
	    	if (n <= 0) {
			formerrmsg = skipnameFailedQuery;
			goto formerr;
		}
		cp += n;
		GETSHORT(type, cp);
		GETSHORT(class, cp);
		if (cp - msg > msglen) {
			formerrmsg = outofDataQuery;
			goto formerr;
		}
	}

	/*
	 * Save answers, authority, and additional records for future use.
	 */
	ancount = ntohs(hp->ancount);
	aucount = ntohs(hp->nscount);
	arcount = ntohs(hp->arcount);
	nscount = 0;
	tp = cp;
	dprintf(3, (ddt, "resp: ancount %d, aucount %d, arcount %d\n",
		    ancount, aucount, arcount));

	/*
	 *  If there's an answer, check if it's a CNAME response;
	 *  if no answer but aucount > 0, see if there is an NS
	 *  or just an SOA.  (NOTE: ancount might be 1 with a CNAME,
	 *  and NS records may still be in the authority section;
	 *  we don't bother counting them, as we only use nscount
	 *  if ancount == 0.)
	 */
	if (ancount == 1 || (ancount == 0 && aucount > 0)) {
		c = aucount;
		do {
			if (tp - msg >= msglen) {
				formerrmsg = outofDataAnswer;
				goto formerr;
			}
			n = dn_skipname(tp, msg + msglen);
			if (n <= 0) {
				formerrmsg = skipnameFailedAnswer;
				goto formerr;
			}
			tp += n;  		/* name */
			GETSHORT(i, tp);	/* type */
			tp += INT16SZ; /* class */
			tp += INT32SZ; /* ttl */
			GETSHORT(count, tp); 	/* dlen */
			if (tp - msg > msglen - count) {
				formerrmsg = dlenOverrunAnswer;
				goto formerr;
			}
			tp += count;
			if (ancount && i == T_CNAME) {
				cname++;
				dprintf(1,
					(ddt,
					 "CNAME - needs more processing\n"
					 )
					);
				if (!qp->q_cmsglen) {
					qp->q_cmsg = qp->q_msg;
					qp->q_cmsglen = qp->q_msglen;
					qp->q_msg = NULL;
					qp->q_msglen = 0;
				}
			}
			/*
			 * See if authority record is a nameserver.
			 */
			if (ancount == 0 && i == T_NS)
				nscount++;
		} while (--c > 0);
		tp = cp;
	}

	if (qp->q_flags & Q_ZSERIAL) {
		if ((hp->aa)
		    && (ancount != 0)
		    && (hp->rcode == NOERROR)
		    && (type == T_SOA)
		    && ((class == C_IN) || (class == C_HS))
		    ) {		/* XXX - should check name, too */
			int n;
			u_int16_t dlen;
			u_int32_t serial;
			u_char *tp = cp;

			if (0 >= (n = dn_skipname(tp, msg + msglen))) {
				formerrmsg = skipnameFailedAnswer;
				goto formerr;
			}
			tp += n  		/* name */
			   + INT16SZ  /* type */
			   + INT16SZ  /* class */
			   + INT32SZ; /* ttl */
			GETSHORT(dlen, tp); 	/* dlen */

			if (dlen < (5 * INT32SZ)) {
				formerrmsg = dlenUnderrunAnswer;
				goto formerr;
			}

			if (0 >= (n = dn_skipname(tp, msg + msglen))) {
				formerrmsg = skipnameFailedAnswer;
				goto formerr;
			}
			tp += n;  		/* mname */
			if (0 >= (n = dn_skipname(tp, msg + msglen))) {
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
#ifdef NCACHE
	/* -ve $ing non-existence of record, must handle non-authoritative
	 * NOERRORs with c == 0.
	 */
	if (!hp->aa && hp->rcode == NOERROR && c == 0) {
		goto return_msg;
	} /*should ideally validate message before returning it*/
#endif /*NCACHE*/
#ifdef notdef
	/*
	 * If the request was for a CNAME that doesn't exist,
	 * but the name is valid, fetch any other data for the name.
	 * DON'T do this now, as it will requery if data are already
	 * in the cache (maybe later with negative caching).
	 */
	if (hp->qdcount && type == T_CNAME && c == 0 && hp->rcode == NOERROR
	    && !(qp->q_flags & Q_SYSTEM)) {
		dprintf(4, (ddt, "resp: leaving, no CNAME\n"));

		/* Cause us to put it in the cache later */
		prime(class, T_ANY, qp);

		/* Nothing to store, just give user the answer */
		goto return_msg;
	}
#endif /* notdef */

	nspp = nsp;
	if (qp->q_flags & Q_SYSTEM)
		dbflags = DB_NOTAUTH | DB_NODATA;
	else
		dbflags = DB_NOTAUTH | DB_NODATA | DB_NOHINTS;
	count = c;
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
#ifdef VALIDATE
	tempcp = cp;
	validatelist = (int *)malloc(count * sizeof(int));
	lesscount = 0; /*initialize*/
	old_ancount = ancount;
	for (i = 0; i < count; i++) {
		int VCode;
		if (tempcp >= msg + msglen) {
			free((char *)validatelist);
			formerrmsg = outofDataFinal;
			goto formerr;
		}
		if ((n = dovalidate(msg, msglen, tempcp, 0,
				    dbflags, server, &VCode)) < 0) {
			dprintf(1, (ddt,
				    "resp: leaving, dovalidate failed\n"));
			free((char *)validatelist);

			/* return code filled in by dovalidate */
			goto return_msg;
		}
		validatelist[i] = VCode;
		if (VCode == INVALID) lesscount++;
		tempcp += n;
	}

	/* need to delete INVALID records from the message
	 * and change fields appropriately
	 */
	n = update_msg(msg, &msglen, validatelist, count);
	free((char *)validatelist);
	if (n < 0)
		goto formerr;
	count -= lesscount;

	if (old_ancount && !hp->ancount) {
		/* We lost all the answers */
		dprintf(1, (ddt, "validate count -> 0"));
		return;
	}
	ancount = ntohs(hp->ancount);
#endif

	for (i = 0;  i < count;  i++) {
		struct databuf *ns3;
		u_char cred;

		if (cp >= msg + msglen) {
			formerrmsg = outofDataFinal;
			goto formerr;
		}
		if (i < ancount) {
			cred = hp->aa ? DB_C_AUTH : DB_C_ANSWER;
		} else {
			cred = DB_C_ADDITIONAL;
		}
		ns3 = 0;
		n = doupdate(msg, msglen, cp, 0, &ns3, dbflags, cred);
		if (n < 0) {
			dprintf(1, (ddt, "resp: leaving, doupdate failed\n"));

			/* return code filled in by doupdate */
			goto return_msg;
		}
		/*
		 * Remember nameservers from the authority section
		 * for referrals.
		 * (This is usually overwritten by findns below(?). XXX
		 */
		if (ns3 && i >= ancount && i < ancount + aucount &&
		    nspp < &nsp[NSMAX-1]) {
			*nspp++ = ns3;
#ifdef DATUMREFCNT
			ns3->d_rcnt++;
			*nspp = NULL;
#endif
		}
		cp += n;
	}

	if ((qp->q_flags & Q_SYSTEM) && ancount) {
		if (qp->q_flags & Q_PRIMING)
			check_root();
		dprintf(3, (ddt, "resp: leaving, SYSQUERY ancount %d\n",
			    ancount));
		qremove(qp);
#ifdef	DATUMREFCNT
		free_nsp(nsp);
#endif
		return;
	}

	if (cp > msg + msglen) {
		formerrmsg = outofDataAFinal;
		goto formerr;
	}

	/*
	 *  If there are addresses and this is a local query,
	 *  sort them appropriately for the local context.
	 */
	if (ancount > 1 && (lp = local(&qp->q_from)) != NULL)
		sort_response(tp, ancount, lp, msg + msglen);

	/*
	 * An answer to a T_ANY query or a successful answer to a
	 * regular query with no indirection, then just return answer.
	 */
	if ((hp->qdcount && type == T_ANY && ancount) ||
	    (!cname && !qp->q_cmsglen && ancount)) {
		dprintf(3, (ddt, "resp: got as much answer as there is\n"));
		goto return_msg;
	}

	/*
	 * We might want to cache this negative answer.
	 */
	if (!ancount &&
	    (!nscount || hp->rcode == NXDOMAIN) &&
	    (hp->aa || fwd || class == C_ANY)) {
		/* we have an authoritative NO */
		dprintf(3, (ddt, "resp: leaving auth NO\n"));
		if (qp->q_cmsglen) {
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
	if (!cname && qp->q_cmsglen && ancount) {
		dprintf(1, (ddt, "Cname second pass\n"));
		newmsglen = qp->q_cmsglen;
		bcopy(qp->q_cmsg, newmsg, newmsglen);
	} else {
		newmsglen = msglen;
		bcopy(msg, newmsg, newmsglen);
	}
	hp = (HEADER *) newmsg;
	hp->ancount = 0;
	hp->nscount = 0;
	hp->arcount = 0;
	dnptrs[0] = newmsg;
	dnptrs[1] = NULL;
	cp = newmsg + HFIXEDSZ;
	if (cname)
		cp += dn_skipname(cp, newmsg + newmsglen) + QFIXEDSZ;
	n = dn_expand(newmsg, newmsg + newmsglen, cp, dname, sizeof name);
	if (n < 0) {
		dprintf(1, (ddt, "dn_expand failed\n"));
		goto servfail;
	}
	if (!cname)
		cp += n + QFIXEDSZ;
	buflen = sizeof(newmsg) - (cp - newmsg);

try_again:
	dprintf(1, (ddt, "resp: nlookup(%s) type=%d\n", dname, type));
	fname = "";
	htp = hashtab;		/* lookup relative to root */
	np = nlookup(dname, &htp, &fname, 0);
	dprintf(1, (ddt, "resp: %s '%s' as '%s' (cname=%d)\n",
		    np == NULL ? "missed" : "found", dname, fname, cname));
	if (np == NULL || fname != dname)
		goto fetch_ns;

	foundname++;
	count = cp - newmsg;
	n = finddata(np, class, type, hp, &dname, &buflen, &count);
	if (n == 0)
		goto fetch_ns;		/* NO data available */
	cp += n;
	buflen -= n;
	hp->ancount += count;
	if (fname != dname && type != T_CNAME && type != T_ANY) {
		cname++;
		goto try_again;
	}
	founddata = 1;

	dprintf(3, (ddt,
		    "resp: foundname=%d, count=%d, founddata=%d, cname=%d\n",
		    foundname, count, founddata, cname));

fetch_ns:
	hp->ancount = htons(hp->ancount);
	/*
 	 * Look for name servers to refer to and fill in the authority
 	 * section or record the address for forwarding the query
 	 * (recursion desired).
 	 */
#ifdef	DATUMREFCNT
	free_nsp(nsp);
#endif
	switch (findns(&np, class, nsp, &count, 0)) {
	case NXDOMAIN:		/* shouldn't happen */
		dprintf(3, (ddt, "req: leaving (%s, rcode %d)\n",
			    dname, hp->rcode));
		if (!foundname)
			hp->rcode = NXDOMAIN;
		if (class != C_ANY) {
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
		n = add_data(np, nsp, cp, buflen);
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
		dprintf(1,
			(ddt,
			 "resp: MAXQUERIES exceeded (%s, class %d, type %d)\n",
			 dname, class, type
			 )
			);
		syslog(LOG_NOTICE,
		    	    "MAXQUERIES exceeded, possible data loop in resolving (%s)",
			    dname);
		goto servfail;
	}

	/* Reset the query control structure */
#ifdef	DATUMREFCNT
	for (i = 0 ; i < qp->q_naddr ; i++) {
		if ((--(qp->q_addr[i].ns->d_rcnt))) {
			dprintf(1 ,(ddt, "ns_resp: ns %s rcnt %d\n",
					qp->q_addr[i].ns->d_data,
					qp->q_addr[i].ns->d_rcnt));
		} else {
			dprintf(1 ,(ddt, "ns_resp: ns %s rcnt %d delayed\n",
					qp->q_addr[i].ns->d_data,
					qp->q_addr[i].ns->d_rcnt));
			free((char*)qp->q_addr[i].ns);
		}
		if ((--(qp->q_addr[i].nsdata->d_rcnt))) {
			dprintf(1 ,(ddt, "ns_resp: nsdata %08.8X rcnt %d\n",
					*(int32_t *)(qp->q_addr[i].nsdata->d_data),
					qp->q_addr[i].nsdata->d_rcnt));
		} else {
			dprintf(1 ,(ddt, "ns_resp: nsdata %08.8X rcnt %d delayed\n",
					*(int32_t *)(qp->q_addr[i].nsdata->d_data),
					qp->q_addr[i].nsdata->d_rcnt));
			free((char*)qp->q_addr[i].nsdata);
		}
	}
#endif
	qp->q_naddr = 0;
	qp->q_curaddr = 0;
	qp->q_fwd = fwdtab;
#ifdef LAME_DELEGATION
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
	for (n = 0; n < qp->q_naddr; n++)
		qp->q_addr[n].stime.tv_sec = 0;
	if (!qp->q_fwd)
		qp->q_addr[0].stime = tt;
	if (cname) {
	 	if (qp->q_cname++ == MAXCNAMES) {
			dprintf(3, (ddt,
				    "resp: leaving, MAXCNAMES exceeded\n"));
			goto servfail;
	 	}
		dprintf(1, (ddt, "q_cname = %d\n",qp->q_cname));
		dprintf(3, (ddt,
			    "resp: building recursive query; nslookup\n"));
		if (qp->q_msg)
			(void) free(qp->q_msg);
		if ((qp->q_msg = (u_char *)malloc(BUFSIZ)) == NULL) {
			dprintf(1, (ddt, "resp: malloc error\n"));
			goto servfail;
		}
		qp->q_msglen = res_mkquery(QUERY, dname, class,
					   type, NULL, 0, NULL,
					   qp->q_msg, BUFSIZ);
		hp = (HEADER *) qp->q_msg;
	    	hp->rd = 0;
	} else
		hp = (HEADER *)qp->q_msg;
	hp->id = qp->q_nsid = htons((u_int16_t)++nsid);
	if (qp->q_fwd)
		hp->rd = 1;
	unsched(qp);
	schedretry(qp, retrytime(qp));
	nsa = Q_NEXTADDR(qp, 0);
	dprintf(1, (ddt, "resp: forw -> [%s].%d ds=%d nsid=%d id=%d %dms\n",
		    inet_ntoa(nsa->sin_addr),
		    ntohs(nsa->sin_port), ds,
		    ntohs(qp->q_nsid), ntohs(qp->q_id),
		    (qp->q_addr[0].nsdata != NULL)
			? qp->q_addr[0].nsdata->d_nstime
			: (-1)));
#ifdef DEBUG
	if (debug >= 10)
		fp_query(msg, ddt);
#endif
	if (sendto(ds, qp->q_msg, qp->q_msglen, 0,
		   (struct sockaddr *)nsa,
		   sizeof(struct sockaddr_in)) < 0) {
		if (!haveComplained((char*)nsa->sin_addr.s_addr, sendtoStr))
			syslog(LOG_NOTICE, "ns_resp: sendto([%s].%d): %m",
			       inet_ntoa(nsa->sin_addr), ntohs(nsa->sin_port));
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
	dprintf(3, (ddt,
		    "FORMERR resp() from [%s].%d size err %d, msglen %d\n",
		    inet_ntoa(from_addr.sin_addr),
		    ntohs(from_addr.sin_port),
		    cp - msg, msglen));
	if (!haveComplained((char*)from_addr.sin_addr.s_addr,
			    (char*)dhash((u_char *)formerrmsg,
					 strlen(formerrmsg)
					 )
			    )
	    ) {
		syslog(LOG_INFO, "Malformed response from [%s].%d (%s)\n",
		       inet_ntoa(from_addr.sin_addr),
		       ntohs(from_addr.sin_port),
		       formerrmsg);
	}
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
	hp->ra = 1;
	(void) send_msg(msg, msglen, qp);
	qremove(qp);
#ifdef	DATUMREFCNT
	free_nsp(nsp);
#endif
	return;

return_newmsg:
	nameserIncr(qp->q_from.sin_addr, nssSentAns);
	if (addcount) {
		n = doaddinfo(hp, cp, buflen);
		cp += n;
		buflen -= n;
	}
	hp->qr = 1;
	hp->id = qp->q_id;
	hp->rd = 1;
	hp->ra = 1;
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
	hp->id = qp->q_id;
	hp->rd = 1;
	hp->ra = 1;
	hp->qr = 1;
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

	dprintf(3, (ddt, "doupdate(zone %d, savens %x, flags %x)\n",
		    zone, savens, flags));

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

	default:
		dprintf(3, (ddt, "unknown type %d\n", type));
		return ((cp - rrp) + dlen);
	}
	if (n > MAXDATA) {
		dprintf(1, (ddt,
			    "update type %d: %d bytes is too much data\n",
			    type, n));
		hp->rcode = NOCHANGE;	/* XXX - FORMERR ??? */
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
			    dprintf(1,
				    (ddt,
				     "FORMERR UPDATE message length off\n"
				     )
				    );
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
		char qname[MAXDNAME], *temp;
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
			 "bogus root NS %s rcvd from [%s] on query for \"%s\"",
				       data, inet_ntoa(from_addr.sin_addr),
				       qname);
			return (cp - rrp);
		}
#ifdef BOGUSNS
		if (bogusns) {
			if (!haveComplained((char*)from_addr.sin_addr.s_addr,
					    "bogus nonroot NS"))
				syslog(LOG_NOTICE,
		      "bogus nonroot NS %s rcvd from [%s] on query for \"%s\"",
				       data, inet_ntoa(from_addr.sin_addr),
				       qname);
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
			fprintf(ddt,"update failed (%d)\n", n);
		else if (debug >= 3)
			fprintf(ddt,"update failed (DATAEXISTS)\n");
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
		fprintf(ddt,"send_msg -> [%s] (%s %d %d) id=%d\n",
			inet_ntoa(qp->q_from.sin_addr),
			qp->q_stream == QSTREAM_NULL ? "UDP" : "TCP",
			qp->q_stream == QSTREAM_NULL ? qp->q_dfd
						     : qp->q_stream->s_rfd,
			ntohs(qp->q_from.sin_port),
			ntohs(qp->q_id));
	}
	if (debug>4) {
		struct qinfo *tqp;

		for (tqp = qhead; tqp!=QINFO_NULL; tqp = tqp->q_link) {
		    fprintf(ddt, "qp %x q_id: %d  q_nsid: %d q_msglen: %d ",
		    	tqp, tqp->q_id,tqp->q_nsid,tqp->q_msglen);
	            fprintf(ddt,"q_naddr: %d q_curaddr: %d\n", tqp->q_naddr,
			tqp->q_curaddr);
	            fprintf(ddt,"q_next: %x q_link: %x\n", qp->q_next,
		   	 qp->q_link);
		}
	}
	if (debug >= 10)
		fp_query(msg, ddt);
#endif /* DEBUG */
	if (qp->q_stream == QSTREAM_NULL) {
		if (sendto(qp->q_dfd, msg, msglen, 0,
			   (struct sockaddr *)&qp->q_from,
			   sizeof(qp->q_from)) < 0) {
			if (!haveComplained((char*)qp->q_from.sin_addr.s_addr,
					    sendtoStr))
				syslog(LOG_NOTICE,
				       "send_msg: sendto([%s].%d): %m",
				       inet_ntoa(qp->q_from.sin_addr),
				       ntohs(qp->q_from.sin_port));
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
	(void) sysquery(dname, class, type, NULL, 0);
}
#endif

void
prime_cache()
{
	register struct qinfo *qp;

	dprintf(1, (ddt, "prime_cache: priming = %d\n", priming));
	if (!priming && fcachetab->h_tab[0] != NULL && !forward_only) {
		priming++;
		if ((qp = sysquery("", C_IN, T_NS, NULL, 0)) == NULL)
			priming = 0;
		else
			qp->q_flags |= (Q_SYSTEM | Q_PRIMING);
	}
	needs_prime_cache = 0;
	return;
}

struct qinfo *
sysquery(dname, class, type, nss, nsc)
	char *dname;
	int class, type;
	struct in_addr *nss;
	int nsc;
{
	register struct qinfo *qp, *oqp;
	register HEADER *hp;
	struct namebuf *np;
	struct databuf *nsp[NSMAX];
	struct hashbuf *htp;
	struct sockaddr_in *nsa;
	char *fname;
	int count;

#ifdef	DATUMREFCNT
	nsp[0] = NULL;
#endif
	dprintf(3, (ddt, "sysquery(%s, %d, %d, 0x%x, %d)\n",
		    dname, class, type, nss, nsc));
	qp = qnew();

	if (nss && nsc) {
		np = NULL;
	} else {
		htp = hashtab;
		if (priming && dname[0] == '\0') {
			np = NULL;
		} else if ((np = nlookup(dname, &htp, &fname, 1)) == NULL) {
			dprintf(1, (ddt,
				    "sysquery: nlookup error on %s?\n",
				    dname));
			qfree(qp);
			return (0);
		}

		switch (findns(&np, class, nsp, &count, 0)) {
		case NXDOMAIN:
		case SERVFAIL:
			dprintf(1, (ddt,
				    "sysquery: findns error on %s?\n", dname));
			qfree(qp);
#ifdef	DATUMREFCNT
			free_nsp(nsp);
#endif
			return (0);
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
#ifdef LAME_DELEGATION
	getname(np, qp->q_domain, sizeof(qp->q_domain));
#endif /* LAME_DELEGATION */

	if ((qp->q_msg = (u_char *)malloc(BUFSIZ)) == NULL) {
		qfree(qp);
#ifdef	DATUMREFCNT
		free_nsp(nsp);
#endif
		return (0);
	}
	qp->q_msglen = res_mkquery(QUERY, dname, class,
				   type, NULL, 0, NULL,
				   qp->q_msg, BUFSIZ);
	hp = (HEADER *) qp->q_msg;
	hp->id = qp->q_nsid = htons((u_int16_t)++nsid);
	hp->rd = (qp->q_fwd ? 1 : 0);

	/* First check for an already pending query for this data */
	for (oqp = qhead;  oqp != QINFO_NULL;  oqp = oqp->q_link) {
		if ((oqp != qp)
		    && (oqp->q_msglen == qp->q_msglen)
		    && bcmp((char *)oqp->q_msg+2,
			    qp->q_msg+2,
			    qp->q_msglen-2) == 0
		    ) {
			dprintf(3, (ddt, "sysquery: duplicate\n"));
			qfree(qp);
#ifdef	DATUMREFCNT
			free_nsp(nsp);
#endif
			return (0);
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
	    if ((count = nslookup(nsp, qp, dname, "sysquery")) <= 0) {
		if (count < 0) {
			dprintf(1, (ddt,
				    "sysquery: nslookup reports danger\n"));
		} else {
			dprintf(1, (ddt,
				    "sysquery: no addrs found for NS's\n"));
		}
		qfree(qp);
#ifdef	DATUMREFCNT
		free_nsp(nsp);
#endif
		return (0);
	    }
	}

	schedretry(qp, retrytime(qp));
	if (qp->q_fwd == NULL)
		qp->q_addr[0].stime = tt;	/* XXX - why not every? */
	nsa = Q_NEXTADDR(qp, 0);

	dprintf(1, (ddt,
		  "sysquery: send -> [%s].%d dfd=%d nsid=%d id=%d retry=%ld\n",
		    inet_ntoa(nsa->sin_addr),
		    ntohs(nsa->sin_port), qp->q_dfd,
		    ntohs(qp->q_nsid), ntohs(qp->q_id),
		    qp->q_time));
#ifdef DEBUG
	if (debug >= 10)
		fp_query(qp->q_msg, ddt);
#endif
	if (sendto(qp->q_dfd, qp->q_msg, qp->q_msglen, 0,
		   (struct sockaddr *)nsa,
		   sizeof(struct sockaddr_in)) < 0) {
		if (!haveComplained((char*)nsa->sin_addr.s_addr, sendtoStr))
			syslog(LOG_NOTICE, "sysquery: sendto([%s].%d): %m",
			       inet_ntoa(nsa->sin_addr), ntohs(nsa->sin_port));
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
		syslog(LOG_ERR, "check_root: Can't find root!\n");
		return;
	}
	for (dp = np->n_data; dp != NULL; dp = dp->d_next)
		if (dp->d_type == T_NS)
			count++;
	dprintf(1, (ddt, "%d root servers\n", count));
	if (count < MINROOTS) {
		syslog(LOG_WARNING,
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
	char *fname;
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
					    "check_ns: %s: not found %s %x\n",
					    dname, fname, tnp));
			    sysquery(dname, dp->d_class, T_A, NULL, 0);
			    continue;
			}
			/* look for name server addresses */
			found_arr = 0;
			for (tdp=tnp->n_data; tdp!=NULL; tdp=tdp->d_next) {
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
			    (void) sysquery(dname, dp->d_class, T_A, NULL, 0);
	        }
	}
}

/* int findns(npp, class, nsp, countp, flag)
 *	Find NS' or an SOA
 * npp, class:
 *	dname whose least-superior NS is wanted
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
		dprintf(5, (ddt, "findns: np 0x%x '%s'\n", np, np->n_dname));
		/* Look first for SOA records. */
#ifdef ADDAUTH
		if (!flag)
#endif
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			if (dp->d_zone != 0 && match(dp, class, T_SOA)) {
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
			/*
			 * Don't use records that may become invalid to
			 * reference later when we do the rtt computation.
			 * Never delete our safety-belt information!
			 *
			 * XXX:	this is horribly bogus.
			 */
			if ((dp->d_zone == 0) &&
			    (dp->d_ttl < (tt.tv_sec+900)) &&
			    !(dp->d_flags & DB_F_HINT)) {
				dprintf(1, (ddt, "findns: stale entry '%s'\n",
					    np->n_dname));
				/* Cache invalidate the NS RR's. */
				if (dp->d_ttl < tt.tv_sec)
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
		syslog(LOG_ERR, "No root nameservers for class %s\n",
		       p_class(class));
	}
	return (SERVFAIL);
}

/*
 *  Extract RR's from the given node that match class and type.
 *  Return number of bytes added to response.
 *  If no matching data is found, then 0 is returned.
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

		for (dpp = &np->n_data;  dp = *dpp;  dpp = &dp->d_next) {
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
				syslog(LOG_WARNING,
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
				syslog(LOG_WARNING,
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
				if (dp->d_zone &&
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

	dprintf(3, (ddt, "wanted(%x, %d, %d) %d, %d\n", dp, class, type,
		    dp->d_class, dp->d_type));

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
 *  added if truncation was required.  Typically you are
 *  adding NS records to a response.
 */
int
add_data(np, dpp, cp, buflen)
	struct namebuf *np;
	struct databuf **dpp;
	register u_char *cp;
	int buflen;
{
	register struct databuf *dp;
	char dname[MAXDNAME];
	register int n, count = 0;

	getname(np, dname, sizeof(dname));
	for(dp = *dpp++; dp != NULL; dp = *dpp++) {
		if (stale(dp))
			continue;	/* ignore old cache entry */
#ifdef NCACHE
		if (dp->d_rcode)
			continue;
#endif
		if ((n = make_rr(dname, dp, cp, buflen, 1)) < 0)
			return (-count);	/* Truncation */
		cp += n;
		buflen -= n;
		count += n;
	}
	return (count);
}

/*
 *  This is best thought of as a "cache invalidate" function.
 *  It is called whenever a piece of data is determined to have
 *  timed out.  It is better to have no information, than to
 *  have partial information you pass off as complete.
 */
void
delete_all(np, class, type)
	register struct namebuf *np;
	int class, type;
{
	register struct databuf *dp, *pdp;

	dprintf(3, (ddt, "delete_all: '%s' 0x%x class %d type %d\n",
		    np->n_dname, np, class, type));
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
