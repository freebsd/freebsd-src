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

#include "utils/stream.hpp"

#include <cstdlib>
#include <sstream>

#include <atf-c++.hpp>

#include "utils/fs/path.hpp"

namespace fs = utils::fs;


ATF_TEST_CASE_WITHOUT_HEAD(open_ostream__stdout);
ATF_TEST_CASE_BODY(open_ostream__stdout)
{
    const pid_t pid = atf::utils::fork();
    if (pid == 0) {
        std::unique_ptr< std::ostream > output = utils::open_ostream(
            fs::path("/dev/stdout"));
        (*output) << "Message to stdout\n";
        output.reset();
        std::exit(EXIT_SUCCESS);
    }
    atf::utils::wait(pid, EXIT_SUCCESS, "Message to stdout\n", "");
}


ATF_TEST_CASE_WITHOUT_HEAD(open_ostream__stderr);
ATF_TEST_CASE_BODY(open_ostream__stderr)
{
    const pid_t pid = atf::utils::fork();
    if (pid == 0) {
        std::unique_ptr< std::ostream > output = utils::open_ostream(
            fs::path("/dev/stderr"));
        (*output) << "Message to stderr\n";
        output.reset();
        std::exit(EXIT_SUCCESS);
    }
    atf::utils::wait(pid, EXIT_SUCCESS, "", "Message to stderr\n");
}


ATF_TEST_CASE_WITHOUT_HEAD(open_ostream__other);
ATF_TEST_CASE_BODY(open_ostream__other)
{
    const pid_t pid = atf::utils::fork();
    if (pid == 0) {
        std::unique_ptr< std::ostream > output = utils::open_ostream(
            fs::path("some-file.txt"));
        (*output) << "Message to other file\n";
        output.reset();
        std::exit(EXIT_SUCCESS);
    }
    atf::utils::wait(pid, EXIT_SUCCESS, "", "");
    atf::utils::compare_file("some-file.txt", "Message to other file\n");
}


ATF_TEST_CASE_WITHOUT_HEAD(stream_length__empty);
ATF_TEST_CASE_BODY(stream_length__empty)
{
    std::istringstream input("");
    ATF_REQUIRE_EQ(0, utils::stream_length(input));
}


ATF_TEST_CASE_WITHOUT_HEAD(stream_length__some);
ATF_TEST_CASE_BODY(stream_length__some)
{
    const std::string contents(8192, 'x');
    std::istringstream input(contents);
    ATF_REQUIRE_EQ(
        contents.length(),
        static_cast< std::string::size_type >(utils::stream_length(input)));
}


ATF_TEST_CASE_WITHOUT_HEAD(read_file__ok);
ATF_TEST_CASE_BODY(read_file__ok)
{
    const char* contents = "These are\nsome file contents";
    atf::utils::create_file("input.txt", contents);
    ATF_REQUIRE_EQ(contents, utils::read_file(fs::path("input.txt")));
}


ATF_TEST_CASE_WITHOUT_HEAD(read_file__missing_file);
ATF_TEST_CASE_BODY(read_file__missing_file)
{
    ATF_REQUIRE_THROW_RE(std::runtime_error,
                         "Failed to open 'foo.txt' for read",
                         utils::read_file(fs::path("foo.txt")));
}


ATF_TEST_CASE_WITHOUT_HEAD(read_stream__empty);
ATF_TEST_CASE_BODY(read_stream__empty)
{
    std::istringstream input("");
    ATF_REQUIRE_EQ("", utils::read_stream(input));
}


ATF_TEST_CASE_WITHOUT_HEAD(read_stream__some);
ATF_TEST_CASE_BODY(read_stream__some)
{
    std::string contents;
    for (int i = 0; i < 1000; i++)
        contents += "abcdef";
    std::istringstream input(contents);
    ATF_REQUIRE_EQ(contents, utils::read_stream(input));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, open_ostream__stdout);
    ATF_ADD_TEST_CASE(tcs, open_ostream__stderr);
    ATF_ADD_TEST_CASE(tcs, open_ostream__other);

    ATF_ADD_TEST_CASE(tcs, stream_length__empty);
    ATF_ADD_TEST_CASE(tcs, stream_length__some);

    ATF_ADD_TEST_CASE(tcs, read_file__ok);
    ATF_ADD_TEST_CASE(tcs, read_file__missing_file);

    ATF_ADD_TEST_CASE(tcs, read_stream__empty);
    ATF_ADD_TEST_CASE(tcs, read_stream__some);
}
