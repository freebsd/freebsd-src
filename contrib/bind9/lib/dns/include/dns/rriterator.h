/*
 * Copyright (C) 2009, 2011, 2012  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
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

/* $Id$ */

#ifndef DNS_RRITERATOR_H
#define DNS_RRITERATOR_H 1

/*****
 ***** Module Info
 *****/

/*! \file dns/rriterator.h
 * \brief
 * Functions for "walking" a zone database, visiting each RR or RRset in turn.
 */

/*****
 ***** Imports
 *****/

#include <isc/lang.h>
#include <isc/magic.h>
#include <isc/ondestroy.h>
#include <isc/stdtime.h>

#include <dns/db.h>
#include <dns/dbiterator.h>
#include <dns/fixedname.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/types.h>

ISC_LANG_BEGINDECLS

/*****
 ***** Types
 *****/

/*%
 * A dns_rriterator_t is an iterator that iterates over an entire database,
 * returning one RR at a time, in some arbitrary order.
 */

typedef struct dns_rriterator {
	unsigned int		magic;
	isc_result_t		result;
	dns_db_t		*db;
	dns_dbiterator_t 	*dbit;
	dns_dbversion_t 	*ver;
	isc_stdtime_t		now;
	dns_dbnode_t		*node;
	dns_fixedname_t		fixedname;
	dns_rdatasetiter_t 	*rdatasetit;
	dns_rdataset_t 		rdataset;
	dns_rdata_t		rdata;
} dns_rriterator_t;

#define RRITERATOR_MAGIC		ISC_MAGIC('R', 'R', 'I', 't')
#define VALID_RRITERATOR(m)		ISC_MAGIC_VALID(m, RRITERATOR_MAGIC)

isc_result_t
dns_rriterator_init(dns_rriterator_t *it, dns_db_t *db,
		       dns_dbversion_t *ver, isc_stdtime_t now);
/*%
 * Initialize an rriterator; sets the cursor to the origin node
 * of the database.
 *
 * Requires:
 *
 * \li	'db' is a valid database.
 *
 * Returns:
 *
 * \li	#ISC_R_SUCCESS
 * \li	#ISC_R_NOMEMORY
 */

isc_result_t
dns_rriterator_first(dns_rriterator_t *it);
/*%<
 * Move the rriterator cursor to the first rdata in the database.
 *
 * Requires:
 *\li	'it' is a valid, initialized rriterator
 *
 * Returns:
 *\li	#ISC_R_SUCCESS
 *\li	#ISC_R_NOMORE			There are no rdata in the set.
 */

isc_result_t
dns_rriterator_nextrrset(dns_rriterator_t *it);
/*%<
 * Move the rriterator cursor to the next rrset in the database,
 * skipping over any remaining records that have the same rdatatype
 * as the current one.
 *
 * Requires:
 *\li	'it' is a valid, initialized rriterator
 *
 * Returns:
 *\li	#ISC_R_SUCCESS
 *\li	#ISC_R_NOMORE			No more rrsets in the database
 */

isc_result_t
dns_rriterator_next(dns_rriterator_t *it);
/*%<
 * Move the rriterator cursor to the next rrset in the database,
 * skipping over any remaining records that have the same rdatatype
 * as the current one.
 *
 * Requires:
 *\li	'it' is a valid, initialized rriterator
 *
 * Returns:
 *\li	#ISC_R_SUCCESS
 *\li	#ISC_R_NOMORE			No more records in the database
 */

void
dns_rriterator_current(dns_rriterator_t *it, dns_name_t **name,
			  isc_uint32_t *ttl, dns_rdataset_t **rdataset,
			  dns_rdata_t **rdata);
/*%<
 * Make '*name' refer to the current name.  If 'rdataset' is not NULL,
 * make '*rdataset' refer to the current * rdataset.  If '*rdata' is not
 * NULL, make '*rdata' refer to the current record.
 *
 * Requires:
 *\li	'*name' is a valid name object
 *\li	'rdataset' is NULL or '*rdataset' is NULL
 *\li	'rdata' is NULL or '*rdata' is NULL
 *
 * Ensures:
 *\li	'rdata' refers to the rdata at the rdata cursor location of
 *\li	'rdataset'.
 */

void
dns_rriterator_pause(dns_rriterator_t *it);
/*%<
 * Pause rriterator.  Frees any locks held by the database iterator.
 * Callers should use this routine any time they are not going to
 * execute another rriterator method in the immediate future.
 *
 * Requires:
 *\li	'it' is a valid iterator.
 *
 * Ensures:
 *\li	Any database locks being held for efficiency of iterator access are
 *	released.
 */

void
dns_rriterator_destroy(dns_rriterator_t *it);
/*%<
 * Shut down and free resources in rriterator 'it'.
 *
 * Requires:
 *
 *\li	'it' is a valid iterator.
 *
 * Ensures:
 *
 *\li	All resources used by the rriterator are freed.
 */

ISC_LANG_ENDDECLS

#endif /* DNS_RRITERATOR_H */
