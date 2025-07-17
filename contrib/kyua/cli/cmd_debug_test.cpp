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

#include "cli/cmd_debug.hpp"

#include <stdexcept>

#include <atf-c++.hpp>

#include "cli/common.ipp"
#include "engine/config.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/cmdline/ui_mock.hpp"
#include "utils/config/tree.ipp"

namespace cmdline = utils::cmdline;


ATF_TEST_CASE_WITHOUT_HEAD(invalid_filter);
ATF_TEST_CASE_BODY(invalid_filter)
{
    cmdline::args_vector args;
    args.push_back("debug");
    args.push_back("incorrect:");

    cli::cmd_debug cmd;
    cmdline::ui_mock ui;
    // TODO(jmmv): This error should really be cmdline::usage_error.
    ATF_REQUIRE_THROW_RE(std::runtime_error, "Test case.*'incorrect:'.*empty",
                         cmd.main(&ui, args, engine::default_config()));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(filter_without_test_case);
ATF_TEST_CASE_BODY(filter_without_test_case)
{
    cmdline::args_vector args;
    args.push_back("debug");
    args.push_back("program");

    cli::cmd_debug cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_THROW_RE(cmdline::error, "'program'.*not a test case",
                         cmd.main(&ui, args, engine::default_config()));
    ATF_REQUIRE(ui.out_log().empty());
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, invalid_filter);
    ATF_ADD_TEST_CASE(tcs, filter_without_test_case);
}
