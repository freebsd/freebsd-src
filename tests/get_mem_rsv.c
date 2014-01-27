/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for fdt_get_mem_rsv() and fdt_num_mem_rsv()
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
	int rc;

	test_init(argc, argv);
	fdt = load_blob_arg(argc, argv);

	rc = fdt_num_mem_rsv(fdt);
	if (rc < 0)
		FAIL("fdt_num_mem_rsv(): %s", fdt_strerror(rc));
	if (rc != 2)
		FAIL("fdt_num_mem_rsv() returned %d instead of 2", rc);

	check_mem_rsv(fdt, 0, TEST_ADDR_1, TEST_SIZE_1);
	check_mem_rsv(fdt, 1, TEST_ADDR_2, TEST_SIZE_2);
	PASS();
}
