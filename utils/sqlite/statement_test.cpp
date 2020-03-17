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

#include "utils/sqlite/statement.ipp"

extern "C" {
#include <stdint.h>
}

#include <cstring>
#include <iostream>

#include <atf-c++.hpp>

#include "utils/sqlite/database.hpp"
#include "utils/sqlite/test_utils.hpp"

namespace sqlite = utils::sqlite;


ATF_TEST_CASE_WITHOUT_HEAD(step__ok);
ATF_TEST_CASE_BODY(step__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    sqlite::statement stmt = db.create_statement(
        "CREATE TABLE foo (a INTEGER PRIMARY KEY)");
    ATF_REQUIRE_THROW(sqlite::error, db.exec("SELECT * FROM foo"));
    ATF_REQUIRE(!stmt.step());
    db.exec("SELECT * FROM foo");
}


ATF_TEST_CASE_WITHOUT_HEAD(step__many);
ATF_TEST_CASE_BODY(step__many)
{
    sqlite::database db = sqlite::database::in_memory();
    create_test_table(raw(db));
    sqlite::statement stmt = db.create_statement(
        "SELECT prime FROM test ORDER BY prime");
    for (int i = 0; i < 5; i++)
        ATF_REQUIRE(stmt.step());
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(step__fail);
ATF_TEST_CASE_BODY(step__fail)
{
    sqlite::database db = sqlite::database::in_memory();
    sqlite::statement stmt = db.create_statement(
        "CREATE TABLE foo (a INTEGER PRIMARY KEY)");
    ATF_REQUIRE(!stmt.step());
    REQUIRE_API_ERROR("sqlite3_step", stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(step_without_results__ok);
ATF_TEST_CASE_BODY(step_without_results__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    sqlite::statement stmt = db.create_statement(
        "CREATE TABLE foo (a INTEGER PRIMARY KEY)");
    ATF_REQUIRE_THROW(sqlite::error, db.exec("SELECT * FROM foo"));
    stmt.step_without_results();
    db.exec("SELECT * FROM foo");
}


ATF_TEST_CASE_WITHOUT_HEAD(step_without_results__fail);
ATF_TEST_CASE_BODY(step_without_results__fail)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a INTEGER PRIMARY KEY)");
    db.exec("INSERT INTO foo VALUES (3)");
    sqlite::statement stmt = db.create_statement(
        "INSERT INTO foo VALUES (3)");
    REQUIRE_API_ERROR("sqlite3_step", stmt.step_without_results());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_count);
ATF_TEST_CASE_BODY(column_count)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a INTEGER PRIMARY KEY, b INTEGER, c TEXT);"
            "INSERT INTO foo VALUES (5, 3, 'foo');");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(3, stmt.column_count());
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_name__ok);
ATF_TEST_CASE_BODY(column_name__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (first INTEGER PRIMARY KEY, second TEXT);"
            "INSERT INTO foo VALUES (5, 'foo');");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ("first", stmt.column_name(0));
    ATF_REQUIRE_EQ("second", stmt.column_name(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_name__fail);
ATF_TEST_CASE_BODY(column_name__fail)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (first INTEGER PRIMARY KEY);"
            "INSERT INTO foo VALUES (5);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ("first", stmt.column_name(0));
    REQUIRE_API_ERROR("sqlite3_column_name", stmt.column_name(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_type__ok);
ATF_TEST_CASE_BODY(column_type__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a_blob BLOB,"
            "                  a_float FLOAT,"
            "                  an_integer INTEGER,"
            "                  a_null BLOB,"
            "                  a_text TEXT);"
            "INSERT INTO foo VALUES (x'0102', 0.3, 5, NULL, 'foo bar');"
            "INSERT INTO foo VALUES (NULL, NULL, NULL, NULL, NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE(sqlite::type_blob == stmt.column_type(0));
    ATF_REQUIRE(sqlite::type_float == stmt.column_type(1));
    ATF_REQUIRE(sqlite::type_integer == stmt.column_type(2));
    ATF_REQUIRE(sqlite::type_null == stmt.column_type(3));
    ATF_REQUIRE(sqlite::type_text == stmt.column_type(4));
    ATF_REQUIRE(stmt.step());
    for (int i = 0; i < stmt.column_count(); i++)
        ATF_REQUIRE(sqlite::type_null == stmt.column_type(i));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_type__out_of_range);
ATF_TEST_CASE_BODY(column_type__out_of_range)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a INTEGER PRIMARY KEY);"
            "INSERT INTO foo VALUES (1);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE(sqlite::type_integer == stmt.column_type(0));
    ATF_REQUIRE(sqlite::type_null == stmt.column_type(1));
    ATF_REQUIRE(sqlite::type_null == stmt.column_type(512));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_id__ok);
