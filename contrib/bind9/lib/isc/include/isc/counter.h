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

#ifndef ISC_COUNTER_H
#define ISC_COUNTER_H 1

/*****
 ***** Module Info
 *****/

/*! \file isc/counter.h
 *
 * \brief The isc_counter_t object is a simplified version of the
 * isc_quota_t object; it tracks the consumption of limited
 * resources, returning an error condition when the quota is
 * exceeded.  However, unlike isc_quota_t, attaching and detaching
 * from a counter object does not increment or decrement the counter.
 */

/***
 *** Imports.
 ***/

#include <isc/lang.h>
#include <isc/mutex.h>
#include <isc/types.h>

/*****
 ***** Types.
 *****/

ISC_LANG_BEGINDECLS

isc_result_t
isc_counter_create(isc_mem_t *mctx, int limit, isc_counter_t **counterp);
/*%<
 * Allocate and initialize a counter object.
 */

isc_result_t
isc_counter_increment(isc_counter_t *counter);
/*%<
 * Increment the counter.
 *
 * If the counter limit is nonzero and has been reached, then
 * return ISC_R_QUOTA, otherwise ISC_R_SUCCESS. (The counter is
 * incremented regardless of return value.)
 */

unsigned int
isc_counter_used(isc_counter_t *counter);
/*%<
 * Return the current counter value.
 */

void
isc_counter_setlimit(isc_counter_t *counter, int limit);
/*%<
 * Set the counter limit.
 */

void
isc_counter_attach(isc_counter_t *source, isc_counter_t **targetp);
/*%<
 * Attach to a counter object, increasing its reference counter.
 */

void
isc_counter_detach(isc_counter_t **counterp);
/*%<
 * Detach (and destroy if reference counter has dropped to zero)
 * a counter object.
 */

ISC_LANG_ENDDECLS

#endif /* ISC_COUNTER_H */
