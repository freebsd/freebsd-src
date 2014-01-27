/*
 * libfdt - Flat Device Tree manipulation
 *	Tests if an asm tree built into a shared object matches a given dtb
 * Copyright (C) 2008 David Gibson, IBM Corporation.
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
#include <errno.h>

#include <dlfcn.h>

#include <libfdt.h>

#include "tests.h"
#include "testdata.h"

int main(int argc, char *argv[])
{
	void *sohandle;
	void *fdt;
	int err;

	test_init(argc, argv);
	if (argc != 3)
		CONFIG("Usage: %s <so file> <dtb file>", argv[0]);

	sohandle = dlopen(argv[1], RTLD_NOW);
	if (!sohandle)
		FAIL("Couldn't dlopen() %s", argv[1]);

	fdt = dlsym(sohandle, "dt_blob_start");
	if (!fdt)
		FAIL("Couldn't locate \"dt_blob_start\" symbol in %s",
		     argv[1]);

	err = fdt_check_header(fdt);
	if (err != 0)
		FAIL("%s contains invalid tree: %s", argv[1],
		     fdt_strerror(err));

	save_blob(argv[2], fdt);

	PASS();
}
