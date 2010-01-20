/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1997,1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Heap implementation of priority queues adapted from the following:
 *
 *	_Introduction to Algorithms_, Cormen, Leiserson, and Rivest,
 *	MIT Press / McGraw Hill, 1990, ISBN 0-262-03141-8, chapter 7.
 *
 *	_Algorithms_, Second Edition, Sedgewick, Addison-Wesley, 1988,
 *	ISBN 0-201-06673-4, chapter 11.
 */

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: heap.c,v 1.1.206.2 2006/03/10 00:17:21 marka Exp $";
#endif /* not lint */

#include "port_before.h"

#include <stddef.h>
#include <stdlib.h>
#include <errno.h>

#include "port_after.h"

#include <isc/heap.h>

/*
 * Note: to make heap_parent and heap_left easy to compute, the first
 * element of the heap array is not used; i.e. heap subscripts are 1-based,
 * not 0-based.
 */
#define heap_parent(i) ((i) >> 1)
#define heap_left(i) ((i) << 1)

#define ARRAY_SIZE_INCREMENT 512

heap_context
heap_new(heap_higher_priority_func higher_priority, heap_index_func index,
	 int array_size_increment) {
	heap_context ctx;

	if (higher_priority == NULL)
		return (NULL);

	ctx = (heap_context)malloc(sizeof (struct heap_context));
	if (ctx == NULL)
		return (NULL);

	ctx->array_size = 0;
	if (array_size_increment == 0)
		ctx->array_size_increment = ARRAY_SIZE_INCREMENT;
	else
		ctx->array_size_increment = array_size_increment;
	ctx->heap_size = 0;
	ctx->heap = NULL;
	ctx->higher_priority = higher_priority;
	ctx->index = index;
	return (ctx);
}

int
heap_free(heap_context ctx) {
	if (ctx == NULL) {
		errno = EINVAL;
		return (-1);
	}

	if (ctx->heap != NULL)
		free(ctx->heap);
	free(ctx);

	return (0);
}

static int
heap_resize(heap_context ctx) {
	void **new_heap;

	ctx->array_size += ctx->array_size_increment;
	new_heap = (void **)realloc(ctx->heap,
				    (ctx->array_size) * (sizeof (void *)));
	if (new_heap == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	ctx->heap = new_heap;
	return (0);
}

static void
float_up(heap_context ctx, int i, void *elt) {
	int p;

	for ( p = heap_parent(i); 
	      i > 1 && ctx->higher_priority(elt, ctx->heap[p]);
	      i = p, p = heap_parent(i) ) {
		ctx->heap[i] = ctx->heap[p];
		if (ctx->index != NULL)
			(ctx->index)(ctx->heap[i], i);
	}
	ctx->heap[i] = elt;
	if (ctx->index != NULL)
		(ctx->index)(ctx->heap[i], i);
}

static void
sink_down(heap_context ctx, int i, void *elt) {
	int j, size, half_size;

	size = ctx->heap_size;
	half_size = size / 2;
	while (i <= half_size) {
		/* find smallest of the (at most) two children */
		j = heap_left(i);
		if (j < size && ctx->higher_priority(ctx->heap[j+1],
						     ctx->heap[j]))
			j++;
		if (ctx->higher_priority(elt, ctx->heap[j]))
			break;
		ctx->heap[i] = ctx->heap[j];
		if (ctx->index != NULL)
			(ctx->index)(ctx->heap[i], i);
		i = j;
	}
	ctx->heap[i] = elt;
	if (ctx->index != NULL)
		(ctx->index)(ctx->heap[i], i);
}

int
heap_insert(heap_context ctx, void *elt) {
	int i;

	if (ctx == NULL || elt == NULL) {
		errno = EINVAL;
		return (-1);
	}

	i = ++ctx->heap_size;
	if (ctx->heap_size >= ctx->array_size && heap_resize(ctx) < 0)
		return (-1);
	
	float_up(ctx, i, elt);

	return (0);
}

int
heap_delete(heap_context ctx, int i) {
	void *elt;
	int less;

	if (ctx == NULL || i < 1 || i > ctx->heap_size) {
		errno = EINVAL;
		return (-1);
	}

	if (i == ctx->heap_size) {
		ctx->heap_size--;
	} else {
		elt = ctx->heap[ctx->heap_size--];
		less = ctx->higher_priority(elt, ctx->heap[i]);
		ctx->heap[i] = elt;
		if (less)
			float_up(ctx, i, ctx->heap[i]);
		else
			sink_down(ctx, i, ctx->heap[i]);
	}

	return (0);
}

int
heap_increased(heap_context ctx, int i) {
     	if (ctx == NULL || i < 1 || i > ctx->heap_size) {
		errno = EINVAL;
		return (-1);
	}
	
	float_up(ctx, i, ctx->heap[i]);

	return (0);
}

int
heap_decreased(heap_context ctx, int i) {
     	if (ctx == NULL || i < 1 || i > ctx->heap_size) {
		errno = EINVAL;
		return (-1);
	}
	
	sink_down(ctx, i, ctx->heap[i]);

	return (0);
}

void *
heap_element(heap_context ctx, int i) {
	if (ctx == NULL || i < 1 || i > ctx->heap_size) {
		errno = EINVAL;
		return (NULL);
	}

	return (ctx->heap[i]);
}

int
heap_for_each(heap_context ctx, heap_for_each_func action, void *uap) {
	int i;

	if (ctx == NULL || action == NULL) {
		errno = EINVAL;
		return (-1);
	}

	for (i = 1; i <= ctx->heap_size; i++)
		(action)(ctx->heap[i], uap);
	return (0);
}
