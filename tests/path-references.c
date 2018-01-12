/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for string references in dtc
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

static void check_ref(const void *fdt, int node, const char *checkpath)
{
	const char *p;
	int len;

	p = fdt_getprop(fdt, node, "ref", &len);
	if (!p)
		FAIL("fdt_getprop(%d, \"ref\"): %s", node, fdt_strerror(len));
	if (!streq(p, checkpath))
		FAIL("'ref' in node at %d has value \"%s\" instead of \"%s\"",
		     node, p, checkpath);

	p = fdt_getprop(fdt, node, "lref", &len);
	if (!p)
		FAIL("fdt_getprop(%d, \"lref\"): %s", node, fdt_strerror(len));
	if (!streq(p, checkpath))
		FAIL("'lref' in node at %d has value \"%s\" instead of \"%s\"",
		     node, p, checkpath);
}

static void check_rref(const void *fdt)
{
	const char *p;
	int len;

	/* Check reference to root node */
	p = fdt_getprop(fdt, 0, "rref", &len);
	if (!p)
		FAIL("fdt_getprop(0, \"rref\"): %s", fdt_strerror(len));
	if (!streq(p, "/"))
		FAIL("'rref' in root node has value \"%s\" instead of \"/\"",
		     p);
}

int main(int argc, char *argv[])
{
	void *fdt;
	const char *p;
	int len, multilen;
	int n1, n2, n3, n4;

	test_init(argc, argv);
	fdt = load_blob_arg(argc, argv);

	n1 = fdt_path_offset(fdt, "/node1");
	if (n1 < 0)
		FAIL("fdt_path_offset(/node1): %s", fdt_strerror(n1));
	n2 = fdt_path_offset(fdt, "/node2");
	if (n2 < 0)
		FAIL("fdt_path_offset(/node2): %s", fdt_strerror(n2));

	check_ref(fdt, n1, "/node2");
	check_ref(fdt, n2, "/node1");

	/* Check multiple reference */
	multilen = strlen("/node1") + strlen("/node2") + 2;
	p = fdt_getprop(fdt, 0, "multiref", &len);
	if (!p)
		FAIL("fdt_getprop(0, \"multiref\"): %s", fdt_strerror(len));
	if (len != multilen)
		FAIL("multiref has wrong length, %d instead of %d",
		     len, multilen);
	if ((!streq(p, "/node1") || !streq(p + strlen("/node1") + 1, "/node2")))
		FAIL("multiref has wrong value");

	/* Check reference to nested nodes with common prefix */
	n3 = fdt_path_offset(fdt, "/foo/baz");
	if (n3 < 0)
		FAIL("fdt_path_offset(/foo/baz): %s", fdt_strerror(n3));
	n4 = fdt_path_offset(fdt, "/foobar/baz");
	if (n4 < 0)
		FAIL("fdt_path_offset(/foobar/baz): %s", fdt_strerror(n4));
	check_ref(fdt, n3, "/foobar/baz");
	check_ref(fdt, n4, "/foo/baz");

	check_rref(fdt);

	PASS();
}
