/*
 * libfdt - Flat Device Tree manipulation
 *	Test labels within values
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

struct val_label {
	const char *labelname;
	int propoff;
};

struct val_label labels1[] = {
	{ "start1", 0 },
	{ "mid1", 2 },
	{ "end1", -1 },
};

struct val_label labels2[] = {
	{ "start2", 0 },
	{ "innerstart2", 0 },
	{ "innermid2", 4 },
	{ "innerend2", -1 },
	{ "end2", -1 },
};

struct val_label labels3[] = {
	{ "start3", 0 },
	{ "innerstart3", 0 },
	{ "innermid3", 1 },
	{ "innerend3", -1 },
	{ "end3", -1 },
};

static void check_prop_labels(void *sohandle, void *fdt, const char *name,
			      const struct val_label* labels, int n)
{
	const struct fdt_property *prop;
	const char *p;
	int len;
	int i;

	prop = fdt_get_property(fdt, 0, name, &len);
	if (!prop)
		FAIL("Couldn't locate property \"%s\"", name);

	p = dlsym(sohandle, name);
	if (!p)
		FAIL("Couldn't locate label symbol \"%s\"", name);

	if (p != (const char *)prop)
		FAIL("Label \"%s\" does not point to correct property", name);

	for (i = 0; i < n; i++) {
		int off = labels[i].propoff;

		if (off == -1)
			off = len;

		p = dlsym(sohandle, labels[i].labelname);
		if (!p)
			FAIL("Couldn't locate label symbol \"%s\"", name);

		if ((p - prop->data) != off)
			FAIL("Label \"%s\" points to offset %ld instead of %d"
			     "in property \"%s\"", labels[i].labelname,
			     (long)(p - prop->data), off, name);
	}
}

int main(int argc, char *argv[])
{
	void *sohandle;
	void *fdt;
	int err;

	test_init(argc, argv);
	if (argc != 2)
		CONFIG("Usage: %s <so file>", argv[0]);

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


	check_prop_labels(sohandle, fdt, "prop1", labels1, ARRAY_SIZE(labels1));
	check_prop_labels(sohandle, fdt, "prop2", labels2, ARRAY_SIZE(labels2));
	check_prop_labels(sohandle, fdt, "prop3", labels3, ARRAY_SIZE(labels3));

	PASS();
}
