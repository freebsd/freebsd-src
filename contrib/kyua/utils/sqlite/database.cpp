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

#include "utils/sqlite/database.hpp"

extern "C" {
#include <sqlite3.h>
}

#include <cstring>
#include <stdexcept>

#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"
#include "utils/sqlite/exceptions.hpp"
#include "utils/sqlite/statement.ipp"
#include "utils/sqlite/transaction.hpp"

namespace fs = utils::fs;
namespace sqlite = utils::sqlite;

using utils::none;
using utils::optional;


/// Internal implementation for sqlite::database.
struct utils::sqlite::database::impl : utils::noncopyable {
    /// Path to the database as seen at construction time.
    optional< fs::path > db_filename;

    /// The SQLite 3 internal database.
    ::sqlite3* db;

    /// Whether we own the database or not (to decide if we close it).
    bool owned;

    /// Constructor.
    ///
    /// \param db_filename_ The path to the database as seen at construction
    ///     time, if any, or none for in-memory databases.  We should use
    ///     sqlite3_db_filename instead, but this function appeared in 3.7.10
    ///     and Ubuntu 12.04 LTS (which we support for Travis CI builds as of
    ///     2015-07-07) ships with 3.7.9.
    /// \param db_ The SQLite internal database.
    /// \param owned_ Whether this object owns the db_ object or not.  If it
    ///     does, the internal db_ will be released during destruction.
    impl(optional< fs::path > db_filename_, ::sqlite3* db_, const bool owned_) :
        db_filename(db_filename_), db(db_), owned(owned_)
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
        if (owned && db != NULL)
            close();
    }

    /// Exception-safe version of sqlite3_open_v2.
    ///
    /// \param file The path to the database file to be opened.
    /// \param flags The flags to be passed to the open routine.
    ///
    /// \return The opened database.
    ///
    /// \throw std::bad_alloc If there is not enough memory to open the
    ///     database.
    /// \throw api_error If there is any problem opening the database.
    static ::sqlite3*
    safe_open(const char* file, const int flags)
    {
        ::sqlite3* db;
        const int error = ::sqlite3_open_v2(file, &db, flags, NULL);
        if (error != SQLITE_OK) {
            if (db == NULL)
                throw std::bad_alloc();
            else {
                sqlite::database error_db(utils::make_optional(fs::path(file)),
                                          db, true);
                throw sqlite::api_error::from_database(error_db,
                                                       "sqlite3_open_v2");
            }
        }
        INV(db != NULL);
        return db;
    }

    /// Shared code for the public close() method.
    void
    close(void)
    {
        PRE(db != NULL);
        int error = ::sqlite3_close(db);
        // For now, let's consider a return of SQLITE_BUSY an error.  We should
        // not be trying to close a busy database in our code.  Maybe revisit
        // this later to raise busy errors as exceptions.
        PRE(error == SQLITE_OK);
        db = NULL;
    }
};


/// Initializes the SQLite database.
///
/// You must share the same database object alongside the lifetime of your
/// SQLite session.  As soon as the object is destroyed, the session is
/// terminated.
///
/// \param db_filename_ The path to the database as seen at construction
///     time, if any, or none for in-memory databases.
/// \param db_ Raw pointer to the C SQLite 3 object.
/// \param owned_ Whether this instance will own the pointer or not.
sqlite::database::database(
    const utils::optional< utils::fs::path >& db_filename_, void* db_,
    const bool owned_) :
    _pimpl(new impl(db_filename_, static_cast< ::sqlite3* >(db_), owned_))
{
}


/// Destructor for the SQLite 3 database.
///
/// Closes the session unless it has already been closed by calling the
/// close() method.  It is recommended to explicitly close the session in the
/// code.
sqlite::database::~database(void)
{
}


/// Opens a memory-based temporary SQLite database.
///
/// \return An in-memory database instance.
///
/// \throw std::bad_alloc If there is not enough memory to open the database.
/// \throw api_error If there is any problem opening the database.
sqlite::database
sqlite::database::in_memory(void)
{
    return database(none, impl::safe_open(":memory:", SQLITE_OPEN_READWRITE),
                    true);
}


