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

#include "utils/signals/programmer.hpp"

extern "C" {
#include <signal.h>
#include <unistd.h>
}

#include <atf-c++.hpp>

#include "utils/sanity.hpp"

namespace signals = utils::signals;


namespace {


namespace sigchld {


static bool happened_1;
static bool happened_2;


void handler_1(const int signo) {
    PRE(signo == SIGCHLD);
    happened_1 = true;
}


void handler_2(const int signo) {
    PRE(signo == SIGCHLD);
    happened_2 = true;
}


}  // namespace sigchld


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(program_unprogram);
ATF_TEST_CASE_BODY(program_unprogram)
{
    signals::programmer programmer(SIGCHLD, sigchld::handler_1);
    sigchld::happened_1 = false;
    ::kill(::getpid(), SIGCHLD);
    ATF_REQUIRE(sigchld::happened_1);

    programmer.unprogram();
    sigchld::happened_1 = false;
    ::kill(::getpid(), SIGCHLD);
    ATF_REQUIRE(!sigchld::happened_1);
}


ATF_TEST_CASE_WITHOUT_HEAD(scope);
ATF_TEST_CASE_BODY(scope)
{
    {
        signals::programmer programmer(SIGCHLD, sigchld::handler_1);
        sigchld::happened_1 = false;
        ::kill(::getpid(), SIGCHLD);
        ATF_REQUIRE(sigchld::happened_1);
    }

    sigchld::happened_1 = false;
    ::kill(::getpid(), SIGCHLD);
    ATF_REQUIRE(!sigchld::happened_1);
}


ATF_TEST_CASE_WITHOUT_HEAD(nested);
ATF_TEST_CASE_BODY(nested)
{
    signals::programmer programmer_1(SIGCHLD, sigchld::handler_1);
    sigchld::happened_1 = false;
    sigchld::happened_2 = false;
    ::kill(::getpid(), SIGCHLD);
    ATF_REQUIRE(sigchld::happened_1);
    ATF_REQUIRE(!sigchld::happened_2);

    signals::programmer programmer_2(SIGCHLD, sigchld::handler_2);
    sigchld::happened_1 = false;
    sigchld::happened_2 = false;
    ::kill(::getpid(), SIGCHLD);
    ATF_REQUIRE(!sigchld::happened_1);
    ATF_REQUIRE(sigchld::happened_2);

    programmer_2.unprogram();
    sigchld::happened_1 = false;
    sigchld::happened_2 = false;
    ::kill(::getpid(), SIGCHLD);
    ATF_REQUIRE(sigchld::happened_1);
    ATF_REQUIRE(!sigchld::happened_2);

    programmer_1.unprogram();
    sigchld::happened_1 = false;
    sigchld::happened_2 = false;
    ::kill(::getpid(), SIGCHLD);
    ATF_REQUIRE(!sigchld::happened_1);
    ATF_REQUIRE(!sigchld::happened_2);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, program_unprogram);
    ATF_ADD_TEST_CASE(tcs, scope);
    ATF_ADD_TEST_CASE(tcs, nested);
}
