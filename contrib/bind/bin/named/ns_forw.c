#if !defined(lint) && !defined(SABER)
static const char sccsid[] = "@(#)ns_forw.c	4.32 (Berkeley) 3/3/91";
static const char rcsid[] = "$Id: ns_forw.c,v 8.89 2002/01/29 03:59:36 marka Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1986
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
#include <sys/un.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

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

struct complaint {
	u_long		tag1, tag2;
	time_t		expire;
	struct complaint *next;
};

static struct complaint *complaints = NULL;
static int retry_timer_set = 0;

/*
 * Forward the query to get the answer since its not in the database.
 * Returns FW_OK if a request struct is allocated and the query sent.
 * Returns FW_DUP if this is a duplicate of a pending request. 
 * Returns FW_NOSERVER if there were no addresses for the nameservers.
 * Returns FW_SERVFAIL on memory allocation error or if asked to do something
 *	dangerous, such as fwd to ourselves or fwd to the host that asked us.
 *
 * (no action is taken on errors and qpp is not filled in.)
 */
int
ns_forw(struct databuf *nsp[], u_char *msg, int msglen,
	struct sockaddr_in from, struct qstream *qsp, int dfd,
	struct qinfo **qpp, const char *dname, int class, int type,
	struct namebuf *np, int use_tcp, struct tsig_record *in_tsig)
{
	struct qinfo *qp;
	char tmpdomain[MAXDNAME];
	struct sockaddr_in *nsa;
	HEADER *hp;
	u_int16_t id;
	int sendto_errno = 0;
	int n, has_tsig, oldqlen = 0;
	u_char *oldqbuf = NULL;
	u_char *smsg = NULL;
	int smsglen, smsgsize = 0, siglen;
	u_char sig[TSIG_SIG_SIZE];
	DST_KEY *key;

	ns_debug(ns_log_default, 3, "ns_forw()");

	hp = (HEADER *) msg;
	id = hp->id;
	/* Look at them all */
	for (qp = nsqhead; qp != NULL; qp = qp->q_link) {
		if (qp->q_id == id &&
		    memcmp(&qp->q_from, &from, sizeof qp->q_from) == 0 &&
		    ((qp->q_cmsglen == 0 && qp->q_msglen == msglen &&
		      memcmp(qp->q_msg + 2, msg + 2, msglen - 2) == 0) ||
		     (qp->q_cmsglen == msglen &&
		      memcmp(qp->q_cmsg + 2, msg + 2, msglen - 2) == 0)
		     )) {
			ns_debug(ns_log_default, 3, "forw: dropped DUP id=%d",
				 ntohs(id));
			nameserIncr(from.sin_addr, nssRcvdDupQ);
			return (FW_DUP);
		}
	}

	qp = qnew(dname, class, type, 1);
	getname(np, tmpdomain, sizeof tmpdomain);
	qp->q_domain = savestr(tmpdomain, 1);
	qp->q_from = from;	/* nslookup wants to know this */
	if (NS_ZFWDTAB(qp->q_fzone))
		nsfwdadd(qp, NS_ZFWDTAB(qp->q_fzone));
	if (NS_ZOPTION_P(qp->q_fzone, OPTION_FORWARD_ONLY))
		n = 0;
	else
		n = nslookup(nsp, qp, dname, "ns_forw");
	if (n < 0) {
		if (n == -1)
			ns_debug(ns_log_default, 2,
				 "forw: nslookup reports danger");
		ns_freeqry(qp);
		return (FW_SERVFAIL);
	}
	if (n == 0 && !NS_ZFWDTAB(qp->q_fzone)) {
		ns_debug(ns_log_default, 2, "forw: no nameservers found");
		ns_freeqry(qp);
		return (FW_NOSERVER);
	}
	qp->q_stream = qsp;
	qp->q_curaddr = 0;
	qp->q_dfd = dfd;
	qp->q_id = id;
	qp->q_expire = tt.tv_sec + RETRY_TIMEOUT*2;
	if (in_tsig != NULL)
		qp->q_tsig = new_tsig(in_tsig->key, in_tsig->sig,
				      in_tsig->siglen);
	if (use_tcp)
		qp->q_flags |= Q_USEVC;
	hp->id = qp->q_nsid = htons(nsid_next());
	hp->ancount = htons(0);
	hp->nscount = htons(0);
	hp->arcount = htons(0);
	if ((qp->q_msg = (u_char *)memget((unsigned)msglen)) == NULL) {
		ns_notice(ns_log_default, "forw: memget: %s",
			  strerror(errno));
		ns_freeqry(qp);
		return (FW_SERVFAIL);
	}
	qp->q_msgsize = msglen;
	memcpy(qp->q_msg, msg, qp->q_msglen = msglen);
	hp = (HEADER *) qp->q_msg;
	hp->rd = (qp->q_addr[0].forwarder ? 1 : 0);
	qp->q_addr[0].stime = tt;

	schedretry(qp, retrytime(qp));

