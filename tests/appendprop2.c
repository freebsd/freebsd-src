/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for fdt_appendprop()
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

#define SPACE		65536

#define CHECK(code) \
	{ \
		err = (code); \
		if (err) \
			FAIL(#code ": %s", fdt_strerror(err)); \
	}

int main(int argc, char *argv[])
{
	void *fdt, *buf;
	int err;
	uint8_t bytes[] = {0x00, 0x01, 0x02, 0x03, 0x04};

	test_init(argc, argv);
	fdt = load_blob_arg(argc, argv);

	buf = xmalloc(SPACE);
	CHECK(fdt_open_into(fdt, buf, SPACE));
	fdt = buf;

	CHECK(fdt_appendprop(fdt, 0, "prop-bytes", bytes, sizeof(bytes)));
	CHECK(fdt_appendprop_cell(fdt, 0, "prop-int", TEST_VALUE_2));
	CHECK(fdt_appendprop_u64(fdt, 0, "prop-int64", TEST_VALUE64_1));
	CHECK(fdt_appendprop_string(fdt, 0, "prop-str", TEST_STRING_2));

	CHECK(fdt_pack(fdt));

	save_blob("appendprop2.test.dtb", fdt);

	PASS();
}
