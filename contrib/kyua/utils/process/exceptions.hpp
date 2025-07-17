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

/// \file utils/process/exceptions.hpp
/// Exception types raised by the process module.

#if !defined(UTILS_PROCESS_EXCEPTIONS_HPP)
#define UTILS_PROCESS_EXCEPTIONS_HPP

#include <stdexcept>

namespace utils {
namespace process {


/// Base exceptions for process errors.
class error : public std::runtime_error {
public:
    explicit error(const std::string&);
    ~error(void) throw();
};


/// Exceptions for errno-based errors.
///
/// TODO(jmmv): This code is duplicated in, at least, utils::fs.  Figure
/// out a way to reuse this exception while maintaining the correct inheritance
/// (i.e. be able to keep it as a child of process::error).
class system_error : public error {
    /// Error number describing this libc error condition.
    int _original_errno;

public:
    explicit system_error(const std::string&, const int);
    ~system_error(void) throw();

    int original_errno(void) const throw();
};


/// Denotes that a deadline was exceeded.
class timeout_error : public error {
public:
    explicit timeout_error(const std::string&);
    ~timeout_error(void) throw();
};


}  // namespace process
}  // namespace utils


#endif  // !defined(UTILS_PROCESS_EXCEPTIONS_HPP)
