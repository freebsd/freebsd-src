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

#include "cli/cmd_db_exec.hpp"

#include <cstring>

#include <atf-c++.hpp>

#include "utils/format/macros.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/statement.ipp"

namespace sqlite = utils::sqlite;


namespace {


/// Performs a test for the cli::format_cell() function.
///
/// \tparam Cell The type of the value to insert into the test column.
/// \param column_type The SQL type of the test column.
/// \param value The value to insert into the test column.
/// \param exp_value The expected return value of cli::format_cell().
template< class Cell >
static void
do_format_cell_test(const std::string column_type,
                    const Cell& value, const std::string& exp_value)
{
    sqlite::database db = sqlite::database::in_memory();

    sqlite::statement create = db.create_statement(
        F("CREATE TABLE test (column %s)") % column_type);
    create.step_without_results();

    sqlite::statement insert = db.create_statement(
        "INSERT INTO test (column) VALUES (:column)");
    insert.bind(":column", value);
    insert.step_without_results();

    sqlite::statement query = db.create_statement("SELECT * FROM test");
    ATF_REQUIRE(query.step());
    ATF_REQUIRE_EQ(exp_value, cli::format_cell(query, 0));
    ATF_REQUIRE(!query.step());
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(format_cell__blob);
ATF_TEST_CASE_BODY(format_cell__blob)
{
    const char* contents = "Some random contents";
    do_format_cell_test(
        "BLOB", sqlite::blob(contents, std::strlen(contents)),
        F("BLOB of %s bytes") % strlen(contents));
}


ATF_TEST_CASE_WITHOUT_HEAD(format_cell__float);
ATF_TEST_CASE_BODY(format_cell__float)
{
    do_format_cell_test("FLOAT", 3.5, "3.5");
}


ATF_TEST_CASE_WITHOUT_HEAD(format_cell__integer);
ATF_TEST_CASE_BODY(format_cell__integer)
{
    do_format_cell_test("INTEGER", 123456, "123456");
}


ATF_TEST_CASE_WITHOUT_HEAD(format_cell__null);
ATF_TEST_CASE_BODY(format_cell__null)
{
    do_format_cell_test("TEXT", sqlite::null(), "NULL");
}


ATF_TEST_CASE_WITHOUT_HEAD(format_cell__text);
ATF_TEST_CASE_BODY(format_cell__text)
{
    do_format_cell_test("TEXT", "Hello, world", "Hello, world");
}


ATF_TEST_CASE_WITHOUT_HEAD(format_headers);
ATF_TEST_CASE_BODY(format_headers)
{
    sqlite::database db = sqlite::database::in_memory();

    sqlite::statement create = db.create_statement(
        "CREATE TABLE test (c1 TEXT, c2 TEXT, c3 TEXT)");
    create.step_without_results();

    sqlite::statement query = db.create_statement(
        "SELECT c1, c2, c3 AS c3bis FROM test");
    ATF_REQUIRE_EQ("c1,c2,c3bis", cli::format_headers(query));
}


ATF_TEST_CASE_WITHOUT_HEAD(format_row);
ATF_TEST_CASE_BODY(format_row)
{
    sqlite::database db = sqlite::database::in_memory();

    sqlite::statement create = db.create_statement(
        "CREATE TABLE test (c1 TEXT, c2 BLOB)");
    create.step_without_results();

    const char* memory = "BLOB contents";
    sqlite::statement insert = db.create_statement(
        "INSERT INTO test VALUES (:v1, :v2)");
    insert.bind(":v1", "A string");
    insert.bind(":v2", sqlite::blob(memory, std::strlen(memory)));
    insert.step_without_results();

    sqlite::statement query = db.create_statement("SELECT * FROM test");
    query.step();
    ATF_REQUIRE_EQ(
        (F("A string,BLOB of %s bytes") % std::strlen(memory)).str(),
        cli::format_row(query));
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, format_cell__blob);
    ATF_ADD_TEST_CASE(tcs, format_cell__float);
    ATF_ADD_TEST_CASE(tcs, format_cell__integer);
    ATF_ADD_TEST_CASE(tcs, format_cell__null);
    ATF_ADD_TEST_CASE(tcs, format_cell__text);

    ATF_ADD_TEST_CASE(tcs, format_headers);

    ATF_ADD_TEST_CASE(tcs, format_row);
}
