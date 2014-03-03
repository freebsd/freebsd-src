/*
 * Copyright (C) 2014  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef ISC_TM_H
#define ISC_TM_H 1

/*! \file isc/tm.h
 * Provides portable conversion routines for struct tm.
 */
#include <time.h>

#include <isc/lang.h>
#include <isc/types.h>


ISC_LANG_BEGINDECLS

time_t
isc_tm_timegm(struct tm *tm);
/*
 * Convert a tm structure to time_t, using UTC rather than the local
 * time zone.
 */

char *
isc_tm_strptime(const char *buf, const char *fmt, struct tm *tm);
/*
 * Parse a formatted date string into struct tm.
 */

ISC_LANG_ENDDECLS

#endif /* ISC_TIMER_H */
