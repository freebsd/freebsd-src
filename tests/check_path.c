/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for node existence
 * Copyright (C) 2016 Konsulko Inc.
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

#include <stdio.h>

#include <libfdt.h>

#include "tests.h"

#define CHECK(code) \
	{ \
		int err = (code); \
		if (err) \
			FAIL(#code ": %s", fdt_strerror(err)); \
	}

/* 4k ought to be enough for anybody */
#define FDT_COPY_SIZE	(4 * 1024)

static void *open_dt(char *path)
{
	void *dt, *copy;

	dt = load_blob(path);
	copy = xmalloc(FDT_COPY_SIZE);

	/*
	 * Resize our DTs to 4k so that we have room to operate on
	 */
	CHECK(fdt_open_into(dt, copy, FDT_COPY_SIZE));

	return copy;
}

int main(int argc, char *argv[])
{
	void *fdt_base;
	int fail_config, exists, check_exists;

	test_init(argc, argv);
	fail_config = 0;

	if (argc != 4)
		fail_config = 1;

	if (!fail_config) {
		if (!strcmp(argv[2], "exists"))
			check_exists = 1;
		else if (!strcmp(argv[2], "not-exists"))
			check_exists = 0;
		else
			fail_config = 1;
	}

	if (fail_config)
		CONFIG("Usage: %s <base dtb> <[exists|not-exists]> <node-path>", argv[0]);

	fdt_base = open_dt(argv[1]);

	exists = fdt_path_offset(fdt_base, argv[3]) >= 0;

	if (exists == check_exists)
		PASS();
	else
		FAIL();
}
