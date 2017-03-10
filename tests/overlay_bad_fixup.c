/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for DT overlays()
 * Copyright (C) 2016 Free Electrons
 * Copyright (C) 2016 NextThing Co.
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

#define CHECK(code, expected)					\
	{							\
		err = (code);					\
		if (err != expected)				\
			FAIL(#code ": %s", fdt_strerror(err));	\
	}

/* 4k ought to be enough for anybody */
#define FDT_COPY_SIZE	(4 * 1024)

static void *open_dt(char *path)
{
	void *dt, *copy;
	int err;

	dt = load_blob(path);
	copy = xmalloc(FDT_COPY_SIZE);

	/*
	 * Resize our DTs to 4k so that we have room to operate on
	 */
	CHECK(fdt_open_into(dt, copy, FDT_COPY_SIZE), 0);

	return copy;
}

int main(int argc, char *argv[])
{
	void *fdt_base, *fdt_overlay;
	int err;

	test_init(argc, argv);
	if (argc != 3)
		CONFIG("Usage: %s <base dtb> <overlay dtb>", argv[0]);

	fdt_base = open_dt(argv[1]);
	fdt_overlay = open_dt(argv[2]);

	/* Apply the overlay */
	CHECK(fdt_overlay_apply(fdt_base, fdt_overlay), -FDT_ERR_BADOVERLAY);

	PASS();
}
