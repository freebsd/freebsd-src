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

/* $Id: util.h,v 1.5.206.1 2004/03/06 10:21:32 marka Exp $ */

#ifndef RNDC_UTIL_H
#define RNDC_UTIL_H 1

#include <isc/lang.h>

#include <isc/formatcheck.h>

#define NS_CONTROL_PORT		953

#undef DO
#define DO(name, function) \
	do { \
		result = function; \
		if (result != ISC_R_SUCCESS) \
			fatal("%s: %s", name, isc_result_totext(result)); \
		else \
			notify("%s", name); \
	} while (0)

ISC_LANG_BEGINDECLS

void
notify(const char *fmt, ...) ISC_FORMAT_PRINTF(1, 2);

void            
fatal(const char *format, ...) ISC_FORMAT_PRINTF(1, 2);

ISC_LANG_ENDDECLS

#endif /* RNDC_UTIL_H */
