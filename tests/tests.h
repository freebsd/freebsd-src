#ifndef _TESTS_H
#define _TESTS_H
/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase definitions
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

#define DEBUG

/* Test return codes */
#define RC_PASS 	0
#define RC_CONFIG 	1
#define RC_FAIL		2
#define RC_BUG		99

extern int verbose_test;
extern char *test_name;
void test_init(int argc, char *argv[]);

#define ALIGN(x, a)	(((x) + (a) - 1) & ~((a) - 1))
#define PALIGN(p, a)	((void *)ALIGN((unsigned long)(p), (a)))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define streq(s1, s2)	(strcmp((s1),(s2)) == 0)

/* Each test case must define this function */
void cleanup(void);

#define verbose_printf(...) \
	if (verbose_test) { \
		printf(__VA_ARGS__); \
		fflush(stdout); \
	}
#define ERR	"ERR: "
#define ERROR(fmt, args...)	fprintf(stderr, ERR fmt, ## args)


#define	PASS()						\
	do {						\
		cleanup();				\
		printf("PASS\n");			\
		exit(RC_PASS);				\
	} while (0)

#define	PASS_INCONCLUSIVE()				\
	do {						\
		cleanup();				\
		printf("PASS (inconclusive)\n");	\
		exit(RC_PASS);				\
	} while (0)

#define IRRELEVANT()					\
	do {						\
		cleanup();				\
		printf("PASS (irrelevant)\n");		\
		exit(RC_PASS);				\
	} while (0)

/* Look out, gcc extension below... */
#define FAIL(fmt, ...)					\
	do {						\
		cleanup();				\
		printf("FAIL\t" fmt "\n", ##__VA_ARGS__);	\
		exit(RC_FAIL);				\
	} while (0)

#define CONFIG(fmt, ...)				\
	do {						\
		cleanup();				\
		printf("Bad configuration: " fmt "\n", ##__VA_ARGS__);	\
		exit(RC_CONFIG);			\
	} while (0)

#define TEST_BUG(fmt, ...)				\
	do {						\
		cleanup();				\
		printf("BUG in testsuite: " fmt "\n", ##__VA_ARGS__);	\
		exit(RC_BUG);				\
	} while (0)

void check_mem_rsv(void *fdt, int n, uint64_t addr, uint64_t size);

void check_property(void *fdt, int nodeoffset, const char *name,
		    int len, const void *val);
#define check_property_cell(fdt, nodeoffset, name, val) \
	({ \
		uint32_t x = cpu_to_fdt32(val);			      \
		check_property(fdt, nodeoffset, name, sizeof(x), &x); \
	})


const void *check_getprop(void *fdt, int nodeoffset, const char *name,
			  int len, const void *val);
#define check_getprop_cell(fdt, nodeoffset, name, val) \
	({ \
		uint32_t x = cpu_to_fdt32(val);			     \
		check_getprop(fdt, nodeoffset, name, sizeof(x), &x); \
	})
#define check_getprop_64(fdt, nodeoffset, name, val) \
	({ \
		uint64_t x = cpu_to_fdt64(val);			     \
		check_getprop(fdt, nodeoffset, name, sizeof(x), &x); \
	})
#define check_getprop_string(fdt, nodeoffset, name, s) \
	check_getprop((fdt), (nodeoffset), (name), strlen(s)+1, (s))
int nodename_eq(const char *s1, const char *s2);
void *load_blob(const char *filename);
void *load_blob_arg(int argc, char *argv[]);
void save_blob(const char *filename, void *blob);
void *open_blob_rw(void *blob);

#include "util.h"

#endif /* _TESTS_H */
