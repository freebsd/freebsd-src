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

#include "utils/sqlite/statement.hpp"

extern "C" {
#include <sqlite3.h>
}

#include <map>

#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/sanity.hpp"
#include "utils/sqlite/c_gate.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"

namespace sqlite = utils::sqlite;


namespace {


static sqlite::type c_type_to_cxx(const int) UTILS_PURE;


/// Maps a SQLite 3 data type to our own representation.
///
/// \param original The native SQLite 3 data type.
///
/// \return Our internal representation for the native data type.
static sqlite::type
c_type_to_cxx(const int original)
{
    switch (original) {
    case SQLITE_BLOB: return sqlite::type_blob;
    case SQLITE_FLOAT: return sqlite::type_float;
    case SQLITE_INTEGER: return sqlite::type_integer;
    case SQLITE_NULL: return sqlite::type_null;
    case SQLITE_TEXT: return sqlite::type_text;
    default: UNREACHABLE_MSG("Unknown data type returned by SQLite 3");
    }
    UNREACHABLE;
}


/// Handles the return value of a sqlite3_bind_* call.
///
/// \param db The database the call was made on.
/// \param api_function The name of the sqlite3_bind_* function called.
/// \param error The error code returned by the function; can be SQLITE_OK.
///
/// \throw std::bad_alloc If there was no memory for the binding.
/// \throw api_error If the binding fails for any other reason.
static void
handle_bind_error(sqlite::database& db, const char* api_function,
                  const int error)
{
    switch (error) {
    case SQLITE_OK:
        return;
    case SQLITE_RANGE:
        UNREACHABLE_MSG("Invalid index for bind argument");
    case SQLITE_NOMEM:
        throw std::bad_alloc();
    default:
        throw sqlite::api_error::from_database(db, api_function);
    }
}


}  // anonymous namespace


/// Internal implementation for sqlite::statement.
struct utils::sqlite::statement::impl : utils::noncopyable {
    /// The database this statement belongs to.
    sqlite::database& db;

    /// The SQLite 3 internal statement.
    ::sqlite3_stmt* stmt;

    /// Cache for the column names in a statement; lazily initialized.
    std::map< std::string, int > column_cache;

    /// Constructor.
    ///
    /// \param db_ The database this statement belongs to.  Be aware that we
    ///     keep a *reference* to the database; in other words, if the database
    ///     vanishes, this object will become invalid.  (It'd be trivial to keep
    ///     a shallow copy here instead, but I feel that statements that outlive
    ///     their database represents sloppy programming.)
    /// \param stmt_ The SQLite internal statement.
    impl(database& db_, ::sqlite3_stmt* stmt_) :
        db(db_),
        stmt(stmt_)
    {
    }

    /// Destructor.
    ///
    /// It is important to keep this as part of the 'impl' class instead of the
    /// container class.  The 'impl' class is destroyed exactly once (because it
    /// is managed by a shared_ptr) and thus releasing the resources here is
    /// OK.  However, the container class is potentially released many times,
    /// which means that we would be double-freeing the internal object and
    /// reusing invalid data.
    ~impl(void)
    {
        (void)::sqlite3_finalize(stmt);
    }
};


/// Initializes a statement object.
///
/// This is an internal function.  Use database::create_statement() to
/// instantiate one of these objects.
///
/// \param db The database this statement belongs to.
/// \param raw_stmt A void pointer representing a SQLite native statement of
///     type sqlite3_stmt.
sqlite::statement::statement(database& db, void* raw_stmt) :
    _pimpl(new impl(db, static_cast< ::sqlite3_stmt* >(raw_stmt)))
{
}


/// Destructor for the statement.
///
/// Remember that statements are reference-counted, so the statement will only
/// cease to be valid once its last copy is destroyed.
sqlite::statement::~statement(void)
{
}


/// Executes a statement that is not supposed to return any data.
///
/// Use this function to execute DDL and INSERT statements; i.e. statements that
/// only have one processing step and deliver no rows.  This frees the caller
/// from having to deal with the return value of the step() function.
///
/// \pre The statement to execute will not produce any rows.
void
sqlite::statement::step_without_results(void)
{
    const bool data = step();
    INV_MSG(!data, "The statement should not have produced any rows, but it "
            "did");
}


