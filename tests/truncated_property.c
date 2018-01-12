/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for misbehaviour on a truncated property
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
	void *fdt = &truncated_property;
	const void *prop;
	int len;

	test_init(argc, argv);

	prop = fdt_getprop(fdt, 0, "truncated", &len);
	if (prop)
		FAIL("fdt_getprop() succeeded on truncated property");
	if (len != -FDT_ERR_BADSTRUCTURE)
		FAIL("fdt_getprop() failed with \"%s\" instead of \"%s\"",
		     fdt_strerror(len), fdt_strerror(-FDT_ERR_BADSTRUCTURE));

	PASS();
}
