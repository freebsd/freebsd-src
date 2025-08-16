// Copyright 2012 The Kyua Authors.
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

#include "model/metadata.hpp"

#include <sstream>

#include <atf-c++.hpp>

#include "model/types.hpp"
#include "utils/datetime.hpp"
#include "utils/format/containers.ipp"
#include "utils/fs/path.hpp"
#include "utils/units.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace units = utils::units;


ATF_TEST_CASE_WITHOUT_HEAD(defaults);
ATF_TEST_CASE_BODY(defaults)
{
    const model::metadata md = model::metadata_builder().build();
    ATF_REQUIRE(md.allowed_architectures().empty());
    ATF_REQUIRE(md.allowed_platforms().empty());
    ATF_REQUIRE(md.allowed_platforms().empty());
    ATF_REQUIRE(md.custom().empty());
    ATF_REQUIRE(md.description().empty());
    ATF_REQUIRE(!md.has_cleanup());
    ATF_REQUIRE(!md.is_exclusive());
    ATF_REQUIRE(md.required_configs().empty());
    ATF_REQUIRE_EQ(units::bytes(0), md.required_disk_space());
    ATF_REQUIRE(md.required_files().empty());
    ATF_REQUIRE(md.required_kmods().empty());
    ATF_REQUIRE_EQ(units::bytes(0), md.required_memory());
    ATF_REQUIRE(md.required_programs().empty());
    ATF_REQUIRE(md.required_user().empty());
    ATF_REQUIRE(datetime::delta(300, 0) == md.timeout());
}


ATF_TEST_CASE_WITHOUT_HEAD(add);
ATF_TEST_CASE_BODY(add)
{
    model::strings_set architectures;
    architectures.insert("1-architecture");
    architectures.insert("2-architecture");

    model::strings_set platforms;
    platforms.insert("1-platform");
    platforms.insert("2-platform");

    model::properties_map custom;
    custom["1-custom"] = "first";
    custom["2-custom"] = "second";

    model::strings_set configs;
    configs.insert("1-config");
    configs.insert("2-config");

    model::paths_set files;
    files.insert(fs::path("1-file"));
    files.insert(fs::path("2-file"));

    model::paths_set programs;
    programs.insert(fs::path("1-program"));
    programs.insert(fs::path("2-program"));

    const model::metadata md = model::metadata_builder()
        .add_allowed_architecture("1-architecture")
        .add_allowed_platform("1-platform")
        .add_custom("1-custom", "first")
        .add_custom("2-custom", "second")
        .add_required_config("1-config")
        .add_required_file(fs::path("1-file"))
        .add_required_program(fs::path("1-program"))
        .add_allowed_architecture("2-architecture")
        .add_allowed_platform("2-platform")
        .add_required_config("2-config")
        .add_required_file(fs::path("2-file"))
        .add_required_program(fs::path("2-program"))
        .build();

    ATF_REQUIRE(architectures == md.allowed_architectures());
    ATF_REQUIRE(platforms == md.allowed_platforms());
    ATF_REQUIRE(custom == md.custom());
    ATF_REQUIRE(configs == md.required_configs());
    ATF_REQUIRE(files == md.required_files());
    ATF_REQUIRE(programs == md.required_programs());
}


ATF_TEST_CASE_WITHOUT_HEAD(copy);
ATF_TEST_CASE_BODY(copy)
{
    const model::metadata md1 = model::metadata_builder()
        .add_allowed_architecture("1-architecture")
        .add_allowed_platform("1-platform")
        .build();

    const model::metadata md2 = model::metadata_builder(md1)
        .add_allowed_architecture("2-architecture")
        .build();

    ATF_REQUIRE_EQ(1, md1.allowed_architectures().size());
    ATF_REQUIRE_EQ(2, md2.allowed_architectures().size());
    ATF_REQUIRE_EQ(1, md1.allowed_platforms().size());
    ATF_REQUIRE_EQ(1, md2.allowed_platforms().size());
}


ATF_TEST_CASE_WITHOUT_HEAD(apply_overrides);
ATF_TEST_CASE_BODY(apply_overrides)
{
    const model::metadata md1 = model::metadata_builder()
        .add_allowed_architecture("1-architecture")
        .add_allowed_platform("1-platform")
        .set_description("Explicit description")
        .build();

    const model::metadata md2 = model::metadata_builder()
        .add_allowed_architecture("2-architecture")
        .set_description("")
        .set_timeout(datetime::delta(500, 0))
        .build();

    const model::metadata merge_1_2 = model::metadata_builder()
        .add_allowed_architecture("2-architecture")
        .add_allowed_platform("1-platform")
        .set_description("")
        .set_timeout(datetime::delta(500, 0))
        .build();
    ATF_REQUIRE_EQ(merge_1_2, md1.apply_overrides(md2));

    const model::metadata merge_2_1 = model::metadata_builder()
        .add_allowed_architecture("1-architecture")
        .add_allowed_platform("1-platform")
        .set_description("Explicit description")
        .set_timeout(datetime::delta(500, 0))
        .build();
    ATF_REQUIRE_EQ(merge_2_1, md2.apply_overrides(md1));
}


