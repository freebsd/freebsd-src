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

#include "utils/logging/operations.hpp"

extern "C" {
#include <unistd.h>
}

#include <fstream>
#include <string>

#include <atf-c++.hpp>

#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace logging = utils::logging;


ATF_TEST_CASE_WITHOUT_HEAD(generate_log_name__before_log);
ATF_TEST_CASE_BODY(generate_log_name__before_log)
{
    datetime::set_mock_now(2011, 2, 21, 18, 10, 0, 0);
    ATF_REQUIRE_EQ(fs::path("/some/dir/foobar.20110221-181000.log"),
                   logging::generate_log_name(fs::path("/some/dir"), "foobar"));

    datetime::set_mock_now(2011, 2, 21, 18, 10, 1, 987654);
    logging::log(logging::level_info, "file", 123, "A message");

    datetime::set_mock_now(2011, 2, 21, 18, 10, 2, 123);
    ATF_REQUIRE_EQ(fs::path("/some/dir/foobar.20110221-181000.log"),
                   logging::generate_log_name(fs::path("/some/dir"), "foobar"));
}


ATF_TEST_CASE_WITHOUT_HEAD(generate_log_name__after_log);
ATF_TEST_CASE_BODY(generate_log_name__after_log)
{
    datetime::set_mock_now(2011, 2, 21, 18, 15, 0, 0);
    logging::log(logging::level_info, "file", 123, "A message");
    datetime::set_mock_now(2011, 2, 21, 18, 15, 1, 987654);
    logging::log(logging::level_info, "file", 123, "A message");

    datetime::set_mock_now(2011, 2, 21, 18, 15, 2, 123);
    ATF_REQUIRE_EQ(fs::path("/some/dir/foobar.20110221-181500.log"),
                   logging::generate_log_name(fs::path("/some/dir"), "foobar"));

    datetime::set_mock_now(2011, 2, 21, 18, 15, 3, 1);
    logging::log(logging::level_info, "file", 123, "A message");

    datetime::set_mock_now(2011, 2, 21, 18, 15, 4, 91);
    ATF_REQUIRE_EQ(fs::path("/some/dir/foobar.20110221-181500.log"),
                   logging::generate_log_name(fs::path("/some/dir"), "foobar"));
}


ATF_TEST_CASE_WITHOUT_HEAD(log);
ATF_TEST_CASE_BODY(log)
{
    logging::set_inmemory();

    datetime::set_mock_now(2011, 2, 21, 18, 10, 0, 0);
    logging::log(logging::level_debug, "f1", 1, "Debug message");

    datetime::set_mock_now(2011, 2, 21, 18, 10, 1, 987654);
    logging::log(logging::level_error, "f2", 2, "Error message");

    logging::set_persistency("debug", fs::path("test.log"));

    datetime::set_mock_now(2011, 2, 21, 18, 10, 2, 123);
    logging::log(logging::level_info, "f3", 3, "Info message");

    datetime::set_mock_now(2011, 2, 21, 18, 10, 3, 456);
    logging::log(logging::level_warning, "f4", 4, "Warning message");

    std::ifstream input("test.log");
    ATF_REQUIRE(input);

    const pid_t pid = ::getpid();

    std::string line;
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110221-181000 D %s f1:1: Debug message") % pid).str(), line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110221-181001 E %s f2:2: Error message") % pid).str(), line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110221-181002 I %s f3:3: Info message") % pid).str(), line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110221-181003 W %s f4:4: Warning message") % pid).str(), line);
}


ATF_TEST_CASE_WITHOUT_HEAD(set_inmemory__reset);
ATF_TEST_CASE_BODY(set_inmemory__reset)
{
    logging::set_persistency("debug", fs::path("test.log"));

    datetime::set_mock_now(2011, 2, 21, 18, 20, 0, 654321);
    logging::log(logging::level_debug, "file", 123, "Debug message");
    logging::set_inmemory();
    logging::log(logging::level_debug, "file", 123, "Debug message 2");

    std::ifstream input("test.log");
    ATF_REQUIRE(input);

    const pid_t pid = ::getpid();

    std::string line;
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110221-182000 D %s file:123: Debug message") % pid).str(), line);
}


ATF_TEST_CASE_WITHOUT_HEAD(set_persistency__no_backlog);
ATF_TEST_CASE_BODY(set_persistency__no_backlog)
{
    logging::set_persistency("debug", fs::path("test.log"));

    datetime::set_mock_now(2011, 2, 21, 18, 20, 0, 654321);
    logging::log(logging::level_debug, "file", 123, "Debug message");

    std::ifstream input("test.log");
    ATF_REQUIRE(input);

    const pid_t pid = ::getpid();

    std::string line;
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110221-182000 D %s file:123: Debug message") % pid).str(), line);
}


