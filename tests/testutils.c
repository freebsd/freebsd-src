/*
 * libfdt - Flat Device Tree manipulation
 *	Testcase common utility functions
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

#define _GNU_SOURCE /* for strsignal() in glibc.  FreeBSD has it either way */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

#include <libfdt.h>

#include "tests.h"

int verbose_test = 1;
char *test_name;

void  __attribute__((weak)) cleanup(void)
{
}

static void sigint_handler(int signum, siginfo_t *si, void *uc)
{
	cleanup();
	fprintf(stderr, "%s: %s (pid=%d)\n", test_name,
		strsignal(signum), getpid());
	exit(RC_BUG);
}

void test_init(int argc, char *argv[])
{
	int err;
	struct sigaction sa_int = {
		.sa_sigaction = sigint_handler,
	};

	test_name = argv[0];

	err = sigaction(SIGINT, &sa_int, NULL);
	if (err)
		FAIL("Can't install SIGINT handler");

	if (getenv("QUIET_TEST"))
		verbose_test = 0;

	verbose_printf("Starting testcase \"%s\", pid %d\n",
		       test_name, getpid());
}

void check_mem_rsv(void *fdt, int n, uint64_t addr, uint64_t size)
{
	int err;
	uint64_t addr_v, size_v;

	err = fdt_get_mem_rsv(fdt, n, &addr_v, &size_v);
	if (err < 0)
		FAIL("fdt_get_mem_rsv(%d): %s", n, fdt_strerror(err));
	if ((addr_v != addr) || (size_v != size))
		FAIL("fdt_get_mem_rsv() returned (0x%llx,0x%llx) "
		     "instead of (0x%llx,0x%llx)",
		     (unsigned long long)addr_v, (unsigned long long)size_v,
		     (unsigned long long)addr, (unsigned long long)size);
}

void check_property(void *fdt, int nodeoffset, const char *name,
		    int len, const void *val)
{
	const struct fdt_property *prop;
	int retlen;
	uint32_t tag, nameoff, proplen;
	const char *propname;

	verbose_printf("Checking property \"%s\"...", name);
	prop = fdt_get_property(fdt, nodeoffset, name, &retlen);
	verbose_printf("pointer %p\n", prop);
	if (! prop)
		FAIL("Error retreiving \"%s\" pointer: %s", name,
		     fdt_strerror(retlen));

	tag = fdt32_to_cpu(prop->tag);
	nameoff = fdt32_to_cpu(prop->nameoff);
	proplen = fdt32_to_cpu(prop->len);

	if (tag != FDT_PROP)
		FAIL("Incorrect tag 0x%08x on property \"%s\"", tag, name);

	propname = fdt_string(fdt, nameoff);
	if (!propname || !streq(propname, name))
		FAIL("Property name mismatch \"%s\" instead of \"%s\"",
		     propname, name);
	if (proplen != retlen)
		FAIL("Length retrieved for \"%s\" by fdt_get_property()"
		     " differs from stored length (%d != %d)",
		     name, retlen, proplen);
	if (proplen != len)
		FAIL("Size mismatch on property \"%s\": %d insead of %d",
		     name, proplen, len);
	if (memcmp(val, prop->data, len) != 0)
		FAIL("Data mismatch on property \"%s\"", name);
}

const void *check_getprop(void *fdt, int nodeoffset, const char *name,
			  int len, const void *val)
{
	const void *propval;
	int proplen;

	propval = fdt_getprop(fdt, nodeoffset, name, &proplen);
	if (! propval)
		FAIL("fdt_getprop(\"%s\"): %s", name, fdt_strerror(proplen));

	if (proplen != len)
		FAIL("Size mismatch on property \"%s\": %d insead of %d",
		     name, proplen, len);
	if (memcmp(val, propval, len) != 0)
		FAIL("Data mismatch on property \"%s\"", name);

	return propval;
}

int nodename_eq(const char *s1, const char *s2)
{
	int len = strlen(s2);

	if (strncmp(s1, s2, len) != 0)
		return 0;
	if (s1[len] == '\0')
		return 1;
	else if (!memchr(s2, '@', len) && (s1[len] == '@'))
		return 1;
	else
		return 0;
}

#define CHUNKSIZE	128

void *load_blob(const char *filename)
{
	char *blob;
	int ret = utilfdt_read_err(filename, &blob);

	if (ret)
		CONFIG("Couldn't open blob from \"%s\": %s", filename,
		       strerror(ret));
	return blob;
}

void *load_blob_arg(int argc, char *argv[])
{
	if (argc != 2)
		CONFIG("Usage: %s <dtb file>", argv[0]);
	return load_blob(argv[1]);
}

void save_blob(const char *filename, void *fdt)
{
	int ret = utilfdt_write_err(filename, fdt);

	if (ret)
		CONFIG("Couldn't write blob to \"%s\": %s", filename,
		       strerror(ret));
}

void *open_blob_rw(void *blob)
{
	int err;
	void *buf = blob;

	err = fdt_open_into(blob, buf, fdt_totalsize(blob));
	if (err == -FDT_ERR_NOSPACE) {
		/* Ran out of space converting to v17 */
		int newsize = fdt_totalsize(blob) + 8;

		buf = xmalloc(newsize);
		err = fdt_open_into(blob, buf, newsize);
	}
	if (err)
		FAIL("fdt_open_into(): %s", fdt_strerror(err));
	return buf;
}
