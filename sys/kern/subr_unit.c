/*-
 * Copyright (c) 2004 Poul-Henning Kamp
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/* Unit number allocation functions.
 *
 * These functions implement a mixed run-length/bitmap management of unit
 * number spaces.
 *
 * Allocation is always lowest free number first.
 *
 * Worst case memory usage (disregarding boundary effects in the low end)
 * is two bits for each slot in the unit number space.  (For a full
 * [0 ... UINT_MAX] space that is still a lot of course.)
 *
 * The typical case, where no unit numbers are freed, is managed in a
 * constant sized memory footprint of:
 *   sizeof(struct unrhdr) + 2 * sizeof (struct unr) == 56 bytes on i386
 *
 * The caller must provide locking.
 *
 * A userland test program is included.
 *
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/bitstring.h>

#ifdef _KERNEL

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/systm.h>

/*
 * In theory it would be smarter to allocate the individual blocks
 * with the zone allocator, but at this time the expectation is that
 * there will typically not even be enough allocations to fill a single
 * page, so we stick with malloc for now.
 */
static MALLOC_DEFINE(M_UNIT, "Unitno", "Unit number allocation");

#define Malloc(foo) malloc(foo, M_UNIT, M_WAITOK | M_ZERO)
#define Free(foo) free(foo, M_UNIT)

#else /* ...USERLAND */

#include <stdio.h>
#include <stdlib.h>

#define KASSERT(cond, arg) \
	do { \
		if (!(cond)) { \
			printf arg; \
			exit (1); \
		} \
	} while (0)

#define Malloc(foo) calloc(foo, 1)
#define Free(foo) free(foo)

#endif

/*
 * This is our basic building block.
 *
 * It can be used in three different ways depending on the value of the ptr
 * element:
 *     If ptr is NULL, it represents a run of free items.
 *     If ptr points to the unrhdr it represents a run of allocated items.
 *     Otherwise it points to an bitstring of allocated items.
 *
 * For runs the len field is the length of the run.
 * For bitmaps the len field represents the number of allocated items.
 *
 * The bitmap is the same size as struct unr to optimize memory management.
 */
struct unr {
	TAILQ_ENTRY(unr)	list;
	u_int			len;
	void			*ptr;
};

/* Number of bits in the bitmap */
#define NBITS	(sizeof(struct unr) * 8)

/* Header element for a unr number space. */

struct unrhdr {
	TAILQ_HEAD(unrhd,unr)	head;
	u_int			low;	/* Lowest item */
	u_int			high;	/* Highest item */
	u_int			busy;	/* Count of allocated items */
	u_int			alloc;	/* Count of memory allocations */
};


#if defined(DIAGNOSTIC) || !defined(_KERNEL)
/*
 * Consistency check function.
 *
 * Checks the internal consistency as well as we can.
 * 
 * Called at all boundaries of this API.
 */
static void
check_unrhdr(struct unrhdr *uh, int line)
{
	struct unr *up;
	u_int x, y, z, w;

	y = 0;
	z = 0;
	TAILQ_FOREACH(up, &uh->head, list) {
		z++;
		if (up->ptr != uh && up->ptr != NULL) {
			z++;
			w = 0;
			for (x = 0; x < NBITS; x++)
				if (bit_test((bitstr_t *)up->ptr, x))
					w++;
			KASSERT (w == up->len,
			    ("UNR inconsistency: bits %u found %u\n",
			    up->len, w));
		}
		if (up->ptr != NULL)
			y += up->len;
	}
	KASSERT (y == uh->busy,
	    ("UNR inconsistency: items %u found %u (line %d)\n",
	    uh->busy, y, line));
	KASSERT (z == uh->alloc,
	    ("UNR inconsistency: chunks %u found %u (line %d)\n",
	    uh->alloc, z, line));
}

#else

static __inline void
check_unrhdr(struct unrhdr *uh, int line)
{

}

#endif


/*
 * Userland memory management.  Just use calloc and keep track of how
 * many elements we have allocated for check_unrhdr().
 */

static __inline void *
new_unr(struct unrhdr *uh)
{
	uh->alloc++;
	return (Malloc(sizeof (struct unr)));

}

