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

#include "cli/cmd_config.hpp"

#include <cstdlib>

#include <atf-c++.hpp>

#include "cli/common.ipp"
#include "engine/config.hpp"
#include "utils/cmdline/globals.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/cmdline/ui_mock.hpp"
#include "utils/config/tree.ipp"
#include "utils/optional.ipp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;

using cli::cmd_config;
using utils::none;


namespace {


/// Instantiates a fake user configuration for testing purposes.
///
/// The user configuration is populated with a collection of test-suite
/// properties and some hardcoded values for the generic configuration options.
///
/// \return A new user configuration object.
static config::tree
fake_config(void)
{
    config::tree user_config = engine::default_config();
    user_config.set_string("architecture", "the-architecture");
    user_config.set_string("execenvs", "the-env");
    user_config.set_string("parallelism", "128");
    user_config.set_string("platform", "the-platform");
    //user_config.set_string("unprivileged_user", "");
    user_config.set_string("test_suites.foo.bar", "first");
    user_config.set_string("test_suites.foo.baz", "second");
    return user_config;
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(all);
ATF_TEST_CASE_BODY(all)
{
    cmdline::args_vector args;
    args.push_back("config");

    cmd_config cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args, fake_config()));

    ATF_REQUIRE_EQ(6, ui.out_log().size());
    ATF_REQUIRE_EQ("architecture = the-architecture", ui.out_log()[0]);
    ATF_REQUIRE_EQ("execenvs = the-env", ui.out_log()[1]);
    ATF_REQUIRE_EQ("parallelism = 128", ui.out_log()[2]);
    ATF_REQUIRE_EQ("platform = the-platform", ui.out_log()[3]);
    ATF_REQUIRE_EQ("test_suites.foo.bar = first", ui.out_log()[4]);
    ATF_REQUIRE_EQ("test_suites.foo.baz = second", ui.out_log()[5]);
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(some__ok);
ATF_TEST_CASE_BODY(some__ok)
{
    cmdline::args_vector args;
    args.push_back("config");
    args.push_back("platform");
    args.push_back("test_suites.foo.baz");

    cmd_config cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_SUCCESS, cmd.main(&ui, args, fake_config()));

    ATF_REQUIRE_EQ(2, ui.out_log().size());
    ATF_REQUIRE_EQ("platform = the-platform", ui.out_log()[0]);
    ATF_REQUIRE_EQ("test_suites.foo.baz = second", ui.out_log()[1]);
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(some__fail);
ATF_TEST_CASE_BODY(some__fail)
{
    cmdline::args_vector args;
    args.push_back("config");
    args.push_back("platform");
    args.push_back("unknown");
    args.push_back("test_suites.foo.baz");

    cmdline::init("progname");

    cmd_config cmd;
    cmdline::ui_mock ui;
    ATF_REQUIRE_EQ(EXIT_FAILURE, cmd.main(&ui, args, fake_config()));

    ATF_REQUIRE_EQ(2, ui.out_log().size());
    ATF_REQUIRE_EQ("platform = the-platform", ui.out_log()[0]);
    ATF_REQUIRE_EQ("test_suites.foo.baz = second", ui.out_log()[1]);
    ATF_REQUIRE_EQ(1, ui.err_log().size());
    ATF_REQUIRE(atf::utils::grep_string("unknown.*not defined",
                                        ui.err_log()[0]));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, all);
    ATF_ADD_TEST_CASE(tcs, some__ok);
    ATF_ADD_TEST_CASE(tcs, some__fail);
}
