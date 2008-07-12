/*
 * Copyright (C) 2004, 2005  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: lwsearch.h,v 1.5.18.2 2005/04/29 00:15:36 marka Exp $ */

#ifndef NAMED_LWSEARCH_H
#define NAMED_LWSEARCH_H 1

#include <isc/mutex.h>
#include <isc/result.h>
#include <isc/types.h>

#include <dns/types.h>

#include <named/types.h>

/*! \file
 * \brief
 * Lightweight resolver search list types and routines.
 *
 * An ns_lwsearchlist_t holds a list of search path elements.
 *
 * An ns_lwsearchctx stores the state of search list during a lookup
 * operation.
 */

/*% An ns_lwsearchlist_t holds a list of search path elements. */
struct ns_lwsearchlist {
	unsigned int magic;

	isc_mutex_t lock;
	isc_mem_t *mctx;
	unsigned int refs;
	dns_namelist_t names;
};
/*% An ns_lwsearchctx stores the state of search list during a lookup operation. */
struct ns_lwsearchctx {
	dns_name_t *relname;
	dns_name_t *searchname;
	unsigned int ndots;
	ns_lwsearchlist_t *list;
	isc_boolean_t doneexact;
	isc_boolean_t exactfirst;
};

isc_result_t
ns_lwsearchlist_create(isc_mem_t *mctx, ns_lwsearchlist_t **listp);
/*%<
 * Create an empty search list object.
 */

void
ns_lwsearchlist_attach(ns_lwsearchlist_t *source, ns_lwsearchlist_t **target);
/*%<
 * Attach to a search list object.
 */

void
ns_lwsearchlist_detach(ns_lwsearchlist_t **listp);
/*%<
 * Detach from a search list object.
 */

isc_result_t
ns_lwsearchlist_append(ns_lwsearchlist_t *list, dns_name_t *name);
/*%<
 * Append an element to a search list.  This creates a copy of the name.
 */

void
ns_lwsearchctx_init(ns_lwsearchctx_t *sctx, ns_lwsearchlist_t *list,
		    dns_name_t *name, unsigned int ndots);
/*%<
 * Creates a search list context structure.
 */

void
ns_lwsearchctx_first(ns_lwsearchctx_t *sctx);
/*%<
 * Moves the search list context iterator to the first element, which
 * is usually the exact name.
 */

isc_result_t
ns_lwsearchctx_next(ns_lwsearchctx_t *sctx);
/*%<
 * Moves the search list context iterator to the next element.
 */

isc_result_t
ns_lwsearchctx_current(ns_lwsearchctx_t *sctx, dns_name_t *absname);
/*%<
 * Obtains the current name to be looked up.  This involves either
 * concatenating the name with a search path element, making an
 * exact name absolute, or doing nothing.
 */

#endif /* NAMED_LWSEARCH_H */
