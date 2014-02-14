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

#if defined(HAVE_CONFIG_H)
#include "bconfig.h"
#endif

extern "C" {
#include <errno.h>
#include <stdlib.h>
#include <string.h>
}

#include <cassert>

#include "env.hpp"
#include "exceptions.hpp"

namespace impl = tools::env;
#define IMPL_NAME "tools::env"

// ------------------------------------------------------------------------
// Free functions.
// ------------------------------------------------------------------------

std::string
impl::get(const std::string& name)
{
    const char* val = getenv(name.c_str());
    assert(val != NULL);
    return val;
}

bool
impl::has(const std::string& name)
{
    return getenv(name.c_str()) != NULL;
}

void
impl::set(const std::string& name, const std::string& val)
{
#if defined(HAVE_SETENV)
    if (setenv(name.c_str(), val.c_str(), 1) == -1)
        throw tools::system_error(IMPL_NAME "::set",
                                "Cannot set environment variable '" + name +
                                "' to '" + val + "'",
                                errno);
#elif defined(HAVE_PUTENV)
    const std::string buf = name + "=" + val;
    if (putenv(strdup(buf.c_str())) == -1)
        throw tools::system_error(IMPL_NAME "::set",
                                "Cannot set environment variable '" + name +
                                "' to '" + val + "'",
                                errno);
#else
#   error "Don't know how to set an environment variable."
#endif
}

void
impl::unset(const std::string& name)
{
#if defined(HAVE_UNSETENV)
    unsetenv(name.c_str());
#elif defined(HAVE_PUTENV)
    const std::string buf = name + "=";

    if (putenv(strdup(buf.c_str())) == -1)
        throw tools::system_error(IMPL_NAME "::unset",
                                "Cannot unset environment variable '" +
                                name + "'", errno);
#else
#   error "Don't know how to unset an environment variable."
#endif
}
