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
 * $Id: cpp_demangle-tc.c 2085 2011-10-27 05:06:47Z jkoshy $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tet_api.h>

#include "../../../cpp_demangle.h"

static void	startup();
static void	cleanup();
static void	test_func();
static void	test_oper();
static void	test_templ();
static void	test_scope();
static void	test_subst();
static void	test_cpp_demangle_ia64(const char *, const char *);

void (*tet_startup)() = NULL;
void (*tet_cleanup)() = NULL;

struct tet_testlist tet_testlist[] = {
	{ test_func, 1},
        { test_oper, 2},
        { test_templ, 3},
        { test_scope, 4},
        { test_subst, 5},
	{ NULL, 0}
};

static void
test_func()
{

	tet_infoline("FUNCTION");
	test_cpp_demangle_ia64("_Z1fv", "f(void)");
	test_cpp_demangle_ia64("_Z1fi", "f(int)");
	test_cpp_demangle_ia64("_Z3foo3bar", "foo(bar)");
}

static void
test_oper()
{

	tet_infoline("OPERATOR");
	test_cpp_demangle_ia64("_Zrm1XS_", "operator%(X, X)");
	test_cpp_demangle_ia64("_ZplR1XS0_", "operator+(X&, X&)");
	test_cpp_demangle_ia64("_ZlsRK1XS1_",
	    "operator<<(X const&, X const&)");
}

static void
test_templ()
{

	tet_infoline("TEMPLATE");
	test_cpp_demangle_ia64("_ZN3FooIA4_iE3barE",
	    "Foo<int[4]>::bar");
	test_cpp_demangle_ia64("_Z1fIiEvi", "void f<int>(int)");
	test_cpp_demangle_ia64("_Z5firstI3DuoEvS0_",
	    "void first<Duo>(Duo)");
	test_cpp_demangle_ia64("_Z5firstI3DuoEvT_",
	    "void first<Duo>(Duo)");
	test_cpp_demangle_ia64("_Z3fooIiPFidEiEvv",
	    "void foo<int, int(*)(double), int>(void)");
	test_cpp_demangle_ia64("_Z1fI1XEvPVN1AIT_E1TE",
	    "void f<X>(A<X>::T volatile*)");
	test_cpp_demangle_ia64("_ZngILi42EEvN1AIXplT_Li2EEE1TE",
	    "void operator-<42>(A<J+2>::T)");
	test_cpp_demangle_ia64("_Z4makeI7FactoryiET_IT0_Ev",
	    "Factory<int> make<Factory, int>(void)");
}

static void
test_scope()
{

	tet_infoline("SCOPE");
	test_cpp_demangle_ia64("_ZN1N1fE", "N::f");
	test_cpp_demangle_ia64("_ZN6System5Sound4beepEv",
	    "System::Sound::beep(void)");
	test_cpp_demangle_ia64("_ZN5Arena5levelE", "Arena::level");
	test_cpp_demangle_ia64("_ZN5StackIiiE5levelE",
	    "Stack<int, int>::level");

}

static void
test_subst()
{

	tet_infoline("SUBSTITUTION");
	test_cpp_demangle_ia64("_Z3foo5Hello5WorldS0_S_",
	    "foo(Hello, World, World, Hello)");
	test_cpp_demangle_ia64("_Z3fooPM2ABi", "foo(int AB::**)");
	test_cpp_demangle_ia64("_ZlsRSoRKSs",
	    "operator<<(std::ostream&, std::string const&)");
	test_cpp_demangle_ia64("_ZTI7a_class",
	    "typeinfo for (a_class)");
	test_cpp_demangle_ia64("_ZSt5state", "std::state");
	test_cpp_demangle_ia64("_ZNSt3_In4wardE", "std::_In::ward");
}

static void
test_cpp_demangle_ia64(const char *org, const char *dst)
{
	char *rst;

	if ((rst = cpp_demangle_ia64(org)) == NULL) {
		const size_t len = strlen(org);
		char *msg;

		if ((msg = malloc(len + 8)) != NULL) {
			snprintf(msg, len + 8, "Cannot demangle : %s", org);
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
