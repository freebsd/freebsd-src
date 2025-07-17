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

/// \file utils/text/operations.hpp
/// Utilities to manipulate strings.

#if !defined(UTILS_TEXT_OPERATIONS_HPP)
#define UTILS_TEXT_OPERATIONS_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace utils {
namespace text {


std::string escape_xml(const std::string&);
std::string quote(const std::string&, const char);


std::vector< std::string > refill(const std::string&, const std::size_t);
std::string refill_as_string(const std::string&, const std::size_t);

std::string replace_all(const std::string&, const std::string&,
                        const std::string&);

template< typename Collection >
std::string join(const Collection&, const std::string&);
std::vector< std::string > split(const std::string&, const char);

template< typename Type >
Type to_type(const std::string&);
template<>
bool to_type(const std::string&);
template<>
std::string to_type(const std::string&);


}  // namespace text
}  // namespace utils

#endif  // !defined(UTILS_TEXT_OPERATIONS_HPP)
