/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for string escapes in dtc
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
#include <errno.h>

#include <libfdt.h>

#include "tests.h"
#include "testdata.h"

#define CHUNKSIZE	1024

static char *load_file(const char *name, int *len)
{
	FILE *f;
	char *buf = NULL;
	int bufsize = 0, n;

	*len = 0;

	f = fopen(name, "r");
	if (!f)
		FAIL("Couldn't open \"%s\": %s", name, strerror(errno));

	while (!feof(f)) {
		if (bufsize < (*len + CHUNKSIZE)) {
			buf = xrealloc(buf, *len + CHUNKSIZE);
			bufsize = *len + CHUNKSIZE;
		}

		n = fread(buf + *len, 1, CHUNKSIZE, f);
		if (ferror(f))
			FAIL("Error reading \"%s\": %s", name, strerror(errno));
		*len += n;
	}

	return buf;
}

int main(int argc, char *argv[])
{
	void *fdt;
	char *incbin;
	int len;

	test_init(argc, argv);

	incbin = load_file("incbin.bin", &len);
	fdt = load_blob_arg(argc, argv);

	check_getprop(fdt, 0, "incbin", len, incbin);
	check_getprop(fdt, 0, "incbin-partial", 17, incbin + 13);

	PASS();
}
