/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for fdt_nop_node()
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
#include <ctype.h>
#include <stdint.h>

#include <libfdt.h>

#include "tests.h"
#include "testdata.h"

#define SPACE	65536

static enum {
	FIXED = 0,
	RESIZE,
	REALLOC,
} alloc_mode;

static void realloc_fdt(void **fdt, size_t *size, bool created)
{
	switch (alloc_mode) {
	case FIXED:
		if (!(*fdt))
			*fdt = xmalloc(*size);
		else
			FAIL("Ran out of space");
		return;

	case RESIZE:
		if (!(*fdt)) {
			*fdt = xmalloc(SPACE);
		} else if (*size < SPACE) {
			*size += 1;
			fdt_resize(*fdt, *fdt, *size);
		} else {
			FAIL("Ran out of space");
		}		
		return;

	case REALLOC:
		*size += 1;
		*fdt = xrealloc(*fdt, *size);
		if (created)
			fdt_resize(*fdt, *fdt, *size);
		return;

	default:
		CONFIG("Bad allocation mode");
	}
}

#define CHECK(code) \
	do {			      \
		err = (code);			     \
		if (err == -FDT_ERR_NOSPACE)			\
			realloc_fdt(&fdt, &size, created);		\
		else if (err)						\
			FAIL(#code ": %s", fdt_strerror(err));		\
	} while (err != 0)

int main(int argc, char *argv[])
{
	void *fdt = NULL;
	size_t size;
	int err;
	bool created = false;
	void *place;
	const char place_str[] = "this is a placeholder string\0string2";
	int place_len = sizeof(place_str);

	test_init(argc, argv);

	if (argc == 1) {
		alloc_mode = FIXED;
		size = SPACE;
	} else if (argc == 2) {
		if (streq(argv[1], "resize")) {
			alloc_mode = REALLOC;
			size = 0;
		} else if (streq(argv[1], "realloc")) {
			alloc_mode = REALLOC;
			size = 0;
		} else {
			char *endp;

			size = strtoul(argv[1], &endp, 0);
			if (*endp == '\0')
				alloc_mode = FIXED;
			else 
				CONFIG("Bad allocation mode \"%s\" specified",
				       argv[1]);
		}
	} else {
		CONFIG("sw_tree1 <dtb file> [<allocation mode>]");
	}

	fdt = xmalloc(size);
	CHECK(fdt_create(fdt, size));

	created = true;

	CHECK(fdt_add_reservemap_entry(fdt, TEST_ADDR_1, TEST_SIZE_1));

	CHECK(fdt_add_reservemap_entry(fdt, TEST_ADDR_2, TEST_SIZE_2));
	CHECK(fdt_finish_reservemap(fdt));

	CHECK(fdt_begin_node(fdt, ""));
	CHECK(fdt_property_string(fdt, "compatible", "test_tree1"));
	CHECK(fdt_property_u32(fdt, "prop-int", TEST_VALUE_1));
	CHECK(fdt_property_u64(fdt, "prop-int64", TEST_VALUE64_1));
	CHECK(fdt_property_string(fdt, "prop-str", TEST_STRING_1));
	CHECK(fdt_property_u32(fdt, "#address-cells", 1));
	CHECK(fdt_property_u32(fdt, "#size-cells", 0));

	CHECK(fdt_begin_node(fdt, "subnode@1"));
	CHECK(fdt_property_string(fdt, "compatible", "subnode1"));
	CHECK(fdt_property_u32(fdt, "reg", 1));
	CHECK(fdt_property_cell(fdt, "prop-int", TEST_VALUE_1));
	CHECK(fdt_begin_node(fdt, "subsubnode"));
	CHECK(fdt_property(fdt, "compatible", "subsubnode1\0subsubnode",
			   23));
	CHECK(fdt_property_placeholder(fdt, "placeholder", place_len, &place));
	memcpy(place, place_str, place_len);
	CHECK(fdt_property_cell(fdt, "prop-int", TEST_VALUE_1));
	CHECK(fdt_end_node(fdt));
	CHECK(fdt_begin_node(fdt, "ss1"));
	CHECK(fdt_end_node(fdt));
	CHECK(fdt_end_node(fdt));

	CHECK(fdt_begin_node(fdt, "subnode@2"));
	CHECK(fdt_property_u32(fdt, "reg", 2));
	CHECK(fdt_property_cell(fdt, "linux,phandle", PHANDLE_1));
	CHECK(fdt_property_cell(fdt, "prop-int", TEST_VALUE_2));
	CHECK(fdt_property_u32(fdt, "#address-cells", 1));
	CHECK(fdt_property_u32(fdt, "#size-cells", 0));
	CHECK(fdt_begin_node(fdt, "subsubnode@0"));
	CHECK(fdt_property_u32(fdt, "reg", 0));
	CHECK(fdt_property_cell(fdt, "phandle", PHANDLE_2));
	CHECK(fdt_property(fdt, "compatible", "subsubnode2\0subsubnode",
			   23));
	CHECK(fdt_property_cell(fdt, "prop-int", TEST_VALUE_2));
	CHECK(fdt_end_node(fdt));
	CHECK(fdt_begin_node(fdt, "ss2"));
	CHECK(fdt_end_node(fdt));

	CHECK(fdt_end_node(fdt));

	CHECK(fdt_end_node(fdt));

	save_blob("unfinished_tree1.test.dtb", fdt);

	CHECK(fdt_finish(fdt));

	verbose_printf("Completed tree, totalsize = %d\n",
		       fdt_totalsize(fdt));

	save_blob("sw_tree1.test.dtb", fdt);

	PASS();
}
