// Copyright 2011 The Kyua Authors.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "utils/logging/macros.hpp"

#include <fstream>
#include <string>

#include <atf-c++.hpp>

#include "utils/datetime.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/operations.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace logging = utils::logging;


ATF_TEST_CASE_WITHOUT_HEAD(ld);
ATF_TEST_CASE_BODY(ld)
{
    logging::set_persistency("debug", fs::path("test.log"));
    datetime::set_mock_now(2011, 2, 21, 18, 30, 0, 0);
    LD("Debug message");

    std::ifstream input("test.log");
    ATF_REQUIRE(input);

    std::string line;
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_MATCH("20110221-183000 D .*: Debug message", line);
}


ATF_TEST_CASE_WITHOUT_HEAD(le);
ATF_TEST_CASE_BODY(le)
{
    logging::set_persistency("debug", fs::path("test.log"));
    datetime::set_mock_now(2011, 2, 21, 18, 30, 0, 0);
    LE("Error message");

    std::ifstream input("test.log");
    ATF_REQUIRE(input);

    std::string line;
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_MATCH("20110221-183000 E .*: Error message", line);
}


ATF_TEST_CASE_WITHOUT_HEAD(li);
ATF_TEST_CASE_BODY(li)
{
    logging::set_persistency("debug", fs::path("test.log"));
    datetime::set_mock_now(2011, 2, 21, 18, 30, 0, 0);
    LI("Info message");

    std::ifstream input("test.log");
    ATF_REQUIRE(input);

    std::string line;
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_MATCH("20110221-183000 I .*: Info message", line);
}


ATF_TEST_CASE_WITHOUT_HEAD(lw);
ATF_TEST_CASE_BODY(lw)
{
    logging::set_persistency("debug", fs::path("test.log"));
    datetime::set_mock_now(2011, 2, 21, 18, 30, 0, 0);
    LW("Warning message");

    std::ifstream input("test.log");
    ATF_REQUIRE(input);

    std::string line;
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_MATCH("20110221-183000 W .*: Warning message", line);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, ld);
    ATF_ADD_TEST_CASE(tcs, le);
    ATF_ADD_TEST_CASE(tcs, li);
    ATF_ADD_TEST_CASE(tcs, lw);
}
