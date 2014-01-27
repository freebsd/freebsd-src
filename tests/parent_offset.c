/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for fdt_parent_offset()
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

static int path_parent_len(const char *path)
{
	const char *p = strrchr(path, '/');

	if (!p)
		TEST_BUG();
	if (p == path)
		return 1;
	else
		return p - path;
}

static void check_path(struct fdt_header *fdt, const char *path)
{
	char *parentpath;
	int nodeoffset, parentoffset, parentpathoffset, pathparentlen;

	pathparentlen = path_parent_len(path);
	parentpath = alloca(pathparentlen + 1);
	strncpy(parentpath, path, pathparentlen);
	parentpath[pathparentlen] = '\0';

	verbose_printf("Path: \"%s\"\tParent: \"%s\"\n", path, parentpath);

	nodeoffset = fdt_path_offset(fdt, path);
	if (nodeoffset < 0)
		FAIL("fdt_path_offset(%s): %s", path, fdt_strerror(nodeoffset));

	parentpathoffset = fdt_path_offset(fdt, parentpath);
	if (parentpathoffset < 0)
		FAIL("fdt_path_offset(%s): %s", parentpath,
		     fdt_strerror(parentpathoffset));

	parentoffset = fdt_parent_offset(fdt, nodeoffset);
	if (parentoffset < 0)
		FAIL("fdt_parent_offset(): %s", fdt_strerror(parentoffset));

	if (parentoffset != parentpathoffset)
		FAIL("fdt_parent_offset() returns %d instead of %d",
		     parentoffset, parentpathoffset);
}

int main(int argc, char *argv[])
{
	void *fdt;
	int err;

	test_init(argc, argv);
	fdt = load_blob_arg(argc, argv);

	check_path(fdt, "/subnode@1");
	check_path(fdt, "/subnode@2");
	check_path(fdt, "/subnode@1/subsubnode");
	check_path(fdt, "/subnode@2/subsubnode@0");
	err = fdt_parent_offset(fdt, 0);
	if (err != -FDT_ERR_NOTFOUND)
		FAIL("fdt_parent_offset(/) returns %d instead of "
		     "-FDT_ERR_NOTFOUND", err);

	PASS();
}
