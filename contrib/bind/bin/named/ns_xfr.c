#if !defined(lint) && !defined(SABER)
static char rcsid[] = "$Id: ns_xfr.c,v 8.25 1998/03/25 18:47:34 halley Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1996, 1997 by Internet Software Consortium.
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

#include <sys/param.h>
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

static struct qs_x_lev *sx_freelev(struct qs_x_lev *lev);

static void		sx_newmsg(struct qstream *qsp),
			sx_sendlev(struct qstream *qsp),
			sx_sendsoa(struct qstream *qsp);

static int		sx_flush(struct qstream *qsp),
			sx_addrr(struct qstream *qsp,
				 const char *dname,
				 struct databuf *dp),
			sx_nsrrs(struct qstream *qsp),
			sx_allrrs(struct qstream *qsp),
			sx_pushlev(struct qstream *qsp, struct namebuf *np);

/*
 * void
 * ns_xfr(qsp, znp, zone, class, type, opcode, id)
 *	Initiate a concurrent (event driven) outgoing zone transfer.
 */
void
ns_xfr(struct qstream *qsp, struct namebuf *znp,
       int zone, int class, int type,
       int opcode, int id)
{
	FILE *rfp;
	int fdstat;
	pid_t pid;
	server_info si;
#ifdef SO_SNDBUF
	static const int sndbuf = XFER_BUFSIZE * 2;
#endif
#ifdef SO_SNDLOWAT
	static const int sndlowat = XFER_BUFSIZE;
#endif

	ns_info(ns_log_xfer_out, "zone transfer of \"%s\" (%s) to %s",
		zones[zone].z_origin, p_class(class), sin_ntoa(qsp->s_from));

#ifdef SO_SNDBUF
	/*
	 * The default seems to be 4K, and we'd like it to have enough room
	 * to parallelize sending the pushed data with accumulating more
	 * write() data from us.
	 */
	(void) setsockopt(qsp->s_rfd, SOL_SOCKET, SO_SNDBUF,
			  (char *)&sndbuf, sizeof sndbuf);
#endif
#ifdef SO_SNDLOWAT
	/*
	 * We don't want select() to show writability 'til we can write
	 * an XFER_BUFSIZE block of data.
	 */
	(void) setsockopt(qsp->s_rfd, SOL_SOCKET, SO_SNDLOWAT,
			  (char *)&sndlowat, sizeof sndlowat);
#endif
	if (sq_openw(qsp, 64*1024) == -1)
		goto abort;
	memset(&qsp->xfr, 0, sizeof qsp->xfr);
	qsp->xfr.top = znp;
	qsp->xfr.zone = zone;
	qsp->xfr.class = class;
	qsp->xfr.id = id;
	qsp->xfr.opcode = opcode;
	qsp->xfr.msg = memget(XFER_BUFSIZE);
	if (!qsp->xfr.msg)
		goto abort;
	qsp->xfr.eom = qsp->xfr.msg + XFER_BUFSIZE;
	qsp->xfr.cp = NULL;
	qsp->xfr.state = s_x_firstsoa;
	zones[zone].z_numxfrs++;
	qsp->flags |= STREAM_AXFR;

	si = find_server(qsp->s_from.sin_addr);
	if (si != NULL && si->transfer_format != axfr_use_default)
		qsp->xfr.transfer_format = si->transfer_format;
	else	
		qsp->xfr.transfer_format = server_options->transfer_format;

	if (sx_pushlev(qsp, znp) < 0) {
 abort:
		(void) shutdown(qsp->s_rfd, 2);
		sq_remove(qsp);
		return;
	}
	(void) sq_writeh(qsp, sx_sendsoa);
}

/*
 * void
 * ns_stopxfrs(zp)
 *	Stop (abort, reset) all transfers of the zone specified by 'zp'.
 */
