/*-
 * Copyright (c) 2022 Netflix, Inc
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
 */

#include <sys/param.h>
#include <sys/endian.h>

#include "stand.h"
#include "host_syscall.h"
#include "kboot.h"

/* Refactor when we do arm64 */

struct memory_segments
{
	uint64_t	start;
	uint64_t	end;
	uint64_t	type;
};

enum types {
	system_ram = 1,
	acpi_tables,
	acpi_nv_storage,
	unusable,
	persistent_old,
	persistent,
	soft_reserved,
	reserved,
};

struct kv
{
	uint64_t	type;
	char *		name;
} str2type_kv[] = {
	{ system_ram,		"System RAM" },
	{ acpi_tables,		"ACPI Tables" },
	{ acpi_nv_storage,	"ACPI Non-volatile Storage" },
	{ unusable,		"Unusable memory" },
	{ persistent_old,	"Persistent Memory (legacy)" },
	{ persistent,		"Persistent Memory" },
	{ soft_reserved,	"Soft Reserved" },
	{ reserved,		"reserved" },
	{ 0, NULL },
};

#define MEMMAP "/sys/firmware/memmap"

static bool
str2type(struct kv *kv, const char *buf, uint64_t *value)
{
	while (kv->name != NULL) {
		if (strcmp(kv->name, buf) == 0) {
			*value = kv->type;
			return true;
		}
		kv++;
	}

	return false;
}

static int
read_memmap(struct memory_segments *segs, int maxseg)
{
	int n;
	char name[MAXPATHLEN];
	char buf[80];

	n = 0;
	do {
		snprintf(name, sizeof(name), "%s/%d/start", MEMMAP, n);
		if (!file2u64(name, &segs[n].start))
			break;
		snprintf(name, sizeof(name), "%s/%d/length", MEMMAP, n);
		if (!file2u64(name, &segs[n].end))
			break;
		snprintf(name, sizeof(name), "%s/%d/type", MEMMAP, n);
		if (!file2str(name, buf, sizeof(buf)))
			break;
		if (!str2type(str2type_kv, buf, &segs[n].type))
			break;
		n++;
	} while (n < maxseg);

	return n;
}

#define BAD_SEG ~0ULL

#define SZ(s) (((s).end - (s).start) + 1)

static uint64_t
find_ram(struct memory_segments *segs, int nr_seg, uint64_t minpa, uint64_t align,
    uint64_t sz, uint64_t maxpa)
{
	uint64_t start;

	/* XXX assume segs are sorted in numeric order -- assumed not ensured */
	for (int i = 0; i < nr_seg; i++) {
		if (segs[i].type != system_ram ||
		    SZ(segs[i]) < sz ||
		    minpa + sz > segs[i].end ||
		    maxpa < segs[i].start)
			continue;
		start = roundup(segs[i].start, align);
		if (start < minpa)	/* Too small, round up and try again */
			start = (roundup(minpa, align));
		if (start + sz > segs[i].end)	/* doesn't fit in seg */
			continue;
		if (start > maxpa ||		/* Over the edge */
		    start + sz > maxpa)		/* on the edge */
			break;			/* No hope to continue */
		return start;
	}

	return BAD_SEG;
}

uint64_t
kboot_get_phys_load_segment(void)
{
	static uint64_t base_seg = BAD_SEG;
	struct memory_segments segs[32];
	int nr_seg;

	if (base_seg != BAD_SEG)
		return (base_seg);

	nr_seg = read_memmap(segs, nitems(segs));
	if (nr_seg > 0)
		base_seg = find_ram(segs, nr_seg, 2ULL << 20, 2ULL << 20,
		    64ULL << 20, 4ULL << 30);
	if (base_seg == BAD_SEG) {
		/* XXX Should fall back to using /proc/iomem maybe? */
		/* XXX PUNT UNTIL I NEED SOMETHING BETTER */
		base_seg = 42ULL * (1 << 20); /* Jam it in at the odd-ball address of 42MB so it stands out */
	}
	return (base_seg);
}
