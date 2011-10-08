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

/* $Id: hip_55.h,v 1.2 2009-02-26 06:09:19 marka Exp $ */

#ifndef GENERIC_HIP_5_H
#define GENERIC_HIP_5_H 1

/* RFC 5205 */

typedef struct dns_rdata_hip {
	dns_rdatacommon_t	common;
	isc_mem_t *		mctx;
	unsigned char *		hit;
	unsigned char *		key;
	unsigned char *		servers;
	isc_uint8_t		algorithm;
	isc_uint8_t		hit_len;
	isc_uint16_t		key_len;
	isc_uint16_t		servers_len;
	/* Private */
	isc_uint16_t		offset;
} dns_rdata_hip_t;

isc_result_t
dns_rdata_hip_first(dns_rdata_hip_t *);

isc_result_t
dns_rdata_hip_next(dns_rdata_hip_t *);

void
dns_rdata_hip_current(dns_rdata_hip_t *, dns_name_t *);

#endif /* GENERIC_HIP_5_H */