	nsa = Q_NEXTADDR(qp, 0);
	ns_debug(ns_log_default, 1,
		"forw: forw -> [%s].%d ds=%d nsid=%d id=%d %dms retry %dsec",
		inet_ntoa(nsa->sin_addr),
		ntohs(nsa->sin_port), ds,
		ntohs(qp->q_nsid), ntohs(qp->q_id),
		(qp->q_addr[0].nsdata != NULL)
			? qp->q_addr[0].nsdata->d_nstime
			: -1,
		(int)(qp->q_time - tt.tv_sec));

#ifdef DEBUG
	if (debug >= 10)
		res_pquery(&res, msg, msglen, log_get_stream(packet_channel));
#endif
        key = qp->q_keys[0];
        if (key == NULL)
                key = qp->q_keys[0] = tsig_key_from_addr(nsa->sin_addr);
	if (key != NULL || !qp->q_addr[0].noedns) {
		smsgsize = qp->q_msglen + TSIG_BUF_SIZE + 11;
		smsg = memget(smsgsize);
		if (smsg == NULL)
			ns_panic(ns_log_default, 1, "ns_forw: memget failed");
		smsglen = qp->q_msglen;
		siglen = sizeof(sig);
		memcpy(smsg, qp->q_msg, qp->q_msglen);
	}

	if (!qp->q_addr[0].noedns)
		smsglen += ns_add_opt(smsg, smsg + smsglen, smsgsize, 0, 0,
				      EDNS_MESSAGE_SZ, 0, NULL, 0);

	if (key != NULL) {
		n = ns_sign(smsg, &smsglen, smsgsize, NOERROR, key, NULL, 0,
			    sig, &siglen, 0);
		if (n == 0) {
			has_tsig = 1;
			free_tsig(qp->q_nstsig);
			qp->q_nstsig = new_tsig(key, sig, siglen);
		} else {
			has_tsig = 0;
			free_tsig(qp->q_nstsig);
			qp->q_nstsig = NULL;
			INSIST(0);
		}
	} else {
		has_tsig = 0;
		free_tsig(qp->q_nstsig);
		qp->q_nstsig = NULL;
	}

	if (smsg != NULL) {
		oldqbuf = qp->q_msg;
		oldqlen = qp->q_msglen;
		qp->q_msglen = smsglen;
		qp->q_msg = smsg;
		hp = (HEADER *) qp->q_msg;
	}

	if (qp->q_flags & Q_USEVC) {
		if (tcp_send(qp) != NOERROR) {
			if (!haveComplained(ina_ulong(nsa->sin_addr),
					    (u_long)tcpsendStr))
				ns_info(ns_log_default,
					"ns_forw: tcp_send(%s) failed: %s",
					sin_ntoa(*nsa), strerror(errno));
		}
	} else if (sendto(ds, (char *)qp->q_msg, qp->q_msglen, 0,
		   (struct sockaddr *)nsa,
		   sizeof(struct sockaddr_in)) < 0) {
		sendto_errno = errno;
		if (!haveComplained(ina_ulong(nsa->sin_addr),
				    (u_long)sendtoStr))
			ns_info(ns_log_default, "ns_forw: sendto(%s): %s",
				sin_ntoa(*nsa), strerror(errno));
		nameserIncr(nsa->sin_addr, nssSendtoErr);
	}

	if (smsgsize != 0) {
		memput(smsg, smsgsize);
		qp->q_msg = oldqbuf;
		qp->q_msglen = oldqlen;
		hp = (HEADER *) qp->q_msg;
	}

	nameserIncr(from.sin_addr, nssRcvdFwdQ);
	nameserIncr(nsa->sin_addr, nssSentFwdQ);
	if (qpp)
		*qpp = qp;
	hp->rd = 1;
	switch (sendto_errno)  {
	case ENETDOWN:
	case ENETUNREACH:
	case EHOSTDOWN:
	case EHOSTUNREACH:
		unsched(qp);
		schedretry(qp, (time_t) 0);
	}
	return (0);
}

/* haveComplained(tag1, tag2)
 *	check to see if we have complained about (tag1,tag2) recently
 * returns:
 *	boolean: have we complained recently?
 * side-effects:
 *	outdated complaint records removed from our static list
 * author:
 *	Paul Vixie (DECWRL) April 1991
 */
int
haveComplained(u_long tag1, u_long tag2) {
	struct complaint *cur, *next, *prev;
	int r = 0;

	for (cur = complaints, prev = NULL;
	     cur != NULL;
	     prev = cur, cur = next) {
		next = cur->next;
		if (tt.tv_sec > cur->expire) {
			if (prev)
				prev->next = next;
			else
				complaints = next;
			memput(cur, sizeof *cur);
			cur = prev;
		} else if (tag1 == cur->tag1 && tag2 == cur->tag2)
			r++;
	}
	if (!r) {
		cur = (struct complaint *)memget(sizeof(struct complaint));
		if (cur) {
			cur->tag1 = tag1;
			cur->tag2 = tag2;
			cur->expire = tt.tv_sec + INIT_REFRESH;	/* "10:00" */
			cur->next = NULL;
			if (prev)
				prev->next = cur;
			else
				complaints = cur;
		}
	}
	return (r);
}

void
freeComplaints(void) {
	struct complaint *cur, *next;

	for (cur = complaints; cur != NULL; cur = next) {
		next = cur->next;
		memput(cur, sizeof *cur);
	}
	complaints = NULL;
}

/* void
 * nslookupComplain(sysloginfo, queryname, complaint, dname, a_rr)
 *	Issue a complaint about a dangerous situation found by nslookup().
 * params:
 *	sysloginfo is a string identifying the complainant.
 *	queryname is the domain name associated with the problem.
 *	complaint is a string describing what is wrong.
 *	dname and a_rr are the problematic other name server.
 */
