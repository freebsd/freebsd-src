#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)db_update.c	4.28 (Berkeley) 3/21/91";
static char rcsid[] = "$Id: db_update.c,v 1.3 1995/08/20 21:18:31 peter Exp $";
#endif /* not lint */

/*
 * ++Copyright++ 1986, 1990
 * -
 * Copyright (c) 1986, 1990
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

#include <stdio.h>
#include <syslog.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include "named.h"

static void			fixttl __P((struct databuf *));
static int			db_cmp __P((struct databuf *,
					    struct databuf *));

/* int
 * isRefByNS(name, htp)
 *	recurse through all of `htp' looking for NS RR's that refer to `name'.
 * returns:
 *	nonzero if at least one such NS RR exists
 * cautions:
 *	this is very expensive; probably you only want to use on fcachetab.
 */
static int
isRefByNS(name, htp)
	char name[];
	struct hashbuf *htp;
{
	register struct namebuf *np;
	register struct databuf *dp;

	for (np = htp->h_tab[0];  np != NULL;  np = np->n_next) {
		for (dp = np->n_data;  dp != NULL;  dp = dp->d_next) {
			if ((dp->d_class == C_ANY ||
			     dp->d_class == C_IN ||
			     dp->d_class == C_HS) &&
			    dp->d_type == T_NS &&
#ifdef NCACHE
			    !dp->d_rcode &&
#endif
			    !strcasecmp(name, (char *)dp->d_data)) {
				return (1);
			}
		}
		if (np->n_hash && isRefByNS(name, np->n_hash))
			return (1);
	}
	return (0);
}


/* int
 * findMyZone(struct namebuf *np)
 *	surf the zone cuts and find this zone the hard way
 * return value:
 *	zone number or DB_Z_CACHE if it's outside a zone
 * interesting cases:
 *	    DEC.COM SOA (primary)
 *	CRL.DEC.COM NS  (in primary)
 *		if you start at CRL.. here, you find the DEC.COM zone
 *		if you start at NS.CRL.. here, you're in the cache
 *	    DEC.COM SOA (primary)
 *	CRL.DEC.COM NS  (in primary)
 *	CRL.DEC.COM SOA (secondary)
 *	CRL.DEC.COM NS  (in secondary)
 *		if you start at CRL.. here, you find the CRL.DEC.COM zone
 *		if you start at NS.CRL.. here, you're in the CRL.. zone
 */
int
findMyZone(np, class)
	struct namebuf *np;
	register int class;
{
	for (; np; np = np->n_parent) {
		register struct databuf *dp;

		/* if we encounter an SOA, we're in its zone (which can be
		 * the cache or an authoritative zone, depending).
		 */
		for (dp = np->n_data; dp; dp = dp->d_next)
			if (match(dp, class, T_SOA))
				return (dp->d_zone);

		/* if we find an NS at some node without having seen an SOA
		 * (above), then we're out in the cache somewhere.
		 */
		for (dp = np->n_data; dp; dp = dp->d_next)
			if (match(dp, class, T_NS))
				return (DB_Z_CACHE);
	}

	/* getting all the way to the root without finding an NS or SOA
	 * probably means that we are in deep dip, but we'll treat it as
	 * being in the cache.  (XXX?)
	 */
	return (DB_Z_CACHE);
}


#ifdef NO_GLUE
#define ISVALIDGLUE(xdp) ((xdp)->d_type == T_NS || (xdp)->d_type == T_A)
#else
#define ISVALIDGLUE(xdp) (1)
#endif /*NO_GLUE*/


/* int
 * db_update(name, odp, newdp, flags, htp)
 *	update data base node at `name'.  `flags' controls the action.
 * side effects:
 *	inverse query tables modified, if we're using them.
 * return value:
 *	OK - success
 *	NONAME - name doesn't exist
 *	AUTH - you can't do that
 *	DATAEXISTS - there's something there and DB_NODATA was specified
 *	NODATA - there's no data, and (DB_DELETE or DB_MEXIST) was spec'd
 *
 *	Policy: How to add data if one more RR is -ve data
 *
 *	NEND	NOERROR_NODATA
 *	NXD	NXDOMAIN
 *
 *				match
 *				old
 *			Data	NEND	NXD
 *		Data	Merge	Data	Data
 *	new	NEND	NEND	NEND	NEND
 *		NXD	NXD	NXD	NXD
 *
 *			     no match
 *				old
 *			Data	NEND	NXD
 *		Data	Merge	Merge	Data
 *	new	NEND	Merge	Merge	NEND
 *		NXD	NXD	NXD	NXD
 *
 */