void
ns_stopxfrs(struct zoneinfo *zp) {
	struct qstream *this, *next;
	u_int zone = (u_int)(zp - zones);

	for (this = streamq; this; this = next) {
		next = this->s_next;
		if (this->xfr.zone == zone) {
			(void) shutdown(this->s_rfd, 2);
			sq_remove(this);
		}
	}
	INSIST(zp->z_numxfrs == 0);
}

/*
 * void
 * ns_freexfr(qsp)
 *	Free all xfr-related dynamic data associated with qsp.
 */
void
ns_freexfr(struct qstream *qsp) {
	if (qsp->xfr.msg != NULL) {
		memput(qsp->xfr.msg, XFER_BUFSIZE);
		qsp->xfr.msg = NULL;
	}
	while (qsp->xfr.lev)
		qsp->xfr.lev = sx_freelev(qsp->xfr.lev);
	zones[qsp->xfr.zone].z_numxfrs--;
	qsp->flags &= ~STREAM_AXFR;
}

/*
 * u_char *
 * renew_msg(msg)
 *	init the header of a message, reset the compression pointers, and
 *	reset the write pointer to the first byte following the header.
 */
static void
sx_newmsg(struct qstream *qsp) {
	HEADER *hp = (HEADER *)qsp->xfr.msg;

	memset(hp, 0, HFIXEDSZ);
	hp->id = htons(qsp->xfr.id);
	hp->opcode = qsp->xfr.opcode;
	hp->qr = 1;
	hp->rcode = NOERROR;

	qsp->xfr.ptrs[0] = qsp->xfr.msg;
	qsp->xfr.ptrs[1] = NULL;

	qsp->xfr.cp = qsp->xfr.msg + HFIXEDSZ;
}

/*
 * int
 * sx_flush(qsp)
 *	flush the intermediate buffer out to the stream IO system.
 * return:
 *	passed through from sq_write().
 */
static int
sx_flush(struct qstream *qsp) {
	int ret;

#ifdef DEBUG
	if (debug >= 10)
		fp_nquery(qsp->xfr.msg, qsp->xfr.cp - qsp->xfr.msg,
			  log_get_stream(packet_channel));
#endif
	ret = sq_write(qsp, qsp->xfr.msg, qsp->xfr.cp - qsp->xfr.msg);
	if (ret >= 0)
		qsp->xfr.cp = NULL;
	return (ret);
}

/*
 * int
 * sx_addrr(qsp, name, dp)
 *	add name/dp's RR to the current assembly message.  if it won't fit,
 *	write current message out, renew the message, and then RR should fit.
 * return:
 *	-1 = the sx_flush() failed so we could not queue the full message.
 *	0 = one way or another, everything is fine.
 * side effects:
 *	on success, the ANCOUNT is incremented and the pointers are advanced.
 */
static int
sx_addrr(struct qstream *qsp, const char *dname, struct databuf *dp) {
	HEADER *hp = (HEADER *)qsp->xfr.msg;
	u_char **edp = qsp->xfr.ptrs + sizeof qsp->xfr.ptrs / sizeof(u_char*);
	int n;

	if (qsp->xfr.cp != NULL) {
		if (qsp->xfr.transfer_format == axfr_one_answer &&
		    sx_flush(qsp) < 0)
			return (-1);
	}
	if (qsp->xfr.cp == NULL)
		sx_newmsg(qsp);
	n = make_rr(dname, dp, qsp->xfr.cp, qsp->xfr.eom - qsp->xfr.cp,
		    0, qsp->xfr.ptrs, edp);
	if (n < 0) {
		if (sx_flush(qsp) < 0)
			return (-1);
		if (qsp->xfr.cp == NULL)
			sx_newmsg(qsp);
		n = make_rr(dname, dp, qsp->xfr.cp, qsp->xfr.eom - qsp->xfr.cp,
			    0, qsp->xfr.ptrs, edp);
		INSIST(n >= 0);
	}
	hp->ancount = htons(ntohs(hp->ancount) + 1);
	qsp->xfr.cp += n;
	return (0);
}