/// Opens a named on-disk SQLite database.
///
/// \param file The path to the database file to be opened.  This does not
///     accept the values "" and ":memory:"; use temporary() and in_memory()
///     instead.
/// \param open_flags The flags to be passed to the open routine.
///
/// \return A file-backed database instance.
///
/// \throw std::bad_alloc If there is not enough memory to open the database.
/// \throw api_error If there is any problem opening the database.
sqlite::database
sqlite::database::open(const fs::path& file, int open_flags)
{
    PRE_MSG(!file.str().empty(), "Use database::temporary() instead");
    PRE_MSG(file.str() != ":memory:", "Use database::in_memory() instead");

    int flags = 0;
    if (open_flags & open_readonly) {
        flags |= SQLITE_OPEN_READONLY;
        open_flags &= ~open_readonly;
    }
    if (open_flags & open_readwrite) {
        flags |= SQLITE_OPEN_READWRITE;
        open_flags &= ~open_readwrite;
    }
    if (open_flags & open_create) {
        flags |= SQLITE_OPEN_CREATE;
        open_flags &= ~open_create;
    }
    PRE(open_flags == 0);

    return database(utils::make_optional(file),
                    impl::safe_open(file.c_str(), flags), true);
}


/// Opens an unnamed on-disk SQLite database.
///
/// \return A file-backed database instance.
///
/// \throw std::bad_alloc If there is not enough memory to open the database.
/// \throw api_error If there is any problem opening the database.
sqlite::database
sqlite::database::temporary(void)
{
    return database(none, impl::safe_open("", SQLITE_OPEN_READWRITE), true);
}


/// Gets the internal sqlite3 object.
///
/// \return The raw SQLite 3 database.  This is returned as a void pointer to
/// prevent including the sqlite3.h header file from our public interface.  The
/// only way to call this method is by using the c_gate module, and c_gate takes
/// care of casting this object to the appropriate type.
void*
sqlite::database::raw_database(void)
{
    return _pimpl->db;
}


/// Terminates the connection to the database.
///
/// It is recommended to call this instead of relying on the destructor to do
/// the cleanup, but it is not a requirement to use close().
///
/// \pre close() has not yet been called.
void
sqlite::database::close(void)
{
    _pimpl->close();
}


/// Returns the path to the connected database.
///
/// It is OK to call this function on a live database object, even after close()
/// has been called.  The returned value is consistent at all times.
///
/// \return The path to the file that matches the connected database or none if
/// the connection points to a transient database.
const optional< fs::path >&
sqlite::database::db_filename(void) const
{
    return _pimpl->db_filename;
}


/// Executes an arbitrary SQL string.
///
/// As the documentation explains, this is unsafe.  The code should really be
/// preparing statements and executing them step by step.  However, it is
/// perfectly fine to use this function for, e.g. the initial creation of
/// tables in a database and in tests.
///
/// \param sql The SQL commands to be executed.
///
/// \throw api_error If there is any problem while processing the SQL.
void
sqlite::database::exec(const std::string& sql)
{
    const int error = ::sqlite3_exec(_pimpl->db, sql.c_str(), NULL, NULL, NULL);
    if (error != SQLITE_OK)
        throw api_error::from_database(*this, "sqlite3_exec");
}


/// Opens a new transaction.
///
/// \return An object representing the state of the transaction.
///
/// \throw api_error If there is any problem while opening the transaction.
sqlite::transaction
sqlite::database::begin_transaction(void)
{
    exec("BEGIN TRANSACTION");
    return transaction(*this);
}


/// Prepares a new statement.
///
/// \param sql The SQL statement to prepare.
///
/// \return The prepared statement.
sqlite::statement
sqlite::database::create_statement(const std::string& sql)
{
    LD(F("Creating statement: %s") % sql);
    sqlite3_stmt* stmt;
    const int error = ::sqlite3_prepare_v2(_pimpl->db, sql.c_str(),
                                           sql.length() + 1, &stmt, NULL);
    if (error != SQLITE_OK)
        throw api_error::from_database(*this, "sqlite3_prepare_v2");
    return statement(*this, static_cast< void* >(stmt));
}


/// Returns the row identifier of the last insert.
///
/// \return A row identifier.
int64_t
sqlite::database::last_insert_rowid(void)
{
    return ::sqlite3_last_insert_rowid(_pimpl->db);
}
