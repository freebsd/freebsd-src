#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)ns_req.c	4.47 (Berkeley) 7/1/91";
static char rcsid[] = "$Id: ns_req.c,v 4.9.1.22 1994/07/23 23:23:56 vixie Exp $";
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
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <syslog.h>
#include <errno.h>
#include <stdio.h>
#include <resolv.h>

#include "named.h"

struct addinfo {
	char	*a_dname;		/* domain name */
	u_int	a_class;		/* class for address */
};

enum req_action { Finish, Refuse, Return };

static enum req_action	req_query __P((HEADER *hp, u_char **cpp, u_char *eom,
				       struct qstream *qsp,
				       int *buflenp, int *msglenp,
				       u_char *msg, int dfd,
				       struct sockaddr_in *from));

#ifdef INVQ
static enum req_action	req_iquery __P((HEADER *hp, u_char **cpp, u_char *eom,
					int *buflenp, u_char *msg,
					struct sockaddr_in *from));
#endif

static void		fwritemsg __P((FILE *, u_char *, int)),
			doaxfr __P((struct namebuf *, FILE *,
				    struct namebuf *, int)),
			startxfr __P((struct qstream *, struct namebuf *,
				      u_char *, int, int, const char *)),
			printSOAdata __P((struct databuf));

#ifdef ALLOW_UPDATES
static int		InitDynUpdate __P((register HEADER *hp,
					   char *msg,
					   int msglen,
					   u_char *startcp,
					   struct sockaddr_in *from,
					   struct qstream *qsp,
					   int dfd));
#endif

static struct addinfo	addinfo[NADDRECS];
static void		addname __P((char *, u_int16_t));

/*
 * Process request using database; assemble and send response.
 */
void
ns_req(msg, msglen, buflen, qsp, from, dfd)
	u_char *msg;
	int msglen, buflen;
	struct qstream *qsp;
	struct sockaddr_in *from;
	int dfd;
{
	register HEADER *hp = (HEADER *) msg;
	u_char *cp, *eom;
	enum req_action action;

#ifdef DEBUG
	if (debug > 3) {
		fprintf(ddt, "ns_req(from=[%s].%d)\n",
			inet_ntoa(from->sin_addr), ntohs(from->sin_port));
		fp_query(msg, ddt);
	}
#endif

	/*
	 * XXX - this decision should be made by our caller, not by us.
	 */
	if (hp->qr) {
		ns_resp(msg, msglen);

		/* Now is a safe time for housekeeping */
		if (needs_prime_cache)
			prime_cache();

		return;
	}

	/* its a query and these bits have no business
	 * being set. will later simplify work if we can
	 * safely assume these are always 0 when a query
	 * comes in
	 */
	hp->aa = hp->ra = 0;

	hp->rcode = NOERROR;
	cp = msg + HFIXEDSZ;
	eom = msg + msglen;

	dnptrs[0] = msg;
	dnptrs[1] = NULL;

	free_addinfo();	/* sets addcount to zero */
	dnptrs[0] = NULL;

	switch (hp->opcode) {
	case QUERY:
		action = req_query(hp, &cp, eom, qsp,
				   &buflen, &msglen,
				   msg, dfd, from);
		break;

#ifdef INVQ
	case IQUERY:
		action = req_iquery(hp, &cp, eom, &buflen, msg, from);
		break;
#endif

#ifdef ALLOW_UPDATES
#define FORWARDED 1000
/*
 * In a sense the following constant should be defined in <arpa/nameser.h>,
 * since it is returned here in place of a response code if the update was
 * forwarded, and the response codes are defined in nameser.h.  On the other
 * hand, though, this constant is only seen in this file.  The assumption
 * here is that none of the other return codes equals this one (a good
 * assumption, since they only occupy 4 bits over-the-wire)
 */
        /* Call InitDynUpdate for all dynamic update requests */
        case UPDATEM:
        case UPDATEMA:
        case UPDATED:
        case UPDATEDA:
        case UPDATEA:
                n = InitDynUpdate(hp, msg, msglen, cp, from, qsp, dfd);
                if (n == FORWARDED) {
			/* Return directly because InitDynUpdate
			 * forwarded the query to the primary, so we
			 * will send response later
			 */
			action = Return;
		} else {
			/* Either sucessful primary update or failure;
			 * return response code to client
			 */
			action = Finish;
		}
#endif /* ALLOW_UPDATES */

	case ZONEREF:
		dprintf(1, (ddt, "Refresh Zone\n"));
		/*FALLTHROUGH*/

	default:
		dprintf(1, (ddt, "ns_req: Opcode %d not implemented\n",
			    hp->opcode));
		/* XXX - should syslog, limited by haveComplained */
		hp->qdcount = 0;
		hp->ancount = 0;
		hp->nscount = 0;
		hp->arcount = 0;
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
		/*FALLTHROUGH*/
	case Finish:
		/* rest of the function handles this case */
		break;
	default:
		syslog(LOG_CRIT, "bad action variable in ns_req() -- %d",
		       (int) action);
		return;	/* XXX - should really exit here */
	}

	/*
	 * apply final polish
	 */
	hp->qr = 1;		/* set Response flag */
	if (NoRecurse)
		hp->ra = 0;	/* No recursion; maybe we're a root server */
	else
		hp->ra = 1;	/* Recursion is Available */
	hp->ancount = htons(hp->ancount);
	if (addcount) {
		int n = doaddinfo(hp, cp, buflen);
		cp += n;
		buflen -= n;
	}

	dprintf(1, (ddt, "ns_req: answer -> [%s].%d fd=%d id=%d %s\n",
		    inet_ntoa(from->sin_addr), 
		    ntohs(from->sin_port),
		    (qsp == QSTREAM_NULL) ?dfd :qsp->s_rfd, 
		    ntohs(hp->id), local(from) == NULL ? "Remote" : "Local"));
#ifdef DEBUG
	if (debug >= 10)
		fp_query(msg, ddt);
#endif
	if (qsp == QSTREAM_NULL) {
		if (sendto(dfd, msg, cp - msg, 0,
			   (struct sockaddr *)from,
			   sizeof(*from)) < 0) {
			if (!haveComplained((char*)from->sin_addr.s_addr,
					    sendtoStr))
				syslog(LOG_NOTICE,
				       "ns_req: sendto([%s].%d): %m",
				       inet_ntoa(from->sin_addr),
				       ntohs(from->sin_port));
			nameserIncr(from->sin_addr, nssSendtoErr);
		}
		nameserIncr(from->sin_addr, nssSentAns);
	} else {
		(void) writemsg(qsp->s_rfd, msg, cp - msg);
		sq_done(qsp);
	}

	if (needs_prime_cache) {
		prime_cache();		/* Now is a safe time */
	}
}

