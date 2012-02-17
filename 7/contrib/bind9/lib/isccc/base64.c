/*
 * Portions Copyright (C) 2004, 2005  Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (C) 2001  Internet Software Consortium.
 * Portions Copyright (C) 2001  Nominum, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NOMINUM DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: base64.c,v 1.3.18.2 2005-04-29 00:17:11 marka Exp $ */

/*! \file */

#include <config.h>

#include <isc/base64.h>
#include <isc/buffer.h>
#include <isc/region.h>
#include <isc/result.h>

#include <isccc/base64.h>
#include <isccc/result.h>
#include <isccc/util.h>

isc_result_t
isccc_base64_encode(isccc_region_t *source, int wordlength,
		  const char *wordbreak, isccc_region_t *target)
{
	isc_region_t sr;
	isc_buffer_t tb;
	isc_result_t result;

	sr.base = source->rstart;
	sr.length = source->rend - source->rstart;
	isc_buffer_init(&tb, target->rstart, target->rend - target->rstart);

	result = isc_base64_totext(&sr, wordlength, wordbreak, &tb);
	if (result != ISC_R_SUCCESS)
		return (result);
	source->rstart = source->rend;
	target->rstart = isc_buffer_used(&tb);
	return (ISC_R_SUCCESS);
}

isc_result_t
isccc_base64_decode(const char *cstr, isccc_region_t *target) {
	isc_buffer_t b;
	isc_result_t result;

	isc_buffer_init(&b, target->rstart, target->rend - target->rstart);
	result = isc_base64_decodestring(cstr, &b);
	if (result != ISC_R_SUCCESS)
		return (result);
	target->rstart = isc_buffer_used(&b);
	return (ISC_R_SUCCESS);
}
