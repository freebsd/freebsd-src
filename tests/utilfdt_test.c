/*
 * Copyright 2011 The Chromium Authors, All Rights Reserved.
 *
 * utilfdt_test - Tests for utilfdt library
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
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#include <libfdt.h>
#include <util.h>

#include "tests.h"
#include "testdata.h"

static void check(const char *fmt, int expect_type, int expect_size)
{
	int type;
	int size;

	if (utilfdt_decode_type(fmt, &type, &size))
		FAIL("format '%s': valid format string returned failure", fmt);
	if (expect_type != type)
		FAIL("format '%s': expected type='%c', got type='%c'", fmt,
		     expect_type, type);
	if (expect_size != size)
		FAIL("format '%s': expected size=%d, got size=%d", fmt,
		     expect_size, size);
}

static void checkfail(const char *fmt)
{
	int type;
	int size;

	if (!utilfdt_decode_type(fmt, &type, &size))
		FAIL("format '%s': invalid format string returned success",
		     fmt);
}

/**
 * Add the given modifier to each of the valid sizes, and check that we get
 * correct values.
 *
 * \param modifier	Modifer string to use as a prefix
 * \param expected_size	The size (in bytes) that we expect (ignored for
 *			strings)
 */
static void check_sizes(char *modifier, int expected_size)
{
	char fmt[10], *ptr;

	/* set up a string with a hole in it for the format character */
	if (strlen(modifier) + 2 >= sizeof(fmt))
		FAIL("modifier string '%s' too long", modifier);
	strcpy(fmt, modifier);
	ptr = fmt + strlen(fmt);
	ptr[1] = '\0';

	/* now try each format character in turn */
	*ptr = 'i';
	check(fmt, 'i', expected_size);

	*ptr = 'u';
	check(fmt, 'u', expected_size);

	*ptr = 'x';
	check(fmt, 'x', expected_size);

	*ptr = 's';
	check(fmt, 's', -1);
}

static void test_utilfdt_decode_type(void)
{
	char fmt[10];
	int ch;

	/* check all the valid modifiers and sizes */
	check_sizes("", -1);
	check_sizes("b", 1);
	check_sizes("hh", 1);
	check_sizes("h", 2);
	check_sizes("l", 4);

	/* try every other character */
	checkfail("");
	for (ch = ' '; ch < 127; ch++) {
		if (!strchr("iuxs", ch)) {
			*fmt = ch;
			fmt[1] = '\0';
			checkfail(fmt);
		}
	}

	/* try a few modifiers at the end */
	checkfail("sx");
	checkfail("ihh");
	checkfail("xb");

	/* and one for the doomsday archives */
	checkfail("He has all the virtues I dislike and none of the vices "
			"I admire.");
}

int main(int argc, char *argv[])
{
	test_utilfdt_decode_type();
	PASS();
}