static enum req_action
req_query(hp, cpp, eom, qsp, buflenp, msglenp, msg, dfd, from)
	HEADER *hp;
	u_char **cpp;
	u_char *eom;
	struct qstream *qsp;
	u_char *msg;
	int *buflenp, *msglenp, dfd;
	struct sockaddr_in *from;
{
	int n, class, type, count, foundname, founddata, omsglen, cname;
	u_int16_t id;
	u_char **dpp, *omsg, *answers;
	char dnbuf[MAXDNAME], *dname, *fname;
	struct hashbuf *htp;
	struct databuf *nsp[NSMAX];
	struct namebuf *np;
	struct qinfo *qp;
	struct netinfo *lp;

	nameserIncr(from->sin_addr, nssRcvdQ);
#ifdef DATUMREFCNT
	nsp[0] = NULL;
#endif

	dpp = dnptrs;
	*dpp++ = msg;
	*dpp = NULL;

	/* valid queries have one question and zero answers */
	if ((ntohs(hp->qdcount) != 1)
	    || hp->ancount
	    || hp->nscount
	    || hp->arcount) {
		dprintf(1, (ddt, "FORMERR Query header counts wrong\n"));
		hp->qdcount = 0;
		hp->ancount = 0;
		hp->nscount = 0;
		hp->arcount = 0;
		hp->rcode = FORMERR;
		return (Finish);
	}

	/*
	 * Get domain name, class, and type.
	 */
	if ((**cpp & INDIR_MASK) == 0) {
		*dpp++ = *cpp;	/* remember name for compression */
	}
	*dpp = NULL;
	n = dn_expand(msg, eom, *cpp, dnbuf, sizeof dnbuf);
	if (n < 0) {
		dprintf(1, (ddt, "FORMERR Query expand name failed\n"));
		hp->rcode = FORMERR;
		return (Finish);
	}
	*cpp += n;
	GETSHORT(type, *cpp);
	GETSHORT(class, *cpp);
	if (*cpp > eom) {
		dprintf(1, (ddt, "FORMERR Query message length short\n"));
		hp->rcode = FORMERR;
		return (Finish);
	}
	if (*cpp < eom) {
		dprintf(6, (ddt,"message length > received message\n"));
		*msglenp = *cpp - msg;
	}

	qtypeIncr(type);

	/*
	 * Process query.
	 */
	if (type == T_AXFR) {
		/* refuse request if not a TCP connection */
		if (qsp == QSTREAM_NULL) {
			syslog(LOG_INFO,
			       "rejected UDP AXFR from [%s].%u for \"%s\"",
			       inet_ntoa(from->sin_addr),
			       ntohs(from->sin_port),
			       *dnbuf ? dnbuf : ".");
			return (Refuse);
		}
		/* the position of this is subtle. */
		nameserIncr(from->sin_addr, nssRcvdAXFR);
#ifdef XFRNETS
		if (xfrnets) {
			/* if xfrnets was specified, peer address
			 * must be on it.  should probably allow
			 * for negation some day.
			 */
			if (!addr_on_netlist(from->sin_addr, xfrnets)) {
				syslog(LOG_INFO,
				       "unapproved AXFR from [%s].%u for %s",
				       inet_ntoa(from->sin_addr),
				       ntohs(from->sin_port),
				       *dnbuf ? dnbuf : ".");
				return (Refuse);
			}
		}
#endif /*XFRNETS*/
		dnptrs[0] = NULL;	/* don't compress names */
		hp->rd = 0;		/* recursion not possible */
		syslog(LOG_INFO, "approved AXFR from [%s].%d for \"%s\"",
		       inet_ntoa(from->sin_addr),
		       ntohs(from->sin_port),
		       *dnbuf ? dnbuf : ".");
	}
	*buflenp -= *msglenp;
	count = 0;
	foundname = 0;
	founddata = 0;
	dname = dnbuf;
	cname = 0;

#ifdef QRYLOG
	if (qrylog) {
		syslog(LOG_INFO, "XX /%s/%s/%s",
		       inet_ntoa(from->sin_addr), 
		       (dname[0] == '\0') ?"." :dname, 
		       p_type(type));
	}
#endif /*QRYLOG*/

try_again:
	dprintf(1, (ddt, "req: nlookup(%s) id %d type=%d\n",
		    dname, hp->id, type));
	htp = hashtab;		/* lookup relative to root */
	if ((np = nlookup(dname, &htp, &fname, 0)) == NULL)
		fname = "";
	dprintf(1, (ddt, "req: %s '%s' as '%s' (cname=%d)\n",
		    np == NULL ? "missed" : "found",
		    dname, fname, cname));

#ifdef LOCALDOM
	/*
	 * if nlookup failed to find the name then
	 * see if there are any '.''s in the name
	 * if not then add local domain name to the
	 * name and try again.
	 */
	if (!np && localdomain && !strchr(dname, '.')) {
		(void) strcat(dname, ".");
		(void) strcat(dname, localdomain);
		dprintf(1, (ddt,"req: nlookup(%s) type=%d\n", dname, type));
		htp = hashtab;
		np = nlookup(dname, &htp, &fname, 0);
        }
#endif /*LOCALDOM*/

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
			dprintf(3, (ddt, "ypkludge: hit as '%s'\n", dname));
			return (Finish);
		}
	}
#endif /*YPKLUDGE*/

	if ((!np) || (fname != dname))
		goto fetchns;

#ifdef SECURE_ZONES
	if (np->n_data) {
		struct zoneinfo *zp;

		zp = &zones[np->n_data->d_zone];
		if (zp->secure_nets
		    && !addr_on_netlist(from->sin_addr, zp->secure_nets)) {
			dprintf(1, (ddt,
				    "REFUSED Unauthorized request from %s\n", 
				    inet_ntoa(from->sin_addr)));
			syslog(LOG_INFO, "Unauthorized request %s from %s",
			       dname, inet_ntoa(from->sin_addr));
			return (Refuse);
		}
	}
#endif
	foundname++;
	answers = *cpp;
	count = *cpp - msg;

#ifdef NCACHE
	/* if this is a NXDOMAIN, will have only one databuf
	 * whose d_rcode field will be NXDOMAIN. So we can go home
	 * right here. -ve $ing: anant@isi.edu
	 */
	if (np->n_data != NULL && !stale(np->n_data)) {
		if (np->n_data->d_rcode == NXDOMAIN) {
#ifdef RETURNSOA
			n = finddata(np, class, T_SOA, hp, &dname,
				buflenp, &count);
			if (n != 0 ) {
			    if (hp->rcode == NOERROR_NODATA) {
				    /* this should not occur */
				    hp->rcode = NOERROR;
				    return (Finish);
			    }
			    *cpp += n;
			    *buflenp -= n;
			    *msglenp += n;
			    hp->rcode = NXDOMAIN;
			    hp->nscount = htons((u_int16_t)count);
			    hp->aa = 1;	
			    return (Finish);
			}
			else
			    goto fetchns;
#else
			hp->rcode = NXDOMAIN;
			hp->aa = 1;
			return (Finish);
#endif
		}
	}

	/* if not NXDOMAIN, the NOERROR_NODATA record might be
	 * anywhere in the chain. have to go through the grind.
	 */
