// Copyright 2015 The Kyua Authors.
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

#include "utils/process/deadline_killer.hpp"

extern "C" {
#include <signal.h>
#include <unistd.h>
}

#include <cstdlib>

#include <atf-c++.hpp>

#include "utils/datetime.hpp"
#include "utils/process/child.ipp"
#include "utils/process/status.hpp"

namespace datetime = utils::datetime;
namespace process = utils::process;


namespace {


/// Body of a child process that sleeps and then exits.
///
/// \tparam Seconds The delay the subprocess has to sleep for.
template< int Seconds >
static void
child_sleep(void)
{
    ::sleep(Seconds);
    std::exit(EXIT_SUCCESS);
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(activation);
ATF_TEST_CASE_BODY(activation)
{
    std::unique_ptr< process::child > child = process::child::fork_capture(
        child_sleep< 60 >);

    datetime::timestamp start = datetime::timestamp::now();
    process::deadline_killer killer(datetime::delta(1, 0), child->pid());
    const process::status status = child->wait();
    killer.unprogram();
    datetime::timestamp end = datetime::timestamp::now();

    ATF_REQUIRE(killer.fired());
    ATF_REQUIRE(end - start <= datetime::delta(10, 0));
    ATF_REQUIRE(status.signaled());
    ATF_REQUIRE_EQ(SIGKILL, status.termsig());
}


ATF_TEST_CASE_WITHOUT_HEAD(no_activation);
ATF_TEST_CASE_BODY(no_activation)
{
    std::unique_ptr< process::child > child = process::child::fork_capture(
        child_sleep< 1 >);

    datetime::timestamp start = datetime::timestamp::now();
    process::deadline_killer killer(datetime::delta(60, 0), child->pid());
    const process::status status = child->wait();
    killer.unprogram();
    datetime::timestamp end = datetime::timestamp::now();

    ATF_REQUIRE(!killer.fired());
    ATF_REQUIRE(end - start <= datetime::delta(10, 0));
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(EXIT_SUCCESS, status.exitstatus());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, activation);
    ATF_ADD_TEST_CASE(tcs, no_activation);
}
