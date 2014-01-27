/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase for phandle references in dtc
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

static void check_ref(const void *fdt, int node, uint32_t checkref)
{
	const uint32_t *p;
	uint32_t ref;
	int len;

	p = fdt_getprop(fdt, node, "ref", &len);
	if (!p)
		FAIL("fdt_getprop(%d, \"ref\"): %s", node, fdt_strerror(len));
	if (len != sizeof(*p))
		FAIL("'ref' in node at %d has wrong size (%d instead of %zd)",
		     node, len, sizeof(*p));
	ref = fdt32_to_cpu(*p);
	if (ref != checkref)
		FAIL("'ref' in node at %d has value 0x%x instead of 0x%x",
		     node, ref, checkref);

	p = fdt_getprop(fdt, node, "lref", &len);
	if (!p)
		FAIL("fdt_getprop(%d, \"lref\"): %s", node, fdt_strerror(len));
	if (len != sizeof(*p))
		FAIL("'lref' in node at %d has wrong size (%d instead of %zd)",
		     node, len, sizeof(*p));
	ref = fdt32_to_cpu(*p);
	if (ref != checkref)
		FAIL("'lref' in node at %d has value 0x%x instead of 0x%x",
		     node, ref, checkref);
}

int main(int argc, char *argv[])
{
	void *fdt;
	int n1, n2, n3, n4, n5;
	uint32_t h1, h2, h4, h5;

	test_init(argc, argv);
	fdt = load_blob_arg(argc, argv);

	n1 = fdt_path_offset(fdt, "/node1");
	if (n1 < 0)
		FAIL("fdt_path_offset(/node1): %s", fdt_strerror(n1));
	n2 = fdt_path_offset(fdt, "/node2");
	if (n2 < 0)
		FAIL("fdt_path_offset(/node2): %s", fdt_strerror(n2));
	n3 = fdt_path_offset(fdt, "/node3");
	if (n3 < 0)
		FAIL("fdt_path_offset(/node3): %s", fdt_strerror(n3));
	n4 = fdt_path_offset(fdt, "/node4");
	if (n4 < 0)
		FAIL("fdt_path_offset(/node4): %s", fdt_strerror(n4));
	n5 = fdt_path_offset(fdt, "/node5");
	if (n5 < 0)
		FAIL("fdt_path_offset(/node5): %s", fdt_strerror(n5));

	h1 = fdt_get_phandle(fdt, n1);
	h2 = fdt_get_phandle(fdt, n2);
	h4 = fdt_get_phandle(fdt, n4);
	h5 = fdt_get_phandle(fdt, n5);

	if (h1 != 0x2000)
		FAIL("/node1 has wrong phandle, 0x%x instead of 0x%x",
		     h1, 0x2000);
	if (h2 != 0x1)
		FAIL("/node2 has wrong phandle, 0x%x instead of 0x%x",
		     h2, 0x1);
	if ((h4 == 0x2000) || (h4 == 0x1) || (h4 == 0))
		FAIL("/node4 has bad phandle, 0x%x", h4);

	if ((h5 == 0) || (h5 == -1))
		FAIL("/node5 has bad phandle, 0x%x", h5);
	if ((h5 == h4) || (h5 == h2) || (h5 == h1))
		FAIL("/node5 has duplicate phandle, 0x%x", h5);

	check_ref(fdt, n1, h2);
	check_ref(fdt, n2, h1);
	check_ref(fdt, n3, h4);

	PASS();
}
