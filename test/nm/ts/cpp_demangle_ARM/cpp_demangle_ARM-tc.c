/*-
 * Copyright (c) 2008 Hyogeol Lee <hyogeollee@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: cpp_demangle_ARM-tc.c 2085 2011-10-27 05:06:47Z jkoshy $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tet_api.h>

#include "../../../cpp_demangle_arm.h"

static void	startup();
static void	cleanup();
static void	test_basic();
static void	test_modifier();
static void	test_subst();
static void	test_example();
static void	test_cpp_demangle_ARM(const char *, const char *);

void (*tet_startup)() = NULL;
void (*tet_cleanup)() = NULL;

struct tet_testlist tet_testlist[] = {
	{ test_basic, 1},
	{ test_modifier, 2},
	{ test_subst, 3},
	{ test_example, 4},
	{ NULL, 0}
};

static void
test_basic()
{

	tet_infoline("BASIC ENCODING");
	test_cpp_demangle_ARM("f__Fide", "f(int, double, ...)");
	test_cpp_demangle_ARM("f__Fv", "f(void)");
	test_cpp_demangle_ARM("f__Q25Outer5Inner__Fv", "Outer::Inner::f(void)");
	test_cpp_demangle_ARM("update__3recFd", "rec::update(double)");
	test_cpp_demangle_ARM("f__1xFi", "x::f(int)");
	test_cpp_demangle_ARM("f__F1xi", "f(x, int)");
	test_cpp_demangle_ARM("__ct__1xFv", "x::x()");
	test_cpp_demangle_ARM("__dt__1xFv", "x::~x()");
	test_cpp_demangle_ARM("__opQ25Name16Class1__Q25Name16Class2",
	    "Name1::Class2::operator Name1::Class1()");
}

static void
test_modifier()
{

	tet_infoline("MODIFIER and TYPE DECLARATOR");
	test_cpp_demangle_ARM("f__FUi", "f(unsigned int)");
	test_cpp_demangle_ARM("f__FCSc", "f(const signed char)");
	test_cpp_demangle_ARM("f__FPc", "f(char*)");
	test_cpp_demangle_ARM("f__FPCc", "f(const char*)");
	test_cpp_demangle_ARM("f__FCPc", "f(char* const)");
	test_cpp_demangle_ARM("f__FPFPc_i", "f(int (*)(char*))");
	test_cpp_demangle_ARM("f__FA10_i", "f(int[10])");
	test_cpp_demangle_ARM("f__FM1S7complex", "f(S::*complex)");
}

static void
test_subst()
{

	tet_infoline("SUBSTITUTION");
	test_cpp_demangle_ARM("f__F7complexT1", "f(complex, complex)");
	test_cpp_demangle_ARM("f__F6recordN21", "f(record, record, record)");
}

static void
test_example()
{

	tet_infoline("EXAMPLE");
	test_cpp_demangle_ARM("__dt__12PathListHeadFv",
	    "PathListHead::~PathListHead()");
	test_cpp_demangle_ARM("__ad__4PathFR4Path", "Path::operator&(Path&)");
	test_cpp_demangle_ARM("first__4PathFv", "Path::first(void)");
	test_cpp_demangle_ARM("last__4PathFv", "Path::last(void)");
	test_cpp_demangle_ARM("findpath__4PathFR6String",
	    "Path::findpath(String&)");
	test_cpp_demangle_ARM("fullpath__4PathFv", "Path::fullpath(void)");
}

static void
test_cpp_demangle_ARM(const char *org, const char *dst)
{
	char *rst;

	if ((rst = cpp_demangle_ARM(org)) == NULL) {
		const size_t len = strlen(org);
		char *msg;

		if ((msg = malloc(len + 19)) != NULL) {
			snprintf(msg, len + 19, "Cannot demangle : %s", org);
			tet_infoline(msg);
			free(msg);
		}

		tet_result(TET_FAIL);

		return;
	}

	if (strcmp(rst, dst) != 0) {
		const size_t len = strlen(org) + strlen(rst) + strlen(dst);
		char *msg;

		if ((msg = malloc(len + 17)) != NULL) {
			snprintf(msg, len + 17, "Diff for %s : %s != %s", org,
			    rst, dst);
			tet_infoline(msg);
			free(msg);
		}

		free(rst);
		tet_result(TET_FAIL);

		return;
	}

	free(rst);

	tet_result(TET_PASS);
}
