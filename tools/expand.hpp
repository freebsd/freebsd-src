//
// Automated Testing Framework (atf)
//
// Copyright (c) 2007 The NetBSD Foundation, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
// CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
// IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#if !defined(TOOLS_EXPAND_HPP)
#define TOOLS_EXPAND_HPP

#include <string>
#include <vector>

namespace tools {
namespace expand {

// ------------------------------------------------------------------------
// Free functions.
// ------------------------------------------------------------------------

//!
//! \brief Checks if the given string is a glob pattern.
//!
//! Returns true if the given string is a glob pattern; i.e. if it contains
//! any character that will be expanded by expand_glob.
//!
bool is_glob(const std::string&);

//!
//! \brief Checks if a given string matches a glob pattern.
//!
//! Given a glob pattern and a string, checks whether the former matches
//! the latter.  Returns a boolean indicating this condition.
//!
bool matches_glob(const std::string&, const std::string&);

//!
//! \brief Expands a glob pattern among multiple candidates.
//!
//! Given a glob pattern and a set of candidate strings, checks which of
//! those strings match the glob pattern and returns them.
//!
template< class T >
std::vector< std::string > expand_glob(const std::string& glob,
                                       const T& candidates)
{
    std::vector< std::string > exps;

    for (typename T::const_iterator iter = candidates.begin();
         iter != candidates.end(); iter++)
        if (matches_glob(glob, *iter))
            exps.push_back(*iter);

    return exps;
}

} // namespace expand
} // namespace tools

#endif // !defined(TOOLS_EXPAND_HPP)