static __inline void
delete_unr(struct unrhdr *uh, void *ptr)
{
	uh->alloc--;
	Free(ptr);
}

/*
 * Allocate a new unrheader set.
 *
 * Highest and lowest valid values given as paramters.
 */

struct unrhdr *
new_unrhdr(u_int low, u_int high, struct mtx *mutex __unused)
{
	struct unrhdr *uh;
	struct unr *up;

	KASSERT(low <= high,
	    ("UNR: use error: new_unrhdr(%u, %u)", low, high));
	uh = Malloc(sizeof *uh);
	TAILQ_INIT(&uh->head);
	uh->low = low;
	uh->high = high;
	up = new_unr(uh);
	up->len = 1 + (high - low);
	up->ptr = NULL;
	TAILQ_INSERT_HEAD(&uh->head, up, list);
	check_unrhdr(uh, __LINE__);
	return (uh);
}

void
delete_unrhdr(struct unrhdr *uh)
{

	KASSERT(uh->busy == 0, ("unrhdr has %u allocations", uh->busy));
	
	/* We should have a single un only */
	delete_unr(uh, TAILQ_FIRST(&uh->head));
	KASSERT(uh->alloc == 0, ("UNR memory leak in delete_unrhdr"));
	Free(uh);
}

/*
 * See if a given unr should be collapsed with a neighbor
 */
static void
collapse_unr(struct unrhdr *uh, struct unr *up)
{
	struct unr *upp;

	upp = TAILQ_PREV(up, unrhd, list);
	if (upp != NULL && up->ptr == upp->ptr) {
		up->len += upp->len;
		TAILQ_REMOVE(&uh->head, upp, list);
		delete_unr(uh, upp);
	}
	upp = TAILQ_NEXT(up, list);
	if (upp != NULL && up->ptr == upp->ptr) {
		up->len += upp->len;
		TAILQ_REMOVE(&uh->head, upp, list);
		delete_unr(uh, upp);
	}
}

/*
 * Allocate a free unr.
 */
u_int
alloc_unr(struct unrhdr *uh)
{
	struct unr *up, *upp;
	u_int x;
	int y;

	check_unrhdr(uh, __LINE__);
	x = uh->low;
	/*
	 * We can always allocate from one of the first two unrs on the list.
	 * The first one is likely an allocated run, but the second has to
	 * be a free run or a bitmap.
	 */
	up = TAILQ_FIRST(&uh->head);
	KASSERT(up != NULL, ("UNR empty list"));
	if (up->ptr == uh) {
		x += up->len;
		up = TAILQ_NEXT(up, list);
	}
	KASSERT(up != NULL, ("UNR Ran out of numbers")); /* XXX */
	KASSERT(up->ptr != uh, ("UNR second element allocated"));

	if (up->ptr != NULL) {
		/* Bitmap unr */
		KASSERT(up->len < NBITS, ("UNR bitmap confusion"));
		bit_ffc((bitstr_t *)up->ptr, NBITS, &y);
		KASSERT(y != -1, ("UNR corruption: No clear bit in bitmap."));
		bit_set((bitstr_t *)up->ptr, y);
		up->len++;
		uh->busy++;
		if (up->len == NBITS) {
			/* The unr is all allocated, drop bitmap */
			delete_unr(uh, up->ptr);
			up->ptr = uh;
			collapse_unr(uh, up);
		}
		check_unrhdr(uh, __LINE__);
		return (x + y);
	}

	if (up->len == 1) {
		/* Run of one free item, grab it */
		up->ptr = uh;
		uh->busy++;
		collapse_unr(uh, up);
		check_unrhdr(uh, __LINE__);
		return (x);
	}

	/*
	 * Slice first item into an preceeding allocated run, even if we
	 * have to create it.  Because allocation is always lowest free
	 * number first, we know the preceeding element (if any) to be
	 * an allocated run.
	 */
	upp = TAILQ_PREV(up, unrhd, list);
	if (upp == NULL) {
		upp = new_unr(uh);
		upp->len = 0;
		upp->ptr = uh;
		TAILQ_INSERT_BEFORE(up, upp, list);
	}
	KASSERT(upp->ptr == uh, ("UNR list corruption"));
	upp->len++;
	up->len--;
	uh->busy++;
	check_unrhdr(uh, __LINE__);
	return (x);
}

