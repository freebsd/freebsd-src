/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for fdt_path_offset()
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
#include <stdint.h>

#include <libfdt.h>

#include "tests.h"
#include "testdata.h"

static int check_subnode(void *fdt, int parent, const char *name)
{
	int offset;
	const struct fdt_node_header *nh;
	uint32_t tag;

	verbose_printf("Checking subnode \"%s\" of %d...", name, parent);
	offset = fdt_subnode_offset(fdt, parent, name);
	verbose_printf("offset %d...", offset);
	if (offset < 0)
		FAIL("fdt_subnode_offset(\"%s\"): %s", name, fdt_strerror(offset));
	nh = fdt_offset_ptr(fdt, offset, sizeof(*nh));
	verbose_printf("pointer %p\n", nh);
	if (! nh)
		FAIL("NULL retrieving subnode \"%s\"", name);

	tag = fdt32_to_cpu(nh->tag);

	if (tag != FDT_BEGIN_NODE)
		FAIL("Incorrect tag 0x%08x on property \"%s\"", tag, name);
	if (!nodename_eq(nh->name, name))
		FAIL("Subnode name mismatch \"%s\" instead of \"%s\"",
		     nh->name, name);

	return offset;
}

int main(int argc, char *argv[])
{
	void *fdt;
	int root_offset;
	int subnode1_offset, subnode2_offset;
	int subnode1_offset_p, subnode2_offset_p;
	int subsubnode1_offset, subsubnode2_offset, subsubnode2_offset2;
	int subsubnode1_offset_p, subsubnode2_offset_p, subsubnode2_offset2_p;

	test_init(argc, argv);
	fdt = load_blob_arg(argc, argv);

	root_offset = fdt_path_offset(fdt, "/");
	if (root_offset < 0)
		FAIL("fdt_path_offset(\"/\") failed: %s",
		     fdt_strerror(root_offset));
	else if (root_offset != 0)
		FAIL("fdt_path_offset(\"/\") returns incorrect offset %d",
		     root_offset);
	subnode1_offset = check_subnode(fdt, 0, "subnode@1");
	subnode2_offset = check_subnode(fdt, 0, "subnode@2");

	subnode1_offset_p = fdt_path_offset(fdt, "/subnode@1");
	subnode2_offset_p = fdt_path_offset(fdt, "/subnode@2");

	if (subnode1_offset != subnode1_offset_p)
		FAIL("Mismatch between subnode_offset (%d) and path_offset (%d)",
		     subnode1_offset, subnode1_offset_p);

	if (subnode2_offset != subnode2_offset_p)
		FAIL("Mismatch between subnode_offset (%d) and path_offset (%d)",
		     subnode2_offset, subnode2_offset_p);

	subsubnode1_offset = check_subnode(fdt, subnode1_offset, "subsubnode");
	subsubnode2_offset = check_subnode(fdt, subnode2_offset, "subsubnode@0");
	subsubnode2_offset2 = check_subnode(fdt, subnode2_offset, "subsubnode");

	subsubnode1_offset_p = fdt_path_offset(fdt, "/subnode@1/subsubnode");
	subsubnode2_offset_p = fdt_path_offset(fdt, "/subnode@2/subsubnode@0");
	subsubnode2_offset2_p = fdt_path_offset(fdt, "/subnode@2/subsubnode");

	if (subsubnode1_offset != subsubnode1_offset_p)
		FAIL("Mismatch between subnode_offset (%d) and path_offset (%d)",
		     subsubnode1_offset, subsubnode1_offset_p);

	if (subsubnode2_offset != subsubnode2_offset_p)
		FAIL("Mismatch between subnode_offset (%d) and path_offset (%d)",
		     subsubnode2_offset, subsubnode2_offset_p);

	if (subsubnode2_offset2 != subsubnode2_offset2_p)
		FAIL("Mismatch between subnode_offset (%d) and path_offset (%d)",
		     subsubnode2_offset2, subsubnode2_offset2_p);

	PASS();
}
