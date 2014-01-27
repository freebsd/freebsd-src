/*
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

#include <libfdt.h>

#include "tests.h"
#include "testdata.h"

int main(int argc, char *argv[])
{
	void *fdt;
	uint32_t cpuid;

	test_init(argc, argv);

	if (argc != 3)
		CONFIG("Usage: %s <dtb file> <cpuid>", argv[0]);

	fdt = load_blob(argv[1]);
	cpuid = strtoul(argv[2], NULL, 0);

	if (fdt_boot_cpuid_phys(fdt) != cpuid)
		FAIL("Incorrect boot_cpuid_phys (0x%x instead of 0x%x)",
		     fdt_boot_cpuid_phys(fdt), cpuid);

	PASS();
}