/*
 * Free a unr.
 *
 * If we can save unrs by using a bitmap, do so.
 */
void
free_unr(struct unrhdr *uh, u_int item)
{
	struct unr *up, *upp, *upn, *ul;
	u_int x, l, xl, n, pl;

	KASSERT(item >= uh->low && item <= uh->high,
	    ("UNR: free_unr(%u) out of range [%u...%u]",
	     item, uh->low, uh->high));
	check_unrhdr(uh, __LINE__);
	item -= uh->low;
	xl = x = 0;
	/* Find the start of the potential bitmap */
	l = item - item % NBITS;
	ul = 0;
	TAILQ_FOREACH(up, &uh->head, list) {

		/* Keep track of which unr we'll split if we do */
		if (x <= l) {
			ul = up;
			xl = x;
		}

		/* Handle bitmap items */
		if (up->ptr != NULL && up->ptr != uh) {
			if (x + NBITS <= item) { /* not yet */
				x += NBITS;
				continue;
			}
			KASSERT(bit_test((bitstr_t *)up->ptr, item - x) != 0,
			    ("UNR: Freeing free item %d (%d) (bitmap)\n",
			     item, item - x));
			bit_clear((bitstr_t *)up->ptr, item - x);
			uh->busy--;
			up->len--;
			/*
			 * XXX: up->len == 1 could possibly be collapsed to
			 * XXX: neighboring runs.
			 */
			if (up->len > 0)
				return;
			/* We have freed all items in bitmap, drop it */
			delete_unr(uh, up->ptr);
			up->ptr = NULL;
			up->len = NBITS;
			collapse_unr(uh, up);
			check_unrhdr(uh, __LINE__);
			return;
		}

		/* Run length unr's */
		
		if (x + up->len <= item) {	/* not yet */
			x += up->len;
			continue;
		}

		/* We now have our run length unr */
		KASSERT(up->ptr == uh,
		    ("UNR Freeing free item %d (run))\n", item));

		/* Just this one left, reap it */
		if (up->len == 1) {
			up->ptr = NULL;
			uh->busy--;
			collapse_unr(uh, up);
			check_unrhdr(uh, __LINE__);
			return;
		}

		/* Check if we can shift the item to the previous run */
		upp = TAILQ_PREV(up, unrhd, list);
		if (item == x && upp != NULL && upp->ptr == NULL) {
			upp->len++;
			up->len--;
			uh->busy--;
			check_unrhdr(uh, __LINE__);
			return;
		}

		/* Check if we can shift the item to the next run */
		upn = TAILQ_NEXT(up, list);
		if (item == x + up->len - 1 &&
		    upn != NULL && upn->ptr == NULL) {
			upn->len++;
			up->len--;
			uh->busy--;
			check_unrhdr(uh, __LINE__);
			return;
		}

		/* Split off the tail end, if any. */
		pl = up->len - (1 + (item - x));
		if (pl > 0) {
			upp = new_unr(uh);
			upp->ptr = uh;
			upp->len = pl;
			TAILQ_INSERT_AFTER(&uh->head, up, upp, list);
		}

		if (item == x) {
			/* We are done splitting */
			up->len = 1;
			up->ptr = NULL;
		} else {
			/* The freed item */
			upp = new_unr(uh);
			upp->len = 1;
			upp->ptr = NULL;
			TAILQ_INSERT_AFTER(&uh->head, up, upp, list);
			/* Adjust current unr */
			up->len = item - x;
		}

		uh->busy--;
		check_unrhdr(uh, __LINE__);

		/* Our ul marker element may have shifted one later */
		if (ul->len + xl <= l) {
			xl += ul->len;
			ul = TAILQ_NEXT(ul, list);
		}
		KASSERT(ul != NULL, ("UNR lost bitmap pointer"));

		/* Count unrs entirely inside potential bitmap */
		n = 0;
		pl = xl;
		item = l + NBITS;
		for (up = ul;
		     up != NULL && pl + up->len <= item;
		     up = TAILQ_NEXT(up, list)) {
			if (pl >= l)
				n++;
			pl += up->len;
		}

		/* If less than three, a bitmap does not pay off */
		if (n < 3)
			return;

		/* Allocate bitmap */
		upp = new_unr(uh);
		upp->ptr = new_unr(uh);

		/* Insert bitmap after ul element */
		TAILQ_INSERT_AFTER(&uh->head, ul, upp, list);

		/* Slice off the tail from the ul element */
		pl = ul->len - (l - xl);
		if (ul->ptr != NULL) {
			bit_nset(upp->ptr, 0, pl - 1);
			upp->len = pl;
		}
		ul->len -= pl;

		/* Ditch ul if it got reduced to zero size */
		if (ul->len == 0) {
			TAILQ_REMOVE(&uh->head, ul, list);
			delete_unr(uh, ul);
		}

		/* Soak up run length unrs until we have absorbed NBITS */
		while (pl != NBITS) {

			/* Grab first one in line */
			upn = TAILQ_NEXT(upp, list);

			/* We may not have a multiple of NBITS totally */
			if (upn == NULL)
				break;

			/* Run may extend past our new bitmap */
			n = NBITS - pl;
			if (n > upn->len)
				n = upn->len;

			if (upn->ptr != NULL) {
				bit_nset(upp->ptr, pl, pl + n - 1);
				upp->len += n;
			}
			pl += n;

			if (n != upn->len) {
				/* We did not absorb the entire run */
				upn->len -= n;
				break;
			} 
			TAILQ_REMOVE(&uh->head, upn, list);
			delete_unr(uh, upn);
		}
		check_unrhdr(uh, __LINE__);
		return;
	}
	KASSERT(0 != 1, ("UNR: Fell off the end in free_unr()"));
}

