/*
 * Copyright (c) 1986, 1988, 1990 Regents of the University of California.
 * All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#ifndef lint
static char sccsid[] = "@(#)ns_resp.c	4.65 (Berkeley) 3/3/91";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <syslog.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <stdio.h>
#include "ns.h"
#include "db.h"

extern	int	debug;
extern	FILE	*ddt;
extern	int errno;
extern	u_char *dnptrs[];
extern	time_t retrytime();
extern	struct	fwdinfo *fwdtab;
extern	struct	sockaddr_in from_addr;	/* Source addr of last packet */
extern	int needs_prime_cache;
extern	int priming;

struct qinfo *sysquery();

ns_resp(msg, msglen)
	u_char *msg;
	int msglen;
{
	register struct qinfo *qp;
	register HEADER *hp;
	register struct qserv *qs;
	register struct databuf *ns, *ns2;
	register u_char *cp;
	struct	databuf *nsp[NSMAX], **nspp;
	int i, c, n, ancount, aucount, nscount, arcount;
	int type, class, dbflags;
	int cname = 0; /* flag for processing cname response */
	int count, founddata, foundname;
	int buflen;
	int newmsglen;
	char name[MAXDNAME], *dname;
	char *fname;
	u_char newmsg[BUFSIZ];
	u_char **dpp, *tp;
	time_t rtrip;

	struct hashbuf *htp;
	struct namebuf *np;
	struct netinfo *lp;
	extern struct netinfo *local();
	extern int nsid;
	extern int addcount;
	struct fwdinfo *fwd;

#ifdef STATS
	stats[S_RESPONSES].cnt++;
#endif
	hp = (HEADER *) msg;
	if ((qp = qfindid(hp->id)) == NULL ) {
#ifdef DEBUG
		if (debug > 1)
			fprintf(ddt,"DUP? dropped (id %d)\n", ntohs(hp->id));
#endif
#ifdef STATS
		stats[S_DUPRESP].cnt++;
#endif
		return;
	}

#ifdef DEBUG
	if (debug >= 2)
		fprintf(ddt,"%s response nsid=%d id=%d\n",
			qp->q_system ? "SYSTEM" : "USER",
			ntohs(qp->q_nsid), ntohs(qp->q_id));
#endif

	/*
	 *  Here we handle bad responses from servers.
	 *  Several possibilities come to mind:
	 *	The server is sick and returns SERVFAIL
	 *	The server returns some garbage opcode (its sick)
	 *	The server can't understand our query and return FORMERR
	 *  In all these cases, we simply drop the packet and force
	 *  a retry.  This will make him look bad due to unresponsiveness.
	 *  Be sure not to include authoritative NXDOMAIN
	 */
	if ((hp->rcode != NOERROR && hp->rcode != NXDOMAIN)
	    || (hp->rcode == NXDOMAIN && !hp->aa)
	    || hp->opcode != QUERY) {
#ifdef DEBUG
		if (debug >= 2)
			fprintf(ddt,"resp: error (ret %d, op %d), dropped\n",
				hp->rcode, hp->opcode);
#endif
#ifdef STATS
		stats[S_BADRESPONSES].cnt++;
#endif
		return;
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
		doupdate(qp->q_msg, qp->q_msglen, qp->q_msg + sizeof(HEADER),
			 0, (struct databuf *)0, 0);
#ifdef DEBUG
		if (debug >= 3)
			fprintf(ddt,"resp: leaving, UPDATE*\n");
#endif
		/* return code filled in by doupdate */
		goto return_msg;
	}
#endif ALLOW_UPDATES

	/*
	 * Determine if the response came from a forwarder.  Packets from
	 * anyplace not listed as a forwarder or as a server to whom we
	 * might have forwarded the query will be dropped.
	 */
	for (fwd = fwdtab; fwd != (struct fwdinfo *)NULL; fwd = fwd->next)
		if (bcmp((char *)&fwd->fwdaddr.sin_addr, &from_addr.sin_addr,
		    sizeof(struct in_addr)) == 0)
			break;
	/*
	 * If we were using nameservers, find the qinfo pointer and update
	 * the rtt and fact that we have called on this server before.
	 */
	if (fwd == (struct fwdinfo *)NULL) {
		struct timeval *stp;

		for (n = 0, qs = qp->q_addr; n < qp->q_naddr; n++, qs++)
			if (bcmp((char *)&qs->ns_addr.sin_addr,
			    &from_addr.sin_addr, sizeof(struct in_addr)) == 0)
				break;
		if (n >= qp->q_naddr) {
#ifdef DEBUG
			if (debug)
			    fprintf(ddt, "Response from unexpected source %s\n",
				inet_ntoa(from_addr.sin_addr));
#endif DEBUG
#ifdef STATS
			stats[S_MARTIANS].cnt++;
#endif
			/* 
			 * We don't know who this response came from so it
			 * gets dropped on the floor.
			 */
			return;
		}
		stp = &qs->stime;

		/* Handle response from different (untried) interface */
		if (stp->tv_sec == 0) {
			ns = qs->ns;
			while (qs > qp->q_addr &&
			    (qs->stime.tv_sec == 0 || qs->ns != ns))
				qs--;
			*stp = qs->stime;
#ifdef DEBUG
			if (debug)
			    fprintf(ddt,
			    "Response from unused address %s, assuming %s\n",
				inet_ntoa(from_addr.sin_addr),
				inet_ntoa(qs->ns_addr.sin_addr));
#endif DEBUG
		}

		/* compute query round trip time */
		rtrip = ((tt.tv_sec - stp->tv_sec) * 1000 +
		    (tt.tv_usec - stp->tv_usec) / 1000);
		
#ifdef DEBUG
		if (debug > 2)
			fprintf(ddt,"stime %d/%d  now %d/%d rtt %d\n",
			    stp->tv_sec, stp->tv_usec,
			    tt.tv_sec, tt.tv_usec, rtrip);
#endif
		/* prevent floating point overflow, limit to 1000 sec */
		if (rtrip > 1000000)
				rtrip = 1000000;
		ns = qs->nsdata;
		/*
		 * Don't update nstime if this doesn't look
		 * like an address databuf now.			XXX
		 */
		if (ns->d_type == T_A && ns->d_class == qs->ns->d_class) {
			if (ns->d_nstime == 0)
				ns->d_nstime = (u_long)rtrip;
			else
				ns->d_nstime = ns->d_nstime * ALPHA +
				    (1-ALPHA) * (u_long)rtrip;
			/* prevent floating point overflow, limit to 1000 sec */
			if (ns->d_nstime > 1000000)
				ns->d_nstime = 1000000;
		}

		/*
		 * Record the source so that we do not use this NS again.
		 */
		if(qp->q_nusedns < NSMAX) {
			qp->q_usedns[qp->q_nusedns++] = qs->ns;
#ifdef DEBUG
			if(debug > 1)
			    fprintf(ddt, "NS #%d addr %s used, rtt %d\n",
				n, inet_ntoa(qs->ns_addr.sin_addr),
				ns->d_nstime);
#endif DEBUG
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
			if (ns2 == ns)
			    continue;
			if (ns2->d_type != T_A ||
			    ns2->d_class != qs->ns->d_class)	/* XXX */
				continue;
			if (qs->stime.tv_sec) {
			    if (ns2->d_nstime == 0)
				ns2->d_nstime = rtrip * BETA;
			    else
				ns2->d_nstime =
				    ns2->d_nstime * BETA + (1-ALPHA) * rtrip;
			    if (ns2->d_nstime > 1000000)
				ns2->d_nstime = 1000000;
			} else
			    ns2->d_nstime = ns2->d_nstime * GAMMA;
#ifdef DEBUG
			if(debug > 1)
			    fprintf(ddt, "NS #%d %s rtt now %d\n", n,
				inet_ntoa(qs->ns_addr.sin_addr),
				ns2->d_nstime);
#endif DEBUG
		}
	}

	/*
	 * Skip query section
	 */
	addcount = 0;
	cp = msg + sizeof(HEADER);
	dpp = dnptrs;
	*dpp++ = msg;
	if ((*cp & INDIR_MASK) == 0)
		*dpp++ = cp;
	*dpp = NULL;
	if (hp->qdcount) {
		n = dn_skipname(cp, msg + msglen);
	    	if (n <= 0)
			goto formerr;
		cp += n;
		GETSHORT(type, cp);
		GETSHORT(class, cp);
		if (cp - msg > msglen)
			goto formerr;
	}

	/*
	 * Save answers, authority, and additional records for future use.
	 */
	ancount = ntohs(hp->ancount);
	aucount = ntohs(hp->nscount);
	arcount = ntohs(hp->arcount);
	nscount = 0;
	tp = cp;
#ifdef DEBUG
	if (debug >= 3)
		fprintf(ddt,"resp: ancount %d, aucount %d, arcount %d\n",
			ancount, aucount, arcount);
#endif

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
			if (tp - msg >= msglen)
				goto formerr;
			n = dn_skipname(tp, msg + msglen);
			if (n <= 0)
				goto formerr;
			tp += n;  		/* name */
			GETSHORT(i, tp);	/* type */
			tp += sizeof(u_short);	/* class */
			tp += sizeof(u_long);	/* ttl */
			GETSHORT(count, tp); 	/* dlen */
			if (tp - msg > msglen - count)
				goto formerr;
			tp += count;
			if (ancount && i == T_CNAME) {
				cname++;
#ifdef DEBUG
				if (debug)
					fprintf(ddt,"CNAME - needs more processing\n");
#endif
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

	/*
	 * Add the info received in the response to the Data Base
	 */
	c = ancount + aucount + arcount;
#ifdef notdef
	/*
	 * If the request was for a CNAME that doesn't exist,
	 * but the name is valid, fetch any other data for the name.
	 * DON'T do this now, as it will requery if data are already
	 * in the cache (maybe later with negative caching).
	 */
	if (hp->qdcount && type == T_CNAME && c == 0 && hp->rcode == NOERROR &&
	   !qp->q_system) {
#ifdef DEBUG
		if (debug >= 3)
			fprintf(ddt,"resp: leaving, no CNAME\n");
#endif
		/* Cause us to put it in the cache later */
		prime(class, T_ANY, qp);

		/* Nothing to store, just give user the answer */
		goto return_msg;
	}
#endif /* notdef */

	nspp = nsp;
	if (qp->q_system)
		dbflags = DB_NOTAUTH | DB_NODATA;
	else
		dbflags = DB_NOTAUTH | DB_NODATA | DB_NOHINTS;
	for (i = 0; i < c; i++) {
		struct databuf *ns3;

		if (cp >= msg + msglen)
			goto formerr;
		ns3 = 0;
		if ((n = doupdate(msg, msglen, cp, 0, &ns3, dbflags)) < 0) {
#ifdef DEBUG
			if (debug)
			    fprintf(ddt,"resp: leaving, doupdate failed\n");
#endif
			/* return code filled in by doupdate */
			goto return_msg;
		}
		/*
		 * Remember nameservers from the authority section
		 * for referrals.
		 * (This is usually overwritten by findns below(?). XXX
		 */
		if (ns3 && i >= ancount && i < ancount + aucount &&
		    nspp < &nsp[NSMAX-1])
			*nspp++ = ns3;
		cp += n;
	}

	if (qp->q_system && ancount) {
		if (qp->q_system == PRIMING_CACHE)
			check_root();
#ifdef DEBUG
		if (debug > 2)
			fprintf(ddt,"resp: leaving, SYSQUERY ancount %d\n", ancount);
#endif
		qremove(qp);
		return;
	}

	if (cp > msg + msglen)
		goto formerr;

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
#ifdef DEBUG
		if (debug >= 3)
			fprintf(ddt,"resp: got as much answer as there is\n");
#endif
		goto return_msg;
	}

	/*
	 * Eventually we will want to cache this negative answer.
	 */
	if (ancount == 0 && nscount == 0 &&
	    (hp->aa || fwd || class == C_ANY)) {
		/* We have an authoritative NO */
#ifdef DEBUG
		if (debug >= 3)
			fprintf(ddt,"resp: leaving auth NO\n");
#endif
		if (qp->q_cmsglen) {
			msg = (u_char *)qp->q_cmsg;
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
	if (!cname && qp->q_cmsglen && ancount) {
#ifdef DEBUG
		if (debug)
			fprintf(ddt,"Cname second pass\n");
#endif
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
	cp = newmsg + sizeof(HEADER);
	if (cname)
		cp += dn_skipname(cp, newmsg + newmsglen) + QFIXEDSZ;
	if ((n = dn_expand(newmsg, newmsg + newmsglen,
		cp, (u_char *)dname, sizeof(name))) < 0) {
#ifdef DEBUG
		if (debug)
			fprintf(ddt,"dn_expand failed\n" );
#endif
		goto servfail;
	}
	if (!cname)
		cp += n + QFIXEDSZ;
	buflen = sizeof(newmsg) - (cp - newmsg);

try_again:
#ifdef DEBUG
	if (debug)
		fprintf(ddt,"resp: nlookup(%s) type=%d\n",dname, type);
#endif
	fname = "";
	htp = hashtab;		/* lookup relative to root */
	np = nlookup(dname, &htp, &fname, 0);
#ifdef DEBUG
	if (debug)
		fprintf(ddt,"resp: %s '%s' as '%s' (cname=%d)\n",
			np == NULL ? "missed" : "found", dname, fname, cname);
#endif
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

#ifdef DEBUG
	if (debug >= 3) {
	    fprintf(ddt,"resp: foundname = %d count = %d ", foundname, count);
	    fprintf(ddt,"founddata = %d cname = %d\n", founddata, cname);
	}
#endif

fetch_ns:
	hp->ancount = htons(hp->ancount);
	/*
 	 * Look for name servers to refer to and fill in the authority
 	 * section or record the address for forwarding the query
 	 * (recursion desired).
 	 */
	switch (findns(&np, class, nsp, &count)) {
	case NXDOMAIN:		/* shouldn't happen */
#ifdef DEBUG
		if (debug >= 3)
			fprintf(ddt,"req: leaving (%s, rcode %d)\n",
				dname, hp->rcode);
#endif
		if (!foundname)
			hp->rcode = NXDOMAIN;
		if (class != C_ANY) {
			hp->aa = 1;
			/*
			 * should return SOA if founddata == 0,
			 * but old named's are confused by an SOA
			 * in the auth. section if there's no error.
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
		hp->nscount = htons((u_short)count);
		goto return_newmsg;
	}

	/*
	 *  If we get here, we don't have the answer yet and are about
	 *  to iterate to try and get it.  First, infinite loop avoidance.
	 */
	if (qp->q_nqueries++ > MAXQUERIES) {
#ifdef DEBUG
		if (debug)
		    fprintf(ddt,"resp: MAXQUERIES exceeded (%s, class %d, type %d)\n",
			dname, class, type);
#endif
		syslog(LOG_NOTICE,
		    	    "MAXQUERIES exceeded, possible data loop in resolving (%s)",
			    dname);
		goto servfail;
	}

	/* Reset the query control structure */
	qp->q_naddr = 0;
	qp->q_curaddr = 0;
	qp->q_fwd = fwdtab;
	if (nslookup(nsp, qp) == 0) {
#ifdef DEBUG
		if (debug >= 3)
			fprintf(ddt,"resp: no addrs found for NS's\n");
#endif
		goto servfail;
	}
	for (n = 0; n < qp->q_naddr; n++)
		qp->q_addr[n].stime.tv_sec = 0;
	if (!qp->q_fwd)
		qp->q_addr[0].stime = tt;
	if (cname) {
	 	if (qp->q_cname++ == MAXCNAMES) {
#ifdef DEBUG
			if (debug >= 3)
				fprintf(ddt,"resp: leaving, MAXCNAMES exceeded\n");
#endif
			goto servfail;
	 	}
#ifdef DEBUG
	 	if (debug)
	 		fprintf(ddt,"q_cname = %d\n",qp->q_cname);
		if (debug >= 3)
		       fprintf(ddt,"resp: building recursive query; nslookup\n");
#endif
		if (qp->q_msg)
			(void) free(qp->q_msg);
		if ((qp->q_msg = malloc(BUFSIZ)) == NULL) {
#ifdef DEBUG
			if (debug)
				fprintf(ddt,"resp: malloc error\n");
#endif
			goto servfail;
		}
		qp->q_msglen = res_mkquery(QUERY, dname, class,
		    type, (char *)NULL, 0, NULL, qp->q_msg, BUFSIZ);
		hp = (HEADER *) qp->q_msg;
	    	hp->rd = 0;
	} else
		hp = (HEADER *)qp->q_msg;
	hp->id = qp->q_nsid = htons((u_short)++nsid);
	if (qp->q_fwd)
		hp->rd = 1;
	unsched(qp);
	schedretry(qp, retrytime(qp));
#ifdef DEBUG
	if (debug)
		fprintf(ddt,"resp: forw -> %s %d (%d) nsid=%d id=%d %dms\n",
			inet_ntoa(Q_NEXTADDR(qp,0)->sin_addr),
			ds, ntohs(Q_NEXTADDR(qp,0)->sin_port),
			ntohs(qp->q_nsid), ntohs(qp->q_id),
			qp->q_addr[0].nsdata->d_nstime);
	if ( debug >= 10)
		fp_query((char *)msg, ddt);
#endif
	if (sendto(ds, qp->q_msg, qp->q_msglen, 0,
		(struct sockaddr *)Q_NEXTADDR(qp,0),
		sizeof(struct sockaddr_in)) < 0) {
#ifdef DEBUG
		if (debug >= 5)
			fprintf(ddt, "sendto error = %d\n", errno);
#endif
	}
	hp->rd = 0;	/* leave set to 0 for dup detection */
#ifdef STATS
	stats[S_OUTPKTS].cnt++;
#endif
#ifdef DEBUG
	if (debug >= 3)
		fprintf(ddt,"resp: Query sent.\n");
#endif
	return;

formerr:
#ifdef DEBUG
	if (debug)
	    fprintf(ddt,"FORMERR resp() from %s size err %d, msglen %d\n",
		inet_ntoa(from_addr.sin_addr),
		cp-msg, msglen);
#endif
	syslog(LOG_INFO, "Malformed response from %s\n",
		inet_ntoa(from_addr.sin_addr));
#ifdef STATS
	stats[S_RESPFORMERR].cnt++;
#endif
	return;

return_msg:
#ifdef STATS
	stats[S_RESPOK].cnt++;
#endif
	/* The "standard" return code */
	hp->qr = 1;
	hp->id = qp->q_id;
	hp->rd = 1;
	hp->ra = 1;
	(void) send_msg(msg, msglen, qp);
	qremove(qp);
	return;

return_newmsg:
#ifdef STATS
	stats[S_RESPOK].cnt++;
#endif
	if (addcount) {
		n = doaddinfo(hp, cp, buflen);
		cp += n;
		buflen -= n;
	}

	hp->id = qp->q_id;
	hp->rd = 1;
	hp->ra = 1;
	hp->qr = 1;
	(void) send_msg(newmsg, cp - newmsg, qp);
	qremove(qp);
	return;

servfail:
#ifdef STATS
	stats[S_RESPFAIL].cnt++;
#endif
	hp = (HEADER *)(cname ? qp->q_cmsg : qp->q_msg);
	hp->rcode = SERVFAIL;
	hp->id = qp->q_id;
	hp->rd = 1;
	hp->ra = 1;
	hp->qr = 1;
	(void) send_msg((char *)hp, (cname ? qp->q_cmsglen : qp->q_msglen), qp);
	qremove(qp);
	return;
}

/*
 * Decode the resource record 'rrp' and update the database.
 * If savens is true, record pointer for forwarding queries a second time.
 */
doupdate(msg, msglen, rrp, zone, savens, flags)
	char *msg;
	u_char *rrp;
	struct databuf **savens;
	int  msglen, zone, flags;
{
	register u_char *cp;
	register int n;
	int class, type, dlen, n1;
	u_long ttl;
	struct databuf *dp;
	char dname[MAXDNAME];
	u_char *cp1;
	u_char data[BUFSIZ];
	register HEADER *hp = (HEADER *) msg;
#ifdef ALLOW_UPDATES
	int zonenum;
#endif

#ifdef DEBUG
	if (debug > 2)
		fprintf(ddt,"doupdate(zone %d, savens %x, flags %x)\n",
			zone, savens, flags);
#endif

	cp = rrp;
	if ((n = dn_expand((u_char *)msg, (u_char *)msg + msglen, cp,
	    (u_char *)dname, sizeof(dname))) < 0) {
		hp->rcode = FORMERR;
		return (-1);
	}
	cp += n;
	GETSHORT(type, cp);
	GETSHORT(class, cp);
	GETLONG(ttl, cp);
	GETSHORT(dlen, cp);
#ifdef DEBUG
	if (debug > 2)
		fprintf(ddt,"doupdate: dname %s type %d class %d ttl %d\n",
			dname, type, class, ttl);
#endif
	/*
	 * Convert the resource record data into the internal
	 * database format.
	 */
	switch (type) {
	case T_A:
	case T_WKS:
	case T_HINFO:
	case T_UINFO:
	case T_UID:
	case T_GID:
	case T_TXT:
#ifdef ALLOW_T_UNSPEC
	case T_UNSPEC:
#endif ALLOW_T_UNSPEC
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
		if ((n = dn_expand((u_char *)msg, (u_char *)msg + msglen,
		    cp, data, sizeof(data))) < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 = data;
		n = strlen((char *)data) + 1;
		break;

	case T_MINFO:
	case T_SOA:
		if ((n = dn_expand((u_char *)msg, (u_char *)msg + msglen,
		    cp, data, sizeof(data))) < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 = data + (n = strlen((char *)data) + 1);
		n1 = sizeof(data) - n;
		if (type == T_SOA)
			n1 -= 5 * sizeof(u_long);
		if ((n = dn_expand((u_char *)msg, (u_char *)msg + msglen,
		    cp, cp1, n1)) < 0) {
			hp->rcode = FORMERR;
			return (-1);
		}
		cp += n;
		cp1 += strlen((char *)cp1) + 1;
		if (type == T_SOA) {
			bcopy(cp, cp1, n = 5 * sizeof(u_long));
			cp += n;
			cp1 += n;
		}
		n = cp1 - data;
		cp1 = data;
		break;

	case T_MX:
		/* grab preference */
		bcopy(cp,data,sizeof(u_short));
		cp1 = data + sizeof(u_short);
		cp += sizeof(u_short);

		/* get name */
		if ((n = dn_expand((u_char *)msg, (u_char *)msg + msglen,
		    cp, cp1, sizeof(data) - sizeof(u_short))) < 0)
			return(-1);
		cp += n;

		/* compute end of data */
		cp1 += strlen((char *)cp1) + 1;
		/* compute size of data */
		n = cp1 - data;
		cp1 = data;
		break;

	default:
#ifdef DEBUG
		if (debug >= 3)
			fprintf(ddt,"unknown type %d\n", type);
#endif
		return ((cp - rrp) + dlen);
	}
	if (n > MAXDATA) {
#ifdef DEBUG
		if (debug)
		    fprintf(ddt,
			"update type %d: %d bytes is too much data\n",
			type, n);
#endif
		hp->rcode = NOCHANGE;	/* XXX - FORMERR ??? */
		return(-1);
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
#ifdef DEBUG
			if (debug)
			   fprintf(ddt, "UDPATE: REFUSED - SOA delete\n");
#endif
			hp->rcode = REFUSED;
			return(-1);
		}
		/*
		 * Don't check message length if doing UPDATEM/UPDATEMA,
		 * since the whole message wont have been demarshalled until
		 * we reach the code for UPDATEA
		 */
		if ( (hp->opcode == UPDATED) || (hp->opcode == UPDATEDA) ) {
			if (cp != (u_char *)(msg + msglen)) {
#ifdef DEBUG
			    if (debug)
			        fprintf(ddt,"FORMERR UPDATE message length off\n");
#endif
			    hp->rcode = FORMERR;
			    return(-1);
			}
		}
		if ((zonenum = findzone(dname, class)) == 0) { 
			hp->rcode = NXDOMAIN;
			return(-1);
		}
		if (zones[zonenum].z_state & Z_DYNADDONLY) {
			hp->rcode = NXDOMAIN;
			return(-1);
		}
		if ( (hp->opcode == UPDATED) || (hp->opcode == UPDATEM) ) {
			/* Make a dp for use in db_update, as old dp */
			dp = savedata(class, type, 0, cp1, n);
			dp->d_zone = zonenum;
			n = db_update(dname, dp, NULL, DB_MEXIST | DB_DELETE,
				      hashtab);
			if (n != OK) {
#ifdef DEBUG
				if (debug)
				    fprintf(ddt,"UPDATE: db_update failed\n");
#endif DEBUG
				free( (struct databuf *) dp);
				hp->rcode = NOCHANGE;
				return(-1);
			}
		} else {	/* UPDATEDA or UPDATEMA */
			int DeletedOne = 0;
			/* Make a dp for use in db_update, as old dp */
			dp = savedata(class, type, 0, NULL, 0);
			dp->d_zone = zonenum;
			do {	/* Loop and delete all matching RR(s) */
				n = db_update(dname, dp, NULL, DB_DELETE,
					      hashtab);
				if (n != OK)
					break;
				DeletedOne++;
			} while (1);
			free( (struct databuf *) dp);
			/* Ok for UPDATEMA not to have deleted any RRs */
			if (!DeletedOne && hp->opcode == UPDATEDA) {
#ifdef DEBUG
				if (debug)
					fprintf(ddt,"UPDATE: db_update failed\n");
#endif DEBUG
				hp->rcode = NOCHANGE;
				return(-1);
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
#ifdef DEBUG
			if (debug)
			    fprintf(ddt,"FORMERR UPDATE expand name failed\n");
#endif
			hp->rcode = FORMERR;
			return(-1);
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
#ifdef DEBUG
			if (debug)
			    fprintf(ddt,"UPDATE: too much data\n");
#endif
			hp->rcode = NOCHANGE;
			return(-1);
		}
		if (cp != (u_char *)(msg + msglen)) {
#ifdef DEBUG
			if (debug)
			    fprintf(ddt,"FORMERR UPDATE message length off\n");
#endif
			hp->rcode = FORMERR;
			return(-1);
		}
		if ((zonenum = findzone(dname, class)) == 0) { 
			hp->rcode = NXDOMAIN;
			return(-1);
		}
		if (zones[zonenum].z_state & Z_DYNADDONLY) {
			struct hashbuf *htp = hashtab;
			char *fname;
			if (nlookup(dname, &htp, &fname, 0) &&
			    !strcmp(dname, fname)) {
#ifdef	DEBUG
			    if (debug)
				fprintf(ddt,"refusing add of existing name\n");
#endif
			    hp->rcode = REFUSED;
			    return(-1);
			}
		}
		dp = savedata(class, type, ttl, cp1, n);
		dp->d_zone = zonenum;
		if ((n = db_update(dname, NULL, dp, DB_NODATA,
				   hashtab)) != OK) {
#ifdef DEBUG
			if (debug)
				fprintf(ddt,"UPDATE: db_update failed\n");
#endif
			hp->rcode = NOCHANGE;
			return (-1);
		}
		else
			return (cp - rrp);
	}
#endif ALLOW_UPDATES

	if (zone == 0)
		ttl += tt.tv_sec;
	dp = savedata(class, type, ttl, cp1, n);
	dp->d_zone = zone;
	if ((n = db_update(dname, dp, dp, flags, hashtab)) < 0) {
#ifdef DEBUG
		if (debug && (n != DATAEXISTS))
			fprintf(ddt,"update failed (%d)\n", n);
		else if (debug >= 3)
			fprintf(ddt,"update failed (DATAEXISTS)\n");
#endif
		(void) free((char *)dp);
	} else if (type == T_NS && savens != NULL)
		*savens = dp;
	return (cp - rrp);
}

send_msg(msg, msglen, qp)
	char *msg;
	int msglen;
	struct qinfo *qp;
{
	extern struct qinfo *qhead;
#ifdef DEBUG
	struct qinfo *tqp;
#endif DEBUG

	if (qp->q_system)
		return(1);
#ifdef DEBUG
	if (debug) {
		fprintf(ddt,"send_msg -> %s (%s %d %d) id=%d\n",
			inet_ntoa(qp->q_from.sin_addr), 
			qp->q_stream == QSTREAM_NULL ? "UDP" : "TCP",
			qp->q_stream == QSTREAM_NULL ? qp->q_dfd
						     : qp->q_stream->s_rfd,
			ntohs(qp->q_from.sin_port),
			ntohs(qp->q_id));
	}
	if (debug>4)
		for (tqp = qhead; tqp!=QINFO_NULL; tqp = tqp->q_link) {
		    fprintf(ddt, "qp %x q_id: %d  q_nsid: %d q_msglen: %d ",
		    	tqp, tqp->q_id,tqp->q_nsid,tqp->q_msglen);
	            fprintf(ddt,"q_naddr: %d q_curaddr: %d\n", tqp->q_naddr,
			tqp->q_curaddr);
	            fprintf(ddt,"q_next: %x q_link: %x\n", qp->q_next,
		   	 qp->q_link);
		}
	if (debug >= 10)
		fp_query(msg, ddt);
#endif DEBUG
	if (qp->q_stream == QSTREAM_NULL) {
		if (sendto(qp->q_dfd, msg, msglen, 0,
		    (struct sockaddr *)&qp->q_from, sizeof(qp->q_from)) < 0) {
#ifdef DEBUG
			if (debug)
				fprintf(ddt, "sendto error errno= %d\n",errno);
#endif
			return(1);
		}
#ifdef STATS
		stats[S_OUTPKTS].cnt++;
#endif
	} else {
		(void) writemsg(qp->q_stream->s_rfd, msg, msglen);
		sq_done(qp->q_stream);
	}
	return(0);
}

prime(class, type, oqp)
	int class, type;
	register struct qinfo *oqp;
{
	char	dname[BUFSIZ];

	if (oqp->q_msg == NULL)
		return;
	if (dn_expand((u_char *)oqp->q_msg,
	    (u_char *)oqp->q_msg + oqp->q_msglen,
	    (u_char *)oqp->q_msg + sizeof(HEADER), (u_char *)dname,
	    sizeof(dname)) < 0)
		return;
#ifdef DEBUG
	if (debug >= 2)
	       fprintf(ddt,"prime: %s\n", dname);
#endif
	(void) sysquery(dname, class, type);
}


prime_cache()
{
	register struct qinfo *qp;

#ifdef DEBUG
	if (debug)
		fprintf(ddt,"prime_cache: priming = %d\n", priming);
#endif
#ifdef STATS
	stats[S_PRIMECACHE].cnt++;
#endif
	if (!priming && fcachetab->h_tab[0] != NULL && !forward_only) {
		priming++;
		if ((qp = sysquery("", C_IN, T_NS)) == NULL)
			priming = 0;
		else
			qp->q_system = PRIMING_CACHE;
	}
	needs_prime_cache = 0;
	return;
}

struct qinfo *
sysquery(dname, class, type)
	char *dname;
	int class, type;
{
	extern struct qinfo *qhead;
	extern int nsid;
	register struct qinfo *qp, *oqp;
	register HEADER *hp;
	struct namebuf *np;
	struct databuf *nsp[NSMAX];
	struct hashbuf *htp;
	char *fname;
	int count;

#ifdef DEBUG
	if (debug > 2)
	       fprintf(ddt,"sysquery(%s, %d, %d)\n", dname, class, type);
#endif
#ifdef STATS
	stats[S_SYSQUERIES].cnt++;
#endif
	htp = hashtab;
	if (priming && dname[0] == '\0')
		np = NULL;
	else if ((np = nlookup(dname, &htp, &fname, 1)) == NULL) {
#ifdef DEBUG
		if (debug)
			fprintf(ddt,"sysquery: nlookup error on %s?\n", dname);
#endif
		return(0);
	}

	switch (findns(&np, class, nsp, &count)) {
	case NXDOMAIN:
	case SERVFAIL:
#ifdef DEBUG
		if (debug)
			fprintf(ddt,"sysquery: findns error on %s?\n", dname);
#endif
		return(0);
	}

	/* build new qinfo struct */
	qp = qnew();
	qp->q_cmsg = qp->q_msg = NULL;
	qp->q_dfd = ds;
	qp->q_fwd = fwdtab;
	qp->q_system++;

	if ((qp->q_msg = malloc(BUFSIZ)) == NULL) {
		qfree(qp);
		return(0);
	}
	qp->q_msglen = res_mkquery(QUERY, dname, class,
	    type, (char *)NULL, 0, NULL, qp->q_msg, BUFSIZ);
	hp = (HEADER *) qp->q_msg;
	hp->id = qp->q_nsid = htons((u_short)++nsid);
	hp->rd = (qp->q_fwd ? 1 : 0);

	/* First check for an already pending query for this data */
	for (oqp = qhead; oqp!=QINFO_NULL; oqp = oqp->q_link) {
		if (oqp != qp && oqp->q_msglen == qp->q_msglen &&
	     bcmp((char *)oqp->q_msg+2, qp->q_msg+2, qp->q_msglen-2) == 0) {
#ifdef DEBUG
			if (debug >= 3)
				fprintf(ddt, "sysquery: duplicate\n");
#endif
			qfree(qp);
			return(0);
		}
	}

	if (nslookup(nsp, qp) == 0) {
#ifdef DEBUG
		if (debug)
			fprintf(ddt,"resp: no addrs found for NS's\n");
#endif
		qfree(qp);
		return(0);
	}

	schedretry(qp, retrytime(qp));
	if (qp->q_fwd == 0)
		qp->q_addr[0].stime = tt;

#ifdef DEBUG
	if (debug)
	    fprintf(ddt,"sysquery: send -> %s %d (%d), nsid=%d id=%d %dms\n",
		inet_ntoa(Q_NEXTADDR(qp,0)->sin_addr),
		qp->q_dfd, ntohs(Q_NEXTADDR(qp,0)->sin_port),
		ntohs(qp->q_nsid), ntohs(qp->q_id),
		qp->q_addr[0].nsdata->d_nstime);
	if ( debug >= 10)
	    fp_query(qp->q_msg, ddt);
#endif
	if (sendto(qp->q_dfd, qp->q_msg, qp->q_msglen, 0,
	    (struct sockaddr *)Q_NEXTADDR(qp,0),
	    sizeof(struct sockaddr_in)) < 0){
#ifdef DEBUG
	    if (debug)
		fprintf(ddt, "sendto error errno= %d\n",errno);
#endif
	}
#ifdef STATS
	stats[S_OUTPKTS].cnt++;
#endif
	return(qp);
}

/*
 * Check the list of root servers after receiving a response
 * to a query for the root servers.
 */
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
#ifdef DEBUG
	if (debug)
	    fprintf(ddt,"%d root servers\n", count);
#endif
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
#ifdef DEBUG
			if (debug)
			    fprintf(ddt,"deleting old root server '%s'\n",
				dp->d_data);
#endif
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
check_ns()
{
	register struct databuf *dp, *tdp;
	register struct namebuf *np, *tnp;
	struct hashbuf *htp;
	char *dname;
	int found_arr;
	char *fname;
	time_t curtime;

#ifdef DEBUG
	if (debug >= 2)
	       fprintf(ddt,"check_ns()\n");
#endif
#ifdef STATS
	stats[S_CHECKNS].cnt++;
#endif

	curtime = (u_long) tt.tv_sec;
	for (np = hashtab->h_tab[0]; np != NULL; np = np->n_next) {
		if (np->n_dname[0] != 0)
			continue;
	        for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
	        	if (dp->d_type != T_NS)
	        	    continue;

	        	/* look for A records */
			dname = dp->d_data;
			htp = hashtab;
			tnp = nlookup(dname, &htp, &fname, 0);
			if (tnp == NULL || fname != dname) {
#ifdef DEBUG
			    if (debug >= 3)
			        fprintf(ddt,"check_ns: %s: not found %s %x\n",
					dname, fname, tnp);
#endif
			    (void) sysquery(dname, dp->d_class, T_A);
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
#ifdef DEBUG
			        if (debug >= 3)
			            fprintf(ddt,"check_ns: stale entry '%s'\n",
			                tnp->n_dname);
#endif
				/* Cache invalidate the address RR's */
				delete_all(tnp, dp->d_class, T_A);
				found_arr = 0;
			        break;
			    }
			    found_arr++;
			}
			if (!found_arr)
			    (void) sysquery(dname, dp->d_class, T_A);
	        }
	}
}

#define	MAXCLASS 255		/* belongs elsewhere */
int	norootlogged[MAXCLASS];

/*
 *  Find NS's or an SOA for the given dname (np) and fill in the
 *  nsp array.  Returns OK on success, and SERVFAIL on error.
 *  We return NXDOMAIN to indicate we are authoritative.
 */
findns(npp, class, nsp, countp)
	register struct namebuf **npp;
	struct databuf **nsp;
	int *countp;
{
	register struct namebuf *np = *npp;
	register struct databuf *dp;
	register struct	databuf **nspp;
	struct hashbuf *htp = hashtab;
	
	if (priming && (np == NULL || np->n_dname[0] == '\0'))
		htp = fcachetab;
try_again:
	if (htp == fcachetab)
		needs_prime_cache = 1;
	while (np == NULL && htp != NULL) {
#ifdef DEBUG
		if (debug > 2)
			fprintf(ddt, "findns: using %s\n", htp == hashtab ?
				"cache" : "hints");
#endif
		for (np = htp->h_tab[0]; np != NULL; np = np->n_next)
			if (np->n_dname[0] == '\0')
				break;
		htp = (htp == hashtab ? fcachetab : NULL);	/* Fallback */
	}
	while(np != NULL) {
#ifdef DEBUG
		if (debug >= 5)
			fprintf(ddt, "findns: np 0x%x\n", np);
#endif
		/* Look first for SOA records. */
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			if (dp->d_zone != 0 && match(dp, class, T_SOA)) {
#ifdef DEBUG
				if (debug >= 3)
					fprintf(ddt,"findns: SOA found\n");
#endif
				if (zones[dp->d_zone].z_auth) {
					*npp = np;
					nsp[0] = dp;
					return(NXDOMAIN);
				} else
					return (SERVFAIL);
			}
		}

		/* If no SOA records, look for NS records. */
		nspp = &nsp[0];
		*nspp = NULL;
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			if (dp->d_type != T_NS ||
			    (dp->d_class != class && class != C_ANY))
				continue;
			/*
			 * Don't use records that may become invalid to
			 * reference later when we do the rtt computation.
			 * Never delete our safety-belt information!
			 */
			if ((dp->d_zone == 0) &&
			    (dp->d_ttl < (tt.tv_sec+900)) &&
			    !(dp->d_flags & DB_F_HINT)) {
#ifdef DEBUG
				if (debug)
					fprintf(ddt,"findns: stale entry '%s'\n",
						    np->n_dname);
#endif
				/* Cache invalidate the NS RR's */
				if (dp->d_ttl < tt.tv_sec)
					delete_all(np, class, T_NS);
				goto try_parent;
			}
			if (nspp < &nsp[NSMAX-1])
				*nspp++ = dp;
		}

		*countp = nspp - nsp;
		if (*countp > 0) {
#ifdef DEBUG
			if (debug >= 3)
				fprintf(ddt,"findns: %d NS's added for '%s'\n",
					*countp, np->n_dname);
#endif
			*nspp = NULL;
			*npp = np;
			return(OK);	/* Success, got some NS's */
		}
try_parent:
		np = np->n_parent;
	}
	if (htp)
		goto try_again;
#ifdef DEBUG
	if (debug)
		fprintf(ddt, "findns: No root nameservers for class %d?\n",
		    class);
#endif
	if ((unsigned)class < MAXCLASS && norootlogged[class] == 0) {
		norootlogged[class] = 1;
		syslog(LOG_ERR, "No root nameservers for class %d\n", class);
	}
	return(SERVFAIL);
}

/*
 *  Extract RR's from the given node that match class and type.
 *  Return number of bytes added to response.
 *  If no matching data is found, then 0 is returned.
 */
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

	buflen = *lenp;
	cp = ((char *)hp) + *countp;
	for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
		if (!wanted(dp, class, type)) {
			if (type == T_CNAME && class == dp->d_class) {
				/* any data means no CNAME exists */
				*countp = 0;
				return(0);
			}
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
#ifdef DEBUG
			if (debug >=3)
			   fprintf(ddt,"finddata: stale entry '%s'\n",np->n_dname);
#endif
			if (dp->d_zone == 0)
				foundstale++;
			continue;
		}
		if ((n = make_rr(*dnamep, dp, cp, buflen, 1)) < 0) {
			hp->tc = 1;
			*countp = count;
			return(*lenp - buflen);
		}

		cp += n;
		buflen -= n;
		count++;
#ifdef notdef
		/* this isn't right for glue records, aa is set in ns_req */
		if (dp->d_zone && zones[dp->d_zone].z_auth && class != C_ANY)
			hp->aa = 1;			/* XXX */
#endif
		if (dp->d_type == T_CNAME) {
			if (type != T_ANY) {	/* or T_NS? */
				*dnamep = dp->d_data;
				if (dp->d_zone && zones[dp->d_zone].z_auth &&
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
	if (foundstale)
		delete_all(np, class, type);
#ifdef DEBUG
	if (debug >=3)
		fprintf(ddt,"finddata: added %d class %d type %d RRs\n",
			count, class, type);
#endif
	*countp = count;
	return(*lenp - buflen);
}

/*
 * Do we want this data record based on the class and type?
 */
wanted(dp, class, type)
	struct databuf *dp;
	int class, type;
{

#ifdef DEBUG
	if (debug > 3)
		fprintf(ddt,"wanted(%x, %d, %d) %d, %d\n", dp, class, type,
			dp->d_class, dp->d_type);
#endif

	if (dp->d_class != class && class != C_ANY)
		return (0);
	if (type == dp->d_type)
		return (1);
	switch (dp->d_type) {
	case T_ANY:
	case T_CNAME:
		return (1);
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
		if (dp->d_type == T_SOA)
			return (1);
	}
	return (0);
}

/*
 *  Add RR entries from dpp array to a query/response.
 *  Return the number of bytes added or negative the amount
 *  added if truncation was required.  Typically you are
 *  adding NS records to a response.
 */
add_data(np, dpp, cp, buflen)
	struct namebuf *np;
	struct databuf **dpp;
	register char *cp;
	int buflen;
{
	register struct databuf *dp;
	char dname[MAXDNAME];
	register int n, count = 0;

	getname(np, dname, sizeof(dname));
	for(dp = *dpp++; dp != NULL; dp = *dpp++) {
		if (stale(dp))
			continue;	/* ignore old cache entry */
		if ((n = make_rr(dname, dp, cp, buflen, 1)) < 0)
			return(-count);		/* Truncation */
		cp += n;
		buflen -= n;
		count += n;
	}
	return(count);
}

/*
 *  This is best thought of as a "cache invalidate" function.
 *  It is called whenever a piece of data is determined to have
 *  timed out.  It is better to have no information, than to
 *  have partial information you pass off as complete.
 */
delete_all(np, class, type)
register struct namebuf *np;
int class, type;
{
	register struct databuf *dp, *pdp;

#ifdef DEBUG
	if (debug > 2)
		fprintf(ddt,"delete_all: '%s' 0x%x class %d type %d\n",
			    np->n_dname, np, class, type);
#endif
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
