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

#include <atf-c++.hpp>

#include "engine/config.hpp"
#include "engine/requirements.hpp"
#include "utils/config/tree.ipp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/memory.hpp"
#include "utils/passwd.hpp"
#include "utils/units.hpp"

namespace config = utils::config;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace units = utils::units;


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__none);
ATF_TEST_CASE_BODY(check_reqs__none)
{
    const model::metadata md = model::metadata_builder().build();
    ATF_REQUIRE(engine::check_reqs(md, engine::empty_config(), "",
                                   fs::path(".")).empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__allowed_architectures__one_ok);
ATF_TEST_CASE_BODY(check_reqs__allowed_architectures__one_ok)
{
    const model::metadata md = model::metadata_builder()
        .add_allowed_architecture("x86_64")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("architecture", "x86_64");
    user_config.set_string("platform", "");
    ATF_REQUIRE(engine::check_reqs(md, user_config, "", fs::path(".")).empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__allowed_architectures__one_fail);
ATF_TEST_CASE_BODY(check_reqs__allowed_architectures__one_fail)
{
    const model::metadata md = model::metadata_builder()
        .add_allowed_architecture("x86_64")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("architecture", "i386");
    user_config.set_string("platform", "");
    ATF_REQUIRE_MATCH("Current architecture 'i386' not supported",
                      engine::check_reqs(md, user_config, "", fs::path(".")));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__allowed_architectures__many_ok);
ATF_TEST_CASE_BODY(check_reqs__allowed_architectures__many_ok)
{
    const model::metadata md = model::metadata_builder()
        .add_allowed_architecture("x86_64")
        .add_allowed_architecture("i386")
        .add_allowed_architecture("powerpc")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("architecture", "i386");
    user_config.set_string("platform", "");
    ATF_REQUIRE(engine::check_reqs(md, user_config, "", fs::path(".")).empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__allowed_architectures__many_fail);
ATF_TEST_CASE_BODY(check_reqs__allowed_architectures__many_fail)
{
    const model::metadata md = model::metadata_builder()
        .add_allowed_architecture("x86_64")
        .add_allowed_architecture("i386")
        .add_allowed_architecture("powerpc")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("architecture", "arm");
    user_config.set_string("platform", "");
    ATF_REQUIRE_MATCH("Current architecture 'arm' not supported",
                      engine::check_reqs(md, user_config, "", fs::path(".")));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__allowed_platforms__one_ok);
ATF_TEST_CASE_BODY(check_reqs__allowed_platforms__one_ok)
{
    const model::metadata md = model::metadata_builder()
        .add_allowed_platform("amd64")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("architecture", "");
    user_config.set_string("platform", "amd64");
    ATF_REQUIRE(engine::check_reqs(md, user_config, "", fs::path(".")).empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__allowed_platforms__one_fail);
ATF_TEST_CASE_BODY(check_reqs__allowed_platforms__one_fail)
{
    const model::metadata md = model::metadata_builder()
        .add_allowed_platform("amd64")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("architecture", "");
    user_config.set_string("platform", "i386");
    ATF_REQUIRE_MATCH("Current platform 'i386' not supported",
                      engine::check_reqs(md, user_config, "", fs::path(".")));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__allowed_platforms__many_ok);
ATF_TEST_CASE_BODY(check_reqs__allowed_platforms__many_ok)
{
    const model::metadata md = model::metadata_builder()
        .add_allowed_platform("amd64")
        .add_allowed_platform("i386")
        .add_allowed_platform("macppc")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("architecture", "");
    user_config.set_string("platform", "i386");
    ATF_REQUIRE(engine::check_reqs(md, user_config, "", fs::path(".")).empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__allowed_platforms__many_fail);
ATF_TEST_CASE_BODY(check_reqs__allowed_platforms__many_fail)
{
    const model::metadata md = model::metadata_builder()
        .add_allowed_platform("amd64")
        .add_allowed_platform("i386")
        .add_allowed_platform("macppc")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("architecture", "");
    user_config.set_string("platform", "shark");
    ATF_REQUIRE_MATCH("Current platform 'shark' not supported",
                      engine::check_reqs(md, user_config, "", fs::path(".")));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_configs__one_ok);
ATF_TEST_CASE_BODY(check_reqs__required_configs__one_ok)
{
    const model::metadata md = model::metadata_builder()
        .add_required_config("my-var")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("test_suites.suite.aaa", "value1");
    user_config.set_string("test_suites.suite.my-var", "value2");
    user_config.set_string("test_suites.suite.zzz", "value3");
    ATF_REQUIRE(engine::check_reqs(md, user_config, "suite",
                                   fs::path(".")).empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_configs__one_fail);
ATF_TEST_CASE_BODY(check_reqs__required_configs__one_fail)
{
    const model::metadata md = model::metadata_builder()
        .add_required_config("unprivileged_user")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("test_suites.suite.aaa", "value1");
    user_config.set_string("test_suites.suite.my-var", "value2");
    user_config.set_string("test_suites.suite.zzz", "value3");
    ATF_REQUIRE_MATCH("Required configuration property 'unprivileged_user' not "
                      "defined",
                      engine::check_reqs(md, user_config, "suite",
                                         fs::path(".")));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_configs__many_ok);
ATF_TEST_CASE_BODY(check_reqs__required_configs__many_ok)
{
    const model::metadata md = model::metadata_builder()
        .add_required_config("foo")
        .add_required_config("bar")
        .add_required_config("baz")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("test_suites.suite.aaa", "value1");
    user_config.set_string("test_suites.suite.foo", "value2");
    user_config.set_string("test_suites.suite.bar", "value3");
    user_config.set_string("test_suites.suite.baz", "value4");
    user_config.set_string("test_suites.suite.zzz", "value5");
    ATF_REQUIRE(engine::check_reqs(md, user_config, "suite",
                                   fs::path(".")).empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_configs__many_fail);
ATF_TEST_CASE_BODY(check_reqs__required_configs__many_fail)
{
    const model::metadata md = model::metadata_builder()
        .add_required_config("foo")
        .add_required_config("bar")
        .add_required_config("baz")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set_string("test_suites.suite.aaa", "value1");
    user_config.set_string("test_suites.suite.foo", "value2");
    user_config.set_string("test_suites.suite.zzz", "value3");
    ATF_REQUIRE_MATCH("Required configuration property 'bar' not defined",
                      engine::check_reqs(md, user_config, "suite",
                                         fs::path(".")));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_configs__special);
ATF_TEST_CASE_BODY(check_reqs__required_configs__special)
{
    const model::metadata md = model::metadata_builder()
        .add_required_config("unprivileged-user")
        .build();

    config::tree user_config = engine::default_config();
    ATF_REQUIRE_MATCH("Required configuration property 'unprivileged-user' "
                      "not defined",
                      engine::check_reqs(md, user_config, "", fs::path(".")));
    user_config.set< engine::user_node >(
        "unprivileged_user", passwd::user("foo", 1, 2));
    ATF_REQUIRE(engine::check_reqs(md, user_config, "foo",
                                   fs::path(".")).empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_user__root__ok);
ATF_TEST_CASE_BODY(check_reqs__required_user__root__ok)
{
    const model::metadata md = model::metadata_builder()
        .set_required_user("root")
        .build();

    config::tree user_config = engine::default_config();
    ATF_REQUIRE(!user_config.is_set("unprivileged_user"));

    passwd::set_current_user_for_testing(passwd::user("", 0, 1));
    ATF_REQUIRE(engine::check_reqs(md, user_config, "", fs::path(".")).empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_user__root__fail);
ATF_TEST_CASE_BODY(check_reqs__required_user__root__fail)
{
    const model::metadata md = model::metadata_builder()
        .set_required_user("root")
        .build();

    passwd::set_current_user_for_testing(passwd::user("", 123, 1));
    ATF_REQUIRE_MATCH("Requires root privileges",
                      engine::check_reqs(md, engine::empty_config(), "",
                                         fs::path(".")));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_user__unprivileged__same);
ATF_TEST_CASE_BODY(check_reqs__required_user__unprivileged__same)
{
    const model::metadata md = model::metadata_builder()
        .set_required_user("unprivileged")
        .build();

    config::tree user_config = engine::default_config();
    ATF_REQUIRE(!user_config.is_set("unprivileged_user"));

    passwd::set_current_user_for_testing(passwd::user("", 123, 1));
    ATF_REQUIRE(engine::check_reqs(md, user_config, "", fs::path(".")).empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_user__unprivileged__ok);
ATF_TEST_CASE_BODY(check_reqs__required_user__unprivileged__ok)
{
    const model::metadata md = model::metadata_builder()
        .set_required_user("unprivileged")
        .build();

    config::tree user_config = engine::default_config();
    user_config.set< engine::user_node >(
        "unprivileged_user", passwd::user("", 123, 1));

    passwd::set_current_user_for_testing(passwd::user("", 0, 1));
    ATF_REQUIRE(engine::check_reqs(md, user_config, "", fs::path(".")).empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_user__unprivileged__fail);
ATF_TEST_CASE_BODY(check_reqs__required_user__unprivileged__fail)
{
    const model::metadata md = model::metadata_builder()
        .set_required_user("unprivileged")
        .build();

    config::tree user_config = engine::default_config();
    ATF_REQUIRE(!user_config.is_set("unprivileged_user"));

    passwd::set_current_user_for_testing(passwd::user("", 0, 1));
    ATF_REQUIRE_MATCH("Requires.*unprivileged.*unprivileged-user",
                      engine::check_reqs(md, user_config, "", fs::path(".")));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_disk_space__ok);
ATF_TEST_CASE_BODY(check_reqs__required_disk_space__ok)
{
    const model::metadata md = model::metadata_builder()
        .set_required_disk_space(units::bytes::parse("1m"))
        .build();

    ATF_REQUIRE(engine::check_reqs(md, engine::empty_config(), "",
                                   fs::path(".")).empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_disk_space__fail);
ATF_TEST_CASE_BODY(check_reqs__required_disk_space__fail)
{
    const model::metadata md = model::metadata_builder()
        .set_required_disk_space(units::bytes::parse("1000t"))
        .build();

    ATF_REQUIRE_MATCH("Requires 1000.00T .*disk space",
                      engine::check_reqs(md, engine::empty_config(), "",
                                         fs::path(".")));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_files__ok);
ATF_TEST_CASE_BODY(check_reqs__required_files__ok)
{
    const model::metadata md = model::metadata_builder()
        .add_required_file(fs::current_path() / "test-file")
        .build();

    atf::utils::create_file("test-file", "");

    ATF_REQUIRE(engine::check_reqs(md, engine::empty_config(), "",
                                   fs::path(".")).empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_files__fail);
ATF_TEST_CASE_BODY(check_reqs__required_files__fail)
{
    const model::metadata md = model::metadata_builder()
        .add_required_file(fs::path("/non-existent/file"))
        .build();

    ATF_REQUIRE_MATCH("'/non-existent/file' not found$",
                      engine::check_reqs(md, engine::empty_config(), "",
                                         fs::path(".")));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_memory__ok);
ATF_TEST_CASE_BODY(check_reqs__required_memory__ok)
{
    const model::metadata md = model::metadata_builder()
        .set_required_memory(units::bytes::parse("1m"))
        .build();

    ATF_REQUIRE(engine::check_reqs(md, engine::empty_config(), "",
                                   fs::path(".")).empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_memory__fail);
ATF_TEST_CASE_BODY(check_reqs__required_memory__fail)
{
    const model::metadata md = model::metadata_builder()
        .set_required_memory(units::bytes::parse("100t"))
        .build();

    if (utils::physical_memory() == 0)
        skip("Don't know how to query the amount of physical memory");
    ATF_REQUIRE_MATCH("Requires 100.00T .*memory",
                      engine::check_reqs(md, engine::empty_config(), "",
                                         fs::path(".")));
}


ATF_TEST_CASE(check_reqs__required_programs__ok);
ATF_TEST_CASE_HEAD(check_reqs__required_programs__ok)
{
    set_md_var("require.progs", "/bin/ls /bin/mv");
}
ATF_TEST_CASE_BODY(check_reqs__required_programs__ok)
{
    const model::metadata md = model::metadata_builder()
        .add_required_program(fs::path("/bin/ls"))
        .add_required_program(fs::path("foo"))
        .add_required_program(fs::path("/bin/mv"))
        .build();

    fs::mkdir(fs::path("bin"), 0755);
    atf::utils::create_file("bin/foo", "");
    utils::setenv("PATH", (fs::current_path() / "bin").str());

    ATF_REQUIRE(engine::check_reqs(md, engine::empty_config(), "",
                                   fs::path(".")).empty());
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_programs__fail_absolute);
ATF_TEST_CASE_BODY(check_reqs__required_programs__fail_absolute)
{
    const model::metadata md = model::metadata_builder()
        .add_required_program(fs::path("/non-existent/program"))
        .build();

    ATF_REQUIRE_MATCH("'/non-existent/program' not found$",
                      engine::check_reqs(md, engine::empty_config(), "",
                                         fs::path(".")));
}


ATF_TEST_CASE_WITHOUT_HEAD(check_reqs__required_programs__fail_relative);
ATF_TEST_CASE_BODY(check_reqs__required_programs__fail_relative)
{
    const model::metadata md = model::metadata_builder()
        .add_required_program(fs::path("foo"))
        .add_required_program(fs::path("bar"))
        .build();

    fs::mkdir(fs::path("bin"), 0755);
    atf::utils::create_file("bin/foo", "");
    utils::setenv("PATH", (fs::current_path() / "bin").str());

    ATF_REQUIRE_MATCH("'bar' not found in PATH$",
                      engine::check_reqs(md, engine::empty_config(), "",
                                         fs::path(".")));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, check_reqs__none);
    ATF_ADD_TEST_CASE(tcs, check_reqs__allowed_architectures__one_ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__allowed_architectures__one_fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__allowed_architectures__many_ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__allowed_architectures__many_fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__allowed_platforms__one_ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__allowed_platforms__one_fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__allowed_platforms__many_ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__allowed_platforms__many_fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_configs__one_ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_configs__one_fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_configs__many_ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_configs__many_fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_configs__special);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_user__root__ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_user__root__fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_user__unprivileged__same);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_user__unprivileged__ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_user__unprivileged__fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_disk_space__ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_disk_space__fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_files__ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_files__fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_memory__ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_memory__fail);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_programs__ok);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_programs__fail_absolute);
    ATF_ADD_TEST_CASE(tcs, check_reqs__required_programs__fail_relative);
}
