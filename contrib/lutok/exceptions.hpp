// Copyright 2011 Google Inc.
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

/// \file exceptions.hpp
/// Exception types raised by lutok.

#if !defined(LUTOK_EXCEPTIONS_HPP)
#define LUTOK_EXCEPTIONS_HPP

#include <stdexcept>
#include <string>

namespace lutok {


class state;


/// Base exception for lua errors.
class error : public std::runtime_error {
public:
    explicit error(const std::string&);
    virtual ~error(void) throw();
};


/// Exception for errors raised by the Lua API library.
class api_error : public error {
    /// Name of the Lua C API function that caused the error.
    std::string _api_function;

public:
    explicit api_error(const std::string&, const std::string&);
    virtual ~api_error(void) throw();

    static api_error from_stack(state&, const std::string&);

    const std::string& api_function(void) const;
};


/// File not found error.
class file_not_found_error : public error {
    /// Name of the not-found file.
    std::string _filename;

public:
    explicit file_not_found_error(const std::string&);
    virtual ~file_not_found_error(void) throw();

    const std::string& filename(void) const;
};


}  // namespace lutok


#endif  // !defined(LUTOK_EXCEPTIONS_HPP)
