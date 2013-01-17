/*
 * Copyright (C) 2009, 2012  Internet Systems Consortium, Inc. ("ISC")
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

#include <isc/lang.h>
#include <isc/types.h>

#include <dns/types.h>
#include <dns/db.h>

#ifndef DNS_PRIVATE_H
#define DNS_PRIVATE_H

ISC_LANG_BEGINDECLS

isc_result_t
dns_private_chains(dns_db_t *db, dns_dbversion_t *ver,
		   dns_rdatatype_t privatetype,
		   isc_boolean_t *build_nsec, isc_boolean_t *build_nsec3);
/*%<
 * Examine the NSEC, NSEC3PARAM and privatetype RRsets at the apex of the
 * database to determine which of NSEC or NSEC3 chains we are currently
 * maintaining.  In normal operations only one of NSEC or NSEC3 is being
 * maintained but when we are transitiong between NSEC and NSEC3 we need
 * to update both sets of chains.  If 'privatetype' is zero then the
 * privatetype RRset will not be examined.
 *
 * Requires:
 * \li	'db' is valid.
 * \li	'version' is valid or NULL.
 * \li	'build_nsec' is a pointer to a isc_boolean_t or NULL.
 * \li	'build_nsec3' is a pointer to a isc_boolean_t or NULL.
 *
 * Returns:
 * \li 	ISC_R_SUCCESS, 'build_nsec' and 'build_nsec3' will be valid.
 * \li	other on error
 */

ISC_LANG_ENDDECLS

#endif
