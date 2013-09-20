/*
 * Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef ISC_OBJPOOL_H
#define ISC_OBJPOOL_H 1

/*****
 ***** Module Info
 *****/

/*! \file isc/pool.h
 * \brief An object pool is a mechanism for sharing a small pool of
 * fungible objects among a large number of objects that depend on them.
 *
 * This is useful, for example, when it causes performance problems for
 * large number of zones to share a single memory context or task object,
 * but it would create a different set of problems for them each to have an
 * independent task or memory context.
 */


/***
 *** Imports.
 ***/

#include <isc/lang.h>
#include <isc/mem.h>
#include <isc/types.h>

ISC_LANG_BEGINDECLS

/*****
 ***** Types.
 *****/

typedef void
(*isc_pooldeallocator_t)(void **object);

typedef isc_result_t
(*isc_poolinitializer_t)(void **target, void *arg);

typedef struct isc_pool isc_pool_t;

/*****
 ***** Functions.
 *****/

isc_result_t
isc_pool_create(isc_mem_t *mctx, unsigned int count,
		isc_pooldeallocator_t free,
		isc_poolinitializer_t init, void *initarg,
		isc_pool_t **poolp);
/*%<
 * Create a pool of "count" object pointers. If 'free' is not NULL,
 * it points to a function that will detach the objects.  'init'
 * points to a function that will initialize the arguments, and
 * 'arg' to an argument to be passed into that function (for example,
 * a relevant manager or context object).
 *
 * Requires:
 *
 *\li	'mctx' is a valid memory context.
 *
 *\li	init != NULL
 *
 *\li	poolp != NULL && *poolp == NULL
 *
 * Ensures:
 *
 *\li	On success, '*poolp' points to the new object pool.
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS
 *\li	#ISC_R_NOMEMORY
 *\li	#ISC_R_UNEXPECTED
 */

void *
isc_pool_get(isc_pool_t *pool);
/*%<
 * Returns a pointer to an object from the pool. Currently the object
 * is chosen from the pool at random.  (This may be changed in the future
 * to something that guaratees balance.)
 */

int
isc_pool_count(isc_pool_t *pool);
/*%<
 * Returns the number of objcts in the pool 'pool'.
 */

isc_result_t
isc_pool_expand(isc_pool_t **sourcep, unsigned int count, isc_pool_t **targetp);

/*%<
 * If 'size' is larger than the number of objects in the pool pointed to by
 * 'sourcep', then a new pool of size 'count' is allocated, the existing
 * objects are copied into it, additional ones created to bring the
 * total number up to 'count', and the resulting pool is attached to
 * 'targetp'.
 *
 * If 'count' is less than or equal to the number of objects in 'source', then
 * 'sourcep' is attached to 'targetp' without any other action being taken.
 *
 * In either case, 'sourcep' is detached.
 *
 * Requires:
 *
 * \li	'sourcep' is not NULL and '*source' is not NULL
 * \li	'targetp' is not NULL and '*source' is NULL
 *
 * Ensures:
 *
 * \li	On success, '*targetp' points to a valid task pool.
 * \li	On success, '*sourcep' points to NULL.
 *
 * Returns:
 *
 * \li	#ISC_R_SUCCESS
 * \li	#ISC_R_NOMEMORY
 */

void
isc_pool_destroy(isc_pool_t **poolp);
/*%<
 * Destroy a task pool.  The tasks in the pool are detached but not
 * shut down.
 *
 * Requires:
 * \li	'*poolp' is a valid task pool.
 */

ISC_LANG_ENDDECLS

#endif /* ISC_OBJPOOL_H */
