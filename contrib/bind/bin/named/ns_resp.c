#if !defined(lint) && !defined(SABER)
static const char sccsid[] = "@(#)ns_resp.c	4.65 (Berkeley) 3/3/91";
static const char rcsid[] = "$Id: ns_resp.c,v 8.144 2000/07/11 08:26:09 vixie Exp $";
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
			danglingCname[] =	"dangling CNAME pointer",
			nonRecursiveForwarder[]= "non-recursive forwarder";

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

static void		rrsetadd(struct flush_set *, const char *, 
				 struct databuf *),
			rrsetupdate(struct flush_set *, int flags,
				    struct sockaddr_in, int),
			flushrrset(struct flush_set *, struct sockaddr_in),
			free_flushset(struct flush_set *, int),
			check_hints(struct flush_set *);
static int		rrsetcmp(char *, struct db_list *, struct hashbuf *),
			check_root(void),
			check_ns(void),
			wanted(const struct databuf *, int, int),
			wantedsig(const struct databuf *, int, int),
			rrextract(u_char *, int, u_char *,
				  struct databuf **, char *, int,
				  struct sockaddr_in, char **);
static void		mark_bad(struct qinfo *qp, struct sockaddr_in from);
static void		mark_lame(struct qinfo *qp, struct sockaddr_in from);
static void		fast_retry(struct qinfo *qp, struct sockaddr_in from);
static void		add_related_additional(char *);
static void		free_related_additional(void);
static int		related_additional(char *);
static void		freestr_maybe(char **);
static enum ordering	match_order(const struct namebuf *, int, int);
static int 		match_name(const struct namebuf *, const char *, size_t);

#define MAX_RELATED 100

static int num_related = 0;
static char *related[MAX_RELATED];

static char *
learntFrom(struct qinfo *qp, struct sockaddr_in *server) {
	static char *buf = NULL;
	char *a, *ns, *na;
	struct databuf *db;
	int i;
	
	a = ns = na = "<Not Available>";

	for (i = 0; (u_int)i < qp->q_naddr; i++) {
		if (ina_equal(qp->q_addr[i].ns_addr.sin_addr,
			      server->sin_addr)) {
			db = qp->q_addr[i].ns;
			if (db != NULL) {
				if (NS_OPTION_P(OPTION_HOSTSTATS)) {
					char nsbuf[20];

					if (db->d_ns != NULL) {
						strcpy(nsbuf,
						    inet_ntoa(db->d_ns->addr));
						ns = nsbuf;
					} else {
						ns = zones[db->d_zone]
							.z_origin;
					}
				}
				if (db->d_rcode == 0)
					na = (char*)qp->q_addr[i].ns->d_data;
			}

			if (NS_OPTION_P(OPTION_HOSTSTATS)) {
				char abuf[20];

				db = qp->q_addr[i].nsdata;
				if (db != NULL) {
					if (db->d_ns != NULL) {
						strcpy(abuf,
						    inet_ntoa(db->d_ns->addr));
						a = abuf;
					} else {
						a = zones[db->d_zone].z_origin;
					}
				}
			}
			break;
		}
	}

	if (a == ns && ns == na)	/* all "UNKNOWN" */
		return (NULL);
	
	if (*a == '\0')
		a = "\".\"";
	if (*ns == '\0')
		ns = "\".\"";
	if (*na == '\0')
		na = "\".\"";

	if (NS_OPTION_P(OPTION_HOSTSTATS)) {
		static const char fmt[] = " '%s': learnt (A=%s,NS=%s)";

		buf = newstr(sizeof fmt + strlen(na) + strlen(a) + strlen(ns),
			     0);
		if (buf == NULL)
			return (NULL);
		sprintf(buf, fmt, na, a, ns);
	} else {
		static const char fmt[] = " '%s'";

		buf = newstr(sizeof fmt + strlen(na), 0);
		if (buf == NULL)
			return (NULL);
		sprintf(buf, fmt, na);
	}

	return (buf);
}