/* XXX:	this code calls nlookup, which can create namebuf's.  if this code
 *	has to exit with a fatal error, it should scan from the new np upward
 *	and for each node which has no children and no data it should remove
 *	the namebuf.  design notes: (1) there's no harm in doing this even if
 *	success occurred; (2) stopping on the first nonremovable np is optimal;
 *	the code for removal should be taken out of remove_zone() and made
 *	general enough for this use, and for remove_zone()'s continued use.
 *							vix, 21jul94
 */
int
db_update(name, odp, newdp, flags, htp)
	char name[];
	struct databuf *odp, *newdp;
	int flags;
	struct hashbuf *htp;
{
	register struct databuf *dp, *pdp;
	register struct namebuf *np;
	int zn, isHintNS;
	const char *fname;

	dprintf(3, (ddt, "db_update(%s, 0x%lx, 0x%lx, 0%o, 0x%lx)%s\n",
		    name, (u_long)odp, (u_long)newdp, flags, (u_long)htp,
		    (odp && (odp->d_flags&DB_F_HINT)) ? " hint":"" ));
	np = nlookup(name, &htp, &fname, newdp != NULL);
	if (np == NULL || fname != name)
		return (NONAME);

	/* don't let nonauthoritative updates write in authority zones */
	if (newdp && ((zn = findMyZone(np, newdp->d_class)) != DB_Z_CACHE) &&
#ifdef STUBS
			    (zones[zn].z_type != Z_STUB) &&
#endif
			    (flags & DB_NOTAUTH)) {
		int foundRR = 0;

		/*
		 * Don't generate the warning if the update
		 * would have been harmless (identical data).
		 */
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			if (!db_cmp(dp, newdp)) {
				foundRR++;
				break;
			}
		}
		if (!foundRR)
			dprintf(5, (ddt,
				    "[%s].%d update? to auth zone \"%s\" (%s)",
				    inet_ntoa(from_addr.sin_addr),
				    ntohs(from_addr.sin_port),
				    zones[zn].z_origin,
				    name));
		return (AUTH);
	}

	if (newdp && zn && !(flags & DB_NOTAUTH)) {
		if (db_getclev(zones[zn].z_origin) > newdp->d_clev) {
			dprintf(5,(ddt, "attempted update child zone %s, %s\n",
				zones[zn].z_origin, name));
			return(AUTH);
		}
	}

	/* some special checks for root NS' A RR's */
	isHintNS = isRefByNS(name, fcachetab);
#ifdef DEPRECATED
	if (newdp && isHintNS && newdp->d_type == T_A) {
		/* upgrade credibility of additional data for rootsrv addrs */
		if (newdp->d_cred == DB_C_ADDITIONAL) {
			dprintf(3, (ddt,
				    "upgrading credibility for A RR (%s)\n",
				    name));
			/* XXX:	should copy NS RR's, but we really just want
			 *	to prevent deprecation later so this will do.
			 */
			newdp->d_cred = DB_C_ANSWER;
			newdp->d_clev = 0;
		}
	}