#endif /*NCACHE*/

	n = finddata(np, class, type, hp, &dname, buflenp, &count);
	if (n == 0) {
		/* NO data available.  Refuse AXFR requests, or
		 * look for better servers for other requests.
		 */
		if (type == T_AXFR) {
			dprintf(1, (ddt, "T_AXFR refused: no data\n"));
			return (Refuse);
		} else {
			goto fetchns;
		}
	}

#ifdef NCACHE
	if (hp->rcode == NOERROR_NODATA) {
		hp->rcode = NOERROR;
		founddata = 1;
		return (Finish);
	}
#endif

	*cpp += n;
	*buflenp -= n;
	*msglenp += n;
	hp->ancount += count;
	if (fname != dname && type != T_CNAME && type != T_ANY) {
		if (cname++ >= MAXCNAMES) {
			dprintf(3, (ddt,
				    "resp: leaving, MAXCNAMES exceeded\n"));
			hp->rcode = SERVFAIL;
			return (Finish);
		}
		goto try_again;
	}
	founddata = 1;
	dprintf(3, (ddt,
		    "req: foundname=%d, count=%d, founddata=%d, cname=%d\n",
		    foundname, count, founddata, cname));

	if ((lp = local(from)) != NULL) 
		sort_response(answers, count, lp, *cpp);
	if (type == T_AXFR) {
		hp->ancount = htons(hp->ancount);
		startxfr(qsp, np, msg, *cpp - msg, class, dname);
		sqrm(qsp);
		return (Return);
	}

#ifdef notdef
	/*
	 * If we found an authoritative answer,
	 * we're done.
	 */
	if (hp->aa)
		return (Finish);
#endif

fetchns:
	/*
 	 * Look for name servers to refer to and fill in the authority
 	 * section or record the address for forwarding the query
 	 * (recursion desired).
 	 */
#ifdef DATUMREFCNT
	free_nsp(nsp);
