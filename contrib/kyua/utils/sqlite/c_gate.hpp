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

/// \file c_gate.hpp
/// Provides direct access to the C state of the SQLite wrappers.

#if !defined(UTILS_SQLITE_C_GATE_HPP)
#define UTILS_SQLITE_C_GATE_HPP

#include "utils/sqlite/c_gate_fwd.hpp"

extern "C" {
#include <sqlite3.h>
}

#include "utils/sqlite/database_fwd.hpp"

namespace utils {
namespace sqlite {


/// Gateway to the raw C database of SQLite 3.
///
/// This class provides a mechanism to muck with the internals of the database
/// wrapper class.
///
/// \warning The use of this class is discouraged.  By using this class, you are
/// entering the world of unsafety.  Anything you do through the objects exposed
/// through this class will not be controlled by RAII patterns not validated in
/// any other way, so you can end up corrupting the SQLite 3 state and later get
/// crashes on otherwise perfectly-valid C++ code.
class database_c_gate {
    /// The C++ database that this class wraps.
    database& _database;

public:
    database_c_gate(database&);
    ~database_c_gate(void);

    static database connect(::sqlite3*);

    ::sqlite3* c_database(void);
};


}  // namespace sqlite
}  // namespace utils

#endif  // !defined(UTILS_SQLITE_C_GATE_HPP)
