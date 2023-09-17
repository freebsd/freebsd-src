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

#include "utils/sqlite/database.hpp"

#include <atf-c++.hpp>

#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/sqlite/statement.ipp"
#include "utils/sqlite/test_utils.hpp"
#include "utils/sqlite/transaction.hpp"

namespace fs = utils::fs;
namespace sqlite = utils::sqlite;

using utils::optional;


ATF_TEST_CASE_WITHOUT_HEAD(in_memory);
ATF_TEST_CASE_BODY(in_memory)
{
    sqlite::database db = sqlite::database::in_memory();
    create_test_table(raw(db));
    verify_test_table(raw(db));

    ATF_REQUIRE(!fs::exists(fs::path(":memory:")));
}


ATF_TEST_CASE_WITHOUT_HEAD(open__readonly__ok);
ATF_TEST_CASE_BODY(open__readonly__ok)
{
    {
        ::sqlite3* db;
        ATF_REQUIRE_EQ(SQLITE_OK, ::sqlite3_open_v2("test.db", &db,
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL));
        create_test_table(db);
        ::sqlite3_close(db);
    }
    {
        sqlite::database db = sqlite::database::open(fs::path("test.db"),
            sqlite::open_readonly);
        verify_test_table(raw(db));
    }
}


ATF_TEST_CASE_WITHOUT_HEAD(open__readonly__fail);
ATF_TEST_CASE_BODY(open__readonly__fail)
{
    REQUIRE_API_ERROR("sqlite3_open_v2",
        sqlite::database::open(fs::path("missing.db"), sqlite::open_readonly));
    ATF_REQUIRE(!fs::exists(fs::path("missing.db")));
}


ATF_TEST_CASE_WITHOUT_HEAD(open__create__ok);
ATF_TEST_CASE_BODY(open__create__ok)
{
    {
        sqlite::database db = sqlite::database::open(fs::path("test.db"),
            sqlite::open_readwrite | sqlite::open_create);
        ATF_REQUIRE(fs::exists(fs::path("test.db")));
        create_test_table(raw(db));
    }
    {
        ::sqlite3* db;
        ATF_REQUIRE_EQ(SQLITE_OK, ::sqlite3_open_v2("test.db", &db,
            SQLITE_OPEN_READONLY, NULL));
        verify_test_table(db);
        ::sqlite3_close(db);
    }
}


ATF_TEST_CASE(open__create__fail);
ATF_TEST_CASE_HEAD(open__create__fail)
{
    set_md_var("require.user", "unprivileged");
}
ATF_TEST_CASE_BODY(open__create__fail)
{
    fs::mkdir(fs::path("protected"), 0555);
    REQUIRE_API_ERROR("sqlite3_open_v2",
        sqlite::database::open(fs::path("protected/test.db"),
                               sqlite::open_readwrite | sqlite::open_create));
}


ATF_TEST_CASE_WITHOUT_HEAD(temporary);
ATF_TEST_CASE_BODY(temporary)
{
    // We could validate if files go to disk by setting the temp_store_directory
    // PRAGMA to a subdirectory of pwd, and then ensuring the subdirectory is
    // not empty.  However, there does not seem to be a way to force SQLite to
    // unconditionally write the temporary database to disk (even with
    // temp_store = FILE), so this scenary is hard to reproduce.
    sqlite::database db = sqlite::database::temporary();
    create_test_table(raw(db));
    verify_test_table(raw(db));
}


ATF_TEST_CASE_WITHOUT_HEAD(close);
ATF_TEST_CASE_BODY(close)
{
    sqlite::database db = sqlite::database::in_memory();
    db.close();
    // The destructor for the database will run now.  If it does a second close,
    // we may crash, so let's see if we don't.
}


ATF_TEST_CASE_WITHOUT_HEAD(copy);
ATF_TEST_CASE_BODY(copy)
{
    sqlite::database db1 = sqlite::database::in_memory();
    {
        sqlite::database db2 = sqlite::database::in_memory();
        create_test_table(raw(db2));
        db1 = db2;
        verify_test_table(raw(db1));
    }
    // db2 went out of scope.  If the destruction is not properly managed, the
    // memory of db1 may have been invalidated and this would not work.
    verify_test_table(raw(db1));
}


ATF_TEST_CASE_WITHOUT_HEAD(db_filename__in_memory);
ATF_TEST_CASE_BODY(db_filename__in_memory)
{
    const sqlite::database db = sqlite::database::in_memory();
    ATF_REQUIRE(!db.db_filename());
}


