/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1997-2001  Internet Software Consortium.
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

/* $Id: heap.h,v 1.16.206.1 2004/03/06 08:14:41 marka Exp $ */

#ifndef ISC_HEAP_H
#define ISC_HEAP_H 1

#include <isc/lang.h>
#include <isc/types.h>

ISC_LANG_BEGINDECLS

/*
 * The comparision function returns ISC_TRUE if the first argument has
 * higher priority than the second argument, and ISC_FALSE otherwise.
 */
typedef isc_boolean_t (*isc_heapcompare_t)(void *, void *);

typedef void (*isc_heapindex_t)(void *, unsigned int);
typedef void (*isc_heapaction_t)(void *, void *);

typedef struct isc_heap isc_heap_t;

isc_result_t	isc_heap_create(isc_mem_t *, isc_heapcompare_t,
				isc_heapindex_t, unsigned int, isc_heap_t **);
void		isc_heap_destroy(isc_heap_t **);
isc_result_t	isc_heap_insert(isc_heap_t *, void *);
void		isc_heap_delete(isc_heap_t *, unsigned int);
void		isc_heap_increased(isc_heap_t *, unsigned int);
void		isc_heap_decreased(isc_heap_t *, unsigned int);
void *		isc_heap_element(isc_heap_t *, unsigned int);
void		isc_heap_foreach(isc_heap_t *, isc_heapaction_t, void *);

ISC_LANG_ENDDECLS

#endif /* ISC_HEAP_H */
