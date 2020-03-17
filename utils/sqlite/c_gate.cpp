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

#include "utils/sqlite/c_gate.hpp"

#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/sqlite/database.hpp"

namespace sqlite = utils::sqlite;

using utils::none;


/// Creates a new gateway to an existing C++ SQLite database.
///
/// \param database_ The database to connect to.  This object must remain alive
///     while the newly-constructed database_c_gate is alive.
sqlite::database_c_gate::database_c_gate(database& database_) :
    _database(database_)
{
}


/// Destructor.
///
/// Destroying this object has no implications on the life cycle of the SQLite
/// database.  Only the corresponding database object controls when the SQLite 3
/// database is closed.
sqlite::database_c_gate::~database_c_gate(void)
{
}


/// Creates a C++ database for a C SQLite 3 database.
///
/// \warning The created database object does NOT own the C database.  You must
/// take care to properly destroy the input sqlite3 when you are done with it to
/// not leak resources.
///
/// \param raw_database The raw database to wrap temporarily.
///
/// \return The wrapped database without strong ownership on the input database.
sqlite::database
sqlite::database_c_gate::connect(::sqlite3* raw_database)
{
    return database(none, static_cast< void* >(raw_database), false);
}


/// Returns the C native SQLite 3 database.
///
/// \return A native sqlite3 object holding the SQLite 3 C API database.
::sqlite3*
sqlite::database_c_gate::c_database(void)
{
    return static_cast< ::sqlite3* >(_database.raw_database());
}
