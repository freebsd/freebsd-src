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

#include "cli/cmd_list.hpp"

#include <atf-c++.hpp>

#include "model/metadata.hpp"
#include "model/test_program.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/parser.hpp"
#include "utils/cmdline/ui_mock.hpp"
#include "utils/fs/path.hpp"

namespace cmdline = utils::cmdline;
namespace fs = utils::fs;


ATF_TEST_CASE_WITHOUT_HEAD(list_test_case__no_verbose);
ATF_TEST_CASE_BODY(list_test_case__no_verbose)
{
    const model::metadata md = model::metadata_builder()
        .set_description("This should not be shown")
        .build();
    const model::test_program test_program = model::test_program_builder(
        "mock", fs::path("the/test-program"), fs::path("root"), "suite")
        .add_test_case("abc", md)
        .set_metadata(md)
        .build();

    cmdline::ui_mock ui;
    cli::detail::list_test_case(&ui, false, test_program, "abc");
    ATF_REQUIRE_EQ(1, ui.out_log().size());
    ATF_REQUIRE_EQ("the/test-program:abc", ui.out_log()[0]);
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(list_test_case__verbose__no_properties);
ATF_TEST_CASE_BODY(list_test_case__verbose__no_properties)
{
    const model::test_program test_program = model::test_program_builder(
        "mock", fs::path("hello/world"), fs::path("root"), "the-suite")
        .add_test_case("my_name")
        .build();

    cmdline::ui_mock ui;
    cli::detail::list_test_case(&ui, true, test_program, "my_name");
    ATF_REQUIRE_EQ(1, ui.out_log().size());
    ATF_REQUIRE_EQ("hello/world:my_name (the-suite)", ui.out_log()[0]);
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(list_test_case__verbose__some_properties);
ATF_TEST_CASE_BODY(list_test_case__verbose__some_properties)
{
    const model::metadata md = model::metadata_builder()
        .add_custom("my-property", "value")
        .set_description("Some description")
        .set_has_cleanup(true)
        .build();
    const model::test_program test_program = model::test_program_builder(
        "mock", fs::path("hello/world"), fs::path("root"), "the-suite")
        .add_test_case("my_name", md)
        .set_metadata(md)
        .build();

    cmdline::ui_mock ui;
    cli::detail::list_test_case(&ui, true, test_program, "my_name");
    ATF_REQUIRE_EQ(4, ui.out_log().size());
    ATF_REQUIRE_EQ("hello/world:my_name (the-suite)", ui.out_log()[0]);
    ATF_REQUIRE_EQ("    custom.my-property = value", ui.out_log()[1]);
    ATF_REQUIRE_EQ("    description = Some description", ui.out_log()[2]);
    ATF_REQUIRE_EQ("    has_cleanup = true", ui.out_log()[3]);
    ATF_REQUIRE(ui.err_log().empty());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, list_test_case__no_verbose);
    ATF_ADD_TEST_CASE(tcs, list_test_case__verbose__no_properties);
    ATF_ADD_TEST_CASE(tcs, list_test_case__verbose__some_properties);

    // Tests for cmd_list::run are located in integration/cmd_list_test.
}
