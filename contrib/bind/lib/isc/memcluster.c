/*
 * Copyright (c) 1997 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#if !defined(LINT) && !defined(CODECENTER)
static char rcsid[] = "$Id: memcluster.c,v 8.7 1998/03/27 00:17:31 halley Exp $";
#endif /* not lint */

#include "port_before.h"

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <isc/memcluster.h>
#include <isc/assertions.h>

#include "port_after.h"

#define DEF_MAX_SIZE		1100
#define DEF_MEM_TARGET		4096

typedef struct {
	void *			next;
} memcluster_element;

#define SMALL_SIZE_LIMIT sizeof(memcluster_element)
#define P_SIZE sizeof(void *)

struct stats {
	u_long			gets;
	u_long			puts;
	u_long			blocks;
	u_long			freefrags;
};

/* Private data. */

static size_t			max_size;
static size_t			mem_target;
static size_t			mem_target_half;
static size_t			mem_target_fudge;
static memcluster_element **	freelists;
static struct stats *		stats;

/* Forward. */

static size_t			quantize(size_t);

/* Public. */

int
meminit(size_t init_max_size, size_t target_size) {
	int i;

	if (freelists != NULL) {
		errno = EEXIST;
		return (-1);
	}
	if (init_max_size == 0)
		max_size = DEF_MAX_SIZE;
	else
		max_size = init_max_size;
	if (target_size == 0)
		mem_target = DEF_MEM_TARGET;
	else
		mem_target = target_size;
	mem_target_half = mem_target / 2;
	mem_target_fudge = mem_target + mem_target / 4;
	freelists = malloc(max_size * sizeof (memcluster_element *));
	stats = malloc((max_size+1) * sizeof (struct stats));
	if (freelists == NULL || stats == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	memset(freelists, 0,
	       max_size * sizeof (memcluster_element *));
	memset(stats, 0, (max_size + 1) * sizeof (struct stats));

	return (0);
}

void *
__memget(size_t size) {
	size_t new_size = quantize(size);
	void *ret;

	if (freelists == NULL)
		if (meminit(0, 0) == -1)
			return (NULL);
	if (size == 0) {
		errno = EINVAL;
		return (NULL);
	}
	if (size >= max_size || new_size >= max_size) {
		/* memget() was called on something beyond our upper limit. */
		stats[max_size].gets++;
		return (malloc(size));
	}

	/* 
	 * If there are no blocks in the free list for this size, get a chunk
	 * of memory and then break it up into "new_size"-sized blocks, adding
	 * them to the free list.
	 */
	if (freelists[new_size] == NULL) {
		int i, frags;
		size_t total_size;
		void *new;
		char *curr, *next;

		if (new_size > mem_target_half)
			total_size = mem_target_fudge;
		else
			total_size = mem_target;
		new = malloc(total_size);
		if (new == NULL) {
			errno = ENOMEM;
			return (NULL);
		}
		frags = total_size / new_size;
		stats[new_size].blocks++;
		stats[new_size].freefrags += frags;
		/* Set up a linked-list of blocks of size "new_size". */
		curr = new;
		next = curr + new_size;
		for (i = 0; i < (frags - 1); i++) {
			((memcluster_element *)curr)->next = next;
			curr = next;
			next += new_size;
		}
		/* curr is now pointing at the last block in the array. */
		((memcluster_element *)curr)->next = freelists[new_size];
		freelists[new_size] = new;
	}

	/* The free list uses the "rounded-up" size "new_size": */
	ret = freelists[new_size];
	freelists[new_size] = freelists[new_size]->next;

	/* 
	 * The stats[] uses the _actual_ "size" requested by the
	 * caller, with the caveat (in the code above) that "size" >= the
	 * max. size (max_size) ends up getting recorded as a call to
	 * max_size.
	 */
	stats[size].gets++;
	stats[new_size].freefrags--;
	return (ret);
}

/* 
 * This is a call from an external caller, 
 * so we want to count this as a user "put". 
 */
void
__memput(void *mem, size_t size) {
	size_t new_size = quantize(size);

	REQUIRE(freelists != NULL);

	if (size == 0) {
		errno = EINVAL;
		return;
	}
	if (size == max_size || new_size >= max_size) {
		/* memput() called on something beyond our upper limit */
		free(mem);
		INSIST(stats[max_size].puts < stats[max_size].gets);
		stats[max_size].puts++;
		return;
	}

	/* The free list uses the "rounded-up" size "new_size": */
	((memcluster_element *)mem)->next = freelists[new_size];
	freelists[new_size] = (memcluster_element *)mem;

	/* 
	 * The stats[] uses the _actual_ "size" requested by the
	 * caller, with the caveat (in the code above) that "size" >= the
	 * max. size (max_size) ends up getting recorded as a call to
	 * max_size.
	 */
	INSIST(stats[size].puts < stats[size].gets);
	stats[size].puts++;
	stats[new_size].freefrags++;
}

void *
__memget_debug(size_t size, const char *file, int line) {
	void *ptr;
	ptr = __memget(size);
	fprintf(stderr, "%s:%d: memget(%lu) -> %p\n", file, line,
		(u_long)size, ptr);
	return (ptr);
}

void
__memput_debug(void *ptr, size_t size, const char *file, int line) {
	fprintf(stderr, "%s:%d: memput(%p, %lu)\n", file, line, ptr,
		(u_long)size);
	__memput(ptr, size);
}

/*
 * Print the stats[] on the stream "out" with suitable formatting.
 */
void
memstats(FILE *out) {
	size_t i;

	if (freelists == NULL)
		return;
	for (i = 1; i <= max_size; i++) {
		const struct stats *s = &stats[i];

		if (s->gets == 0 && s->puts == 0)
			continue;
		INSIST(s->gets >= s->puts);
		fprintf(out, "%s%5d: %11lu get, %11lu put, %11lu rem",
			(i == max_size) ? ">=" : "  ",
			i, s->gets, s->puts, s->gets - s->puts);
		if (s->blocks != 0)
			fprintf(out, " (%lu bl, %lu ff)",
				s->blocks, s->freefrags);
		fputc('\n', out);
	}
}

/* Private. */

/* 
 * Round up size to a multiple of sizeof(void *).  This guarantees that a
 * block is at least sizeof void *, and that we won't violate alignment
 * restrictions, both of which are needed to make lists of blocks.
 */
static size_t 
quantize(size_t size) {
	int remainder;
	/*
	 * If there is no remainder for the integer division of 
	 *
	 *	(rightsize/P_SIZE)
	 *
	 * then we already have a good size; if not, then we need
	 * to round up the result in order to get a size big
	 * enough to satisfy the request _and_ aligned on P_SIZE boundaries.
	 */
	remainder = size % P_SIZE;
	if (remainder != 0)
        	size += P_SIZE - remainder;
	return (size);
}