#endif
	nsp[0] = NULL;
	count = 0;
	switch (findns(&np, class, nsp, &count, 0)) {
	case NXDOMAIN:
		if (!foundname) {
			hp->rcode = NXDOMAIN;
		}
		dprintf(3, (ddt, "req: leaving (%s, rcode %d)\n",
			    dname, hp->rcode));
		if (class != C_ANY) {
			hp->aa = 1;
			/* XXX:	should return SOA if founddata == 0,
			 *	but old named's are confused by an SOA
			 *	in the auth. section if there's no error.
			 */
			if (foundname == 0 && np) {
				n = doaddauth(hp, *cpp, *buflenp, np, nsp[0]);
				*cpp += n;
				*buflenp -= n;
#ifdef ADDAUTH
			} else if (hp->ancount) {
				/* don't add NS records for NOERROR NODATA
				   as some severs can get confused */
#ifdef DATUMREFCNT
				free_nsp(nsp);
#endif
				switch (findns(&np, class, nsp, &count, 1)) {
				case NXDOMAIN:
				case SERVFAIL:
					break;
				default:
					if (np) {
						n = add_data(np, nsp, *cpp,
							     *buflenp);
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
#ifdef DATUMREFCNT
		free_nsp(nsp);
#endif
		return (Finish);

	case SERVFAIL:
		if (!founddata && !(forward_only && fwdtab)) {
			hp->rcode = SERVFAIL;
#ifdef DATUMREFCNT
			free_nsp(nsp);
#endif
			return (Finish);
		}
	}

	/*
	 *  If we successfully found the answer in the cache,
	 *  or this is not a recursive query, or we are purposely
	 *  never recursing, then add the nameserver references
	 *  ("authority section") here and we're done.
	 */
	if (founddata || (!hp->rd) || NoRecurse) {
		n = add_data(np, nsp, *cpp, *buflenp);
		if (n < 0) {
			hp->tc = 1;
			n = (-n);
		}
		*cpp += n;
		*buflenp -= n;
		hp->nscount = htons((u_int16_t)count);
#ifdef DATUMREFCNT
		free_nsp(nsp);
#endif
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
		omsg = (u_char *)malloc((unsigned) *msglenp);
		if (omsg == (u_char *)NULL) {
			dprintf(1, (ddt, "ns_req: malloc fail\n"));
			syslog(LOG_ERR, "ns_req: Out Of Memory");
			hp->rcode = SERVFAIL;
#ifdef DATUMREFCNT
			free_nsp(nsp);
#endif
			return (Finish);
		}
		id = hp->id;
		hp->ancount = htons(hp->ancount);
		omsglen = *msglenp;
		bcopy(msg, omsg, omsglen);
		*msglenp = res_mkquery(QUERY, dname, class, type,
				       NULL, 0, NULL, msg,
				       *msglenp + *buflenp);
	}
	n = ns_forw(nsp, msg, *msglenp, from, qsp, dfd, &qp, dname, np);
	if (n != FW_OK && cname)
		free(omsg);
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
		** Don't go into an infinite loop if 
		** the admin gave root NS records in the cache
		** file without giving address records
		** for the root servers.
		*/
		if (np) {
			if (np->n_dname[0] == '\0') {
				dprintf(1, (ddt,
					    "ns_req: no address for root NS\n"
					    ));
				syslog(LOG_ERR, 
				       "ns_req: no address for root server");
				hp->rcode = SERVFAIL;
#ifdef DATUMREFCNT
				free_nsp(nsp);
#endif
				return (Finish);
			}
#ifdef	VALIDATE
			/*
			 * we need to kill all the NS records here as
			 * validate will fail as we are talking to the parent
			 * server
			 */
			delete_all(np, class, T_NS);
#endif
			np = np->n_parent;
		}
		goto fetchns;	/* Try again. */
	case FW_SERVFAIL:
		hp->rcode = SERVFAIL;
#ifdef DATUMREFCNT
		free_nsp(nsp);
#endif
		return (Finish);
	}
#ifdef DATUMREFCNT
	free_nsp(nsp);
#endif
	return (Return);
}

#ifdef INVQ
static enum req_action
req_iquery(hp, cpp, eom, buflenp, msg, from)
	HEADER *hp;
	u_char **cpp, *eom;
	int *buflenp;
	u_char *msg;
	struct sockaddr_in *from;
{
	register struct invbuf *ip;
	int dlen, alen, i, n, type, class, count;
	char dnbuf[MAXDNAME], anbuf[PACKETSZ], *data, *fname;
	struct namebuf *np;
	struct qinfo *qp;
	struct databuf *dp;

	nameserIncr(from->sin_addr, nssRcvdIQ);

	hp->ancount = htons(hp->ancount);
	if ((hp->ancount != 1)
	    || hp->qdcount
	    || hp->nscount
	    || hp->arcount) {
		dprintf(1, (ddt, "FORMERR IQuery header counts wrong\n"));
		hp->qdcount = 0;
		hp->ancount = 0;
		hp->nscount = 0;
		hp->arcount = 0;
		hp->rcode = FORMERR;
		return (Finish);
	}

	/*
	 * Skip domain name, get class, and type.
	 */
	if ((n = dn_skipname(*cpp, eom)) < 0) {
		dprintf(1, (ddt, "FORMERR IQuery packet name problem\n"));
		hp->rcode = FORMERR;
		return (Finish);
	}
	*cpp += n;
	GETSHORT(type, *cpp);
	GETSHORT(class, *cpp);
	*cpp += INT32SZ;	/* ttl */
	GETSHORT(dlen, *cpp);
	*cpp += dlen;
	if (*cpp != eom) {
		dprintf(1, (ddt, "FORMERR IQuery message length off\n"));
		hp->rcode = FORMERR;
		return (Finish);
	}

	/*
	 * not all inverse queries are handled.
	 */
	switch (type) {
	case T_A:
	case T_UID:
	case T_GID:
		break;
	default:
		return (Refuse);
	}
	dprintf(1, (ddt, "req: IQuery class %d type %d\n", class, type));

	fname = (char *)msg + HFIXEDSZ;
	bcopy(fname, anbuf, alen = (char *)*cpp - fname);
	data = anbuf + alen - dlen;
	*cpp = (u_char *)fname;
	*buflenp -= HFIXEDSZ;
	count = 0;
	for (ip = invtab[dhash((u_char *)data, dlen)];
	     ip != NULL;
	     ip = ip->i_next) {
		for (i = 0; i < INVBLKSZ; i++) {
			if ((np = ip->i_dname[i]) == NULL)
				break;
			dprintf(5, (ddt, "dname = %d\n", np->n_dname));
			for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
				if (!match(dp, class, type))
					continue;
				if (dp->d_size != dlen ||
				    bcmp(dp->d_data, data, dlen))
					continue;
				getname(np, dnbuf, sizeof(dnbuf));
				dprintf(2, (ddt, "req: IQuery found %s\n",
					    dnbuf));
				*buflenp -= QFIXEDSZ;
				n = dn_comp(dnbuf, *cpp, *buflenp, NULL, NULL);
				if (n < 0) {
					hp->tc = 1;
					return (Finish);
				}
				*cpp += n;
				PUTSHORT((u_int16_t)dp->d_type, *cpp);
				PUTSHORT((u_int16_t)dp->d_class, *cpp);
				*buflenp -= n;
				count++;
			}
		}
	}
	dprintf(1, (ddt, "req: IQuery %d records\n", count));
	hp->qdcount = htons((u_int16_t)count);
	if (alen > *buflenp) {
		hp->tc = 1;
		return (Finish);
	}
	bcopy(anbuf, *cpp, alen);
	*cpp += alen;
	return (Finish);
}
#endif

static void
fwritemsg(rfp, msg, msglen)
	FILE *rfp;
	u_char *msg;
	int msglen;
{
	u_char len[INT16SZ];

	__putshort(msglen, len);
	if (fwrite((char *)len, INT16SZ, 1, rfp) != 1 ||
	    fwrite((char *)msg, msglen, 1, rfp) != 1) {
		dprintf(1, (ddt, "fwrite failed %d\n", errno));
	}
}

/*
 *  Test a datum for validity and return non-zero if it is out of date.
 */
int
stale(dp)
	register struct databuf *dp;
{
	register struct zoneinfo *zp = &zones[dp->d_zone];

	switch (zp->z_type) {

	case Z_PRIMARY:
		return (0);

	case Z_SECONDARY:
#ifdef STUBS
	case Z_STUB:
#endif
		/*
		 * Check to see whether a secondary zone
		 * has expired; if so clear authority flag
		 * for zone and return true.  If lastupdate
		 * is in the future, assume zone is up-to-date.
		 */
		if ((int32_t)(tt.tv_sec - zp->z_lastupdate)
		    > (int32_t)zp->z_expire) {
			dprintf(1, (ddt,
				    "stale: secondary zone %s expired\n",
				    zp->z_origin));
			if (!haveComplained(zp->z_origin, (char*)stale)) {
				syslog(LOG_ERR,
				       "secondary zone \"%s\" expired",
				       zp->z_origin);
			}
			zp->z_flags &= ~Z_AUTH;
			return (1);
		}
		return (0);

	case Z_CACHE:
		if (dp->d_flags & DB_F_HINT || dp->d_ttl >= tt.tv_sec)
			return (0);
		dprintf(3, (ddt, "stale: ttl %d %d (x%x)\n",
			    dp->d_ttl, dp->d_ttl - tt.tv_sec, dp->d_flags));
		return (1);

	default:
		/* FALLTHROUGH */ ;

	}
	abort();
	/* NOTREACHED */
}

/*
 * Copy databuf into a resource record for replies.
 * Return size of RR if OK, -1 if buffer is full.
 */
int
make_rr(name, dp, buf, buflen, doadd)
	char *name;
	register struct databuf *dp;
	u_char *buf;
	int buflen, doadd;
{
	register u_char *cp;
	u_char *cp1, *sp;
	struct zoneinfo *zp;
	register int32_t n;
	register int32_t ttl;
	u_char **edp = dnptrs + sizeof dnptrs / sizeof dnptrs[0];

	dprintf(5, (ddt, "make_rr(%s, %x, %x, %d, %d) %d zone %d ttl %d\n",
		    name, dp, buf,
		    buflen, doadd, dp->d_size, dp->d_zone, dp->d_ttl));

#ifdef	NCACHE
	if (dp->d_rcode
#ifdef RETURNSOA
	&& dp->d_rcode != NXDOMAIN
#endif
	) {
		syslog(LOG_CRIT, "make_rr d_rcode %d", dp->d_rcode);
#ifdef DEBUG
		if (debug) abort();
#endif
		return (-1);	/* XXX We should exit here */
	}
#endif
	zp = &zones[dp->d_zone];
	/* check for outdated RR before updating dnptrs by dn_comp() (???) */
	if (zp->z_type == Z_CACHE) {
		ttl = dp->d_ttl - (u_int32_t) tt.tv_sec;
		if ((dp->d_flags & DB_F_HINT) || (ttl < 0)) {
			dprintf(3, (ddt,
				    "make_rr: %d=>0, x%x\n",
				    ttl, dp->d_flags));		/* XXX */
			ttl = 0;
		}
	} else {
		if (dp->d_ttl)
			ttl = dp->d_ttl;
		else
			ttl = zp->z_minimum;		/* really default */
#ifdef notdef /* don't decrease ttl based on time since verification */
		if (zp->z_type == Z_SECONDARY) {
			/*
			 * Set ttl to value received from primary,
			 * less time since we verified it (but never
			 * less than a small positive value).
			 */
			ttl -= tt.tv_sec - zp->z_lastupdate;
			if (ttl <= 0)
				ttl = 120;
		}
#endif
	}

	buflen -= RRFIXEDSZ;
#if defined(RETURNSOA) && defined(NCACHE)
	if (dp->d_rcode == NXDOMAIN) {
		name = (char *)dp->d_data;
		name += strlen(name) +1;
		name += strlen(name) +1;
		name += 5 * INT32SZ;
	}
#endif
	if ((n = dn_comp(name, buf, buflen, dnptrs, edp)) < 0)
		return (-1);
	cp = buf + n;
	buflen -= n;
	PUTSHORT((u_int16_t)dp->d_type, cp);
	PUTSHORT((u_int16_t)dp->d_class, cp);
	PUTLONG(ttl, cp);
	sp = cp;
	cp += INT16SZ;
	switch (dp->d_type) {
	case T_CNAME:
	case T_MG:
	case T_MR:
	case T_PTR:
		n = dn_comp((char *)dp->d_data, cp, buflen, dnptrs, edp);
		if (n < 0)
			return (-1);
		PUTSHORT((u_int16_t)n, sp);
		cp += n;
		break;

	case T_MB:
	case T_NS:
		/* Store domain name in answer */
		n = dn_comp((char *)dp->d_data, cp, buflen, dnptrs, edp);
		if (n < 0)
			return (-1);
		PUTSHORT((u_int16_t)n, sp);
		cp += n;
		if (doadd)
			addname((char*)dp->d_data, dp->d_class);
		break;

	case T_SOA:
	case T_MINFO:
	case T_RP:
		cp1 = dp->d_data;
		n = dn_comp((char *)cp1, cp, buflen, dnptrs, edp);
		if (n < 0)
			return (-1);
		cp += n;
		buflen -= dp->d_type == T_SOA ? n + 5 * INT32SZ : n;
		cp1 += strlen((char *)cp1) + 1;
		n = dn_comp((char *)cp1, cp, buflen, dnptrs, edp);
		if (n < 0)
			return (-1);
		cp += n;
		if (dp->d_type == T_SOA) {
			cp1 += strlen((char *)cp1) + 1;
			bcopy(cp1, cp, (n = 5 * INT32SZ));
			cp += n;
		}
		n = (u_int16_t)((cp - sp) - INT16SZ);
		PUTSHORT((u_int16_t)n, sp);
		break;

	case T_MX:
	case T_AFSDB:
	case T_RT:
		/* cp1 == our data/ cp == data of RR */
		cp1 = dp->d_data;

 		/* copy preference */
 		bcopy(cp1, cp, INT16SZ);
 		cp += INT16SZ;
 		cp1 += INT16SZ;
 		buflen -= INT16SZ;

		n = dn_comp((char *)cp1, cp, buflen, dnptrs, edp);
  		if (n < 0)
  			return (-1);
  		cp += n;

  		/* save data length */
		n = (u_int16_t)((cp - sp) - INT16SZ);
  		PUTSHORT((u_int16_t)n, sp);
		if (doadd)
			addname((char*)cp1, dp->d_class);
		break;

	default:
		if (dp->d_size > buflen)
			return (-1);
		bcopy(dp->d_data, cp, dp->d_size);
		PUTSHORT((u_int16_t)dp->d_size, sp);
		cp += dp->d_size;
	}
	return (cp - buf);
}

#if defined(__STDC__) || defined(__GNUC__)
static void
addname(register char *name,
	u_int16_t class)
#else
static void
addname(name, class)
	register char	*name;
	u_int16_t	class;
#endif
{
	register struct addinfo *ap;
	register int n;

	for (ap = addinfo, n = addcount; --n >= 0; ap++)
		if (strcasecmp(ap->a_dname, name) == 0)
			return;
	

	/* add domain name to additional section */
	if (addcount < NADDRECS) {
		addcount++;
		ap->a_dname = (char *)malloc(strlen(name)+1);
		strcpy(ap->a_dname,name);
		ap->a_class = class;
	}
}

/*
 * Lookup addresses for names in addinfo and put into the message's
 * additional section.
 */
int
doaddinfo(hp, msg, msglen)
	HEADER *hp;
	u_char *msg;
	int msglen;
{
	register struct namebuf *np;
	register struct databuf *dp;
	register struct addinfo *ap;
	register u_char *cp;
	struct hashbuf *htp;
	char *fname;
	int n, count;

	dprintf(3, (ddt, "doaddinfo() addcount = %d\n", addcount));

	if (hp->tc) {
		dprintf(4, (ddt, "doaddinfo(): tc already set, bailing\n"));
		return (0);
	}

	count = 0;
	cp = msg;
	for (ap = addinfo; --addcount >= 0; ap++) {
		int	foundstale = 0,
			foundany = 0,
			save_count = count,
			save_msglen = msglen;
		u_char	*save_cp = cp;

		dprintf(3, (ddt, "do additional '%s'\n", ap->a_dname));
		htp = hashtab;	/* because "nlookup" stomps on arg. */
		np = nlookup(ap->a_dname, &htp, &fname, 0);
		if (np == NULL || fname != ap->a_dname)
			goto next_rr;
		dprintf(3, (ddt, "found it\n"));
		/* look for the data */
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			if ( (!match(dp, (int)ap->a_class, T_A))
			  && (!match(dp, C_IN, T_A))
			   ) {
				continue;
			}
			foundany++;
			if (stale(dp)) {
				foundstale++;
				dprintf(1, (ddt,
					    "doaddinfo: stale entry '%s'%s\n",
					    np->n_dname,
					    (dp->d_flags&DB_F_HINT)
					        ? " hint"
					        : ""
					    ));
				continue;
			}
#ifdef	NCACHE
			if (dp->d_rcode)
				continue;
#endif
			/*
			 *  Should be smart and eliminate duplicate
			 *  data here.	XXX
			 */
			if ((n = make_rr(ap->a_dname, dp, cp, msglen, 0)) < 0){
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
				cp = save_cp;
				msglen = save_msglen;
				count = save_count;
				break;
			}
			dprintf(5, (ddt,
				    "addinfo: adding address data n = %d\n",
				    n));
			cp += n;
			msglen -= n;
			count++;
		}
next_rr:	if (foundstale) {
			/* Cache invalidate the address RR's */
			delete_all(np, (int)ap->a_class, T_A);
		}
		if (foundstale || !foundany) {
			/* ask a real server for this info */
			(void) sysquery(ap->a_dname, (int)ap->a_class, T_A,
					NULL, 0);
		}
		free(ap->a_dname);
	}
	hp->arcount = htons((u_int16_t)count);
	return (cp - msg);
}

