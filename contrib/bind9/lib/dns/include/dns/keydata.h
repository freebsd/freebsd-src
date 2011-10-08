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

/* $Id: keydata.h,v 1.2 2009-06-30 02:52:32 each Exp $ */

#ifndef DNS_KEYDATA_H
#define DNS_KEYDATA_H 1

/*****
 ***** Module Info
 *****/

/*! \file dns/keydata.h
 * \brief
 * KEYDATA utilities.
 */

/***
 *** Imports
 ***/

#include <isc/lang.h>
#include <isc/types.h>

#include <dns/types.h>
#include <dns/rdatastruct.h>

ISC_LANG_BEGINDECLS

isc_result_t
dns_keydata_todnskey(dns_rdata_keydata_t *keydata,
		     dns_rdata_dnskey_t *dnskey, isc_mem_t *mctx);

isc_result_t
dns_keydata_fromdnskey(dns_rdata_keydata_t *keydata,
		       dns_rdata_dnskey_t *dnskey,
		       isc_uint32_t refresh, isc_uint32_t addhd,
		       isc_uint32_t removehd, isc_mem_t *mctx);

ISC_LANG_ENDDECLS

#endif /* DNS_KEYDATA_H */