static void
nslookupComplain(const char *sysloginfo, const char *queryname,
		 const char *complaint, const char *dname,
		 const struct databuf *a_rr, const struct databuf *nsdp)
{
	char *a, *ns;
	const char *a_type;
	int print_a;

	ns_debug(ns_log_default, 2, "NS '%s' %s", dname, complaint);
	if (sysloginfo && queryname && !haveComplained((u_long)queryname,
						       (u_long)complaint)) {
		char nsbuf[20], abuf[20];

		a = ns = (char *)NULL;
		print_a = (a_rr->d_type == T_A);
		a_type = p_type(a_rr->d_type);
		if (a_rr->d_rcode) {
			print_a = 0;
			switch(a_rr->d_rcode) {
			case NXDOMAIN:
				a_type = "NXDOMAIN";
				break;
			case NOERROR_NODATA:
				a_type = "NODATA";
				break;
			}
		}
		if (nsdp != NULL) {
			if (nsdp->d_addr.s_addr != htonl(0)) {
				strcpy(nsbuf,
				       inet_ntoa(nsdp->d_addr));
				ns = nsbuf;
			} else {
				ns = zones[nsdp->d_zone].z_origin;
			}
		}
		if (a_rr->d_addr.s_addr != htonl(0)) {
			strcpy(abuf, inet_ntoa(a_rr->d_addr));
			a = abuf;
		} else {
			a = zones[a_rr->d_zone].z_origin;
		}
		if (a != NULL || ns != NULL)
			ns_info(ns_log_default, 
			       "%s: query(%s) %s (%s:%s) learnt (%s=%s:NS=%s)",
				sysloginfo, queryname,
				complaint, dname,
				print_a ?
				    inet_ntoa(ina_get(a_rr->d_data)) : "",
				a_type,
				a ? a : "<Not Available>",
				ns ? ns : "<Not Available>" );
		else
			ns_info(ns_log_default, "%s: query(%s) %s (%s:%s)",
				sysloginfo, queryname,
				complaint, dname,
				print_a ?
				inet_ntoa(ina_get(a_rr->d_data)) : "");
	}
}

/*
 * nslookup(nsp, qp, syslogdname, sysloginfo)
 *	Lookup the address for each nameserver in `nsp' and add it to
 * 	the list saved in the qinfo structure pointed to by `qp'.
 *	Omits information about nameservers that we shouldn't ask.
 *	Detects the following dangerous operations:
 *		One of the A records for one of the nameservers in nsp
 *		refers to the address of one of our own interfaces;
 *		One of the A records refers to the nameserver port on
 *		the host that asked us this question.
 * returns: the number of addresses added, or -1 if a dangerous operation
 *	is detected.
 * side effects:
 *	logs if a dangerous situation is detected and
 *	(syslogdname && sysloginfo)
 */
