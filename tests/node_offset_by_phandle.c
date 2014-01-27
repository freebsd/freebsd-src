/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for fdt_node_offset_by_phandle()
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
#include <stdarg.h>

#include <libfdt.h>

#include "tests.h"
#include "testdata.h"

static void check_search(void *fdt, uint32_t phandle, int target)
{
	int offset;

	offset = fdt_node_offset_by_phandle(fdt, phandle);

	if (offset != target)
		FAIL("fdt_node_offset_by_phandle(0x%x) returns %d "
		     "instead of %d", phandle, offset, target);
}

int main(int argc, char *argv[])
{
	void *fdt;
	int subnode2_offset, subsubnode2_offset;

	test_init(argc, argv);
	fdt = load_blob_arg(argc, argv);

	subnode2_offset = fdt_path_offset(fdt, "/subnode@2");
	subsubnode2_offset = fdt_path_offset(fdt, "/subnode@2/subsubnode@0");

	if ((subnode2_offset < 0) || (subsubnode2_offset < 0))
		FAIL("Can't find required nodes");

	check_search(fdt, PHANDLE_1, subnode2_offset);
	check_search(fdt, PHANDLE_2, subsubnode2_offset);
	check_search(fdt, ~PHANDLE_1, -FDT_ERR_NOTFOUND);
	check_search(fdt, 0, -FDT_ERR_BADPHANDLE);
	check_search(fdt, -1, -FDT_ERR_BADPHANDLE);

	PASS();
}
