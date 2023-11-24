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

/// \file utils/sqlite/test_utils.hpp
/// Utilities for tests of the sqlite modules.
///
/// This file is intended to be included once, and only once, for every test
/// program that needs it.  All the code is herein contained to simplify the
/// dependency chain in the build rules.

#if !defined(UTILS_SQLITE_TEST_UTILS_HPP)
#   define UTILS_SQLITE_TEST_UTILS_HPP
#else
#   error "test_utils.hpp can only be included once"
#endif

#include <iostream>

#include <atf-c++.hpp>

#include "utils/defs.hpp"
#include "utils/sqlite/c_gate.hpp"
#include "utils/sqlite/exceptions.hpp"


namespace {


/// Checks that a given expression raises a particular sqlite::api_error.
///
/// We cannot make any assumptions regarding the error text provided by SQLite,
/// so we resort to checking only which API function raised the error (because
/// our code is the one hardcoding these strings).
///
/// \param exp_api_function The name of the SQLite 3 C API function that causes
///     the error.
/// \param statement The statement to execute.
#define REQUIRE_API_ERROR(exp_api_function, statement) \
    do { \
        try { \
            statement; \
            ATF_FAIL("api_error not raised by " #statement); \
        } catch (const utils::sqlite::api_error& api_error) { \
            ATF_REQUIRE_EQ(exp_api_function, api_error.api_function()); \
        } \
    } while (0)


/// Gets the pointer to the internal sqlite3 of a database object.
///
/// This is pure syntactic sugar to simplify typing in the test cases.
///
/// \param db The SQLite database.
///
/// \return The internal sqlite3 of the input database.
static inline ::sqlite3*
raw(utils::sqlite::database& db)
{
    return utils::sqlite::database_c_gate(db).c_database();
}


/// SQL commands to create a test table.
///
/// See create_test_table() for more details.
static const char* create_test_table_sql =
    "CREATE TABLE test (prime INTEGER PRIMARY KEY);"
    "INSERT INTO test (prime) VALUES (1);\n"
    "INSERT INTO test (prime) VALUES (2);\n"
    "INSERT INTO test (prime) VALUES (7);\n"
    "INSERT INTO test (prime) VALUES (5);\n"
    "INSERT INTO test (prime) VALUES (3);\n";


static void create_test_table(::sqlite3*) UTILS_UNUSED;


/// Creates a 'test' table in a database.
///
/// The created 'test' table is populated with a few rows.  If there are any
/// problems creating the database, this fails the test case.
///
/// Use the verify_test_table() function on the same database to verify that
/// the table is present and contains the expected data.
///
/// \param db The database in which to create the test table.
static void
create_test_table(::sqlite3* db)
{
    std::cout << "Creating 'test' table in the database\n";
    ATF_REQUIRE_EQ(SQLITE_OK, ::sqlite3_exec(db, create_test_table_sql,
                                             NULL, NULL, NULL));
}


static void verify_test_table(::sqlite3*) UTILS_UNUSED;


/// Verifies that the specified database contains the 'test' table.
///
/// This function ensures that the provided database contains the 'test' table
/// created by the create_test_table() function on the same database.  If it
/// doesn't, this fails the caller test case.
///
/// \param db The database to validate.
static void
verify_test_table(::sqlite3* db)
{
    std::cout << "Verifying that the 'test' table is in the database\n";
    char **result;
    int rows, columns;
    ATF_REQUIRE_EQ(SQLITE_OK, ::sqlite3_get_table(db,
        "SELECT * FROM test ORDER BY prime", &result, &rows, &columns, NULL));
    ATF_REQUIRE_EQ(5, rows);
    ATF_REQUIRE_EQ(1, columns);
    ATF_REQUIRE_EQ("prime", std::string(result[0]));
    ATF_REQUIRE_EQ("1", std::string(result[1]));
    ATF_REQUIRE_EQ("2", std::string(result[2]));
    ATF_REQUIRE_EQ("3", std::string(result[3]));
    ATF_REQUIRE_EQ("5", std::string(result[4]));
    ATF_REQUIRE_EQ("7", std::string(result[5]));
    ::sqlite3_free_table(result);
}


}  // anonymous namespace