int
nslookup(struct databuf *nsp[], struct qinfo *qp,
	 const char *syslogdname, const char *sysloginfo)
{
	struct namebuf *np;
	struct databuf *dp, *nsdp;
	struct qserv *qs;
	int n;
	u_int i;
	struct hashbuf *tmphtp;
	char *dname;
	const char *fname;
	int oldn, naddr, class, found_arr, potential_ns, lame_ns;
	time_t curtime;

	ns_debug(ns_log_default, 3, "nslookup(nsp=%p, qp=%p, \"%s\", d=%d)",
		 nsp, qp, syslogdname, qp->q_distance);

	lame_ns = potential_ns = 0;
	naddr = n = qp->q_naddr;
	curtime = (u_long) tt.tv_sec;
	while ((nsdp = *nsp++) != NULL && n < NSMAX) {
		class = nsdp->d_class;
		dname = (char *)nsdp->d_data;
		ns_debug(ns_log_default, 3,
			 "nslookup: NS \"%s\" c=%d t=%d (flags 0x%lu)",
			 dname, class, nsdp->d_type, (u_long)nsdp->d_flags);

		/* don't put in servers we have tried */
		for (i = 0; i < qp->q_nusedns; i++) {
			if (qp->q_usedns[i] == nsdp) {
				ns_debug(ns_log_default, 2,
					 "skipping used NS w/name %s",
					 nsdp->d_data);
				goto skipserver;
			}
		}

		/* skip lame servers */
		if ((nsdp->d_flags & DB_F_LAME) != 0) {
			time_t when;
			when = db_lame_find(qp->q_domain, nsdp);
			if (when != 0 && when > tt.tv_sec) {
				ns_debug(ns_log_default, 3,
					 "skipping lame NS");
				lame_ns++;
				goto skipserver;
			}
		}

		tmphtp = ((nsdp->d_flags & DB_F_HINT) ?fcachetab :hashtab);
		np = nlookup(dname, &tmphtp, &fname, 0);
		if (np == NULL) {
			ns_debug(ns_log_default, 3, "%s: not found %s %p",
				 dname, fname, np);
			found_arr = 0;
			goto need_sysquery;
		}
		if (fname != dname) {
			found_arr = 0;
			goto need_sysquery;
		}
		found_arr = 0;
		oldn = n;

		/* look for name server addresses */
		(void)delete_stale(np);
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			struct in_addr nsa;

			if (dp->d_type == T_CNAME && dp->d_class == class) {
				static const char *complaint =
				        "NS points to CNAME";
				if (dp->d_rcode)
					continue;
				nslookupComplain(sysloginfo, syslogdname,
						complaint, dname, dp, nsdp);
				goto skipserver;
			}
			if (dp->d_rcode == NXDOMAIN && dp->d_class == class)
				goto skipserver;
			if (dp->d_type != T_A || dp->d_class != class)
				continue;
			if (dp->d_rcode) {
				/* Negative caching element. */
				goto skipserver;
			}
			if (ina_hlong(ina_get(dp->d_data)) == INADDR_ANY) {
				static const char *complaint =
					"Bogus (0.0.0.0) A RR";
				nslookupComplain(sysloginfo, syslogdname,
						 complaint, dname, dp, nsdp);
				continue;
			}
#ifdef INADDR_LOOPBACK
			if (ina_hlong(ina_get(dp->d_data))==INADDR_LOOPBACK) {
				static const char *complaint =
					"Bogus LOOPBACK A RR";
				nslookupComplain(sysloginfo, syslogdname,
						 complaint, dname, dp, nsdp);
				continue;
			}
#endif
#ifdef INADDR_BROADCAST
			if (ina_hlong(ina_get(dp->d_data))==INADDR_BROADCAST){
				static const char *complaint = 
					"Bogus BROADCAST A RR";
				nslookupComplain(sysloginfo, syslogdname,
						 complaint, dname, dp, nsdp);
				continue;
			}
#endif
#ifdef IN_MULTICAST
			if (IN_MULTICAST(ina_hlong(ina_get(dp->d_data)))) {
				static const char *complaint =
					"Bogus MULTICAST A RR";
				nslookupComplain(sysloginfo, syslogdname,
						 complaint, dname, dp, nsdp);
				continue;
			}
#endif
			/*
			 * Don't use records that may become invalid to
			 * reference later when we do the rtt computation.
			 * Never delete our safety-belt information!
			 */
			if ((dp->d_zone == DB_Z_CACHE) &&
			    (dp->d_ttl < (u_int32_t)curtime) &&
			    !(dp->d_flags & DB_F_HINT) )
		        {
				ns_debug(ns_log_default, 1,
					 "nslookup: stale '%s'",
					 NAME(*np));
				n = oldn;
				found_arr = 0;
				goto need_sysquery;
			}

			found_arr++;
			nsa = ina_get(dp->d_data);
			/* don't put in duplicates */
			qs = qp->q_addr;
			for (i = 0; i < (u_int)n; i++, qs++)
				if (ina_equal(qs->ns_addr.sin_addr, nsa))
					goto skipaddr;
			qs->ns_addr.sin_family = AF_INET;
			qs->ns_addr.sin_port = ns_port;
			qs->ns_addr.sin_addr = nsa;
			qp->q_keys[n] = NULL;
			qs->ns = nsdp;
			qs->nsdata = dp;
			qs->forwarder = 0;
			qs->noedns = dp->d_noedns;
			qs->nretry = 0;
			/*
			 * If this A RR has no RTT, initialize its RTT to a
			 * small random value.
			 */
			if (dp->d_nstime == 0)
				dp->d_nstime = 1 +
					(int)(25.0*rand()/(RAND_MAX + 1.0));
			/*
			 * if we are being asked to fwd a query whose
			 * nameserver list includes our own name/address(es),
			 * then we have detected a lame delegation and rather
			 * than melt down the network and hose down the other
			 * servers (who will hose us in return), we'll return
			 * -1 here which will cause SERVFAIL to be sent to
			 * the client's resolver which will hopefully then
			 * shut up.
			 *
			 * (originally done in nsContainsUs by vix@dec mar92;
			 * moved into nslookup by apb@und jan1993)
			 *
			 * try to limp along instead of denying service
			 * gdonl mar96
			 */
			if (aIsUs(nsa)) {
			    static const char *complaint =
					 "contains our address";
			    nslookupComplain(sysloginfo, syslogdname,
					     complaint, dname, dp, nsdp);
			    continue;
			}
			/*
			 * If we want to forward to a host that asked us
			 * this question then either we or they are sick
			 * (unless they asked from some port other than
			 * their nameserver port).  (apb@und jan1993)
			 *
			 * try to limp along instead of denying service
			 * gdonl mar96
			 */
			if (memcmp(&qp->q_from, &qs->ns_addr,
				   sizeof(qp->q_from)) == 0)
			{
			    static const char *complaint = "forwarding loop";
			    nslookupComplain(sysloginfo, syslogdname,
					     complaint, dname, dp, nsdp);
			    continue;
			}
#ifdef BOGUSNS
			/*
			 * Don't forward queries to bogus servers.  Note
			 * that this is unlike the previous tests, which
			 * are fatal to the query.  Here we just skip the
			 * server, which is only fatal if it's the last
			 * server.  Note also that we antialias here -- all
			 * A RR's of a server are considered the same server,
			 * and if any of them is bogus we skip the whole
			 * server.  Those of you using multiple A RR's to
			 * load-balance your servers will (rightfully) lose
			 * here.  But (unfortunately) only if they are bogus.
			 */
			if (ip_match_address(bogus_nameservers, nsa) > 0)
				goto skipserver;
#endif
			if (server_options->blackhole_acl != NULL && 
			    ip_match_address(server_options->blackhole_acl,
					     nsa) == 1)
				continue;

			n++;
			if (n >= NSMAX)
				break;
 skipaddr:
			(void)NULL;
		}
		ns_debug(ns_log_default, 8, "nslookup: %d ns addrs", n);
 need_sysquery:
		if (found_arr == 0) {
			potential_ns++;
			if (qp->q_distance < NS_MAX_DISTANCE)
				(void) sysquery(dname, class, T_A, NULL, NULL,
						0, ns_port, QUERY,
						qp->q_distance + 1);
		}
 skipserver:
		(void)NULL;
	}
	ns_debug(ns_log_default, 3, "nslookup: %d ns addrs total", n);
	qp->q_naddr = n;
	if (n == 0 && potential_ns == 0 && !NS_ZFWDTAB(qp->q_fzone)) {
		static const char *complaint = "No possible A RRs";
		if (lame_ns != 0)
			complaint = "All possible A RR's lame";
		if (sysloginfo && syslogdname &&
		    !haveComplained((u_long)syslogdname, (u_long)complaint))
		{
			ns_info(ns_log_default, "%s: query(%s) %s",
				sysloginfo, syslogdname, complaint);
		}
		return ((lame_ns == 0) ? -1 : -2);
	}
	/* Update the refcounts before the sort. */
	for (i = naddr; i < (u_int)n; i++) {
		DRCNTINC(qp->q_addr[i].nsdata);
		DRCNTINC(qp->q_addr[i].ns);
	}
	/* Just sort the NS RR's we added, since the forwarders may
	 * be ahead of us (naddr > 0)
	 */
	if (n > naddr) {
		qsort((char *)(qp->q_addr+naddr), n-naddr, sizeof(struct qserv),
		      (int (*)(const void *, const void *))qcomp);
	}
	return (n - naddr);
}

