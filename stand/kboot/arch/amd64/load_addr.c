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
#include <machine/pc/bios.h>
#include <machine/metadata.h>

#include "stand.h"
#include "host_syscall.h"
#include "kboot.h"
#include "bootstrap.h"

/* Refactor when we do arm64 */

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

static struct memory_segments segs[64];	/* make dynamic later */
static int nr_seg;

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

bool
enumerate_memory_arch(void)
{
	int n;
	char name[MAXPATHLEN];
	char buf[80];

	for (n = 0; n < nitems(segs); n++) {
		snprintf(name, sizeof(name), "%s/%d/start", MEMMAP, n);
		if (!file2u64(name, &segs[n].start))
			break;
		snprintf(name, sizeof(name), "%s/%d/end", MEMMAP, n);
		if (!file2u64(name, &segs[n].end))
			break;
		snprintf(name, sizeof(name), "%s/%d/type", MEMMAP, n);
		if (!file2str(name, buf, sizeof(buf)))
			break;
		if (!str2type(str2type_kv, buf, &segs[n].type))
			break;
	}

	nr_seg = n;

	return true;
}

#define BAD_SEG ~0ULL

#define SZ(s) (((s).end - (s).start) + 1)

static uint64_t
find_ram(struct memory_segments *segs, int nr_seg, uint64_t minpa, uint64_t align,
    uint64_t sz, uint64_t maxpa)
{
	uint64_t start;

	printf("minpa %#jx align %#jx sz %#jx maxpa %#jx\n",
	    (uintmax_t)minpa,
	    (uintmax_t)align,
	    (uintmax_t)sz,
	    (uintmax_t)maxpa);
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

	if (base_seg != BAD_SEG)
		return (base_seg);

	if (nr_seg > 0)
		base_seg = find_ram(segs, nr_seg, 2ULL << 20, 2ULL << 20,
		    64ULL << 20, 4ULL << 30);
	if (base_seg == BAD_SEG) {
		/* XXX Should fall back to using /proc/iomem maybe? */
		/* XXX PUNT UNTIL I NEED SOMETHING BETTER */
		base_seg = 300ULL * (1 << 20);
	}
	return (base_seg);
}

void
bi_loadsmap(struct preloaded_file *kfp)
{
	struct bios_smap smap[32], *sm;
	struct memory_segments *s;
	int smapnum, len;

	for (smapnum = 0; smapnum < min(32, nr_seg); smapnum++) {
		sm = &smap[smapnum];
		s = &segs[smapnum];
		sm->base = s->start;
		sm->length = s->end - s->start + 1;
		sm->type = SMAP_TYPE_MEMORY;
	}

        len = smapnum * sizeof(struct bios_smap);
        file_addmetadata(kfp, MODINFOMD_SMAP, len, &smap[0]);
}