ATF_TEST_CASE_BODY(column_id__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (bar INTEGER PRIMARY KEY, "
            "                  baz INTEGER);"
            "INSERT INTO foo VALUES (1, 2);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(0, stmt.column_id("bar"));
    ATF_REQUIRE_EQ(1, stmt.column_id("baz"));
    ATF_REQUIRE_EQ(0, stmt.column_id("bar"));
    ATF_REQUIRE_EQ(1, stmt.column_id("baz"));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_id__missing);
ATF_TEST_CASE_BODY(column_id__missing)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (bar INTEGER PRIMARY KEY, "
            "                  baz INTEGER);"
            "INSERT INTO foo VALUES (1, 2);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(0, stmt.column_id("bar"));
    try {
        stmt.column_id("bazo");
        fail("invalid_column_error not raised");
    } catch (const sqlite::invalid_column_error& e) {
        ATF_REQUIRE_EQ("bazo", e.column_name());
    }
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_blob);
ATF_TEST_CASE_BODY(column_blob)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a INTEGER, b BLOB, c INTEGER);"
            "INSERT INTO foo VALUES (NULL, x'cafe', NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    const sqlite::blob blob = stmt.column_blob(1);
    ATF_REQUIRE_EQ(0xca, static_cast< const uint8_t* >(blob.memory)[0]);
    ATF_REQUIRE_EQ(0xfe, static_cast< const uint8_t* >(blob.memory)[1]);
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_double);
ATF_TEST_CASE_BODY(column_double)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a INTEGER, b DOUBLE, c INTEGER);"
            "INSERT INTO foo VALUES (NULL, 0.5, NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(0.5, stmt.column_double(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_int__ok);
ATF_TEST_CASE_BODY(column_int__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a TEXT, b INTEGER, c TEXT);"
            "INSERT INTO foo VALUES (NULL, 987, NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(987, stmt.column_int(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_int__overflow);
ATF_TEST_CASE_BODY(column_int__overflow)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a TEXT, b INTEGER, c TEXT);"
            "INSERT INTO foo VALUES (NULL, 4294967419, NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(123, stmt.column_int(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_int64);
ATF_TEST_CASE_BODY(column_int64)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a TEXT, b INTEGER, c TEXT);"
            "INSERT INTO foo VALUES (NULL, 4294967419, NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(4294967419LL, stmt.column_int64(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_text);
ATF_TEST_CASE_BODY(column_text)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a INTEGER, b TEXT, c INTEGER);"
            "INSERT INTO foo VALUES (NULL, 'foo bar', NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ("foo bar", stmt.column_text(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_bytes__blob);
ATF_TEST_CASE_BODY(column_bytes__blob)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a BLOB);"
            "INSERT INTO foo VALUES (x'12345678');");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(4, stmt.column_bytes(0));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(column_bytes__text);
ATF_TEST_CASE_BODY(column_bytes__text)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a TEXT);"
            "INSERT INTO foo VALUES ('foo bar');");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(7, stmt.column_bytes(0));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(safe_column_blob__ok);
ATF_TEST_CASE_BODY(safe_column_blob__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a INTEGER, b BLOB, c INTEGER);"
            "INSERT INTO foo VALUES (NULL, x'cafe', NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    const sqlite::blob blob = stmt.safe_column_blob("b");
    ATF_REQUIRE_EQ(0xca, static_cast< const uint8_t* >(blob.memory)[0]);
    ATF_REQUIRE_EQ(0xfe, static_cast< const uint8_t* >(blob.memory)[1]);
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(safe_column_blob__fail);
ATF_TEST_CASE_BODY(safe_column_blob__fail)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a INTEGER);"
            "INSERT INTO foo VALUES (123);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_THROW(sqlite::invalid_column_error,
                      stmt.safe_column_blob("b"));
    ATF_REQUIRE_THROW_RE(sqlite::error, "not a blob",
                         stmt.safe_column_blob("a"));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(safe_column_double__ok);
ATF_TEST_CASE_BODY(safe_column_double__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a INTEGER, b DOUBLE, c INTEGER);"
            "INSERT INTO foo VALUES (NULL, 0.5, NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(0.5, stmt.safe_column_double("b"));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(safe_column_double__fail);
ATF_TEST_CASE_BODY(safe_column_double__fail)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a INTEGER);"
            "INSERT INTO foo VALUES (NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_THROW(sqlite::invalid_column_error,
                      stmt.safe_column_double("b"));
    ATF_REQUIRE_THROW_RE(sqlite::error, "not a float",
                         stmt.safe_column_double("a"));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(safe_column_int__ok);