/*
 * qcomp - compare two NS addresses, and return a negative, zero, or
 *	   positive value depending on whether the first NS address is
 *	   "better than", "equally good as", or "inferior to" the second
 *	   NS address.
 *
 * How "goodness" is defined (for the purposes of this routine):
 *  - If the estimated round trip times differ by an amount deemed significant
 *    then the one with the smaller estimate is preferred; else
 *  - If we can determine which one is topologically closer then the
 *    closer one is preferred; else
 *  - The one with the smaller estimated round trip time is preferred
 *    (zero is returned if the two estimates are identical).
 *
 * How "topological closeness" is defined (for the purposes of this routine):
 *    Ideally, named could consult some magic map of the Internet and
 *    determine the length of the path to an arbitrary destination.  Sadly,
 *    no such magic map exists.  However, named does have a little bit of
 *    topological information in the form of the sortlist (which includes
 *    the directly connected subnet(s), the directly connected net(s), and
 *    any additional nets that the administrator has added using the "sortlist"
 *    directive in the bootfile.  Thus, if only one of the addresses matches
 *    something in the sortlist then it is considered to be topologically
 *    closer.  If both match, but match different entries in the sortlist,
 *    then the one that matches the entry closer to the beginning of the
 *    sorlist is considered to be topologically closer.  In all other cases,
 *    topological closeness is ignored because it's either indeterminate or
 *    equal.
 *
 * How times are compared:
 *    Both times are rounded to the closest multiple of the NOISE constant
 *    defined below and then compared.  If the rounded values are equal
 *    then the difference in the times is deemed insignificant.  Rounding
 *    is used instead of merely taking the absolute value of the difference
 *    because doing the latter would make the ordering defined by this
 *    routine be incomplete in the mathematical sense (e.g. A > B and
 *    B > C would not imply A > C).  The mathematics are important in
 *    practice to avoid core dumps in qsort().
 *
 * XXX: this doesn't solve the European root nameserver problem very well.
 * XXX: we should detect and mark as inferior nameservers that give bogus
 *      answers
 *
 * (this was originally vixie's stuff but almquist fixed fatal bugs in it
 * and wrote the above documentation)
 */

/*
 * RTT delta deemed to be significant, in milliseconds.  With the current
 * definition of RTTROUND it must be a power of 2.
 */
#define NOISE 64

#define RTTROUND(rtt) (((rtt) + (NOISE >> 1)) & ~(NOISE - 1))

int
qcomp(struct qserv *qs1, struct qserv *qs2) {
	u_int rtt1, rtt2, rttr1, rttr2;

	if (qs1->nsdata == NULL) {
		rtt1 = 0;
		rttr1 = 0;
	} else {
		rtt1 = qs1->nsdata->d_nstime;
		rttr1 = RTTROUND(rtt1);
	}
	if (qs2->nsdata == NULL) {
		rtt2 = 0;
		rttr2 = 0;
	} else {
		rtt2 = qs2->nsdata->d_nstime;
		rttr2 = RTTROUND(rtt2);
	}

#ifdef DEBUG
	if (debug >= 10) {
		char t[sizeof "255.255.255.255"];

		strcpy(t, inet_ntoa(qs1->ns_addr.sin_addr));
		ns_debug(ns_log_default, 10,
			 "qcomp(%s, %s) %u (%u) - %u (%u) = %u",
			 t, inet_ntoa(qs2->ns_addr.sin_addr),
			 rtt1, rttr1, rtt2, rttr2, rtt1 - rtt2);
	}
#endif
	if (rttr1 == rttr2) {
		int pos1, pos2, pdiff;

		pos1 = distance_of_address(server_options->topology,
					   qs1->ns_addr.sin_addr);
		pos2 = distance_of_address(server_options->topology,
					   qs2->ns_addr.sin_addr);
		pdiff = pos1 - pos2;
		ns_debug(ns_log_default, 10, "\tpos1=%d, pos2=%d", pos1, pos2);
		if (pdiff != 0)
			return (pdiff);
	}
	return (rtt1 - rtt2);
}
#undef RTTROUND

/*
 * Arrange that forwarded query (qp) is retried after t seconds.
 * Query list will be sorted after z_time is updated.
 */
