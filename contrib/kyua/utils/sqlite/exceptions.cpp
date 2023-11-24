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

#include <string>

#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/sqlite/c_gate.hpp"
#include "utils/sqlite/database.hpp"

namespace fs = utils::fs;
namespace sqlite = utils::sqlite;

using utils::optional;


namespace {


/// Formats the database filename returned by sqlite for user consumption.
///
/// \param db_filename An optional database filename.
///
/// \return A string describing the filename.
static std::string
format_db_filename(const optional< fs::path >& db_filename)
{
    if (db_filename)
        return db_filename.get().str();
    else
        return "in-memory or temporary";
}


}  // anonymous namespace


/// Constructs a new error with a plain-text message.
///
/// \param db_filename_ Database filename as returned by database::db_filename()
///     for error reporting purposes.
/// \param message The plain-text error message.
sqlite::error::error(const optional< fs::path >& db_filename_,
                     const std::string& message) :
    std::runtime_error(F("%s (sqlite db: %s)") % message %
                       format_db_filename(db_filename_)),
    _db_filename(db_filename_)
{
}


/// Destructor for the error.
sqlite::error::~error(void) throw()
{
}


/// Returns the path to the database that raised this error.
///
/// \return A database filename as returned by database::db_filename().
const optional< fs::path >&
sqlite::error::db_filename(void) const
{
    return _db_filename;
}


/// Constructs a new error.
///
/// \param db_filename_ Database filename as returned by database::db_filename()
///     for error reporting purposes.
/// \param api_function_ The name of the API function that caused the error.
/// \param message_ The plain-text error message provided by SQLite.
sqlite::api_error::api_error(const optional< fs::path >& db_filename_,
                             const std::string& api_function_,
                             const std::string& message_) :
    error(db_filename_, F("%s (sqlite op: %s)") % message_ % api_function_),
    _api_function(api_function_)
{
}


/// Destructor for the error.
sqlite::api_error::~api_error(void) throw()
{
}


/// Constructs a new api_error with the message in the SQLite database.
///
/// \param database_ The SQLite database.
/// \param api_function_ The name of the SQLite C API function that caused the
///     error.
///
/// \return A new api_error with the retrieved message.
sqlite::api_error
sqlite::api_error::from_database(database& database_,
                                 const std::string& api_function_)
{
    ::sqlite3* c_db = database_c_gate(database_).c_database();
    return api_error(database_.db_filename(), api_function_,
                     ::sqlite3_errmsg(c_db));
}


/// Gets the name of the SQlite C API function that caused this error.
///
/// \return The name of the function.
const std::string&
sqlite::api_error::api_function(void) const
{
    return _api_function;
}


/// Constructs a new error.
///
/// \param db_filename_ Database filename as returned by database::db_filename()
///     for error reporting purposes.
/// \param name_ The name of the unknown column.
sqlite::invalid_column_error::invalid_column_error(
    const optional< fs::path >& db_filename_,
    const std::string& name_) :
    error(db_filename_, F("Unknown column '%s'") % name_),
    _column_name(name_)
{
}


/// Destructor for the error.
sqlite::invalid_column_error::~invalid_column_error(void) throw()
{
}


/// Gets the name of the column that could not be found.
///
/// \return The name of the column requested by the user.
const std::string&
sqlite::invalid_column_error::column_name(void) const
{
    return _column_name;
}
