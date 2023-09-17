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

#include "utils/fs/lua_module.hpp"

#include <atf-c++.hpp>
#include <lutok/operations.hpp>
#include <lutok/state.hpp>
#include <lutok/test_utils.hpp>

#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"

namespace fs = utils::fs;


ATF_TEST_CASE_WITHOUT_HEAD(open_fs);
ATF_TEST_CASE_BODY(open_fs)
{
    lutok::state state;
    stack_balance_checker checker(state);
    fs::open_fs(state);
    lutok::do_string(state, "return fs.basename", 0, 1, 0);
    ATF_REQUIRE(state.is_function(-1));
    lutok::do_string(state, "return fs.dirname", 0, 1, 0);
    ATF_REQUIRE(state.is_function(-1));
    lutok::do_string(state, "return fs.join", 0, 1, 0);
    ATF_REQUIRE(state.is_function(-1));
    state.pop(3);
}


ATF_TEST_CASE_WITHOUT_HEAD(basename__ok);
ATF_TEST_CASE_BODY(basename__ok)
{
    lutok::state state;
    fs::open_fs(state);

    lutok::do_string(state, "return fs.basename('/my/test//file_foobar')",
                     0, 1, 0);
    ATF_REQUIRE_EQ("file_foobar", state.to_string(-1));
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(basename__fail);
ATF_TEST_CASE_BODY(basename__fail)
{
    lutok::state state;
    fs::open_fs(state);

    ATF_REQUIRE_THROW_RE(lutok::error, "Need a string",
                         lutok::do_string(state, "return fs.basename({})",
                                          0, 1, 0));
    ATF_REQUIRE_THROW_RE(lutok::error, "Invalid path",
                         lutok::do_string(state, "return fs.basename('')",
                                          0, 1, 0));
}


ATF_TEST_CASE_WITHOUT_HEAD(dirname__ok);
ATF_TEST_CASE_BODY(dirname__ok)
{
    lutok::state state;
    fs::open_fs(state);

    lutok::do_string(state, "return fs.dirname('/my/test//file_foobar')",
                     0, 1, 0);
    ATF_REQUIRE_EQ("/my/test", state.to_string(-1));
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(dirname__fail);
ATF_TEST_CASE_BODY(dirname__fail)
{
    lutok::state state;
    fs::open_fs(state);

    ATF_REQUIRE_THROW_RE(lutok::error, "Need a string",
                         lutok::do_string(state, "return fs.dirname({})",
                                          0, 1, 0));
    ATF_REQUIRE_THROW_RE(lutok::error, "Invalid path",
                         lutok::do_string(state, "return fs.dirname('')",
                                          0, 1, 0));
}


ATF_TEST_CASE_WITHOUT_HEAD(exists__ok);
ATF_TEST_CASE_BODY(exists__ok)
{
    lutok::state state;
    fs::open_fs(state);

    atf::utils::create_file("foo", "");

    lutok::do_string(state, "return fs.exists('foo')", 0, 1, 0);
    ATF_REQUIRE(state.to_boolean(-1));
    state.pop(1);

    lutok::do_string(state, "return fs.exists('bar')", 0, 1, 0);
    ATF_REQUIRE(!state.to_boolean(-1));
    state.pop(1);

    lutok::do_string(state,
                     F("return fs.exists('%s')") % fs::current_path(), 0, 1, 0);
    ATF_REQUIRE(state.to_boolean(-1));
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(exists__fail);
ATF_TEST_CASE_BODY(exists__fail)
{
    lutok::state state;
    fs::open_fs(state);

    ATF_REQUIRE_THROW_RE(lutok::error, "Need a string",
                         lutok::do_string(state, "return fs.exists({})",
                                          0, 1, 0));
    ATF_REQUIRE_THROW_RE(lutok::error, "Invalid path",
                         lutok::do_string(state, "return fs.exists('')",
                                          0, 1, 0));
}


ATF_TEST_CASE_WITHOUT_HEAD(exists__custom_start_dir);
ATF_TEST_CASE_BODY(exists__custom_start_dir)
{
    lutok::state state;
    fs::open_fs(state, fs::path("subdir"));

    fs::mkdir(fs::path("subdir"), 0755);
    atf::utils::create_file("subdir/foo", "");
    atf::utils::create_file("bar", "");

    lutok::do_string(state, "return fs.exists('foo')", 0, 1, 0);
    ATF_REQUIRE(state.to_boolean(-1));
    state.pop(1);

    lutok::do_string(state, "return fs.exists('subdir/foo')", 0, 1, 0);
    ATF_REQUIRE(!state.to_boolean(-1));
    state.pop(1);

    lutok::do_string(state, "return fs.exists('bar')", 0, 1, 0);
    ATF_REQUIRE(!state.to_boolean(-1));
    state.pop(1);

    lutok::do_string(state, "return fs.exists('../bar')", 0, 1, 0);
    ATF_REQUIRE(state.to_boolean(-1));
    state.pop(1);

    lutok::do_string(state,
                     F("return fs.exists('%s')") % (fs::current_path() / "bar"),
                     0, 1, 0);
    ATF_REQUIRE(state.to_boolean(-1));
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(files__none);
ATF_TEST_CASE_BODY(files__none)
{
    lutok::state state;
    state.open_table();
    fs::open_fs(state);

    fs::mkdir(fs::path("root"), 0755);

    lutok::do_string(state,
                     "names = {}\n"
                     "for file in fs.files('root') do\n"
                     "    table.insert(names, file)\n"
                     "end\n"
                     "table.sort(names)\n"
                     "return table.concat(names, ' ')",
                     0, 1, 0);
    ATF_REQUIRE_EQ(". ..", state.to_string(-1));
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(files__some);
ATF_TEST_CASE_BODY(files__some)
{
    lutok::state state;
    state.open_table();
    fs::open_fs(state);

    fs::mkdir(fs::path("root"), 0755);
    atf::utils::create_file("root/file1", "");
    atf::utils::create_file("root/file2", "");

    lutok::do_string(state,
                     "names = {}\n"
                     "for file in fs.files('root') do\n"
                     "    table.insert(names, file)\n"
                     "end\n"
                     "table.sort(names)\n"
                     "return table.concat(names, ' ')",
                     0, 1, 0);
    ATF_REQUIRE_EQ(". .. file1 file2", state.to_string(-1));
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(files__some_with_custom_start_dir);
ATF_TEST_CASE_BODY(files__some_with_custom_start_dir)
{
    lutok::state state;
    state.open_table();
    fs::open_fs(state, fs::current_path() / "root");

    fs::mkdir(fs::path("root"), 0755);
    atf::utils::create_file("root/file1", "");
    atf::utils::create_file("root/file2", "");
    atf::utils::create_file("file3", "");

    lutok::do_string(state,
                     "names = {}\n"
                     "for file in fs.files('.') do\n"
                     "    table.insert(names, file)\n"
                     "end\n"
                     "table.sort(names)\n"
                     "return table.concat(names, ' ')",
                     0, 1, 0);
    ATF_REQUIRE_EQ(". .. file1 file2", state.to_string(-1));
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(files__fail_arg);
ATF_TEST_CASE_BODY(files__fail_arg)
{
    lutok::state state;
    fs::open_fs(state);

    ATF_REQUIRE_THROW_RE(lutok::error, "Need a string parameter",
                         lutok::do_string(state, "fs.files({})", 0, 0, 0));
    ATF_REQUIRE_THROW_RE(lutok::error, "Invalid path",
                         lutok::do_string(state, "fs.files('')", 0, 0, 0));
}


ATF_TEST_CASE_WITHOUT_HEAD(files__fail_opendir);
ATF_TEST_CASE_BODY(files__fail_opendir)
{
    lutok::state state;
    fs::open_fs(state);

    ATF_REQUIRE_THROW_RE(lutok::error, "Failed to open directory",
                         lutok::do_string(state, "fs.files('root')", 0, 0, 0));
}


ATF_TEST_CASE_WITHOUT_HEAD(is_absolute__ok);
ATF_TEST_CASE_BODY(is_absolute__ok)
{
    lutok::state state;
    fs::open_fs(state);

    lutok::do_string(state, "return fs.is_absolute('my/test//file_foobar')",
                     0, 1, 0);
    ATF_REQUIRE(!state.to_boolean(-1));
    lutok::do_string(state, "return fs.is_absolute('/my/test//file_foobar')",
                     0, 1, 0);
    ATF_REQUIRE(state.to_boolean(-1));
    state.pop(2);
}


ATF_TEST_CASE_WITHOUT_HEAD(is_absolute__fail);
ATF_TEST_CASE_BODY(is_absolute__fail)
{
    lutok::state state;
    fs::open_fs(state);

    ATF_REQUIRE_THROW_RE(lutok::error, "Need a string",
                         lutok::do_string(state, "return fs.is_absolute({})",
                                          0, 1, 0));
    ATF_REQUIRE_THROW_RE(lutok::error, "Invalid path",
                         lutok::do_string(state, "return fs.is_absolute('')",
                                          0, 1, 0));
}


ATF_TEST_CASE_WITHOUT_HEAD(join__ok);
ATF_TEST_CASE_BODY(join__ok)
{
    lutok::state state;
    fs::open_fs(state);

    lutok::do_string(state, "return fs.join('/a/b///', 'c/d')", 0, 1, 0);
    ATF_REQUIRE_EQ("/a/b/c/d", state.to_string(-1));
    state.pop(1);
}


ATF_TEST_CASE_WITHOUT_HEAD(join__fail);
ATF_TEST_CASE_BODY(join__fail)
{
    lutok::state state;
    fs::open_fs(state);

    ATF_REQUIRE_THROW_RE(lutok::error, "Need a string",
                         lutok::do_string(state, "return fs.join({}, 'a')",
                                          0, 1, 0));
    ATF_REQUIRE_THROW_RE(lutok::error, "Need a string",
                         lutok::do_string(state, "return fs.join('a', {})",
                                          0, 1, 0));

    ATF_REQUIRE_THROW_RE(lutok::error, "Invalid path",
                         lutok::do_string(state, "return fs.join('', 'a')",
                                          0, 1, 0));
    ATF_REQUIRE_THROW_RE(lutok::error, "Invalid path",
                         lutok::do_string(state, "return fs.join('a', '')",
                                          0, 1, 0));

    ATF_REQUIRE_THROW_RE(lutok::error, "Cannot join.*'a/b'.*'/c'",
                         lutok::do_string(state, "fs.join('a/b', '/c')",
                                          0, 0, 0));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, open_fs);

    ATF_ADD_TEST_CASE(tcs, basename__ok);
    ATF_ADD_TEST_CASE(tcs, basename__fail);

    ATF_ADD_TEST_CASE(tcs, dirname__ok);
    ATF_ADD_TEST_CASE(tcs, dirname__fail);

    ATF_ADD_TEST_CASE(tcs, exists__ok);
    ATF_ADD_TEST_CASE(tcs, exists__fail);
    ATF_ADD_TEST_CASE(tcs, exists__custom_start_dir);

    ATF_ADD_TEST_CASE(tcs, files__none);
    ATF_ADD_TEST_CASE(tcs, files__some);
    ATF_ADD_TEST_CASE(tcs, files__some_with_custom_start_dir);
    ATF_ADD_TEST_CASE(tcs, files__fail_arg);
    ATF_ADD_TEST_CASE(tcs, files__fail_opendir);

    ATF_ADD_TEST_CASE(tcs, is_absolute__ok);
    ATF_ADD_TEST_CASE(tcs, is_absolute__fail);

    ATF_ADD_TEST_CASE(tcs, join__ok);
    ATF_ADD_TEST_CASE(tcs, join__fail);
}
