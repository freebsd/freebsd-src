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

#include "engine/kyuafile.hpp"

extern "C" {
#include <unistd.h>
}

#include <stdexcept>
#include <typeinfo>

#include <atf-c++.hpp>
#include <lutok/operations.hpp>
#include <lutok/state.ipp>
#include <lutok/test_utils.hpp>

#include "engine/atf.hpp"
#include "engine/exceptions.hpp"
#include "engine/plain.hpp"
#include "engine/scheduler.hpp"
#include "engine/tap.hpp"
#include "model/metadata.hpp"
#include "model/test_program.hpp"
#include "utils/config/tree.ipp"
#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/optional.ipp"

namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace scheduler = engine::scheduler;

using utils::none;


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__empty);
ATF_TEST_CASE_BODY(kyuafile__load__empty)
{
    scheduler::scheduler_handle handle = scheduler::setup();

    atf::utils::create_file("config", "syntax(2)\n");

    const engine::kyuafile suite = engine::kyuafile::load(
        fs::path("config"), none, config::tree(), handle);
    ATF_REQUIRE_EQ(fs::path("."), suite.source_root());
    ATF_REQUIRE_EQ(fs::path("."), suite.build_root());
    ATF_REQUIRE_EQ(0, suite.test_programs().size());

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__real_interfaces);
ATF_TEST_CASE_BODY(kyuafile__load__real_interfaces)
{
    scheduler::scheduler_handle handle = scheduler::setup();

    atf::utils::create_file(
        "config",
        "syntax(2)\n"
        "test_suite('one-suite')\n"
        "atf_test_program{name='1st'}\n"
        "atf_test_program{name='2nd', test_suite='first'}\n"
        "plain_test_program{name='3rd'}\n"
        "tap_test_program{name='4th', test_suite='second'}\n"
        "include('dir/config')\n");

    fs::mkdir(fs::path("dir"), 0755);
    atf::utils::create_file(
        "dir/config",
        "syntax(2)\n"
        "atf_test_program{name='1st', test_suite='other-suite'}\n"
        "include('subdir/config')\n");

    fs::mkdir(fs::path("dir/subdir"), 0755);
    atf::utils::create_file(
        "dir/subdir/config",
        "syntax(2)\n"
        "atf_test_program{name='5th', test_suite='last-suite'}\n");

    atf::utils::create_file("1st", "");
    atf::utils::create_file("2nd", "");
    atf::utils::create_file("3rd", "");
    atf::utils::create_file("4th", "");
    atf::utils::create_file("dir/1st", "");
    atf::utils::create_file("dir/subdir/5th", "");

    const engine::kyuafile suite = engine::kyuafile::load(
        fs::path("config"), none, config::tree(), handle);
    ATF_REQUIRE_EQ(fs::path("."), suite.source_root());
    ATF_REQUIRE_EQ(fs::path("."), suite.build_root());
    ATF_REQUIRE_EQ(6, suite.test_programs().size());

    ATF_REQUIRE_EQ("atf", suite.test_programs()[0]->interface_name());
    ATF_REQUIRE_EQ(fs::path("1st"), suite.test_programs()[0]->relative_path());
    ATF_REQUIRE_EQ("one-suite", suite.test_programs()[0]->test_suite_name());

    ATF_REQUIRE_EQ("atf", suite.test_programs()[1]->interface_name());
    ATF_REQUIRE_EQ(fs::path("2nd"), suite.test_programs()[1]->relative_path());
    ATF_REQUIRE_EQ("first", suite.test_programs()[1]->test_suite_name());

    ATF_REQUIRE_EQ("plain", suite.test_programs()[2]->interface_name());
    ATF_REQUIRE_EQ(fs::path("3rd"), suite.test_programs()[2]->relative_path());
    ATF_REQUIRE_EQ("one-suite", suite.test_programs()[2]->test_suite_name());

    ATF_REQUIRE_EQ("tap", suite.test_programs()[3]->interface_name());
    ATF_REQUIRE_EQ(fs::path("4th"), suite.test_programs()[3]->relative_path());
    ATF_REQUIRE_EQ("second", suite.test_programs()[3]->test_suite_name());

    ATF_REQUIRE_EQ("atf", suite.test_programs()[4]->interface_name());
    ATF_REQUIRE_EQ(fs::path("dir/1st"),
                   suite.test_programs()[4]->relative_path());
    ATF_REQUIRE_EQ("other-suite", suite.test_programs()[4]->test_suite_name());

    ATF_REQUIRE_EQ("atf", suite.test_programs()[5]->interface_name());
    ATF_REQUIRE_EQ(fs::path("dir/subdir/5th"),
                   suite.test_programs()[5]->relative_path());
    ATF_REQUIRE_EQ("last-suite", suite.test_programs()[5]->test_suite_name());

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__mock_interfaces);
ATF_TEST_CASE_BODY(kyuafile__load__mock_interfaces)
{
    scheduler::scheduler_handle handle = scheduler::setup();

    std::shared_ptr< scheduler::interface > mock_interface(
        new engine::plain_interface());

    scheduler::register_interface("some", mock_interface);
    scheduler::register_interface("random", mock_interface);
    scheduler::register_interface("names", mock_interface);

    atf::utils::create_file(
        "config",
        "syntax(2)\n"
        "test_suite('one-suite')\n"
        "some_test_program{name='1st'}\n"
        "random_test_program{name='2nd'}\n"
        "names_test_program{name='3rd'}\n");

    atf::utils::create_file("1st", "");
    atf::utils::create_file("2nd", "");
    atf::utils::create_file("3rd", "");

    const engine::kyuafile suite = engine::kyuafile::load(
        fs::path("config"), none, config::tree(), handle);
    ATF_REQUIRE_EQ(fs::path("."), suite.source_root());
    ATF_REQUIRE_EQ(fs::path("."), suite.build_root());
    ATF_REQUIRE_EQ(3, suite.test_programs().size());

    ATF_REQUIRE_EQ("some", suite.test_programs()[0]->interface_name());
    ATF_REQUIRE_EQ(fs::path("1st"), suite.test_programs()[0]->relative_path());

    ATF_REQUIRE_EQ("random", suite.test_programs()[1]->interface_name());
    ATF_REQUIRE_EQ(fs::path("2nd"), suite.test_programs()[1]->relative_path());

    ATF_REQUIRE_EQ("names", suite.test_programs()[2]->interface_name());
    ATF_REQUIRE_EQ(fs::path("3rd"), suite.test_programs()[2]->relative_path());

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__metadata);
ATF_TEST_CASE_BODY(kyuafile__load__metadata)
{
    scheduler::scheduler_handle handle = scheduler::setup();

    atf::utils::create_file(
        "config",
        "syntax(2)\n"
        "atf_test_program{name='1st', test_suite='first',"
        " allowed_architectures='amd64 i386', timeout=15}\n"
        "plain_test_program{name='2nd', test_suite='second',"
        " required_files='foo /bar//baz', required_user='root',"
        " ['custom.a-number']=123, ['custom.a-bool']=true}\n");
    atf::utils::create_file("1st", "");
    atf::utils::create_file("2nd", "");

    const engine::kyuafile suite = engine::kyuafile::load(
        fs::path("config"), none, config::tree(), handle);
    ATF_REQUIRE_EQ(2, suite.test_programs().size());

    ATF_REQUIRE_EQ("atf", suite.test_programs()[0]->interface_name());
    ATF_REQUIRE_EQ(fs::path("1st"), suite.test_programs()[0]->relative_path());
    ATF_REQUIRE_EQ("first", suite.test_programs()[0]->test_suite_name());
    const model::metadata md1 = model::metadata_builder()
        .add_allowed_architecture("amd64")
        .add_allowed_architecture("i386")
        .set_timeout(datetime::delta(15, 0))
        .build();
    ATF_REQUIRE_EQ(md1, suite.test_programs()[0]->get_metadata());

    ATF_REQUIRE_EQ("plain", suite.test_programs()[1]->interface_name());
    ATF_REQUIRE_EQ(fs::path("2nd"), suite.test_programs()[1]->relative_path());
    ATF_REQUIRE_EQ("second", suite.test_programs()[1]->test_suite_name());
    const model::metadata md2 = model::metadata_builder()
        .add_required_file(fs::path("foo"))
        .add_required_file(fs::path("/bar/baz"))
        .add_custom("a-bool", "true")
        .add_custom("a-number", "123")
        .set_required_user("root")
        .build();
    ATF_REQUIRE_EQ(md2, suite.test_programs()[1]->get_metadata());

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__current_directory);
ATF_TEST_CASE_BODY(kyuafile__load__current_directory)
{
    scheduler::scheduler_handle handle = scheduler::setup();

    atf::utils::create_file(
        "config",
        "syntax(2)\n"
        "atf_test_program{name='one', test_suite='first'}\n"
        "include('config2')\n");

    atf::utils::create_file(
        "config2",
        "syntax(2)\n"
        "test_suite('second')\n"
        "atf_test_program{name='two'}\n");

    atf::utils::create_file("one", "");
    atf::utils::create_file("two", "");

    const engine::kyuafile suite = engine::kyuafile::load(
        fs::path("config"), none, config::tree(), handle);
    ATF_REQUIRE_EQ(fs::path("."), suite.source_root());
    ATF_REQUIRE_EQ(fs::path("."), suite.build_root());
    ATF_REQUIRE_EQ(2, suite.test_programs().size());
    ATF_REQUIRE_EQ(fs::path("one"), suite.test_programs()[0]->relative_path());
    ATF_REQUIRE_EQ("first", suite.test_programs()[0]->test_suite_name());
    ATF_REQUIRE_EQ(fs::path("two"),
                   suite.test_programs()[1]->relative_path());
    ATF_REQUIRE_EQ("second", suite.test_programs()[1]->test_suite_name());

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__other_directory);
ATF_TEST_CASE_BODY(kyuafile__load__other_directory)
{
    scheduler::scheduler_handle handle = scheduler::setup();

    fs::mkdir(fs::path("root"), 0755);
    atf::utils::create_file(
        "root/config",
        "syntax(2)\n"
        "test_suite('abc')\n"
        "atf_test_program{name='one'}\n"
        "include('dir/config')\n");

    fs::mkdir(fs::path("root/dir"), 0755);
    atf::utils::create_file(
        "root/dir/config",
        "syntax(2)\n"
        "test_suite('foo')\n"
        "atf_test_program{name='two', test_suite='def'}\n"
        "atf_test_program{name='three'}\n");

    atf::utils::create_file("root/one", "");
    atf::utils::create_file("root/dir/two", "");
    atf::utils::create_file("root/dir/three", "");

    const engine::kyuafile suite = engine::kyuafile::load(
        fs::path("root/config"), none, config::tree(), handle);
    ATF_REQUIRE_EQ(fs::path("root"), suite.source_root());
    ATF_REQUIRE_EQ(fs::path("root"), suite.build_root());
    ATF_REQUIRE_EQ(3, suite.test_programs().size());
    ATF_REQUIRE_EQ(fs::path("one"), suite.test_programs()[0]->relative_path());
    ATF_REQUIRE_EQ("abc", suite.test_programs()[0]->test_suite_name());
    ATF_REQUIRE_EQ(fs::path("dir/two"),
                   suite.test_programs()[1]->relative_path());
    ATF_REQUIRE_EQ("def", suite.test_programs()[1]->test_suite_name());
    ATF_REQUIRE_EQ(fs::path("dir/three"),
                   suite.test_programs()[2]->relative_path());
    ATF_REQUIRE_EQ("foo", suite.test_programs()[2]->test_suite_name());

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__build_directory);
ATF_TEST_CASE_BODY(kyuafile__load__build_directory)
{
    scheduler::scheduler_handle handle = scheduler::setup();

    fs::mkdir(fs::path("srcdir"), 0755);
    atf::utils::create_file(
        "srcdir/config",
        "syntax(2)\n"
        "test_suite('abc')\n"
        "atf_test_program{name='one'}\n"
        "include('dir/config')\n");

    fs::mkdir(fs::path("srcdir/dir"), 0755);
    atf::utils::create_file(
        "srcdir/dir/config",
        "syntax(2)\n"
        "test_suite('foo')\n"
        "atf_test_program{name='two', test_suite='def'}\n"
        "atf_test_program{name='three'}\n");

    fs::mkdir(fs::path("builddir"), 0755);
    atf::utils::create_file("builddir/one", "");
    fs::mkdir(fs::path("builddir/dir"), 0755);
    atf::utils::create_file("builddir/dir/two", "");
    atf::utils::create_file("builddir/dir/three", "");

    const engine::kyuafile suite = engine::kyuafile::load(
        fs::path("srcdir/config"), utils::make_optional(fs::path("builddir")),
        config::tree(), handle);
    ATF_REQUIRE_EQ(fs::path("srcdir"), suite.source_root());
    ATF_REQUIRE_EQ(fs::path("builddir"), suite.build_root());
    ATF_REQUIRE_EQ(3, suite.test_programs().size());
    ATF_REQUIRE_EQ(fs::path("builddir/one").to_absolute(),
                   suite.test_programs()[0]->absolute_path());
    ATF_REQUIRE_EQ(fs::path("one"), suite.test_programs()[0]->relative_path());
    ATF_REQUIRE_EQ("abc", suite.test_programs()[0]->test_suite_name());
    ATF_REQUIRE_EQ(fs::path("builddir/dir/two").to_absolute(),
                   suite.test_programs()[1]->absolute_path());
    ATF_REQUIRE_EQ(fs::path("dir/two"),
                   suite.test_programs()[1]->relative_path());
    ATF_REQUIRE_EQ("def", suite.test_programs()[1]->test_suite_name());
    ATF_REQUIRE_EQ(fs::path("builddir/dir/three").to_absolute(),
                   suite.test_programs()[2]->absolute_path());
    ATF_REQUIRE_EQ(fs::path("dir/three"),
                   suite.test_programs()[2]->relative_path());
    ATF_REQUIRE_EQ("foo", suite.test_programs()[2]->test_suite_name());

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__absolute_paths_are_stable);
ATF_TEST_CASE_BODY(kyuafile__load__absolute_paths_are_stable)
{
    scheduler::scheduler_handle handle = scheduler::setup();

    atf::utils::create_file(
        "config",
        "syntax(2)\n"
        "atf_test_program{name='one', test_suite='first'}\n");
    atf::utils::create_file("one", "");

    const engine::kyuafile suite = engine::kyuafile::load(
        fs::path("config"), none, config::tree(), handle);

    const fs::path previous_dir = fs::current_path();
    fs::mkdir(fs::path("other"), 0755);
    // Change the directory.  We want later calls to absolute_path() on the test
    // programs to return references to previous_dir instead.
    ATF_REQUIRE(::chdir("other") != -1);

    ATF_REQUIRE_EQ(fs::path("."), suite.source_root());
    ATF_REQUIRE_EQ(fs::path("."), suite.build_root());
    ATF_REQUIRE_EQ(1, suite.test_programs().size());
    ATF_REQUIRE_EQ(previous_dir / "one",
                   suite.test_programs()[0]->absolute_path());
    ATF_REQUIRE_EQ(fs::path("one"), suite.test_programs()[0]->relative_path());
    ATF_REQUIRE_EQ("first", suite.test_programs()[0]->test_suite_name());

    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__fs_calls_are_relative);
