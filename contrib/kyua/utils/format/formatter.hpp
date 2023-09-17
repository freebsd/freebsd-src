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

/// \file utils/format/formatter.hpp
/// Provides the definition of the utils::format::formatter class.
///
/// The utils::format::formatter class is a poor man's replacement for the
/// Boost.Format library, as it is much simpler and has less dependencies.
///
/// Be aware that the formatting supported by this module is NOT compatible
/// with printf(3) nor with Boost.Format.  The general syntax for a
/// placeholder in a formatting string is:
///
///     %[0][width][.precision]s
///
/// In particular, note that the only valid formatting specifier is %s: the
/// library deduces what to print based on the type of the variable passed
/// in, not based on what the format string says.  Also, note that the only
/// valid padding character is 0.

#if !defined(UTILS_FORMAT_FORMATTER_HPP)
#define UTILS_FORMAT_FORMATTER_HPP

#include "utils/format/formatter_fwd.hpp"

#include <sstream>
#include <string>

namespace utils {
namespace format {


/// Mechanism to format strings similar to printf.
///
/// A formatter always maintains the original format string but also holds a
/// partial expansion.  The partial expansion is immutable in the context of a
/// formatter instance, but calls to operator% return new formatter objects with
/// one less formatting placeholder.
///
/// In general, one can format a string in the following manner:
///
/// \code
/// const std::string s = (formatter("%s %s") % "foo" % 5).str();
/// \endcode
///
/// which, following the explanation above, would correspond to:
///
/// \code
/// const formatter f1("%s %s");
/// const formatter f2 = f1 % "foo";
/// const formatter f3 = f2 % 5;
/// const std::string s = f3.str();
/// \endcode
class formatter {
    /// The original format string provided by the user.
    std::string _format;

    /// The current "expansion" of the format string.
    ///
    /// This field gets updated on every call to operator%() to have one less
    /// formatting placeholder.
    std::string _expansion;

    /// The position of _expansion from which to scan for placeholders.
    std::string::size_type _last_pos;

    /// The position of the first placeholder in the current expansion.
    std::string::size_type _placeholder_pos;

    /// The first placeholder in the current expansion.
    std::string _placeholder;

    /// Stream used to format any possible argument supplied by operator%().
    std::ostringstream* _oss;

    formatter replace(const std::string&) const;

    void init(void);
    formatter(const std::string&, const std::string&,
              const std::string::size_type);

public:
    explicit formatter(const std::string&);
    ~formatter(void);

    const std::string& str(void) const;
    operator const std::string&(void) const;

    template< typename Type > formatter operator%(const Type&) const;
    formatter operator%(const bool&) const;
};


}  // namespace format
}  // namespace utils


#endif  // !defined(UTILS_FORMAT_FORMATTER_HPP)
