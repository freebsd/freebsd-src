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

#include <stdexcept>

#include "expand.hpp"
#include "text.hpp"

namespace impl = tools::expand;
#define IMPL_NAME "tools::expand"

// ------------------------------------------------------------------------
// Auxiliary functions.
// ------------------------------------------------------------------------

namespace {

std::string
glob_to_regex(const std::string& glob)
{
    std::string regex;
    regex.reserve(glob.length() * 2);

    regex += '^';
    for (std::string::const_iterator iter = glob.begin(); iter != glob.end();
         iter++) {
        switch (*iter) {
        case '*': regex += ".*"; break;
        case '?': regex += "."; break;
        default: regex += *iter;
        }
    }
    regex += '$';

    return regex;
}

} // anonymous namespace

// ------------------------------------------------------------------------
// Free functions.
// ------------------------------------------------------------------------

bool
impl::is_glob(const std::string& glob)
{
    // NOTE: Keep this in sync with glob_to_regex!
    return glob.find_first_of("*?") != std::string::npos;
}

bool
impl::matches_glob(const std::string& glob, const std::string& candidate)
{
    return tools::text::match(candidate, glob_to_regex(glob));
}
