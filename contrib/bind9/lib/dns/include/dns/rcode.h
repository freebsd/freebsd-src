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

/* $Id: rcode.h,v 1.12.206.1 2004/03/06 08:13:59 marka Exp $ */

#ifndef DNS_RCODE_H
#define DNS_RCODE_H 1

#include <isc/lang.h>

#include <dns/types.h>

ISC_LANG_BEGINDECLS

isc_result_t dns_rcode_fromtext(dns_rcode_t *rcodep, isc_textregion_t *source);
/*
 * Convert the text 'source' refers to into a DNS error value.
 *
 * Requires:
 *	'rcodep' is a valid pointer.
 *
 *	'source' is a valid text region.
 *
 * Returns:
 *	ISC_R_SUCCESS			on success
 *	DNS_R_UNKNOWN			type is unknown
 */

isc_result_t dns_rcode_totext(dns_rcode_t rcode, isc_buffer_t *target);
/*
 * Put a textual representation of error 'rcode' into 'target'.
 *
 * Requires:
 *	'rcode' is a valid rcode.
 *
 *	'target' is a valid text buffer.
 *
 * Ensures:
 *	If the result is success:
 *		The used space in 'target' is updated.
 *
 * Returns:
 *	ISC_R_SUCCESS			on success
 *	ISC_R_NOSPACE			target buffer is too small
 */

isc_result_t dns_tsigrcode_fromtext(dns_rcode_t *rcodep,
				    isc_textregion_t *source);
/*
 * Convert the text 'source' refers to into a TSIG/TKEY error value.
 *
 * Requires:
 *	'rcodep' is a valid pointer.
 *
 *	'source' is a valid text region.
 *
 * Returns:
 *	ISC_R_SUCCESS			on success
 *	DNS_R_UNKNOWN			type is unknown
 */

isc_result_t dns_tsigrcode_totext(dns_rcode_t rcode, isc_buffer_t *target);
/*
 * Put a textual representation of TSIG/TKEY error 'rcode' into 'target'.
 *
 * Requires:
 *	'rcode' is a valid TSIG/TKEY error code.
 *
 *	'target' is a valid text buffer.
 *
 * Ensures:
 *	If the result is success:
 *		The used space in 'target' is updated.
 *
 * Returns:
 *	ISC_R_SUCCESS			on success
 *	ISC_R_NOSPACE			target buffer is too small
 */

ISC_LANG_ENDDECLS

#endif /* DNS_RCODE_H */
