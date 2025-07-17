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

#include "utils/config/tree.ipp"

#include "utils/config/exceptions.hpp"
#include "utils/format/macros.hpp"
#include "utils/text/operations.hpp"

namespace config = utils::config;
namespace text = utils::text;


/// Converts a key to its textual representation.
///
/// \param key The key to convert.
///
/// \return a flattened representation of \p key, "."-joined.
std::string
utils::config::detail::flatten_key(const tree_key& key)
{
    PRE(!key.empty());
    return text::join(key, ".");
}


/// Parses and validates a textual key.
///
/// \param str The key to process in dotted notation.
///
/// \return The tokenized key if valid.
///
/// \throw invalid_key_error If the input key is empty or invalid for any other
///     reason.  Invalid does NOT mean unknown though.
utils::config::detail::tree_key
utils::config::detail::parse_key(const std::string& str)
{
    const tree_key key = text::split(str, '.');
    if (key.empty())
        throw invalid_key_error("Empty key");
    for (tree_key::const_iterator iter = key.begin(); iter != key.end(); iter++)
        if ((*iter).empty())
            throw invalid_key_error(F("Empty component in key '%s'") % str);
    return key;
}