int
doaddauth(hp, cp, buflen, np, dp)
	register HEADER *hp;
	u_char *cp;
	int buflen;
	struct namebuf *np;
	struct databuf *dp;
{
	char dnbuf[MAXDNAME];
	int n;

	getname(np, dnbuf, sizeof(dnbuf));
	if (stale(dp)) {
		dprintf(1, (ddt,
			    "doaddauth: can't add stale '%s' (%d)\n",
			    dnbuf, buflen));
		return (0);
	}
	n = make_rr(dnbuf, dp, cp, buflen, 1);
	if (n <= 0) {
		dprintf(1, (ddt,
			    "doaddauth: can't add oversize '%s' (%d) (n=%d)\n",
			    dnbuf, buflen, n));
		if (n < 0) {
			hp->tc = 1;
		}
		return (0);
	}
	hp->nscount = htons((u_int16_t)1);
	return (n);
}

/*
 * Do a zone transfer (or a recursive part of a zone transfer).
 * SOA record already sent.
 *
 * top always refers to the domain at the top of the zone being transferred.
 * np refers to a domain inside the zone being transferred,
 *	which will be equal to top if this is the first call,
 *	or will be a subdomain below top if this is a recursive call,
 * rfp is a stdio file to which output is sent.
 */
static void
doaxfr(np, rfp, top, class)
	register struct namebuf *np;
	FILE *rfp;
	struct namebuf *top;
	int class;		/* Class to transfer */
{
	register struct databuf *dp;
	register int n;
	struct hashbuf *htp;
	struct databuf *gdp;	/* glue databuf */
	struct namebuf *gnp;	/* glue namebuf */
	struct namebuf *tnp;	/* top namebuf */
	struct databuf *tdp;	/* top databuf */
	struct namebuf **npp, **nppend;
	u_char msg[PACKETSZ];
	u_char *cp;
	char *fname;
	char dname[MAXDNAME];
	HEADER *hp = (HEADER *) msg;
	int fndns;

	if (np == top)
		dprintf(1, (ddt, "doaxfr()\n"));
	fndns = 0;
	hp->id = 0;
	hp->opcode = QUERY;
	hp->aa = hp->tc = hp->ra = hp->pr = hp->rd = 0;
	hp->qr = 1;
	hp->rcode = NOERROR;
	hp->qdcount = 0;
	hp->ancount = htons(1);
	hp->nscount = 0;
	hp->arcount = 0;
	cp = (u_char *) (msg + HFIXEDSZ);
	getname(np, dname, sizeof(dname));

	/* first do the NS records (del@harris) */
	for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
#ifdef GEN_AXFR
	    if (dp->d_class != class && class != C_ANY)
		continue;
#endif
#ifdef	    NCACHE
	    if (dp->d_rcode)
		continue;
#endif
	    if (dp->d_type == T_NS) {
		fndns = 1;
		n = make_rr(dname, dp, cp, sizeof(msg)-HFIXEDSZ, 0);
		if (n < 0)
			continue;
		fwritemsg(rfp, msg, n + HFIXEDSZ);
#ifdef NO_GLUE
		if ((np != top) || (top->n_dname[0] == '\0')) {
#endif /*NO_GLUE*/
		    /*  Glue the sub domains together by sending 
		     *  the address records for the sub domain
		     *  name servers along if necessary.
		     *  Glue is necessary if the server is in any zone
		     *  delegated from the current (top) zone.  Such
		     *  a delegated zone might or might not be that
		     *  referred to by the NS record now being handled.
		     */
 		    htp = hashtab;
		    cp = (u_char *) (msg + HFIXEDSZ);
 		    gnp = nlookup((char *)dp->d_data, &htp, &fname, 0);
 		    if (gnp == NULL || fname != (char *)dp->d_data)
 			continue;
#ifdef NO_GLUE
		    for (tnp = gnp; tnp != NULL; tnp = tnp->n_parent)
			if ( tnp == top )
			    break;
		    if ( (tnp == NULL) && (top->n_dname[0] != '\0') )
			continue;  /* name server is not below top domain */
		    for (tnp = gnp; tnp != top; tnp = tnp->n_parent) {
			for (tdp = tnp->n_data;
			     tdp != NULL;
			     tdp = tdp->d_next) {
#ifdef GEN_AXFR
			    if (tdp->d_class != class && class != C_ANY)
				continue;
#endif
			    if (tdp->d_type == T_NS)
				break;
			}
			if (tdp != NULL)
			    break; /* found a zone cut */
		    }
		    if (tnp == top)
			continue;  /* name server is not in a delegated zone */
		    /* now we know glue records are needed.  send them. */
#endif /*NO_GLUE*/
 		    for (gdp=gnp->n_data; gdp != NULL; gdp=gdp->d_next) {
#ifdef GEN_AXFR
			if (gdp->d_class != class && class != C_ANY)
			    continue;
#endif
 			if (gdp->d_type != T_A || stale(gdp))
 			    continue;
#ifdef NCACHE
			if (gdp->d_rcode)
			    continue;
#endif
 			n = make_rr(fname, gdp, cp, sizeof(msg)-HFIXEDSZ, 0);
			if (n < 0)
			    continue;
 			fwritemsg(rfp, msg, n + HFIXEDSZ);
 		    }
#ifdef NO_GLUE
		}
#endif /*NO_GLUE*/
	    }
	}
	/* no need to send anything else if a delegation appeared */
	if ((np != top) && fndns)
		return;

	/* do the rest of the data records */
	for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
