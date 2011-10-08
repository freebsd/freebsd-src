/*
 * Copyright (C) 2009, 2010  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: tsec.h,v 1.6 2010-12-09 00:54:34 marka Exp $ */

#ifndef DNS_TSEC_H
#define DNS_TSEC_H 1

/*****
 ***** Module Info
 *****/

/*! \file
 *
 * \brief
 * The TSEC (Transaction Security) module is an abstraction layer for managing
 * DNS transaction mechanisms such as TSIG or SIG(0).  A TSEC structure is a
 * mechanism-independent object containing key information specific to the
 * mechanism, and is expected to be used as an argument to other modules
 * that use transaction security in a mechanism-independent manner.
 *
 * MP:
 *\li	A TSEC structure is expected to be thread-specific.  No inter-thread
 *	synchronization is ensured in multiple access to a single TSEC
 *	structure.
 *
 * Resources:
 *\li	TBS
 *
 * Security:
 *\li	This module does not handle any low-level data directly, and so no
 *	security issue specific to this module is anticipated.
 */

#include <dns/types.h>

#include <dst/dst.h>

ISC_LANG_BEGINDECLS

/***
 *** Types
 ***/

/*%
 * Transaction security types.
 */
typedef enum {
	dns_tsectype_none,
	dns_tsectype_tsig,
	dns_tsectype_sig0
} dns_tsectype_t;

isc_result_t
dns_tsec_create(isc_mem_t *mctx, dns_tsectype_t type, dst_key_t *key,
		dns_tsec_t **tsecp);
/*%<
 * Create a TSEC structure and stores a type-dependent key structure in it.
 * For a TSIG key (type is dns_tsectype_tsig), dns_tsec_create() creates a
 * TSIG key structure from '*key' and keeps it in the structure.  For other
 * types, this function simply retains '*key' in the structure.  In either
 * case, the ownership of '*key' is transferred to the TSEC module; the caller
 * must not modify or destroy it after the call to dns_tsec_create().
 *
 * Requires:
 *
 *\li	'mctx' is a valid memory context.
 *
 *\li	'type' is a valid value of dns_tsectype_t (see above).
 *
 *\li	'key' is a valid key.
 *
 *\li	tsecp != NULL && *tsecp == NULL.
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS				On success.
 *
 *\li	Anything else				Failure.
 */

void
dns_tsec_destroy(dns_tsec_t **tsecp);
/*%<
 * Destroy the TSEC structure.  The stored key is also detached or destroyed.
 *
 * Requires
 *
 *\li	'*tsecp' is a valid TSEC structure.
 *
 * Ensures
 *
 *\li	*tsecp == NULL.
 *
 */

dns_tsectype_t
dns_tsec_gettype(dns_tsec_t *tsec);
/*%<
 * Return the TSEC type of '*tsec'.
 *
 * Requires
 *
 *\li	'tsec' is a valid TSEC structure.
 *
 */

void
dns_tsec_getkey(dns_tsec_t *tsec, void *keyp);
/*%<
 * Return the TSEC key of '*tsec' in '*keyp'.
 *
 * Requires
 *
 *\li	keyp != NULL
 *
 * Ensures
 *
 *\li	*tsecp points to a valid key structure depending on the TSEC type.
 */

#endif /* DNS_TSEC_H */