ATF_TEST_CASE_BODY(kyuafile__load__fs_calls_are_relative)
{
    scheduler::scheduler_handle handle = scheduler::setup();

    atf::utils::create_file(
        "Kyuafile",
        "syntax(2)\n"
        "if fs.exists('one') then\n"
        "    plain_test_program{name='one', test_suite='first'}\n"
        "end\n"
        "if fs.exists('two') then\n"
        "    plain_test_program{name='two', test_suite='first'}\n"
        "end\n"
        "include('dir/Kyuafile')\n");
    atf::utils::create_file("one", "");
    fs::mkdir(fs::path("dir"), 0755);
    atf::utils::create_file(
        "dir/Kyuafile",
        "syntax(2)\n"
        "if fs.exists('one') then\n"
        "    plain_test_program{name='one', test_suite='first'}\n"
        "end\n"
        "if fs.exists('two') then\n"
        "    plain_test_program{name='two', test_suite='first'}\n"
        "end\n");
    atf::utils::create_file("dir/two", "");

    const engine::kyuafile suite = engine::kyuafile::load(
        fs::path("Kyuafile"), none, config::tree(), handle);

    ATF_REQUIRE_EQ(2, suite.test_programs().size());
    ATF_REQUIRE_EQ(fs::current_path() / "one",
                   suite.test_programs()[0]->absolute_path());
    ATF_REQUIRE_EQ(fs::current_path() / "dir/two",
                   suite.test_programs()[1]->absolute_path());

    handle.cleanup();
}


