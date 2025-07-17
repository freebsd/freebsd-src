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

/// \file utils/units.hpp
/// Formatters and parsers of user-friendly units.

#if !defined(UTILS_UNITS_HPP)
#define UTILS_UNITS_HPP

#include "utils/units_fwd.hpp"

extern "C" {
#include <stdint.h>
}

#include <istream>
#include <ostream>
#include <string>

namespace utils {
namespace units {


namespace {

/// Constant representing 1 kilobyte.
const uint64_t KB = int64_t(1) << 10;

/// Constant representing 1 megabyte.
const uint64_t MB = int64_t(1) << 20;

/// Constant representing 1 gigabyte.
const uint64_t GB = int64_t(1) << 30;

/// Constant representing 1 terabyte.
const uint64_t TB = int64_t(1) << 40;

}  // anonymous namespace


/// Representation of a bytes quantity.
///
/// The purpose of this class is to represent an amount of bytes in a semantic
/// manner, and to provide functions to format such numbers for nice user
/// presentation and to parse back such numbers.
///
/// The input follows this regular expression: [0-9]+(|\.[0-9]+)[GgKkMmTt]?
/// The output follows this regular expression: [0-9]+\.[0-9]{3}[GKMT]?
class bytes {
    /// Raw representation, in bytes, of the quantity.
    uint64_t _count;

public:
    bytes(void);
    explicit bytes(const uint64_t);

    static bytes parse(const std::string&);
    std::string format(void) const;

    operator uint64_t(void) const;
};


std::istream& operator>>(std::istream&, bytes&);
std::ostream& operator<<(std::ostream&, const bytes&);


}  // namespace units
}  // namespace utils

#endif  // !defined(UTILS_UNITS_HPP)
