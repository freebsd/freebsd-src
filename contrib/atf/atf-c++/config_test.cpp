//
// Automated Testing Framework (atf)
//
// Copyright (c) 2007 The NetBSD Foundation, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
// CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include <cstring>
#include <iostream>

#include "config.hpp"
#include "macros.hpp"

#include "detail/env.hpp"
#include "detail/exceptions.hpp"
#include "detail/test_helpers.hpp"

static const char *test_value = "env-value";

static struct varnames {
    const char *lc;
    const char *uc;
    bool can_be_empty;
} all_vars[] = {
    { "atf_build_cc",       "ATF_BUILD_CC",       false },
    { "atf_build_cflags",   "ATF_BUILD_CFLAGS",   true  },
    { "atf_build_cpp",      "ATF_BUILD_CPP",      false },
    { "atf_build_cppflags", "ATF_BUILD_CPPFLAGS", true  },
    { "atf_build_cxx",      "ATF_BUILD_CXX",      false },
    { "atf_build_cxxflags", "ATF_BUILD_CXXFLAGS", true  },
    { "atf_includedir",     "ATF_INCLUDEDIR",     false },
    { "atf_libexecdir",     "ATF_LIBEXECDIR",     false },
    { "atf_pkgdatadir",     "ATF_PKGDATADIR",     false },
    { "atf_shell",          "ATF_SHELL",          false },
    { "atf_workdir",        "ATF_WORKDIR",        false },
    { NULL,                 NULL,                 false }
};

// ------------------------------------------------------------------------
// Auxiliary functions.
// ------------------------------------------------------------------------

namespace atf {
    namespace config {
        void __reinit(void);
    }
}

static
void
set_env_var(const char* name, const char* val)
{
    try {
        atf::env::set(name, val);
    } catch (const atf::system_error&) {
        ATF_FAIL(std::string("set_env_var(") + name + ", " + val +
                 ") failed");
    }
}

static
void
unset_env_var(const char* name)
{
    try {
        atf::env::unset(name);
    } catch (const atf::system_error&) {
        ATF_FAIL(std::string("unset_env_var(") + name + ") failed");
    }
}

static
size_t
all_vars_count(void)
{
    size_t count = 0;
    for (const struct varnames* v = all_vars; v->lc != NULL; v++)
        count++;
    return count;
}

static
void
unset_all(void)
{
    for (const struct varnames* v = all_vars; v->lc != NULL; v++)
        unset_env_var(v->uc);
}

static
void
compare_one(const char* var, const char* expvalue)
{
    std::cout << "Checking that " << var << " is set to " << expvalue << "\n";

    for (const struct varnames* v = all_vars; v->lc != NULL; v++) {
        if (std::strcmp(v->lc, var) == 0)
            ATF_REQUIRE_EQ(atf::config::get(v->lc), test_value);
        else
            ATF_REQUIRE(atf::config::get(v->lc) != test_value);
    }
}

// ------------------------------------------------------------------------
// Test cases for the free functions.
// ------------------------------------------------------------------------

ATF_TEST_CASE(get);
ATF_TEST_CASE_HEAD(get)
{
    set_md_var("descr", "Tests the config::get function");
}
ATF_TEST_CASE_BODY(get)
{
    // Unset all known environment variables and make sure the built-in
    // values do not match the bogus value we will use for testing.
    unset_all();
    atf::config::__reinit();
    for (const struct varnames* v = all_vars; v->lc != NULL; v++)
        ATF_REQUIRE(atf::config::get(v->lc) != test_value);

    // Test the behavior of empty values.
    for (const struct varnames* v = all_vars; v->lc != NULL; v++) {
        unset_all();
        if (!atf::config::get(v->lc).empty()) {
            set_env_var(v->uc, "");
            atf::config::__reinit();
            if (v->can_be_empty)
                ATF_REQUIRE(atf::config::get(v->lc).empty());
            else
                ATF_REQUIRE(!atf::config::get(v->lc).empty());
        }
    }

    // Check if the ATF_ARCH variable is recognized.
    for (const struct varnames* v = all_vars; v->lc != NULL; v++) {
        unset_all();
        set_env_var(v->uc, test_value);
        atf::config::__reinit();
        compare_one(v->lc, test_value);
    }
}

ATF_TEST_CASE(get_all);
ATF_TEST_CASE_HEAD(get_all)
{
    set_md_var("descr", "Tests the config::get_all function");
}
ATF_TEST_CASE_BODY(get_all)
{
    atf::config::__reinit();

    // Check that the valid variables, and only those, are returned.
    std::map< std::string, std::string > vars = atf::config::get_all();
    ATF_REQUIRE_EQ(vars.size(), all_vars_count());
    for (const struct varnames* v = all_vars; v->lc != NULL; v++)
        ATF_REQUIRE(vars.find(v->lc) != vars.end());
}

ATF_TEST_CASE(has);
ATF_TEST_CASE_HEAD(has)
{
    set_md_var("descr", "Tests the config::has function");
}
ATF_TEST_CASE_BODY(has)
{
    atf::config::__reinit();

    // Check for all the variables that must exist.
    for (const struct varnames* v = all_vars; v->lc != NULL; v++)
        ATF_REQUIRE(atf::config::has(v->lc));

    // Same as above, but using uppercase (which is incorrect).
    for (const struct varnames* v = all_vars; v->lc != NULL; v++)
        ATF_REQUIRE(!atf::config::has(v->uc));

    // Check for some other variables that cannot exist.
    ATF_REQUIRE(!atf::config::has("foo"));
    ATF_REQUIRE(!atf::config::has("BAR"));
    ATF_REQUIRE(!atf::config::has("atf_foo"));
    ATF_REQUIRE(!atf::config::has("ATF_BAR"));
    ATF_REQUIRE(!atf::config::has("atf_shel"));
    ATF_REQUIRE(!atf::config::has("atf_shells"));
}

// ------------------------------------------------------------------------
// Tests cases for the header file.
// ------------------------------------------------------------------------

HEADER_TC(include, "atf-c++/config.hpp");

// ------------------------------------------------------------------------
// Main.
// ------------------------------------------------------------------------

ATF_INIT_TEST_CASES(tcs)
{
    // Add the test cases for the free functions.
    ATF_ADD_TEST_CASE(tcs, has);
    ATF_ADD_TEST_CASE(tcs, get);
    ATF_ADD_TEST_CASE(tcs, get_all);

    // Add the test cases for the header file.
    ATF_ADD_TEST_CASE(tcs, include);
}
