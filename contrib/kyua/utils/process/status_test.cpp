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

#include "utils/process/status.hpp"

extern "C" {
#include <sys/wait.h>

#include <signal.h>
#include <unistd.h>
}

#include <cstdlib>

#include <atf-c++.hpp>

#include "utils/test_utils.ipp"

using utils::process::status;


namespace {


/// Body of a subprocess that exits with a particular exit status.
///
/// \tparam ExitStatus The status to exit with.
template< int ExitStatus >
void child_exit(void)
{
    std::exit(ExitStatus);
}


/// Body of a subprocess that sends a particular signal to itself.
///
/// \tparam Signo The signal to send to self.
template< int Signo >
void child_signal(void)
{
    ::kill(::getpid(), Signo);
}


/// Spawns a process and waits for completion.
///
/// \param hook The function to run within the child.  Should not return.
///
/// \return The termination status of the spawned subprocess.
status
fork_and_wait(void (*hook)(void))
{
    pid_t pid = ::fork();
    ATF_REQUIRE(pid != -1);
    if (pid == 0) {
        hook();
        std::abort();
    } else {
        int stat_loc;
        ATF_REQUIRE(::waitpid(pid, &stat_loc, 0) != -1);
        const status s = status(pid, stat_loc);
        ATF_REQUIRE_EQ(pid, s.dead_pid());
        return s;
    }
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(fake_exited)
ATF_TEST_CASE_BODY(fake_exited)
{
    const status fake = status::fake_exited(123);
    ATF_REQUIRE_EQ(-1, fake.dead_pid());
    ATF_REQUIRE(fake.exited());
    ATF_REQUIRE_EQ(123, fake.exitstatus());
    ATF_REQUIRE(!fake.signaled());
}


ATF_TEST_CASE_WITHOUT_HEAD(fake_signaled)
ATF_TEST_CASE_BODY(fake_signaled)
{
    const status fake = status::fake_signaled(567, true);
    ATF_REQUIRE_EQ(-1, fake.dead_pid());
    ATF_REQUIRE(!fake.exited());
    ATF_REQUIRE(fake.signaled());
    ATF_REQUIRE_EQ(567, fake.termsig());
    ATF_REQUIRE(fake.coredump());
}


ATF_TEST_CASE_WITHOUT_HEAD(output__exitstatus);
ATF_TEST_CASE_BODY(output__exitstatus)
{
    const status fake = status::fake_exited(123);
    std::ostringstream str;
    str << fake;
    ATF_REQUIRE_EQ("status{exitstatus=123}", str.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(output__signaled_without_core);
ATF_TEST_CASE_BODY(output__signaled_without_core)
{
    const status fake = status::fake_signaled(8, false);
    std::ostringstream str;
    str << fake;
    ATF_REQUIRE_EQ("status{termsig=8, coredump=false}", str.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(output__signaled_with_core);
ATF_TEST_CASE_BODY(output__signaled_with_core)
{
    const status fake = status::fake_signaled(9, true);
    std::ostringstream str;
    str << fake;
    ATF_REQUIRE_EQ("status{termsig=9, coredump=true}", str.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__exited);
ATF_TEST_CASE_BODY(integration__exited)
{
    const status exit_success = fork_and_wait(child_exit< EXIT_SUCCESS >);
    ATF_REQUIRE(exit_success.exited());
    ATF_REQUIRE_EQ(EXIT_SUCCESS, exit_success.exitstatus());
    ATF_REQUIRE(!exit_success.signaled());

    const status exit_failure = fork_and_wait(child_exit< EXIT_FAILURE >);
    ATF_REQUIRE(exit_failure.exited());
    ATF_REQUIRE_EQ(EXIT_FAILURE, exit_failure.exitstatus());
    ATF_REQUIRE(!exit_failure.signaled());
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__signaled);
ATF_TEST_CASE_BODY(integration__signaled)
{
    const status sigterm = fork_and_wait(child_signal< SIGTERM >);
    ATF_REQUIRE(!sigterm.exited());
    ATF_REQUIRE(sigterm.signaled());
    ATF_REQUIRE_EQ(SIGTERM, sigterm.termsig());
    ATF_REQUIRE(!sigterm.coredump());

    const status sigkill = fork_and_wait(child_signal< SIGKILL >);
    ATF_REQUIRE(!sigkill.exited());
    ATF_REQUIRE(sigkill.signaled());
    ATF_REQUIRE_EQ(SIGKILL, sigkill.termsig());
    ATF_REQUIRE(!sigkill.coredump());
}


ATF_TEST_CASE_WITHOUT_HEAD(integration__coredump);
ATF_TEST_CASE_BODY(integration__coredump)
{
    utils::prepare_coredump_test(this);

    const status coredump = fork_and_wait(child_signal< SIGQUIT >);
    ATF_REQUIRE(!coredump.exited());
    ATF_REQUIRE(coredump.signaled());
    ATF_REQUIRE_EQ(SIGQUIT, coredump.termsig());
#if !defined(WCOREDUMP)
    expect_fail("Platform does not support checking for coredump");
#endif
    ATF_REQUIRE(coredump.coredump());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, fake_exited);
    ATF_ADD_TEST_CASE(tcs, fake_signaled);

    ATF_ADD_TEST_CASE(tcs, output__exitstatus);
    ATF_ADD_TEST_CASE(tcs, output__signaled_without_core);
    ATF_ADD_TEST_CASE(tcs, output__signaled_with_core);

    ATF_ADD_TEST_CASE(tcs, integration__exited);
    ATF_ADD_TEST_CASE(tcs, integration__signaled);
    ATF_ADD_TEST_CASE(tcs, integration__coredump);
}