ATF_TEST_CASE_WITHOUT_HEAD(override_all_with_setters);
ATF_TEST_CASE_BODY(override_all_with_setters)
{
    model::strings_set architectures;
    architectures.insert("the-architecture");

    model::strings_set platforms;
    platforms.insert("the-platforms");

    model::properties_map custom;
    custom["first"] = "hello";
    custom["second"] = "bye";

    const std::string description = "Some long text";

    model::strings_set configs;
    configs.insert("the-configs");

    model::paths_set files;
    files.insert(fs::path("the-files"));

    const units::bytes disk_space(6789);

    const units::bytes memory(12345);

    model::paths_set programs;
    programs.insert(fs::path("the-programs"));

    const std::string user = "root";

    const datetime::delta timeout(123, 0);

    const model::metadata md = model::metadata_builder()
        .set_allowed_architectures(architectures)
        .set_allowed_platforms(platforms)
        .set_custom(custom)
        .set_description(description)
        .set_has_cleanup(true)
        .set_is_exclusive(true)
        .set_required_configs(configs)
        .set_required_disk_space(disk_space)
        .set_required_files(files)
        .set_required_memory(memory)
        .set_required_programs(programs)
        .set_required_user(user)
        .set_timeout(timeout)
        .build();

    ATF_REQUIRE(architectures == md.allowed_architectures());
    ATF_REQUIRE(platforms == md.allowed_platforms());
    ATF_REQUIRE(custom == md.custom());
    ATF_REQUIRE_EQ(description, md.description());
    ATF_REQUIRE(md.has_cleanup());
    ATF_REQUIRE(md.is_exclusive());
    ATF_REQUIRE(configs == md.required_configs());
    ATF_REQUIRE_EQ(disk_space, md.required_disk_space());
    ATF_REQUIRE(files == md.required_files());
    ATF_REQUIRE_EQ(memory, md.required_memory());
    ATF_REQUIRE(programs == md.required_programs());
    ATF_REQUIRE_EQ(user, md.required_user());
    ATF_REQUIRE(timeout == md.timeout());
}


ATF_TEST_CASE_WITHOUT_HEAD(override_all_with_set_string);
ATF_TEST_CASE_BODY(override_all_with_set_string)
{
    model::strings_set architectures;
    architectures.insert("a1");
    architectures.insert("a2");

    model::strings_set platforms;
    platforms.insert("p1");
    platforms.insert("p2");

    model::properties_map custom;
    custom["user-defined"] = "the-value";

    const std::string description = "Another long text";

    model::strings_set configs;
    configs.insert("config-var");

    model::paths_set files;
    files.insert(fs::path("plain"));
    files.insert(fs::path("/absolute/path"));

    const units::bytes disk_space(
        static_cast< uint64_t >(16) * 1024 * 1024 * 1024);

    const units::bytes memory(1024 * 1024);

    model::paths_set programs;
    programs.insert(fs::path("program"));
    programs.insert(fs::path("/absolute/prog"));

    const std::string user = "unprivileged";

    const datetime::delta timeout(45, 0);

    const model::metadata md = model::metadata_builder()
        .set_string("allowed_architectures", "a1 a2")
        .set_string("allowed_platforms", "p1 p2")
        .set_string("custom.user-defined", "the-value")
        .set_string("description", "Another long text")
        .set_string("has_cleanup", "true")
        .set_string("is_exclusive", "true")
        .set_string("required_configs", "config-var")
        .set_string("required_disk_space", "16G")
        .set_string("required_files", "plain /absolute/path")
        .set_string("required_memory", "1M")
        .set_string("required_programs", "program /absolute/prog")
        .set_string("required_user", "unprivileged")
        .set_string("timeout", "45")
        .build();

    ATF_REQUIRE(architectures == md.allowed_architectures());
    ATF_REQUIRE(platforms == md.allowed_platforms());
    ATF_REQUIRE(custom == md.custom());
    ATF_REQUIRE_EQ(description, md.description());
    ATF_REQUIRE(md.has_cleanup());
    ATF_REQUIRE(md.is_exclusive());
    ATF_REQUIRE(configs == md.required_configs());
    ATF_REQUIRE_EQ(disk_space, md.required_disk_space());
    ATF_REQUIRE(files == md.required_files());
    ATF_REQUIRE_EQ(memory, md.required_memory());
    ATF_REQUIRE(programs == md.required_programs());
    ATF_REQUIRE_EQ(user, md.required_user());
    ATF_REQUIRE(timeout == md.timeout());
}