/*
 * int
 * sx_soarr(qsp)
 *	add the SOA RR's at the current level's top np to the assembly message.
 * return:
 *	0 = success
 *	-1 = write buffer full, cannot continue at this time
 * side effects:
 *	if progress was made, header and pointers will be advanced.
 */
static int
sx_soarr(struct qstream *qsp) {
	struct databuf *dp;

	foreach_rr(dp, qsp->xfr.top, T_SOA, qsp->xfr.class, qsp->xfr.zone) {
		if (sx_addrr(qsp, zones[qsp->xfr.zone].z_origin, dp) < 0) {
			/* RR wouldn't fit. Bail out. */
			return (-1);
		}
		return (0);
	}
	ns_panic(ns_log_xfer_out, 1, "no SOA at zone top");
}

/*
 * int
 * sx_nsrrs(qsp)
 *	add the NS RR's at the current level's current np,
 *	to the assembly message
 * return:
 *	>1 = number of NS RRs added, note that there may be more
 *	0 = success, there are no more NS RRs at this level
 *	-1 = write buffer full, cannot continue at this time
 * side effects:
 *	if progress was made, header and pointers will be advanced.
 * note:
 *	this is meant for AXFR, which includes glue as part of the answer
 *	sections.  this is different from and incompatible with the additional
 *	data of a referral response.
 */
static int
sx_nsrrs(struct qstream *qsp) {
	struct databuf *dp, *tdp, *gdp;
	struct namebuf *gnp, *tnp, *top;
	struct hashbuf *htp;
	const char *fname;
	int rrcount, class;

	class = qsp->xfr.class;
	top = qsp->xfr.top;
	rrcount = 0;
	for ((void)NULL;
	     (dp = qsp->xfr.lev->dp) != NULL;
	     qsp->xfr.lev->dp = dp->d_next) {
		/* XYZZY foreach_rr? */
		if (dp->d_class != class && class != C_ANY)
			continue;
		if (dp->d_rcode)
			continue;
		/*
		 * It might not be in the same zone, if we are authoritative
		 * for both parent and child, but it does have to be a zone.
		 *
		 * XXX: this is sort of a bug, since it means we merge the
		 *	@ NS RRset into our parent's zone.  But that is what
		 *	db_load() does, so for now we have no choice.
		 */
		if (dp->d_zone == DB_Z_CACHE)
			continue;
		if (dp->d_type != T_NS)
			continue;
		if (!(qsp->xfr.lev->flags & SXL_GLUING)) {
			if (sx_addrr(qsp, qsp->xfr.lev->dname, dp) < 0) {
				/* RR wouldn't fit. Bail out. */
				return (-1);
			}
			rrcount++;
		}

		/*
		 * Glue the sub domains together by sending the address
		 * records for the sub domain name servers along if necessary.
		 * Glue is necessary if the server is in any zone delegated
		 * from the current (top) zone.  Such a delegated zone might
		 * or might not be that referred to by the NS record now
		 * being handled.
		 */
		htp = hashtab;
		gnp = nlookup((char *)dp->d_data, &htp, &fname, 0);
		if (gnp == NULL || fname != (char *)dp->d_data)
			continue;
		for (tnp = gnp;
		     tnp != NULL && tnp != top;
		     tnp = tnp->n_parent)
			(void)NULL;
		if (tnp == NULL && NAME(*top)[0] != '\0')
			continue;  /* name server is not below top domain */
		for (tnp = gnp;
		     tnp != NULL && tnp != top;
		     tnp = tnp->n_parent) {
			foreach_rr(tdp, tnp, T_NS, class, DB_Z_CACHE)
				break;
			/* If we found a zone cut, we're outta here. */
			if (tdp != NULL)
				break;
		}
		/* If name server is not in a delegated zone, skip it. */
		if (tnp == top || (tnp == NULL && NAME(*top)[0] == '\0'))
			continue;
		/* Now we know glue records are needed.  Send them. */
		qsp->xfr.lev->flags |= SXL_GLUING;
		foreach_rr(gdp, gnp, T_A, class, DB_Z_CACHE)
			if (sx_addrr(qsp, fname, gdp) < 0) {
				/*
				 * Rats.  We already sent the NS RR, too.
				 * Note that SXL_GLUING is being left on.
				 */
				return (-1);
			}
		qsp->xfr.lev->flags &= ~SXL_GLUING;
	}
	return (rrcount);
}

