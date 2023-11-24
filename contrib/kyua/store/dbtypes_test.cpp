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

#include "store/dbtypes.hpp"

#include <atf-c++.hpp>

#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "store/exceptions.hpp"
#include "utils/datetime.hpp"
#include "utils/optional.ipp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/statement.ipp"

namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace sqlite = utils::sqlite;

using utils::none;


namespace {


/// Validates that a particular bind_x/column_x sequence works.
///
/// \param bind The store::bind_* function to put the value.
/// \param value The value to store and validate.
/// \param column The store::column_* function to get the value.
template< typename Type1, typename Type2, typename Type3 >
static void
do_ok_test(void (*bind)(sqlite::statement&, const char*, Type1),
           Type2 value,
           Type3 (*column)(sqlite::statement&, const char*))
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE test (column DONTCARE)");

    sqlite::statement insert = db.create_statement("INSERT INTO test "
                                                   "VALUES (:v)");
    bind(insert, ":v", value);
    insert.step_without_results();

    sqlite::statement query = db.create_statement("SELECT * FROM test");
    ATF_REQUIRE(query.step());
    ATF_REQUIRE(column(query, "column") == value);
    ATF_REQUIRE(!query.step());
}


/// Validates an error condition of column_*.
///
/// \param value The invalid value to insert into the database.
/// \param column The store::column_* function to get the value.
/// \param error_regexp The expected message in the raised integrity_error.
template< typename Type1, typename Type2 >
static void
do_invalid_test(Type1 value,
                Type2 (*column)(sqlite::statement&, const char*),
                const std::string& error_regexp)
{
    sqlite::database db = sqlite::database::in_memory();
    db.exec("CREATE TABLE test (column DONTCARE)");

    sqlite::statement insert = db.create_statement("INSERT INTO test "
                                                   "VALUES (:v)");
    insert.bind(":v", value);
    insert.step_without_results();

    sqlite::statement query = db.create_statement("SELECT * FROM test");
    ATF_REQUIRE(query.step());
    ATF_REQUIRE_THROW_RE(store::integrity_error, error_regexp,
                         column(query, "column"));
    ATF_REQUIRE(!query.step());
}


}  // anonymous namespace


ATF_TEST_CASE_WITHOUT_HEAD(bool__ok);
ATF_TEST_CASE_BODY(bool__ok)
{
    do_ok_test(store::bind_bool, true, store::column_bool);
    do_ok_test(store::bind_bool, false, store::column_bool);
}


ATF_TEST_CASE_WITHOUT_HEAD(bool__get_invalid_type);
ATF_TEST_CASE_BODY(bool__get_invalid_type)
{
    do_invalid_test(123, store::column_bool, "not a string");
}


ATF_TEST_CASE_WITHOUT_HEAD(bool__get_invalid_value);
ATF_TEST_CASE_BODY(bool__get_invalid_value)
{
    do_invalid_test("foo", store::column_bool, "Unknown boolean.*foo");
}


ATF_TEST_CASE_WITHOUT_HEAD(delta__ok);
ATF_TEST_CASE_BODY(delta__ok)
{
    do_ok_test(store::bind_delta, datetime::delta(15, 34), store::column_delta);
}


ATF_TEST_CASE_WITHOUT_HEAD(delta__get_invalid_type);
ATF_TEST_CASE_BODY(delta__get_invalid_type)
{
    do_invalid_test(15.6, store::column_delta, "not an integer");
}


ATF_TEST_CASE_WITHOUT_HEAD(optional_string__ok);
ATF_TEST_CASE_BODY(optional_string__ok)
{
    do_ok_test(store::bind_optional_string, "", store::column_optional_string);
    do_ok_test(store::bind_optional_string, "a", store::column_optional_string);
}


ATF_TEST_CASE_WITHOUT_HEAD(optional_string__get_invalid_type);
ATF_TEST_CASE_BODY(optional_string__get_invalid_type)
{
    do_invalid_test(35, store::column_optional_string, "Invalid string");
}


ATF_TEST_CASE_WITHOUT_HEAD(test_result_type__ok);
ATF_TEST_CASE_BODY(test_result_type__ok)
{
    do_ok_test(store::bind_test_result_type,
               model::test_result_passed,
               store::column_test_result_type);
}


ATF_TEST_CASE_WITHOUT_HEAD(test_result_type__get_invalid_type);
ATF_TEST_CASE_BODY(test_result_type__get_invalid_type)
{
    do_invalid_test(12, store::column_test_result_type, "not a string");
}


ATF_TEST_CASE_WITHOUT_HEAD(test_result_type__get_invalid_value);
ATF_TEST_CASE_BODY(test_result_type__get_invalid_value)
{
    do_invalid_test("foo", store::column_test_result_type,
                    "Unknown test result type foo");
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__ok);
ATF_TEST_CASE_BODY(timestamp__ok)
{
    do_ok_test(store::bind_timestamp,
               datetime::timestamp::from_microseconds(0),
               store::column_timestamp);
    do_ok_test(store::bind_timestamp,
               datetime::timestamp::from_microseconds(123),
               store::column_timestamp);

    do_ok_test(store::bind_timestamp,
               datetime::timestamp::from_values(2012, 2, 9, 23, 15, 51, 987654),
               store::column_timestamp);
    do_ok_test(store::bind_timestamp,
               datetime::timestamp::from_values(1980, 1, 2, 3, 4, 5, 0),
               store::column_timestamp);
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__get_invalid_type);
ATF_TEST_CASE_BODY(timestamp__get_invalid_type)
{
    do_invalid_test(35.6, store::column_timestamp, "not an integer");
}


ATF_TEST_CASE_WITHOUT_HEAD(timestamp__get_invalid_value);
ATF_TEST_CASE_BODY(timestamp__get_invalid_value)
{
    do_invalid_test(-1234, store::column_timestamp, "must be positive");
}


ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, bool__ok);
    ATF_ADD_TEST_CASE(tcs, bool__get_invalid_type);
    ATF_ADD_TEST_CASE(tcs, bool__get_invalid_value);

    ATF_ADD_TEST_CASE(tcs, delta__ok);
    ATF_ADD_TEST_CASE(tcs, delta__get_invalid_type);

    ATF_ADD_TEST_CASE(tcs, optional_string__ok);
    ATF_ADD_TEST_CASE(tcs, optional_string__get_invalid_type);

    ATF_ADD_TEST_CASE(tcs, test_result_type__ok);
    ATF_ADD_TEST_CASE(tcs, test_result_type__get_invalid_type);
    ATF_ADD_TEST_CASE(tcs, test_result_type__get_invalid_value);

    ATF_ADD_TEST_CASE(tcs, timestamp__ok);
    ATF_ADD_TEST_CASE(tcs, timestamp__get_invalid_type);
    ATF_ADD_TEST_CASE(tcs, timestamp__get_invalid_value);
}
