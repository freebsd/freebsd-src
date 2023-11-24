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

#if !defined(UTILS_FORMAT_FORMATTER_IPP)
#define UTILS_FORMAT_FORMATTER_IPP

#include <ostream>

#include "utils/format/formatter.hpp"

namespace utils {
namespace format {


/// Replaces the first format placeholder in a formatter.
///
/// Constructs a new formatter object that has one less formatting placeholder,
/// as this has been replaced by the provided argument.  Calling this operator
/// N times, where N is the number of formatting placeholders, effectively
/// formats the string.
///
/// \param arg The argument to use as replacement for the format placeholder.
///
/// \return A new formatter that has one less format placeholder.
template< typename Type >
inline formatter
formatter::operator%(const Type& arg) const
{
    (*_oss) << arg;
    return replace(_oss->str());
}


/// Inserts a formatter string into a stream.
///
/// \param os The output stream.
/// \param f The formatter to process and inject into the stream.
///
/// \return The output stream os.
inline std::ostream&
operator<<(std::ostream& os, const formatter& f)
{
    return (os << f.str());
}


}  // namespace format
}  // namespace utils


#endif  // !defined(UTILS_FORMAT_FORMATTER_IPP)
