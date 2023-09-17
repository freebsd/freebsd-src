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

#include "utils/sqlite/c_gate.hpp"

#include <atf-c++.hpp>

#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/test_utils.hpp"

namespace fs = utils::fs;
namespace sqlite = utils::sqlite;


ATF_TEST_CASE_WITHOUT_HEAD(connect);
ATF_TEST_CASE_BODY(connect)
{
    ::sqlite3* raw_db;
    ATF_REQUIRE_EQ(SQLITE_OK, ::sqlite3_open_v2(":memory:", &raw_db,
                                                SQLITE_OPEN_READWRITE, NULL));
    {
        sqlite::database database = sqlite::database_c_gate::connect(raw_db);
        create_test_table(raw(database));
    }
    // If the wrapper object has closed the SQLite 3 database, we will misbehave
    // here either by crashing or not finding our test table.
    verify_test_table(raw_db);
    ::sqlite3_close(raw_db);
}


ATF_TEST_CASE_WITHOUT_HEAD(c_database);
ATF_TEST_CASE_BODY(c_database)
{
    sqlite::database db = sqlite::database::in_memory();
    create_test_table(raw(db));
    {
        sqlite::database_c_gate gate(db);
        ::sqlite3* raw_db = gate.c_database();
        verify_test_table(raw_db);
    }
}


ATF_TEST_CASE(database__db_filename);
ATF_TEST_CASE_HEAD(database__db_filename)
{
    set_md_var("descr", "The current implementation of db_filename() has no "
               "means to access the filename of a database connected to a raw "
               "sqlite3 object");
}
ATF_TEST_CASE_BODY(database__db_filename)
{
    ::sqlite3* raw_db;
    ATF_REQUIRE_EQ(SQLITE_OK, ::sqlite3_open_v2(
        "test.db", &raw_db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL));

    sqlite::database database = sqlite::database_c_gate::connect(raw_db);
    ATF_REQUIRE(!database.db_filename());
    ::sqlite3_close(raw_db);
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, c_database);
    ATF_ADD_TEST_CASE(tcs, connect);
    ATF_ADD_TEST_CASE(tcs, database__db_filename);
}
