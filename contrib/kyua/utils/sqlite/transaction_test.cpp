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

#include "utils/sqlite/transaction.hpp"

#include <atf-c++.hpp>

#include "utils/format/macros.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.ipp"

namespace sqlite = utils::sqlite;


namespace {


/// Ensures that a table has a single specific value in a column.
///
/// \param db The SQLite database.
/// \param table_name The table to be checked.
/// \param column_name The column to be checked.
/// \param exp_value The value expected to be found in the column.
///
/// \return True if the column contains a single value and it matches exp_value;
/// false if not.  If the query fails, the calling test is marked as bad.
static bool
check_in_table(sqlite::database& db, const char* table_name,
               const char* column_name, int exp_value)
{
    sqlite::statement stmt = db.create_statement(
        F("SELECT * FROM %s WHERE %s == %s") % table_name % column_name %
        exp_value);
    if (!stmt.step())
        return false;
    if (stmt.step())
        ATF_FAIL("More than one value found in table");
    return true;
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(automatic_rollback);
ATF_TEST_CASE_BODY(automatic_rollback)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE t (col INTEGER PRIMARY KEY)");
    db.exec("INSERT INTO t VALUES (3)");
    {
        sqlite::transaction tx = db.begin_transaction();
        db.exec("INSERT INTO t VALUES (5)");
    }
    ATF_REQUIRE( check_in_table(db, "t", "col", 3));
    ATF_REQUIRE(!check_in_table(db, "t", "col", 5));
}


ATF_TEST_CASE_WITHOUT_HEAD(explicit_commit);
ATF_TEST_CASE_BODY(explicit_commit)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE t (col INTEGER PRIMARY KEY)");
    db.exec("INSERT INTO t VALUES (3)");
    {
        sqlite::transaction tx = db.begin_transaction();
        db.exec("INSERT INTO t VALUES (5)");
        tx.commit();
    }
    ATF_REQUIRE(check_in_table(db, "t", "col", 3));
    ATF_REQUIRE(check_in_table(db, "t", "col", 5));
}


ATF_TEST_CASE_WITHOUT_HEAD(explicit_rollback);
ATF_TEST_CASE_BODY(explicit_rollback)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE t (col INTEGER PRIMARY KEY)");
    db.exec("INSERT INTO t VALUES (3)");
    {
        sqlite::transaction tx = db.begin_transaction();
        db.exec("INSERT INTO t VALUES (5)");
        tx.rollback();
    }
    ATF_REQUIRE( check_in_table(db, "t", "col", 3));
    ATF_REQUIRE(!check_in_table(db, "t", "col", 5));
}


ATF_TEST_CASE_WITHOUT_HEAD(nested_fail);
ATF_TEST_CASE_BODY(nested_fail)
{
    sqlite::database db = sqlite::database::in_memory();
    {
        sqlite::transaction tx = db.begin_transaction();
        ATF_REQUIRE_THROW(sqlite::error, db.begin_transaction());
    }
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, automatic_rollback);
    ATF_ADD_TEST_CASE(tcs, explicit_commit);
    ATF_ADD_TEST_CASE(tcs, explicit_rollback);
    ATF_ADD_TEST_CASE(tcs, nested_fail);
}
