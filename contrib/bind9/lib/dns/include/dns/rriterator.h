/*
 * Copyright (C) 2009  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: rriterator.h,v 1.2 2009-06-30 02:52:32 each Exp $ */

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

isc_result_t
dns_rriterator_first(dns_rriterator_t *it);

isc_result_t
dns_rriterator_nextrrset(dns_rriterator_t *it);

isc_result_t
dns_rriterator_next(dns_rriterator_t *it);

void
dns_rriterator_current(dns_rriterator_t *it, dns_name_t **name,
			  isc_uint32_t *ttl, dns_rdataset_t **rdataset,
			  dns_rdata_t **rdata);

void
dns_rriterator_pause(dns_rriterator_t *it);

void
dns_rriterator_destroy(dns_rriterator_t *it);

ISC_LANG_ENDDECLS

#endif /* DNS_RRITERATOR_H */
