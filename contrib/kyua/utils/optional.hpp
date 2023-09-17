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

/// \file utils/optional.hpp
/// Provides the utils::optional class.
///
/// The class is provided as a separate module on its own to minimize
/// header-inclusion side-effects.

#if !defined(UTILS_OPTIONAL_HPP)
#define UTILS_OPTIONAL_HPP

#include "utils/optional_fwd.hpp"

#include <ostream>

namespace utils {


/// Holds a data value or none.
///
/// This class allows users to represent values that may be uninitialized.
/// Instead of having to keep separate variables to track whether a variable is
/// supposed to have a value or not, this class allows multiplexing the
/// behaviors.
///
/// This class is a simplified version of Boost.Optional.
template< class T >
class optional {
    /// Internal representation of the optional data value.
    T* _data;

public:
    optional(void);
    optional(utils::detail::none_t);
    optional(const optional< T >&);
    explicit optional(const T&);
    ~optional(void);

    optional& operator=(utils::detail::none_t);
    optional& operator=(const T&);
    optional& operator=(const optional< T >&);

    bool operator==(const optional< T >&) const;
    bool operator!=(const optional< T >&) const;

    operator bool(void) const;

    const T& get(void) const;
    const T& get_default(const T&) const;
    T& get(void);
};


template< class T >
std::ostream& operator<<(std::ostream&, const optional< T >&);


template< class T >
optional< T > make_optional(const T&);


}  // namespace utils

#endif  // !defined(UTILS_OPTIONAL_HPP)
