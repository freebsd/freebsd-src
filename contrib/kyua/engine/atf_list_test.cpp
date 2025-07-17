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

#include "engine/atf_list.hpp"

#include <sstream>
#include <string>

#include <atf-c++.hpp>

#include "engine/exceptions.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/types.hpp"
#include "utils/datetime.hpp"
#include "utils/format/containers.ipp"
#include "utils/fs/path.hpp"
#include "utils/units.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace units = utils::units;


ATF_TEST_CASE_WITHOUT_HEAD(parse_atf_metadata__defaults)
ATF_TEST_CASE_BODY(parse_atf_metadata__defaults)
{
    const model::properties_map properties;
    const model::metadata md = engine::parse_atf_metadata(properties);

    const model::metadata exp_md = model::metadata_builder().build();
    ATF_REQUIRE_EQ(exp_md, md);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_atf_metadata__override_all)
ATF_TEST_CASE_BODY(parse_atf_metadata__override_all)
{
    model::properties_map properties;
    properties["descr"] = "Some text";
    properties["has.cleanup"] = "true";
    properties["is.exclusive"] = "true";
    properties["require.arch"] = "i386 x86_64";
    properties["require.config"] = "var1 var2 var3";
    properties["require.diskspace"] = "10g";
    properties["require.files"] = "/file1 /dir/file2";
    properties["require.machine"] = "amd64";
    properties["require.memory"] = "1m";
    properties["require.progs"] = "/bin/ls svn";
    properties["require.user"] = "root";
    properties["timeout"] = "123";
    properties["X-foo"] = "value1";
    properties["X-bar"] = "value2";
    properties["X-baz-www"] = "value3";
    const model::metadata md = engine::parse_atf_metadata(properties);

    const model::metadata exp_md = model::metadata_builder()
        .add_allowed_architecture("i386")
        .add_allowed_architecture("x86_64")
        .add_allowed_platform("amd64")
        .add_custom("foo", "value1")
        .add_custom("bar", "value2")
        .add_custom("baz-www", "value3")
        .add_required_config("var1")
        .add_required_config("var2")
        .add_required_config("var3")
        .add_required_file(fs::path("/file1"))
        .add_required_file(fs::path("/dir/file2"))
        .add_required_program(fs::path("/bin/ls"))
        .add_required_program(fs::path("svn"))
        .set_description("Some text")
        .set_has_cleanup(true)
        .set_is_exclusive(true)
        .set_required_disk_space(units::bytes::parse("10g"))
        .set_required_memory(units::bytes::parse("1m"))
        .set_required_user("root")
        .set_timeout(datetime::delta(123, 0))
        .build();
    ATF_REQUIRE_EQ(exp_md, md);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_atf_metadata__unknown)
ATF_TEST_CASE_BODY(parse_atf_metadata__unknown)
{
    model::properties_map properties;
    properties["foobar"] = "Some text";

    ATF_REQUIRE_THROW_RE(engine::format_error, "Unknown.*property.*'foobar'",
                         engine::parse_atf_metadata(properties));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_atf_list__empty);
ATF_TEST_CASE_BODY(parse_atf_list__empty)
{
    const std::string text = "";
    std::istringstream input(text);
    ATF_REQUIRE_THROW_RE(engine::format_error, "expecting Content-Type",
        engine::parse_atf_list(input));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_atf_list__invalid_header);
