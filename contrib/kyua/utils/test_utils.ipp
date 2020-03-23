// Copyright 2016 The Kyua Authors.
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

/// \file utils/test_utils.ipp
/// Provides test-only convenience utilities.

#if defined(UTILS_TEST_UTILS_IPP)
#   error "utils/test_utils.hpp can only be included once"
#endif
#define UTILS_TEST_UTILS_IPP

extern "C" {
#include <sys/resource.h>
}

#include <cstdlib>
#include <iostream>

#include <atf-c++.hpp>

#include "utils/defs.hpp"
#include "utils/stacktrace.hpp"
#include "utils/text/operations.ipp"

namespace utils {


/// Tries to prevent dumping core if we do not need one on a crash.
///
/// This is a best-effort operation provided so that tests that will cause
/// a crash do not collect an unnecessary core dump, which can be slow on
/// some systems (e.g. on macOS).
inline void
avoid_coredump_on_crash(void)
{
    struct ::rlimit rl;
    rl.rlim_cur = 0;
    rl.rlim_max = 0;
    if (::setrlimit(RLIMIT_CORE, &rl) == -1) {
        std::cerr << "Failed to zero core size limit; may dump core\n";
    }
}


inline void abort_without_coredump(void) UTILS_NORETURN;


/// Aborts execution and tries to not dump core.
///
/// The coredump avoidance is a best-effort operation provided so that tests
/// that will cause a crash do not collect an unnecessary core dump, which can
/// be slow on some systems (e.g. on macOS).
inline void
abort_without_coredump(void)
{
    avoid_coredump_on_crash();
    std::abort();
}


/// Skips the test if coredump tests have been disabled by the user.
///
/// \param tc The calling test.
inline void
require_run_coredump_tests(const atf::tests::tc* tc)
{
    if (tc->has_config_var("run_coredump_tests") &&
        !text::to_type< bool >(tc->get_config_var("run_coredump_tests"))) {
        tc->skip("run_coredump_tests=false; not running test");
    }
}


/// Prepares the test so that it can dump core, or skips it otherwise.
///
/// \param tc The calling test.
inline void
prepare_coredump_test(const atf::tests::tc* tc)
{
    require_run_coredump_tests(tc);

    if (!unlimit_core_size()) {
        tc->skip("Cannot unlimit the core file size; check limits manually");
    }
}


}  // namespace utils
