#if !defined(lint) && !defined(SABER)
static const char rcsid[] = "$Id: res_update.c,v 1.11.2.2 2001/06/08 23:12:45 mellon Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1996-1999 by Internet Software Consortium.
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

/*
 * Based on the Dynamic DNS reference implementation by Viraj Bais
 * <viraj_bais@ccm.fm.intel.com>
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <isc-dhcp/list.h>
#include "minires/minires.h"
#include "arpa/nameser.h"

/*
 * Separate a linked list of records into groups so that all records
 * in a group will belong to a single zone on the nameserver.
 * Create a dynamic update packet for each zone and send it to the
 * nameservers for that zone, and await answer.
 * Abort if error occurs in updating any zone.
 * Return the number of zones updated on success, < 0 on error.
 *
 * On error, caller must deal with the unsynchronized zones
 * eg. an A record might have been successfully added to the forward
 * zone but the corresponding PTR record would be missing if error
 * was encountered while updating the reverse zone.
 */

struct zonegrp {
	char z_origin[MAXDNAME];
	ns_class z_class;
	struct in_addr z_nsaddrs[MAXNS];
	int z_nscount;
	int z_flags;
	ISC_LIST(ns_updrec) z_rrlist;
	ISC_LINK(struct zonegrp) z_link;
};

#define ZG_F_ZONESECTADDED	0x0001

/* Forward. */

static int	nscopy(struct sockaddr_in *, const struct sockaddr_in *, int);
static int	nsprom(struct sockaddr_in *, const struct in_addr *, int);

void tkey_free (ns_tsig_key **);

isc_result_t
res_nupdate(res_state statp, ns_updrec *rrecp_in) {
	ns_updrec *rrecp;
	double answer[PACKETSZ / sizeof (double)];
	double packet[2*PACKETSZ / sizeof (double)];
	struct zonegrp *zptr, tgrp;
	int nzones = 0, nscount = 0;
	unsigned n;
	unsigned rval;
	struct sockaddr_in nsaddrs[MAXNS];
	ns_tsig_key *key;
	void *zcookie = 0;
	void *zcookp = &zcookie;
	isc_result_t rcode;

      again:
	/* Make sure all the updates are in the same zone, and find out
	   what zone they are in. */
	zptr = NULL;
	for (rrecp = rrecp_in; rrecp; rrecp = ISC_LIST_NEXT(rrecp, r_link)) {
		/* Find the origin for it if there is one. */
		tgrp.z_class = rrecp->r_class;
		rcode = res_findzonecut(statp, rrecp->r_dname, tgrp.z_class,
					RES_EXHAUSTIVE,
					tgrp.z_origin,
					sizeof tgrp.z_origin,
					tgrp.z_nsaddrs, MAXNS, &tgrp.z_nscount,
					zcookp);
		if (rcode != ISC_R_SUCCESS)
			goto done;
		if (tgrp.z_nscount <= 0) {
			rcode = ISC_R_NOTZONE;
			goto done;
		}
		/* Make a group for it if there isn't one. */
		if (zptr == NULL) {
			zptr = malloc(sizeof *zptr);
			if (zptr == NULL) {
				rcode = ISC_R_NOMEMORY;
				goto done;
			}
			*zptr = tgrp;
			zptr->z_flags = 0;
			ISC_LIST_INIT(zptr->z_rrlist);
		} else if (ns_samename(tgrp.z_origin, zptr->z_origin) == 0 ||
			   tgrp.z_class != zptr->z_class) {
			/* Some of the records are in different zones. */
			rcode = ISC_R_CROSSZONE;
			goto done;
		}
		/* Thread this rrecp onto the zone group. */
		ISC_LIST_APPEND(zptr->z_rrlist, rrecp, r_glink);
	}

	/* Construct zone section and prepend it. */
	rrecp = res_mkupdrec(ns_s_zn, zptr->z_origin,
			     zptr->z_class, ns_t_soa, 0);
	if (rrecp == NULL) {
		rcode = ISC_R_UNEXPECTED;
		goto done;
	}
	ISC_LIST_PREPEND(zptr->z_rrlist, rrecp, r_glink);
	zptr->z_flags |= ZG_F_ZONESECTADDED;

	/* Marshall the update message. */
	n = sizeof packet;
	rcode = res_nmkupdate(statp,
			      ISC_LIST_HEAD(zptr->z_rrlist), packet, &n);
	if (rcode != ISC_R_SUCCESS)
		goto done;

	/* Temporarily replace the resolver's nameserver set. */
	nscount = nscopy(nsaddrs, statp->nsaddr_list, statp->nscount);
	statp->nscount = nsprom(statp->nsaddr_list,
				zptr->z_nsaddrs, zptr->z_nscount);

	/* Send the update and remember the result. */
	key = (ns_tsig_key *)0;
	rcode = find_tsig_key (&key, zptr->z_origin, zcookie);
	if (rcode == ISC_R_SUCCESS) {
		rcode = res_nsendsigned(statp, packet, n, key,
					answer, sizeof answer, &rval);
		tkey_free (&key);
	} else if (rcode == ISC_R_NOTFOUND || rcode == ISC_R_KEY_UNKNOWN) {
		rcode = res_nsend(statp, packet, n,
				  answer, sizeof answer, &rval);
	}
	if (rcode != ISC_R_SUCCESS)
		goto undone;

	rcode = ns_rcode_to_isc (((HEADER *)answer)->rcode);
	if (zcookie && rcode == ISC_R_BADSIG) {
		repudiate_zone (&zcookie);
	}

 undone:
	/* Restore resolver's nameserver set. */
	statp->nscount = nscopy(statp->nsaddr_list, nsaddrs, nscount);
	nscount = 0;
 done:
	if (zptr) {
		if ((zptr->z_flags & ZG_F_ZONESECTADDED) != 0)
			res_freeupdrec(ISC_LIST_HEAD(zptr->z_rrlist));
		free(zptr);
	}

	/* If the update failed because we used a cached zone and it
	   didn't work, try it again without the cached zone. */
	if (zcookp && (rcode == ISC_R_NOTZONE || rcode == ISC_R_BADSIG)) {
		zcookp = 0;
		goto again;
	}

	if (zcookie)
		forget_zone (&zcookie);
	return rcode;
}

/* Private. */

static int
nscopy(struct sockaddr_in *dst, const struct sockaddr_in *src, int n) {
	int i;

	for (i = 0; i < n; i++)
		dst[i] = src[i];
	return (n);
}

static int
nsprom(struct sockaddr_in *dst, const struct in_addr *src, int n) {
	int i;

	for (i = 0; i < n; i++) {
		memset(&dst[i], 0, sizeof dst[i]);
		dst[i].sin_family = AF_INET;
		dst[i].sin_port = htons(NS_DEFAULTPORT);
		dst[i].sin_addr = src[i];
	}
	return (n);
}
