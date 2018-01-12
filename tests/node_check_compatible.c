/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for fdt_node_check_compatible()
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

static void check_compatible(const void *fdt, const char *path,
			     const char *compat)
{
	int offset, err;

	offset = fdt_path_offset(fdt, path);
	if (offset < 0)
		FAIL("fdt_path_offset(%s): %s", path, fdt_strerror(offset));

	err = fdt_node_check_compatible(fdt, offset, compat);
	if (err < 0)
		FAIL("fdt_node_check_compatible(%s): %s", path,
		     fdt_strerror(err));
	if (err != 0)
		FAIL("%s is not compatible with \"%s\"", path, compat);
}

static void check_not_compatible(const void *fdt, const char *path,
				 const char *compat)
{
	int offset, err;

	offset = fdt_path_offset(fdt, path);
	if (offset < 0)
		FAIL("fdt_path_offset(%s): %s", path, fdt_strerror(offset));

	err = fdt_node_check_compatible(fdt, offset, compat);
	if (err < 0)
		FAIL("fdt_node_check_compatible(%s): %s", path,
		     fdt_strerror(err));
	if (err == 0)
		FAIL("%s is incorrectly compatible with \"%s\"", path, compat);
}

int main(int argc, char *argv[])
{
	void *fdt;

	test_init(argc, argv);
	fdt = load_blob_arg(argc, argv);

	check_compatible(fdt, "/", "test_tree1");
	check_compatible(fdt, "/subnode@1/subsubnode", "subsubnode1");
	check_compatible(fdt, "/subnode@1/subsubnode", "subsubnode");
	check_not_compatible(fdt, "/subnode@1/subsubnode", "subsubnode2");
	check_compatible(fdt, "/subnode@2/subsubnode", "subsubnode2");
	check_compatible(fdt, "/subnode@2/subsubnode", "subsubnode");
	check_not_compatible(fdt, "/subnode@2/subsubnode", "subsubnode1");

	PASS();
}
