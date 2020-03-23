// Copyright 2014 The Kyua Authors.
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

#include "store/layout.hpp"

extern "C" {
#include <unistd.h>
}

#include <iostream>

#include <atf-c++.hpp>

#include "store/exceptions.hpp"
#include "store/layout.hpp"
#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace layout = store::layout;


ATF_TEST_CASE_WITHOUT_HEAD(find_results__latest);
ATF_TEST_CASE_BODY(find_results__latest)
{
    const fs::path store_dir = layout::query_store_dir();
    fs::mkdir_p(store_dir, 0755);

    const std::string test_suite = layout::test_suite_for_path(
        fs::current_path());
    const std::string base = (store_dir / (
        "results." + test_suite + ".")).str();

    atf::utils::create_file(base + "20140613-194515-000000.db", "");
    ATF_REQUIRE_EQ(base + "20140613-194515-000000.db",
                   layout::find_results("LATEST").str());

    atf::utils::create_file(base + "20140614-194515-123456.db", "");
    ATF_REQUIRE_EQ(base + "20140614-194515-123456.db",
                   layout::find_results("LATEST").str());

    atf::utils::create_file(base + "20130614-194515-999999.db", "");
    ATF_REQUIRE_EQ(base + "20140614-194515-123456.db",
                   layout::find_results("LATEST").str());
}


ATF_TEST_CASE_WITHOUT_HEAD(find_results__directory);
ATF_TEST_CASE_BODY(find_results__directory)
{
    const fs::path store_dir = layout::query_store_dir();
    fs::mkdir_p(store_dir, 0755);

    const fs::path dir1("dir1/foo");
    fs::mkdir_p(dir1, 0755);
    const fs::path dir2("dir1/bar");
    fs::mkdir_p(dir2, 0755);

    const std::string base1 = (store_dir / (
        "results." + layout::test_suite_for_path(dir1) + ".")).str();
    const std::string base2 = (store_dir / (
        "results." + layout::test_suite_for_path(dir2) + ".")).str();

    atf::utils::create_file(base1 + "20140613-194515-000000.db", "");
    ATF_REQUIRE_EQ(base1 + "20140613-194515-000000.db",
                   layout::find_results(dir1.str()).str());

    atf::utils::create_file(base2 + "20140615-111111-000000.db", "");
    ATF_REQUIRE_EQ(base2 + "20140615-111111-000000.db",
                   layout::find_results(dir2.str()).str());

    atf::utils::create_file(base1 + "20140614-194515-123456.db", "");
    ATF_REQUIRE_EQ(base1 + "20140614-194515-123456.db",
                   layout::find_results(dir1.str()).str());

    atf::utils::create_file(base1 + "20130614-194515-999999.db", "");
    ATF_REQUIRE_EQ(base1 + "20140614-194515-123456.db",
                   layout::find_results(dir1.str()).str());
}


ATF_TEST_CASE_WITHOUT_HEAD(find_results__file);
ATF_TEST_CASE_BODY(find_results__file)
{
    const fs::path store_dir = layout::query_store_dir();
    fs::mkdir_p(store_dir, 0755);

    atf::utils::create_file("a-file.db", "");
    ATF_REQUIRE_EQ(fs::path("a-file.db").to_absolute(),
                   layout::find_results("a-file.db"));
}


ATF_TEST_CASE_WITHOUT_HEAD(find_results__id);
ATF_TEST_CASE_BODY(find_results__id)
{
    const fs::path store_dir = layout::query_store_dir();
    fs::mkdir_p(store_dir, 0755);

    const fs::path dir1("dir1/foo");
    fs::mkdir_p(dir1, 0755);
    const fs::path dir2("dir1/bar");
    fs::mkdir_p(dir2, 0755);

    const std::string id1 = layout::test_suite_for_path(dir1);
    const std::string base1 = (store_dir / ("results." + id1 + ".")).str();
    const std::string id2 = layout::test_suite_for_path(dir2);
    const std::string base2 = (store_dir / ("results." + id2 + ".")).str();

    atf::utils::create_file(base1 + "20140613-194515-000000.db", "");
    ATF_REQUIRE_EQ(base1 + "20140613-194515-000000.db",
                   layout::find_results(id1).str());

    atf::utils::create_file(base2 + "20140615-111111-000000.db", "");
    ATF_REQUIRE_EQ(base2 + "20140615-111111-000000.db",
                   layout::find_results(id2).str());

    atf::utils::create_file(base1 + "20140614-194515-123456.db", "");
    ATF_REQUIRE_EQ(base1 + "20140614-194515-123456.db",
                   layout::find_results(id1).str());

    atf::utils::create_file(base1 + "20130614-194515-999999.db", "");
    ATF_REQUIRE_EQ(base1 + "20140614-194515-123456.db",
                   layout::find_results(id1).str());
}


