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
#include "efi.h"
#include "kboot.h"
#include "bootstrap.h"

bool
enumerate_memory_arch(void)
{
	efi_read_from_sysfs();
	if (!populate_avail_from_iomem())
		return (false);
	print_avail();
	return (true);
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
