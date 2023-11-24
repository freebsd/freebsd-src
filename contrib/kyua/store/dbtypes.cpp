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

#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "store/exceptions.hpp"
#include "utils/datetime.hpp"
#include "utils/format/macros.hpp"
#include "utils/sanity.hpp"
#include "utils/sqlite/statement.ipp"

namespace datetime = utils::datetime;
namespace sqlite = utils::sqlite;


/// Binds a boolean value to a statement parameter.
///
/// \param stmt The statement to which to bind the parameter.
/// \param field The name of the parameter; must exist.
/// \param value The value to bind.
void
store::bind_bool(sqlite::statement& stmt, const char* field, const bool value)
{
    stmt.bind(field, value ? "true" : "false");
}


/// Binds a time delta to a statement parameter.
///
/// \param stmt The statement to which to bind the parameter.
/// \param field The name of the parameter; must exist.
/// \param delta The value to bind.
void
store::bind_delta(sqlite::statement& stmt, const char* field,
                  const datetime::delta& delta)
{
    stmt.bind(field, static_cast< int64_t >(delta.to_microseconds()));
}


/// Binds a string to a statement parameter.
///
/// If the string is not empty, this binds the string itself.  Otherwise, it
/// binds a NULL value.
///
/// \param stmt The statement to which to bind the parameter.
/// \param field The name of the parameter; must exist.
/// \param str The string to bind.
void
store::bind_optional_string(sqlite::statement& stmt, const char* field,
                            const std::string& str)
{
    if (str.empty())
        stmt.bind(field, sqlite::null());
    else
        stmt.bind(field, str);
}


/// Binds a test result type to a statement parameter.
///
/// \param stmt The statement to which to bind the parameter.
/// \param field The name of the parameter; must exist.
/// \param type The result type to bind.
void
store::bind_test_result_type(sqlite::statement& stmt, const char* field,
                             const model::test_result_type& type)
{
    switch (type) {
    case model::test_result_broken:
        stmt.bind(field, "broken");
        break;

    case model::test_result_expected_failure:
        stmt.bind(field, "expected_failure");
        break;

    case model::test_result_failed:
        stmt.bind(field, "failed");
        break;

    case model::test_result_passed:
        stmt.bind(field, "passed");
        break;

    case model::test_result_skipped:
        stmt.bind(field, "skipped");
        break;

    default:
        UNREACHABLE;
    }
}


/// Binds a timestamp to a statement parameter.
///
/// \param stmt The statement to which to bind the parameter.
/// \param field The name of the parameter; must exist.
/// \param timestamp The value to bind.
void
store::bind_timestamp(sqlite::statement& stmt, const char* field,
                      const datetime::timestamp& timestamp)
{
    stmt.bind(field, timestamp.to_microseconds());
}


/// Queries a boolean value from a statement.
///
/// \param stmt The statement from which to get the column.
/// \param column The name of the column holding the value.
///
/// \return The parsed value if all goes well.
///
/// \throw integrity_error If the value in the specified column is invalid.
bool
store::column_bool(sqlite::statement& stmt, const char* column)
{
    const int id = stmt.column_id(column);
    if (stmt.column_type(id) != sqlite::type_text)
        throw store::integrity_error(F("Boolean value in column %s is not a "
                                       "string") % column);
    const std::string value = stmt.column_text(id);
    if (value == "true")
        return true;
    else if (value == "false")
        return false;
    else
        throw store::integrity_error(F("Unknown boolean value '%s'") % value);
}


/// Queries a time delta from a statement.
///
/// \param stmt The statement from which to get the column.
/// \param column The name of the column holding the value.
///
/// \return The parsed value if all goes well.
///
/// \throw integrity_error If the value in the specified column is invalid.
datetime::delta
store::column_delta(sqlite::statement& stmt, const char* column)
{
    const int id = stmt.column_id(column);
    if (stmt.column_type(id) != sqlite::type_integer)
        throw store::integrity_error(F("Time delta in column %s is not an "
                                       "integer") % column);
    return datetime::delta::from_microseconds(stmt.column_int64(id));
}


/// Queries an optional string from a statement.
///
/// \param stmt The statement from which to get the column.
/// \param column The name of the column holding the value.
///
/// \return The parsed value if all goes well.
///
/// \throw integrity_error If the value in the specified column is invalid.
std::string
store::column_optional_string(sqlite::statement& stmt, const char* column)
{
    const int id = stmt.column_id(column);
    switch (stmt.column_type(id)) {
    case sqlite::type_text:
        return stmt.column_text(id);
    case sqlite::type_null:
        return "";
    default:
        throw integrity_error(F("Invalid string type in column %s") % column);
    }
}


/// Queries a test result type from a statement.
///
/// \param stmt The statement from which to get the column.
/// \param column The name of the column holding the value.
///
/// \return The parsed value if all goes well.
///
/// \throw integrity_error If the value in the specified column is invalid.
model::test_result_type
store::column_test_result_type(sqlite::statement& stmt, const char* column)
{
    const int id = stmt.column_id(column);
    if (stmt.column_type(id) != sqlite::type_text)
        throw store::integrity_error(F("Result type in column %s is not a "
                                       "string") % column);
    const std::string type = stmt.column_text(id);
    if (type == "passed") {
        return model::test_result_passed;
    } else if (type == "broken") {
        return model::test_result_broken;
    } else if (type == "expected_failure") {
        return model::test_result_expected_failure;
    } else if (type == "failed") {
        return model::test_result_failed;
    } else if (type == "skipped") {
        return model::test_result_skipped;
    } else {
        throw store::integrity_error(F("Unknown test result type %s") % type);
    }
}


/// Queries a timestamp from a statement.
///
/// \param stmt The statement from which to get the column.
/// \param column The name of the column holding the value.
///
/// \return The parsed value if all goes well.
///
/// \throw integrity_error If the value in the specified column is invalid.
datetime::timestamp
store::column_timestamp(sqlite::statement& stmt, const char* column)
{
    const int id = stmt.column_id(column);
    if (stmt.column_type(id) != sqlite::type_integer)
        throw store::integrity_error(F("Timestamp in column %s is not an "
                                       "integer") % column);
    const int64_t value = stmt.column_int64(id);
    if (value < 0)
        throw store::integrity_error(F("Timestamp in column %s must be "
                                       "positive") % column);
    return datetime::timestamp::from_microseconds(value);
}
