/*
 * Testcase for dtc expression support
 *
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


#include <libfdt.h>

#include "tests.h"
#include "testdata.h"

static struct test_expr {
	const char *expr;
	uint32_t result;
} expr_table[] = {
#define TE(expr)	{ #expr, (expr) }
	TE(0xdeadbeef),
	TE(-0x21524111),
	TE(1+1),
	TE(2*3),
	TE(4/2),
	TE(10/3),
	TE(19%4),
	TE(1 << 13),
	TE(0x1000 >> 4),
	TE(3*2+1), TE(3*(2+1)),
	TE(1+2*3), TE((1+2)*3),
	TE(1 < 2), TE(2 < 1), TE(1 < 1),
	TE(1 <= 2), TE(2 <= 1), TE(1 <= 1),
	TE(1 > 2), TE(2 > 1), TE(1 > 1),
	TE(1 >= 2), TE(2 >= 1), TE(1 >= 1),
	TE(1 == 1), TE(1 == 2),
	TE(1 != 1), TE(1 != 2),
	TE(0xabcdabcd & 0xffff0000),
	TE(0xdead4110 ^ 0xf0f0f0f0),
	TE(0xabcd0000 | 0x0000abcd),
	TE(~0x21524110),
	TE(~~0xdeadbeef),
	TE(0 && 0), TE(17 && 0), TE(0 && 17), TE(17 && 17),
	TE(0 || 0), TE(17 || 0), TE(0 || 17), TE(17 || 17),
	TE(!0), TE(!1), TE(!17), TE(!!0), TE(!!17),
	TE(0 ? 17 : 39), TE(1 ? 17 : 39), TE(17 ? 0xdeadbeef : 0xabcd1234),
	TE(11 * 257 * 1321517ULL),
	TE(123456790 - 4/2 + 17%4),
};

#define ARRAY_SIZE(x)	(sizeof(x) / sizeof((x)[0]))

int main(int argc, char *argv[])
{
	void *fdt;
	const fdt32_t *res;
	int reslen;
	int i;

	test_init(argc, argv);

	if ((argc == 3) && (strcmp(argv[1], "-g") == 0)) {
		FILE *f = fopen(argv[2], "w");

		if (!f)
			FAIL("Couldn't open \"%s\" for output: %s\n",
			     argv[2], strerror(errno));

		fprintf(f, "/dts-v1/;\n");
		fprintf(f, "/ {\n");
		fprintf(f, "\texpressions = <\n");
		for (i = 0; i < ARRAY_SIZE(expr_table); i++)
			fprintf(f, "\t\t(%s)\n", expr_table[i].expr);
		fprintf(f, "\t>;\n");
		fprintf(f, "};\n");
		fclose(f);
	} else {
		fdt = load_blob_arg(argc, argv);

		res = fdt_getprop(fdt, 0, "expressions", &reslen);

		if (!res)
			FAIL("Error retreiving expression results: %s\n",
		     fdt_strerror(reslen));

		if (reslen != (ARRAY_SIZE(expr_table) * sizeof(uint32_t)))
			FAIL("Unexpected length of results %d instead of %zd\n",
			     reslen, ARRAY_SIZE(expr_table) * sizeof(uint32_t));

		for (i = 0; i < ARRAY_SIZE(expr_table); i++)
			if (fdt32_to_cpu(res[i]) != expr_table[i].result)
				FAIL("Incorrect result for expression \"%s\","
				     " 0x%x instead of 0x%x\n",
				     expr_table[i].expr, fdt32_to_cpu(res[i]),
				     expr_table[i].result);
	}

	PASS();
}
