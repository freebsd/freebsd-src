/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
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

/* $Id: gssapi.h,v 1.1.4.1 2004/12/09 04:07:20 marka Exp $ */

#ifndef DST_GSSAPI_H
#define DST_GSSAPI_H 1

#include <isc/lang.h>

#include <isc/types.h>

ISC_LANG_BEGINDECLS

/***
 *** Types
 ***/

/***
 *** Functions
 ***/

isc_result_t
dst_gssapi_acquirecred(dns_name_t *name, isc_boolean_t initiate, void **cred);

isc_result_t
dst_gssapi_initctx(dns_name_t *name, void *cred,
		   isc_region_t *intoken, isc_buffer_t *outtoken,
		   void **context);

isc_result_t
dst_gssapi_acceptctx(dns_name_t *name, void *cred,
		     isc_region_t *intoken, isc_buffer_t *outtoken,
		     void **context);

/*
 * XXX
 */

ISC_LANG_ENDDECLS

#endif /* DST_GSSAPI_H */
