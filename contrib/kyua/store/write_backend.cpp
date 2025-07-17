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

#include "store/write_backend.hpp"

#include <stdexcept>

#include "store/exceptions.hpp"
#include "store/metadata.hpp"
#include "store/read_backend.hpp"
#include "store/write_transaction.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/sanity.hpp"
#include "utils/stream.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.ipp"

namespace fs = utils::fs;
namespace sqlite = utils::sqlite;


/// The current schema version.
///
/// Any new database gets this schema version.  Existing databases with an older
/// schema version must be first migrated to the current schema with
/// migrate_schema() before they can be used.
///
/// This must be kept in sync with the value in the corresponding schema_vX.sql
/// file, where X matches this version number.
///
/// This variable is not const to allow tests to modify it.  No other code
/// should change its value.
int store::detail::current_schema_version = 3;


namespace {


/// Checks if a database is empty (i.e. if it is new).
///
/// \param db The database to check.
///
/// \return True if the database is empty.
static bool
empty_database(sqlite::database& db)
{
    sqlite::statement stmt = db.create_statement("SELECT * FROM sqlite_master");
    return !stmt.step();
}


}  // anonymous namespace


/// Calculates the path to the schema file for the database.
///
/// \return The path to the installed schema_vX.sql file that matches the
/// current_schema_version.
fs::path
store::detail::schema_file(void)
{
    return fs::path(utils::getenv_with_default("KYUA_STOREDIR", KYUA_STOREDIR))
        / (F("schema_v%s.sql") % current_schema_version);
}


/// Initializes an empty database.
///
/// \param db The database to initialize.
///
/// \return The metadata record written into the new database.
///
/// \throw store::error If there is a problem initializing the database.
store::metadata
store::detail::initialize(sqlite::database& db)
{
    PRE(empty_database(db));

    const fs::path schema = schema_file();

    LI(F("Populating new database with schema from %s") % schema);
    try {
        db.exec(utils::read_file(schema));

        const metadata metadata = metadata::fetch_latest(db);
        LI(F("New metadata entry %s") % metadata.timestamp());
        if (metadata.schema_version() != detail::current_schema_version) {
            UNREACHABLE_MSG(F("current_schema_version is out of sync with "
                              "%s") % schema);
        }
        return metadata;
    } catch (const store::integrity_error& e) {
        // Could be raised by metadata::fetch_latest.
        UNREACHABLE_MSG("Inconsistent code while creating a database");
    } catch (const sqlite::error& e) {
        throw error(F("Failed to initialize database: %s") % e.what());
    } catch (const std::runtime_error& e) {
        throw error(F("Cannot read database schema '%s'") % schema);
    }
}


/// Internal implementation for the backend.
struct store::write_backend::impl : utils::noncopyable {
    /// The SQLite database this backend talks to.
    sqlite::database database;

    /// Constructor.
    ///
    /// \param database_ The SQLite database instance.
    impl(sqlite::database& database_) : database(database_)
    {
    }
};


/// Constructs a new backend.
///
/// \param pimpl_ The internal data.
store::write_backend::write_backend(impl* pimpl_) :
    _pimpl(pimpl_)
{
}


/// Destructor.
store::write_backend::~write_backend(void)
{
}


/// Opens a database in read-write mode and creates it if necessary.
///
/// \param file The database file to be opened.
///
/// \return The backend representation.
///
/// \throw store::error If there is any problem opening or creating
///     the database.
store::write_backend
store::write_backend::open_rw(const fs::path& file)
{
    sqlite::database db = detail::open_and_setup(
        file, sqlite::open_readwrite | sqlite::open_create);
    if (!empty_database(db))
        throw error(F("%s already exists and is not empty; cannot open "
                      "for write") % file);
    detail::initialize(db);
    return write_backend(new impl(db));
}


/// Closes the SQLite database.
void
store::write_backend::close(void)
{
    _pimpl->database.close();
}


/// Gets the connection to the SQLite database.
///
/// \return A database connection.
sqlite::database&
store::write_backend::database(void)
{
    return _pimpl->database;
}


/// Opens a write-only transaction.
///
/// \return A new transaction.
store::write_transaction
store::write_backend::start_write(void)
{
    return write_transaction(*this);
}