ATF_TEST_CASE_WITHOUT_HEAD(find_results__id_with_timestamp);
ATF_TEST_CASE_BODY(find_results__id_with_timestamp)
{
    const fs::path store_dir = layout::query_store_dir();
    fs::mkdir_p(store_dir, 0755);

    const fs::path dir1("dir1/foo");
    fs::mkdir_p(dir1, 0755);
    const fs::path dir2("dir1/bar");
    fs::mkdir_p(dir2, 0755);

    const std::string id1 = layout::test_suite_for_path(dir1);
    const std::string base1 = (store_dir / ("results." + id1 + ".")).str();
    const std::string id2 = layout::test_suite_for_path(dir2);
    const std::string base2 = (store_dir / ("results." + id2 + ".")).str();

    atf::utils::create_file(base1 + "20140613-194515-000000.db", "");
    atf::utils::create_file(base2 + "20140615-111111-000000.db", "");
    atf::utils::create_file(base1 + "20140614-194515-123456.db", "");
    atf::utils::create_file(base1 + "20130614-194515-999999.db", "");

    ATF_REQUIRE_MATCH(
        "_dir1_foo.20140613-194515-000000.db$",
        layout::find_results(id1 + ".20140613-194515-000000").str());

    ATF_REQUIRE_MATCH(
        "_dir1_foo.20140614-194515-123456.db$",
        layout::find_results(id1 + ".20140614-194515-123456").str());

    ATF_REQUIRE_MATCH(
        "_dir1_bar.20140615-111111-000000.db$",
        layout::find_results(id2 + ".20140615-111111-000000").str());
}


ATF_TEST_CASE_WITHOUT_HEAD(find_results__not_found);
ATF_TEST_CASE_BODY(find_results__not_found)
{
    ATF_REQUIRE_THROW_RE(
        store::error,
        "No previous results file found for test suite foo_bar",
        layout::find_results("foo_bar"));

    const fs::path store_dir = layout::query_store_dir();
    fs::mkdir_p(store_dir, 0755);
    ATF_REQUIRE_THROW_RE(
        store::error,
        "No previous results file found for test suite foo_bar",
        layout::find_results("foo_bar"));

    const char* candidates[] = {
        "results.foo.20140613-194515-012345.db",  // Bad test suite.
        "results.foo_bar.20140613-194515-012345",  // Missing extension.
        "foo_bar.20140613-194515-012345.db",  // Missing prefix.
        "results.foo_bar.2010613-194515-012345.db",  // Bad date.
        "results.foo_bar.20140613-19515-012345.db",  // Bad time.
        "results.foo_bar.20140613-194515-01245.db",  // Bad microseconds.
        NULL,
    };
    for (const char** candidate = candidates; *candidate != NULL; ++candidate) {
        std::cout << "Current candidate: " << *candidate << '\n';
        atf::utils::create_file((store_dir / *candidate).str(), "");
        ATF_REQUIRE_THROW_RE(
            store::error,
            "No previous results file found for test suite foo_bar",
            layout::find_results("foo_bar"));
    }

    atf::utils::create_file(
        (store_dir / "results.foo_bar.20140613-194515-012345.db").str(), "");
    layout::find_results("foo_bar");  // Expected not to throw.
}


ATF_TEST_CASE_WITHOUT_HEAD(new_db__new);
ATF_TEST_CASE_BODY(new_db__new)
{
    datetime::set_mock_now(2014, 6, 13, 19, 45, 15, 5000);
    ATF_REQUIRE(!fs::exists(fs::path(".kyua/store")));
    const layout::results_id_file_pair results = layout::new_db(
        "NEW", fs::path("/some/path/to/the/suite"));
    ATF_REQUIRE( fs::exists(fs::path(".kyua/store")));
    ATF_REQUIRE( fs::is_directory(fs::path(".kyua/store")));

    const std::string id = "some_path_to_the_suite.20140613-194515-005000";
    ATF_REQUIRE_EQ(id, results.first);
    ATF_REQUIRE_EQ(layout::query_store_dir() / ("results." + id + ".db"),
                   results.second);
}


