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

#include "store/metadata.hpp"

#include "store/exceptions.hpp"
#include "utils/format/macros.hpp"
#include "utils/sanity.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.ipp"

namespace sqlite = utils::sqlite;


namespace {


/// Fetches an integer column from a statement of the 'metadata' table.
///
/// \param stmt The statement from which to get the column value.
/// \param column The name of the column to retrieve.
///
/// \return The value of the column.
///
/// \throw store::integrity_error If there is a problem fetching the value
///     caused by an invalid schema or data.
static int64_t
int64_column(sqlite::statement& stmt, const char* column)
{
    int index;
    try {
        index = stmt.column_id(column);
    } catch (const sqlite::invalid_column_error& e) {
        UNREACHABLE_MSG("Invalid column specification; the SELECT statement "
                        "should have caught this");
    }
    if (stmt.column_type(index) != sqlite::type_integer)
        throw store::integrity_error(F("The '%s' column in 'metadata' table "
                                       "has an invalid type") % column);
    return stmt.column_int64(index);
}


}  // anonymous namespace


/// Constructs a new metadata object.
///
/// \param schema_version_ The schema version.
/// \param timestamp_ The time at which this version was created.
store::metadata::metadata(const int schema_version_, const int64_t timestamp_) :
    _schema_version(schema_version_),
    _timestamp(timestamp_)
{
}


/// Returns the timestamp of this entry.
///
/// \return The timestamp in this metadata entry.
int64_t
store::metadata::timestamp(void) const
{
    return _timestamp;
}


/// Returns the schema version.
///
/// \return The schema version in this metadata entry.
int
store::metadata::schema_version(void) const
{
    return _schema_version;
}


/// Reads the latest metadata entry from the database.
///
/// \param db The database from which to read the metadata from.
///
/// \return The current metadata of the database.  It is not OK for the metadata
/// table to be empty, so this is guaranteed to return a value unless there is
/// an error.
///
/// \throw store::integrity_error If the metadata in the database is empty,
///     has an invalid schema or its contents are bogus.
store::metadata
store::metadata::fetch_latest(sqlite::database& db)
{
    try {
        sqlite::statement stmt = db.create_statement(
            "SELECT schema_version, timestamp FROM metadata "
            "ORDER BY schema_version DESC LIMIT 1");
        if (!stmt.step())
            throw store::integrity_error("The 'metadata' table is empty");

        const int schema_version_ =
            static_cast< int >(int64_column(stmt, "schema_version"));
        const int64_t timestamp_ = int64_column(stmt, "timestamp");

        if (stmt.step())
            UNREACHABLE_MSG("Got more than one result from a query that "
                            "does not permit this; any pragmas defined?");

        return metadata(schema_version_, timestamp_);
    } catch (const sqlite::error& e) {
        throw store::integrity_error(F("Invalid metadata schema: %s") %
                                     e.what());
    }
}