/// Performs a processing step on the statement.
///
/// \return True if the statement returned a row; false if the processing has
/// finished.
///
/// \throw api_error If the processing of the step raises an error.
bool
sqlite::statement::step(void)
{
    const int error = ::sqlite3_step(_pimpl->stmt);
    switch (error) {
    case SQLITE_DONE:
        LD("Step statement; no more rows");
        return false;
    case SQLITE_ROW:
        LD("Step statement; row available for processing");
        return true;
    default:
        throw api_error::from_database(_pimpl->db, "sqlite3_step");
    }
    UNREACHABLE;
}


/// Returns the number of columns in the step result.
///
/// \return The number of columns available for data retrieval.
int
sqlite::statement::column_count(void)
{
    return ::sqlite3_column_count(_pimpl->stmt);
}


/// Returns the name of a particular column in the result.
///
/// \param index The column to request the name of.
///
/// \return The name of the requested column.
std::string
sqlite::statement::column_name(const int index)
{
    const char* name = ::sqlite3_column_name(_pimpl->stmt, index);
    if (name == NULL)
        throw api_error::from_database(_pimpl->db, "sqlite3_column_name");
    return name;
}


/// Returns the type of a particular column in the result.
///
/// \param index The column to request the type of.
///
/// \return The type of the requested column.
sqlite::type
sqlite::statement::column_type(const int index)
{
    return c_type_to_cxx(::sqlite3_column_type(_pimpl->stmt, index));
}


/// Finds a column by name.
///
/// \param name The name of the column to search for.
///
/// \return The column identifier.
///
/// \throw value_error If the name cannot be found.
int
sqlite::statement::column_id(const char* name)
{
    std::map< std::string, int >& cache = _pimpl->column_cache;

    if (cache.empty()) {
        for (int i = 0; i < column_count(); i++) {
            const std::string aux_name = column_name(i);
            INV(cache.find(aux_name) == cache.end());
            cache[aux_name] = i;
        }
    }

    const std::map< std::string, int >::const_iterator iter = cache.find(name);
    if (iter == cache.end())
        throw invalid_column_error(_pimpl->db.db_filename(), name);
    else
        return (*iter).second;
}


/// Returns a particular column in the result as a blob.
///
/// \param index The column to retrieve.
///
/// \return A block of memory with the blob contents.  Note that the pointer
/// returned by this call will be invalidated on the next call to any SQLite API
/// function.
sqlite::blob
sqlite::statement::column_blob(const int index)
{
    PRE(column_type(index) == type_blob);
    return blob(::sqlite3_column_blob(_pimpl->stmt, index),
                ::sqlite3_column_bytes(_pimpl->stmt, index));
}


/// Returns a particular column in the result as a double.
///
/// \param index The column to retrieve.
///
/// \return The double value.
double
sqlite::statement::column_double(const int index)
{
    PRE(column_type(index) == type_float);
    return ::sqlite3_column_double(_pimpl->stmt, index);
}


/// Returns a particular column in the result as an integer.
///
/// \param index The column to retrieve.
///
/// \return The integer value.  Note that the value may not fit in an integer
/// depending on the platform.  Use column_int64 to retrieve the integer without
/// truncation.
int
sqlite::statement::column_int(const int index)
{
    PRE(column_type(index) == type_integer);
    return ::sqlite3_column_int(_pimpl->stmt, index);
}


/// Returns a particular column in the result as a 64-bit integer.
///
/// \param index The column to retrieve.
///
/// \return The integer value.
int64_t
sqlite::statement::column_int64(const int index)
{
    PRE(column_type(index) == type_integer);
    return ::sqlite3_column_int64(_pimpl->stmt, index);
}


/// Returns a particular column in the result as a double.
///
/// \param index The column to retrieve.
///
/// \return A C string with the contents.  Note that the pointer returned by
/// this call will be invalidated on the next call to any SQLite API function.
/// If you want to be extra safe, store the result in a std::string to not worry
/// about this.
std::string
sqlite::statement::column_text(const int index)
{
    PRE(column_type(index) == type_text);
    return reinterpret_cast< const char* >(::sqlite3_column_text(
        _pimpl->stmt, index));
}


