// Copyright 2014 The Kyua Authors.
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

/// \file utils/text/regex.hpp
/// Utilities to build and match regular expressions.

#if !defined(UTILS_TEXT_REGEX_HPP)
#define UTILS_TEXT_REGEX_HPP

#include "utils/text/regex_fwd.hpp"

#include <cstddef>
#include <memory>


namespace utils {
namespace text {


/// Container for regex match results.
class regex_matches {
    struct impl;

    /// Pointer to shared implementation.
    std::shared_ptr< impl > _pimpl;

    friend class regex;
    regex_matches(std::shared_ptr< impl >);

public:
    ~regex_matches(void);

    std::size_t count(void) const;
    std::string get(const std::size_t) const;

    operator bool(void) const;
};


/// Regular expression compiler and executor.
///
/// All regular expressions handled by this class are "extended".
class regex {
    struct impl;

    /// Pointer to shared implementation.
    std::shared_ptr< impl > _pimpl;

    regex(std::shared_ptr< impl >);

public:
    ~regex(void);

    static regex compile(const std::string&, const std::size_t,
                         const bool = false);
    regex_matches match(const std::string&) const;
};


regex_matches match_regex(const std::string&, const std::string&,
                          const std::size_t, const bool = false);


}  // namespace text
}  // namespace utils

#endif  // !defined(UTILS_TEXT_REGEX_HPP)
