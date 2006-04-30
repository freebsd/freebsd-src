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

/* $Id: tsig_250.h,v 1.20.206.1 2004/03/06 08:14:02 marka Exp $ */

/* RFC 2845 */

#ifndef ANY_255_TSIG_250_H
#define ANY_255_TSIG_250_H 1

typedef struct dns_rdata_any_tsig {
	dns_rdatacommon_t	common;
	isc_mem_t *		mctx;
	dns_name_t		algorithm;
	isc_uint64_t		timesigned;
	isc_uint16_t		fudge;
	isc_uint16_t		siglen;
	unsigned char *		signature;
	isc_uint16_t		originalid;
	isc_uint16_t		error;
	isc_uint16_t		otherlen;
	unsigned char *		other;
} dns_rdata_any_tsig_t;

#endif /* ANY_255_TSIG_250_H */