/*
 * int
 * sx_allrrs(qsp)
 *	add the non-(SOA,NS) RR's at the current level's current np,
 *	to the assembly message
 * return:
 *	>0 = number of RR's added, note that there may be more
 *	0 = success, there are no more RRs at this level
 *	-1 = write buffer full, cannot continue at this time
 * side effects:
 *	if progress was made, header and pointers will be advanced.
 * note:
 *	this is meant for AXFR, which includes glue as part of the answer
 *	sections.  this is different from and incompatible with the additional
 *	data of a referral response.
 */
static int
sx_allrrs(struct qstream *qsp) {
	struct databuf *dp, *tdp, *gdp;
	struct namebuf *gnp, *tnp, *top;
	struct hashbuf *htp;
	const char *fname;
	int rrcount, class;
	u_int zone;

	class = qsp->xfr.class;
	top = qsp->xfr.top;
	zone = qsp->xfr.zone;
	rrcount = 0;
	for ((void)NULL;
	     (dp = qsp->xfr.lev->dp) != NULL;
	     qsp->xfr.lev->dp = dp->d_next) {
		/* XYZZY foreach_rr? */
		if (dp->d_class != class && class != C_ANY)
			continue;
		if (dp->d_rcode)
			continue;
		if (dp->d_zone != zone || stale(dp))
			continue;
		if (dp->d_type == T_SOA || dp->d_type == T_NS)
			continue;
		/* XXXRTH I presume this is still relevant and that
		   this is the right place... */
#if 0 /* Not yet implemented.  Only a SHOULD in the I-D.  -gnu@toad.com */
		/* skip the SIG AXFR record because we did it first too. */
		if (dp->d_type == T_SIG) {
			int sig_rrtype = GETSHORT (dp->d_data);
			if (sig_rrtype == T_AXFR)
				continue;
		}
#endif /* 0 */
		INSIST(!(qsp->xfr.lev->flags & SXL_GLUING));

		if (sx_addrr(qsp, qsp->xfr.lev->dname, dp) < 0) {
			/* RR wouldn't fit. Bail out. */
			return (-1);
		}
		rrcount++;
	}
	return (rrcount);
}

/*
 * void
 * sx_sendlev(qsp)
 *	send all the RRs at the current level (really a domain name), and
 *	do a decomposed recursion to get all subdomains up to and including
 *	but not exceeding bottom zone cuts.
 * side effects:
 *	advances qsp->xfr pointers.  changes qsp->xfr.lev quite often.
 *	causes messages to be sent to a remote TCP client.  changes the
 *	qsp->xfr.state at the end of the topmost level.  changes the
 *	qsp->xfr.lev->state several times per domain name.
 */
