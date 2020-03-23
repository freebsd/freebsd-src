// Copyright 2010 The Kyua Authors.
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

#include "utils/cmdline/globals.hpp"

#include <atf-c++.hpp>

namespace cmdline = utils::cmdline;


ATF_TEST_CASE_WITHOUT_HEAD(progname__absolute);
ATF_TEST_CASE_BODY(progname__absolute)
{
    cmdline::init("/path/to/foobar");
    ATF_REQUIRE_EQ("foobar", cmdline::progname());
}


ATF_TEST_CASE_WITHOUT_HEAD(progname__relative);
ATF_TEST_CASE_BODY(progname__relative)
{
    cmdline::init("to/barbaz");
    ATF_REQUIRE_EQ("barbaz", cmdline::progname());
}


ATF_TEST_CASE_WITHOUT_HEAD(progname__plain);
ATF_TEST_CASE_BODY(progname__plain)
{
    cmdline::init("program");
    ATF_REQUIRE_EQ("program", cmdline::progname());
}


ATF_TEST_CASE_WITHOUT_HEAD(progname__override_for_testing);
ATF_TEST_CASE_BODY(progname__override_for_testing)
{
    cmdline::init("program");
    ATF_REQUIRE_EQ("program", cmdline::progname());

    cmdline::init("foo", true);
    ATF_REQUIRE_EQ("foo", cmdline::progname());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, progname__absolute);
    ATF_ADD_TEST_CASE(tcs, progname__relative);
    ATF_ADD_TEST_CASE(tcs, progname__plain);
    ATF_ADD_TEST_CASE(tcs, progname__override_for_testing);
}
