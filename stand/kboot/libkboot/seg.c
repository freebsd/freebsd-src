/*-
 * Copyright (c) 2023, Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "stand.h"
#include "seg.h"

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
	free(segs);
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

enum types {
	system_ram = SYSTEM_RAM,
	firmware_reserved,
	linux_code,
	linux_data,
	linux_bss,
	unknown,
};

static struct kv
{
	uint64_t	type;
	char *		name;
	int		flags;
#define KV_KEEPER 1
} str2type_kv[] = {
	{ linux_code,		"Kernel code", KV_KEEPER },
	{ linux_data,		"Kernel data", KV_KEEPER },
	{ linux_bss,		"Kernel bss", KV_KEEPER },
	{ firmware_reserved,	"Reserved" },
};

static const char *
parse_line(const char *line, uint64_t *startp, uint64_t *endp)
{
	const char *walker;
	char *next;
	uint64_t start, end;

	/*
	 * Each line is a range followed by a description of the form:
	 * <hex-number><dash><hex-number><space><colon><space><string>
	 * Bail if we have any parsing errors.
	 */
	walker = line;
	start = strtoull(walker, &next, 16);
	if (start == ULLONG_MAX || walker == next)
		return (NULL);
	walker = next;
	if (*walker != '-')
		return (NULL);
	walker++;
	end = strtoull(walker, &next, 16);
	if (end == ULLONG_MAX || walker == next)
		return (NULL);
	walker = next;
	/* Now eat the ' : ' in front of the string we want to return */
	if (strncmp(walker, " : ", 3) != 0)
		return (NULL);
	*startp = start;
	*endp = end;
	return (walker + 3);
}

static struct kv *
kvlookup(const char *str, struct kv *kvs, size_t nkv)
{
	for (int i = 0; i < nkv; i++)
		if (strcmp(kvs[i].name, str) == 0)
			return (&kvs[i]);

	return (NULL);
}

/* Trim trailing whitespace */
static void
chop(char *line)
{
	char *ep = line + strlen(line) - 1;

	while (ep >= line && isspace(*ep))
		*ep-- = '\0';
}

#define SYSTEM_RAM_STR "System RAM"
#define RESERVED "reserved"

bool
populate_avail_from_iomem(void)
{
	int fd;
	char buf[128];
	const char *str;
	uint64_t start, end;
	struct kv *kv;

	fd = open("host:/proc/iomem", O_RDONLY);
	if (fd == -1) {
		printf("Can't get memory map\n");
		init_avail();
		// Hack: 32G of RAM starting at 4G
		add_avail(4ull << 30, 36ull << 30, system_ram);
		return false;
	}

	if (fgetstr(buf, sizeof(buf), fd) < 0)
		goto out;	/* Nothing to do ???? */
	init_avail();
	chop(buf);
	while (true) {
		/*
		 * Look for top level items we understand.  Skip anything that's
		 * a continuation, since we don't care here. If we care, we'll
		 * consume them all when we recognize that top level item.
		 */
		if (buf[0] == ' ')	/* Continuation lines? Ignore */
			goto next_line;
		str = parse_line(buf, &start, &end);
		if (str == NULL)	/* Malformed -> ignore */
			goto next_line;
		/*
		 * All we care about is System RAM
		 */
		if (strncmp(str, SYSTEM_RAM_STR, sizeof(SYSTEM_RAM_STR) - 1) == 0)
			add_avail(start, end, system_ram);
		else if (strncmp(str, RESERVED, sizeof(RESERVED) - 1) == 0)
			add_avail(start, end, firmware_reserved);
		else
			goto next_line;	/* Ignore hardware */
		while (fgetstr(buf, sizeof(buf), fd) >= 0 && buf[0] == ' ') {
			chop(buf);
			str = parse_line(buf, &start, &end);
			if (str == NULL)
				break;
			kv = kvlookup(str, str2type_kv, nitems(str2type_kv));
			if (kv == NULL) /* failsafe for new types: igonre */
				remove_avail(start, end, unknown);
			else if ((kv->flags & KV_KEEPER) == 0)
				remove_avail(start, end, kv->type);
			/* Else no need to adjust since it's a keeper */
		}

		/*
		 * if buf[0] == ' ' then we know that the fgetstr failed and we
		 * should break. Otherwise fgetstr succeeded and we have a
		 * buffer we need to examine for being a top level item.
		 */
		if (buf[0] == ' ')
			break;
		chop(buf);
		continue; /* buf has next top level line to parse */
next_line:
		if (fgetstr(buf, sizeof(buf), fd) < 0)
			break;
	}

out:
	close(fd);
	return true;
}

/*
 * Return the amount of space available in the segment that @start@ lives in,
 * from @start@ to the end of the segment.
 */
uint64_t
space_avail(uint64_t start)
{
	for (int i = 0; i < nr_seg; i++) {
		if (start >= segs[i].start && start <= segs[i].end)
			return segs[i].end - start;
	}

	/*
	 * Properly used, we should never get here. Unsure if this should be a
	 * panic or not.
	 */
	return 0;
}