static void
sx_sendlev(struct qstream *qsp) {
	struct qs_x_lev *lev;
	int rrcount;

 again:
	lev = qsp->xfr.lev;
	switch (lev->state) {
	    case sxl_ns: {
		while (lev->dp) {
			rrcount = sx_nsrrs(qsp);
			/* If we can't pack this one in, come back later. */
			if (rrcount < 0)
				return;
			/*
			 * NS RRs other than those at the
			 * zone top are zone cuts.
			 */
			if (rrcount > 0 && qsp->xfr.top != lev->np)
				lev->flags |= SXL_ZONECUT;
		}
		/* No more DP's for the NS RR pass on this NP. */
		if (lev->flags & SXL_ZONECUT) {
			/* Zone cut, so go directly to end of level. */
			break;
		}
		/* No NS RR's, so it's safe to send other types. */
		lev->state = sxl_all;
		lev->dp = lev->np->n_data;
		goto again;
	    }
	    case sxl_all: {
		while (lev->dp) {
			/* If we can't pack this one in, come back later. */
			if (sx_allrrs(qsp) < 0)
				return;
		}
		/* No more non-NS DP's for this NP, do subdomains. */
		lev->state = sxl_sub;
		goto again;
	    }
	    case sxl_sub: {
		struct namebuf *np;

		/* Get next in-use hash chain if we're not following one. */
		while (lev->nnp == NULL) {
			/* If no, or no more subdomains, end of level. */
			if (lev->npp == NULL || lev->npp == lev->npe)
				break;
			lev->nnp = *lev->npp++;
		}
		/* If we encountered the end of the level, we're outta here. */
		if ((np = lev->nnp) == NULL)
			break;
		/* Next time, we'll do the following NP, or the next chain. */
		lev->nnp = np->n_next;
		/* Skip our own NP if it appears as a subdom (as in root). */
		if (np != lev->np)
			sx_pushlev(qsp, np);
		goto again;
	    }
	    default:
		abort();
	}

	/* End of level. Pop it off the stack. */

	if ((qsp->xfr.lev = sx_freelev(lev)) == NULL) {
		/* End of topmost level. */
		qsp->xfr.state = s_x_lastsoa;
		sq_writeh(qsp, sx_sendsoa);
		return;
	}
	goto again;
}

/*
 * void
 * sx_sendsoa(qsp)
 *	send either the first or last SOA needed for an AXFR.
 * side effects:
 *	changes qsp->xfr.state.  adds RR to output buffer.
 */
static void
sx_sendsoa(struct qstream *qsp) {
	if (sx_soarr(qsp) == -1)
		return;		/* No state change, come back here later. */

	switch (qsp->xfr.state) {
	    case s_x_firstsoa: {
		/* Next thing to do is send the zone. */
		qsp->xfr.state = s_x_zone;
		sq_writeh(qsp, sx_sendlev);
		break;
	    }
	    case s_x_lastsoa: {
		/* Next thing to do is go back and wait for another query. */
		(void)sx_flush(qsp);
		qsp->xfr.state = s_x_done;
		sq_writeh(qsp, sq_flushw);
		break;
	    }
	    default: {
		ns_panic(ns_log_xfer_out, 1,
			 "unexpected state %d in sx_sendsoa", qsp->xfr.state);
	    }
	}
}

/* int
 * sx_pushlev(qsp, np)
 *	manage the decomposed recursion.  set up for a new level (domain).
 * returns:
 *	0 = success
 *	-1 = failure (check errno)
 */
static int
sx_pushlev(struct qstream *qsp, struct namebuf *np) {
	struct qs_x_lev *new = memget(sizeof *new);
	struct hashbuf *htp;

	if (!new) {
		errno = ENOMEM;
		return (-1);
	}
	memset(new, 0, sizeof *new);
	new->state = sxl_ns;
	new->np = np;
	new->dp = np->n_data;
	getname(np, new->dname, sizeof new->dname);
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
	htp = ((new->dname[0] == '\0') ? hashtab : np->n_hash);
	if (htp) {
		new->npp = htp->h_tab;
		new->npe = htp->h_tab + htp->h_size;
	} else {
		new->npp = NULL;
		new->npe = NULL;
	}
	new->nnp = NULL;
	new->next = qsp->xfr.lev;
	qsp->xfr.lev = new;
	return (0);
}

/*
 * qs_x_lev *
 * sx_freelev(lev)
 *	free the memory occupied by a level descriptor
 * return:
 *	pointer to "next" level descriptor
 */
static struct qs_x_lev *
sx_freelev(struct qs_x_lev *lev) {
	struct qs_x_lev *next = lev->next;

	memput(lev, sizeof *lev);
	return (next);
}
