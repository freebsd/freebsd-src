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

#if !defined(UTILS_CMDLINE_PARSER_IPP)
#define UTILS_CMDLINE_PARSER_IPP

#include "utils/cmdline/parser.hpp"


/// Gets the value of an option.
///
/// If the option has been specified multiple times on the command line, this
/// only returns the last value.  This is the traditional behavior.
///
/// The option must support arguments.  Otherwise, a call to this function will
/// not compile because the option type will lack the definition of some fields
/// and/or methods.
///
/// \param name The option to query.
///
/// \return The value of the option converted to the appropriate type.
///
/// \pre has_option(name) must be true.
template< typename Option > typename Option::option_type
utils::cmdline::parsed_cmdline::get_option(const std::string& name) const
{
    const std::vector< std::string >& raw_values = get_option_raw(name);
    return Option::convert(raw_values[raw_values.size() - 1]);
}


/// Gets the values of an option that supports repetition.
///
/// The option must support arguments.  Otherwise, a call to this function will
/// not compile because the option type will lack the definition of some fields
/// and/or methods.
///
/// \param name The option to query.
///
/// \return The values of the option converted to the appropriate type.
///
/// \pre has_option(name) must be true.
template< typename Option > std::vector< typename Option::option_type >
utils::cmdline::parsed_cmdline::get_multi_option(const std::string& name) const
{
    std::vector< typename Option::option_type > values;

    const std::vector< std::string >& raw_values = get_option_raw(name);
    for (std::vector< std::string >::const_iterator iter = raw_values.begin();
         iter != raw_values.end(); iter++) {
        values.push_back(Option::convert(*iter));
    }

    return values;
}


#endif  // !defined(UTILS_CMDLINE_PARSER_IPP)