#ifdef GEN_AXFR
		if (dp->d_class != class && class != C_ANY)
			continue;
#endif
		/*
		 * Skip the top SOA record (marks end of data);
		 * don't send SOA for subdomains, as we're not sending them;
		 * skip the NS records because we did them first.
		 */
		if (dp->d_type == T_SOA || dp->d_type == T_NS)
			continue;
		if (dp->d_zone == 0 || stale(dp))
			continue;
#ifdef NCACHE
		if (dp->d_rcode)
			continue;
#endif
		if ((n = make_rr(dname, dp, cp, sizeof(msg)-HFIXEDSZ, 0)) < 0)
			continue;
		fwritemsg(rfp, msg, n + HFIXEDSZ);
	}

	/* Finally do non-delegated subdomains.  Delegated subdomains
	 * have already been handled.
	 */
	/*
	 * We find the subdomains by looking in the hash table for this
	 * domain, but the root domain needs special treatment, because
	 * of the following wart in the database design:
	 *
	 * The top level hash table (pointed to by the global `hashtab'
	 * variable) contains pointers to the namebuf's for the root as
	 * well as for the top-level domains below the root, in contrast
	 * to the usual situation where a hash table contains entries
	 * for domains at the same level.  The n_hash member of the
	 * namebuf for the root domain is NULL instead of pointing to a
	 * hashbuf for the top-level domains.  The n_parent members of
	 * the namebufs for the top-level domains are NULL instead of
	 * pointing to the namebuf for the root.
	 *
	 * We work around the wart as follows:
	 *
	 * If we are not dealing with the root zone then we just set
	 * htp = np->n_hash, pointing to the hash table for the current
	 * domain, and we walk through the hash table as usual,
	 * processing the namebufs for all the subdomains.
	 *
	 * If we are dealing with the root zone, then we set
	 * htp = hashtab, pointing to the global hash table (because
	 * there is no hash table associated with the root domain's
	 * namebuf.  While we walk this hash table, we take care not to
	 * recursively process the entry for the root namebuf.
	 *
	 * (apb@und nov1990)
	 */
	htp = ((dname[0] == '\0') ? hashtab : np->n_hash);
	if (htp == NULL) {
		return; /* no subdomains */
	}
	npp = htp->h_tab;
	nppend = npp + htp->h_size;
	while (npp < nppend) {
		for (np = *npp++; np != NULL; np = np->n_next) {
		    if (np->n_dname[0] != '\0') { /* don't redo root domain */
			doaxfr(np, rfp, top, class);
		    }
		}
	}
	if (np == top)
		dprintf(1, (ddt, "exit doaxfr()\n"));
}