#ifndef _KERNEL	/* USERLAND test driver */

/*
 * Simple stochastic test driver for the above functions
 */

static void
print_unr(struct unrhdr *uh, struct unr *up)
{
	u_int x;

	printf("  %p len = %5u ", up, up->len);
	if (up->ptr == NULL)
		printf("free\n");
	else if (up->ptr == uh)
		printf("alloc\n");
	else {
		printf(" [");
		for (x = 0; x < NBITS; x++) {
			if (bit_test((bitstr_t *)up->ptr, x))
				putchar('#');
			else 
				putchar(' ');
		}
		printf("]\n");
	}
}

static void
print_unrhdr(struct unrhdr *uh)
{
	struct unr *up;
	u_int x;

	printf("%p low = %u high = %u busy %u\n",
	    uh, uh->low, uh->high, uh->busy);
	x = uh->low;
	TAILQ_FOREACH(up, &uh->head, list) {
		printf("  from = %5u", x);
		print_unr(uh, up);
		if (up->ptr == NULL || up->ptr == uh)
			x += up->len;
		else
			x += NBITS;
	}
}

/* Number of unrs to test */
#define NN	10000

int
main(int argc __unused, const char **argv __unused)
{
	struct unrhdr *uh;
	int i, x, m;
	char a[NN];

	uh = new_unrhdr(0, NN - 1, NULL);

	memset(a, 0, sizeof a);

	fprintf(stderr, "sizeof(struct unr) %d\n", sizeof (struct unr));
	fprintf(stderr, "sizeof(struct unrhdr) %d\n", sizeof (struct unrhdr));
	x = 1;
	for (m = 0; m < NN; m++) {
		i = random() % NN;
		if (a[i]) {
			printf("F %u\n", i);
			free_unr(uh, i);
			a[i] = 0;
		} else {
			i = alloc_unr(uh);
			a[i] = 1;
			printf("A %u\n", i);
		}
		if (1)	/* XXX: change this for detailed debug printout */
			print_unrhdr(uh);
		check_unrhdr(uh, __LINE__);
	}
	for (i = 0; i < NN; i++)
		if (a[i])
			free_unr(uh, i);
	print_unrhdr(uh);
	delete_unrhdr(uh);
	return (0);
}
#endif
