/*
 * Copyright (C) 2004-2007, 2009  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
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

/* $Id: soa.h,v 1.12 2009-09-10 01:47:09 each Exp $ */

#ifndef DNS_SOA_H
#define DNS_SOA_H 1

/*****
 ***** Module Info
 *****/

/*! \file dns/soa.h
 * \brief
 * SOA utilities.
 */

/***
 *** Imports
 ***/

#include <isc/lang.h>
#include <isc/types.h>

#include <dns/types.h>

ISC_LANG_BEGINDECLS

#define DNS_SOA_BUFFERSIZE      ((2 * DNS_NAME_MAXWIRE) + (4 * 5))

isc_result_t
dns_soa_buildrdata(dns_name_t *origin, dns_name_t *contact,
		   dns_rdataclass_t rdclass,
		   isc_uint32_t serial, isc_uint32_t refresh,
		   isc_uint32_t retry, isc_uint32_t expire,
		   isc_uint32_t minimum, unsigned char *buffer,
		   dns_rdata_t *rdata);
/*%<
 * Build the rdata of an SOA record.
 *
 * Requires:
 *\li   buffer  Points to a temporary buffer of at least
 *              DNS_SOA_BUFFERSIZE bytes.
 *\li   rdata   Points to an initialized dns_rdata_t.
 *
 * Ensures:
 *  \li    *rdata       Contains a valid SOA rdata.  The 'data' member
 *  			refers to 'buffer'.
 */

isc_uint32_t
dns_soa_getserial(dns_rdata_t *rdata);
isc_uint32_t
dns_soa_getrefresh(dns_rdata_t *rdata);
isc_uint32_t
dns_soa_getretry(dns_rdata_t *rdata);
isc_uint32_t
dns_soa_getexpire(dns_rdata_t *rdata);
isc_uint32_t
dns_soa_getminimum(dns_rdata_t *rdata);
/*
 * Extract an integer field from the rdata of a SOA record.
 *
 * Requires:
 *	rdata refers to the rdata of a well-formed SOA record.
 */

void
dns_soa_setserial(isc_uint32_t val, dns_rdata_t *rdata);
void
dns_soa_setrefresh(isc_uint32_t val, dns_rdata_t *rdata);
void
dns_soa_setretry(isc_uint32_t val, dns_rdata_t *rdata);
void
dns_soa_setexpire(isc_uint32_t val, dns_rdata_t *rdata);
void
dns_soa_setminimum(isc_uint32_t val, dns_rdata_t *rdata);
/*
 * Change an integer field of a SOA record by modifying the
 * rdata in-place.
 *
 * Requires:
 *	rdata refers to the rdata of a well-formed SOA record.
 */


ISC_LANG_ENDDECLS

#endif /* DNS_SOA_H */