void
ns_resp(u_char *msg, int msglen, struct sockaddr_in from, struct qstream *qsp)
{
	struct qinfo *qp;
	HEADER *hp;
	struct qserv *qs = NULL;
	struct databuf *ns, *ns2;
	u_char *cp, *answers, *eom = msg + msglen;
	struct flush_set *flushset = NULL;
	int flushset_size = 0;
	struct sockaddr_in *nsa;
	struct databuf *nsp[NSMAX];
	int i, c, n, qdcount, ancount, aucount, nscount, arcount, arfirst;
	int soacount;
	u_int qtype, qclass;
	int restart;	/* flag for processing cname response */
	int validanswer, dbflags;
	int cname, lastwascname, externalcname;
	int count, founddata, foundname;
	int buflen;
	int newmsglen;
	char name[MAXDNAME], qname[MAXDNAME], aname[MAXDNAME];
	char msgbuf[MAXDNAME+100];
	char *dname, tmpdomain[MAXDNAME];
	const char *fname;
	const char *formerrmsg = "brain damage";
	u_char newmsg[PACKETSZ];
	u_char **dpp, *tp;
	time_t rtrip;
	struct hashbuf *htp;
	struct namebuf *np;
	struct fwdinfo *fwd;
	struct databuf *dp;
	int forcecmsg = 0;
	char *tname = NULL;
	int sendto_errno = 0;
	int has_tsig, oldqlen;
	u_char *oldqbuf;
	u_char *smsg;
	int smsglen, smsgsize, siglen;
	u_char sig[TSIG_SIG_SIZE];
	time_t tsig_time;
	DST_KEY *key;

	nameserIncr(from.sin_addr, nssRcvdR);
	nsp[0] = NULL;
	hp = (HEADER *) msg;
	if ((qp = qfindid(hp->id)) == NULL ) {
		ns_debug(ns_log_default, 1, "DUP? dropped (id %d)",
			 ntohs(hp->id));
		nameserIncr(from.sin_addr, nssRcvdDupR);
		return;
	}

	if (ns_wouldlog(ns_log_default, 2)) {
		ns_debug(ns_log_default, 2, "Response (%s %s %s) nsid=%d id=%d",
			 (qp->q_flags & Q_SYSTEM) ?"SYSTEM" :"USER",
			 (qp->q_flags & Q_PRIMING) ?"PRIMING" :"NORMAL",
			 (qp->q_flags & Q_ZSERIAL) ?"ZSERIAL" :"-",
			 ntohs(qp->q_nsid), ntohs(qp->q_id));
	}

	if (qp->q_nstsig == NULL)
		has_tsig = 0;
	else {
		int ret;

		ret = ns_verify(msg, &msglen, qp->q_nstsig->key,
				qp->q_nstsig->sig, qp->q_nstsig->siglen,
				NULL, NULL, &tsig_time, 0);
		if (ret == 0)
			has_tsig = 1;
		else {
			if (hp->rcode == NOERROR)
				hp->rcode = NOTAUTH;
			ns_debug(ns_log_default, 1,
				 "resp: error bad tsig, record dropped");
			return;
		}
	}

	/*
	 * Here we handle high level formatting problems by parsing the header.
	 */
	qdcount = ntohs(hp->qdcount);
	ancount = ntohs(hp->ancount);
	aucount = ntohs(hp->nscount);
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
		if (!ns_nameok(qp, qname, qclass, NULL, response_trans,
			       ns_ownercontext(qtype, response_trans),
			       qname, from.sin_addr)) {
			formerrmsg = badNameFound;
			goto refused;
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
		if (ns_samename(qp->q_name, qname) != 1 ||
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
		ns_debug(ns_log_default, 2,
			 "resp: error (ret %d, op %d), dropped",
			 hp->rcode, hp->opcode);
		switch (hp->rcode) {
		case SERVFAIL:
			nameserIncr(from.sin_addr, nssRcvdFail);
			break;
		case FORMERR:
			nameserIncr(from.sin_addr, nssRcvdFErr);
			break;
		default:
			nameserIncr(from.sin_addr, nssRcvdErr);
			break;
		}
		if (ns_samename(qp->q_name, qp->q_domain) == 1 &&
		    hp->rcode == SERVFAIL && hp->opcode == QUERY)
			mark_lame(qp, from);
		mark_bad(qp, from);
		fast_retry(qp, from);
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
	 * XXX - should put this in STATS somewhere.
	 */
	for (fwd = NS_ZFWDTAB(qp->q_fzone); fwd; fwd = fwd->next)
		if (ina_equal(fwd->fwddata->fwdaddr.sin_addr, from.sin_addr))
			break;
	/*
	/*
	 * find the qinfo pointer and update
	 * the rtt and fact that we have called on this server before.
	 */
	{
		struct timeval *stp;

		for (n = 0, qs = qp->q_addr; (u_int)n < qp->q_naddr; n++, qs++)
			if (ina_equal(qs->ns_addr.sin_addr, from.sin_addr))
				break;
		if ((u_int)n >= qp->q_naddr) {
			if (!haveComplained(ina_ulong(from.sin_addr),
					    (u_long)"unexpected source")) {
				ns_info(ns_log_default,
					"Response from unexpected source (%s)",
					sin_ntoa(from));
			}
			/* 
			 * We don't know who this response came from so it
			 * gets dropped on the floor.
			 */
			return;
		}
		stp = &qs->stime;

		/* Handle response from different (untried) interface. */
		if (qs->ns != NULL && stp->tv_sec == 0) {
			ns = qs->ns;
			while (qs > qp->q_addr
			       && (qs->stime.tv_sec == 0 || qs->ns != ns))
				qs--;
			*stp = qs->stime;
			/* XXX - sometimes stp still ends up pointing to
			 * a zero timeval, in spite of the above attempt.
			 * Why?  What should we do about it?
			 */
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
		
		if (ns_wouldlog(ns_log_default,3)) {
			ns_debug(ns_log_default, 3,
				 "stime %lu/%lu  now %lu/%lu rtt %ld",
				 (u_long)stp->tv_sec, (u_long)stp->tv_usec,
				 (u_long)tt.tv_sec, (u_long)tt.tv_usec,
				 (long)rtrip);
		}

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
			if (ns_wouldlog(ns_log_default,2)) {
				ns_debug(ns_log_default, 2,
					 "NS #%d addr %s used, rtt %d",
					 n, sin_ntoa(qs->ns_addr), ns->d_nstime);
			}
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
			if (!ns2 || ns2 == ns)
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
			if (ns_wouldlog(ns_log_default,2)) {
				ns_debug(ns_log_default, 2, "NS #%d %s rtt now %d", n,
					 sin_ntoa(qs->ns_addr),
					 ns2->d_nstime);
			}
		}
	}

#ifdef BIND_NOTIFY
	/*
	 * For now, NOTIFY isn't defined for ANCOUNT!=0, AUCOUNT!=0,
	 * or ADCOUNT!=0.  Therefore the only real work to be done for
	 * a NOTIFY-QR is to remove it from the query queue.
	 */
	if (hp->opcode == NS_NOTIFY_OP) {
		ns_info(ns_log_notify,
		      "Received NOTIFY answer (%sAA) from %s for \"%s %s %s\"",
			hp->aa ? "" : "!",
			inet_ntoa(from.sin_addr), 
			*(qp->q_name) ? qp->q_name : ".",
			p_class(qp->q_class), p_type(qp->q_type));
		qremove(qp);
		return;
	}
#endif

	if ((qp->q_flags & Q_ZSERIAL) != 0) {
		if (hp->aa && ancount > 0 && hp->rcode == NOERROR &&
		    qtype == T_SOA && (qclass == C_IN || qclass == C_HS))
		{
			int n;
			u_int type, class, dlen;
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
			if (!ns_nameok(qp, name, class, NULL, response_trans,
				       ns_ownercontext(type, response_trans),
				       name, from.sin_addr)) {
				formerrmsg = badNameFound;
				goto refused;
			}
			if (ns_samename(qname, name) != 1 ||
			    qtype != type || qclass != class) {
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
			for (n = 0, qs = qp->q_addr; (u_int)n < qp->q_naddr;
					n++, qs++)
				if (ina_equal(qs->ns_addr.sin_addr,
					      from.sin_addr))
					break;
			if (n == qp->q_naddr) {
				qserial_answer(qp);
				qremove(qp);
				return;
			}
			qs->serial = serial;
		}
		retry(qp);
		return;
	}

	/*
	 *  Non-authoritative, no answer, no error, with referral.
	 */
	if (hp->rcode == NOERROR && !hp->tc && !hp->aa &&
	    ancount == 0 && aucount > 0
#ifdef BIND_NOTIFY
	    && hp->opcode != NS_NOTIFY_OP
#endif
	    ) {
		u_char *tp;
		int type, class, dlen;
		int foundns, foundsoa;
#ifdef DEBUG
		if (debug > 0)
			res_pquery(&res, msg, msglen,
				   log_get_stream(packet_channel));
#endif
		/*
		 * Since there is no answer section (ancount == 0),
		 * we must be pointing at the authority section (aucount > 0).
		 */
		tp = cp;
		foundns = foundsoa = 0;
		for (i = 0 ; i < aucount ; i++) {
			n = dn_expand(msg, eom, tp, name, sizeof name);
			if (n < 0) {
				formerrmsg = expandFailedAuth;
				goto formerr;
			}
			tp += n;
			if (tp + 3 * INT16SZ + INT32SZ > eom) {
				formerrmsg = outofDataAuth;
				goto formerr;
			}
			GETSHORT(type, tp);
			GETSHORT(class, tp);
			tp += INT32SZ;	/* ttl */
			GETSHORT(dlen, tp);
			if (!ns_nameok(qp, name, class, NULL, response_trans,
				       ns_ownercontext(type, response_trans),
				       name, from.sin_addr)) {
				formerrmsg = badNameFound;
				goto refused;
			}
			/* skip rest of record */
			if (tp + dlen > eom) {
				formerrmsg = outofDataAuth;
				goto formerr;
			}
			tp += dlen;
			if (type == T_NS) {
				strcpy(aname, name);
				foundns = 1;
			}
			if (type == T_SOA)
				foundsoa = 1;
		}

		/*
		 * If the answer delegates us either to the same level in
		 * the hierarchy or closer to the root, we consider this
		 * server lame.  Note that for now we only log the message
		 * if the T_NS was C_IN, which is technically wrong (NS is
		 * visible in all classes) but necessary anyway (non-IN
		 * classes tend to not have good strong delegation graphs).
		 */

		if (foundns && !foundsoa &&
		    ns_samedomain(qp->q_domain, aname)) {
			if (fwd == NULL) {
				nameserIncr(from.sin_addr, nssRcvdLDel);
				mark_lame(qp, from);
			}
			mark_bad(qp, from);
			if (class == C_IN && fwd == NULL &&
			    !haveComplained(ina_ulong(from.sin_addr),
					    nhash(qp->q_domain))) {
				char *learnt_from = learntFrom(qp, &from);

				ns_info(ns_log_lame_servers,
					"Lame server on '%s' (in '%s'?): %s%s",
					qname, qp->q_domain, 
					sin_ntoa(from),
					(learnt_from == NULL) ? "" :
					learnt_from);
				if (learnt_from != NULL)
					freestr(learnt_from);
			} else if (fwd != NULL) {
				if (!haveComplained(ina_ulong(from.sin_addr),
					  (u_long)nonRecursiveForwarder))
					ns_warning(ns_log_default, "%s: %s",
						   nonRecursiveForwarder,
						   sin_ntoa(from));
			}

			fast_retry(qp, from);
			return;
		}
	}

	/*
	 * Add the info received in the response to the data base.
	 */
	arfirst = ancount + aucount;
	c = arfirst + arcount;

	/* Don't return if it's a TSIG signed truncated message */
	if (has_tsig > 0 && hp->tc)
		goto tcp_retry;

	/* -ve $ing non-existence of record, must handle non-authoritative
	 * NOERRORs with c == 0.
	 */
	if (!hp->aa && !hp->tc && hp->rcode == NOERROR && c == 0)
		goto return_msg;

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

tcp_retry:
		/* retry using tcp provided this was not a tcp query */
		if (!(qp->q_flags & Q_USEVC)) {
			qp->q_flags |= Q_USEVC;
			unsched(qp);
			schedretry(qp, 60);

			nsa = Q_NEXTADDR(qp, 0);

			key = tsig_key_from_addr(nsa->sin_addr);
			if (key != NULL) {
				smsgsize = qp->q_msglen + TSIG_BUF_SIZE;
				smsg = memget(smsgsize);
				smsglen = qp->q_msglen;
				siglen = sizeof(sig);
				memcpy(smsg, qp->q_msg, qp->q_msglen);
				n = ns_sign(smsg, &smsglen, smsgsize,
					    NOERROR, key, NULL, 0,
					    sig, &siglen, 0);
				if (n == 0) {
					oldqbuf = qp->q_msg;
					oldqlen = qp->q_msglen;
					qp->q_msglen = smsglen;
					qp->q_msg = smsg;
					has_tsig = 1;
					qp->q_nstsig = new_tsig(key, sig,
								siglen);
				}
				else {
					has_tsig = 0;
					free_tsig(qp->q_nstsig);
					qp->q_nstsig = NULL;
					INSIST(0);
				}
			}
			else {
				has_tsig = 0;
				free_tsig(qp->q_nstsig);
				qp->q_nstsig = NULL;
			}

			if (tcp_send(qp) != NOERROR)
				/*
				 * We're probably in trouble if tcp_send
				 * failed, but we'll try to press on because
				 * there isn't anything else to do.
				 */
				retry(qp);

			if (has_tsig == 1) {
				memput(qp->q_msg, smsgsize);
				qp->q_msg = oldqbuf;
				qp->q_msglen = oldqlen;
			}
			return;
		} else if (!qsp) {
			/* outstanding udp response */
			return;
		}

		/* XXX truncated tcp response */
		ns_error(ns_log_default,
			 "ns_resp: TCP truncated: \"%s\" %s %s from %s",
			 qname, p_class(qclass), p_type(qtype),
			 sin_ntoa(from));
		/* mark this server as bad */
		mark_bad(qp, from);
		/* try another server, it may have a bigger write buffer */
		retry(qp);
		return;
	}

	tp = cp;

	restart = 0;
	validanswer = 0;
	nscount = 0;
	soacount = 0;
	cname = 0;
	lastwascname = 0;
	externalcname = 0;
	strcpy(aname, qname);

	if (count) {
		/* allocate 1 extra record for end of set detection */
		flushset_size = (count + 1) * sizeof *flushset;
		flushset = memget(flushset_size);
		if (flushset == NULL)
			panic("flushset: out of memory", NULL);
		memset(flushset, 0, flushset_size);
	} else
		flushset = NULL;

	for (i = 0; i < count; i++) {
		struct databuf *dp;
		int type;

		freestr_maybe(&tname);
		if (cp >= eom) {
			free_related_additional();
			if (flushset != NULL)
				free_flushset(flushset, flushset_size);
			formerrmsg = outofDataFinal;
			goto formerr;
		}
		n = rrextract(msg, msglen, cp, &dp, name, sizeof name, from,
			      &tname);
		if (n < 0) {
			free_related_additional();
			freestr_maybe(&tname);
			if (flushset != NULL)
				free_flushset(flushset, flushset_size);
			formerrmsg = outofDataFinal;
			if (hp->rcode == REFUSED)
				goto refused;
			else
				goto formerr;
		}
		cp += n;
		if (!dp)
			continue;
		type = dp->d_type;
		if (i < ancount) {
			/* Answer section. */
			if (externalcname || ns_samename(name, aname) != 1) {
				if (!externalcname)
					ns_info(ns_log_resp_checks,
						"wrong ans. name (%s != %s)",
						name[0] ? name : ".", 
						aname[0] ? aname : ".");
				else
					ns_debug(ns_log_resp_checks, 3,
				 "ignoring answer '%s' after external cname",
						 name);
				db_freedata(dp);
				continue;
			}
			if (type == T_CNAME &&
			    qtype != T_CNAME && qtype != T_ANY) {
				strcpy(aname, (char *)dp->d_data);
				if (!ns_samedomain(aname, qp->q_domain))
					externalcname = 1;
				cname++;
				lastwascname = 1;
			} else {
				validanswer = 1;
				lastwascname = 0;
			}

			if (tname != NULL) {
				add_related_additional(tname);
				tname = NULL;
			}

			dp->d_cred = (hp->aa && ns_samename(name, qname) == 1)
				? DB_C_AUTH
				: DB_C_ANSWER;
		} else {
			/* After answer section. */
			if (lastwascname) {
				ns_debug(ns_log_resp_checks, 3,
				 "last was cname, ignoring auth. and add.");
				db_freedata(dp);
				break;
			}
			if (i < arfirst) {
				/* Authority section. */
				switch (type) {
				case T_NS:
				case T_SOA:
					if (!ns_samedomain(aname, name)) {
						ns_info(ns_log_resp_checks,
					    "bad referral (%s !< %s) from %s",
							aname[0] ? aname : ".",
							name[0] ? name : ".",
							sin_ntoa(from));
						db_freedata(dp);
						continue;
					} else if (fwd == NULL &&
						   !ns_samedomain(name,
							       qp->q_domain)) {
						if (!externalcname)
						    ns_info(ns_log_resp_checks,
					    "bad referral (%s !< %s) from %s",
							 name[0] ? name : ".",
							 qp->q_domain[0] ?
							 qp->q_domain : ".",
							 sin_ntoa(from));
						db_freedata(dp);
						continue;
					}
					if (type == T_NS) {
						nscount++;
						add_related_additional(tname);
						tname = NULL;
					}
					if (type == T_SOA) {
						soacount++;
					}
					break;
				case T_NXT:
					/* XXX check */
					break;
				case T_SIG:
					/* XXX check that it relates to an
					   NS or SOA or NXT */
					break;
				default:
					ns_info(ns_log_resp_checks,
	"invalid RR type '%s' in authority section (name = '%s') from %s",
						p_type(type), name,
						sin_ntoa(from));
					db_freedata(dp);
					continue;
				}
				dp->d_cred = (hp->aa && (cname == 0)) ?
					DB_C_AUTH : (qp->q_flags & Q_PRIMING)
						? DB_C_ANSWER
						: DB_C_ADDITIONAL;
			} else {
				/* Additional section. */
				switch (type) {
				case T_A:
				case T_AAAA:
					if (externalcname ||
					    !ns_samedomain(name, qp->q_domain)) {
						ns_debug(ns_log_resp_checks, 3,
				       "ignoring additional info '%s' type %s",
							 name, p_type(type));
						db_freedata(dp);
						continue;
					}
					if (!related_additional(name)) {
						ns_info(ns_log_resp_checks,
			     "unrelated additional info '%s' type %s from %s",
							 name, p_type(type),
							 sin_ntoa(from));
						db_freedata(dp);
						continue;
					}
					break;
				case T_KEY:
					/* XXX  check? */
					break;
				case T_SIG:
					/*
					 * XXX  a SIG RR should relate
					 * to some other RR in this section,
					 * although if it's the last RR
					 * it might be a transaction signature.
					 */
					break;
				default:
					ns_info(ns_log_resp_checks,
	"invalid RR type '%s' in additional section (name = '%s') from %s",
						p_type(type), name,
						sin_ntoa(from));
					db_freedata(dp);
					continue;
				}
				dp->d_cred = (qp->q_flags & Q_PRIMING)
					? DB_C_ANSWER
					: DB_C_ADDITIONAL;
			}
		}
		rrsetadd(flushset, name, dp);
	}
	free_related_additional();
	freestr_maybe(&tname);
	if (flushset != NULL) {
		if ((qp->q_flags & Q_SYSTEM) && (qp->q_flags & Q_PRIMING)) {
			check_hints(flushset); /* before rrsetupdate */
			rrsetupdate(flushset, dbflags, from, 1);
		} else
			rrsetupdate(flushset, dbflags, from, 0);
		free_flushset(flushset, flushset_size);
 	}
	if (lastwascname && !externalcname)
		ns_debug(ns_log_cname, 3, "%s (%s) q(%s %s %s) %s qd(%s)",
			danglingCname, aname,
			(qname && *qname) ? qname : ".",
			p_class(qclass), p_type(qtype),
			sin_ntoa(from), qp->q_domain);

	if (cp > eom) {
		formerrmsg = outofDataAFinal;
		goto formerr;
	}

	if ((qp->q_flags & Q_SYSTEM) && ancount) {
		if ((qp->q_flags & Q_PRIMING) && !check_root()) {
			/* mark server as bad */
			mark_bad(qp, from);
			fast_retry(qp, from);
			return;
		}
		ns_debug(ns_log_default, 3,
			 "resp: leaving, SYSQUERY ancount %d", ancount);
#ifdef BIND_NOTIFY
		if (qp->q_notifyzone != DB_Z_CACHE) {
			struct zoneinfo *zp = &zones[qp->q_notifyzone];

			qp->q_notifyzone = DB_Z_CACHE;
			ns_notify(zp->z_origin, zp->z_class, ns_t_soa);
		}
#endif
		qremove(qp);
		return;
	}

	if (ancount && count && !validanswer) {
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
	}

	if (!restart && !qp->q_cmsglen && ancount > 1 && qtype == T_A)
		sort_response(tp, eom, ancount, &qp->q_from);

	/*
	 * An answer to a T_ANY query or a successful answer to a
	 * regular query with no indirection, then just return answer.
	 */
	if (!restart && ancount && (qtype == T_ANY || !qp->q_cmsglen)) {
		ns_debug(ns_log_default, 3,
			 "resp: got as much answer as there is");
		goto return_msg;
	}

	/*
	 * We might want to cache this negative answer.
	 *
	 * if ancount != 0 and rcode == NOERROR we cannot determine if the
	 * CNAME chain has been processed to completion or not, so just
	 * restart the query. DNS needs a NODATA return code!
	 *
	 * As some servers incorrectly return a NODATA indication when
	 * there is a CNAME chain instead of NXDOMAIN, we requery to get
	 * a definitive answer.
	 */
	if ((hp->rcode == NXDOMAIN && cname == ancount) ||
	    (hp->rcode == NOERROR && ancount == 0 && 
	     (nscount == 0 || soacount != 0)
	     )
	    )
	{
		cache_n_resp(msg, msglen, from, qp->q_name,
			     qp->q_class, qp->q_type);

		if (!qp->q_cmsglen) {
			ns_debug(ns_log_default, 3,
				 "resp: leaving NO: auth = %d", hp->aa);
			goto return_msg;
		}
		forcecmsg = 1;
	}

	/*
	 * All messages in here need further processing.  i.e. they
	 * are either CNAMEs or we got referred again.
	 */
	count = 0;
	founddata = 0;
	dname = name;
	/*
	 * XXX - the restart stuff doesn't work if any of the answer RRs
	 * is not cacheable (TTL==0 or unknown RR type), since all of the
	 * answer must pass through the cache and be re-assembled.
	 */
	if (qp->q_cmsglen != 0) {
		ns_debug(ns_log_default, 1, "Cname second pass");
		newmsglen = MIN(PACKETSZ, qp->q_cmsglen);
		memcpy(newmsg, qp->q_cmsg, newmsglen);
	} else {
		newmsglen = MIN(PACKETSZ, msglen);
		memcpy(newmsg, msg, newmsglen);
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
	n = dn_expand(newmsg, newmsg + newmsglen, cp, dname, sizeof name);
	if (n < 0) {
		ns_debug(ns_log_default, 1, "dn_expand failed");
		goto servfail;
	}
	if (!res_dnok(dname)) {
		ns_debug(ns_log_default, 1, "bad name (%s)", dname);
		goto servfail;
	}
	cp += n + QFIXEDSZ;
	buflen = sizeof(newmsg) - (cp - newmsg);

	cname = 0;

 try_again:
	ns_debug(ns_log_default, 1, "resp: nlookup(%s) qtype=%d", dname,
		 qtype);
	foundname = 0;
	fname = "";
	htp = hashtab;		/* lookup relative to root */
	np = nlookup(dname, &htp, &fname, 0);
	ns_debug(ns_log_default, 1, "resp: %s '%s' as '%s' (cname=%d)",
		 np == NULL ? "missed" : "found", dname, fname, cname);
	if (np == NULL || fname != dname)
		goto fetch_ns;

	foundname++;
	answers = cp;
	count = cp - newmsg;
	/*
	 * Look for NXDOMAIN record.
	 */
	for (dp = np->n_data; dp; dp = dp->d_next) {
		if (!stale(dp) && (dp->d_rcode == NXDOMAIN) &&
		    (dp->d_class == (int)qclass)) {
#ifdef RETURNSOA
			n = finddata(np, qclass, T_SOA, hp, &dname,
				     &buflen, &count);
			if ( n != 0) {
				if (count) {
					cp += n;
					buflen -= n;
					newmsglen += n;
					hp->nscount = htons((u_int16_t)count);
				}
				if (hp->rcode == NOERROR_NODATA) {
					hp->rcode = NOERROR;
					goto return_newmsg;
				}
			}
#else
			count = 0;
#endif
			hp->rcode = NXDOMAIN;
			/* 
			 * XXX forcing AA all the time isn't right, but
			 * we have to work that way by default
			 * for compatibility with older servers.
			 */
			if (!NS_OPTION_P(OPTION_NONAUTH_NXDOMAIN))
				hp->aa = 1;
			ns_debug(ns_log_default, 3, "resp: NXDOMAIN aa = %d",
				 hp->aa);
			if ((count == 0) || NS_OPTION_P(OPTION_NORFC2308_TYPE1))
				goto return_newmsg;
			founddata = 1;
			goto fetch_ns;
		}
	}
	n = finddata(np, qclass, qtype, hp, &dname, &buflen, &count);
	if (n == 0)
		goto fetch_ns;		/* NO data available */
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
		if ((count == 0) || NS_OPTION_P(OPTION_NORFC2308_TYPE1))
			goto return_newmsg;
		founddata = 1;
		goto fetch_ns;
	}
	cp += n;
	buflen -= n;
	hp->ancount = htons(ntohs(hp->ancount) + (u_int16_t)count);
	if (fname != dname && qtype != T_CNAME && qtype != T_ANY) {
		cname++;
		goto try_again;
	}
	founddata = 1;

	ns_debug(ns_log_default, 3,
		 "resp: foundname=%d, count=%d, founddata=%d, cname=%d",
		 foundname, count, founddata, cname);

	if (count > 1 && qtype == T_A)
		sort_response(answers, cp, count, &qp->q_from);

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
		ns_debug(ns_log_default, 3, "req: leaving (%s, rcode %d)",
			 dname, hp->rcode);
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
		hp->nscount = htons((u_int16_t)count + ntohs(hp->nscount));
		goto return_newmsg;
	}

	/*
	 *  If we get here, we don't have the answer yet and are about
	 *  to iterate to try and get it.  First, infinite loop avoidance.
	 */
	if (qp->q_nqueries++ > MAXQUERIES) {
		ns_debug(ns_log_default, 1,
			 "resp: MAXQUERIES exceeded (%s %s %s)",
			 dname, p_class(qclass), p_type(qtype));
		ns_info(ns_log_default,
		   "MAXQUERIES exceeded, possible data loop in resolving (%s)",
			dname);
		goto servfail;
	}

	/* Reset the query control structure */

	ns_freeqns(qp, "ns_resp");
	qp->q_naddr = 0;
	qp->q_curaddr = 0;
	nsfwdadd(qp, NS_ZFWDTAB(qp->q_fzone));

	if (qp->q_domain != NULL)
		freestr(qp->q_domain);
	getname(np, tmpdomain, sizeof tmpdomain);
	qp->q_domain = savestr(tmpdomain, 1);

	if (NS_ZOPTION_P(qp->q_fzone, OPTION_FORWARD_ONLY))
		n = 0;
	else if ((n = nslookup(nsp, qp, dname, "ns_resp")) <= 0) {
		if (n < 0) {
			if (n == -1)
				ns_debug(ns_log_default, 3,
					 "resp: nslookup reports danger");
			if (cname) /* a remote CNAME that does not have data */
				goto return_newmsg;
			goto servfail;
		} else {
			ns_debug(ns_log_default, 3,
				 "resp: no addrs found for NS's");
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
	qp->q_addr[0].stime = tt;
	if (cname) {
	 	if (qp->q_cname++ == MAXCNAMES) {
			ns_debug(ns_log_default, 3,
				 "resp: leaving, MAXCNAMES exceeded");
			goto servfail;
	 	}
		ns_debug(ns_log_default, 1, "q_cname = %d", qp->q_cname);
		ns_debug(ns_log_default, 3,
			 "resp: building recursive query; nslookup");
		if (qp->q_cmsg == NULL) {
			qp->q_cmsg = qp->q_msg;
			qp->q_cmsglen = qp->q_msglen;
			qp->q_cmsgsize = qp->q_msgsize;
		} else if (qp->q_msg != NULL)
			memput(qp->q_msg, qp->q_msgsize);
		qp->q_msg = (u_char *)memget(PACKETSZ);
		if (qp->q_msg == NULL) {
			ns_notice(ns_log_default, "resp: memget error");
			goto servfail;
		}
		qp->q_msgsize = PACKETSZ;
		n = res_nmkquery(&res, QUERY, dname, qclass, qtype,
				 NULL, 0, NULL, qp->q_msg, PACKETSZ);
		if (n < 0) {
			ns_info(ns_log_default, "resp: res_mkquery(%s) failed",
				dname);
			goto servfail;
		}
		if (qp->q_name != NULL)
			freestr(qp->q_name);
		qp->q_name = savestr(dname, 1);
		qp->q_msglen = n;
		hp = (HEADER *) qp->q_msg;
		hp->rd = 0;
	} else
		hp = (HEADER *) qp->q_msg;
	hp->id = qp->q_nsid = htons(nsid_next());
	hp->rd = (qp->q_addr[0].forwarder ? 1 : 0);
	unsched(qp);
	schedretry(qp, retrytime(qp));
	nsa = Q_NEXTADDR(qp, 0);
	if (ns_wouldlog(ns_log_default,1)) {
	    ns_debug(ns_log_default, 1,
		     "resp: forw -> %s ds=%d nsid=%d id=%d %dms",
		     sin_ntoa(*nsa), ds,
		     ntohs(qp->q_nsid), ntohs(qp->q_id),
		     (qp->q_addr[0].nsdata != NULL)
			    ? qp->q_addr[0].nsdata->d_nstime
			    : -1);
	}
#ifdef DEBUG
	if (debug >= 10)
		res_pquery(&res, qp->q_msg, qp->q_msglen,
			   log_get_stream(packet_channel));
#endif
	key = tsig_key_from_addr(nsa->sin_addr);
	if (key != NULL) {
		smsgsize = qp->q_msglen + TSIG_BUF_SIZE;
		smsg = memget(smsgsize);
		smsglen = qp->q_msglen;
		siglen = sizeof(sig);
		memcpy(smsg, qp->q_msg, qp->q_msglen);
		n = ns_sign(smsg, &smsglen, smsgsize, NOERROR, key, NULL, 0,
			    sig, &siglen, 0);
		if (n == 0) {
			oldqbuf = qp->q_msg;
			oldqlen = qp->q_msglen;
			qp->q_msglen = smsglen;
			qp->q_msg = smsg;
			has_tsig = 1;
			qp->q_nstsig = new_tsig(key, sig, siglen);
		}
		else {
			has_tsig = 0;
			free_tsig(qp->q_nstsig);
			qp->q_nstsig = NULL;
			INSIST(0);
		}
	}
	else {
		has_tsig = 0;
		free_tsig(qp->q_nstsig);
		qp->q_nstsig = NULL;
	}

	if (qp->q_flags & Q_USEVC) {
		if (tcp_send(qp) != NOERROR) {
			if (!haveComplained(ina_ulong(nsa->sin_addr),
					    (u_long)tcpsendStr))
				ns_info(ns_log_default,
					"ns_forw: tcp_send(%s) failed: %s",
					sin_ntoa(*nsa), strerror(errno));
		}
	} else if (sendto(ds, (char*)qp->q_msg, qp->q_msglen, 0,
		   (struct sockaddr *)nsa,
		   sizeof(struct sockaddr_in)) < 0)
	{
		sendto_errno = errno;
		if (!haveComplained(ina_ulong(nsa->sin_addr),
				    (u_long)sendtoStr))
			ns_info(ns_log_default, "ns_resp: sendto(%s): %s",
				sin_ntoa(*nsa), strerror(errno));
		nameserIncr(nsa->sin_addr, nssSendtoErr);
	}
	if (has_tsig == 1) {
		memput(qp->q_msg, smsgsize);
		qp->q_msg = oldqbuf;
		qp->q_msglen = oldqlen;
	}
	hp->rd = 0;	/* leave set to 0 for dup detection */
	nameserIncr(nsa->sin_addr, nssSentFwdR);
	nameserIncr(qp->q_from.sin_addr, nssRcvdFwdR);
	ns_debug(ns_log_default, 3, "resp: Query sent.");
	free_nsp(nsp);
	switch (sendto_errno) {
	case ENETDOWN:
	case ENETUNREACH:
	case EHOSTDOWN:
	case EHOSTUNREACH:
		unsched(qp);
		schedretry(qp, (time_t) 0);
	}
	return;

 formerr:
	if (!haveComplained(ina_ulong(from.sin_addr), (u_long)formerrmsg))
		ns_info(ns_log_resp_checks, "Malformed response from %s (%s)",
			sin_ntoa(from), formerrmsg);
	fast_retry(qp, from);
	free_nsp(nsp);
	return;

 return_msg:
	nameserIncr(from.sin_addr, nssRcvdFwdR);
	nameserIncr(qp->q_from.sin_addr, nssSentFwdR);
	nameserIncr(qp->q_from.sin_addr, nssSentAns);
	if (!hp->aa)
		nameserIncr(qp->q_from.sin_addr, nssSentNaAns);
	if (hp->rcode == NXDOMAIN)
		nameserIncr(qp->q_from.sin_addr, nssSentNXD);
	/* The "standard" return code */
	hp->qr = 1;
	hp->id = qp->q_id;
	hp->rd = 1;
	hp->ra = (NS_OPTION_P(OPTION_NORECURSE) == 0);
	(void) send_msg(msg, msglen, qp);
	qremove(qp);
	free_nsp(nsp);
	return;

 return_newmsg:
	nameserIncr(qp->q_from.sin_addr, nssSentAns);

	if (!hp->aa)
		nameserIncr(qp->q_from.sin_addr, nssSentNaAns);
	if (hp->rcode == NXDOMAIN) 
		nameserIncr(qp->q_from.sin_addr, nssSentNXD);
	n = doaddinfo(hp, cp, buflen);
	cp += n;
	buflen -= n;
	hp->qr = 1;
	hp->id = qp->q_id;
	hp->rd = 1;
	hp->ra = (NS_OPTION_P(OPTION_NORECURSE) == 0);
	(void) send_msg(newmsg, cp - newmsg, qp);
	qremove(qp);
	free_nsp(nsp);
	return;

 refused:
	hp = (HEADER *)(qp->q_cmsglen ? qp->q_cmsg : qp->q_msg);
	hp->rcode = REFUSED;
	hp->qr = 1;
	hp->id = qp->q_id;
	hp->rd = 1;
	hp->ra = (NS_OPTION_P(OPTION_NORECURSE) == 0);
	(void) send_msg((u_char *)hp,
			(qp->q_cmsglen ? qp->q_cmsglen : qp->q_msglen),
			qp);
	qremove(qp);
	free_nsp(nsp);
	return;
	
 servfail:
	nameserIncr(qp->q_from.sin_addr, nssSentFail);
	hp = (HEADER *)(qp->q_cmsglen ? qp->q_cmsg : qp->q_msg);
	hp->rcode = SERVFAIL;
	hp->qr = 1;
	hp->id = qp->q_id;
	hp->rd = 1;
	hp->ra = (NS_OPTION_P(OPTION_NORECURSE) == 0);
	(void) send_msg((u_char *)hp,
			(qp->q_cmsglen ? qp->q_cmsglen : qp->q_msglen),
			qp);
	qremove(qp);
	free_nsp(nsp);
	return;

 timeout:
	if (qp->q_stream)
		sq_remove(qp->q_stream);
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
rrextract(u_char *msg, int msglen, u_char *rrp, struct databuf **dpp,
	  char *dname, int namelen, struct sockaddr_in from, char **tnamep)
{
	u_char *cp, *eom, *rdatap;
	u_int class, type, dlen;
	int n, n1, n2;
	u_int32_t ttl;
	u_char *cp1, data[MAXDATA*2];
	HEADER *hp = (HEADER *)msg;
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
	if (class > CLASS_MAX) {
		ns_debug(ns_log_default, 3, "bad class in rrextract");
		hp->rcode = FORMERR;
		return (-1);
	}
	GETLONG(ttl, cp);
	if (ttl > MAXIMUM_TTL) {
		ns_debug(ns_log_default, 5, "%s: converted TTL > %u to 0",
			 dname, MAXIMUM_TTL);
		ttl = 0;
	}
	GETSHORT(dlen, cp);
	BOUNDS_CHECK(cp, dlen);
	rdatap = cp;
	if (!ns_nameok(NULL, dname, class, NULL, response_trans,
		       ns_ownercontext(type, response_trans),
		       dname, from.sin_addr)) {
		hp->rcode = REFUSED;
		return (-1);
	}
	ns_debug(ns_log_default, 3,
		 "rrextract: dname %s type %d class %d ttl %d",
		 dname, type, class, ttl);
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
	case T_TXT:
	case T_X25:
	case T_ISDN:
	case T_NSAP:
	case T_AAAA:
	case T_LOC:
	case T_KEY:
	case ns_t_cert:
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
		if (!ns_nameok(NULL, (char *)data, class, NULL, response_trans,
			       type == T_PTR ?ns_ptrcontext(dname) :domain_ctx,
			       dname, from.sin_addr)) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 = data;
		n = strlen((char *)data) + 1;
		if (tnamep != NULL && (type == T_NS || type == T_MB))
			*tnamep = savestr((char *)cp1, 1);
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
		if (!ns_nameok(NULL, (char *)data, class, NULL, response_trans,
			       context, dname, from.sin_addr)) {
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
		if (!ns_nameok(NULL, (char *)cp1, class, NULL, response_trans,
			       context, dname, from.sin_addr)) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 += strlen((char *)cp1) + 1;
		if (type == T_SOA) {
			n = 5 * INT32SZ;
			BOUNDS_CHECK(cp, n);
			memcpy(cp1, cp, n);
			cp += n;
			cp1 += n;
		}
		n = cp1 - data;
		cp1 = data;
		break;

	case T_NAPTR:
		/* Grab weight and port. */
		BOUNDS_CHECK(cp, INT16SZ*2);
		memcpy(data, cp, INT16SZ*2);
		cp1 = data + INT16SZ*2;
		cp += INT16SZ*2;

		/* Flags */
		BOUNDS_CHECK(cp, 1);
		n = *cp++;
		BOUNDS_CHECK(cp, n);
		*cp1++ = n;
		memcpy(cp1, cp, n);
		cp += n; cp1 += n;

		/* Service */
		BOUNDS_CHECK(cp, 1);
		n = *cp++;
		BOUNDS_CHECK(cp, n);
		*cp1++ = n;
		memcpy(cp1, cp, n);
		cp += n; cp1 += n;

		/* Regexp */
		BOUNDS_CHECK(cp, 1);
		n = *cp++;
		BOUNDS_CHECK(cp, n);
		*cp1++ = n;
		memcpy(cp1, cp, n);
		cp += n; cp1 += n;

		/* Replacement */
		n = dn_expand(msg, eom, cp, (char *)cp1,
			      sizeof data - (cp1 - data));
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		if (!ns_nameok(NULL, (char *)cp1, class, NULL, response_trans,
			       hostname_ctx, dname, from.sin_addr)) {
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
		memcpy(data, cp, INT16SZ);
		cp1 = data + INT16SZ;
		cp += INT16SZ;

		if (type == T_SRV) {
			/* Grab weight and port. */
			BOUNDS_CHECK(cp, INT16SZ*2);
			memcpy(cp1, cp, INT16SZ*2);
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
		if (!ns_nameok(NULL, (char *)cp1, class, NULL, response_trans,
			       hostname_ctx, dname, from.sin_addr)) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;

		if (tnamep != NULL)
			*tnamep = savestr((char *)cp1, 1);

		/* compute end of data */
		cp1 += strlen((char *)cp1) + 1;
		/* compute size of data */
		n = cp1 - data;
		cp1 = data;
		break;

	case T_PX:
		/* grab preference */
		BOUNDS_CHECK(cp, INT16SZ);
		memcpy(data, cp, INT16SZ);
		cp1 = data + INT16SZ;
		cp += INT16SZ;

		/* get MAP822 name */
		n = dn_expand(msg, eom, cp, (char *)cp1,
			      sizeof data - INT16SZ);
		if (n < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		if (!ns_nameok(NULL, (char *)cp1, class, NULL, response_trans,
			       domain_ctx, dname, from.sin_addr)) {
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
		if (!ns_nameok(NULL, (char *)cp1, class, NULL, response_trans,
			       domain_ctx, dname, from.sin_addr)) {
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
		u_int8_t alg;

		/* Check signature time, expiration, and adjust TTL.  */
		/* This code is similar to that in db_load.c.  */

		/* Skip coveredType, save alg, skip labels */
		BOUNDS_CHECK(cp, INT16SZ + 1 + 1 + 3*INT32SZ);
		cp1 = cp + INT16SZ;
		alg = *cp1++;
		cp1++;
		GETLONG(origTTL, cp1);
		GETLONG(exptime, cp1);
		GETLONG(signtime, cp1);
		now = time(NULL);	/* Get current time in GMT/UTC */

		/* Don't let bogus name servers increase the signed TTL */
		if (ttl > origTTL) {
			ns_debug(ns_log_default, 3,
				 "shrinking SIG TTL from %d to origTTL %d",
				 ttl, origTTL);
			ttl = origTTL;
		}

		/* Don't let bogus signers "sign" in the future.  */
		if (signtime > now) {
			ns_debug(ns_log_default, 3,
			  "ignoring SIG: signature date %s is in the future",
				 p_secstodate (signtime));
			return ((cp - rrp) + dlen);
		}
		
		/* Ignore received SIG RR's that are already expired.  */
		if (exptime <= now) {
			ns_debug(ns_log_default, 3,
				"ignoring SIG: expiration %s is in the past",
				 p_secstodate (exptime));
			return ((cp - rrp) + dlen);
		}

		/* Lop off the TTL at the expiration time.  */
		timetilexp = exptime - now;
		if (timetilexp < ttl) {
			ns_debug(ns_log_default, 3,
				 "shrinking expiring %s SIG TTL from %d to %d",
				 p_secstodate (exptime), ttl, timetilexp);
			ttl = timetilexp;
		}

		/* The following code is copied from named-xfer.c.  */
		cp1 = (u_char *)data;

		/* first just copy over the type_covered, algorithm, */
		/* labels, orig ttl, two timestamps, and the footprint */
		BOUNDS_CHECK(cp, 18);
		memcpy(cp1, cp, 18);
		cp  += 18;
		cp1 += 18;

		/* then the signer's name */
		n = dn_expand(msg, eom, cp, (char *)cp1, (sizeof data) - 18);
		if (n < 0 || n + NS_SIG_SIGNER > dlen) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 += strlen((char*)cp1)+1;

		/* finally, we copy over the variable-length signature.
		   Its size is the total data length, minus what we copied. */
		n = dlen - (NS_SIG_SIGNER + n);

		if (n > (sizeof data) - (cp1 - (u_char *)data)) {
			hp->rcode = FORMERR;
			return (-1);  /* out of room! */
		}

		switch (alg) {
		    case NS_ALG_MD5RSA:
			if (n < NS_MD5RSA_MIN_SIZE || n > NS_MD5RSA_MAX_SIZE)
				hp->rcode = FORMERR;
			break;

		    case NS_ALG_DSA:
			if (n != NS_DSA_SIG_SIZE)
				hp->rcode = FORMERR;
			break;

		    default:
			break;
		}

		if (hp->rcode == FORMERR)
			return (-1);

		memcpy(cp1, cp, n);
		cp += n;
		cp1 += n;
		
		/* compute size of data */
		n = cp1 - (u_char *)data;
		cp1 = (u_char *)data;
		break;
	    }

	case T_NXT:
		n = dn_expand(msg, eom, cp, (char *)data, sizeof data);
		/*
		 * By testing if n >= dlen, we are requiring that the type
		 * bitmap be at least one octet.  This is reasonable
		 * because we always have to look at the 0 bit to see if
		 * this is a "different format" NXT or not.
		 */
		if (n < 0 || n >= dlen) {
			hp->rcode = FORMERR;
			return (-1);
		}
		if (!ns_nameok(NULL, (char *)data, class, NULL, response_trans,
			       domain_ctx, dname, from.sin_addr)) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		n1 = strlen((char *)data) + 1;
		cp1 = data + n1;
		/*
		 * We don't need to BOUNDS_CHECK() cp here because we've
		 * previously checked that 'dlen' bytes are in bounds, and
		 * we know that n < dlen.
		 */
		n2 = dlen - n;
		/*
		 * The first bit of the first octet determines the format
		 * of the NXT record.  A format for types >= 128 has not
		 * yet been defined, so if bit zero is set, we just copy
		 * what's there because we don't understand it.
		 */
		if ((*cp & 0x80) == 0) {
			/*
			 * Bit zero is not set; this is an ordinary NXT
			 * record.  The bitmap must be at least 4 octets
			 * because the NXT bit should be set.  It should be
			 * less than or equal to 16 octets because this NXT
			 * format is only defined for types < 128.
			 */
			if (n2 < 4 || n2 > 16) {
				hp->rcode = FORMERR;
				return (-1);
			}
		}
		if (n2 > sizeof data - n1) {
			hp->rcode = FORMERR;
			return (-1);
		}
		memcpy(cp1, cp, n2);
		cp += n2;
		cp1 += n2;

		/* compute size of data */
		n = cp1 - (u_char *)data;
		cp1 = (u_char *)data;
		break;
	
	default:
		ns_debug(ns_log_default, 3, "unknown type %d", type);
		return ((cp - rrp) + dlen);
	}

	if (cp > eom) {
		hp->rcode = FORMERR;
		return (-1);
	}
	if ((u_int)(cp - rdatap) != dlen) {
		ns_debug(ns_log_default, 3,
		     "encoded rdata length is %u, but actual length was %u",
			 dlen, (u_int)(cp - rdatap));
		hp->rcode = FORMERR;
		return (-1);
	}
	if (n > MAXDATA) {
		ns_debug(ns_log_default, 1,
			 "update type %d: %d bytes is too much data",
			 type, n);
		hp->rcode = FORMERR;
		return (-1);
	}

	ttl += tt.tv_sec;
	*dpp = savedata(class, type, ttl, cp1, n);
	return (cp - rrp);
}

int
send_msg(u_char *msg, int msglen, struct qinfo *qp) {
	HEADER *hp = (HEADER *) msg;
	u_char *oldmsg;
	int oldlen;
	int msgsize;
	int ret;

	if (qp->q_flags & Q_SYSTEM)
		return (1);
	if (!qp->q_stream && (msglen > PACKETSZ))
		msglen = trunc_adjust(msg, msglen, PACKETSZ);
	if (ns_wouldlog(ns_log_default, 1)) {
		ns_debug(ns_log_default, 1, "send_msg -> %s (%s %d) id=%d",
			 sin_ntoa(qp->q_from), 
			 qp->q_stream == NULL ? "UDP" : "TCP",
			 qp->q_stream == NULL ? qp->q_dfd : qp->q_stream->s_rfd,
			 ntohs(qp->q_id));
	}
#ifdef DEBUG
	if (ns_wouldlog(ns_log_default, 4)) {
		struct qinfo *tqp;

		for (tqp = nsqhead; tqp != NULL; tqp = tqp->q_link) {
			ns_debug(ns_log_default, 4,
				 "qp %#lx q_id: %d  q_nsid: %d q_msglen: %d",
				 (u_long)tqp, tqp->q_id,
				 tqp->q_nsid, tqp->q_msglen);
			ns_debug(ns_log_default, 4,
				 "\tq_naddr: %d q_curaddr: %d",
				 tqp->q_naddr, tqp->q_curaddr);
			ns_debug(ns_log_default, 4,
				 "\tq_next: %#lx q_link: %#lx",
				 (u_long)qp->q_next, (u_long)qp->q_link);
		}
	}
	if (debug >= 6)
		res_pquery(&res, msg, msglen, log_get_stream(packet_channel));
#endif /* DEBUG */

	if (qp->q_tsig != NULL) {
		u_char sig[TSIG_SIG_SIZE];
		int siglen = sizeof(sig);

		oldmsg = msg;
		oldlen = msglen;

		msgsize = msglen + TSIG_BUF_SIZE;
		msg = memget(msgsize);
		memcpy(msg, oldmsg, oldlen);
 
		ret = ns_sign(msg, &msglen, msgsize, NOERROR, qp->q_tsig->key,
			      qp->q_tsig->sig, qp->q_tsig->siglen,
			      sig, &siglen, 0);

		if (ret != 0) {
			INSIST(0);
		}
	}
	
	if (qp->q_stream == NULL) {
		/*
		 * Don't send FORMERR to these well known ports
		 * (loop avoidance).
		 */
		switch (ntohs(qp->q_from.sin_port)) {
		case 7: /* echo */
		case 13: /* daytime */
		case 19: /* chargen */
		case 37: /* time */
			if (hp->rcode == FORMERR)
				return (-1);
		default:
			break;
		}
		if (sendto(qp->q_dfd, (char*)msg, msglen, 0,
			   (struct sockaddr *)&qp->q_from,
			   sizeof(qp->q_from)) < 0) {
			if (!haveComplained(ina_ulong(qp->q_from.sin_addr),
					    (u_long)sendtoStr))
#if defined(SPURIOUS_ECONNREFUSED)
                           if (errno != ECONNREFUSED)
#endif
				ns_info(ns_log_default,
					"send_msg: sendto(%s): %s",
					sin_ntoa(qp->q_from),
					strerror(errno));
			nameserIncr(qp->q_from.sin_addr, nssSendtoErr);
			return (1);
		}
	} else
		writestream(qp->q_stream, (u_char*)msg, msglen);

	if (qp->q_tsig != NULL) 
		memput(msg, oldlen + TSIG_BUF_SIZE);

	return (0);
}

static int
root_server_p(ns_class class) {
	struct zoneinfo *zp = find_zone("", class);

	return (zp != NULL &&
		(zp->z_type == z_master || zp->z_type == z_slave));
}

void
prime_cache(void) {
	int root = root_server_p(ns_c_in);

	ns_debug(ns_log_default, 1, "prime_cache: priming = %d, root = %d",
		 priming, root);
	if (!priming && !root) {
		struct qinfo *qp = sysquery("", ns_c_in, ns_t_ns,
					    NULL, 0, ns_port, ns_o_query);

		if (qp != NULL) {
			qp->q_flags |= (Q_SYSTEM | Q_PRIMING);
			priming++;
		}
	}
	needs_prime_cache = 0;
}

struct qinfo *
sysquery(const char *dname, int class, int type,
	 struct in_addr *nss, int nsc, u_int16_t port, int opcode)
{
	struct qinfo *qp, *oqp;
	HEADER *hp;
	char tmpdomain[MAXDNAME];
	struct namebuf *np = NULL;
	struct databuf *nsp[NSMAX];
	struct hashbuf *htp1;
	struct hashbuf *htp2;
	struct hashbuf *htp3;
	struct sockaddr_in *nsa;
	const char *fname;
	int n, count;
	int sendto_errno = 0;
	u_char *oldqbuf;
	int oldqlen, has_tsig;
	u_char *smsg;
	int smsglen, smsgsize, siglen;
	u_char sig[TSIG_SIG_SIZE];
	DST_KEY *key;

	nsp[0] = NULL;
	ns_debug(ns_log_default, 3, "sysquery(%s, %d, %d, %#x, %d, %d)",
		 dname, class, type, nss, nsc, ntohs(port));
	qp = qnew(dname, class, type, (nss != NULL && nsc != 0) ? 0 : 1);

	if (nss != NULL && nsc != 0)
		np = NULL;
	else if (!NS_ZOPTION_P(qp->q_fzone, OPTION_FORWARD_ONLY)) {
		htp1 = hashtab;
		htp2 = hashtab;
		htp3 = fcachetab;
		if (priming && dname[0] == '\0') {
			np = NULL;
		} else if (((np = nlookup(dname, &htp1, &fname, 0)) == NULL) &&
			   ((np = nlookup("", &htp2, &fname, 0)) == NULL) &&
			   ((np = nlookup("", &htp3, &fname, 0)) == NULL)) {
			ns_info(ns_log_default,
				"sysquery: nlookup error on %s?",
				dname);
 err1:
			ns_freeqry(qp);
			return (NULL);
		}

		n = findns(&np, class, nsp, &count, 0);
		switch (n) {
		case NXDOMAIN:
		case SERVFAIL:
			ns_info(ns_log_default,
				"sysquery: findns error (%s) on %s?",
				n == NXDOMAIN ? "NXDOMAIN" : "SERVFAIL",
				dname);
 err2:
			free_nsp(nsp);
			goto err1;
		}
	}

	/* Build new qinfo struct. */
	qp->q_cmsg = qp->q_msg = NULL;
	qp->q_dfd = ds;
	if (nss == NULL || nsc == 0)
		nsfwdadd(qp, NS_ZFWDTAB(qp->q_fzone));
	qp->q_expire = tt.tv_sec + RETRY_TIMEOUT*2;
	qp->q_flags |= Q_SYSTEM;

	getname(np, tmpdomain, sizeof tmpdomain);
	qp->q_domain = savestr(tmpdomain, 1);

	if ((qp->q_msg = (u_char *)memget(PACKETSZ)) == NULL) {
		ns_notice(ns_log_default, "sysquery: memget failed");
		goto err2;
	}
	qp->q_msgsize = PACKETSZ;
	n = res_nmkquery(&res, opcode, dname, class,
			 type, NULL, 0, NULL,
			 qp->q_msg, PACKETSZ);
	if (n < 0) {
		ns_info(ns_log_default,
			"sysquery: res_mkquery(%s) failed", dname);
		goto err2;
	}
	qp->q_msglen = n;
	hp = (HEADER *) qp->q_msg;
	hp->id = qp->q_nsid = htons(nsid_next());
	hp->rd = (qp->q_addr[qp->q_curaddr].forwarder ? 1 : 0);
	hp->aa = (opcode == NS_NOTIFY_OP);

	/* First check for an already pending query for this data. */
	for (oqp = nsqhead; oqp != NULL; oqp = oqp->q_link) {
		if ((oqp != qp)
		    && (oqp->q_msglen == qp->q_msglen)
		    && memcmp(oqp->q_msg+2, qp->q_msg + 2,
			      qp->q_msglen - 2) == 0
		    ) {
#ifdef BIND_NOTIFY
			/* XXX - need fancier test to suppress duplicate
			 *       NOTIFYs to the same server (compare nss?)
			 */
			if (opcode != NS_NOTIFY_OP)
#endif /*BIND_NOTIFY*/
			{
			    ns_debug(ns_log_default, 3,
				     "sysquery: duplicate");
			    goto err2;
			}
		}
	}

	if (nss != NULL && nsc != 0) {
		int i;
		struct qserv *qs;

		for (i = 0, qs = qp->q_addr; i < nsc; i++, qs++) {
			qs->ns_addr.sin_family = AF_INET;
			qs->ns_addr.sin_addr = nss[i];
			qs->ns_addr.sin_port = port;
			qs->ns = NULL;
			qs->nsdata = NULL;
			qs->stime = tt;
			qs->forwarder = 0;
			qs->nretry = 0;
		}
		qp->q_naddr = nsc;
	} else if (!NS_ZOPTION_P(qp->q_fzone, OPTION_FORWARD_ONLY)) {
 fetch_a:
		count = nslookup(nsp, qp, dname, "sysquery");
		if (count <= 0) {
			if (count < 0) {
				if (n == -1)
					ns_info(ns_log_default,
				      "sysquery: nslookup reports danger (%s)",
						dname);
				goto err2;
			} else if (np && NAME(*np)[0] == '\0') {
				/*
				 * It's not too serious if we don't have
				 * the root server addresses if we have to
				 * go through a forwarder anyway.  Don't
				 * bother to log it, since prime_cache()
				 * won't do anything about it as currently
				 * implemented.
				 *
				 * XXX - should we skip setting
				 *       needs_prime_cache as well?
				 *
				 * XXX - what happens when we implement
				 *       selective forwarding?
				 */
				if (!NS_OPTION_P(OPTION_FORWARD_ONLY))
					ns_warning(ns_log_default,
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
					ns_info(ns_log_default,
					  "sysquery: findns error (%d) on %s?",
						n, dname);
					goto err2;
				}
				getname(np, tmpdomain, sizeof tmpdomain);
				if (qp->q_domain != NULL)
					freestr(qp->q_domain);
				qp->q_domain = savestr(tmpdomain, 1);
				goto fetch_a;
			}
			goto err2;
		}
	}

	schedretry(qp, retrytime(qp));
	qp->q_addr[0].stime = tt;	/* XXX - why not every? */
	nsa = Q_NEXTADDR(qp, 0);

	ns_debug(ns_log_default, 1,
		 "sysquery: send -> %s dfd=%d nsid=%d id=%d retry=%ld",
		 sin_ntoa(*nsa), qp->q_dfd, 
		 ntohs(qp->q_nsid), ntohs(qp->q_id),
		 (long)qp->q_time);
#ifdef DEBUG
	if (debug >= 10)
		res_pquery(&res, qp->q_msg, qp->q_msglen,
			   log_get_stream(packet_channel));
#endif

	key = tsig_key_from_addr(nsa->sin_addr);
	if (key != NULL) {
		smsgsize = qp->q_msglen + TSIG_BUF_SIZE;
		smsg = memget(smsgsize);
		smsglen = qp->q_msglen;
		siglen = sizeof(sig);
		memcpy(smsg, qp->q_msg, qp->q_msglen);
		n = ns_sign(smsg, &smsglen, smsgsize, NOERROR, key, NULL, 0,
			    sig, &siglen, 0);
		if (n == 0) {
			oldqbuf = qp->q_msg;
			oldqlen = qp->q_msglen;
			qp->q_msglen = smsglen;
			qp->q_msg = smsg;
			has_tsig = 1;
			qp->q_nstsig = new_tsig(key, sig, siglen); /* BEW? */

		}
		else {
			INSIST(0);
			has_tsig = 0;
			free_tsig(qp->q_nstsig);
			qp->q_nstsig = NULL;
		}
	}
	else {
		has_tsig = 0;
		free_tsig(qp->q_nstsig);
		qp->q_nstsig = NULL;
	}

	if (sendto(qp->q_dfd, (char*)qp->q_msg, qp->q_msglen, 0,
		   (struct sockaddr *)nsa,
		   sizeof(struct sockaddr_in)) < 0) {
		sendto_errno = errno;
		if (!haveComplained(ina_ulong(nsa->sin_addr),
				    (u_long)sendtoStr))
			ns_info(ns_log_default, "sysquery: sendto(%s): %s",
				sin_ntoa(*nsa), strerror(errno));
		nameserIncr(nsa->sin_addr, nssSendtoErr);
	}
	if (has_tsig == 1) {
		memput(qp->q_msg, smsgsize);
		qp->q_msg = oldqbuf;
		qp->q_msglen = oldqlen;
	}

	nameserIncr(nsa->sin_addr, nssSentSysQ);
	free_nsp(nsp);
	switch (sendto_errno) {
	case ENETDOWN:
	case ENETUNREACH:
	case EHOSTDOWN:
	case EHOSTUNREACH:
		unsched(qp);
		schedretry(qp, (time_t) 0);
	}
	return (qp);
}

/*
 * Check the list of root servers after receiving a response
 * to a query for the root servers.
 */
static int
check_root() {
	struct databuf *dp, *pdp;
	struct namebuf *np;
	int count = 0;

	priming = 0;
	for (np = hashtab->h_tab[0]; np != NULL; np = np->n_next)
		if (NAME(*np)[0] == '\0')
			break;
	if (np == NULL) {
		ns_notice(ns_log_default, "check_root: Can't find root!");
		return (0);
	}
	for (dp = np->n_data; dp != NULL; dp = dp->d_next)
		if (dp->d_type == T_NS)
			count++;
	ns_debug(ns_log_default, 1, "%d root servers", count);
	if (count < server_options->minroots) {
		ns_notice(ns_log_default,
		"check_root: %d root servers after query to root server < min",
			  count);
		return (0);
	}
	pdp = NULL;
	dp = np->n_data;
	while (dp != NULL) {
		if (dp->d_type == T_NS && dp->d_zone == DB_Z_CACHE &&
		    dp->d_ttl < (u_int32_t)tt.tv_sec) {
			ns_debug(ns_log_default, 1,
				 "deleting old root server '%s'",
				 dp->d_data);
			dp = rm_datum(dp, np, pdp, NULL);
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
check_ns() {
	struct databuf *dp, *tdp;
	struct namebuf *np, *tnp;
	struct hashbuf *htp;
	char *dname;
	int found_arr;
	const char *fname;
	time_t curtime;
	int servers = 0, rrsets = 0;

	ns_debug(ns_log_default, 2, "check_ns()");

	curtime = (u_int32_t) tt.tv_sec;
	for (np = hashtab->h_tab[0]; np != NULL; np = np->n_next) {
		if (NAME(*np)[0] != '\0')
			continue;
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			int cnames = 0;

			if (dp->d_rcode)
				continue;

			if (dp->d_type != T_NS)
				continue;

			servers++;

	        	/* look for A records */
			dname = (caddr_t) dp->d_data;
			htp = hashtab;
			tnp = nlookup(dname, &htp, &fname, 0);
			if (tnp == NULL || fname != dname) {
				ns_debug(ns_log_default, 3,
					 "check_ns: %s: not found %s %#lx",
					 dname, fname, (u_long)tnp);
				sysquery(dname, dp->d_class, T_A, NULL,
					 0, ns_port, QUERY);
				continue;
			}
			/* look for name server addresses */
			found_arr = 0;
			(void)delete_stale(tnp);
			for (tdp = tnp->n_data;
			     tdp != NULL;
			     tdp = tdp->d_next) {
				if (tdp->d_rcode)
					continue;
				if (tdp->d_type == T_CNAME)
					cnames++;
				if (tdp->d_type != T_A ||
				    tdp->d_class != dp->d_class)
					continue;
				if ((tdp->d_zone == DB_Z_CACHE) &&
				    (tdp->d_ttl < (u_int32_t)curtime)) {
					ns_debug(ns_log_default, 3, 
						 "check_ns: stale entry '%s'",
						 NAME(*tnp));
					found_arr = 0;
					break;
				}
				found_arr++;
			}
			if (found_arr)
				rrsets++;
			else if (cnames > 0)
				ns_info(ns_log_default,
					"Root NS %s -> CNAME %s",
					NAME(*np), NAME(*tnp));
			else
				sysquery(dname, dp->d_class, T_A, NULL,
					 0, ns_port, QUERY);
	        }
	}

	ns_debug(ns_log_default, 2, "check_ns: %d %d", servers, rrsets);
	return ((servers <= 2)
		? (rrsets == servers)
		: ((rrsets * 2) >= servers)
		);
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
findns(struct namebuf **npp, int class,
       struct databuf **nsp, int *countp, int flag)
{
	struct namebuf *np = *npp;
	struct databuf *dp;
	struct	databuf **nspp;
	struct hashbuf *htp;
	
	nsp[0] = NULL;

	if (priming && (np == NULL || NAME(*np)[0] == '\0'))
		htp = fcachetab;
	else
		htp = hashtab;

 try_again:
	if (htp == fcachetab && class == C_IN && !priming)
		/*
		 * XXX - do we want to set needs_prime_cache if
		 *       OPTION_FORWARD_ONLY?
		 */
		needs_prime_cache = 1;
	if (np == NULL) {
		/* find the root */
		for (np = htp->h_tab[0]; np != NULL; np = np->n_next)
			if (NAME(*np)[0] == '\0')
				break;
	}
	while (np != NULL) {
		ns_debug(ns_log_default, 5, "findns: np %#x '%s'", np,
			 NAME(*np));
		/* Look first for SOA records. */
#ifdef ADDAUTH
		if (!flag)
#endif
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			if (dp->d_zone != DB_Z_CACHE &&
			    ((zones[dp->d_zone].z_type == Z_PRIMARY) ||
			     (zones[dp->d_zone].z_type == Z_SECONDARY)) &&
			    match(dp, class, T_SOA) && dp->d_type == T_SOA) {
				ns_debug(ns_log_default, 3,
					 "findns: SOA found");
				if (zones[dp->d_zone].z_flags & Z_AUTH) {
					*npp = np;
					nsp[0] = dp;
					nsp[1] = NULL;
					DRCNTINC(dp);
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
		(void)delete_stale(np);
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			if (!match(dp, class, T_NS))
				continue;
			if (dp->d_rcode)
				continue;
			/*
			 * Don't use records that may become invalid to
			 * reference later when we do the rtt computation.
			 * Never delete our safety-belt information!
			 *
			 * XXX:	this is horribly bogus.
			 */
			if ((dp->d_zone == DB_Z_CACHE) &&
			    (dp->d_ttl < (u_int32_t)tt.tv_sec) &&
			    !(dp->d_flags & DB_F_HINT)) {
				ns_debug(ns_log_default, 1,
					 "findns: stale entry '%s'",
					 NAME(*np));
				/*
				 * We may have already added NS databufs
				 * and are going to throw them away. Fix
				 * reference counts. We don't need to free
				 * them here as we just got them from the
				 * cache.
				 */
				while (nspp > &nsp[0]) {
					nspp--;
					DRCNTDEC(*nspp);
				}
				nsp[0] = NULL;
				goto try_parent;
			}
			if (nspp < &nsp[NSMAX-1]) {
				*nspp++ = dp;
				DRCNTINC(dp);
			}
		}

		*countp = nspp - nsp;
		if (*countp > 0) {
			ns_debug(ns_log_default, 3,
				 "findns: %d NS's added for '%s'",
				 *countp, NAME(*np));
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
	ns_debug(ns_log_default, 1,
		 "findns: No root nameservers for class %s?", p_class(class));
	if ((unsigned)class < MAXCLASS && norootlogged[class] == 0) {
		norootlogged[class] = 1;
		ns_info(ns_log_default, "No root nameservers for class %s",
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
finddata(struct namebuf *np, int class, int type,
	 HEADER *hp, char **dnamep, int *lenp, int *countp)
{
	struct databuf *dp;
	char *cp;
	int buflen, n, count = 0;
	char *new_dnamep = NULL;
 	int defer = 0, found_count = 0, choice, i;
 	struct databuf **found = NULL;
	struct databuf **tmpfound = NULL;
	int foundcname;
	int stalecount;
	int ret = 0;

	stalecount = delete_stale(np);

	/* We don't want to return cached SIG records when asked for SIGs,
	 * since we may have an incomplete set.
	 */
	if (type == T_SIG && findMyZone(np, class) == DB_Z_CACHE)
		return(0);

	if (type != T_ANY && type != T_PTR && type != T_NXT) {
		found = memget((stalecount + 1) * sizeof *found);
		tmpfound = memget((stalecount + 1) * sizeof *tmpfound);
		if (found == NULL || tmpfound == NULL)
			ns_panic(ns_log_default, 1, "finddata: out of memory");
		defer = 1;
	}

	buflen = *lenp;
	
#ifdef DEBUG
	if (buflen > PACKETSZ)
		ns_debug(ns_log_default, 1, "finddata(): buflen=%d", buflen);
#endif
	cp = ((char *)hp) + *countp;
	foundcname = 0;
	for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
		if (!wanted(dp, class, type)) {
			if (type == T_CNAME && class == dp->d_class) {
				/* any data means no CNAME exists */
				if (dp->d_type != T_NXT &&
				    dp->d_type != T_KEY &&
				    dp->d_type != T_SIG) {
					ret = 0;
					goto done;
				}
			}
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
		/* -ve $ing stuff, anant@isi.edu
		 * if we have a -ve $ed record, change the rcode on the
		 * header to reflect that
		 */
		if (dp->d_rcode == NOERROR_NODATA) {
			if (count != 0) {
				/*
				 * This should not happen, yet it does...
				 */
				ns_info(ns_log_default,
				   "NODATA & data for \"%s\" type %d class %d",
					*dnamep, type, class);
				continue;
			}
			if (type == T_ANY)
				continue;
			hp->rcode = NOERROR_NODATA;
			if (dp->d_size == 0) { /* !RETURNSOA */
				ret = 1;
				goto done;
			}
		}
		if (dp->d_rcode == NXDOMAIN) {
			if (count != 0) {
				/*
				 * This should not happen, yet it might...
				 */
				ns_info(ns_log_default,
			  "NXDOMAIN & data for \"%s\" type %d class %d",
					*dnamep, type, class);
				continue;
			}
			hp->rcode = NXDOMAIN;
			if (dp->d_size == 0) { /* !RETURNSOA */	
				ret = 1;
				goto done;
			}
		}

		/* Don't put anything but key or sig RR's in response to
			     requests for key or sig */
		if (((type == T_SIG) || (type == T_KEY)) &&
		    (!((dp->d_type == T_SIG) || (dp->d_type == T_KEY))) )
			continue;

		if (!defer) {
			if (foundcname != 0 && dp->d_type == T_CNAME)
				continue;

			if ((n = make_rr(*dnamep, dp, (u_char *)cp, buflen, 1,
					 dnptrs, dnptrs_end, 0)) < 0) {
				hp->tc = 1;
				ret = *lenp - buflen;
				goto done;
			}
			if (dp->d_secure != DB_S_SECURE)
				hp->ad = 0;
			cp += n;
			buflen -= n;
			count++;
		
			if (dp->d_type == T_CNAME) {
				foundcname = 1;
#define FOLLOWCNAME(type) \
	(type != T_KEY) && (type != T_SIG) && (type != T_NXT) && (type != T_ANY)
			/* don't alias if querying for key, sig, nxt, or any */

				if (FOLLOWCNAME(type))
					new_dnamep = (char *)dp->d_data;
			}
		} else {
			if (dp->d_type == T_CNAME)
				foundcname = 1;
			found[found_count++] = dp;
		}
	}

	if (found_count == 0 && count == 0) {
		ret = 0;
		goto done;
	}

	/*
	 * If the query type was SIG or ANY we will have returned the SIG
	 * records already.
	 */
	if (type != T_SIG && type != T_ANY) {
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			if (!wantedsig(dp, class, type))
				continue;
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
			if (!defer) {
				if ((n = make_rr(*dnamep, dp, (u_char *)cp,
						 buflen, 1, dnptrs, dnptrs_end,
						 0)) < 0) {
					hp->tc = 1;
					ret = *lenp - buflen;
					goto done;
				}
				if (dp->d_secure != DB_S_SECURE)
					hp->ad = 0;
				cp += n;
				buflen -= n;
				count++;
			} else
				found[found_count++] = dp;
		}
	}

	if (defer && found_count > 0) {
		int first_sig;
		int non_sig_count;
		int sig_count;		/* number of SIG records in found */
		int idx, jdx;
		enum ordering order;

		order = match_order(np, class, foundcname ? T_CNAME : type);

		/* shuffle the SIG records down to the bottom of the array
		 * as we need to make sure they get packed last, no matter
		 * what the ordering is. We're sure to maintain the
		 * original ordering within the two sets of records (so
		 * that fixed_order can work).
		 * First we pack the non-SIG records into the temp array.
		 */
		for (idx = jdx = 0 ; idx < found_count ; idx++) {
			if (found[idx]->d_type != T_SIG) {
				tmpfound[jdx++] = found[idx];
			}
		}
		non_sig_count = jdx;
		sig_count = found_count - jdx;
		first_sig = jdx ;
		
		/* now shift the SIG records down to the end of the array
		 *  and copy in the non-SIG records
		 */
		for (i = idx = found_count - 1 ; idx >= 0 ; idx--) {
			if (i < non_sig_count) {
				found[i] = tmpfound[i];
				i--;
			} else if (found[idx]->d_type == T_SIG) {
				found[i--] = found[idx] ;
			}
		}

		foundcname = 0;
		switch (order) {
		case fixed_order:
			for (i = 0; i < found_count; i++) {
				dp = found[i];
				if (foundcname != 0 && dp->d_type == T_CNAME)
					continue;
				if (dp->d_type == T_CNAME) {
					foundcname = 1;
					if (FOLLOWCNAME(type)) {
						new_dnamep = (char *)dp->d_data;
					}
				}
				if ((n = make_rr(*dnamep, dp, (u_char *)cp,
						 buflen, 1,
						 dnptrs, dnptrs_end, 0)) < 0) {
					hp->tc = 1;
					ret = *lenp - buflen;
					goto done;
				}
				if (dp->d_secure != DB_S_SECURE)
					hp->ad = 0;
				cp += n;
				buflen -= n;
				count++;
			}
			break;

		case random_order: {
			/* first we shuffle the non-SIG records */
			int iters = non_sig_count;
			for (i = 0; i < iters; i++) {
				choice = ((u_int)rand()>>3) % non_sig_count;
				non_sig_count--;
				dp = found[choice];
				found[choice] = found[non_sig_count];
				if (foundcname != 0 && dp->d_type == T_CNAME)
					continue;
				if (dp->d_type == T_CNAME) {
					foundcname = 1;
					if (FOLLOWCNAME(type)) {
						new_dnamep = (char *)dp->d_data;
					}
				}
				if ((n = make_rr(*dnamep, dp, (u_char *)cp,
						 buflen, 1,
						 dnptrs, dnptrs_end, 0)) < 0) {
					hp->tc = 1;
					ret = *lenp - buflen;
					goto done;
				}
				if (dp->d_secure != DB_S_SECURE)
					hp->ad = 0;
				cp += n;
				buflen -= n;
				count++;
			}

			/* now shuffle the SIG records */
			iters = sig_count;
			for (i = 0; i < iters; i++) {
				choice = ((u_int)rand()>>3) % sig_count;
				choice += first_sig;
				sig_count--;
				dp = found[choice];
				found[choice] = found[sig_count + first_sig];
				if ((n = make_rr(*dnamep, dp, (u_char *)cp,
						 buflen, 1,
						 dnptrs, dnptrs_end, 0)) < 0) {
					hp->tc = 1;
					ret = *lenp - buflen;
					goto done;
				}
				if (dp->d_secure != DB_S_SECURE)
					hp->ad = 0;
				cp += n;
				buflen -= n;
				count++;
			}
			break;
		}

		case cyclic_order:
			/* first we do the non-SIG records */
			if (non_sig_count > 0)
				choice = ((u_int)rand()>>3) % non_sig_count;
			else
				choice = 0;
			for (i = 0; i < non_sig_count ; i++) {
				dp = found[(i + choice) % non_sig_count];
				if (foundcname != 0 && dp->d_type == T_CNAME)
					continue;
				if (dp->d_type == T_CNAME) {
					foundcname = 1;
					if (FOLLOWCNAME(type)) {
						new_dnamep = (char *)dp->d_data;
					}
				}
				if ((n = make_rr(*dnamep, dp, (u_char *)cp,
						 buflen, 1,
						 dnptrs, dnptrs_end, 0)) < 0) {
					hp->tc = 1;
					ret = *lenp - buflen;
					goto done;
				}
				if (dp->d_secure != DB_S_SECURE)
					hp->ad = 0;
				cp += n;
				buflen -= n;
				count++;
			}

			/* now do the SIG record rotation. */
			if (sig_count > 0) {
				choice = ((u_int)rand()>>3) % sig_count;
				choice += first_sig;
				i = choice;
				do {
					dp = found[i];
					if ((n = make_rr(*dnamep, dp,
							 (u_char *)cp,
							 buflen, 1,
							 dnptrs,
							 dnptrs_end, 0)) < 0) {
						hp->tc = 1;
						ret = *lenp - buflen;
						goto done;
					}
					if (dp->d_secure != DB_S_SECURE)
						hp->ad = 0;
					cp += n;
					buflen -= n;
					count++;
					i++;
					if (i >= found_count)
						i = first_sig;
				} while (i != choice);
			}
			
			break;

		default:
			ns_warning(ns_log_default, "finddata: unknown ordering: %d",
				   order);
			break;
		}
	}
	
	if (new_dnamep != NULL)
		*dnamep = new_dnamep;

	ns_debug(ns_log_default, 3, "finddata: added %d class %d type %d RRs",
		 count, class, type);
	ret = *lenp - buflen;
 done:
	if (found != NULL)
		memput(found, (stalecount + 1) * sizeof *found);
	if (tmpfound != NULL)
		memput(tmpfound, (stalecount + 1) * sizeof *tmpfound);
	*countp = count;
	return (ret);
}

/*
 * Do we want this data record based on the class and type?
 */
static int
wanted(const struct databuf *dp, int class, int type) {
	const u_char *cp;
	int coveredType;
	time_t expiration;
#ifdef DEBUG
	char pclass[15], ptype[15];
#endif

#ifdef DEBUG
	strcpy(pclass, p_class(class));
	strcpy(ptype, p_type(type));
	ns_debug(ns_log_default, 3, "wanted(%#x, %s %s) [%s %s]",
		 dp, pclass, ptype,
		 p_class(dp->d_class), p_type(dp->d_type));
#endif

	if (dp->d_class != class && class != C_ANY)
		return (0);
	/*
	 * Must check SIG for expiration below, other matches
	 * return OK here.
	 */
	if (type == dp->d_type && (type != T_SIG))
		return (1);
	/* For a T_ANY query, we do not want to return -ve $ed RRs. */
	if (type == T_ANY && dp->d_rcode == NOERROR_NODATA)
		return (0);

	/* First, look at the type of RR.  */
	switch (dp->d_type) {

		/* Cases to deal with:
			T_ANY search, return all unexpired SIGs.
			T_SIG search, return all unexpired SIGs.
			T_<foo> search, return all unexp SIG <FOO>s.
		 */
	case T_SIG:
		cp = dp->d_data;
		GETSHORT(coveredType, cp);
		cp += INT16SZ + INT32SZ; /* skip alg, labels, & orig TTL */
		GETLONG(expiration,cp);

		if (type == T_ANY || type == T_SIG) {
			if (expiration > time(0))
				return (1);	/* Unexpired matching SIG */
		}
		return (0);		/* We don't return this SIG. */

	case T_ANY:
		return (1);
	case T_CNAME:
		if (dp->d_rcode != NOERROR_NODATA)
			return (1);
		else
			break;
	}
	/* OK, now look at the type of query.  */
	if (type == ns_t_any)
		return (1);
	else if (type == ns_t_mailb)
		switch (dp->d_type) {
		case T_MR:
		case T_MB:
		case T_MG:
		case T_MINFO:
			return (1);
		}
	else if (ns_t_xfr_p(type)) {
		/*
		 * This is used to validate transfer requests, not
		 * generate transfer responses.  Is there an SOA?
		 */
		if (dp->d_type == ns_t_soa && dp->d_zone != DB_Z_CACHE
		    && (zones[dp->d_zone].z_flags & Z_AUTH))
			return (1);
	}
	return (0);
}

static int
wantedsig(const struct databuf *dp, int class, int type) {
	const u_char *cp;
	int coveredType;
	time_t expiration;
#ifdef DEBUG
	char pclass[15], ptype[15];
#endif

#ifdef DEBUG
	strcpy(pclass, p_class(class));
	strcpy(ptype, p_type(type));
	ns_debug(ns_log_default, 3, "wantedtsig(%#x, %s %s) [%s %s]",
		 dp, pclass, ptype,
		 p_class(dp->d_class), p_type(dp->d_type));
#endif

	if (dp->d_class != class && class != C_ANY)
		return (0);
	if (dp->d_type != T_SIG || dp->d_rcode != 0)
		return (0);

	cp = dp->d_data;
	GETSHORT(coveredType, cp);
	cp += INT16SZ + INT32SZ; /* skip alg, labels, & orig TTL */
	GETLONG(expiration,cp);
	if (expiration < time(0))
		return (0);

	if (type == T_ANY || type == T_SIG || type == coveredType)
			return (1);
	if (type == ns_t_mailb) {
		switch (coveredType) {
		case T_MR:
		case T_MB:
		case T_MG:
		case T_MINFO:
			return (1);
		}
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
add_data(struct namebuf *np, struct databuf **dpp,
	 u_char *cp, int buflen, int *countp)
{
	struct databuf *dp;
	char dname[MAXDNAME];
	int n, bytes;

	bytes = *countp = 0;
	getname(np, dname, sizeof(dname));
	for (dp = *dpp++; dp != NULL; dp = *dpp++) {
		if (stale(dp))
			continue;	/* ignore old cache entry */
		if (dp->d_rcode)
			continue;
		if ((n = make_rr(dname, dp, cp, buflen, 1,
				 dnptrs, dnptrs_end, 0)) < 0)
			return (-bytes);	/* Truncation */
		cp += n;
		buflen -= n;
		bytes += n;
		(*countp)++;
	}
	return (bytes);
}

static void
rrsetadd(struct flush_set *flushset, const char *name, struct databuf *dp) {
	struct flush_set *fs = flushset;
	struct db_list *dbl;

	while (fs->fs_name && (
		ns_samename(fs->fs_name,name) != 1 ||
		(fs->fs_class != dp->d_class) ||
		(fs->fs_type != dp->d_type) ||
		(fs->fs_cred != dp->d_cred))) {
		fs++;
	}
	if (!fs->fs_name) {
		fs->fs_name = savestr(name, 1);
		fs->fs_class = dp->d_class;
		fs->fs_type = dp->d_type;
		fs->fs_cred = dp->d_cred;
		fs->fs_list = NULL;
		fs->fs_last = NULL;
	}
	dbl = (struct db_list *)memget(sizeof(struct db_list));
	if (!dbl)
		panic("rrsetadd: out of memory", NULL);
	dbl->db_next = NULL;
	dbl->db_dp = dp;
	if (fs->fs_last == NULL)
		fs->fs_list = dbl;
	else
		fs->fs_last->db_next = dbl;
	fs->fs_last = dbl;
}

static int
ttlcheck(const char *name, struct db_list *dbl, int update) {
	int type = dbl->db_dp->d_type;
	int class = dbl->db_dp->d_class;
	struct hashbuf *htp = hashtab;
	const char *fname;
	struct namebuf *np;
	struct db_list *dbp = dbl;
	struct databuf *dp;
	u_int32_t ttl = 0;	/* Make gcc happy. */
	int first;


	np = nlookup(name, &htp, &fname, 0);
	if (np == NULL || fname != name || ns_wildcard(NAME(*np)))
		return (1);

	/* check that all the ttl's we have are the same, if not return 1 */
	first = 1;
	for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
		if (!match(dp, class, type))
			continue;
		if (first) {
 			/* we can't update zone data so return early */
			if (dp->d_zone != DB_Z_CACHE)
 				return (0);
			ttl = dp->d_ttl;
			first = 0;
		} else if (ttl != dp->d_ttl)
			return (1);
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

/*
 * lookup rrset in table and compare to dbl
 * tri state result
 * -1: lookup failed
 * 0: rrsets same
 * 1: rrsets differ
 */

static int
rrsetcmp(char * name, struct db_list * dbl, struct hashbuf * table) {
	int type = dbl->db_dp->d_type;
	int class = dbl->db_dp->d_class;
	struct hashbuf *htp = table;
	const char *fname;
	struct namebuf *np;
	struct db_list *dbp = dbl;
	struct databuf *dp;
	int exists = 0;


	np = nlookup(name, &htp, &fname, 0);
	if (np == NULL || fname != name || ns_wildcard(NAME(*np))) {
		ns_debug(ns_log_default, 3, "rrsetcmp: name not in database");
		return (-1);
	}

	/* check that all entries in dbl are in the cache */
	while (dbp) {
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			if (!match(dp, class, type))
				continue;
			exists = 1;
			if (!db_cmp(dp, dbp->db_dp) 
#ifdef NOADDITIONAL
			&& ((dp->d_cred == dbp->db_dp->d_cred) ||
				 (dp->d_cred != DB_C_ADDITIONAL))
#endif
				 )
				break;
		}
		if (!dp) {
			ns_debug(ns_log_default, 3,
				 "rrsetcmp: %srecord%s in database",
				 exists ? "" : "no ", exists ? " not" : "s");
			return (exists ? 1 : -1);
		}
		dbp = dbp->db_next;
	}

	/* Check that all cache entries are in the list. */
	for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
		if (!match(dp, class, type))
			continue;
#ifdef NCACHE
		if (dp->d_rcode)
			return (1);
#endif
		dbp = dbl;
		while (dbp) {
			if (!db_cmp(dp, dbp->db_dp))
				break;
			dbp = dbp->db_next;
		}
		if (!dbp) {
			ns_debug(ns_log_default, 3,
				 "rrsetcmp: record not in rrset");
			return (1);
		}
	}
	ns_debug(ns_log_default, 3, "rrsetcmp: rrsets matched");
	return (0);
}

/*
 * verify incoming answer against what we already have in the hints
 * issue warnings / errors if differences detected.
 */

static void
check_hints(struct flush_set * flushset) {
	struct zoneinfo *zp;
	struct flush_set *fs;
	struct db_list *dbp;

	/* We don't use hints when in forward only mode */
	if (NS_OPTION_P(OPTION_FORWARD_ONLY))
		return;

	/* find "." NS rrset and hence class */
	for (fs = flushset; fs->fs_name != NULL; fs++) {
		if ((fs->fs_name[0] != '\0') || (fs->fs_type != ns_t_ns))
			continue;

		/* see if we are a root server */
		zp = find_zone(fs->fs_name, fs->fs_class);
		if (zp != NULL &&
		    (zp->z_type == z_master || zp->z_type == z_slave))
			return;
		switch (rrsetcmp(fs->fs_name, fs->fs_list, fcachetab)) {
		case -1:
			ns_error(ns_log_default,
			    "check_hints: no NS records for class %d in hints",
				fs->fs_class);
			break;
		case 1:
			ns_warning(ns_log_default,
 "check_hints: root NS list in hints for class %d does not match root NS list",
				fs->fs_class);
			break;
		case 0:
			break;
		default:
			ns_error(ns_log_default,
			    "check_hints: unexpected response from rrsetcmp");
			break;
		}
		break;
	}

	if (fs->fs_name == NULL)	/* no root NS records */
		return;

	dbp = fs->fs_list;
	while (dbp) {
		/* for each NS find A rrset in answer and check */
		for (fs = flushset; fs->fs_name != NULL; fs++) {
			if (ns_samename(fs->fs_name, (char *)dbp->db_dp->d_data) != 1
			    || fs->fs_type != ns_t_a)
				continue;
			switch (rrsetcmp(fs->fs_name, fs->fs_list, fcachetab)) {
			case -1:
				ns_error(ns_log_default,
			"check_hints: no A records for %s class %d in hints",
					fs->fs_name[0] ? fs->fs_name : ".",
					fs->fs_class);
				break;
			case 1:
				ns_warning(ns_log_default,
	 "check_hints: A records for %s class %d do not match hint records",
					fs->fs_name[0] ? fs->fs_name : ".",
					fs->fs_class);
				break;
			case 0:
				break;
			default:
				ns_error(ns_log_default,
				    "check_hints: unexpected response from rrsetcmp");
				break;
			}
			break;
		}

		if (fs->fs_name == NULL)
			ns_debug(ns_log_default, 2,
				"check_hints: no A records for %s",
				dbp->db_dp->d_data);

		dbp = dbp->db_next;
	}
}

static void
rrsetupdate(struct flush_set * flushset, int flags, struct sockaddr_in from,
	    int updatettl) {
	struct flush_set *fs = flushset;
	struct db_list *dbp, *odbp;
	int n;
	void *state = NULL;

	while (fs->fs_name) {
		ns_debug(ns_log_default, 2, "rrsetupdate: %s",
			 fs->fs_name[0] ? fs->fs_name : ".");
		if ((n = rrsetcmp(fs->fs_name, fs->fs_list, hashtab)) &&
		    ttlcheck(fs->fs_name, fs->fs_list, 0)) {
			if (n > 0)
				flushrrset(fs, from);

			dbp = fs->fs_list;
			while (dbp) {
				n = db_set_update(fs->fs_name, dbp->db_dp,
						  &state, flags,
						  &hashtab, from, NULL,
						  0, NULL);
				ns_debug(ns_log_default, 3,
					 "rrsetupdate: %s %d",
					 fs->fs_name[0] ? fs->fs_name : ".",
					 n);
				odbp = dbp;
				dbp = dbp->db_next;
				memput(odbp, sizeof *odbp);
			}    
			ns_debug(ns_log_default, 3,
				 "rrsetupdate: %s %d",
				 fs->fs_name[0] ? fs->fs_name : ".", n);
		} else {
			if ((n == 0) && updatettl)
				(void)ttlcheck(fs->fs_name,fs->fs_list, 1);
			dbp = fs->fs_list;
			while (dbp) {
				db_freedata(dbp->db_dp);
				odbp = dbp;
				dbp = dbp->db_next;
				memput(odbp, sizeof *odbp);
			}
		}
		fs->fs_list = NULL;
		fs++;
	}
	n = db_set_update(NULL, NULL, &state, flags, &hashtab, from,
			  NULL, 0, NULL);
}

static void
flushrrset(struct flush_set * fs, struct sockaddr_in from) {
	struct databuf *dp;
	int n;

	ns_debug(ns_log_default, 2, "flushrrset(%s, %s, %s, %d)",
		 fs->fs_name[0]?fs->fs_name:".", p_type(fs->fs_type),
		 p_class(fs->fs_class), fs->fs_cred);
	dp = savedata(fs->fs_class, fs->fs_type, 0, NULL, 0);
	dp->d_zone = DB_Z_CACHE;
	dp->d_cred = fs->fs_cred;
	dp->d_clev = 0;	
	do {
		n = db_update(fs->fs_name, dp, NULL, NULL, DB_DELETE, hashtab,
			      from);
		ns_debug(ns_log_default, 3, "flushrrset: %d", n);
	} while (n == OK);
	db_freedata(dp);
}

static void
free_flushset(struct flush_set *flushset, int flushset_size) {
	struct flush_set *fs;

	for (fs = flushset; fs->fs_name != NULL; fs++)
		freestr(fs->fs_name);
	memput(flushset, flushset_size);
}

/*
 *  This is best thought of as a "cache invalidate" function.
 *  It is called whenever a piece of data is determined to have
 *  become invalid either through a timeout or a validation
 *  failure.  It is better to have no information, than to
 *  have partial information you pass off as complete.
 */
void
delete_all(struct namebuf *np, int class, int type) {
	struct databuf *dp, *pdp;

	ns_debug(ns_log_default, 3, "delete_all(%#x:\"%s\" %s %s)",
		 np, NAME(*np), p_class(class), p_type(type));
	pdp = NULL;
	dp = np->n_data;
	while (dp != NULL) {
		if (dp->d_zone == DB_Z_CACHE && (dp->d_flags & DB_F_HINT) == 0
		    && match(dp, class, type)) {
			dp = rm_datum(dp, np, pdp, NULL);
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
 *	number of RRs associated with this name.
 * side effects:
 *	delete_all() can be called, freeing memory and relinking chains.
 */
int 
delete_stale(np)
	struct namebuf *np;
{
	struct databuf *dp;
	int count;
 again:
	count = 0;
        for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
                if (dp->d_zone == DB_Z_CACHE && stale(dp)) {
                        delete_all(np, dp->d_class, dp->d_type);
                        goto again;
		}
		count++;
 	}
	return (count);
}


/*
 * Adjust answer message so that it fits in outlen. Set tc if required.
 *
 * If outlen = msglen, can be used to verify qdcount, ancount, nscount
 * and arcount.
 *
 * return new length
 */

int
trunc_adjust(u_char *msg, int msglen, int outlen) {
	register HEADER *hp;
	u_int qdcount, ancount, nscount, arcount, dlen;
	u_char *cp = msg, *cp1, *eom_in, *eom_out;
	int n;

	eom_in = msg + msglen;
	eom_out = msg + outlen;

	hp = (HEADER *)msg;
	qdcount = ntohs(hp->qdcount);
	ancount = ntohs(hp->ancount);
	nscount = ntohs(hp->nscount);
	arcount = ntohs(hp->arcount);
	cp += HFIXEDSZ;

	while ((qdcount || ancount || nscount || arcount) &&
	       cp < eom_in && cp < eom_out) {

		cp1 = cp; /* use temporary in case we break */

		n = dn_skipname(cp1, eom_in);
		if (n < 0)
			break;
		cp1 += n + 2 * INT16SZ; /* type, class */

		if (!qdcount) {
			cp1 += INT32SZ;     /* ttl */
			if (cp1 + INT16SZ > eom_in)
				break;
			GETSHORT(dlen, cp1);
			cp1 += dlen;
		}

		if (cp1 > eom_in || cp1 > eom_out)
			break;

		cp = cp1;

		if (qdcount)
			qdcount--;
		else if (ancount)
			ancount--;
		else if (nscount)
			nscount--;
		else
			arcount--;
	}

	if (qdcount || ancount || nscount || arcount) {
		ns_debug(ns_log_default, 1,
			 "trunc_adjust:%s %d %d %d %d %d, %d %d %d %d %d",
			 hp->tc?" tc":"", msglen,
			 ntohs(hp->qdcount), ntohs(hp->ancount),
			 ntohs(hp->nscount), ntohs(hp->arcount),
			 cp-msg, qdcount, ancount, nscount, arcount);
		hp->tc = 1;
		hp->qdcount = htons(ntohs(hp->qdcount) - qdcount);
		hp->ancount = htons(ntohs(hp->ancount) - ancount);
		hp->nscount = htons(ntohs(hp->nscount) - nscount);
		hp->arcount = htons(ntohs(hp->arcount) - arcount);
	}
	ENSURE(cp <= eom_out);
	return (cp - msg);
}

/*
 * mark the server "from" bad in the qp structure so it won't be retried.
 */
static void
mark_bad(struct qinfo *qp, struct sockaddr_in from) {
	int i;

	for (i = 0; i < (int)qp->q_naddr; i++)
		if (ina_equal(qp->q_addr[i].ns_addr.sin_addr, from.sin_addr))
			qp->q_addr[i].nretry = MAXRETRY;
}

static void
mark_lame(struct qinfo *qp, struct sockaddr_in from) {
	int i;

	for (i = 0; i < (int)qp->q_naddr; i++)
		if (ina_equal(qp->q_addr[i].ns_addr.sin_addr, from.sin_addr) &&
		    qp->q_addr[i].ns != NULL) {
			qp->q_addr[i].ns->d_flags |= DB_F_LAME;
			db_lame_add(qp->q_domain,
				    (char*)qp->q_addr[i].ns->d_data, 
				    tt.tv_sec + server_options->lame_ttl);
		}
}

/*
 * Retry the message if and only if from matches where the query was
 * last sent to.  The code does not handle responses sent from the
 * wrong interface an a multihomed server.
 */
static void
fast_retry(struct qinfo *qp, struct sockaddr_in from) {
        if (ina_equal(qp->q_addr[qp->q_curaddr].ns_addr.sin_addr,
                   from.sin_addr))
                retry(qp);
}

static void
add_related_additional(char *name) {
	int i;

	if (num_related >= MAX_RELATED - 1)
		return;
	for (i = 0; i < num_related; i++)
		if (ns_samename(name, related[i]) == 1) {
			freestr(name);
			return;
		}
	related[num_related++] = name;
}

static void
free_related_additional() {
	int i;

	for (i = 0; i < num_related; i++)
		freestr(related[i]);
	num_related = 0;
}

static int
related_additional(char *name) {
	int i;

	for (i = 0; i < num_related; i++)
		if (ns_samename(name, related[i]) == 1)
			return (1);
	return (0);
}

static void
freestr_maybe(char **tname) {
	if (tname == NULL || *tname == NULL)
		return;
	freestr(*tname);
	*tname = NULL;
}

/*
 * Match a request namebuf against the configured rrset-order info.  First
 * match wins.  There is an implicit '*.' at the front to the ordering names.
 */
static enum ordering
match_order(const struct namebuf *np, int class, int type) {
	rrset_order_list orders = server_options->ordering;
	rrset_order_element roe;
	
	if (orders == NULL)
		return (DEFAULT_ORDERING);
	
	for (roe = orders->first ; roe != NULL ; roe = roe->next) {
		if (roe->class != C_ANY && roe->class != class)
			continue;
		if (roe->type != T_ANY && roe->type != type)
			continue;

		if (match_name(np, roe->name, strlen(roe->name)) == 0) {
			return (roe->order);
		}
	}

	/* none matched so use default */
	return (DEFAULT_ORDERING);
}

/* Do a simple compare of the NP data against the given NAME, recursively
 * looking at the NP parent if necessary. NAMELEN is the length of the NAME
 * that needs to be matched. Matching happen from right to left. Returns -1
 * on failure, on success the index of the first character of the matched
 * portion of the string is returned. In the first level call a return
 * value of 0 is of interest.
 */
static int
match_name(const struct namebuf *np, const char *name, size_t namelen)
{
	int matched ;
	
	if (name[0] == '*' && name[1] == '\0')
		return 0;

	if (np->n_parent != NULL) { /* recurse to end of np list */
		matched = match_name(np->n_parent,name,namelen);
	} else {
		matched = namelen;
	}
	
	if (matched > 0) {
		int labellen = NAMELEN(*np);
		char pch;
		const char *start;
		
		if (labellen > matched) {
			return -1;
		} else if (labellen < matched) {
			/* string is longer than this namebuf's data, so
			   make sure there's a period before the end of the 
			   match so we don't just match a suffix. */
			start = name + (matched - labellen);
			pch = start[-1];
			if (pch != '.') {
				return -1;
			}
		} else {
			start = name ;
		}

		if (strncasecmp(start, NAME(*np), labellen) == 0) {
			/* looking good. tell our caller what portion of
			   the tail of string has been matched */
			if (start == name)
				return (0) ;
			else 
				return (start - name - 1); /* matched '.' too */
		} else {
			return (-1);
		}
	}

	return (matched);
}

