/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for fdt_supernode_atdepth_offset()
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

static int path_depth(const char *path)
{
	const char *p;
	int depth = 0;

	if (path[0] != '/')
		TEST_BUG();

	if (strcmp(path, "/") == 0)
		return 0;
	for (p = path; *p; p++)
		if (*p == '/')
			depth++;

	/* Special case for path == "/" */
	if (p == (path + 1))
		return 0;
	else
		return depth;
}

static int path_prefix(const char *path, int depth)
{
	const char *p;
	int i;

	if (path[0] != '/')
		TEST_BUG();

	if (depth == 0)
		return 1;

	p = path;
	for (i = 0; i < depth; i++)
		p = p+1 + strcspn(p+1, "/");

	return p - path;
}

static void check_supernode_atdepth(struct fdt_header *fdt, const char *path,
			     int depth)
{
	int pdepth = path_depth(path);
	char *superpath;
	int nodeoffset, supernodeoffset, superpathoffset, pathprefixlen;
	int nodedepth;

	pathprefixlen = path_prefix(path, depth);
	superpath = alloca(pathprefixlen + 1);
	strncpy(superpath, path, pathprefixlen);
	superpath[pathprefixlen] = '\0';

	verbose_printf("Path %s (%d), depth %d, supernode is %s\n",
		       path, pdepth, depth, superpath);

	nodeoffset = fdt_path_offset(fdt, path);
	if (nodeoffset < 0)
		FAIL("fdt_path_offset(%s): %s", path, fdt_strerror(nodeoffset));
	superpathoffset = fdt_path_offset(fdt, superpath);
	if (superpathoffset < 0)
		FAIL("fdt_path_offset(%s): %s", superpath,
		     fdt_strerror(superpathoffset));

	supernodeoffset = fdt_supernode_atdepth_offset(fdt, nodeoffset,
						       depth, &nodedepth);
	if (supernodeoffset < 0)
		FAIL("fdt_supernode_atdepth_offset(): %s",
		     fdt_strerror(supernodeoffset));

	if (supernodeoffset != superpathoffset)
		FAIL("fdt_supernode_atdepth_offset() returns %d instead of %d",
		     supernodeoffset, superpathoffset);

	if (nodedepth != pdepth)
		FAIL("fdt_supernode_atdept_offset() returns node depth %d "
		     "instead of %d", nodedepth, pdepth);
}

static void check_supernode_overdepth(struct fdt_header *fdt, const char *path)
{
	int pdepth = path_depth(path);
	int nodeoffset, err;

	nodeoffset = fdt_path_offset(fdt, path);
	if (nodeoffset < 0)
		FAIL("fdt_path_offset(%s): %s", path, fdt_strerror(nodeoffset));

	err = fdt_supernode_atdepth_offset(fdt, nodeoffset, pdepth + 1, NULL);
	if (err != -FDT_ERR_NOTFOUND)
		FAIL("fdt_supernode_atdept_offset(%s, %d) returns %d instead "
		     "of FDT_ERR_NOTFOUND", path, pdepth+1, err);
}

static void check_path(struct fdt_header *fdt, const char *path)
{
	int i;

	for (i = 0; i <= path_depth(path); i++)
		check_supernode_atdepth(fdt, path, i);
	check_supernode_overdepth(fdt, path);
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