/// Returns the number of bytes stored in the column.
///
/// \pre This is only valid for columns of type blob and text.
///
/// \param index The column to retrieve the size of.
///
/// \return The number of bytes in the column.  Remember that strings are stored
/// in their UTF-8 representation; this call returns the number of *bytes*, not
/// characters.
int
sqlite::statement::column_bytes(const int index)
{
    PRE(column_type(index) == type_blob || column_type(index) == type_text);
    return ::sqlite3_column_bytes(_pimpl->stmt, index);
}


/// Type-checked version of column_blob.
///
/// \param name The name of the column to retrieve.
///
/// \return The same as column_blob if the value can be retrieved.
///
/// \throw error If the type of the cell to retrieve is invalid.
/// \throw invalid_column_error If name is invalid.
sqlite::blob
sqlite::statement::safe_column_blob(const char* name)
{
    const int column = column_id(name);
    if (column_type(column) != sqlite::type_blob)
        throw sqlite::error(_pimpl->db.db_filename(),
                            F("Column '%s' is not a blob") % name);
    return column_blob(column);
}


/// Type-checked version of column_double.
///
/// \param name The name of the column to retrieve.
///
/// \return The same as column_double if the value can be retrieved.
///
/// \throw error If the type of the cell to retrieve is invalid.
/// \throw invalid_column_error If name is invalid.
double
sqlite::statement::safe_column_double(const char* name)
{
    const int column = column_id(name);
    if (column_type(column) != sqlite::type_float)
        throw sqlite::error(_pimpl->db.db_filename(),
                            F("Column '%s' is not a float") % name);
    return column_double(column);
}


/// Type-checked version of column_int.
///
/// \param name The name of the column to retrieve.
///
/// \return The same as column_int if the value can be retrieved.
///
/// \throw error If the type of the cell to retrieve is invalid.
/// \throw invalid_column_error If name is invalid.
int
sqlite::statement::safe_column_int(const char* name)
{
    const int column = column_id(name);
    if (column_type(column) != sqlite::type_integer)
        throw sqlite::error(_pimpl->db.db_filename(),
                            F("Column '%s' is not an integer") % name);
    return column_int(column);
}


/// Type-checked version of column_int64.
///
/// \param name The name of the column to retrieve.
///
/// \return The same as column_int64 if the value can be retrieved.
///
/// \throw error If the type of the cell to retrieve is invalid.
/// \throw invalid_column_error If name is invalid.
int64_t
sqlite::statement::safe_column_int64(const char* name)
{
    const int column = column_id(name);
    if (column_type(column) != sqlite::type_integer)
        throw sqlite::error(_pimpl->db.db_filename(),
                            F("Column '%s' is not an integer") % name);
    return column_int64(column);
}


/// Type-checked version of column_text.
///
/// \param name The name of the column to retrieve.
///
/// \return The same as column_text if the value can be retrieved.
///
/// \throw error If the type of the cell to retrieve is invalid.
/// \throw invalid_column_error If name is invalid.
std::string
sqlite::statement::safe_column_text(const char* name)
{
    const int column = column_id(name);
    if (column_type(column) != sqlite::type_text)
        throw sqlite::error(_pimpl->db.db_filename(),
                            F("Column '%s' is not a string") % name);
    return column_text(column);
}


/// Type-checked version of column_bytes.
///
/// \param name The name of the column to retrieve the size of.
///
/// \return The same as column_bytes if the value can be retrieved.
///
/// \throw error If the type of the cell to retrieve the size of is invalid.
/// \throw invalid_column_error If name is invalid.
int
sqlite::statement::safe_column_bytes(const char* name)
{
    const int column = column_id(name);
    if (column_type(column) != sqlite::type_blob &&
        column_type(column) != sqlite::type_text)
        throw sqlite::error(_pimpl->db.db_filename(),
                            F("Column '%s' is not a blob or a string") % name);
    return column_bytes(column);
}


/// Resets a statement to allow further processing.
void
sqlite::statement::reset(void)
{
    (void)::sqlite3_reset(_pimpl->stmt);
}