void
schedretry(struct qinfo *qp, time_t t) {
	struct qinfo *qp1, *qp2;

	ns_debug(ns_log_default, 4, "schedretry(%p, %ld sec)", qp, (long)t);
	if (qp->q_time)
		ns_debug(ns_log_default, 4,
			 "WARNING: schedretry(%#lx, %ld) q_time already %ld",
			 (u_long)qp, (long)t, (long)qp->q_time);
	gettime(&tt);
	t += (u_long) tt.tv_sec;
	qp->q_time = t;

	if ((qp1 = retryqp) == NULL) {
		retryqp = qp;
		qp->q_next = NULL;
		goto done;
	}
	if (t < qp1->q_time) {
		qp->q_next = qp1;
		retryqp = qp;
		goto done;
	}
	while ((qp2 = qp1->q_next) != NULL && qp2->q_time < t)
		qp1 = qp2;
	qp1->q_next = qp;
	qp->q_next = qp2;
 done:
	reset_retrytimer();
}

/*
 * Unsched is called to remove a forwarded query entry.
 */
void
unsched(struct qinfo *qp) {
	struct qinfo *np;

	ns_debug(ns_log_default, 3, "unsched(%#lx, %d)",
		 (u_long)qp, ntohs(qp->q_id));
	if (retryqp == qp) {
		retryqp = qp->q_next;
	} else {
		for (np = retryqp; np->q_next != NULL; np = np->q_next) {
			if (np->q_next != qp)
				continue;
			np->q_next = qp->q_next;	/* dequeue */
			break;
		}
	}
	qp->q_next = NULL;		/* sanity check */
	qp->q_time = 0;
	reset_retrytimer();
}

void
reset_retrytimer() {
	static evTimerID id;

	if (retry_timer_set) {
		(void) evClearTimer(ev, id);
		retry_timer_set = 0;
	}

	if (retryqp) {
		evSetTimer(ev, retrytimer, NULL,
			   evConsTime(retryqp->q_time, 0),
			   evConsTime(0, 0), &id);
		retry_timer_set = 1;
	} else
		memset(&id, 0, sizeof id);
}

void
retrytimer(evContext ctx, void *uap, struct timespec due,
	   struct timespec ival) {

	UNUSED(ctx);
	UNUSED(uap);
	UNUSED(due);
	UNUSED(ival);

	retry_timer_set = 0;
	retry(retryqp, 0);
}

/*
 * Retry is called to retransmit query 'qp'.
 */
