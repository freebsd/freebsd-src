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

#include "utils/fs/exceptions.hpp"

#include <cerrno>
#include <cstring>

#include <atf-c++.hpp>

#include "utils/format/macros.hpp"

namespace fs = utils::fs;


ATF_TEST_CASE_WITHOUT_HEAD(error);
ATF_TEST_CASE_BODY(error)
{
    const fs::error e("Some text");
    ATF_REQUIRE(std::strcmp("Some text", e.what()) == 0);
}


ATF_TEST_CASE_WITHOUT_HEAD(invalid_path_error);
ATF_TEST_CASE_BODY(invalid_path_error)
{
    const fs::invalid_path_error e("some/invalid/path", "The reason");
    ATF_REQUIRE(std::strcmp("Invalid path 'some/invalid/path': The reason",
                            e.what()) == 0);
    ATF_REQUIRE_EQ("some/invalid/path", e.invalid_path());
}


ATF_TEST_CASE_WITHOUT_HEAD(join_error);
ATF_TEST_CASE_BODY(join_error)
{
    const fs::join_error e("dir1/file1", "/dir2/file2", "The reason");
    ATF_REQUIRE(std::strcmp("Cannot join paths 'dir1/file1' and '/dir2/file2': "
                            "The reason", e.what()) == 0);
    ATF_REQUIRE_EQ("dir1/file1", e.textual_path1());
    ATF_REQUIRE_EQ("/dir2/file2", e.textual_path2());
}


ATF_TEST_CASE_WITHOUT_HEAD(system_error);
ATF_TEST_CASE_BODY(system_error)
{
    const fs::system_error e("Call failed", ENOENT);
    const std::string expected = F("Call failed: %s") % std::strerror(ENOENT);
    ATF_REQUIRE_EQ(expected, e.what());
    ATF_REQUIRE_EQ(ENOENT, e.original_errno());
}


ATF_TEST_CASE_WITHOUT_HEAD(unsupported_operation_error);
ATF_TEST_CASE_BODY(unsupported_operation_error)
{
    const fs::unsupported_operation_error e("Some text");
    ATF_REQUIRE(std::strcmp("Some text", e.what()) == 0);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, error);
    ATF_ADD_TEST_CASE(tcs, invalid_path_error);
    ATF_ADD_TEST_CASE(tcs, join_error);
    ATF_ADD_TEST_CASE(tcs, system_error);
    ATF_ADD_TEST_CASE(tcs, unsupported_operation_error);
}