ATF_TEST_CASE_WITHOUT_HEAD(new_db__explicit);
ATF_TEST_CASE_BODY(new_db__explicit)
{
    ATF_REQUIRE(!fs::exists(fs::path(".kyua/store")));
    const layout::results_id_file_pair results = layout::new_db(
        "foo/results-file.db", fs::path("unused"));
    ATF_REQUIRE(!fs::exists(fs::path(".kyua/store")));
    ATF_REQUIRE(!fs::exists(fs::path("foo")));

    ATF_REQUIRE(results.first.empty());
    ATF_REQUIRE_EQ(fs::path("foo/results-file.db"), results.second);
}


ATF_TEST_CASE_WITHOUT_HEAD(new_db_for_migration);
ATF_TEST_CASE_BODY(new_db_for_migration)
{
    ATF_REQUIRE(!fs::exists(fs::path(".kyua/store")));
    const fs::path results_file = layout::new_db_for_migration(
        fs::path("/some/root"),
        datetime::timestamp::from_values(2014, 7, 30, 10, 5, 20, 76500));
    ATF_REQUIRE( fs::exists(fs::path(".kyua/store")));
    ATF_REQUIRE( fs::is_directory(fs::path(".kyua/store")));

    ATF_REQUIRE_EQ(
        layout::query_store_dir() /
        "results.some_root.20140730-100520-076500.db",
        results_file);
}


ATF_TEST_CASE_WITHOUT_HEAD(query_store_dir__home_absolute);
ATF_TEST_CASE_BODY(query_store_dir__home_absolute)
{
    const fs::path home = fs::current_path() / "homedir";
    utils::setenv("HOME", home.str());
    const fs::path store_dir = layout::query_store_dir();
    ATF_REQUIRE(store_dir.is_absolute());
    ATF_REQUIRE_EQ(home / ".kyua/store", store_dir);
}


ATF_TEST_CASE_WITHOUT_HEAD(query_store_dir__home_relative);
ATF_TEST_CASE_BODY(query_store_dir__home_relative)
{
    const fs::path home("homedir");
    utils::setenv("HOME", home.str());
    const fs::path store_dir = layout::query_store_dir();
    ATF_REQUIRE(store_dir.is_absolute());
    ATF_REQUIRE_MATCH((home / ".kyua/store").str(), store_dir.str());
}


ATF_TEST_CASE_WITHOUT_HEAD(query_store_dir__no_home);
ATF_TEST_CASE_BODY(query_store_dir__no_home)
{
    utils::unsetenv("HOME");
    ATF_REQUIRE_EQ(fs::current_path(), layout::query_store_dir());
}


ATF_TEST_CASE_WITHOUT_HEAD(test_suite_for_path__absolute);
ATF_TEST_CASE_BODY(test_suite_for_path__absolute)
{
    ATF_REQUIRE_EQ("dir1_dir2_dir3",
                   layout::test_suite_for_path(fs::path("/dir1/dir2/dir3")));
    ATF_REQUIRE_EQ("dir1",
                   layout::test_suite_for_path(fs::path("/dir1")));
    ATF_REQUIRE_EQ("dir1_dir2",
                   layout::test_suite_for_path(fs::path("/dir1///dir2")));
}


ATF_TEST_CASE_WITHOUT_HEAD(test_suite_for_path__relative);
ATF_TEST_CASE_BODY(test_suite_for_path__relative)
{
    const std::string test_suite = layout::test_suite_for_path(
        fs::path("foo/bar"));
    ATF_REQUIRE_MATCH("_foo_bar$", test_suite);
    ATF_REQUIRE_MATCH("^[^_]", test_suite);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, find_results__latest);
    ATF_ADD_TEST_CASE(tcs, find_results__directory);
    ATF_ADD_TEST_CASE(tcs, find_results__file);
    ATF_ADD_TEST_CASE(tcs, find_results__id);
    ATF_ADD_TEST_CASE(tcs, find_results__id_with_timestamp);
    ATF_ADD_TEST_CASE(tcs, find_results__not_found);

    ATF_ADD_TEST_CASE(tcs, new_db__new);
    ATF_ADD_TEST_CASE(tcs, new_db__explicit);

    ATF_ADD_TEST_CASE(tcs, new_db_for_migration);

    ATF_ADD_TEST_CASE(tcs, query_store_dir__home_absolute);
    ATF_ADD_TEST_CASE(tcs, query_store_dir__home_relative);
    ATF_ADD_TEST_CASE(tcs, query_store_dir__no_home);

    ATF_ADD_TEST_CASE(tcs, test_suite_for_path__absolute);
    ATF_ADD_TEST_CASE(tcs, test_suite_for_path__relative);
}
