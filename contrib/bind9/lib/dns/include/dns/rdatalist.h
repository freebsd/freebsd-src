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

/* $Id: rdatalist.h,v 1.13.206.1 2004/03/06 08:13:59 marka Exp $ */

#ifndef DNS_RDATALIST_H
#define DNS_RDATALIST_H 1

/*****
 ***** Module Info
 *****/

/*
 * DNS Rdatalist
 *
 * A DNS rdatalist is a list of rdata of a common type and class.
 *
 * MP:
 *	Clients of this module must impose any required synchronization.
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

#include <isc/lang.h>

#include <dns/types.h>

/*
 * Clients may use this type directly.
 */
struct dns_rdatalist {
	dns_rdataclass_t		rdclass;
	dns_rdatatype_t			type;
	dns_rdatatype_t			covers;
	dns_ttl_t			ttl;
	ISC_LIST(dns_rdata_t)		rdata;
	ISC_LINK(dns_rdatalist_t)	link;
};

ISC_LANG_BEGINDECLS

void
dns_rdatalist_init(dns_rdatalist_t *rdatalist);
/*
 * Initialize rdatalist.
 *
 * Ensures:
 *	All fields of rdatalist have been initialized to their default
 *	values.
 */

isc_result_t
dns_rdatalist_tordataset(dns_rdatalist_t *rdatalist,
			 dns_rdataset_t *rdataset);
/*
 * Make 'rdataset' refer to the rdata in 'rdatalist'.
 *
 * Note:
 *	The caller must ensure that 'rdatalist' remains valid and unchanged
 *	while 'rdataset' is associated with it.
 *
 * Requires:
 *
 *	'rdatalist' is a valid rdatalist.
 *
 *	'rdataset' is a valid rdataset that is not currently associated with
 *	any rdata.
 *
 * Ensures:
 *	On success,
 *
 *		'rdataset' is associated with the rdata in rdatalist.
 *
 * Returns:
 *	ISC_R_SUCCESS
 */

ISC_LANG_ENDDECLS

#endif /* DNS_RDATALIST_H */
