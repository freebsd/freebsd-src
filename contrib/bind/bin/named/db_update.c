#if !defined(lint) && !defined(SABER)
static char sccsid[] = "@(#)db_update.c	4.28 (Berkeley) 3/21/91";
static char rcsid[] = "$Id: db_update.c,v 8.23 1998/02/13 20:01:38 halley Exp $";
#endif /* not lint */

/*
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
 * Portions Copyright (c) 1996, 1997 by Internet Software Consortium.
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

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <resolv.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>

#include <isc/eventlib.h>
#include <isc/logging.h>

#include "port_after.h"

#include "named.h"

/* int
 * isRefByNS(name, htp)
 *	recurse through all of `htp' looking for NS RR's that refer to `name'.
 * returns:
 *	nonzero if at least one such NS RR exists
 * cautions:
 *	this is very expensive; probably you only want to use on fcachetab.
 */
static int
isRefByNS(const char *name, struct hashbuf *htp) {
	struct namebuf *np;
	struct databuf *dp;

	for (np = htp->h_tab[0]; np != NULL; np = np->n_next) {
		for (dp = np->n_data; dp != NULL; dp = dp->d_next) {
			if ((dp->d_class == C_ANY ||
			     dp->d_class == C_IN ||
			     dp->d_class == C_HS) &&
			    dp->d_type == T_NS &&
			    !dp->d_rcode &&
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
 * findMyZone(struct namebuf *np, int class)
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
findMyZone(struct namebuf *np, int class) {
	for ((void)NULL; np; np = np_parent(np)) {
		struct databuf *dp;

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


#define ISVALIDGLUE(xdp) ((xdp)->d_type == T_NS || (xdp)->d_type == T_A \
			 || (xdp)->d_type == T_AAAA)

/* int
 * db_update(name, odp, newdp, savedpp, flags, htp, from)
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
 *	the code for removal should be taken out of clean_cache() and made
 *	general enough for this use, and for clean_cache()'s continued use.
 *							vix, 21jul94
 */
int
db_update(const char *name,
	  struct databuf *odp, struct databuf *newdp, 
	  struct databuf **savedpp,
	  int flags, struct hashbuf *htp, struct sockaddr_in from)
{
	struct databuf *dp, *pdp;
	struct namebuf *np;
	int zn, isHintNS;
	int check_ttl = 0;
	const char *fname;
#ifdef BIND_UPDATE
	int i, found_other_ns = 0;
	struct databuf *tmpdp;
        u_char *cp1, *cp2;
        u_int32_t dp_serial, newdp_serial;
#endif

	ns_debug(ns_log_db, 3, "db_update(%s, %#x, %#x, %#x, 0%o, %#x)%s",
		 name, odp, newdp, savedpp, flags, htp,
		 (odp && (odp->d_flags&DB_F_HINT)) ? " hint" : "");
	np = nlookup(name, &htp, &fname, newdp != NULL);
	if (np == NULL || fname != name)
		return (NONAME);
	
	if (newdp && zones[newdp->d_zone].z_type == Z_PRIMARY)
		check_ttl = 1;

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
			ns_debug(ns_log_db, 5,
				 "[%s].%d update? to auth zone \"%s\" (%s)",
				 inet_ntoa(from.sin_addr),
				 ntohs(from.sin_port),
				 zones[zn].z_origin,
				 name);
		return (AUTH);
	}

	if (newdp && zn && !(flags & DB_NOTAUTH)) {
		if (nlabels(zones[zn].z_origin) > newdp->d_clev) {
			ns_debug(ns_log_db, 5,
				 "attempted update child zone %s, %s",
				 zones[zn].z_origin, name);
			return (AUTH);
		}
	}

	/* some special checks for root NS' A RR's */
	isHintNS = isRefByNS(name, fcachetab);
#ifdef DEPRECATED
	if (newdp && isHintNS && newdp->d_type == T_A) {
		/* upgrade credibility of additional data for rootsrv addrs */
		if (newdp->d_cred == DB_C_ADDITIONAL) {
			ns_debug(ns_log_db, 3,
				 "upgrading credibility for A RR (%s)",
				   name);
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
	    (DB_Z_SPECIAL(odp->d_zone)) &&
	    !(odp->d_flags & DB_F_HINT) &&
	    (!newdp || !newdp->d_rcode) &&
            ((name[0] == '\0' && odp->d_type == T_NS) ||
	     (odp->d_type == T_A && isHintNS)
	     )
	    )
        {
		ns_debug(ns_log_db, 3, "db_update: hint '%s' %u",
			 name, odp->d_ttl);
		dp = savedata(odp->d_class, odp->d_type, odp->d_ttl,
			      odp->d_data, odp->d_size);
		dp->d_zone = DB_Z_CACHE;
		dp->d_flags = DB_F_HINT;
		dp->d_cred = DB_C_CACHE;
		dp->d_clev = 0;
		if (db_update(name,
			      dp, dp, NULL,
			      (flags|DB_NOHINTS),
			      fcachetab, from)
		    != OK) {
			ns_debug(ns_log_db, 3,
				 "db_update: hint %#x freed", dp);
			db_freedata(dp);
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
				/* Check that CNAMEs are only accompanied by
				 * Secure DNS RR's (KEY, SIG, and NXT).
				 */
				if (((dp->d_type == T_CNAME &&
				      odp->d_type != T_KEY &&
				      odp->d_type != T_SIG &&
				      odp->d_type != T_NXT) ||
				     (odp->d_type == T_CNAME &&
				      dp->d_type != T_KEY &&
				      dp->d_type != T_SIG &&
				      dp->d_type != T_NXT)) &&
				    odp->d_class == dp->d_class &&
				    /* XXXRTH d_mark removed in 4.9.5,
				       but still here for dynamic
				       update */
				    odp->d_mark == dp->d_mark &&
				    !dp->d_rcode &&
				    !odp->d_rcode &&
#ifdef BIND_UPDATE
			 /* updating a CNAME with another CNAME is permitted */
				    (dp->d_type != T_CNAME ||
				     odp->d_type != T_CNAME) &&
#endif
				    zones[odp->d_zone].z_type != Z_CACHE) {
					ns_info(ns_log_db,
				     "%s has CNAME and other data (invalid)",
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
						ns_info(ns_log_db,
		"domain %s %s record in zone %s should be in zone %s, ignored",
						 name, p_type(newdp->d_type),
						 zones[newdp->d_zone].z_origin,
						 zones[dp->d_zone].z_origin);
					    }
					    return (AUTH);
					}
					if (newdp->d_clev > dp->d_clev) {
					    if (!ISVALIDGLUE(dp)) {
						ns_info(ns_log_db,
		"domain %s %s record in zone %s should be in zone %s, deleted",
						 name, p_type(dp->d_type),
						 zones[dp->d_zone].z_origin,
						 zones[newdp->d_zone].z_origin);
					    }
					    goto delete;
					}
				}

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

				goto skip;
			} /*if {class,type} did not match*/

			/*
			 * {type,class} did match.  This is the replace case.
			 */
			ns_debug(ns_log_db, 5,
			   "db_update: flags = %#x, sizes = %d, %d (cmp %d)",
				 flags, odp->d_size, dp->d_size,
				 db_cmp(dp, odp));
			if (newdp) {
				ns_debug(ns_log_db, 4,
	     "credibility for %s is %d(%d) from [%s].%d, is %d(%d) in cache",
					 *name ? name : ".",
					 newdp->d_cred,
					 newdp->d_clev,
					 inet_ntoa(from.sin_addr),
					 ntohs(from.sin_port),
					 dp->d_cred,
					 dp->d_clev);
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
						ns_info(ns_log_db,
		"domain %s %s record in zone %s should be in zone %s, ignored",
						 name, p_type(newdp->d_type),
						 zones[newdp->d_zone].z_origin,
						 zones[dp->d_zone].z_origin);
					    }
					    return (AUTH);
					}
					if (newdp->d_clev > dp->d_clev) {
					    if (!ISVALIDGLUE(dp)) {
						ns_info(ns_log_db,
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

				/*
				 *  Some RR types should not be aggregated.
				 */
				if (dp->d_type == T_SOA) {
#ifdef BIND_UPDATE
				        u_int32_t dp_ser, ndp_ser;
					u_char *dp_cp, *ndp_cp;

					dp_cp = findsoaserial(dp->d_data);
					ndp_cp = findsoaserial(newdp->d_data);
					GETLONG(dp_ser, dp_cp);
					GETLONG(ndp_ser, ndp_cp);
					
					if (SEQ_GT(ndp_ser, dp_ser))
					        goto delete;
					else
					        return (SERIAL);
#else
					goto delete;
#endif /*BIND_UPDATE*/
				}
				if (dp->d_type == T_WKS &&
				    !memcmp(dp->d_data, newdp->d_data,
					    INT32SZ + sizeof(u_char)))
					goto delete;
				if (dp->d_type == T_CNAME &&
				    !NS_OPTION_P(OPTION_MULTIPLE_CNAMES))
					goto delete;
#ifdef BIND_UPDATE
                                if (dp->d_type == T_SIG)
                                        /* 
					 * Type covered has already been
					 * checked.
					 */
                                        goto delete;
#endif
				if (check_ttl) {
					if (newdp->d_ttl != dp->d_ttl) 
					ns_warning(ns_log_db,
					"%s %s %s differing ttls: corrected",
						name[0]?name:".", 
						p_class(dp->d_class),
						p_type(dp->d_type));
					if (newdp->d_ttl > dp->d_ttl) {
						newdp->d_ttl = dp->d_ttl;
					} else {
						dp->d_ttl = newdp->d_ttl;
					} 
				}
			}
			if ((flags & DB_NODATA) && !db_cmp(dp, odp)) {
				/* Refresh ttl if cache entry. */
				if (dp->d_zone == DB_Z_CACHE) {
					if (odp->d_zone != DB_Z_CACHE) {
						/* Changing cache->auth. */
						dp->d_zone = odp->d_zone;
						dp->d_ttl = odp->d_ttl;
						ns_debug(ns_log_db, 4,
				  "db_update: cache entry now in auth zone");
						return (DATAEXISTS);
					}
					fixttl(odp);
					if (odp->d_ttl > dp->d_ttl)
						dp->d_ttl = odp->d_ttl;
					ns_debug(ns_log_db, 3,
					       "db_update: new ttl %u +%lu",
						 dp->d_ttl,
						(u_long)(dp->d_ttl - tt.tv_sec)
						);
				}
				return (DATAEXISTS);
			}
			/*
			 * If the old databuf has some data, check that the
			 * data matches that in the new databuf.
			 */
			if (odp->d_size > 0)
				if (db_cmp(dp, odp))
					goto skip;
			if (odp->d_clev < dp->d_clev)
				goto skip;
			if (odp->d_cred < dp->d_cred)
				goto skip;
#ifdef BIND_UPDATE
			if (!strcasecmp(name, zones[dp->d_zone].z_origin) &&
			    !newdp) {
				/* do not delete SOA or NS records as a set */
				/* XXXRTH isn't testing d_size unnecessary? */
				if ((odp->d_size == 0) &&
				    (odp->d_class == C_ANY) &&
				    (odp->d_type == T_ANY ||
				     odp->d_type == T_SOA ||
				     odp->d_type == T_NS) &&
				    (dp->d_type == T_SOA ||
				     dp->d_type == T_NS))
					goto skip;
				/* XXXRTH I added this to prevent SOA deletion
				   I'm using the same style of comparison as
				   the other code in this section.  Do we
				   really need to look at dp->d_type here?
				   We're in the "match" section... */
				if ((odp->d_type == T_SOA) && 
				    (dp->d_type == T_SOA))
					goto skip;
				/* do not delete the last NS record
				   for the zone */
				if ((odp->d_type == T_NS) &&
				    (dp->d_type == T_NS)) {
					found_other_ns = 0;
					for (tmpdp = np->n_data;
					     tmpdp && !found_other_ns;
					     tmpdp = tmpdp->d_next)
						if ((tmpdp->d_type == T_NS) &&
						    (tmpdp != dp))
							found_other_ns = 1;
					if (!found_other_ns) {
						ns_debug(ns_log_db, 3,
		     "cannot delete last remaining NS record for zone %s",
							 name);
						goto skip;
					}
				}
			}
#endif

			foundRR = 1;
			if (flags & DB_DELETE) {
 delete:		       
#ifdef BIND_UPDATE
			/*
			 * XXX	assume here that savedpp!=NULL iff. db_update
			 *	has been called by the dyanmic update code.
			 *	Maybe a new flag is more appropriate?
			 */
                                if (savedpp != NULL)
					foundRR = 1;
#endif
				dp = rm_datum(dp, np, pdp, savedpp);
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
 	/* XXX:	delete a terminal namebuf also if all databuf's
	 *	underneath of it have been deleted) */
	if (newdp == NULL)
		return (OK);
	/* XXX:	empty nodes bypass credibility checks above; should check
	 *	response source address here if flags&NOTAUTH.
	 */
	fixttl(newdp);
	ns_debug(ns_log_db, 3, "db_update: adding%s %#x",
		 (newdp->d_flags&DB_F_HINT) ? " hint":"", newdp);

	if (NS_OPTION_P(OPTION_HOSTSTATS) &&
	    newdp->d_zone == DB_Z_CACHE &&
	    (newdp->d_flags & DB_F_HINT) == 0)
		newdp->d_ns = nameserFind(from.sin_addr, NS_F_INSERT);

	/* Add to end of list, generally preserving order */
	newdp->d_next = NULL;
	if ((dp = np->n_data) == NULL)  {
		DRCNTINC(newdp);
		if (newdp->d_flags & DB_F_ACTIVE)
			panic("db_update: DB_F_ACTIVE set", NULL);
		newdp->d_flags |= DB_F_ACTIVE;
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
	DRCNTINC(newdp);
	if (newdp->d_flags & DB_F_ACTIVE)
		panic("db_update: DB_F_ACTIVE set", NULL);
	newdp->d_flags |= DB_F_ACTIVE;	
	dp->d_next = newdp;
	return (OK);
}

void
fixttl(struct databuf *dp) {
	if (dp->d_zone == DB_Z_CACHE && (dp->d_flags & DB_F_HINT) == 0) {
		if (dp->d_ttl <= (u_int32_t)tt.tv_sec)
			return;
		else if (dp->d_ttl < (u_int32_t)tt.tv_sec+min_cache_ttl)
			dp->d_ttl = (u_int32_t)tt.tv_sec+min_cache_ttl;
		else if (dp->d_ttl > (u_int32_t)tt.tv_sec+max_cache_ttl)
			dp->d_ttl = (u_int32_t)tt.tv_sec+max_cache_ttl;
	}
}

/*
 * Compare type, class and data from databufs for equivalence.
 * All domain names in RR's must be compared case-insensitively.
 * Return 0 if equivalent, nonzero otherwise.
 */
int
db_cmp(const struct databuf *dp1, const struct databuf *dp2) {
	const u_char *cp1, *cp2;
	int len, len2;

 	/* XXXDYNUP- should be changed to 
	   if (!match(dp1, dp2->d_type, dp2->d_class) */
	if (dp1->d_type != dp2->d_type || dp1->d_class != dp2->d_class)
		return (1);
	/* XXXDYNUP - should be changed to (dp1->d_size != dp2->d_size &&
	   dp1->d_size != 0 && dp2->d_size != 0) */
	if (dp1->d_size != dp2->d_size)
		return (1);
 	/* d_mark is only used for dynamic updates currently */
#ifndef BIND_UPDATE
	if (dp1->d_mark != dp2->d_mark)
		return (1);		/* old and new RR's are distinct */
#endif
	if (dp1->d_rcode && dp2->d_rcode)
		return ((dp1->d_rcode == dp1->d_rcode)?0:1);
	if (dp1->d_rcode || dp2->d_rcode)
		return (1);

	switch (dp1->d_type) {

	case T_A:
	case T_WKS:
	case T_NULL:
	case T_NSAP:
	case T_AAAA:
	case T_LOC:
	case T_KEY:
		/* Only binary data */
		return (memcmp(dp1->d_data, dp2->d_data, dp1->d_size));

	case T_NS:
	case T_CNAME:
	case T_PTR:
	case T_MB:
	case T_MG:
	case T_MR:
		/* Only a domain name */
		return (strcasecmp((char *)dp1->d_data, (char *)dp2->d_data));

	case T_SIG:
		/* Binary data, a domain name, more binary data */
		if (dp1->d_size < NS_SIG_SIGNER)
			return (1);
		if (memcmp(dp1->d_data, dp2->d_data, NS_SIG_SIGNER))
			return (1);
		len = NS_SIG_SIGNER +
			strlen((char *)dp1->d_data + NS_SIG_SIGNER);
		if (strcasecmp((char *)dp1->d_data + NS_SIG_SIGNER,
			       (char *)dp2->d_data + NS_SIG_SIGNER))
			return (1);
		return (memcmp(dp1->d_data + len,
			       dp2->d_data + len,
			       dp1->d_size - len));

	case T_NXT:
		/* First a domain name, then binary data */
		if (strcasecmp((char *)dp1->d_data, (char *)dp2->d_data))
			return (1);
		len = strlen((char *)dp1->d_data)+1;
		return (memcmp(dp1->d_data + len,
			       dp2->d_data + len,
			       dp1->d_size - len));

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
		return (memcmp(cp1, cp2, INT32SZ * 5));
	
	case T_NAPTR: {
		int t1,t2;

		if (dp1->d_size != dp2->d_size)
			return (1);
		cp1 = dp1->d_data;
		cp2 = dp2->d_data;

		/* Order */
		if (*cp1++ != *cp2++ || *cp1++ != *cp2++)	
			return (1);

		/* Preference */
		if (*cp1++ != *cp2++ || *cp1++ != *cp2++)	
			return (1);

		/* Flags */
		t1 = *cp1++; t2 = *cp2++;
		if (t1 != t2 || memcmp(cp1, cp2, t1))
			return (1);
		cp1 += t1; cp2 += t2;

		/* Services */
		t1 = *cp1++; t2 = *cp2++;
		if (t1 != t2 || memcmp(cp1, cp2, t1))
			return (1);
		cp1 += t1; cp2 += t2;

		/* Regexp */
		t1 = *cp1++; t2 = *cp2++;
		if (t1 != t2 || memcmp(cp1, cp2, t1))
			return (1);
		cp1 += t1; cp2 += t2;

		/* Replacement */
		t1 = strlen((char *)cp1); t2 = strlen((char *)cp2);
		if (t1 != t2 || memcmp(cp1, cp2, t1))
			return (1);
		cp1 += t1 + 1; cp2 += t2 + 1;

		/* they all checked out! */
		return (0);
	    }

	case T_MX:
	case T_AFSDB:
	case T_RT:
	case T_SRV:
		cp1 = dp1->d_data;
		cp2 = dp2->d_data;
		if (*cp1++ != *cp2++ || *cp1++ != *cp2++)	/* cmp prio */
			return (1);
		if (dp1->d_type == T_SRV) {
			if (*cp1++ != *cp2++ || *cp1++ != *cp2++) /* weight */
				return (1);
			if (*cp1++ != *cp2++ || *cp1++ != *cp2++) /* port */
				return (1);
		}
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
		return (memcmp(dp1->d_data, dp2->d_data, dp1->d_size));

	default:
		return (1);
	}
}