#endif

        /* Reflect certain updates in hint cache also... */
	/* Don't stick data we are authoritative for in hints. */
        if (!(flags & DB_NOHINTS) &&
	    (flags & DB_PRIMING) &&
	    (odp != NULL) &&
	    (htp != fcachetab) &&
	    (odp->d_zone <= 0) &&
	    !(odp->d_flags & DB_F_HINT) &&
#ifdef NCACHE
	    (!newdp || !newdp->d_rcode) &&
#endif
            ((name[0] == '\0' && odp->d_type == T_NS) ||
	     (odp->d_type == T_A && isHintNS)
	     )
	    )
        {
		dprintf(3, (ddt, "db_update: hint '%s' %d\n",
			    name, odp->d_ttl));
		dp = savedata(odp->d_class, odp->d_type, odp->d_ttl,
			odp->d_data, odp->d_size);
		dp->d_zone = DB_Z_CACHE;
		dp->d_flags = DB_F_HINT;
		dp->d_cred = DB_C_CACHE;
		dp->d_clev = 0;
		if (db_update(name,
			      dp, dp,
			      (flags|DB_NOHINTS),
			      fcachetab)
		    != OK) {
			dprintf(3, (ddt, "db_update: hint %lx freed\n",
				    (u_long)dp));
			(void) free((char *)dp);
		}
        }

	if (odp != NULL) {
		int foundRR = 0;

		pdp = NULL;
		for (dp = np->n_data; dp != NULL; ) {
			if (!match(dp, odp->d_class, odp->d_type)) {
				/* {class,type} doesn't match.  these are
				 * the aggregation cases.
				 */
				if ((dp->d_type == T_CNAME ||
				     odp->d_type == T_CNAME) &&
				    odp->d_class == dp->d_class &&
				    odp->d_mark == dp->d_mark &&
#ifdef NCACHE
				    /* neither the odp nor the new dp are
				     * negatively cached records...
				     */
				    !dp->d_rcode &&
				    !odp->d_rcode &&
#endif /*NCACHE*/
				    zones[odp->d_zone].z_type != Z_CACHE) {
					syslog(LOG_INFO,
				     "%s has CNAME and other data (illegal)\n",
					    name);
					goto skip;
				}
				if (!newdp || newdp->d_class != dp->d_class)
					goto skip;

				/* if the new data is authorative 
				 * remove any data for this domain with
				 * the same class that isn't as credable
				 */
				if (newdp->d_cred == DB_C_ZONE &&
				    newdp->d_cred > dp->d_cred)
					/* better credibility and the old datum
					 * was not from a zone file.  remove
					 * the old datum.
					 */
					goto delete;

#if 0	/* caught by findMyZone() now. */
				/* if we have authoritative data for a
				 * node, don't add in other data.
				 */
				if (dp->d_cred == DB_C_ZONE &&
				    newdp->d_cred < dp->d_cred)
					return (AUTH);
#endif

				/* if the new data is authoritative but
				 * but isn't as credible, reject it.
				 */
				if (newdp->d_cred == DB_C_ZONE &&
				    dp->d_cred == DB_C_ZONE) {
					/* Both records are from a zone file.
					 * If their credibility levels differ,
					 * we're dealing with a zone cut.  The
					 * record with lower clev is from the
					 * upper zone's file and is therefore
					 * glue.
					 */
					if (newdp->d_clev < dp->d_clev) {
					    if (!ISVALIDGLUE(newdp)) {
						syslog(LOG_INFO,
		"domain %s %s record in zone %s should be in zone %s, ignored",
						 name, p_type(newdp->d_type),
						 zones[newdp->d_zone].z_origin,
						 zones[dp->d_zone].z_origin);
					    }
					    return (AUTH);
					}
					if (newdp->d_clev > dp->d_clev) {
					    if (!ISVALIDGLUE(dp)) {
						syslog(LOG_INFO,
		"domain %s %s record in zone %s should be in zone %s, deleted",
						 name, p_type(dp->d_type),
						 zones[dp->d_zone].z_origin,
						 zones[newdp->d_zone].z_origin);
					    }
					    goto delete;
					}
				}
#ifdef NCACHE
				/* process NXDOMAIN */
				/* policy */
				if (newdp->d_rcode == NXDOMAIN) {
					if (dp->d_cred < DB_C_AUTH)
						goto delete;
					else
						return (DATAEXISTS);
				}

				if (dp->d_rcode == NXDOMAIN)
					goto delete;

				/* process NOERROR_NODATA */
				/* NO PROCESSING REQUIRED */
#endif /*NCACHE*/
				goto skip;
			} /*if {class,type} did not match*/

			/* {type,class} did match.  this is the replace case.
			 */
			dprintf(5, (ddt,
			   "db_update: flags = %#x, sizes = %d, %d (cmp %d)\n",
				    flags, odp->d_size, dp->d_size,
				    db_cmp(dp, odp)));
			if (newdp) {
				dprintf(4, (ddt,
	     "credibility for %s is %d(%d) from [%s].%d, is %d(%d) in cache\n",
					    *name? name : ".",
					    newdp->d_cred,
					    newdp->d_clev,
					    inet_ntoa(from_addr.sin_addr),
					    ntohs(from_addr.sin_port),
					    dp->d_cred,
					    dp->d_clev));
				if (newdp->d_cred > dp->d_cred) {
					/* better credibility.
					 * remove the old datum.
					 */
					goto delete;
				}
				if (newdp->d_cred < dp->d_cred) {
					/* credibility is worse.  ignore it. */
					return (AUTH);
				}
				if (newdp->d_cred == DB_C_ZONE &&
				    dp->d_cred == DB_C_ZONE ) {
					/* Both records are from a zone file.
					 * If their credibility levels differ,
					 * we're dealing with a zone cut.  The
					 * record with lower clev is from the
					 * upper zone's file and is therefore
					 * glue.
					 */

					/* XXX - Tricky situation here is you
					 * have 2 zones a.b.c and sub.a.b.c
					 * being served by the same server.
					 * named will send NS records for
					 * sub.a.b.c during zone transfer of
					 * a.b.c zone.  If we're secondary for
					 * both zones, and we reload zone
					 * a.b.c, we'll get the NS records
					 * (and possibly A records to go with
					 * them?) for sub.a.b.c as part of the
					 * a.b.c zone transfer.  But we've
					 * already got a more credible record
					 * from the sub.a.b.c zone.  So we want
					 * to ignore the new record, but we
					 * shouldn't syslog because there's
					 * nothing the user can do to prevent
					 * the situation.  Perhaps we should
					 * only complain when we are primary?
	 				 */

					if (newdp->d_clev < dp->d_clev) {
					    if (!ISVALIDGLUE(newdp)) {
						syslog(LOG_INFO,
		"domain %s %s record in zone %s should be in zone %s, ignored",
						 name, p_type(newdp->d_type),
						 zones[newdp->d_zone].z_origin,
						 zones[dp->d_zone].z_origin);
					    }
					    return (AUTH);
					}
					if (newdp->d_clev > dp->d_clev) {
					    if (!ISVALIDGLUE(dp)) {
						syslog(LOG_INFO,
		"domain %s %s record in zone %s should be in zone %s, deleted",
						 name, p_type(dp->d_type),
						 zones[dp->d_zone].z_origin,
						 zones[newdp->d_zone].z_origin);
					    }
					    goto delete;
					}
				}

				/* credibility is the same.
				 * let it aggregate in the normal way.
				 */
#ifdef NCACHE
				/*
				 * if the new or old RR is -ve, delete old.
				 */
				if (dp->d_rcode || newdp->d_rcode) {
					/* XXX: how can a zone rr be neg? */
					if (dp->d_cred != DB_C_ZONE)
						goto delete;
					else
						return (DATAEXISTS);
				}
#endif
				/*
				 *  Some RR types should not be aggregated.
				 */
				if (dp->d_type == T_SOA)
					goto delete;
				if (dp->d_type == T_WKS &&
				    !bcmp(dp->d_data, newdp->d_data,
					  INT32SZ + sizeof(u_char)))
					goto delete;
			}
			if ((flags & DB_NODATA) && !db_cmp(dp, odp)) {
				/* refresh ttl if cache entry */
				if (dp->d_zone == 0) {
					if (odp->d_zone != 0) {	/* XXX */
						/* changing cache->auth */
						dp->d_zone = odp->d_zone;
						dp->d_ttl = odp->d_ttl;
						dprintf(4, (ddt,
				    "db_update: cache entry now in auth zone\n"
							    ));
						return (DATAEXISTS);
					}
					fixttl(odp);
					if (odp->d_ttl > dp->d_ttl)
						dp->d_ttl = odp->d_ttl;
					dprintf(3, (ddt,
						"db_update: new ttl %ld +%d\n",
						    (u_long)dp->d_ttl,
						    dp->d_ttl - tt.tv_sec));
				}
				return (DATAEXISTS);
			}
			/*
			 * If the old databuf has some data, check that the
			 * data matches that in the new databuf (so UPDATED
			 * will delete only the matching RR)
			 */
			if (odp->d_size > 0)
				if (db_cmp(dp, odp))
					goto skip;
			foundRR = 1;
			if (flags & DB_DELETE) {
 delete:			dp = rm_datum(dp, np, pdp);
			} else {
 skip:				pdp = dp;
				dp = dp->d_next;
			}
		}
                if (!foundRR) {
			if (flags & DB_DELETE)
				return (NODATA);
			if (flags & DB_MEXIST)
				return (NODATA);
		}
	}
	if (newdp == NULL)
		return (OK);
	/* XXX:	empty nodes bypass credibility checks above; should check
	 *	response source address here if flags&NOTAUTH.
	 */
	fixttl(newdp);
	dprintf(3, (ddt, "db_update: adding%s %lx\n",
		    (newdp->d_flags&DB_F_HINT) ? " hint":"", (u_long)newdp));
