/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: rdatasetiter.h,v 1.14.206.1 2004/03/06 08:13:59 marka Exp $ */

#ifndef DNS_RDATASETITER_H
#define DNS_RDATASETITER_H 1

/*****
 ***** Module Info
 *****/

/*
 * DNS Rdataset Iterator
 *
 * The DNS Rdataset Iterator interface allows iteration of all of the
 * rdatasets at a node.
 *
 * The dns_rdatasetiter_t type is like a "virtual class".  To actually use
 * it, an implementation of the class is required.  This implementation is
 * supplied by the database.
 *
 * It is the client's responsibility to call dns_rdataset_disassociate()
 * on all rdatasets returned.
 *
 * XXX <more> XXX
 *
 * MP:
 *	The iterator itself is not locked.  The caller must ensure
 *	synchronization.
 *
 *	The iterator methods ensure appropriate database locking.
 *
 * Reliability:
 *	No anticipated impact.
 *
 * Resources:
 *	<TBS>
 *
 * Security:
 *	No anticipated impact.
 *
 * Standards:
 *	None.
 */

/*****
 ***** Imports
 *****/

#include <isc/lang.h>
#include <isc/magic.h>
#include <isc/stdtime.h>

#include <dns/types.h>

ISC_LANG_BEGINDECLS

/*****
 ***** Types
 *****/

typedef struct dns_rdatasetitermethods {
	void		(*destroy)(dns_rdatasetiter_t **iteratorp);
	isc_result_t	(*first)(dns_rdatasetiter_t *iterator);
	isc_result_t	(*next)(dns_rdatasetiter_t *iterator);
	void		(*current)(dns_rdatasetiter_t *iterator,
				   dns_rdataset_t *rdataset);
} dns_rdatasetitermethods_t;

#define DNS_RDATASETITER_MAGIC	     ISC_MAGIC('D','N','S','i')
#define DNS_RDATASETITER_VALID(i)    ISC_MAGIC_VALID(i, DNS_RDATASETITER_MAGIC)

/*
 * This structure is actually just the common prefix of a DNS db
 * implementation's version of a dns_rdatasetiter_t.
 *
 * Direct use of this structure by clients is forbidden.  DB implementations
 * may change the structure.  'magic' must be DNS_RDATASETITER_MAGIC for
 * any of the dns_rdatasetiter routines to work.  DB implementations must
 * maintain all DB rdataset iterator invariants.
 */
struct dns_rdatasetiter {
	/* Unlocked. */
	unsigned int			magic;
	dns_rdatasetitermethods_t *	methods;
	dns_db_t *			db;
	dns_dbnode_t *			node;
	dns_dbversion_t *		version;
	isc_stdtime_t			now;
};

void
dns_rdatasetiter_destroy(dns_rdatasetiter_t **iteratorp);
/*
 * Destroy '*iteratorp'.
 *
 * Requires:
 *
 *	'*iteratorp' is a valid iterator.
 *
 * Ensures:
 *
 *	All resources used by the iterator are freed.
 *
 *	*iteratorp == NULL.
 */

isc_result_t
dns_rdatasetiter_first(dns_rdatasetiter_t *iterator);
/*
 * Move the rdataset cursor to the first rdataset at the node (if any).
 *
 * Requires:
 *	'iterator' is a valid iterator.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOMORE			There are no rdatasets at the node.
 *
 *	Other results are possible, depending on the DB implementation.
 */

isc_result_t
dns_rdatasetiter_next(dns_rdatasetiter_t *iterator);
/*
 * Move the rdataset cursor to the next rdataset at the node (if any).
 *
 * Requires:
 *	'iterator' is a valid iterator.
 *
 * Returns:
 *	ISC_R_SUCCESS
 *	ISC_R_NOMORE			There are no more rdatasets at the
 *					node.
 *
 *	Other results are possible, depending on the DB implementation.
 */

void
dns_rdatasetiter_current(dns_rdatasetiter_t *iterator,
			 dns_rdataset_t *rdataset);
/*
 * Return the current rdataset.
 *
 * Requires:
 *	'iterator' is a valid iterator.
 *
 *	'rdataset' is a valid, disassociated rdataset.
 *
 *	The rdataset cursor of 'iterator' is at a valid location (i.e. the
 *	result of last call to a cursor movement command was ISC_R_SUCCESS).
 */

ISC_LANG_ENDDECLS

#endif /* DNS_RDATASETITER_H */
