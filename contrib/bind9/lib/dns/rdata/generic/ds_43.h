/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2002  Internet Software Consortium.
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

/* $Id: ds_43.h,v 1.3.2.1 2004/03/08 02:08:03 marka Exp $ */

/* draft-ietf-dnsext-delegation-signer-05.txt */
#ifndef GENERIC_DS_43_H
#define GENERIC_DS_43_H 1

typedef struct dns_rdata_ds {
	dns_rdatacommon_t	common;
	isc_mem_t		*mctx;
	isc_uint16_t		key_tag;
	isc_uint8_t		algorithm;
	isc_uint8_t		digest_type;
	isc_uint16_t		length;
	unsigned char		*digest;
} dns_rdata_ds_t;

#endif /* GENERIC_DS_43_H */
