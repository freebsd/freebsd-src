// Copyright 2010 The Kyua Authors.
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

/// \file engine/exceptions.hpp
/// Exception types raised by the engine module.

#if !defined(ENGINE_EXCEPTIONS_HPP)
#define ENGINE_EXCEPTIONS_HPP

#include <stdexcept>

#include "utils/fs/path.hpp"

namespace engine {


/// Base exception for engine errors.
class error : public std::runtime_error {
public:
    explicit error(const std::string&);
    virtual ~error(void) throw();
};


/// Error while processing data.
class format_error : public error {
public:
    explicit format_error(const std::string&);
    virtual ~format_error(void) throw();
};


/// Error while parsing external data.
class load_error : public error {
public:
    /// The path to the file that caused the load error.
    utils::fs::path file;

    /// The reason for the error; may not include the file name.
    std::string reason;

    explicit load_error(const utils::fs::path&, const std::string&);
    virtual ~load_error(void) throw();
};


}  // namespace engine


#endif  // !defined(ENGINE_EXCEPTIONS_HPP)
