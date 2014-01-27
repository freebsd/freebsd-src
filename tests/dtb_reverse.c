/*
 * libfdt - Flat Device Tree manipulation
 *	Tests if two given dtbs are structurally equal (including order)
 * Copyright (C) 2010 David Gibson, IBM Corporation.
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
#include <stdint.h>
#include <limits.h>

#include <libfdt.h>

#include "tests.h"
#include "testdata.h"

#define CHECK(code) \
	{ \
		err = (code); \
		if (err) \
			FAIL(#code ": %s", fdt_strerror(err)); \
	}

static void reverse_reservemap(void *in, void *out, int n)
{
	int err;
	uint64_t addr, size;

	verbose_printf("reverse_reservemap(): %d/%d\n",
		       n, fdt_num_mem_rsv(in));

	if (n < (fdt_num_mem_rsv(in)-1))
		reverse_reservemap(in, out, n+1);

	CHECK(fdt_get_mem_rsv(in, n, &addr, &size));
	CHECK(fdt_add_reservemap_entry(out, addr, size));
	verbose_printf("Added entry 0x%llx 0x%llx\n",
		       (unsigned long long)addr, (unsigned long long)size);
}

static void reverse_properties(void *in, void *out, int offset)
{
	int err;
	int len;
	const char *name;
	const void *data;

	data = fdt_getprop_by_offset(in, offset, &name, &len);
	if (!data)
		FAIL("fdt_getprop_by_offset(): %s\n", fdt_strerror(len));

	verbose_printf("reverse_properties(): offset=%d  name=%s\n",
		       offset, name);

	offset = fdt_next_property_offset(in, offset);
	if (offset >= 0)
		reverse_properties(in, out, offset);
	else if (offset != -FDT_ERR_NOTFOUND)
		FAIL("fdt_next_property_offset(): %s\n", fdt_strerror(offset));

	CHECK(fdt_property(out, name, data, len));
	verbose_printf("  -> output property %s\n", name);
}

static void reverse_node(void *in, void *out, int nodeoffset);

static void reverse_children(void *in, void *out, int offset)
{
	int err;
	int nextoffset = offset;
	int depth = 1;

	do {
		char path[PATH_MAX];

		CHECK(fdt_get_path(in, nextoffset, path, sizeof(path)));
		verbose_printf("reverse_children() offset=%d nextoffset=%d [%s]"
			       " depth=%d\n", offset, nextoffset, path, depth);

		nextoffset = fdt_next_node(in, nextoffset, &depth);
	} while ((depth >= 0) && (depth != 1));

	if (depth == 1)
		reverse_children(in, out, nextoffset);

	reverse_node(in, out, offset);
}

static void reverse_node(void *in, void *out, int nodeoffset)
{
	const char *name = fdt_get_name(in, nodeoffset, NULL);
	char path[PATH_MAX];
	int err;
	int offset;
	int depth = 0;

	CHECK(fdt_get_path(in, nodeoffset, path, sizeof(path)));
	verbose_printf("reverse_node(): nodeoffset=%d [%s]\n",
		       nodeoffset, path);

	CHECK(fdt_begin_node(out, name));

	offset = fdt_first_property_offset(in, nodeoffset);
	if (offset >= 0)
		reverse_properties(in, out, offset);
	else if (offset != -FDT_ERR_NOTFOUND)
		FAIL("fdt_first_property(): %s\n", fdt_strerror(offset));

	offset = fdt_next_node(in, nodeoffset, &depth);

	if (depth == 1)
		reverse_children(in, out, offset);

	CHECK(fdt_end_node(out));
}

int main(int argc, char *argv[])
{
	void *in, *out;
	char outname[PATH_MAX];
	int bufsize;
	int err;

	test_init(argc, argv);
	if (argc != 2)
		CONFIG("Usage: %s <dtb file>", argv[0]);

	in = load_blob(argv[1]);
	sprintf(outname, "%s.reversed.test.dtb", argv[1]);

	bufsize = fdt_totalsize(in);
	out = xmalloc(bufsize);

	CHECK(fdt_create(out, bufsize));

	fdt_set_boot_cpuid_phys(out, fdt_boot_cpuid_phys(in));

	reverse_reservemap(in, out, 0);
	CHECK(fdt_finish_reservemap(out));

	reverse_node(in, out, 0);

	CHECK(fdt_finish(out));

	save_blob(outname, out);

	PASS();
}
