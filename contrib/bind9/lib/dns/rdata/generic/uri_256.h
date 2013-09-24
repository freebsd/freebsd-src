/*
 * Copyright (C) 2011-2013  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef GENERIC_URI_256_H
#define GENERIC_URI_256_H 1

/* $Id: uri_256.h,v 1.2 2011/03/03 14:10:27 fdupont Exp $ */

typedef struct dns_rdata_uri {
	dns_rdatacommon_t	common;
	isc_mem_t *		mctx;
	isc_uint16_t		priority;
	isc_uint16_t		weight;
	unsigned char *		target;
	isc_uint16_t		tgt_len;
} dns_rdata_uri_t;

#endif /* GENERIC_URI_256_H */
