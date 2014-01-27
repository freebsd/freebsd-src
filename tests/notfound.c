/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for behaviour on searching for a non-existent node
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

static void check_error(const char *s, int err)
{
	if (err != -FDT_ERR_NOTFOUND)
		FAIL("%s return error %s instead of -FDT_ERR_NOTFOUND", s,
		     fdt_strerror(err));
}

int main(int argc, char *argv[])
{
	void *fdt;
	int offset;
	int subnode1_offset;
	int lenerr;

	test_init(argc, argv);
	fdt = load_blob_arg(argc, argv);

	fdt_get_property(fdt, 0, "nonexistant-property", &lenerr);
	check_error("fdt_get_property(\"nonexistant-property\")", lenerr);

	fdt_getprop(fdt, 0, "nonexistant-property", &lenerr);
	check_error("fdt_getprop(\"nonexistant-property\"", lenerr);

	subnode1_offset = fdt_subnode_offset(fdt, 0, "subnode@1");
	if (subnode1_offset < 0)
		FAIL("Couldn't find subnode1: %s", fdt_strerror(subnode1_offset));

	fdt_getprop(fdt, subnode1_offset, "prop-str", &lenerr);
	check_error("fdt_getprop(\"prop-str\")", lenerr);

	offset = fdt_subnode_offset(fdt, 0, "nonexistant-subnode");
	check_error("fdt_subnode_offset(\"nonexistant-subnode\")", offset);

	offset = fdt_subnode_offset(fdt, 0, "subsubnode");
	check_error("fdt_subnode_offset(\"subsubnode\")", offset);

	offset = fdt_path_offset(fdt, "/nonexistant-subnode");
	check_error("fdt_path_offset(\"/nonexistant-subnode\")", offset);

	PASS();
}