/// Verifies that load raises a load_error on a given input.
///
/// \param file Name of the file to load.
/// \param regex Expression to match on load_error's contents.
static void
do_load_error_test(const char* file, const char* regex)
{
    scheduler::scheduler_handle handle = scheduler::setup();
    ATF_REQUIRE_THROW_RE(engine::load_error, regex,
                         engine::kyuafile::load(fs::path(file), none,
                                                config::tree(), handle));
    handle.cleanup();
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__test_program_not_basename);
ATF_TEST_CASE_BODY(kyuafile__load__test_program_not_basename)
{
    atf::utils::create_file(
        "config",
        "syntax(2)\n"
        "test_suite('abc')\n"
        "atf_test_program{name='one'}\n"
        "atf_test_program{name='./ls'}\n");

    atf::utils::create_file("one", "");
    do_load_error_test("config", "./ls.*path components");
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__lua_error);
ATF_TEST_CASE_BODY(kyuafile__load__lua_error)
{
    atf::utils::create_file("config", "this syntax is invalid\n");

    do_load_error_test("config", ".*");
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__syntax__not_called);
ATF_TEST_CASE_BODY(kyuafile__load__syntax__not_called)
{
    atf::utils::create_file("config", "");

    do_load_error_test("config", "syntax.* never called");
}



ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__syntax__deprecated_format);
ATF_TEST_CASE_BODY(kyuafile__load__syntax__deprecated_format)
{
    atf::utils::create_file("config", "syntax('foo', 1)\n");
    do_load_error_test("config", "must be 'kyuafile'");

    atf::utils::create_file("config", "syntax('config', 2)\n");
    do_load_error_test("config", "only takes one argument");
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__syntax__twice);
ATF_TEST_CASE_BODY(kyuafile__load__syntax__twice)
{
    atf::utils::create_file(
        "config",
        "syntax(2)\n"
        "syntax(2)\n");

    do_load_error_test("config", "Can only call syntax.* once");
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__syntax__bad_version);
ATF_TEST_CASE_BODY(kyuafile__load__syntax__bad_version)
{
    atf::utils::create_file("config", "syntax(12)\n");

    do_load_error_test("config", "Unsupported file version 12");
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__test_suite__missing);
ATF_TEST_CASE_BODY(kyuafile__load__test_suite__missing)
{
    atf::utils::create_file(
        "config",
        "syntax(2)\n"
        "plain_test_program{name='one'}");

    atf::utils::create_file("one", "");

    do_load_error_test("config", "No test suite defined");
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__test_suite__twice);
ATF_TEST_CASE_BODY(kyuafile__load__test_suite__twice)
{
    atf::utils::create_file(
        "config",
        "syntax(2)\n"
        "test_suite('foo')\n"
        "test_suite('bar')\n");

    do_load_error_test("config", "Can only call test_suite.* once");
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__missing_file);
ATF_TEST_CASE_BODY(kyuafile__load__missing_file)
{
    do_load_error_test("missing", "Load of 'missing' failed");
}


