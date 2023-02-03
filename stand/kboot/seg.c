/*-
 * Copyright (c) 2023, Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "stand.h"
#include "kboot.h"

#include <sys/param.h>

static struct memory_segments *segs;
static int nr_seg = 0;
static int segalloc = 0;

void
init_avail(void)
{
	if (segs)
		free(segs);
	nr_seg = 0;
	segalloc = 16;
	segs = malloc(sizeof(*segs) * segalloc);
	if (segs == NULL)
		panic("not enough memory to get memory map\n");
}

/*
 * Make sure at least n items can be accessed in the segs array.  Note the
 * realloc here will invalidate cached pointers (potentially), so addresses
 * into the segs array must be recomputed after this call.
 */
void
need_avail(int n)
{
	if (n <= segalloc)
		return;

	while (n > segalloc)
		segalloc *= 2;
	segs = realloc(segs, segalloc * sizeof(*segs));
	if (segs == NULL)
		panic("not enough memory to get memory map\n");
}

/*
 * Always called for a new range, so always just append a range,
 * unless it's continuous with the prior range.
 */
void
add_avail(uint64_t start, uint64_t end, uint64_t type)
{
	/*
	 * This range is contiguous with the previous range, and is
	 * the same type: we can collapse the two.
	 */
	if (nr_seg >= 1 &&
	    segs[nr_seg - 1].end + 1 == start &&
	    segs[nr_seg - 1].type == type) {
		segs[nr_seg - 1].end = end;
		return;
	}

	/*
	 * Otherwise we need to add a new range at the end, but don't need to
	 * adjust the current end.
	 */
	need_avail(nr_seg + 1);
	segs[nr_seg].start = start;
	segs[nr_seg].end = end;
	segs[nr_seg].type = type;
	nr_seg++;
}

/*
 * All or part of a prior entry needs to be modified. Given the structure of the
 * code, we know that it will always be modifying the last time and/or extending
 * the one before it if its contiguous.
 */
void
remove_avail(uint64_t start, uint64_t end, uint64_t type)
{
	struct memory_segments *s;

	/*
	 * simple case: we are extending a previously removed item.
	 */
	if (nr_seg >= 2) {
		s = &segs[nr_seg - 2];
		if (s->end + 1 == start &&
		    s->type == type) {
			s->end = end;
			/* Now adjust the ending element */
			s++;
			if (s->end == end) {
				/* we've used up the 'free' space */
				nr_seg--;
				return;
			}
			/* Otherwise adjust the 'free' space */
			s->start = end + 1;
			return;
		}
	}

	/*
	 * OK, we have four cases:
	 * (1) The new chunk is at the start of the free space, but didn't catch the above
	 *     folding for whatever reason (different type, start of space). In this case,
	 *     we allocate 1 additional item. The current end is copied to the new end. The
	 *     current end is set to <start, end, type> and the new end's start is set to end + 1.
	 * (2) The new chunk is in the middle of the free space. In this case we allocate 2
	 *     additional items. We copy the current end to the new end, set the new end's start
	 *     to end + 1, the old end's end to start - 1 and the new item is <start, end, type>
	 * (3) The new chunk is at the end of the current end. In this case we allocate 1 more
	 *     and adjust the current end's end to start - 1 and set the new end to <start, end, type>.
	 * (4) The new chunk is exactly the current end, except for type. In this case, we just adjust
	 *     the type.
	 * We can assume we always have at least one chunk since that's created with new_avail() above
	 * necessarily before we are called to subset it.
	 */
	s = &segs[nr_seg - 1];
	if (s->start == start) {
		if (s->end == end) { /* (4) */
			s->type = type;
			return;
		}
		/* chunk at start of old chunk -> (1) */
		need_avail(nr_seg + 1);
		s = &segs[nr_seg - 1];	/* Realloc may change pointers */
		s[1] = s[0];
		s->start = start;
		s->end = end;
		s->type = type;
		s[1].start = end + 1;
		nr_seg++;
		return;
	}
	if (s->end == end) {	/* At end of old chunk (3) */
		need_avail(nr_seg + 1);
		s = &segs[nr_seg - 1];	/* Realloc may change pointers */
		s[1] = s[0];
		s->end = start - 1;
		s[1].start = start;
		s[1].type = type;
		nr_seg++;
		return;
	}
	/* In the middle, need to split things up (2) */
	need_avail(nr_seg + 2);
	s = &segs[nr_seg - 1];	/* Realloc may change pointers */
	s[2] = s[1] = s[0];
	s->end = start - 1;
	s[1].start = start;
	s[1].end = end;
	s[1].type = type;
	s[2].start = end + 1;
	nr_seg += 2;
}

void
print_avail(void)
{
	printf("Found %d RAM segments:\n", nr_seg);

	for (int i = 0; i < nr_seg; i++) {
		printf("%#jx-%#jx type %lu\n",
		    (uintmax_t)segs[i].start,
		    (uintmax_t)segs[i].end,
		    (u_long)segs[i].type);
	}
}

uint64_t
first_avail(uint64_t align, uint64_t min_size, uint64_t memtype)
{
	uint64_t s, len;

	for (int i = 0; i < nr_seg; i++) {
		if (segs[i].type != memtype)	/* Not candidate */
			continue;
		s = roundup(segs[i].start, align);
		if (s >= segs[i].end)		/* roundup past end */
			continue;
		len = segs[i].end - s + 1;
		if (len >= min_size) {
			printf("Found a big enough hole at in seg %d at %#jx (%#jx-%#jx)\n",
			    i,
			    (uintmax_t)s,
			    (uintmax_t)segs[i].start,
			    (uintmax_t)segs[i].end);
			return (s);
		}
	}

	return (0);
}
