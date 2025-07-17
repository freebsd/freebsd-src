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

#include "utils/fs/auto_cleaners.hpp"

extern "C" {
#include <unistd.h>
}

#include <atf-c++.hpp>

#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"

namespace fs = utils::fs;


ATF_TEST_CASE_WITHOUT_HEAD(auto_directory__automatic);
ATF_TEST_CASE_BODY(auto_directory__automatic)
{
    const fs::path root("root");
    fs::mkdir(root, 0755);

    {
        fs::auto_directory dir(root);
        ATF_REQUIRE_EQ(root, dir.directory());

        ATF_REQUIRE(::access("root", X_OK) == 0);

        {
            fs::auto_directory dir_copy(dir);
        }
        // Should still exist after a copy is destructed.
        ATF_REQUIRE(::access("root", X_OK) == 0);
    }
    ATF_REQUIRE(::access("root", X_OK) == -1);
}


ATF_TEST_CASE_WITHOUT_HEAD(auto_directory__explicit);
ATF_TEST_CASE_BODY(auto_directory__explicit)
{
    const fs::path root("root");
    fs::mkdir(root, 0755);

    fs::auto_directory dir(root);
    ATF_REQUIRE_EQ(root, dir.directory());

    ATF_REQUIRE(::access("root", X_OK) == 0);
    dir.cleanup();
    dir.cleanup();
    ATF_REQUIRE(::access("root", X_OK) == -1);
}


ATF_TEST_CASE_WITHOUT_HEAD(auto_directory__mkdtemp_public);
ATF_TEST_CASE_BODY(auto_directory__mkdtemp_public)
{
    utils::setenv("TMPDIR", (fs::current_path() / "tmp").str());
    fs::mkdir(fs::path("tmp"), 0755);

    const std::string path_template("test.XXXXXX");
    {
        fs::auto_directory auto_directory = fs::auto_directory::mkdtemp_public(
            path_template);
        ATF_REQUIRE(::access((fs::path("tmp") / path_template).c_str(),
                             X_OK) == -1);
        ATF_REQUIRE(::rmdir("tmp") == -1);

        ATF_REQUIRE(::access(auto_directory.directory().c_str(), X_OK) == 0);
    }
    ATF_REQUIRE(::rmdir("tmp") != -1);
}


ATF_TEST_CASE_WITHOUT_HEAD(auto_file__automatic);
ATF_TEST_CASE_BODY(auto_file__automatic)
{
    const fs::path file("foo");
    atf::utils::create_file(file.str(), "");
    {
        fs::auto_file auto_file(file);
        ATF_REQUIRE_EQ(file, auto_file.file());

        ATF_REQUIRE(::access(file.c_str(), R_OK) == 0);

        {
            fs::auto_file auto_file_copy(auto_file);
        }
        // Should still exist after a copy is destructed.
        ATF_REQUIRE(::access(file.c_str(), R_OK) == 0);
    }
    ATF_REQUIRE(::access(file.c_str(), R_OK) == -1);
}


ATF_TEST_CASE_WITHOUT_HEAD(auto_file__explicit);
ATF_TEST_CASE_BODY(auto_file__explicit)
{
    const fs::path file("bar");
    atf::utils::create_file(file.str(), "");

    fs::auto_file auto_file(file);
    ATF_REQUIRE_EQ(file, auto_file.file());

    ATF_REQUIRE(::access(file.c_str(), R_OK) == 0);
    auto_file.remove();
    auto_file.remove();
    ATF_REQUIRE(::access(file.c_str(), R_OK) == -1);
}


ATF_TEST_CASE_WITHOUT_HEAD(auto_file__mkstemp);
ATF_TEST_CASE_BODY(auto_file__mkstemp)
{
    utils::setenv("TMPDIR", (fs::current_path() / "tmp").str());
    fs::mkdir(fs::path("tmp"), 0755);

    const std::string path_template("test.XXXXXX");
    {
        fs::auto_file auto_file = fs::auto_file::mkstemp(path_template);
        ATF_REQUIRE(::access((fs::path("tmp") / path_template).c_str(),
                             X_OK) == -1);
        ATF_REQUIRE(::rmdir("tmp") == -1);

        ATF_REQUIRE(::access(auto_file.file().c_str(), R_OK) == 0);
    }
    ATF_REQUIRE(::rmdir("tmp") != -1);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, auto_directory__automatic);
    ATF_ADD_TEST_CASE(tcs, auto_directory__explicit);
    ATF_ADD_TEST_CASE(tcs, auto_directory__mkdtemp_public);

    ATF_ADD_TEST_CASE(tcs, auto_file__automatic);
    ATF_ADD_TEST_CASE(tcs, auto_file__explicit);
    ATF_ADD_TEST_CASE(tcs, auto_file__mkstemp);
}
