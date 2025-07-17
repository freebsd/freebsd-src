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

/// \file utils/cmdline/parser.hpp
/// Routines and data types to parse command line options and arguments.

#if !defined(UTILS_CMDLINE_PARSER_HPP)
#define UTILS_CMDLINE_PARSER_HPP

#include "utils/cmdline/parser_fwd.hpp"

#include <map>
#include <string>
#include <vector>

namespace utils {
namespace cmdline {


/// Representation of a parsed command line.
///
/// This class is returned by the command line parsing algorithm and provides
/// methods to query the values of the options and the value of the arguments.
/// All the values fed into this class can considered to be sane (i.e. the
/// arguments to the options and the arguments to the command are valid), as all
/// validation happens during parsing (before this class is instantiated).
class parsed_cmdline {
    /// Mapping of option names to all the values provided.
    std::map< std::string, std::vector< std::string > > _option_values;

    /// Collection of arguments with all options removed.
    args_vector _arguments;

    const std::vector< std::string >& get_option_raw(const std::string&) const;

public:
    parsed_cmdline(const std::map< std::string, std::vector< std::string > >&,
                   const args_vector&);

    bool has_option(const std::string&) const;

    template< typename Option >
    typename Option::option_type get_option(const std::string&) const;

    template< typename Option >
    std::vector< typename Option::option_type > get_multi_option(
        const std::string&) const;

    const args_vector& arguments(void) const;
};


parsed_cmdline parse(const args_vector&, const options_vector&);
parsed_cmdline parse(const int, const char* const*, const options_vector&);


}  // namespace cmdline
}  // namespace utils

#endif  // !defined(UTILS_CMDLINE_PARSER_HPP)
