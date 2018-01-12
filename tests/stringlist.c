/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for string handling
 * Copyright (C) 2015 NVIDIA Corporation
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

static void check_expected_failure(const void *fdt, const char *path,
				   const char *property)
{
	int offset, err;

	offset = fdt_path_offset(fdt, "/");
	if (offset < 0)
		FAIL("Couldn't find path %s", path);

	err = fdt_stringlist_count(fdt, offset, "#address-cells");
	if (err != -FDT_ERR_BADVALUE)
		FAIL("unexpectedly succeeded in parsing #address-cells\n");

	err = fdt_stringlist_search(fdt, offset, "#address-cells", "foo");
	if (err != -FDT_ERR_BADVALUE)
		FAIL("found string in #address-cells: %d\n", err);

	/*
	 * Note that the #address-cells property contains a small 32-bit
	 * unsigned integer, hence some bytes will be zero, and searching for
	 * the empty string will succeed.
	 *
	 * The reason for this oddity is that the function will exit when the
	 * first occurrence of the string is found, but in order to determine
	 * that the property does not contain a valid string list it would
	 * need to process the whole value.
	 */
	err = fdt_stringlist_search(fdt, offset, "#address-cells", "");
	if (err != 0)
		FAIL("empty string not found in #address-cells: %d\n", err);

	/*
	 * fdt_getprop_string() can successfully extract strings from
	 * non-string properties. This is because it doesn't
	 * necessarily parse the whole property value, which would be
	 * necessary for it to determine if a valid string or string
	 * list is present.
	 */
}

static void check_string_count(const void *fdt, const char *path,
			       const char *property, int count)
{
	int offset, err;

	offset = fdt_path_offset(fdt, path);
	if (offset < 0)
		FAIL("Couldn't find path %s", path);

	err = fdt_stringlist_count(fdt, offset, property);
	if (err < 0)
		FAIL("Couldn't count strings in property %s of node %s: %d\n",
		     property, path, err);

	if (err != count)
		FAIL("String count for property %s of node %s is %d instead of %d\n",
		     path, property, err, count);
}

static void check_string_index(const void *fdt, const char *path,
			       const char *property, const char *string,
			       int idx)
{
	int offset, err;

	offset = fdt_path_offset(fdt, path);
	if (offset < 0)
		FAIL("Couldn't find path %s", path);

	err = fdt_stringlist_search(fdt, offset, property, string);

	if (err != idx)
		FAIL("Index of %s in property %s of node %s is %d, expected %d\n",
		     string, property, path, err, idx);
}

static void check_string(const void *fdt, const char *path,
			 const char *property, int idx,
			 const char *string)
{
	const char *result;
	int offset, len;

	offset = fdt_path_offset(fdt, path);
	if (offset < 0)
		FAIL("Couldn't find path %s", path);

	result = fdt_stringlist_get(fdt, offset, property, idx, &len);
	if (!result)
		FAIL("Couldn't extract string %d from property %s of node %s: %d\n",
		     idx, property, path, len);

	if (strcmp(string, result) != 0)
		FAIL("String %d in property %s of node %s is %s, expected %s\n",
		     idx, property, path, result, string);
}

int main(int argc, char *argv[])
{
	void *fdt;

	if (argc != 2)
		CONFIG("Usage: %s <dtb file>\n", argv[0]);

	test_init(argc, argv);
	fdt = load_blob(argv[1]);

	check_expected_failure(fdt, "/", "#address-cells");
	check_expected_failure(fdt, "/", "#size-cells");

	check_string_count(fdt, "/", "compatible", 1);
	check_string_count(fdt, "/device", "compatible", 2);
	check_string_count(fdt, "/device", "big-endian", 0);

	check_string_index(fdt, "/", "compatible", "test-strings", 0);
	check_string_index(fdt, "/device", "compatible", "foo", 0);
	check_string_index(fdt, "/device", "compatible", "bar", 1);
	check_string_index(fdt, "/device", "big-endian", "baz", -1);

	check_string(fdt, "/", "compatible", 0, "test-strings");
	check_string(fdt, "/device", "compatible", 0, "foo");
	check_string(fdt, "/device", "compatible", 1, "bar");

	PASS();
}
