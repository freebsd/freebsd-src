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

/// \file store/write_transaction.hpp
/// Implementation of write-only transactions on the backend.

#if !defined(STORE_WRITE_TRANSACTION_HPP)
#define STORE_WRITE_TRANSACTION_HPP

#include "store/write_transaction_fwd.hpp"

extern "C" {
#include <stdint.h>
}

#include <memory>
#include <string>

#include "model/context_fwd.hpp"
#include "model/test_program_fwd.hpp"
#include "model/test_result_fwd.hpp"
#include "store/write_backend_fwd.hpp"
#include "utils/datetime_fwd.hpp"
#include "utils/fs/path_fwd.hpp"
#include "utils/optional_fwd.hpp"

namespace store {


/// Representation of a write-only transaction.
///
/// Transactions are the entry place for high-level calls that access the
/// database.
class write_transaction {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::shared_ptr< impl > _pimpl;

    friend class write_backend;
    write_transaction(write_backend&);

public:
    ~write_transaction(void);

    void commit(void);
    void rollback(void);

    void put_context(const model::context&);
    int64_t put_test_program(const model::test_program&);
    int64_t put_test_case(const model::test_program&, const std::string&,
                          const int64_t);
    utils::optional< int64_t > put_test_case_file(const std::string&,
                                                  const utils::fs::path&,
                                                  const int64_t);
    int64_t put_result(const model::test_result&, const int64_t,
                       const utils::datetime::timestamp&,
                       const utils::datetime::timestamp&);
};


}  // namespace store

#endif  // !defined(STORE_WRITE_TRANSACTION_HPP)
