/*
 * Copyright (c) 2005 by Internet Systems Consortium, Inc. ("ISC")
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


/* When this symbol is defined allocations via memget are made slightly 
   bigger and some debugging info stuck before and after the region given 
   back to the caller. */
/* #define DEBUGGING_MEMCLUSTER */
#define MEMCLUSTER_ATEND


#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: memcluster.c,v 1.3.206.8 2006/08/30 23:35:06 marka Exp $";
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

#ifdef MEMCLUSTER_RECORD
#ifndef DEBUGGING_MEMCLUSTER
#define DEBUGGING_MEMCLUSTER
#endif
#endif

#define DEF_MAX_SIZE		1100
#define DEF_MEM_TARGET		4096

typedef u_int32_t fence_t;

typedef struct {
	void *			next;
#if defined(DEBUGGING_MEMCLUSTER)
#if defined(MEMCLUSTER_RECORD)
	const char *		file;
	int			line;
#endif
	size_t			size;
	fence_t			fencepost;
#endif
} memcluster_element;

#define SMALL_SIZE_LIMIT sizeof(memcluster_element)
#define P_SIZE sizeof(void *)
#define FRONT_FENCEPOST 0xfebafeba
#define BACK_FENCEPOST 0xabefabef
#define FENCEPOST_SIZE 4

#ifndef MEMCLUSTER_LITTLE_MALLOC
#define MEMCLUSTER_BIG_MALLOC 1
#define NUM_BASIC_BLOCKS 64
#endif

struct stats {
	u_long			gets;
	u_long			totalgets;
	u_long			blocks;
	u_long			freefrags;
};

#ifdef DO_PTHREADS
#include <pthread.h>
static pthread_mutex_t	memlock = PTHREAD_MUTEX_INITIALIZER;
#define MEMLOCK		(void)pthread_mutex_lock(&memlock)
#define MEMUNLOCK	(void)pthread_mutex_unlock(&memlock)
#else
/*
 * Catch bad lock usage in non threaded build.
 */
static unsigned int	memlock = 0;
#define MEMLOCK		do { INSIST(memlock == 0); memlock = 1; } while (0)
#define MEMUNLOCK	do { INSIST(memlock == 1); memlock = 0; } while (0)
#endif  /* DO_PTHEADS */

/* Private data. */

static size_t			max_size;
static size_t			mem_target;
#ifndef MEMCLUSTER_BIG_MALLOC
static size_t			mem_target_half;
static size_t			mem_target_fudge;
#endif
static memcluster_element **	freelists;
#ifdef MEMCLUSTER_RECORD
static memcluster_element **	activelists;
#endif
#ifdef MEMCLUSTER_BIG_MALLOC
static memcluster_element *	basic_blocks;
#endif
static struct stats *		stats;

/* Forward. */

static size_t			quantize(size_t);
#if defined(DEBUGGING_MEMCLUSTER)
static void			check(unsigned char *, int, size_t);
#endif

/* Public. */