ATF_TEST_CASE_WITHOUT_HEAD(kyuafile__load__missing_test_program);
ATF_TEST_CASE_BODY(kyuafile__load__missing_test_program)
{
    atf::utils::create_file(
        "config",
        "syntax(2)\n"
        "atf_test_program{name='one', test_suite='first'}\n"
        "atf_test_program{name='two', test_suite='first'}\n");

    atf::utils::create_file("one", "");

    do_load_error_test("config", "Non-existent.*'two'");
}


ATF_INIT_TEST_CASES(tcs)
{
    scheduler::register_interface(
        "atf", std::shared_ptr< scheduler::interface >(
            new engine::atf_interface()));
    scheduler::register_interface(
        "plain", std::shared_ptr< scheduler::interface >(
            new engine::plain_interface()));
    scheduler::register_interface(
        "tap", std::shared_ptr< scheduler::interface >(
            new engine::tap_interface()));

    ATF_ADD_TEST_CASE(tcs, kyuafile__load__empty);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__real_interfaces);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__mock_interfaces);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__metadata);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__current_directory);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__other_directory);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__build_directory);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__absolute_paths_are_stable);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__fs_calls_are_relative);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__test_program_not_basename);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__lua_error);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__syntax__not_called);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__syntax__deprecated_format);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__syntax__twice);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__syntax__bad_version);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__test_suite__missing);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__test_suite__twice);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__missing_file);
    ATF_ADD_TEST_CASE(tcs, kyuafile__load__missing_test_program);
}
