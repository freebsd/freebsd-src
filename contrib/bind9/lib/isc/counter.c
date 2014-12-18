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

/*! \file */

#include <config.h>

#include <stddef.h>

#include <isc/counter.h>
#include <isc/magic.h>
#include <isc/mem.h>
#include <isc/util.h>

#define COUNTER_MAGIC			ISC_MAGIC('C', 'n', 't', 'r')
#define VALID_COUNTER(r)		ISC_MAGIC_VALID(r, COUNTER_MAGIC)

struct isc_counter {
	unsigned int	magic;
	isc_mem_t	*mctx;
	isc_mutex_t	lock;
	unsigned int	references;
	unsigned int	limit;
	unsigned int	used;
};

isc_result_t
isc_counter_create(isc_mem_t *mctx, int limit, isc_counter_t **counterp) {
	isc_result_t result;
	isc_counter_t *counter;

	REQUIRE(counterp != NULL && *counterp == NULL);

	counter = isc_mem_get(mctx, sizeof(*counter));
	if (counter == NULL)
		return (ISC_R_NOMEMORY);

	result = isc_mutex_init(&counter->lock);
	if (result != ISC_R_SUCCESS) {
		isc_mem_put(mctx, counter, sizeof(*counter));
		return (result);
	}

	counter->mctx = NULL;
	isc_mem_attach(mctx, &counter->mctx);

	counter->references = 1;
	counter->limit = limit;
	counter->used = 0;

	counter->magic = COUNTER_MAGIC;
	*counterp = counter;
	return (ISC_R_SUCCESS);
}

isc_result_t
isc_counter_increment(isc_counter_t *counter) {
	isc_result_t result = ISC_R_SUCCESS;

	LOCK(&counter->lock);
	counter->used++;
	if (counter->limit != 0 && counter->used >= counter->limit)
		result = ISC_R_QUOTA;
	UNLOCK(&counter->lock);

	return (result);
}

unsigned int
isc_counter_used(isc_counter_t *counter) {
	REQUIRE(VALID_COUNTER(counter));

	return (counter->used);
}

void
isc_counter_setlimit(isc_counter_t *counter, int limit) {
	REQUIRE(VALID_COUNTER(counter));

	LOCK(&counter->lock);
	counter->limit = limit;
	UNLOCK(&counter->lock);
}

void
isc_counter_attach(isc_counter_t *source, isc_counter_t **targetp) {
	REQUIRE(VALID_COUNTER(source));
	REQUIRE(targetp != NULL && *targetp == NULL);

	LOCK(&source->lock);
	source->references++;
	INSIST(source->references > 0);
	UNLOCK(&source->lock);

	*targetp = source;
}

static void
destroy(isc_counter_t *counter) {
	counter->magic = 0;
	isc_mutex_destroy(&counter->lock);
	isc_mem_putanddetach(&counter->mctx, counter, sizeof(*counter));
}

void
isc_counter_detach(isc_counter_t **counterp) {
	isc_counter_t *counter;
	isc_boolean_t want_destroy = ISC_FALSE;

	REQUIRE(counterp != NULL && *counterp != NULL);
	counter = *counterp;
	REQUIRE(VALID_COUNTER(counter));

	*counterp = NULL;

	LOCK(&counter->lock);
	INSIST(counter->references > 0);
	counter->references--;
	if (counter->references == 0)
		want_destroy = ISC_TRUE;
	UNLOCK(&counter->lock);

	if (want_destroy)
		destroy(counter);
}
