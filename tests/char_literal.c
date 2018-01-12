/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for character literals in dtc
 * Copyright (C) 2006 David Gibson, IBM Corporation.
 * Copyright (C) 2011 The Chromium Authors. All rights reserved.
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
	fdt32_t expected_cells[5];

	expected_cells[0] = cpu_to_fdt32((unsigned char)TEST_CHAR1);
	expected_cells[1] = cpu_to_fdt32((unsigned char)TEST_CHAR2);
	expected_cells[2] = cpu_to_fdt32((unsigned char)TEST_CHAR3);
	expected_cells[3] = cpu_to_fdt32((unsigned char)TEST_CHAR4);
	expected_cells[4] = cpu_to_fdt32((unsigned char)TEST_CHAR5);

	test_init(argc, argv);
	fdt = load_blob_arg(argc, argv);

	check_getprop(fdt, 0, "char-literal-cells",
		      sizeof(expected_cells), expected_cells);

	PASS();
}
