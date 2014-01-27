/*
 * libfdt - Flat Device Tree manipulation
 *	Basic testcase for read-only access
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

int main(int argc, char *argv[])
{
	void *fdt;
	const struct fdt_node_header *nh;

	test_init(argc, argv);
	fdt = load_blob_arg(argc, argv);

	nh = fdt_offset_ptr(fdt, 0, sizeof(*nh));

	if (! nh)
		FAIL("NULL retrieving root node");

	if (fdt32_to_cpu(nh->tag) != FDT_BEGIN_NODE)
		FAIL("Wrong tag on root node");

	if (strlen(nh->name) != 0)
		FAIL("Wrong name for root node, \"%s\" instead of empty",
		     nh->name);

	PASS();
}