#ifdef ALLOW_UPDATES
/*
 * Called by UPDATE{A,D,DA,M,MA} to initiate a dynamic update.  If this is the
 * primary server for the zone being updated, we update the zone's serial
 * number and then call doupdate directly. If this is a secondary, we just
 * forward the update; this way, if the primary update fails (e.g., if the
 * primary is unavailable), we don't update the secondary; if the primary
 * update suceeds, ns_resp will get called with the response (when it comes
 * in), and then update the secondary's copy.
 */
static int
InitDynUpdate(hp, msg, msglen, startcp, from, qsp, dfd)
	register HEADER *hp;
	char *msg;
	int msglen;
	u_char *startcp;
	struct sockaddr_in *from;
	struct qstream *qsp;
	int dfd;
{
	struct databuf *nsp[NSMAX];
	struct zoneinfo *zp;
	char dnbuf[MAXDNAME];
	struct hashbuf *htp = hashtab;	/* lookup relative to root */
	struct namebuf *np;
	struct databuf *olddp, *newdp, *dp;
	struct databuf **nspp;
	char *fname;
	register u_char *cp = startcp;
	u_int16_t class, type;
	int n, size, zonenum;
	char ZoneName[MAXDNAME], *znp;

#ifdef 	DATUMREFCNT
	nsp[0] = NULL;
#endif
	if ((n = dn_expand(msg, msg + msglen, cp, dnbuf, sizeof(dnbuf))) < 0) {
		dprintf(1, (ddt,"FORMERR InitDynUpdate expand name failed\n"));
		hp->rcode = FORMERR;
		return (FORMERR);
	}
	cp += n;
	GETSHORT(type, cp);
	if (type == T_SOA) {	/* T_SOA updates not allowed */
		hp->rcode = REFUSED;
		dprintf(1, (ddt, "InitDynUpdate: REFUSED - SOA update\n"));
		return (REFUSED);
	}
	GETSHORT(class, cp);
	cp += INT32SZ;
	GETSHORT(size, cp);
/****XXX - need bounds checking here ****/
	cp += size;

	if ((zonenum = findzone(dnbuf, class)) == 0) {  /* zone not found */
		hp->rcode = NXDOMAIN;
		return (NXDOMAIN);
	}
	zp = &zones[zonenum];

	/* Disallow updates for which we aren't authoratative.  Note: the
	   following test doesn't work right:  If it's for a non-local zone,
	   we will think it's a primary but be unable to lookup the namebuf,
	   thus returning 'NXDOMAIN' */
	if (zp->z_type != Z_PRIMARY && zp->z_type != Z_SECONDARY) {
		hp->rcode = REFUSED;
		dprintf(1, (ddt,
		 "InitDynUpdate: REFUSED - non-{primary,secondary} update\n"));
		return (REFUSED);
	}
	if (!(zp->z_flags & Z_DYNAMIC)) {
		hp->rcode = REFUSED;
		dprintf(1, (ddt,
		 "InitDynUpdate: REFUSED - dynamic flag not set for zone\n"));
		return (REFUSED);
	}

	/*
	 * Lookup the zone namebuf.  Lookup "xyz" not "xyz.", since
	 * otherwise the lookup fails, because '.' may have a nil n_hash
	 * associated with it.
	 */
	strcpy(ZoneName, zp->z_origin);
	znp = &ZoneName[strlen(ZoneName) - 1];
	if (*znp == '.')
		*znp = NULL;
	np = nlookup(ZoneName, &htp, &fname, 0);
	if ((np == NULL) || (fname != ZoneName)) {
		dprintf(1, (ddt, "InitDynUpdate: lookup failed on zone (%s)\n",
			    ZoneName));
	        syslog(LOG_ERR, "InitDynUpdate: lookup failed on zone (%s)\n",
		       ZoneName);
		hp->rcode = NXDOMAIN;
		return (NXDOMAIN);
	}

	/*
	 * If this is the primary copy increment the serial number.  Don't
	 * increment the serial number if this is a secondary; this way, if 2
	 * different secondaries both update the primary, they will both have
	 * lower serial numbers than the primary has, and hence eventually
	 * refresh and get all updates and become consistent.
	 *
	 * Note that the serial number must be incremented in both the zone
	 * data structure and the zone's namebuf.
	 */
	switch (zp->z_type) {
	case Z_SECONDARY:		/* forward update to primary */
		nspp = nsp;
		dp = np->n_data;
		while (dp != NULL) {
			if (match(dp, class, T_NS)) {
				if (nspp < &nsp[NSMAX-1]) {
					*nspp++ = dp;
#ifdef	DATUMREFCNT
					dp->d_rcnt++;
#endif
				} else
					break;
			}
			dp = dp->d_next;
		}
		*nspp = NULL; /* Delimiter */
		if (ns_forw(nsp, msg, msglen, from, qsp, dfd, NULL, dnbuf, np)
		    <
		    0) {
			hp->rcode = SERVFAIL;
#ifdef	DATUMREFCNT
			free_nsp(nsp);
#endif
			return (SERVFAIL);
		}
#ifdef	DATUMREFCNT
		free_nsp(nsp);
#endif
		return (FORWARDED);

	case Z_PRIMARY:
		zp->z_serial++;
		/* Find the SOA record */
		for (olddp = np->n_data; olddp != NULL; olddp = olddp->d_next)
			if (match(olddp, class, T_SOA))
				break;
		if (olddp == NULL) {
			dprintf(1, (ddt,
			 "InitDynUpdate: Couldn't find SOA record for '%s'\n",
				    ZoneName));
			syslog(LOG_ERR,
			   "InitDynUpdate: Couldn't find SOA record for '%s'\n"
,
			   ZoneName);
			hp->rcode = NXDOMAIN;
#ifdef	DATUMREFCNT
			free_nsp(nsp);
#endif
			return (NXDOMAIN);
		}
		newdp = savedata(olddp->d_class, olddp->d_type, olddp->d_ttl,
				 olddp->d_data, olddp->d_size);
		newdp->d_zone = olddp->d_zone;
		newdp->d_cred = DB_C_AUTH;	/* XXX - it may not be so */
		newdp->d_clev = db_getclev(zp->z_origin);
		cp = (u_char *)newdp->d_data;
		cp += strlen(cp) + 1; /* skip origin string */
		cp += strlen(cp) + 1; /* skip in-charge string */
		putlong((u_int32_t)(zp->z_serial), cp);
		dprintf(4, (ddt, "after stuffing data into newdp:\n"));
#ifdef DEBUG
		if (debug >= 4)
			printSOAdata(newdp);
#endif

		if ((n = db_update(ZoneName, olddp, newdp, DB_DELETE,
				   hashtab)) != NOERROR) {	/* XXX */
			dprintf(1, (ddt,
				    "InitDynUpdate: SOA update failed\n"));
			hp->rcode = NOCHANGE;
			free((char*) dp);
#ifdef	DATUMREFCNT
			free_nsp(nsp);
#endif
			return (NOCHANGE);
		}

		/* Now update the RR itself */
		/* XXX - DB_C_AUTH may be wrong */
		if (doupdate(msg, msglen, msg + HFIXEDSZ, zonenum,
			     (struct databuf *)0, DB_NODATA, DB_C_AUTH) < 0) {
			dprintf(1, (ddt, "InitDynUpdate: doupdate failed\n"));
			/* doupdate fills in rcode */
#ifdef	DATUMREFCNT
			free_nsp(nsp);
#endif
			return (hp->rcode);
		}
		zp->z_flags |= Z_CHANGED;
#ifdef	DATUMREFCNT
			free_nsp(nsp);
#endif
		return (NOERROR);
	}
}

