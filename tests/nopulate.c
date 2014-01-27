/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase/tool for rearranging blocks of a dtb
 * Copyright (C) 2006 David Gibson, IBM Corporation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

#include <libfdt.h>

#include "tests.h"
#include "testdata.h"

static int nopulate_struct(char *buf, const char *fdt)
{
	int offset, nextoffset = 0;
	uint32_t tag;
	char *p;

	p = buf;

	do {
		offset = nextoffset;
		tag = fdt_next_tag(fdt, offset, &nextoffset);

		memcpy(p, (const char *)fdt + fdt_off_dt_struct(fdt) + offset,
		       nextoffset - offset);
		p += nextoffset - offset;

		*((uint32_t *)p) = cpu_to_fdt32(FDT_NOP);
		p += FDT_TAGSIZE;

	} while (tag != FDT_END);

	return p - buf;
}

int main(int argc, char *argv[])
{
	char *fdt, *fdt2, *buf;
	int newsize, struct_start, struct_end_old, struct_end_new, delta;
	const char *inname;
	char outname[PATH_MAX];

	test_init(argc, argv);
	if (argc != 2)
		CONFIG("Usage: %s <dtb file>", argv[0]);

	inname = argv[1];
	fdt = load_blob(argv[1]);
	sprintf(outname, "noppy.%s", inname);

	if (fdt_version(fdt) < 17)
		FAIL("Can't deal with version <17");

	buf = xmalloc(2 * fdt_size_dt_struct(fdt));

	newsize = nopulate_struct(buf, fdt);

	verbose_printf("Nopulated structure block has new size %d\n", newsize);

	/* Replace old strcutre block with the new */

	fdt2 = xmalloc(fdt_totalsize(fdt) + newsize);

	struct_start = fdt_off_dt_struct(fdt);
	delta = newsize - fdt_size_dt_struct(fdt);
	struct_end_old = struct_start + fdt_size_dt_struct(fdt);
	struct_end_new = struct_start + newsize;

	memcpy(fdt2, fdt, struct_start);
	memcpy(fdt2 + struct_start, buf, newsize);
	memcpy(fdt2 + struct_end_new, fdt + struct_end_old,
	       fdt_totalsize(fdt) - struct_end_old);

	fdt_set_totalsize(fdt2, fdt_totalsize(fdt) + delta);
	fdt_set_size_dt_struct(fdt2, newsize);

	if (fdt_off_mem_rsvmap(fdt) > struct_start)
		fdt_set_off_mem_rsvmap(fdt2, fdt_off_mem_rsvmap(fdt) + delta);
	if (fdt_off_dt_strings(fdt) > struct_start)
		fdt_set_off_dt_strings(fdt2, fdt_off_dt_strings(fdt) + delta);

	save_blob(outname, fdt2);

	PASS();
}
