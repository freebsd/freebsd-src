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

/// \file utils/sqlite/statement.hpp
/// Wrapper classes and utilities for SQLite statement processing.
///
/// This module contains thin RAII wrappers around the SQLite 3 structures
/// representing statements.

#if !defined(UTILS_SQLITE_STATEMENT_HPP)
#define UTILS_SQLITE_STATEMENT_HPP

#include "utils/sqlite/statement_fwd.hpp"

extern "C" {
#include <stdint.h>
}

#include <memory>
#include <string>

#include "utils/sqlite/database_fwd.hpp"

namespace utils {
namespace sqlite {


/// Representation of a BLOB.
class blob {
public:
    /// Memory representing the contents of the blob, or NULL if empty.
    ///
    /// This memory must remain valid throughout the life of this object, as we
    /// do not grab ownership of the memory.
    const void* memory;

    /// Number of bytes in memory.
    int size;

    /// Constructs a new blob.
    ///
    /// \param memory_ Pointer to the contents of the blob.
    /// \param size_ The size of memory_.
    blob(const void* memory_, const int size_) :
        memory(memory_), size(size_)
    {
    }
};


/// Representation of a SQL NULL value.
class null {
};


/// A RAII model for an SQLite 3 statement.
class statement {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::shared_ptr< impl > _pimpl;

    statement(database&, void*);
    friend class database;

public:
    ~statement(void);

    bool step(void);
    void step_without_results(void);

    int column_count(void);
    std::string column_name(const int);
    type column_type(const int);
    int column_id(const char*);

    blob column_blob(const int);
    double column_double(const int);
    int column_int(const int);
    int64_t column_int64(const int);
    std::string column_text(const int);
    int column_bytes(const int);

    blob safe_column_blob(const char*);
    double safe_column_double(const char*);
    int safe_column_int(const char*);
    int64_t safe_column_int64(const char*);
    std::string safe_column_text(const char*);
    int safe_column_bytes(const char*);

    void reset(void);

    void bind(const int, const blob&);
    void bind(const int, const double);
    void bind(const int, const int);
    void bind(const int, const int64_t);
    void bind(const int, const null&);
    void bind(const int, const std::string&);
    template< class T > void bind(const char*, const T&);

    int bind_parameter_count(void);
    int bind_parameter_index(const std::string&);
    std::string bind_parameter_name(const int);

    void clear_bindings(void);
};


}  // namespace sqlite
}  // namespace utils

#endif  // !defined(UTILS_SQLITE_STATEMENT_HPP)
