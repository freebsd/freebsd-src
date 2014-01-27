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

#define CHECK(code) \
	{ \
		err = (code); \
		if (err) \
			FAIL(#code ": %s", fdt_strerror(err)); \
	}

#define OFF_CHECK(off, code) \
	{ \
		(off) = (code); \
		if (off < 0) \
			FAIL(#code ": %s", fdt_strerror(off)); \
	}

int main(int argc, char *argv[])
{
	void *fdt;
	int err;
	int offset, s1, s2;

	test_init(argc, argv);

	fdt = xmalloc(SPACE);

	/* First create empty tree with SW */
	CHECK(fdt_create_empty_tree(fdt, SPACE));

	CHECK(fdt_add_mem_rsv(fdt, TEST_ADDR_1, TEST_SIZE_1));
	CHECK(fdt_add_mem_rsv(fdt, TEST_ADDR_2, TEST_SIZE_2));

	CHECK(fdt_setprop_string(fdt, 0, "compatible", "test_tree1"));
	CHECK(fdt_setprop_u32(fdt, 0, "prop-int", TEST_VALUE_1));
	CHECK(fdt_setprop_u64(fdt, 0, "prop-int64", TEST_VALUE64_1));
	CHECK(fdt_setprop_string(fdt, 0, "prop-str", TEST_STRING_1));

	OFF_CHECK(offset, fdt_add_subnode(fdt, 0, "subnode@1"));
	s1 = offset;
	CHECK(fdt_setprop_string(fdt, s1, "compatible", "subnode1"));
	CHECK(fdt_setprop_cell(fdt, s1, "prop-int", TEST_VALUE_1));
	OFF_CHECK(offset, fdt_add_subnode(fdt, s1, "subsubnode"));
	CHECK(fdt_setprop(fdt, offset, "compatible",
			  "subsubnode1\0subsubnode", 23));
	CHECK(fdt_setprop_cell(fdt, offset, "prop-int", TEST_VALUE_1));
	OFF_CHECK(offset, fdt_add_subnode(fdt, s1, "ss1"));

	OFF_CHECK(offset, fdt_add_subnode(fdt, 0, "subnode@2"));
	s2 = offset;
	CHECK(fdt_setprop_cell(fdt, s2, "linux,phandle", PHANDLE_1));
	CHECK(fdt_setprop_cell(fdt, s2, "prop-int", TEST_VALUE_2));
	OFF_CHECK(offset, fdt_add_subnode(fdt, s2, "subsubnode@0"));
	CHECK(fdt_setprop_cell(fdt, offset, "linux,phandle", PHANDLE_2));
	CHECK(fdt_setprop(fdt, offset, "compatible",
			  "subsubnode2\0subsubnode", 23));
	CHECK(fdt_setprop_cell(fdt, offset, "prop-int", TEST_VALUE_2));
	OFF_CHECK(offset, fdt_add_subnode(fdt, s2, "ss2"));

	CHECK(fdt_pack(fdt));

	save_blob("rw_tree1.test.dtb", fdt);

	PASS();
}
