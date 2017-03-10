/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for fdt_setprop_inplace()
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

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#include <libfdt.h>

#include "tests.h"
#include "testdata.h"

int main(int argc, char *argv[])
{
	void *fdt;
	const uint32_t *intp;
	const uint64_t *int64p;
	const char *strp;
	char *xstr;
	int xlen, i;
	int err;

	test_init(argc, argv);
	fdt = load_blob_arg(argc, argv);

	intp = check_getprop_cell(fdt, 0, "prop-int", TEST_VALUE_1);

	verbose_printf("Old int value was 0x%08x\n", *intp);
	err = fdt_setprop_inplace_cell(fdt, 0, "prop-int", ~TEST_VALUE_1);
	if (err)
		FAIL("Failed to set \"prop-int\" to 0x%08x: %s",
		     ~TEST_VALUE_1, fdt_strerror(err));
	intp = check_getprop_cell(fdt, 0, "prop-int", ~TEST_VALUE_1);
	verbose_printf("New int value is 0x%08x\n", *intp);

	strp = check_getprop(fdt, 0, "prop-str", strlen(TEST_STRING_1)+1,
			     TEST_STRING_1);


	int64p = check_getprop_64(fdt, 0, "prop-int64", TEST_VALUE64_1);

	verbose_printf("Old int64 value was 0x%016" PRIx64 "\n", *int64p);
	err = fdt_setprop_inplace_u64(fdt, 0, "prop-int64", ~TEST_VALUE64_1);
	if (err)
		FAIL("Failed to set \"prop-int64\" to 0x%016llx: %s",
		     ~TEST_VALUE64_1, fdt_strerror(err));
	int64p = check_getprop_64(fdt, 0, "prop-int64", ~TEST_VALUE64_1);
	verbose_printf("New int64 value is 0x%016" PRIx64 "\n", *int64p);

	strp = check_getprop(fdt, 0, "prop-str", strlen(TEST_STRING_1)+1,
			     TEST_STRING_1);

	verbose_printf("Old string value was \"%s\"\n", strp);
	xstr = strdup(strp);
	xlen = strlen(xstr);
	for (i = 0; i < xlen; i++)
		xstr[i] = toupper(xstr[i]);
	err = fdt_setprop_inplace(fdt, 0, "prop-str", xstr, xlen+1);
	if (err)
		FAIL("Failed to set \"prop-str\" to \"%s\": %s",
		     xstr, fdt_strerror(err));

	strp = check_getprop(fdt, 0, "prop-str", xlen+1, xstr);
	verbose_printf("New string value is \"%s\"\n", strp);

	err = fdt_setprop_inplace_namelen_partial(fdt, 0, "compatible",
						  strlen("compatible"), 4,
						  TEST_STRING_4_PARTIAL,
						  strlen(TEST_STRING_4_PARTIAL));
	if (err)
		FAIL("Failed to set \"compatible\": %s\n", fdt_strerror(err));

	check_getprop(fdt, 0, "compatible", strlen(TEST_STRING_4_RESULT) + 1,
		      TEST_STRING_4_RESULT);

	PASS();
}