ATF_TEST_CASE_WITHOUT_HEAD(to_properties);
ATF_TEST_CASE_BODY(to_properties)
{
    const model::metadata md = model::metadata_builder()
        .add_allowed_architecture("abc")
        .add_required_file(fs::path("foo"))
        .add_required_file(fs::path("bar"))
        .set_required_memory(units::bytes(1024))
        .add_custom("foo", "bar")
        .build();

    model::properties_map props;
    props["allowed_architectures"] = "abc";
    props["allowed_platforms"] = "";
    props["custom.foo"] = "bar";
    props["description"] = "";
    props["execenv"] = "";
    props["execenv_jail_params"] = "";
    props["has_cleanup"] = "false";
    props["is_exclusive"] = "false";
    props["required_configs"] = "";
    props["required_disk_space"] = "0";
    props["required_files"] = "bar foo";
    props["required_kmods"] = "";
    props["required_memory"] = "1.00K";
    props["required_programs"] = "";
    props["required_user"] = "";
    props["timeout"] = "300";
    ATF_REQUIRE_EQ(props, md.to_properties());
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne__empty);
ATF_TEST_CASE_BODY(operators_eq_and_ne__empty)
{
    const model::metadata md1 = model::metadata_builder().build();
    const model::metadata md2 = model::metadata_builder().build();
    ATF_REQUIRE(  md1 == md2);
    ATF_REQUIRE(!(md1 != md2));
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne__copy);
ATF_TEST_CASE_BODY(operators_eq_and_ne__copy)
{
    const model::metadata md1 = model::metadata_builder()
        .add_custom("foo", "bar")
        .build();
    const model::metadata md2 = md1;
    ATF_REQUIRE(  md1 == md2);
    ATF_REQUIRE(!(md1 != md2));
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne__equal);
ATF_TEST_CASE_BODY(operators_eq_and_ne__equal)
{
    const model::metadata md1 = model::metadata_builder()
        .add_allowed_architecture("a")
        .add_allowed_architecture("b")
        .add_custom("foo", "bar")
        .build();
    const model::metadata md2 = model::metadata_builder()
        .add_allowed_architecture("b")
        .add_allowed_architecture("a")
        .add_custom("foo", "bar")
        .build();
    ATF_REQUIRE(  md1 == md2);
    ATF_REQUIRE(!(md1 != md2));
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne__equal_overriden_defaults);
ATF_TEST_CASE_BODY(operators_eq_and_ne__equal_overriden_defaults)
{
    const model::metadata defaults = model::metadata_builder().build();

    const model::metadata md1 = model::metadata_builder()
        .add_allowed_architecture("a")
        .build();
    const model::metadata md2 = model::metadata_builder()
        .add_allowed_architecture("a")
        .set_timeout(defaults.timeout())
        .build();
    ATF_REQUIRE(  md1 == md2);
    ATF_REQUIRE(!(md1 != md2));
}


ATF_TEST_CASE_WITHOUT_HEAD(operators_eq_and_ne__different);
ATF_TEST_CASE_BODY(operators_eq_and_ne__different)
{
    const model::metadata md1 = model::metadata_builder()
        .add_custom("foo", "bar")
        .build();
    const model::metadata md2 = model::metadata_builder()
        .add_custom("foo", "bar")
        .add_custom("baz", "foo bar")
        .build();
    ATF_REQUIRE(!(md1 == md2));
    ATF_REQUIRE(  md1 != md2);
}


ATF_TEST_CASE_WITHOUT_HEAD(output__defaults);
ATF_TEST_CASE_BODY(output__defaults)
{
    std::ostringstream str;
    str << model::metadata_builder().build();
    ATF_REQUIRE_EQ("metadata{allowed_architectures='', allowed_platforms='', "
                   "description='', execenv='', execenv_jail_params='', "
                   "has_cleanup='false', is_exclusive='false', "
                   "required_configs='', "
                   "required_disk_space='0', required_files='', "
                   "required_kmods='', required_memory='0', "
                   "required_programs='', required_user='', timeout='300'}",
                   str.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(output__some_values);
ATF_TEST_CASE_BODY(output__some_values)
{
    std::ostringstream str;
    str << model::metadata_builder()
        .add_allowed_architecture("abc")
        .add_required_file(fs::path("foo"))
        .add_required_file(fs::path("bar"))
        .set_is_exclusive(true)
        .set_required_memory(units::bytes(1024))
        .build();
    ATF_REQUIRE_EQ(
        "metadata{allowed_architectures='abc', allowed_platforms='', "
        "description='', execenv='', execenv_jail_params='', "
        "has_cleanup='false', is_exclusive='true', "
        "required_configs='', "
        "required_disk_space='0', required_files='bar foo', "
        "required_kmods='', required_memory='1.00K', "
        "required_programs='', required_user='', timeout='300'}",
        str.str());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, defaults);
    ATF_ADD_TEST_CASE(tcs, add);
    ATF_ADD_TEST_CASE(tcs, copy);
    ATF_ADD_TEST_CASE(tcs, apply_overrides);
    ATF_ADD_TEST_CASE(tcs, override_all_with_setters);
    ATF_ADD_TEST_CASE(tcs, override_all_with_set_string);
    ATF_ADD_TEST_CASE(tcs, to_properties);

    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne__empty);
    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne__copy);
    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne__equal);
    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne__equal_overriden_defaults);
    ATF_ADD_TEST_CASE(tcs, operators_eq_and_ne__different);

    ATF_ADD_TEST_CASE(tcs, output__defaults);
    ATF_ADD_TEST_CASE(tcs, output__some_values);

    // TODO(jmmv): Add tests for error conditions (invalid keys and invalid
    // values).
}
