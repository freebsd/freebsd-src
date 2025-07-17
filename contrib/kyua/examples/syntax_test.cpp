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

extern "C" {
#include <unistd.h>
}

#include <atf-c++.hpp>

#include "engine/config.hpp"
#include "engine/kyuafile.hpp"
#include "engine/plain.hpp"
#include "engine/scheduler.hpp"
#include "model/metadata.hpp"
#include "model/test_program.hpp"
#include "utils/config/tree.ipp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/operations.hpp"
#include "utils/optional.ipp"
#include "utils/passwd.hpp"

namespace config = utils::config;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace scheduler = engine::scheduler;

using utils::none;


namespace {


/// Gets the path to an example file.
///
/// \param name The name of the example file.
///
/// \return A path to the desired example file.  This can either be inside the
/// source tree before installing Kyua or in the target installation directory
/// after installation.
static fs::path
example_file(const char* name)
{
    const fs::path examplesdir(utils::getenv_with_default(
        "KYUA_EXAMPLESDIR", KYUA_EXAMPLESDIR));
    return examplesdir / name;
}


}  // anonymous namespace


ATF_TEST_CASE(kyua_conf);
ATF_TEST_CASE_HEAD(kyua_conf)
{
    utils::logging::set_inmemory();
    set_md_var("require.files", example_file("kyua.conf").str());
}
ATF_TEST_CASE_BODY(kyua_conf)
{
    std::vector< passwd::user > users;
    users.push_back(passwd::user("nobody", 1, 2));
    passwd::set_mock_users_for_testing(users);

    const config::tree user_config = engine::load_config(
        example_file("kyua.conf"));

    ATF_REQUIRE_EQ(
        "x86_64",
        user_config.lookup< config::string_node >("architecture"));
    ATF_REQUIRE_EQ(
        16,
        user_config.lookup< config::positive_int_node >("parallelism"));
    ATF_REQUIRE_EQ(
        "amd64",
        user_config.lookup< config::string_node >("platform"));

    ATF_REQUIRE_EQ(
        "nobody",
        user_config.lookup< engine::user_node >("unprivileged_user").name);

    config::properties_map exp_test_suites;
    exp_test_suites["test_suites.kyua.run_coredump_tests"] = "false";
    exp_test_suites["test_suites.FreeBSD.iterations"] = "1000";
    exp_test_suites["test_suites.FreeBSD.run_old_tests"] = "false";
    exp_test_suites["test_suites.NetBSD.file_systems"] = "ffs lfs ext2fs";
    exp_test_suites["test_suites.NetBSD.iterations"] = "100";
    exp_test_suites["test_suites.NetBSD.run_broken_tests"] = "true";
    ATF_REQUIRE(exp_test_suites == user_config.all_properties("test_suites"));
}


ATF_TEST_CASE(kyuafile_top__no_matches);
ATF_TEST_CASE_HEAD(kyuafile_top__no_matches)
{
    utils::logging::set_inmemory();
    set_md_var("require.files", example_file("Kyuafile.top").str());
}
ATF_TEST_CASE_BODY(kyuafile_top__no_matches)
{
    scheduler::scheduler_handle handle = scheduler::setup();

    fs::mkdir(fs::path("root"), 0755);
    const fs::path source_path = example_file("Kyuafile.top");
    ATF_REQUIRE(::symlink(source_path.c_str(), "root/Kyuafile") != -1);

    atf::utils::create_file("root/file", "");
    fs::mkdir(fs::path("root/subdir"), 0755);

    const engine::kyuafile kyuafile = engine::kyuafile::load(
        fs::path("root/Kyuafile"), none, engine::default_config(), handle);
    ATF_REQUIRE_EQ(fs::path("root"), kyuafile.source_root());
    ATF_REQUIRE_EQ(fs::path("root"), kyuafile.build_root());
    ATF_REQUIRE(kyuafile.test_programs().empty());

    handle.cleanup();
}


ATF_TEST_CASE(kyuafile_top__some_matches);
ATF_TEST_CASE_HEAD(kyuafile_top__some_matches)
{
    utils::logging::set_inmemory();
    set_md_var("require.files", example_file("Kyuafile.top").str());
}
ATF_TEST_CASE_BODY(kyuafile_top__some_matches)
{
    scheduler::scheduler_handle handle = scheduler::setup();

    fs::mkdir(fs::path("root"), 0755);
    const fs::path source_path = example_file("Kyuafile.top");
    ATF_REQUIRE(::symlink(source_path.c_str(), "root/Kyuafile") != -1);

    atf::utils::create_file("root/file", "");

    fs::mkdir(fs::path("root/subdir1"), 0755);
    atf::utils::create_file("root/subdir1/Kyuafile",
                            "syntax(2)\n"
                            "plain_test_program{name='a', test_suite='b'}\n");
    atf::utils::create_file("root/subdir1/a", "");

    fs::mkdir(fs::path("root/subdir2"), 0755);
    atf::utils::create_file("root/subdir2/Kyuafile",
                            "syntax(2)\n"
                            "plain_test_program{name='c', test_suite='d'}\n");
    atf::utils::create_file("root/subdir2/c", "");
    atf::utils::create_file("root/subdir2/Kyuafile.etc", "invalid");

    const engine::kyuafile kyuafile = engine::kyuafile::load(
        fs::path("root/Kyuafile"), none, engine::default_config(), handle);
    ATF_REQUIRE_EQ(fs::path("root"), kyuafile.source_root());
    ATF_REQUIRE_EQ(fs::path("root"), kyuafile.build_root());

    const model::test_program exp_test_program_a = model::test_program_builder(
        "plain", fs::path("subdir1/a"), fs::path("root").to_absolute(), "b")
        .add_test_case("main")
        .build();
    const model::test_program exp_test_program_c = model::test_program_builder(
        "plain", fs::path("subdir2/c"), fs::path("root").to_absolute(), "d")
        .add_test_case("main")
        .build();

    ATF_REQUIRE_EQ(2, kyuafile.test_programs().size());
    ATF_REQUIRE((exp_test_program_a == *kyuafile.test_programs()[0] &&
                 exp_test_program_c == *kyuafile.test_programs()[1])
                ||
                (exp_test_program_a == *kyuafile.test_programs()[1] &&
                 exp_test_program_c == *kyuafile.test_programs()[0]));

    handle.cleanup();
}


ATF_INIT_TEST_CASES(tcs)
{
    scheduler::register_interface(
        "plain", std::shared_ptr< scheduler::interface >(
            new engine::plain_interface()));

    ATF_ADD_TEST_CASE(tcs, kyua_conf);

    ATF_ADD_TEST_CASE(tcs, kyuafile_top__no_matches);
    ATF_ADD_TEST_CASE(tcs, kyuafile_top__some_matches);
}