#ifdef INVQ
	if (!(newdp->d_flags & DB_F_HINT))
		addinv(np, newdp);	/* modify inverse query tables */
#endif

#ifdef STATS
	if (!newdp->d_zone && !(newdp->d_flags & DB_F_HINT))
		newdp->d_ns = nameserFind(from_addr.sin_addr, NS_F_INSERT);
#endif

	/* Add to end of list, generally preserving order */
	newdp->d_next = NULL;
	if ((dp = np->n_data) == NULL)  {
#ifdef DATUMREFCNT
		newdp->d_rcnt = 1;
#endif
		np->n_data = newdp;
		return (OK);
	}
	while (dp->d_next != NULL) {
		if ((flags & DB_NODATA) && !db_cmp(dp, newdp))
			return (DATAEXISTS);
		dp = dp->d_next;
	}
	if ((flags & DB_NODATA) && !db_cmp(dp, newdp))
		return (DATAEXISTS);
#ifdef	DATUMREFCNT
	newdp->d_rcnt = 1;
#endif
	dp->d_next = newdp;
	return (OK);
}

static void
fixttl(dp)
	register struct databuf *dp;
{
	if (dp->d_zone == 0 && !(dp->d_flags & DB_F_HINT)) {
		if (dp->d_ttl <= tt.tv_sec)
			return;
		else if (dp->d_ttl < tt.tv_sec+min_cache_ttl)
			dp->d_ttl = tt.tv_sec+min_cache_ttl;
		else if (dp->d_ttl > tt.tv_sec+max_cache_ttl)
			dp->d_ttl = tt.tv_sec+max_cache_ttl;
	}
	return;
}

