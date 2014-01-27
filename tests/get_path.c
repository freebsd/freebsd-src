/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for fdt_get_path()
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

#define POISON	('\xff')

static void check_path_buf(void *fdt, const char *path, int pathlen, int buflen)
{
	int offset;
	char buf[buflen+1];
	int len;

	offset = fdt_path_offset(fdt, path);
	if (offset < 0)
		FAIL("Couldn't find path \"%s\": %s", path, fdt_strerror(offset));

	memset(buf, POISON, sizeof(buf)); /* poison the buffer */

	len = fdt_get_path(fdt, offset, buf, buflen);
	verbose_printf("get_path() %s -> %d -> %s\n", path, offset, buf);

	if (buflen <= pathlen) {
		if (len != -FDT_ERR_NOSPACE)
			FAIL("fdt_get_path([%d bytes]) returns %d with "
			     "insufficient buffer space", buflen, len);
	} else {
		if (len < 0)
			FAIL("fdt_get_path([%d bytes]): %s", buflen,
			     fdt_strerror(len));
		if (len != 0)
			FAIL("fdt_get_path([%d bytes]) returns %d "
			     "instead of 0", buflen, len);
		if (strcmp(buf, path) != 0)
			FAIL("fdt_get_path([%d bytes]) returns \"%s\" "
			     "instead of \"%s\"", buflen, buf, path);
	}

	if (buf[buflen] != POISON)
		FAIL("fdt_get_path([%d bytes]) overran buffer", buflen);
}

static void check_path(void *fdt, const char *path)
{
	int pathlen = strlen(path);

	check_path_buf(fdt, path, pathlen, 1024);
	check_path_buf(fdt, path, pathlen, pathlen+1);
	check_path_buf(fdt, path, pathlen, pathlen);
	check_path_buf(fdt, path, pathlen, 0);
	check_path_buf(fdt, path, pathlen, 2);
}

int main(int argc, char *argv[])
{
	void *fdt;

	test_init(argc, argv);
	fdt = load_blob_arg(argc, argv);

	check_path(fdt, "/");
	check_path(fdt, "/subnode@1");
	check_path(fdt, "/subnode@2");
	check_path(fdt, "/subnode@1/subsubnode");
	check_path(fdt, "/subnode@2/subsubnode@0");

	PASS();
}