#ifdef DEBUG
/*
 * Print the contents of the data in databuf pointed to by dp for an SOA record
 */
static void
printSOAdata(dp)
	struct databuf *dp;
{
	register u_char *cp;

	if (!debug)
		return;  /* Otherwise fprintf to ddt will bomb */
	cp = (u_char *)dp->d_data;
	fprintf(ddt, "printSOAdata(%x): origin(%x)='%s'\n", dp, cp, cp);
	cp += strlen(cp) + 1; /* skip origin string */
	fprintf(ddt, "printSOAdata: in-charge(%x)='%s'\n", cp, cp);
	cp += strlen(cp) + 1; /* skip in-charge string */
	fprintf(ddt, "printSOAdata: serial(%x)=%d\n", cp, _getlong(cp));
}
#endif
#endif

static void
startxfr(qsp, np, soa, soalen, class, dname)
	struct qstream	*qsp;
	struct namebuf	*np;
	u_char		*soa;
	int		soalen;
	int		class;
	const char	*dname;
{
	FILE *rfp;
	int fdstat;
	pid_t pid;

	dprintf(5, (ddt, "startxfr()\n"));

	/*
	 * child does the work while
	 * the parent continues
	 */
	switch (pid = fork()) {
	case -1:
		syslog(LOG_ERR, "startxfr(%s -> [%s]) failing; fork: %m",
		       dname, inet_ntoa(qsp->s_from.sin_addr));
		return;
	case 0:
		/* child */
		break;
	default:
		/* parent */
		syslog(LOG_DEBUG, "zone transfer of \"%s\" to [%s] (pid %lu)",
		       dname, inet_ntoa(qsp->s_from.sin_addr), pid);
		return;
	}

	/*
	 * Child.
	 *
	 * XXX:	this should be a vfork/exec since on non-copy-on-write
	 *	systems with huge nameserver images, this is very expensive.
	 */
	close(vs);
	sqflush(/*allbut*/ qsp);
	dqflush((time_t)0);

#ifdef RENICE
	nice(-40);  nice(20);  nice(0);		/* back to "normal" */
#endif
	dprintf(5, (ddt, "startxfr: child pid %lu\n", (u_long)pid));

	if (!(rfp = fdopen(qsp->s_rfd, "w"))) {
		syslog(LOG_ERR, "fdopen: %m");
		_exit(1);
	}
	ns_setproctitle("zone XFR to", qsp->s_rfd);
	if (-1 == (fdstat = fcntl(qsp->s_rfd, F_GETFL, 0))) {
		syslog(LOG_ERR, "fcntl(F_GETFL): %m");
		_exit(1);
	}
	(void) fcntl(qsp->s_rfd, F_SETFL, fdstat & ~PORT_NONBLOCK);
	fwritemsg(rfp, soa, soalen);
	doaxfr(np, rfp, np, class);
	fwritemsg(rfp, soa, soalen);
	(void) fflush(rfp);
	_exit(0);
}

free_addinfo() {
	struct addinfo *ap;

	for (ap = addinfo; --addcount >= 0; ap++) {
		free(ap->a_dname);
	}
	addcount = 0;
}

#ifdef DATUMREFCNT
free_nsp(nsp)
struct databuf **nsp;
{
	while (*nsp) {
		if (--((*nsp)->d_rcnt)) {
			dprintf(3, (ddt, "free_nsp: %s rcnt %d\n",
			(*nsp)->d_data, (*nsp)->d_rcnt));
		} else {
			dprintf(3, (ddt, "free_nsp: %s rcnt %d delayed\n",
				(*nsp)->d_data, (*nsp)->d_rcnt));
			free(*nsp);	/* delayed free */
		}
		*nsp++ = NULL;
	}
}
#endif
