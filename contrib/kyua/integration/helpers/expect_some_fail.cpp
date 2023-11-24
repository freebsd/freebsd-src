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

extern "C" {
#include <signal.h>
#include <unistd.h>
}

#include <cstdlib>

#include <atf-c++.hpp>


ATF_TEST_CASE_WITHOUT_HEAD(die);
ATF_TEST_CASE_BODY(die)
{
    expect_death("Won't die");
}


ATF_TEST_CASE_WITHOUT_HEAD(exit);
ATF_TEST_CASE_BODY(exit)
{
    expect_exit(12, "Invalid exit code");
    std::exit(34);
}


ATF_TEST_CASE_WITHOUT_HEAD(failure);
ATF_TEST_CASE_BODY(failure)
{
    expect_fail("Does not fail");
}


ATF_TEST_CASE_WITHOUT_HEAD(pass);
ATF_TEST_CASE_BODY(pass)
{
}


ATF_TEST_CASE_WITHOUT_HEAD(signal);
ATF_TEST_CASE_BODY(signal)
{
    expect_signal(SIGTERM, "Invalid signal");
    ::kill(::getpid(), SIGKILL);
}


ATF_TEST_CASE(timeout);
ATF_TEST_CASE_HEAD(timeout)
{
    set_md_var("timeout", "1");
}
ATF_TEST_CASE_BODY(timeout)
{
    expect_timeout("Does not time out");
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, die);
    ATF_ADD_TEST_CASE(tcs, exit);
    ATF_ADD_TEST_CASE(tcs, failure);
    ATF_ADD_TEST_CASE(tcs, pass);
    ATF_ADD_TEST_CASE(tcs, signal);
    ATF_ADD_TEST_CASE(tcs, timeout);
}
