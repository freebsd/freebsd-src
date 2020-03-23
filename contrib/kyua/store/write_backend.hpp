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

/// \file store/write_backend.hpp
/// Interface to the backend database for write-only operations.

#if !defined(STORE_WRITE_BACKEND_HPP)
#define STORE_WRITE_BACKEND_HPP

#include "store/write_backend_fwd.hpp"

#include <memory>

#include "store/metadata_fwd.hpp"
#include "store/write_transaction_fwd.hpp"
#include "utils/fs/path_fwd.hpp"
#include "utils/sqlite/database_fwd.hpp"

namespace store {


namespace detail {


utils::fs::path schema_file(void);
metadata initialize(utils::sqlite::database&);


}  // anonymous namespace


/// Public interface to the database store for write-only operations.
class write_backend {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::shared_ptr< impl > _pimpl;

    friend class metadata;

    write_backend(impl*);

public:
    ~write_backend(void);

    static write_backend open_rw(const utils::fs::path&);
    void close(void);

    utils::sqlite::database& database(void);
    write_transaction start_write(void);
};


}  // namespace store

#endif  // !defined(STORE_WRITE_BACKEND_HPP)