ATF_TEST_CASE_BODY(safe_column_int__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a TEXT, b INTEGER, c TEXT);"
            "INSERT INTO foo VALUES (NULL, 987, NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(987, stmt.safe_column_int("b"));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(safe_column_int__fail);
ATF_TEST_CASE_BODY(safe_column_int__fail)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a TEXT);"
            "INSERT INTO foo VALUES ('def');");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_THROW(sqlite::invalid_column_error,
                      stmt.safe_column_int("b"));
    ATF_REQUIRE_THROW_RE(sqlite::error, "not an integer",
                         stmt.safe_column_int("a"));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(safe_column_int64__ok);
ATF_TEST_CASE_BODY(safe_column_int64__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a TEXT, b INTEGER, c TEXT);"
            "INSERT INTO foo VALUES (NULL, 4294967419, NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(4294967419LL, stmt.safe_column_int64("b"));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(safe_column_int64__fail);
ATF_TEST_CASE_BODY(safe_column_int64__fail)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a TEXT);"
            "INSERT INTO foo VALUES ('abc');");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_THROW(sqlite::invalid_column_error,
                      stmt.safe_column_int64("b"));
    ATF_REQUIRE_THROW_RE(sqlite::error, "not an integer",
                         stmt.safe_column_int64("a"));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(safe_column_text__ok);
ATF_TEST_CASE_BODY(safe_column_text__ok)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a INTEGER, b TEXT, c INTEGER);"
            "INSERT INTO foo VALUES (NULL, 'foo bar', NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ("foo bar", stmt.safe_column_text("b"));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(safe_column_text__fail);
ATF_TEST_CASE_BODY(safe_column_text__fail)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a INTEGER);"
            "INSERT INTO foo VALUES (NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_THROW(sqlite::invalid_column_error,
                      stmt.safe_column_text("b"));
    ATF_REQUIRE_THROW_RE(sqlite::error, "not a string",
                         stmt.safe_column_text("a"));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(safe_column_bytes__ok__blob);
ATF_TEST_CASE_BODY(safe_column_bytes__ok__blob)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a BLOB);"
            "INSERT INTO foo VALUES (x'12345678');");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(4, stmt.safe_column_bytes("a"));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(safe_column_bytes__ok__text);
ATF_TEST_CASE_BODY(safe_column_bytes__ok__text)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a TEXT);"
            "INSERT INTO foo VALUES ('foo bar');");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_EQ(7, stmt.safe_column_bytes("a"));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(safe_column_bytes__fail);
ATF_TEST_CASE_BODY(safe_column_bytes__fail)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a TEXT);"
            "INSERT INTO foo VALUES (NULL);");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE_THROW(sqlite::invalid_column_error,
                      stmt.safe_column_bytes("b"));
    ATF_REQUIRE_THROW_RE(sqlite::error, "not a blob or a string",
                         stmt.safe_column_bytes("a"));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(reset);
