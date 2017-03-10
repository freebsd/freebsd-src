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

#define CHECK(code) \
	{ \
		int err = (code); \
		if (err) \
			FAIL(#code ": %s", fdt_strerror(err)); \
	}

/* 4k ought to be enough for anybody */
#define FDT_COPY_SIZE	(4 * 1024)

static int fdt_getprop_u32_by_poffset(void *fdt, const char *path,
				      const char *name, int poffset,
				      unsigned long *out)
{
	const fdt32_t *val;
	int node_off;
	int len;

	node_off = fdt_path_offset(fdt, path);
	if (node_off < 0)
		return node_off;

	val = fdt_getprop(fdt, node_off, name, &len);
	if (!val || (len < (sizeof(uint32_t) * (poffset + 1))))
		return -FDT_ERR_NOTFOUND;

	*out = fdt32_to_cpu(*(val + poffset));

	return 0;
}

static int check_getprop_string_by_name(void *fdt, const char *path,
					const char *name, const char *val)
{
	int node_off;

	node_off = fdt_path_offset(fdt, path);
	if (node_off < 0)
		return node_off;

	check_getprop_string(fdt, node_off, name, val);

	return 0;
}

static int check_getprop_u32_by_name(void *fdt, const char *path,
				     const char *name, uint32_t val)
{
	int node_off;

	node_off = fdt_path_offset(fdt, path);
	CHECK(node_off < 0);

	check_getprop_cell(fdt, node_off, name, val);

	return 0;
}

static int check_getprop_null_by_name(void *fdt, const char *path,
				      const char *name)
{
	int node_off;

	node_off = fdt_path_offset(fdt, path);
	CHECK(node_off < 0);

	check_property(fdt, node_off, name, 0, NULL);

	return 0;
}

static int fdt_overlay_change_int_property(void *fdt)
{
	return check_getprop_u32_by_name(fdt, "/test-node", "test-int-property",
					 43);
}

static int fdt_overlay_change_str_property(void *fdt)
{
	return check_getprop_string_by_name(fdt, "/test-node",
					    "test-str-property", "foobar");
}

static int fdt_overlay_add_str_property(void *fdt)
{
	return check_getprop_string_by_name(fdt, "/test-node",
					    "test-str-property-2", "foobar2");
}

static int fdt_overlay_add_node(void *fdt)
{
	return check_getprop_null_by_name(fdt, "/test-node/new-node",
					  "new-property");
}

static int fdt_overlay_add_subnode_property(void *fdt)
{
	check_getprop_null_by_name(fdt, "/test-node/sub-test-node",
				   "sub-test-property");
	check_getprop_null_by_name(fdt, "/test-node/sub-test-node",
				   "new-sub-test-property");

	return 0;
}

static int fdt_overlay_local_phandle(void *fdt)
{
	uint32_t local_phandle;
	unsigned long val = 0;
	int off;

	off = fdt_path_offset(fdt, "/test-node/new-local-node");
	CHECK(off < 0);

	local_phandle = fdt_get_phandle(fdt, off);
	CHECK(!local_phandle);

	CHECK(fdt_getprop_u32_by_poffset(fdt, "/test-node",
					 "test-several-phandle",
					 0, &val));
	CHECK(val != local_phandle);

	CHECK(fdt_getprop_u32_by_poffset(fdt, "/test-node",
					 "test-several-phandle",
					 1, &val));
	CHECK(val != local_phandle);

	return 0;
}

static int fdt_overlay_local_phandles(void *fdt)
{
	uint32_t local_phandle, test_phandle;
	unsigned long val = 0;
	int off;

	off = fdt_path_offset(fdt, "/test-node/new-local-node");
	CHECK(off < 0);

	local_phandle = fdt_get_phandle(fdt, off);
	CHECK(!local_phandle);

	off = fdt_path_offset(fdt, "/test-node");
	CHECK(off < 0);

	test_phandle = fdt_get_phandle(fdt, off);
	CHECK(!test_phandle);

	CHECK(fdt_getprop_u32_by_poffset(fdt, "/test-node",
					 "test-phandle", 0, &val));
	CHECK(test_phandle != val);

	CHECK(fdt_getprop_u32_by_poffset(fdt, "/test-node",
					 "test-phandle", 1, &val));
	CHECK(local_phandle != val);

	return 0;
}

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
	void *fdt_base, *fdt_overlay;

	test_init(argc, argv);
	if (argc != 3)
		CONFIG("Usage: %s <base dtb> <overlay dtb>", argv[0]);

	fdt_base = open_dt(argv[1]);
	fdt_overlay = open_dt(argv[2]);

	/* Apply the overlay */
	CHECK(fdt_overlay_apply(fdt_base, fdt_overlay));

	fdt_overlay_change_int_property(fdt_base);
	fdt_overlay_change_str_property(fdt_base);
	fdt_overlay_add_str_property(fdt_base);
	fdt_overlay_add_node(fdt_base);
	fdt_overlay_add_subnode_property(fdt_base);

	/*
	 * If the base tree has a __symbols__ node, do the tests that
	 * are only successful with a proper phandle support, and thus
	 * dtc -@
	 */
	if (fdt_path_offset(fdt_base, "/__symbols__") >= 0) {
		fdt_overlay_local_phandle(fdt_base);
		fdt_overlay_local_phandles(fdt_base);
	}

	PASS();
}