void
retry(struct qinfo *qp, int samehost) {
	int n, has_tsig, oldqlen = 0;
	HEADER *hp;
	struct sockaddr_in *nsa;
	int sendto_errno = 0;
	u_char *oldqbuf = NULL;
	u_char *smsg = NULL;
	int smsglen, smsgsize = 0, siglen;
	u_char sig[TSIG_SIG_SIZE];
	DST_KEY *key;

	ns_debug(ns_log_default, 3, "retry(%#lx) id=%d", (u_long)qp,
		 ntohs(qp->q_id));

	if (qp->q_msg == NULL) {
		qremove(qp);
		return;
	}

	if (qp->q_expire < tt.tv_sec) {
		ns_debug(ns_log_default, 1,
		     "retry(%#lx): expired @ %lu (%d secs before now (%lu))",
			 (u_long)qp, (u_long)qp->q_expire,
			 (int)(tt.tv_sec - qp->q_expire),
			 (u_long)tt.tv_sec);
		goto fail;
	}

	/* Try next address. */
	n = qp->q_curaddr;
	if (samehost) {
		qp->q_addr[n].nretry++;
		if (qp->q_addr[n].nretry < MAXRETRY)
			goto found;
		qp->q_addr[n].nretry--;
	}

	if (qp->q_naddr > 0) {
		qp->q_addr[n].noedns = 1;
		++qp->q_addr[n].nretry;
		do {
			if (++n >= (int)qp->q_naddr)
				n = 0;
			if ((qp->q_flags & Q_ZSERIAL) != 0 &&
			    qp->q_addr[n].serial != 0)
				continue;
			if (qp->q_addr[n].nretry < MAXRETRY)
				goto found;
		} while (n != qp->q_curaddr);
		if ((qp->q_flags & Q_ZSERIAL) != 0) {
			qremove(qp);
			return;
		}
	}
 fail:
	/*
	 * Give up. Can't reach destination.
	 */
	hp = (HEADER *)(qp->q_cmsg ? qp->q_cmsg : qp->q_msg);
	if ((qp->q_flags & Q_PRIMING) != 0) {
		/* Can't give up priming */
		if (qp->q_expire < tt.tv_sec) {
			/*
			 * The query has expired.  Reset it and retry from
			 * the beginning.
			 */
			hp->rcode = NOERROR;
			hp->qr = hp->aa = 0;
			for (n = 0; n < (int)qp->q_naddr; n++)
				qp->q_addr[n].nretry = 0;
			n = 0;
			qp->q_expire = tt.tv_sec + RETRY_TIMEOUT*2;
			goto found;
		}
		/*
		 * The query hasn't expired yet; it probably ran out
		 * of servers or forwarders.  Wait up to 60 seconds
		 * past the expire time.
		 */
		unsched(qp);
		schedretry(qp, (time_t)(qp->q_expire - tt.tv_sec + 60));
		return;
	}
	ns_debug(ns_log_default, 5, "give up");
	if ((qp->q_flags & Q_SYSTEM) == 0) {
		n = (qp->q_cmsg ? qp->q_cmsglen : qp->q_msglen);
		hp->id = qp->q_id;
		hp->qr = 1;
		hp->ra = (NS_OPTION_P(OPTION_NORECURSE) == 0);
		hp->rd = 1;
		hp->rcode = SERVFAIL;
#ifdef DEBUG
		if (debug >= 10)
			res_pquery(&res, qp->q_msg, n,
				    log_get_stream(packet_channel));
#endif
		if (send_msg((u_char *)hp, n, qp)) {
			ns_debug(ns_log_default, 1,
				 "gave up retry(%#lx) nsid=%d id=%d",
				 (u_long)qp,
				 ntohs(qp->q_nsid), ntohs(qp->q_id));
		}
		if (NS_OPTION_P(OPTION_HOSTSTATS))
			nameserIncr(qp->q_from.sin_addr, nssSentFail);
	}
	qremove(qp);
	return;

 found:
	if (qp->q_addr[n].nretry == 0)
		qp->q_addr[n].stime = tt;
	qp->q_curaddr = n;
	hp = (HEADER *)qp->q_msg;
	hp->rd = (qp->q_addr[n].forwarder ? 1 : 0);
	nsa = Q_NEXTADDR(qp, n);
	ns_debug(ns_log_default, 1,
		 "%s(addr=%d n=%d) -> [%s].%d ds=%d nsid=%d id=%d %dms",
		 (qp->q_addr[n].forwarder ? "reforw" : "resend"),
		 n, qp->q_addr[n].nretry,
		 inet_ntoa(nsa->sin_addr),
		 ntohs(nsa->sin_port), ds,
		 ntohs(qp->q_nsid), ntohs(qp->q_id),
		 (qp->q_addr[n].nsdata != 0)
		 ? qp->q_addr[n].nsdata->d_nstime
		 : (-1));
#ifdef DEBUG
	if (debug >= 10)
		res_pquery(&res, qp->q_msg, qp->q_msglen,
			    log_get_stream(packet_channel));
#endif
	key = qp->q_keys[n];
        if (key == NULL)
                key = qp->q_keys[n] = tsig_key_from_addr(nsa->sin_addr);
	if (key != NULL || !qp->q_addr[n].noedns) {
		smsgsize = qp->q_msglen + TSIG_BUF_SIZE + 11;
		smsg = memget(smsgsize);
		smsglen = qp->q_msglen;
		siglen = sizeof(sig);
		memcpy(smsg, qp->q_msg, qp->q_msglen);
	}

	if (!qp->q_addr[n].noedns)
		smsglen += ns_add_opt(smsg, smsg + smsglen, smsgsize, 0, 0,
				      EDNS_MESSAGE_SZ, 0, NULL, 0);

	if (key != NULL) {
		n = ns_sign(smsg, &smsglen, smsgsize, NOERROR, key, NULL, 0,
			    sig, &siglen, 0);
		if (n == 0) {
			has_tsig = 1;
			free_tsig(qp->q_nstsig);
			qp->q_nstsig = new_tsig(key, sig, siglen);
		} else {
			has_tsig = 0;
			free_tsig(qp->q_nstsig);
			qp->q_nstsig = NULL;
			INSIST(0);
		}
	} else {
		has_tsig = 0;
		free_tsig(qp->q_nstsig);
		qp->q_nstsig = NULL;
	}

	if (smsg != NULL) {
		oldqbuf = qp->q_msg;
		oldqlen = qp->q_msglen;
		qp->q_msglen = smsglen;
		qp->q_msg = smsg;
	}

	if (qp->q_flags & Q_USEVC) {
		if (tcp_send(qp) != NOERROR)
			ns_debug(ns_log_default, 3,
				 "error resending tcp msg: %s",
				 strerror(errno));
	} else if (sendto(ds, (char*)qp->q_msg, qp->q_msglen, 0,
		   (struct sockaddr *)nsa,
		   sizeof(struct sockaddr_in)) < 0)
	{
		sendto_errno = errno;
		ns_debug(ns_log_default, 3, "error resending msg: %s",
			 strerror(errno));
	}
	if (smsgsize != 0) {
		memput(smsg, smsgsize);
		qp->q_msg = oldqbuf;
		qp->q_msglen = oldqlen;
	}
	hp->rd = 1;	/* leave set to 1 for dup detection */
	nameserIncr(nsa->sin_addr, nssSentDupQ);
	unsched(qp);
	switch (sendto_errno) {
	case ENETDOWN:
	case ENETUNREACH:
	case EHOSTDOWN:
	case EHOSTUNREACH:
		schedretry(qp, (time_t) 0);
		return;
	}
	schedretry(qp, retrytime(qp));
}

/*
 * Compute retry time for the next server for a query.
 * Use a minimum time of RETRYBASE (4 sec.) or twice the estimated
 * service time; * back off exponentially on retries, but place a 45-sec.
 * ceiling on retry times for now.  (This is because we don't hold a reference
 * on servers or their addresses, and we have to finish before they time out.)
 */
time_t
retrytime(struct qinfo *qp) {
	time_t t, u, v;
	struct qserv *ns = &qp->q_addr[qp->q_curaddr];

	if (ns->nsdata != NULL)
		t = (time_t) MAX(RETRYBASE, 2 * ns->nsdata->d_nstime / 1000);
	else
		t = (time_t) RETRYBASE;
	u = t << ns->nretry;
	v = MIN(u, RETRY_TIMEOUT);	/* max. retry timeout for now */
	ns_debug(ns_log_default, 3,
		 "retrytime: nstime%ldms t%ld nretry%ld u%ld : v%ld",
		 ns->nsdata ? (long)(ns->nsdata->d_nstime / 1000) : (long)-1,
		 (long)t, (long)ns->nretry, (long)u, (long)v);
	return (v);
}