int
meminit(size_t init_max_size, size_t target_size) {

#if defined(DEBUGGING_MEMCLUSTER)
	INSIST(sizeof(fence_t) == FENCEPOST_SIZE);
#endif
	if (freelists != NULL) {
		errno = EEXIST;
		return (-1);
	}
	if (init_max_size == 0U)
		max_size = DEF_MAX_SIZE;
	else
		max_size = init_max_size;
	if (target_size == 0U)
		mem_target = DEF_MEM_TARGET;
	else
		mem_target = target_size;
#ifndef MEMCLUSTER_BIG_MALLOC
	mem_target_half = mem_target / 2;
	mem_target_fudge = mem_target + mem_target / 4;
#endif
	freelists = malloc(max_size * sizeof (memcluster_element *));
	stats = malloc((max_size+1) * sizeof (struct stats));
	if (freelists == NULL || stats == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	memset(freelists, 0,
	       max_size * sizeof (memcluster_element *));
	memset(stats, 0, (max_size + 1) * sizeof (struct stats));
#ifdef MEMCLUSTER_RECORD
	activelists = malloc((max_size + 1) * sizeof (memcluster_element *));
	if (activelists == NULL) {
		errno = ENOMEM;
		return (-1);
	}
	memset(activelists, 0,
	       (max_size + 1) * sizeof (memcluster_element *));
#endif
#ifdef MEMCLUSTER_BIG_MALLOC
	basic_blocks = NULL;
#endif
	return (0);
}

void *
__memget(size_t size) {
	return (__memget_record(size, NULL, 0));
}

void *
__memget_record(size_t size, const char *file, int line) {
	size_t new_size = quantize(size);
#if defined(DEBUGGING_MEMCLUSTER)
	memcluster_element *e;
	char *p;
	fence_t fp = BACK_FENCEPOST;
#endif
	void *ret;

	MEMLOCK;

#if !defined(MEMCLUSTER_RECORD)
	UNUSED(file);
	UNUSED(line);
#endif
	if (freelists == NULL) {
		if (meminit(0, 0) == -1) {
			MEMUNLOCK;
			return (NULL);
		}
	}
	if (size == 0U) {
		MEMUNLOCK;
		errno = EINVAL;
		return (NULL);
	}
	if (size >= max_size || new_size >= max_size) {
		/* memget() was called on something beyond our upper limit. */
		stats[max_size].gets++;
		stats[max_size].totalgets++;
#if defined(DEBUGGING_MEMCLUSTER)
		e = malloc(new_size);
		if (e == NULL) {
			MEMUNLOCK;
			errno = ENOMEM;
			return (NULL);
		}
		e->next = NULL;
		e->size = size;
#ifdef MEMCLUSTER_RECORD
		e->file = file;
		e->line = line;
		e->next = activelists[max_size];
		activelists[max_size] = e;
#endif
		MEMUNLOCK;
		e->fencepost = FRONT_FENCEPOST;
		p = (char *)e + sizeof *e + size;
		memcpy(p, &fp, sizeof fp);
		return ((char *)e + sizeof *e);
#else
		MEMUNLOCK;
		return (malloc(size));
#endif
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

#ifdef MEMCLUSTER_BIG_MALLOC
		if (basic_blocks == NULL) {
			new = malloc(NUM_BASIC_BLOCKS * mem_target);
			if (new == NULL) {
				MEMUNLOCK;
				errno = ENOMEM;
				return (NULL);
			}
			curr = new;
			next = curr + mem_target;
			for (i = 0; i < (NUM_BASIC_BLOCKS - 1); i++) {
				((memcluster_element *)curr)->next = next;
				curr = next;
				next += mem_target;
			}
			/*
			 * curr is now pointing at the last block in the
			 * array.
			 */
			((memcluster_element *)curr)->next = NULL;
			basic_blocks = new;
		}
		total_size = mem_target;
		new = basic_blocks;
		basic_blocks = basic_blocks->next;
#else
		if (new_size > mem_target_half)
			total_size = mem_target_fudge;
		else
			total_size = mem_target;
		new = malloc(total_size);
		if (new == NULL) {
			MEMUNLOCK;
			errno = ENOMEM;
			return (NULL);
		}
#endif
		frags = total_size / new_size;
		stats[new_size].blocks++;
		stats[new_size].freefrags += frags;
		/* Set up a linked-list of blocks of size "new_size". */
		curr = new;
		next = curr + new_size;
		for (i = 0; i < (frags - 1); i++) {
#if defined (DEBUGGING_MEMCLUSTER)
			memset(curr, 0xa5, new_size);
#endif
			((memcluster_element *)curr)->next = next;
			curr = next;
			next += new_size;
		}
		/* curr is now pointing at the last block in the array. */
#if defined (DEBUGGING_MEMCLUSTER)
		memset(curr, 0xa5, new_size);
#endif
		((memcluster_element *)curr)->next = freelists[new_size];
		freelists[new_size] = new;
	}

	/* The free list uses the "rounded-up" size "new_size". */
#if defined (DEBUGGING_MEMCLUSTER)
	e = freelists[new_size];
	ret = (char *)e + sizeof *e;
	/*
	 * Check to see if this buffer has been written to while on free list.
	 */
	check(ret, 0xa5, new_size - sizeof *e);
	/*
	 * Mark memory we are returning.
	 */
	memset(ret, 0xe5, size);
#else
	ret = freelists[new_size];
#endif
	freelists[new_size] = freelists[new_size]->next;
#if defined(DEBUGGING_MEMCLUSTER)
	e->next = NULL;
	e->size = size;
	e->fencepost = FRONT_FENCEPOST;
#ifdef MEMCLUSTER_RECORD
	e->file = file;
	e->line = line;
	e->next = activelists[size];
	activelists[size] = e;
#endif
	p = (char *)e + sizeof *e + size;
	memcpy(p, &fp, sizeof fp);
#endif

	/* 
	 * The stats[] uses the _actual_ "size" requested by the
	 * caller, with the caveat (in the code above) that "size" >= the
	 * max. size (max_size) ends up getting recorded as a call to
	 * max_size.
	 */
	stats[size].gets++;
	stats[size].totalgets++;
	stats[new_size].freefrags--;
	MEMUNLOCK;
#if defined(DEBUGGING_MEMCLUSTER)
	return ((char *)e + sizeof *e);
#else
	return (ret);
#endif
}

/* 
 * This is a call from an external caller, 
 * so we want to count this as a user "put". 
 */
void
__memput(void *mem, size_t size) {
	__memput_record(mem, size, NULL, 0);
}

void
__memput_record(void *mem, size_t size, const char *file, int line) {
	size_t new_size = quantize(size);
#if defined (DEBUGGING_MEMCLUSTER)
	memcluster_element *e;
	memcluster_element *el;
#ifdef MEMCLUSTER_RECORD
	memcluster_element *prev;
#endif
	fence_t fp;
	char *p;
#endif

	MEMLOCK;

#if !defined (MEMCLUSTER_RECORD)
	UNUSED(file);
	UNUSED(line);
#endif

	REQUIRE(freelists != NULL);

	if (size == 0U) {
		MEMUNLOCK;
		errno = EINVAL;
		return;
	}

#if defined (DEBUGGING_MEMCLUSTER)
	e = (memcluster_element *) ((char *)mem - sizeof *e);
	INSIST(e->fencepost == FRONT_FENCEPOST);
	INSIST(e->size == size);
	p = (char *)e + sizeof *e + size;
	memcpy(&fp, p, sizeof fp);
	INSIST(fp == BACK_FENCEPOST);
	INSIST(((u_long)mem % 4) == 0);
#ifdef MEMCLUSTER_RECORD
	prev = NULL;
	if (size == max_size || new_size >= max_size)
		el = activelists[max_size];
	else
		el = activelists[size];
	while (el != NULL && el != e) {
		prev = el;
		el = el->next;
	}
	INSIST(el != NULL);	/* double free */
	if (prev == NULL) {
		if (size == max_size || new_size >= max_size)
			activelists[max_size] = el->next;
		else
			activelists[size] = el->next;
	} else
		prev->next = el->next;
#endif
#endif

	if (size == max_size || new_size >= max_size) {
		/* memput() called on something beyond our upper limit */
#if defined(DEBUGGING_MEMCLUSTER)
		free(e);
#else
		free(mem);
#endif

		INSIST(stats[max_size].gets != 0U);
		stats[max_size].gets--;
		MEMUNLOCK;
		return;
	}

	/* The free list uses the "rounded-up" size "new_size": */
#if defined(DEBUGGING_MEMCLUSTER)
	memset(mem, 0xa5, new_size - sizeof *e); /* catch write after free */
	e->size = 0;	/* catch double memput() */
#ifdef MEMCLUSTER_RECORD
	e->file = file;
	e->line = line;
#endif
#ifdef MEMCLUSTER_ATEND
	e->next = NULL;
	el = freelists[new_size];
	while (el != NULL && el->next != NULL)
		el = el->next;
	if (el)
		el->next = e;
	else
		freelists[new_size] = e;
#else
	e->next = freelists[new_size];
	freelists[new_size] = (void *)e;
#endif
#else
	((memcluster_element *)mem)->next = freelists[new_size];
	freelists[new_size] = (memcluster_element *)mem;
#endif

	/* 
	 * The stats[] uses the _actual_ "size" requested by the
	 * caller, with the caveat (in the code above) that "size" >= the
	 * max. size (max_size) ends up getting recorded as a call to
	 * max_size.
	 */
	INSIST(stats[size].gets != 0U);
	stats[size].gets--;
	stats[new_size].freefrags++;
	MEMUNLOCK;
}

void *
__memget_debug(size_t size, const char *file, int line) {
	void *ptr;
	ptr = __memget_record(size, file, line);
	fprintf(stderr, "%s:%d: memget(%lu) -> %p\n", file, line,
		(u_long)size, ptr);
	return (ptr);
}

void
__memput_debug(void *ptr, size_t size, const char *file, int line) {
	fprintf(stderr, "%s:%d: memput(%p, %lu)\n", file, line, ptr,
		(u_long)size);
	__memput_record(ptr, size, file, line);
}

/*
 * Print the stats[] on the stream "out" with suitable formatting.
 */
void
memstats(FILE *out) {
	size_t i;
#ifdef MEMCLUSTER_RECORD
	memcluster_element *e;
#endif

	MEMLOCK;

	if (freelists == NULL) {
		MEMUNLOCK;
		return;
	}
	for (i = 1; i <= max_size; i++) {
		const struct stats *s = &stats[i];

		if (s->totalgets == 0U && s->gets == 0U)
			continue;
		fprintf(out, "%s%5lu: %11lu gets, %11lu rem",
			(i == max_size) ? ">=" : "  ",
			(unsigned long)i, s->totalgets, s->gets);
		if (s->blocks != 0U)
			fprintf(out, " (%lu bl, %lu ff)",
				s->blocks, s->freefrags);
		fputc('\n', out);
	}
#ifdef MEMCLUSTER_RECORD
	fprintf(out, "Active Memory:\n");
	for (i = 1; i <= max_size; i++) {
		if ((e = activelists[i]) != NULL)
			while (e != NULL) {
				fprintf(out, "%s:%d %p:%lu\n",
				        e->file != NULL ? e->file :
						"<UNKNOWN>", e->line,
					(char *)e + sizeof *e,
					(u_long)e->size);
				e = e->next;
			}
	}
#endif
	MEMUNLOCK;
}

int
memactive(void) {
	size_t i;

	if (stats == NULL)
		return (0);
	for (i = 1; i <= max_size; i++)
		if (stats[i].gets != 0U)
			return (1);
	return (0);
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
#if defined(DEBUGGING_MEMCLUSTER)
	return (size + SMALL_SIZE_LIMIT + sizeof (int));
#else
	return (size);
#endif
}

#if defined(DEBUGGING_MEMCLUSTER)
static void
check(unsigned char *a, int value, size_t len) {
	size_t i;
	for (i = 0; i < len; i++)
		INSIST(a[i] == value);
}
#endif
