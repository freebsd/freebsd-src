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

/// \file utils/format/exceptions.hpp
/// Exception types raised by the format module.

#if !defined(UTILS_FORMAT_EXCEPTIONS_HPP)
#define UTILS_FORMAT_EXCEPTIONS_HPP

#include <stdexcept>
#include <string>

namespace utils {
namespace format {


/// Base exception for format errors.
class error : public std::runtime_error {
public:
    explicit error(const std::string&);
    virtual ~error(void) throw();
};


/// Error denoting a bad format string.
class bad_format_error : public error {
    /// The format string that caused the error.
    std::string _format;

public:
    explicit bad_format_error(const std::string&, const std::string&);
    virtual ~bad_format_error(void) throw();

    const std::string& format(void) const;
};


/// Error denoting too many arguments for the format string.
class extra_args_error : public error {
    /// The format string that was passed too many arguments.
    std::string _format;

    /// The first argument that caused the error.
    std::string _arg;

public:
    explicit extra_args_error(const std::string&, const std::string&);
    virtual ~extra_args_error(void) throw();

    const std::string& format(void) const;
    const std::string& arg(void) const;
};


}  // namespace format
}  // namespace utils


#endif  // !defined(UTILS_FORMAT_EXCEPTIONS_HPP)
