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

#include "store/read_backend.hpp"

#include "store/exceptions.hpp"
#include "store/metadata.hpp"
#include "store/read_transaction.hpp"
#include "store/write_backend.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/noncopyable.hpp"
#include "utils/sqlite/database.hpp"
#include "utils/sqlite/exceptions.hpp"

namespace fs = utils::fs;
namespace sqlite = utils::sqlite;


/// Opens a database and defines session pragmas.
///
/// This auxiliary function ensures that, every time we open a SQLite database,
/// we define the same set of pragmas for it.
///
/// \param file The database file to be opened.
/// \param flags The flags for the open; see sqlite::database::open.
///
/// \return The opened database.
///
/// \throw store::error If there is a problem opening or creating the database.
sqlite::database
store::detail::open_and_setup(const fs::path& file, const int flags)
{
    try {
        sqlite::database database = sqlite::database::open(file, flags);
        database.exec("PRAGMA foreign_keys = ON");
        return database;
    } catch (const sqlite::error& e) {
        throw store::error(F("Cannot open '%s': %s") % file % e.what());
    }
}


/// Internal implementation for the backend.
struct store::read_backend::impl : utils::noncopyable {
    /// The SQLite database this backend talks to.
    sqlite::database database;

    /// Constructor.
    ///
    /// \param database_ The SQLite database instance.
    /// \param metadata_ The metadata for the loaded database.  This must match
    ///     the schema version we implement in this module; otherwise, a
    ///     migration is necessary.
    ///
    /// \throw integrity_error If the schema in the database is too modern,
    ///     which might indicate some form of corruption or an old binary.
    /// \throw old_schema_error If the schema in the database is older than our
    ///     currently-implemented version and needs an upgrade.  The caller can
    ///     use migrate_schema() to fix this problem.
    impl(sqlite::database& database_, const metadata& metadata_) :
        database(database_)
    {
        const int database_version = metadata_.schema_version();

        if (database_version == detail::current_schema_version) {
            // OK.
        } else if (database_version < detail::current_schema_version) {
            throw old_schema_error(database_version);
        } else if (database_version > detail::current_schema_version) {
            throw integrity_error(
                F("Database at schema version %s, which is newer than the "
                  "supported version %s")
                % database_version % detail::current_schema_version);
        }
    }
};


/// Constructs a new backend.
///
/// \param pimpl_ The internal data.
store::read_backend::read_backend(impl* pimpl_) :
    _pimpl(pimpl_)
{
}


/// Destructor.
store::read_backend::~read_backend(void)
{
}


/// Opens a database in read-only mode.
///
/// \param file The database file to be opened.
///
/// \return The backend representation.
///
/// \throw store::error If there is any problem opening the database.
store::read_backend
store::read_backend::open_ro(const fs::path& file)
{
    sqlite::database db = detail::open_and_setup(file, sqlite::open_readonly);
    return read_backend(new impl(db, metadata::fetch_latest(db)));
}


/// Closes the SQLite database.
void
store::read_backend::close(void)
{
    _pimpl->database.close();
}


/// Gets the connection to the SQLite database.
///
/// \return A database connection.
sqlite::database&
store::read_backend::database(void)
{
    return _pimpl->database;
}


/// Opens a read-only transaction.
///
/// \return A new transaction.
store::read_transaction
store::read_backend::start_read(void)
{
    return read_transaction(*this);
}
