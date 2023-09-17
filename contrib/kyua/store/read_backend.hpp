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

/// \file store/read_backend.hpp
/// Interface to the backend database for read-only operations.

#if !defined(STORE_READ_BACKEND_HPP)
#define STORE_READ_BACKEND_HPP

#include "store/read_backend_fwd.hpp"

#include <memory>

#include "store/read_transaction_fwd.hpp"
#include "utils/fs/path_fwd.hpp"
#include "utils/sqlite/database_fwd.hpp"

namespace store {


namespace detail {


utils::sqlite::database open_and_setup(const utils::fs::path&, const int);


}  // anonymous namespace


/// Public interface to the database store for read-only operations.
class read_backend {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::shared_ptr< impl > _pimpl;

    read_backend(impl*);

public:
    ~read_backend(void);

    static read_backend open_ro(const utils::fs::path&);
    void close(void);

    utils::sqlite::database& database(void);
    read_transaction start_read(void);
};


}  // namespace store

#endif  // !defined(STORE_READ_BACKEND_HPP)