ATF_TEST_CASE_WITHOUT_HEAD(db_filename__file);
ATF_TEST_CASE_BODY(db_filename__file)
{
    const sqlite::database db = sqlite::database::open(fs::path("test.db"),
        sqlite::open_readwrite | sqlite::open_create);
    ATF_REQUIRE(db.db_filename());
    ATF_REQUIRE_EQ(fs::path("test.db"), db.db_filename().get());
}


ATF_TEST_CASE_WITHOUT_HEAD(db_filename__temporary);
ATF_TEST_CASE_BODY(db_filename__temporary)
{
    const sqlite::database db = sqlite::database::temporary();
    ATF_REQUIRE(!db.db_filename());
}


ATF_TEST_CASE_WITHOUT_HEAD(db_filename__ok_after_close);
ATF_TEST_CASE_BODY(db_filename__ok_after_close)
{
    sqlite::database db = sqlite::database::open(fs::path("test.db"),
        sqlite::open_readwrite | sqlite::open_create);
    const optional< fs::path > db_filename = db.db_filename();
    ATF_REQUIRE(db_filename);
    db.close();
    ATF_REQUIRE_EQ(db_filename, db.db_filename());
}


ATF_TEST_CASE_WITHOUT_HEAD(exec__ok);
ATF_TEST_CASE_BODY(exec__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec(create_test_table_sql);
    verify_test_table(raw(db));
}


ATF_TEST_CASE_WITHOUT_HEAD(exec__fail);
ATF_TEST_CASE_BODY(exec__fail)
{
    sqlite::database db = sqlite::database::in_memory();
    REQUIRE_API_ERROR("sqlite3_exec",
                      db.exec("SELECT * FROM test"));
    REQUIRE_API_ERROR("sqlite3_exec",
                      db.exec("CREATE TABLE test (col INTEGER PRIMARY KEY);"
                              "FOO BAR"));
    db.exec("SELECT * FROM test");
}


ATF_TEST_CASE_WITHOUT_HEAD(create_statement__ok);
ATF_TEST_CASE_BODY(create_statement__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    sqlite::statement stmt = db.create_statement("SELECT 3");
    // Statement testing happens in statement_test.  We are only interested here
    // in ensuring that the API call exists and runs.
}


ATF_TEST_CASE_WITHOUT_HEAD(begin_transaction);
ATF_TEST_CASE_BODY(begin_transaction)
{
    sqlite::database db = sqlite::database::in_memory();
    sqlite::transaction stmt = db.begin_transaction();
    // Transaction testing happens in transaction_test.  We are only interested
    // here in ensuring that the API call exists and runs.
}


ATF_TEST_CASE_WITHOUT_HEAD(create_statement__fail);
ATF_TEST_CASE_BODY(create_statement__fail)
{
    sqlite::database db = sqlite::database::in_memory();
    REQUIRE_API_ERROR("sqlite3_prepare_v2",
                      db.create_statement("SELECT * FROM missing"));
}


ATF_TEST_CASE_WITHOUT_HEAD(last_insert_rowid);
ATF_TEST_CASE_BODY(last_insert_rowid)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE test (a INTEGER PRIMARY KEY, b INTEGER)");
    db.exec("INSERT INTO test VALUES (723, 5)");
    ATF_REQUIRE_EQ(723, db.last_insert_rowid());
    db.exec("INSERT INTO test VALUES (145, 20)");
    ATF_REQUIRE_EQ(145, db.last_insert_rowid());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, in_memory);

    ATF_ADD_TEST_CASE(tcs, open__readonly__ok);
    ATF_ADD_TEST_CASE(tcs, open__readonly__fail);
    ATF_ADD_TEST_CASE(tcs, open__create__ok);
    ATF_ADD_TEST_CASE(tcs, open__create__fail);

    ATF_ADD_TEST_CASE(tcs, temporary);

    ATF_ADD_TEST_CASE(tcs, close);

    ATF_ADD_TEST_CASE(tcs, copy);

    ATF_ADD_TEST_CASE(tcs, db_filename__in_memory);
    ATF_ADD_TEST_CASE(tcs, db_filename__file);
    ATF_ADD_TEST_CASE(tcs, db_filename__temporary);
    ATF_ADD_TEST_CASE(tcs, db_filename__ok_after_close);

    ATF_ADD_TEST_CASE(tcs, exec__ok);
    ATF_ADD_TEST_CASE(tcs, exec__fail);

    ATF_ADD_TEST_CASE(tcs, begin_transaction);

    ATF_ADD_TEST_CASE(tcs, create_statement__ok);
    ATF_ADD_TEST_CASE(tcs, create_statement__fail);

    ATF_ADD_TEST_CASE(tcs, last_insert_rowid);
}
