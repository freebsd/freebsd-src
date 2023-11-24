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

#include "utils/sanity.hpp"

extern "C" {
#include <signal.h>
#include <unistd.h>
}

#include <cstdlib>
#include <iostream>

#include <atf-c++.hpp>

#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/process/child.ipp"
#include "utils/process/status.hpp"
#include "utils/test_utils.ipp"

namespace fs = utils::fs;
namespace process = utils::process;


#define FILE_REGEXP __FILE__ ":[0-9]+: "


static const fs::path Stdout_File("stdout.txt");
static const fs::path Stderr_File("stderr.txt");


#if NDEBUG
static bool NDebug = true;
#else
static bool NDebug = false;
#endif


template< typename Function >
static process::status
run_test(Function function)
{
    utils::avoid_coredump_on_crash();

    const process::status status = process::child::fork_files(
        function, Stdout_File, Stderr_File)->wait();
    atf::utils::cat_file(Stdout_File.str(), "Helper stdout: ");
    atf::utils::cat_file(Stderr_File.str(), "Helper stderr: ");
    return status;
}


static void
verify_success(const process::status& status)
{
    ATF_REQUIRE(status.exited());
    ATF_REQUIRE_EQ(EXIT_SUCCESS, status.exitstatus());
    ATF_REQUIRE(atf::utils::grep_file("Before test", Stdout_File.str()));
    ATF_REQUIRE(atf::utils::grep_file("After test", Stdout_File.str()));
}


static void
verify_failed(const process::status& status, const char* type,
              const char* exp_message, const bool check_ndebug)
{
    if (check_ndebug && NDebug) {
        std::cout << "Built with NDEBUG; skipping verification\n";
        verify_success(status);
    } else {
        ATF_REQUIRE(status.signaled());
        ATF_REQUIRE_EQ(SIGABRT, status.termsig());
        ATF_REQUIRE(atf::utils::grep_file("Before test", Stdout_File.str()));
        ATF_REQUIRE(!atf::utils::grep_file("After test", Stdout_File.str()));
        if (exp_message != NULL)
            ATF_REQUIRE(atf::utils::grep_file(F(FILE_REGEXP "%s: %s") %
                                              type % exp_message,
                                              Stderr_File.str()));
        else
            ATF_REQUIRE(atf::utils::grep_file(F(FILE_REGEXP "%s") % type,
                                              Stderr_File.str()));
    }
}


template< bool Expression, bool WithMessage >
static void
do_inv_test(void)
{
    std::cout << "Before test\n";
    if (WithMessage)
        INV_MSG(Expression, "Custom message");
    else
        INV(Expression);
    std::cout << "After test\n";
    std::exit(EXIT_SUCCESS);
}


ATF_TEST_CASE_WITHOUT_HEAD(inv__holds);
ATF_TEST_CASE_BODY(inv__holds)
{
    const process::status status = run_test(do_inv_test< true, false >);
    verify_success(status);
}


ATF_TEST_CASE_WITHOUT_HEAD(inv__triggers_default_message);
ATF_TEST_CASE_BODY(inv__triggers_default_message)
{
    const process::status status = run_test(do_inv_test< false, false >);
    verify_failed(status, "Invariant check failed", "Expression", true);
}


ATF_TEST_CASE_WITHOUT_HEAD(inv__triggers_custom_message);
ATF_TEST_CASE_BODY(inv__triggers_custom_message)
{
    const process::status status = run_test(do_inv_test< false, true >);
    verify_failed(status, "Invariant check failed", "Custom", true);
}


template< bool Expression, bool WithMessage >
static void
do_pre_test(void)
{
    std::cout << "Before test\n";
    if (WithMessage)
        PRE_MSG(Expression, "Custom message");
    else
        PRE(Expression);
    std::cout << "After test\n";
    std::exit(EXIT_SUCCESS);
}


ATF_TEST_CASE_WITHOUT_HEAD(pre__holds);
ATF_TEST_CASE_BODY(pre__holds)
{
    const process::status status = run_test(do_pre_test< true, false >);
    verify_success(status);
}


ATF_TEST_CASE_WITHOUT_HEAD(pre__triggers_default_message);
ATF_TEST_CASE_BODY(pre__triggers_default_message)
{
    const process::status status = run_test(do_pre_test< false, false >);
    verify_failed(status, "Precondition check failed", "Expression", true);
}


ATF_TEST_CASE_WITHOUT_HEAD(pre__triggers_custom_message);
ATF_TEST_CASE_BODY(pre__triggers_custom_message)
{
    const process::status status = run_test(do_pre_test< false, true >);
    verify_failed(status, "Precondition check failed", "Custom", true);
}