/// Binds a blob to a prepared statement.
///
/// \param index The index of the binding.
/// \param b Description of the blob, which must remain valid during the
///     execution of the statement.
///
/// \throw api_error If the binding fails.
void
sqlite::statement::bind(const int index, const blob& b)
{
    const int error = ::sqlite3_bind_blob(_pimpl->stmt, index, b.memory, b.size,
                                          SQLITE_STATIC);
    handle_bind_error(_pimpl->db, "sqlite3_bind_blob", error);
}


/// Binds a double value to a prepared statement.
///
/// \param index The index of the binding.
/// \param value The double value to bind.
///
/// \throw api_error If the binding fails.
void
sqlite::statement::bind(const int index, const double value)
{
    const int error = ::sqlite3_bind_double(_pimpl->stmt, index, value);
    handle_bind_error(_pimpl->db, "sqlite3_bind_double", error);
}


/// Binds an integer value to a prepared statement.
///
/// \param index The index of the binding.
/// \param value The integer value to bind.
///
/// \throw api_error If the binding fails.
void
sqlite::statement::bind(const int index, const int value)
{
    const int error = ::sqlite3_bind_int(_pimpl->stmt, index, value);
    handle_bind_error(_pimpl->db, "sqlite3_bind_int", error);
}


/// Binds a 64-bit integer value to a prepared statement.
///
/// \param index The index of the binding.
/// \param value The 64-bin integer value to bind.
///
/// \throw api_error If the binding fails.
void
sqlite::statement::bind(const int index, const int64_t value)
{
    const int error = ::sqlite3_bind_int64(_pimpl->stmt, index, value);
    handle_bind_error(_pimpl->db, "sqlite3_bind_int64", error);
}


/// Binds a NULL value to a prepared statement.
///
/// \param index The index of the binding.
///
/// \throw api_error If the binding fails.
void
sqlite::statement::bind(const int index, const null& /* null */)
{
    const int error = ::sqlite3_bind_null(_pimpl->stmt, index);
    handle_bind_error(_pimpl->db, "sqlite3_bind_null", error);
}


/// Binds a text string to a prepared statement.
///
/// \param index The index of the binding.
/// \param text The string to bind.  SQLite generates an internal copy of this
///     string, so the original string object does not have to remain live.  We
///     do this because handling the lifetime of std::string objects is very
///     hard (think about implicit conversions), so it is very easy to shoot
///     ourselves in the foot if we don't do this.
///
/// \throw api_error If the binding fails.
void
sqlite::statement::bind(const int index, const std::string& text)
{
    const int error = ::sqlite3_bind_text(_pimpl->stmt, index, text.c_str(),
                                          text.length(), SQLITE_TRANSIENT);
    handle_bind_error(_pimpl->db, "sqlite3_bind_text", error);
}


/// Returns the index of the highest parameter.
///
/// \return A parameter index.
int
sqlite::statement::bind_parameter_count(void)
{
    return ::sqlite3_bind_parameter_count(_pimpl->stmt);
}


/// Returns the index of a named parameter.
///
/// \param name The name of the parameter to be queried; must exist.
///
/// \return A parameter index.
int
sqlite::statement::bind_parameter_index(const std::string& name)
{
    const int index = ::sqlite3_bind_parameter_index(_pimpl->stmt,
                                                     name.c_str());
    PRE_MSG(index > 0, "Parameter name not in statement");
    return index;
}


/// Returns the name of a parameter by index.
///
/// \param index The index to query; must be valid.
///
/// \return The name of the parameter.
std::string
sqlite::statement::bind_parameter_name(const int index)
{
    const char* name = ::sqlite3_bind_parameter_name(_pimpl->stmt, index);
    PRE_MSG(name != NULL, "Index value out of range or nameless parameter");
    return std::string(name);
}


/// Clears any bindings and releases their memory.
void
sqlite::statement::clear_bindings(void)
{
    const int error = ::sqlite3_clear_bindings(_pimpl->stmt);
    PRE_MSG(error == SQLITE_OK, "SQLite3 contract has changed; it should "
            "only return SQLITE_OK");
}