/*
 * Compare type, class and data from databufs for equivalence.
 * Must be case insensitive for some domain names.
 * Return 0 if equivalent, nonzero otherwise.
 */
static int
db_cmp(dp1, dp2)
	register struct databuf *dp1, *dp2;
{
	register u_char *cp1, *cp2;
	int len, len2;

	if (dp1->d_type != dp2->d_type || dp1->d_class != dp2->d_class)
		return (1);
	if (dp1->d_size != dp2->d_size)
		return (1);
	if (dp1->d_mark != dp2->d_mark)
		return (1);		/* old and new RR's are distinct */
#ifdef NCACHE
	if (dp1->d_rcode && dp2->d_rcode)
		return ((dp1->d_rcode == dp1->d_rcode)?0:1);
	if (dp1->d_rcode || dp2->d_rcode)
		return (1);
#endif

	switch (dp1->d_type) {

	case T_A:
	case T_UID:
	case T_GID:
	case T_WKS:
	case T_NULL:
	case T_NSAP:
	case T_LOC:
#ifdef ALLOW_T_UNSPEC
        case T_UNSPEC:
#endif
		return (bcmp(dp1->d_data, dp2->d_data, dp1->d_size));

	case T_NS:
	case T_CNAME:
	case T_PTR:
	case T_MB:
	case T_MG:
	case T_MR:
	case T_UINFO:
		return (strcasecmp((char *)dp1->d_data, (char *)dp2->d_data));

	case T_HINFO:
	case T_ISDN:
		cp1 = dp1->d_data;
		cp2 = dp2->d_data;
		len = *cp1;
		len2 = *cp2;
		if (len != len2)
                      return (1);
		if (strncasecmp((char *)++cp1, (char *)++cp2, len))
			return (1);
		cp1 += len;
		cp2 += len;
		len = *cp1;
		len2 = *cp2;
		if (len != len2)
                      return (1);
		return (strncasecmp((char *)++cp1, (char *)++cp2, len));

	case T_SOA:
	case T_MINFO:
	case T_RP:
		if (strcasecmp((char *)dp1->d_data, (char *)dp2->d_data))
			return (1);
		cp1 = dp1->d_data + strlen((char *)dp1->d_data) + 1;
		cp2 = dp2->d_data + strlen((char *)dp2->d_data) + 1;
		if (dp1->d_type != T_SOA)
			return (strcasecmp((char *)cp1, (char *)cp2));
		if (strcasecmp((char *)cp1, (char *)cp2))
			return (1);
		cp1 += strlen((char *)cp1) + 1;
		cp2 += strlen((char *)cp2) + 1;
		return (bcmp(cp1, cp2, INT32SZ * 5));

	case T_MX:
	case T_AFSDB:
	case T_RT:
		cp1 = dp1->d_data;
		cp2 = dp2->d_data;
		if (*cp1++ != *cp2++ || *cp1++ != *cp2++)	/* cmp prio */
			return (1);
		return (strcasecmp((char *)cp1, (char *)cp2));

	case T_PX:
		cp1 = dp1->d_data;
		cp2 = dp2->d_data;
		if (*cp1++ != *cp2++ || *cp1++ != *cp2++)       /* cmp prio */
			return (1);
		if (strcasecmp((char *)cp1, (char *)cp2))
			return (1);
		cp1 += strlen((char *)cp1) + 1;
		cp2 += strlen((char *)cp2) + 1;
		return (strcasecmp((char *)cp1, (char *)cp2));

	case T_TXT:
	case T_X25:
		if (dp1->d_size != dp2->d_size)
			return (1);
		return (bcmp(dp1->d_data, dp2->d_data, dp1->d_size));

	default:
		return (1);
	}
}
