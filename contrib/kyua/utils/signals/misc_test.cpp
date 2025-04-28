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

#include "utils/signals/misc.hpp"

extern "C" {
#include <signal.h>
#include <unistd.h>
}

#include <cstdlib>

#include <atf-c++.hpp>

#include "utils/defs.hpp"
#include "utils/fs/path.hpp"
#include "utils/process/child.ipp"
#include "utils/process/status.hpp"
#include "utils/signals/exceptions.hpp"

namespace fs = utils::fs;
namespace process = utils::process;
namespace signals = utils::signals;


namespace {


static void program_reset_raise(void) UTILS_NORETURN;


/// Body of a subprocess that tests the signals::reset function.
///
/// This function programs a signal to be ignored, then uses signal::reset to
/// bring it back to its default handler and then delivers the signal to self.
/// The default behavior of the signal is for the process to die, so this
/// function should never return correctly (and thus the child process should
/// always die due to a signal if all goes well).
static void
program_reset_raise(void)
{
    struct ::sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (::sigaction(SIGUSR1, &sa, NULL) == -1)
        std::exit(EXIT_FAILURE);

    signals::reset(SIGUSR1);
    ::kill(::getpid(), SIGUSR1);

    // Should not be reached, but we do not assert this condition because we
    // want to exit cleanly if the signal does not abort our execution to let
    // the parent easily know what happened.
    std::exit(EXIT_SUCCESS);
}


/// Body of a subprocess that executes the signals::reset_all function.
///
/// The process exits with success if the function worked, or with a failure if
/// an error is reported.  No signals are tested.
static void
run_reset_all(void)
{
    const bool ok = signals::reset_all();
    std::exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(reset__ok);
ATF_TEST_CASE_BODY(reset__ok)
{
    std::unique_ptr< process::child > child = process::child::fork_files(
        program_reset_raise, fs::path("/dev/stdout"), fs::path("/dev/stderr"));
    process::status status = child->wait();
    ATF_REQUIRE(status.signaled());
    ATF_REQUIRE_EQ(SIGUSR1, status.termsig());
}


ATF_TEST_CASE_WITHOUT_HEAD(reset__invalid);
ATF_TEST_CASE_BODY(reset__invalid)
{
    ATF_REQUIRE_THROW(signals::system_error, signals::reset(-1));
}


ATF_TEST_CASE_WITHOUT_HEAD(reset_all);
ATF_TEST_CASE_BODY(reset_all)
{
    std::unique_ptr< process::child > child = process::child::fork_files(
        run_reset_all, fs::path("/dev/stdout"), fs::path("/dev/stderr"));
    process::status status = child->wait();
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(EXIT_SUCCESS, status.exitstatus());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, reset__ok);
    ATF_ADD_TEST_CASE(tcs, reset__invalid);
    ATF_ADD_TEST_CASE(tcs, reset_all);
}