ATF_TEST_CASE_BODY(reset)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE foo (a TEXT);"
            "INSERT INTO foo VALUES ('foo bar');");
    sqlite::statement stmt = db.create_statement("SELECT * FROM foo");
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE(!stmt.step());
    stmt.reset();
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(bind__blob);
ATF_TEST_CASE_BODY(bind__blob)
{
    sqlite::database db = sqlite::database::in_memory();
    sqlite::statement stmt = db.create_statement("SELECT 3, ?");

    const unsigned char blob[] = {0xca, 0xfe};
    stmt.bind(1, sqlite::blob(static_cast< const void* >(blob), 2));
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE(sqlite::type_integer == stmt.column_type(0));
    ATF_REQUIRE_EQ(3, stmt.column_int(0));
    ATF_REQUIRE(sqlite::type_blob == stmt.column_type(1));
    const unsigned char* ret_blob =
        static_cast< const unsigned char* >(stmt.column_blob(1).memory);
    ATF_REQUIRE(std::memcmp(blob, ret_blob, 2) == 0);
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(bind__double);
ATF_TEST_CASE_BODY(bind__double)
{
    sqlite::database db = sqlite::database::in_memory();
    sqlite::statement stmt = db.create_statement("SELECT 3, ?");

    stmt.bind(1, 0.5);
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE(sqlite::type_integer == stmt.column_type(0));
    ATF_REQUIRE_EQ(3, stmt.column_int(0));
    ATF_REQUIRE(sqlite::type_float == stmt.column_type(1));
    ATF_REQUIRE_EQ(0.5, stmt.column_double(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(bind__int);
ATF_TEST_CASE_BODY(bind__int)
{
    sqlite::database db = sqlite::database::in_memory();
    sqlite::statement stmt = db.create_statement("SELECT 3, ?");

    stmt.bind(1, 123);
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE(sqlite::type_integer == stmt.column_type(0));
    ATF_REQUIRE_EQ(3, stmt.column_int(0));
    ATF_REQUIRE(sqlite::type_integer == stmt.column_type(1));
    ATF_REQUIRE_EQ(123, stmt.column_int(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(bind__int64);
ATF_TEST_CASE_BODY(bind__int64)
{
    sqlite::database db = sqlite::database::in_memory();
    sqlite::statement stmt = db.create_statement("SELECT 3, ?");

    stmt.bind(1, static_cast< int64_t >(4294967419LL));
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE(sqlite::type_integer == stmt.column_type(0));
    ATF_REQUIRE_EQ(3, stmt.column_int(0));
    ATF_REQUIRE(sqlite::type_integer == stmt.column_type(1));
    ATF_REQUIRE_EQ(4294967419LL, stmt.column_int64(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(bind__null);
ATF_TEST_CASE_BODY(bind__null)
{
    sqlite::database db = sqlite::database::in_memory();
    sqlite::statement stmt = db.create_statement("SELECT 3, ?");

    stmt.bind(1, sqlite::null());
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE(sqlite::type_integer == stmt.column_type(0));
    ATF_REQUIRE_EQ(3, stmt.column_int(0));
    ATF_REQUIRE(sqlite::type_null == stmt.column_type(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(bind__text);
ATF_TEST_CASE_BODY(bind__text)
{
    sqlite::database db = sqlite::database::in_memory();
    sqlite::statement stmt = db.create_statement("SELECT 3, ?");

    const std::string str = "Hello";
    stmt.bind(1, str);
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE(sqlite::type_integer == stmt.column_type(0));
    ATF_REQUIRE_EQ(3, stmt.column_int(0));
    ATF_REQUIRE(sqlite::type_text == stmt.column_type(1));
    ATF_REQUIRE_EQ(str, stmt.column_text(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(bind__text__transient);
ATF_TEST_CASE_BODY(bind__text__transient)
{
    sqlite::database db = sqlite::database::in_memory();
    sqlite::statement stmt = db.create_statement("SELECT 3, :foo");

    {
        const std::string str = "Hello";
        stmt.bind(":foo", str);
    }

    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE(sqlite::type_integer == stmt.column_type(0));
    ATF_REQUIRE_EQ(3, stmt.column_int(0));
    ATF_REQUIRE(sqlite::type_text == stmt.column_type(1));
    ATF_REQUIRE_EQ(std::string("Hello"), stmt.column_text(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(bind__by_name);
ATF_TEST_CASE_BODY(bind__by_name)
{
    sqlite::database db = sqlite::database::in_memory();
    sqlite::statement stmt = db.create_statement("SELECT 3, :foo");

    const std::string str = "Hello";
    stmt.bind(":foo", str);
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE(sqlite::type_integer == stmt.column_type(0));
    ATF_REQUIRE_EQ(3, stmt.column_int(0));
    ATF_REQUIRE(sqlite::type_text == stmt.column_type(1));
    ATF_REQUIRE_EQ(str, stmt.column_text(1));
    ATF_REQUIRE(!stmt.step());
}


ATF_TEST_CASE_WITHOUT_HEAD(bind_parameter_count);
ATF_TEST_CASE_BODY(bind_parameter_count)
{
    sqlite::database db = sqlite::database::in_memory();
    sqlite::statement stmt = db.create_statement("SELECT 3, ?, ?");
    ATF_REQUIRE_EQ(2, stmt.bind_parameter_count());
}


ATF_TEST_CASE_WITHOUT_HEAD(bind_parameter_index);
ATF_TEST_CASE_BODY(bind_parameter_index)
{
    sqlite::database db = sqlite::database::in_memory();
    sqlite::statement stmt = db.create_statement("SELECT 3, :foo, ?, :bar");
    ATF_REQUIRE_EQ(1, stmt.bind_parameter_index(":foo"));
    ATF_REQUIRE_EQ(3, stmt.bind_parameter_index(":bar"));
}


ATF_TEST_CASE_WITHOUT_HEAD(bind_parameter_name);
ATF_TEST_CASE_BODY(bind_parameter_name)
{
    sqlite::database db = sqlite::database::in_memory();
    sqlite::statement stmt = db.create_statement("SELECT 3, :foo, ?, :bar");
    ATF_REQUIRE_EQ(":foo", stmt.bind_parameter_name(1));
    ATF_REQUIRE_EQ(":bar", stmt.bind_parameter_name(3));
}


ATF_TEST_CASE_WITHOUT_HEAD(clear_bindings);
ATF_TEST_CASE_BODY(clear_bindings)
{
    sqlite::database db = sqlite::database::in_memory();
    sqlite::statement stmt = db.create_statement("SELECT 3, ?");

    stmt.bind(1, 5);
    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE(sqlite::type_integer == stmt.column_type(0));
    ATF_REQUIRE_EQ(3, stmt.column_int(0));
    ATF_REQUIRE(sqlite::type_integer == stmt.column_type(1));
    ATF_REQUIRE_EQ(5, stmt.column_int(1));
    stmt.clear_bindings();
    stmt.reset();

    ATF_REQUIRE(stmt.step());
    ATF_REQUIRE(sqlite::type_integer == stmt.column_type(0));
    ATF_REQUIRE_EQ(3, stmt.column_int(0));
    ATF_REQUIRE(sqlite::type_null == stmt.column_type(1));

    ATF_REQUIRE(!stmt.step());
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, step__ok);
    ATF_ADD_TEST_CASE(tcs, step__many);
    ATF_ADD_TEST_CASE(tcs, step__fail);

    ATF_ADD_TEST_CASE(tcs, step_without_results__ok);
    ATF_ADD_TEST_CASE(tcs, step_without_results__fail);

    ATF_ADD_TEST_CASE(tcs, column_count);

    ATF_ADD_TEST_CASE(tcs, column_name__ok);
    ATF_ADD_TEST_CASE(tcs, column_name__fail);

    ATF_ADD_TEST_CASE(tcs, column_type__ok);
    ATF_ADD_TEST_CASE(tcs, column_type__out_of_range);

    ATF_ADD_TEST_CASE(tcs, column_id__ok);
    ATF_ADD_TEST_CASE(tcs, column_id__missing);

    ATF_ADD_TEST_CASE(tcs, column_blob);
    ATF_ADD_TEST_CASE(tcs, column_double);
    ATF_ADD_TEST_CASE(tcs, column_int__ok);
    ATF_ADD_TEST_CASE(tcs, column_int__overflow);
    ATF_ADD_TEST_CASE(tcs, column_int64);
    ATF_ADD_TEST_CASE(tcs, column_text);

    ATF_ADD_TEST_CASE(tcs, column_bytes__blob);
    ATF_ADD_TEST_CASE(tcs, column_bytes__text);

    ATF_ADD_TEST_CASE(tcs, safe_column_blob__ok);
    ATF_ADD_TEST_CASE(tcs, safe_column_blob__fail);
    ATF_ADD_TEST_CASE(tcs, safe_column_double__ok);
    ATF_ADD_TEST_CASE(tcs, safe_column_double__fail);
    ATF_ADD_TEST_CASE(tcs, safe_column_int__ok);
    ATF_ADD_TEST_CASE(tcs, safe_column_int__fail);
    ATF_ADD_TEST_CASE(tcs, safe_column_int64__ok);
    ATF_ADD_TEST_CASE(tcs, safe_column_int64__fail);
    ATF_ADD_TEST_CASE(tcs, safe_column_text__ok);
    ATF_ADD_TEST_CASE(tcs, safe_column_text__fail);

    ATF_ADD_TEST_CASE(tcs, safe_column_bytes__ok__blob);
    ATF_ADD_TEST_CASE(tcs, safe_column_bytes__ok__text);
    ATF_ADD_TEST_CASE(tcs, safe_column_bytes__fail);

    ATF_ADD_TEST_CASE(tcs, reset);

    ATF_ADD_TEST_CASE(tcs, bind__blob);
    ATF_ADD_TEST_CASE(tcs, bind__double);
    ATF_ADD_TEST_CASE(tcs, bind__int64);
    ATF_ADD_TEST_CASE(tcs, bind__int);
    ATF_ADD_TEST_CASE(tcs, bind__null);
    ATF_ADD_TEST_CASE(tcs, bind__text);
    ATF_ADD_TEST_CASE(tcs, bind__text__transient);
    ATF_ADD_TEST_CASE(tcs, bind__by_name);

    ATF_ADD_TEST_CASE(tcs, bind_parameter_count);
    ATF_ADD_TEST_CASE(tcs, bind_parameter_index);
    ATF_ADD_TEST_CASE(tcs, bind_parameter_name);

    ATF_ADD_TEST_CASE(tcs, clear_bindings);
}