ATF_TEST_CASE_BODY(parse_atf_list__invalid_header)
{
    {
        const std::string text =
            "Content-Type: application/X-atf-tp; version=\"1\"\n";
        std::istringstream input(text);
        ATF_REQUIRE_THROW_RE(engine::format_error, "expecting.*blank line",
            engine::parse_atf_list(input));
    }

    {
        const std::string text =
            "Content-Type: application/X-atf-tp; version=\"1\"\nfoo\n";
        std::istringstream input(text);
        ATF_REQUIRE_THROW_RE(engine::format_error, "expecting.*blank line",
            engine::parse_atf_list(input));
    }

    {
        const std::string text =
            "Content-Type: application/X-atf-tp; version=\"2\"\n\n";
        std::istringstream input(text);
        ATF_REQUIRE_THROW_RE(engine::format_error, "expecting Content-Type",
            engine::parse_atf_list(input));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_atf_list__no_test_cases);
ATF_TEST_CASE_BODY(parse_atf_list__no_test_cases)
{
    const std::string text =
        "Content-Type: application/X-atf-tp; version=\"1\"\n\n";
    std::istringstream input(text);
    ATF_REQUIRE_THROW_RE(engine::format_error, "No test cases",
        engine::parse_atf_list(input));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_atf_list__one_test_case_simple);
ATF_TEST_CASE_BODY(parse_atf_list__one_test_case_simple)
{
    const std::string text =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident: test-case\n";
    std::istringstream input(text);
    const model::test_cases_map tests = engine::parse_atf_list(input);

    const model::test_cases_map exp_tests = model::test_cases_map_builder()
        .add("test-case").build();
    ATF_REQUIRE_EQ(exp_tests, tests);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_atf_list__one_test_case_complex);
ATF_TEST_CASE_BODY(parse_atf_list__one_test_case_complex)
{
    const std::string text =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident: first\n"
        "descr: This is the description\n"
        "timeout: 500\n";
    std::istringstream input(text);
    const model::test_cases_map tests = engine::parse_atf_list(input);

    const model::test_cases_map exp_tests = model::test_cases_map_builder()
        .add("first", model::metadata_builder()
             .set_description("This is the description")
             .set_timeout(datetime::delta(500, 0))
             .build())
        .build();
    ATF_REQUIRE_EQ(exp_tests, tests);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_atf_list__one_test_case_invalid_syntax);
ATF_TEST_CASE_BODY(parse_atf_list__one_test_case_invalid_syntax)
{
    const std::string text =
        "Content-Type: application/X-atf-tp; version=\"1\"\n\n"
        "descr: This is the description\n"
        "ident: first\n";
    std::istringstream input(text);
    ATF_REQUIRE_THROW_RE(engine::format_error, "preceeded.*identifier",
        engine::parse_atf_list(input));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_atf_list__one_test_case_invalid_properties);
ATF_TEST_CASE_BODY(parse_atf_list__one_test_case_invalid_properties)
{
    // Inject a single invalid property that makes test_case::from_properties()
    // raise a particular error message so that we can validate that such
    // function was called.  We do intensive testing separately, so it is not
    // necessary to redo it here.
    const std::string text =
        "Content-Type: application/X-atf-tp; version=\"1\"\n\n"
        "ident: first\n"
        "require.progs: bin/ls\n";
    std::istringstream input(text);
    ATF_REQUIRE_THROW_RE(engine::format_error, "Relative path 'bin/ls'",
        engine::parse_atf_list(input));
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_atf_list__many_test_cases);
ATF_TEST_CASE_BODY(parse_atf_list__many_test_cases)
{
    const std::string text =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident: first\n"
        "descr: This is the description\n"
        "\n"
        "ident: second\n"
        "timeout: 500\n"
        "descr: Some text\n"
        "\n"
        "ident: third\n";
    std::istringstream input(text);
    const model::test_cases_map tests = engine::parse_atf_list(input);

    const model::test_cases_map exp_tests = model::test_cases_map_builder()
        .add("first", model::metadata_builder()
             .set_description("This is the description")
             .build())
        .add("second", model::metadata_builder()
             .set_description("Some text")
             .set_timeout(datetime::delta(500, 0))
             .build())
        .add("third")
        .build();
    ATF_REQUIRE_EQ(exp_tests, tests);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_atf_list__is_exclusive_support);
ATF_TEST_CASE_BODY(parse_atf_list__is_exclusive_support)
{
    const std::string text =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident: first\n"
        "is.exclusive: false\n"
        "descr: This is the descr\n"
        "\n"
        "ident: second\n"
        "is.exclusive: true\n"
        "\n"
        "ident: third\n";
    std::istringstream input(text);
    const model::test_cases_map tests = engine::parse_atf_list(input);

    const model::test_cases_map exp_tests = model::test_cases_map_builder()
        .add("first", model::metadata_builder()
             .set_description("This is the descr")
             .build())
        .add("second", model::metadata_builder()
             .set_is_exclusive(true)
             .build())
        .add("third")
        .build();
    ATF_REQUIRE_EQ(exp_tests, tests);
}


ATF_TEST_CASE_WITHOUT_HEAD(parse_atf_list__disk_space_support);
ATF_TEST_CASE_BODY(parse_atf_list__disk_space_support)
{
    const std::string text =
        "Content-Type: application/X-atf-tp; version=\"1\"\n"
        "\n"
        "ident: first\n"
        "require.diskspace: 123M\n";
    std::istringstream input(text);
    const model::test_cases_map tests = engine::parse_atf_list(input);

    const model::test_cases_map exp_tests = model::test_cases_map_builder()
        .add("first", model::metadata_builder()
             .set_required_disk_space(units::bytes::parse("123M"))
             .build())
        .build();
    ATF_REQUIRE_EQ(exp_tests, tests);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, parse_atf_metadata__defaults);
    ATF_ADD_TEST_CASE(tcs, parse_atf_metadata__override_all);
    ATF_ADD_TEST_CASE(tcs, parse_atf_metadata__unknown);

    ATF_ADD_TEST_CASE(tcs, parse_atf_list__empty);
    ATF_ADD_TEST_CASE(tcs, parse_atf_list__invalid_header);
    ATF_ADD_TEST_CASE(tcs, parse_atf_list__no_test_cases);
    ATF_ADD_TEST_CASE(tcs, parse_atf_list__one_test_case_simple);
    ATF_ADD_TEST_CASE(tcs, parse_atf_list__one_test_case_complex);
    ATF_ADD_TEST_CASE(tcs, parse_atf_list__one_test_case_invalid_syntax);
    ATF_ADD_TEST_CASE(tcs, parse_atf_list__one_test_case_invalid_properties);
    ATF_ADD_TEST_CASE(tcs, parse_atf_list__many_test_cases);
    ATF_ADD_TEST_CASE(tcs, parse_atf_list__is_exclusive_support);
    ATF_ADD_TEST_CASE(tcs, parse_atf_list__disk_space_support);
}
