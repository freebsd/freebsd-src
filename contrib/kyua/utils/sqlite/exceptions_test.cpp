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

#include "utils/sqlite/exceptions.hpp"

extern "C" {
#include <sqlite3.h>
}

#include <cstring>
#include <string>

#include <atf-c++.hpp>

#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/sqlite/c_gate.hpp"
#include "utils/sqlite/database.hpp"

namespace fs = utils::fs;
namespace sqlite = utils::sqlite;

using utils::none;


ATF_TEST_CASE_WITHOUT_HEAD(error__no_filename);
ATF_TEST_CASE_BODY(error__no_filename)
{
    const sqlite::database db = sqlite::database::in_memory();
    const sqlite::error e(db.db_filename(), "Some text");
    ATF_REQUIRE_EQ("Some text (sqlite db: in-memory or temporary)",
                   std::string(e.what()));
    ATF_REQUIRE_EQ(db.db_filename(), e.db_filename());
}


ATF_TEST_CASE_WITHOUT_HEAD(error__with_filename);
ATF_TEST_CASE_BODY(error__with_filename)
{
    const sqlite::database db = sqlite::database::open(
        fs::path("test.db"), sqlite::open_readwrite | sqlite::open_create);
    const sqlite::error e(db.db_filename(), "Some text");
    ATF_REQUIRE_EQ("Some text (sqlite db: test.db)", std::string(e.what()));
    ATF_REQUIRE_EQ(db.db_filename(), e.db_filename());
}


ATF_TEST_CASE_WITHOUT_HEAD(api_error__explicit);
ATF_TEST_CASE_BODY(api_error__explicit)
{
    const sqlite::api_error e(none, "some_function", "Some text");
    ATF_REQUIRE_EQ(
        "Some text (sqlite op: some_function) "
        "(sqlite db: in-memory or temporary)",
        std::string(e.what()));
    ATF_REQUIRE_EQ("some_function", e.api_function());
}


ATF_TEST_CASE_WITHOUT_HEAD(api_error__from_database);
ATF_TEST_CASE_BODY(api_error__from_database)
{
    sqlite::database db = sqlite::database::open(
        fs::path("test.db"), sqlite::open_readwrite | sqlite::open_create);

    // Use the raw sqlite3 API to cause an error.  Our C++ wrappers catch all
    // errors and reraise them as exceptions, but here we want to handle the raw
    // error directly for testing purposes.
    sqlite::database_c_gate gate(db);
    ::sqlite3_stmt* dummy_stmt;
    const char* query = "ABCDE INVALID QUERY";
    (void)::sqlite3_prepare_v2(gate.c_database(), query, std::strlen(query),
                               &dummy_stmt, NULL);

    const sqlite::api_error e = sqlite::api_error::from_database(
        db, "real_function");
    ATF_REQUIRE_MATCH(
        ".*ABCDE.*\\(sqlite op: real_function\\) \\(sqlite db: test.db\\)",
        std::string(e.what()));
    ATF_REQUIRE_EQ("real_function", e.api_function());
}


ATF_TEST_CASE_WITHOUT_HEAD(invalid_column_error);
ATF_TEST_CASE_BODY(invalid_column_error)
{
    const sqlite::invalid_column_error e(none, "some_name");
    ATF_REQUIRE_EQ("Unknown column 'some_name' "
                   "(sqlite db: in-memory or temporary)",
                   std::string(e.what()));
    ATF_REQUIRE_EQ("some_name", e.column_name());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, error__no_filename);
    ATF_ADD_TEST_CASE(tcs, error__with_filename);

    ATF_ADD_TEST_CASE(tcs, api_error__explicit);
    ATF_ADD_TEST_CASE(tcs, api_error__from_database);

    ATF_ADD_TEST_CASE(tcs, invalid_column_error);
}
