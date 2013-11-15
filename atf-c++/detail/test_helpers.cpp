//
// Automated Testing Framework (atf)
//
// Copyright (c) 2009 The NetBSD Foundation, Inc.
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

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../check.hpp"
#include "../config.hpp"
#include "../macros.hpp"

#include "fs.hpp"
#include "process.hpp"
#include "test_helpers.hpp"

void
build_check_cxx_o_aux(const atf::fs::path& sfile, const char* failmsg,
                      const bool expect_pass)
{
    std::vector< std::string > optargs;
    optargs.push_back("-I" + atf::config::get("atf_includedir"));
    optargs.push_back("-Wall");
    optargs.push_back("-Werror");

    const bool result = atf::check::build_cxx_o(
        sfile.str(), "test.o", atf::process::argv_array(optargs));
    if ((expect_pass && !result) || (!expect_pass && result))
        ATF_FAIL(failmsg);
}

void
build_check_cxx_o(const atf::tests::tc& tc, const char* sfile,
                  const char* failmsg, const bool expect_pass)
{
    const atf::fs::path sfilepath =
        atf::fs::path(tc.get_config_var("srcdir")) / sfile;
    build_check_cxx_o_aux(sfilepath, failmsg, expect_pass);
}

void
header_check(const char *hdrname)
{
    std::ofstream srcfile("test.c");
    ATF_REQUIRE(srcfile);
    srcfile << "#include <" << hdrname << ">\n";
    srcfile.close();

    const std::string failmsg = std::string("Header check failed; ") +
        hdrname + " is not self-contained";
    build_check_cxx_o_aux(atf::fs::path("test.c"), failmsg.c_str(), true);
}

atf::fs::path
get_process_helpers_path(const atf::tests::tc& tc)
{
    return atf::fs::path(tc.get_config_var("srcdir")) /
           ".." / "atf-c" / "detail" / "process_helpers";
}

void
test_helpers_detail::check_equal(const char* expected[],
                                 const string_vector& actual)
{
    const char** expected_iter = expected;
    string_vector::const_iterator actual_iter = actual.begin();

    bool equals = true;
    while (equals && *expected_iter != NULL && actual_iter != actual.end()) {
        if (*expected_iter != *actual_iter) {
            equals = false;
        } else {
            expected_iter++;
            actual_iter++;
        }
    }
    if (equals && ((*expected_iter == NULL && actual_iter != actual.end()) ||
                   (*expected_iter != NULL && actual_iter == actual.end())))
        equals = false;

    if (!equals) {
        std::cerr << "EXPECTED:\n";
        for (expected_iter = expected; *expected_iter != NULL; expected_iter++)
            std::cerr << *expected_iter << "\n";

        std::cerr << "ACTUAL:\n";
        for (actual_iter = actual.begin(); actual_iter != actual.end();
             actual_iter++)
            std::cerr << *actual_iter << "\n";

        ATF_FAIL("Expected results differ to actual values");
    }
}
