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

/// \file store/dbtypes.hpp
/// Functions to internalize/externalize various types.
///
/// These helper functions are only provided to help in the implementation of
/// other modules.  Therefore, this header file should never be included from
/// other header files.

#if defined(STORE_DBTYPES_HPP)
#   error "Do not include dbtypes.hpp multiple times"
#endif  // !defined(STORE_DBTYPES_HPP)
#define STORE_DBTYPES_HPP

#include <string>

#include "model/test_result_fwd.hpp"
#include "utils/datetime_fwd.hpp"
#include "utils/sqlite/statement_fwd.hpp"

namespace store {


void bind_bool(utils::sqlite::statement&, const char*, const bool);
void bind_delta(utils::sqlite::statement&, const char*,
                const utils::datetime::delta&);
void bind_optional_string(utils::sqlite::statement&, const char*,
                          const std::string&);
void bind_test_result_type(utils::sqlite::statement&, const char*,
                           const model::test_result_type&);
void bind_timestamp(utils::sqlite::statement&, const char*,
                    const utils::datetime::timestamp&);
bool column_bool(utils::sqlite::statement&, const char*);
utils::datetime::delta column_delta(utils::sqlite::statement&, const char*);
std::string column_optional_string(utils::sqlite::statement&, const char*);
model::test_result_type column_test_result_type(
    utils::sqlite::statement&, const char*);
utils::datetime::timestamp column_timestamp(utils::sqlite::statement&,
                                            const char*);


}  // namespace store