/// Creates a log for testing purposes, buffering messages on start.
///
/// \param level The level of the desired log.
/// \param path The output file.
static void
create_log(const std::string& level, const std::string& path)
{
    logging::set_inmemory();

    datetime::set_mock_now(2011, 3, 19, 11, 40, 0, 100);
    logging::log(logging::level_debug, "file1", 11, "Debug 1");

    datetime::set_mock_now(2011, 3, 19, 11, 40, 1, 200);
    logging::log(logging::level_error, "file2", 22, "Error 1");

    datetime::set_mock_now(2011, 3, 19, 11, 40, 2, 300);
    logging::log(logging::level_info, "file3", 33, "Info 1");

    datetime::set_mock_now(2011, 3, 19, 11, 40, 3, 400);
    logging::log(logging::level_warning, "file4", 44, "Warning 1");

    logging::set_persistency(level, fs::path(path));

    datetime::set_mock_now(2011, 3, 19, 11, 40, 4, 500);
    logging::log(logging::level_debug, "file1", 11, "Debug 2");

    datetime::set_mock_now(2011, 3, 19, 11, 40, 5, 600);
    logging::log(logging::level_error, "file2", 22, "Error 2");

    datetime::set_mock_now(2011, 3, 19, 11, 40, 6, 700);
    logging::log(logging::level_info, "file3", 33, "Info 2");

    datetime::set_mock_now(2011, 3, 19, 11, 40, 7, 800);
    logging::log(logging::level_warning, "file4", 44, "Warning 2");
}


ATF_TEST_CASE_WITHOUT_HEAD(set_persistency__some_backlog__debug);
ATF_TEST_CASE_BODY(set_persistency__some_backlog__debug)
{
    create_log("debug", "test.log");

    std::ifstream input("test.log");
    ATF_REQUIRE(input);

    const pid_t pid = ::getpid();

    std::string line;
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114000 D %s file1:11: Debug 1") % pid).str(), line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114001 E %s file2:22: Error 1") % pid).str(), line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114002 I %s file3:33: Info 1") % pid).str(), line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114003 W %s file4:44: Warning 1") % pid).str(), line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114004 D %s file1:11: Debug 2") % pid).str(), line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114005 E %s file2:22: Error 2") % pid).str(), line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114006 I %s file3:33: Info 2") % pid).str(), line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114007 W %s file4:44: Warning 2") % pid).str(), line);
}


ATF_TEST_CASE_WITHOUT_HEAD(set_persistency__some_backlog__error);
ATF_TEST_CASE_BODY(set_persistency__some_backlog__error)
{
    create_log("error", "test.log");

    std::ifstream input("test.log");
    ATF_REQUIRE(input);

    const pid_t pid = ::getpid();

    std::string line;
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114001 E %s file2:22: Error 1") % pid).str(), line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114005 E %s file2:22: Error 2") % pid).str(), line);
}


ATF_TEST_CASE_WITHOUT_HEAD(set_persistency__some_backlog__info);
ATF_TEST_CASE_BODY(set_persistency__some_backlog__info)
{
    create_log("info", "test.log");

    std::ifstream input("test.log");
    ATF_REQUIRE(input);

    const pid_t pid = ::getpid();

    std::string line;
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114001 E %s file2:22: Error 1") % pid).str(), line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114002 I %s file3:33: Info 1") % pid).str(), line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114003 W %s file4:44: Warning 1") % pid).str(), line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114005 E %s file2:22: Error 2") % pid).str(), line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114006 I %s file3:33: Info 2") % pid).str(), line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114007 W %s file4:44: Warning 2") % pid).str(), line);
}


ATF_TEST_CASE_WITHOUT_HEAD(set_persistency__some_backlog__warning);
ATF_TEST_CASE_BODY(set_persistency__some_backlog__warning)
{
    create_log("warning", "test.log");

    std::ifstream input("test.log");
    ATF_REQUIRE(input);

    const pid_t pid = ::getpid();

    std::string line;
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114001 E %s file2:22: Error 1") % pid).str(), line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114003 W %s file4:44: Warning 1") % pid).str(), line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114005 E %s file2:22: Error 2") % pid).str(), line);
    ATF_REQUIRE(std::getline(input, line).good());
    ATF_REQUIRE_EQ(
        (F("20110319-114007 W %s file4:44: Warning 2") % pid).str(), line);
}


ATF_TEST_CASE(set_persistency__fail);
ATF_TEST_CASE_HEAD(set_persistency__fail)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(set_persistency__fail)
{
    ATF_REQUIRE_THROW_RE(std::range_error, "'foobar'",
                         logging::set_persistency("foobar", fs::path("log")));

    fs::mkdir(fs::path("dir"), 0644);
    ATF_REQUIRE_THROW_RE(std::runtime_error, "dir/fail.log",
                         logging::set_persistency("debug",
                                                  fs::path("dir/fail.log")));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, generate_log_name__before_log);
    ATF_ADD_TEST_CASE(tcs, generate_log_name__after_log);

    ATF_ADD_TEST_CASE(tcs, log);

    ATF_ADD_TEST_CASE(tcs, set_inmemory__reset);

    ATF_ADD_TEST_CASE(tcs, set_persistency__no_backlog);
    ATF_ADD_TEST_CASE(tcs, set_persistency__some_backlog__debug);
    ATF_ADD_TEST_CASE(tcs, set_persistency__some_backlog__error);
    ATF_ADD_TEST_CASE(tcs, set_persistency__some_backlog__info);
    ATF_ADD_TEST_CASE(tcs, set_persistency__some_backlog__warning);
    ATF_ADD_TEST_CASE(tcs, set_persistency__fail);
}
