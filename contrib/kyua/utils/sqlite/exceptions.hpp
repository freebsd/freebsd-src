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

/// \file utils/sqlite/exceptions.hpp
/// Exception types raised by the sqlite module.

#if !defined(UTILS_SQLITE_EXCEPTIONS_HPP)
#define UTILS_SQLITE_EXCEPTIONS_HPP

#include <stdexcept>
#include <string>

#include "utils/fs/path_fwd.hpp"
#include "utils/optional.hpp"
#include "utils/sqlite/database_fwd.hpp"

namespace utils {
namespace sqlite {


/// Base exception for sqlite errors.
class error : public std::runtime_error {
    /// Path to the database that raised this error.
    utils::optional< utils::fs::path > _db_filename;

public:
    explicit error(const utils::optional< utils::fs::path >&,
                   const std::string&);
    virtual ~error(void) throw();

    const utils::optional< utils::fs::path >& db_filename(void) const;
};


/// Exception for errors raised by the SQLite 3 API library.
class api_error : public error {
    /// The name of the SQLite 3 C API function that caused this error.
    std::string _api_function;

public:
    explicit api_error(const utils::optional< utils::fs::path >&,
                       const std::string&, const std::string&);
    virtual ~api_error(void) throw();

    static api_error from_database(database&, const std::string&);

    const std::string& api_function(void) const;
};


/// The caller requested a non-existent column name.
class invalid_column_error : public error {
    /// The name of the invalid column.
    std::string _column_name;

public:
    explicit invalid_column_error(const utils::optional< utils::fs::path >&,
                                  const std::string&);
    virtual ~invalid_column_error(void) throw();

    const std::string& column_name(void) const;
};


}  // namespace sqlite
}  // namespace utils


#endif  // !defined(UTILS_SQLITE_EXCEPTIONS_HPP)