template< bool Expression, bool WithMessage >
static void
do_post_test(void)
{
    std::cout << "Before test\n";
    if (WithMessage)
        POST_MSG(Expression, "Custom message");
    else
        POST(Expression);
    std::cout << "After test\n";
    std::exit(EXIT_SUCCESS);
}


ATF_TEST_CASE_WITHOUT_HEAD(post__holds);
ATF_TEST_CASE_BODY(post__holds)
{
    const process::status status = run_test(do_post_test< true, false >);
    verify_success(status);
}


ATF_TEST_CASE_WITHOUT_HEAD(post__triggers_default_message);
ATF_TEST_CASE_BODY(post__triggers_default_message)
{
    const process::status status = run_test(do_post_test< false, false >);
    verify_failed(status, "Postcondition check failed", "Expression", true);
}


ATF_TEST_CASE_WITHOUT_HEAD(post__triggers_custom_message);
ATF_TEST_CASE_BODY(post__triggers_custom_message)
{
    const process::status status = run_test(do_post_test< false, true >);
    verify_failed(status, "Postcondition check failed", "Custom", true);
}


template< bool WithMessage >
static void
do_unreachable_test(void)
{
    std::cout << "Before test\n";
    if (WithMessage)
        UNREACHABLE_MSG("Custom message");
    else
        UNREACHABLE;
    std::cout << "After test\n";
    std::exit(EXIT_SUCCESS);
}


ATF_TEST_CASE_WITHOUT_HEAD(unreachable__default_message);
ATF_TEST_CASE_BODY(unreachable__default_message)
{
    const process::status status = run_test(do_unreachable_test< false >);
    verify_failed(status, "Unreachable point reached", NULL, false);
}


ATF_TEST_CASE_WITHOUT_HEAD(unreachable__custom_message);
ATF_TEST_CASE_BODY(unreachable__custom_message)
{
    const process::status status = run_test(do_unreachable_test< true >);
    verify_failed(status, "Unreachable point reached", "Custom", false);
}


template< int Signo >
static void
do_crash_handler_test(void)
{
    utils::install_crash_handlers("test-log.txt");
    ::kill(::getpid(), Signo);
    std::cout << "After signal\n";
    std::exit(EXIT_FAILURE);
}


template< int Signo >
static void
crash_handler_test(void)
{
    utils::avoid_coredump_on_crash();

    const process::status status = run_test(do_crash_handler_test< Signo >);
    ATF_REQUIRE(status.signaled());
    ATF_REQUIRE_EQ(Signo, status.termsig());
    ATF_REQUIRE(atf::utils::grep_file(F("Fatal signal %s") % Signo,
                                      Stderr_File.str()));
    ATF_REQUIRE(atf::utils::grep_file("Log file is test-log.txt",
                                      Stderr_File.str()));
    ATF_REQUIRE(!atf::utils::grep_file("After signal", Stdout_File.str()));
}


ATF_TEST_CASE_WITHOUT_HEAD(install_crash_handlers__sigabrt);
ATF_TEST_CASE_BODY(install_crash_handlers__sigabrt)
{
    crash_handler_test< SIGABRT >();
}


ATF_TEST_CASE_WITHOUT_HEAD(install_crash_handlers__sigbus);
ATF_TEST_CASE_BODY(install_crash_handlers__sigbus)
{
    crash_handler_test< SIGBUS >();
}


ATF_TEST_CASE_WITHOUT_HEAD(install_crash_handlers__sigsegv);
ATF_TEST_CASE_BODY(install_crash_handlers__sigsegv)
{
    crash_handler_test< SIGSEGV >();
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, inv__holds);
    ATF_ADD_TEST_CASE(tcs, inv__triggers_default_message);
    ATF_ADD_TEST_CASE(tcs, inv__triggers_custom_message);
    ATF_ADD_TEST_CASE(tcs, pre__holds);
    ATF_ADD_TEST_CASE(tcs, pre__triggers_default_message);
    ATF_ADD_TEST_CASE(tcs, pre__triggers_custom_message);
    ATF_ADD_TEST_CASE(tcs, post__holds);
    ATF_ADD_TEST_CASE(tcs, post__triggers_default_message);
    ATF_ADD_TEST_CASE(tcs, post__triggers_custom_message);
    ATF_ADD_TEST_CASE(tcs, unreachable__default_message);
    ATF_ADD_TEST_CASE(tcs, unreachable__custom_message);

    ATF_ADD_TEST_CASE(tcs, install_crash_handlers__sigabrt);
    ATF_ADD_TEST_CASE(tcs, install_crash_handlers__sigbus);
    ATF_ADD_TEST_CASE(tcs, install_crash_handlers__sigsegv);
}
