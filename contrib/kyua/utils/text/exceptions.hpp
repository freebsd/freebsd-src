// Copyright 2012 The Kyua Authors.
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

/// \file utils/text/exceptions.hpp
/// Exception types raised by the text module.

#if !defined(UTILS_TEXT_EXCEPTIONS_HPP)
#define UTILS_TEXT_EXCEPTIONS_HPP

#include <stdexcept>

namespace utils {
namespace text {


/// Base exceptions for text errors.
class error : public std::runtime_error {
public:
    explicit error(const std::string&);
    ~error(void) throw();
};


/// Exception denoting an error in a regular expression.
class regex_error : public error {
public:
    explicit regex_error(const std::string&);
    ~regex_error(void) throw();
};


/// Exception denoting an error while parsing templates.
class syntax_error : public error {
public:
    explicit syntax_error(const std::string&);
    ~syntax_error(void) throw();
};


/// Exception denoting an error in a text value format.
class value_error : public error {
public:
    explicit value_error(const std::string&);
    ~value_error(void) throw();
};


}  // namespace text
}  // namespace utils


#endif  // !defined(UTILS_TEXT_EXCEPTIONS_HPP)