void
qflush() {
	while (nsqhead)
		qremove(nsqhead);
	nsqhead = NULL;
	priming = 0;
}

void
qremove(struct qinfo *qp) {
	ns_debug(ns_log_default, 3, "qremove(%#lx)", (u_long)qp);
	
	if ((qp->q_flags & Q_ZSERIAL) != 0)
		qserial_answer(qp);
	unsched(qp);
	ns_freeqry(qp);
}

struct qinfo *
qfindid(u_int16_t id) {
	struct qinfo *qp;

	for (qp = nsqhead; qp != NULL; qp = qp->q_link)
		if (qp->q_nsid == id)
			break;
	ns_debug(ns_log_default, 3, "qfindid(%d) -> %#lx", ntohs(id),
		 (u_long)qp);
	return (qp);
}

struct qinfo *
qnew(const char *name, int class, int type, int forward) {
	struct qinfo *qp;
	const char *s;
	int escape = 0;

	qp = (struct qinfo *)memget(sizeof *qp);
	if (qp == NULL)
		ns_panic(ns_log_default, 1, "qnew: memget failed");
	memset(qp, 0, sizeof *qp);
	ns_debug(ns_log_default, 5, "qnew(%#lx)", (u_long)qp);
#ifdef BIND_NOTIFY
	qp->q_notifyzone = DB_Z_CACHE;
#endif
	qp->q_link = nsqhead;
	nsqhead = qp;
	qp->q_name = savestr(name, 1);
	qp->q_class = (u_int16_t)class;
	qp->q_type = (u_int16_t)type;
	qp->q_flags = 0;
	s = name;
	qp->q_fzone = NULL;
	for (;forward;) {		/* find forwarding zone, if any */
		if ((qp->q_fzone = find_zone(s, class)) != NULL &&
		    (qp->q_fzone->z_flags & Z_FORWARD_SET) != 0)
			break;
		qp->q_fzone = NULL;
		if (*s == '\0')
			break;
		while (*s != '\0' && (escape || *s != '.')) {
			escape = escape ? 0 : (*s == '\\');
			s++;
		}
		if (*s != '\0')
			s++;
	}
	return (qp);
}

void
ns_freeqns(struct qinfo *qp) {
	unsigned int i;

	for (i = 0 ; i < qp->q_naddr ; i++) {
		if (qp->q_addr[i].ns != NULL)
			db_detach(&qp->q_addr[i].ns);
		if (qp->q_addr[i].nsdata != NULL)
			db_detach(&qp->q_addr[i].nsdata);
	}
}

void
ns_freeqry(struct qinfo *qp) {
	struct qinfo *np;

	ns_debug(ns_log_default, 3, "ns_freeqry(%#lx)", (u_long)qp);
	if (qp->q_next)
		ns_debug(ns_log_default, 1,
			 "WARNING: ns_freeqry of linked ptr %#lx", (u_long)qp);
	if (qp->q_msg != NULL)
		memput(qp->q_msg, qp->q_msgsize);
 	if (qp->q_cmsg != NULL)
		memput(qp->q_cmsg, qp->q_cmsgsize);
	if (qp->q_domain != NULL)
		qp->q_domain = freestr(qp->q_domain);
	if (qp->q_name != NULL)
		qp->q_name = freestr(qp->q_name);
	if (qp->q_tsig != NULL)
		memput(qp->q_tsig, sizeof(struct tsig_record));
	if (qp->q_nstsig != NULL)
		memput(qp->q_nstsig, sizeof(struct tsig_record));
	ns_freeqns(qp);
	if (nsqhead == qp)
		nsqhead = qp->q_link;
	else {
		for(np = nsqhead;
		    np->q_link != NULL;
		    np = np->q_link) {
			if (np->q_link != qp)
				continue;
			np->q_link = qp->q_link;	/* dequeue */
			break;
		}
	}
	memput(qp, sizeof *qp);
}

void
nsfwdadd(struct qinfo *qp, struct fwdinfo *fwd) {
	int i, n;
	struct qserv *qs;

	n = qp->q_naddr;
	while (fwd != NULL && n < NSMAX) {
		qs = qp->q_addr;
		for (i = 0; i < n; i++, qs++)
			if (ina_equal(qs->ns_addr.sin_addr,
				      fwd->fwddata->fwdaddr.sin_addr))
				goto nextfwd;
		qs->ns_addr = fwd->fwddata->fwdaddr;
		qs->ns = fwd->fwddata->ns;
		qs->nsdata = fwd->fwddata->nsdata;
		qs->forwarder = 1;
		qs->noedns = fwd->fwddata->nsdata->d_noedns;
		qs->nretry = 0;
		n++;
 nextfwd:
		fwd = fwd->next;
	}

	/* Update the refcounts before the sort. */
	for (i = qp->q_naddr; i < n; i++) {
		DRCNTINC(qp->q_addr[i].nsdata);
		DRCNTINC(qp->q_addr[i].ns);
	}
	qp->q_naddr = n;
	if (n > 1) {
		qsort((char *)qp->q_addr, n, sizeof(struct qserv),
		      (int (*)(const void *, const void *))qcomp);
	}
}
