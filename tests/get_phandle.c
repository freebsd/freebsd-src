/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for fdt_get_phandle()
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

static void check_phandle(void *fdt, const char *path, uint32_t checkhandle)
{
	int offset;
	uint32_t phandle;

	offset = fdt_path_offset(fdt, path);
	if (offset < 0)
		FAIL("Couldn't find %s", path);

	phandle = fdt_get_phandle(fdt, offset);
	if (phandle != checkhandle)
		FAIL("fdt_get_phandle(%s) returned 0x%x instead of 0x%x\n",
		     path, phandle, checkhandle);
}

int main(int argc, char *argv[])
{
	void *fdt;

	test_init(argc, argv);
	fdt = load_blob_arg(argc, argv);

	check_phandle(fdt, "/", 0);
	check_phandle(fdt, "/subnode@2", PHANDLE_1);
	check_phandle(fdt, "/subnode@2/subsubnode@0", PHANDLE_2);

	PASS();
}
