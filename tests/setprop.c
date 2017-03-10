/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for fdt_setprop()
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
#define NEW_STRING	"here is quite a long test string, blah blah blah"

int main(int argc, char *argv[])
{
	void *fdt;
	void *buf;
	const uint32_t *intp;
	const char *strp;
	int err;

	test_init(argc, argv);
	fdt = load_blob_arg(argc, argv);

	buf = xmalloc(SPACE);

	err = fdt_open_into(fdt, buf, SPACE);
	if (err)
		FAIL("fdt_open_into(): %s", fdt_strerror(err));

	fdt = buf;

	intp = check_getprop_cell(fdt, 0, "prop-int", TEST_VALUE_1);

	verbose_printf("Old int value was 0x%08x\n", *intp);
	err = fdt_setprop_string(fdt, 0, "prop-int", NEW_STRING);
	if (err)
		FAIL("Failed to set \"prop-int\" to \"%s\": %s",
		     NEW_STRING, fdt_strerror(err));

	strp = check_getprop_string(fdt, 0, "prop-int", NEW_STRING);
	verbose_printf("New value is \"%s\"\n", strp);

	strp = check_getprop(fdt, 0, "prop-str", strlen(TEST_STRING_1)+1,
			     TEST_STRING_1);

	verbose_printf("Old string value was \"%s\"\n", strp);
	err = fdt_setprop_empty(fdt, 0, "prop-str");
	if (err)
		FAIL("Failed to empty \"prop-str\": %s",
		     fdt_strerror(err));

	check_getprop(fdt, 0, "prop-str", 0, NULL);

	err = fdt_setprop_u32(fdt, 0, "prop-u32", TEST_VALUE_2);
	if (err)
		FAIL("Failed to set \"prop-u32\" to 0x%08x: %s",
		     TEST_VALUE_2, fdt_strerror(err));
	check_getprop_cell(fdt, 0, "prop-u32", TEST_VALUE_2);

	err = fdt_setprop_cell(fdt, 0, "prop-cell", TEST_VALUE_2);
	if (err)
		FAIL("Failed to set \"prop-cell\" to 0x%08x: %s",
		     TEST_VALUE_2, fdt_strerror(err));
	check_getprop_cell(fdt, 0, "prop-cell", TEST_VALUE_2);

	err = fdt_setprop_u64(fdt, 0, "prop-u64", TEST_VALUE64_1);
	if (err)
		FAIL("Failed to set \"prop-u64\" to 0x%016llx: %s",
		     TEST_VALUE64_1, fdt_strerror(err));
	check_getprop_64(fdt, 0, "prop-u64", TEST_VALUE64_1);
	
	PASS();
}
