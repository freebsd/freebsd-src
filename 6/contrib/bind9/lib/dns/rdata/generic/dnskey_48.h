/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2003  Internet Software Consortium.
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

#ifndef GENERIC_DNSKEY_48_H
#define GENERIC_DNSKEY_48_H 1

/* $Id: dnskey_48.h,v 1.3.2.1 2004/03/08 02:08:02 marka Exp $ */

/* RFC 2535 */

typedef struct dns_rdata_dnskey {
        dns_rdatacommon_t	common;
        isc_mem_t *		mctx;
        isc_uint16_t		flags;
        isc_uint8_t		protocol;
        isc_uint8_t		algorithm;
        isc_uint16_t		datalen;
        unsigned char *		data;
} dns_rdata_dnskey_t;


#endif /* GENERIC_DNSKEY_48_H */
