/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for variable sized cells in dtc
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

static void check_compare_properties(void *fdt,
				     char const *name_one,
				     char const *name_two)
{
	const void *propval;
	int proplen;

	propval = fdt_getprop(fdt, 0, name_one, &proplen);

	if (!propval)
		FAIL("fdt_getprop(\"%s\"): %s",
		     name_one,
		     fdt_strerror(proplen));

	check_getprop(fdt, 0, name_two, proplen, propval);
}

int main(int argc, char *argv[])
{
	void *fdt;
	uint8_t expected_8[6] = {TEST_CHAR1,
				 TEST_CHAR2,
				 TEST_CHAR3,
				 TEST_CHAR4,
				 TEST_CHAR5,
				 TEST_VALUE_1 >> 24};
	fdt16_t expected_16[6];
	fdt32_t expected_32[6];
	fdt64_t expected_64[6];
	int i;

	for (i = 0; i < 5; ++i) {
		expected_16[i] = cpu_to_fdt16(expected_8[i]);
		expected_32[i] = cpu_to_fdt32(expected_8[i]);
		expected_64[i] = cpu_to_fdt64(expected_8[i]);
	}

	expected_16[5] = cpu_to_fdt16(TEST_VALUE_1 >> 16);
	expected_32[5] = cpu_to_fdt32(TEST_VALUE_1);
	expected_64[5] = cpu_to_fdt64(TEST_ADDR_1);

	test_init(argc, argv);
	fdt = load_blob_arg(argc, argv);

	check_getprop(fdt, 0, "cells-8b", sizeof(expected_8), expected_8);
	check_getprop(fdt, 0, "cells-16b", sizeof(expected_16), expected_16);
	check_getprop(fdt, 0, "cells-32b", sizeof(expected_32), expected_32);
	check_getprop(fdt, 0, "cells-64b", sizeof(expected_64), expected_64);

	check_compare_properties(fdt, "cells-one-16b", "cells-one-32b");

	PASS();
}
