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

#include <cassert>
#include <map>

#include "config.hpp"
#include "env.hpp"
#include "text.hpp"

static std::map< std::string, std::string > m_variables;

static struct var {
    const char *name;
    const char *default_value;
    bool can_be_empty;
} vars[] = {
    { "ATF_ARCH",           ATF_ARCH,           false, },
    { "ATF_BUILD_CC",       ATF_BUILD_CC,       false, },
    { "ATF_BUILD_CFLAGS",   ATF_BUILD_CFLAGS,   true,  },
    { "ATF_BUILD_CPP",      ATF_BUILD_CPP,      false, },
    { "ATF_BUILD_CPPFLAGS", ATF_BUILD_CPPFLAGS, true,  },
    { "ATF_BUILD_CXX",      ATF_BUILD_CXX,      false, },
    { "ATF_BUILD_CXXFLAGS", ATF_BUILD_CXXFLAGS, true,  },
    { "ATF_CONFDIR",        ATF_CONFDIR,        false, },
    { "ATF_INCLUDEDIR",     ATF_INCLUDEDIR,     false, },
    { "ATF_LIBDIR",         ATF_LIBDIR,         false, },
    { "ATF_LIBEXECDIR",     ATF_LIBEXECDIR,     false, },
    { "ATF_MACHINE",        ATF_MACHINE,        false, },
    { "ATF_PKGDATADIR",     ATF_PKGDATADIR,     false, },
    { "ATF_SHELL",          ATF_SHELL,          false, },
    { "ATF_WORKDIR",        ATF_WORKDIR,        false, },
    { NULL,                 NULL,               false, },
};

//
// Adds all predefined standard build-time variables to the m_variables
// map, considering the values a user may have provided in the environment.
//
// Can only be called once during the program's lifetime.
//
static
void
init_variables(void)
{
    assert(m_variables.empty());

    for (struct var* v = vars; v->name != NULL; v++) {
        const std::string varname = tools::text::to_lower(v->name);

        if (tools::env::has(v->name)) {
            const std::string envval = tools::env::get(v->name);
            if (envval.empty() && !v->can_be_empty)
                m_variables[varname] = v->default_value;
            else
                m_variables[varname] = envval;
        } else {
            m_variables[varname] = v->default_value;
        }
    }

    assert(!m_variables.empty());
}

const std::string&
tools::config::get(const std::string& varname)
{
    if (m_variables.empty())
        init_variables();

    assert(has(varname));
    return m_variables[varname];
}

const std::map< std::string, std::string >&
tools::config::get_all(void)
{
    if (m_variables.empty())
        init_variables();

    return m_variables;
}

bool
tools::config::has(const std::string& varname)
{
    if (m_variables.empty())
        init_variables();

    return m_variables.find(varname) != m_variables.end();
}

namespace tools {
namespace config {
//
// Auxiliary function for the t_config test program so that it can
// revert the configuration's global status to an empty state and
// do new tests from there on.
//
// Ideally this shouldn't be part of the production library... but
// this is so small that it does not matter.
//
void
__reinit(void)
{
    m_variables.clear();
}
} // namespace config
} // namespace tools
