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

/// \file utils/sqlite/database.hpp
/// Wrapper classes and utilities for the SQLite database state.
///
/// This module contains thin RAII wrappers around the SQLite 3 structures
/// representing the database, and lightweight.

#if !defined(UTILS_SQLITE_DATABASE_HPP)
#define UTILS_SQLITE_DATABASE_HPP

#include "utils/sqlite/database_fwd.hpp"

extern "C" {
#include <stdint.h>
}

#include <cstddef>
#include <memory>

#include "utils/fs/path_fwd.hpp"
#include "utils/optional_fwd.hpp"
#include "utils/sqlite/c_gate_fwd.hpp"
#include "utils/sqlite/statement_fwd.hpp"
#include "utils/sqlite/transaction_fwd.hpp"

namespace utils {
namespace sqlite {


/// Constant for the database::open flags: open in read-only mode.
static const int open_readonly = 1 << 0;
/// Constant for the database::open flags: open in read-write mode.
static const int open_readwrite = 1 << 1;
/// Constant for the database::open flags: create on open.
static const int open_create = 1 << 2;


/// A RAII model for the SQLite 3 database.
///
/// This class holds the database of the SQLite 3 interface during its existence
/// and provides wrappers around several SQLite 3 library functions that operate
/// on such database.
///
/// These wrapper functions differ from the C versions in that they use the
/// implicit database hold by the class, they use C++ types where appropriate
/// and they use exceptions to report errors.
///
/// The wrappers intend to be as lightweight as possible but, in some
/// situations, they are pretty complex because of the workarounds needed to
/// make the SQLite 3 more C++ friendly.  We prefer a clean C++ interface over
/// optimal efficiency, so this is OK.
class database {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::shared_ptr< impl > _pimpl;

    friend class database_c_gate;
    database(const utils::optional< utils::fs::path >&, void*, const bool);
    void* raw_database(void);

public:
    ~database(void);

    static database in_memory(void);
    static database open(const fs::path&, int);
    static database temporary(void);
    void close(void);

    const utils::optional< utils::fs::path >& db_filename(void) const;

    void exec(const std::string&);

    transaction begin_transaction(void);
    statement create_statement(const std::string&);

    int64_t last_insert_rowid(void);
};


}  // namespace sqlite
}  // namespace utils

#endif  // !defined(UTILS_SQLITE_DATABASE_HPP)
