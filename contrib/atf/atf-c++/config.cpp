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

#include <map>

extern "C" {
#include "atf-c/config.h"
}

#include "config.hpp"

#include "detail/env.hpp"
#include "detail/sanity.hpp"

static std::map< std::string, std::string > m_variables;

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
    PRE(m_variables.empty());

    m_variables["atf_build_cc"] = atf_config_get("atf_build_cc");
    m_variables["atf_build_cflags"] = atf_config_get("atf_build_cflags");
    m_variables["atf_build_cpp"] = atf_config_get("atf_build_cpp");
    m_variables["atf_build_cppflags"] = atf_config_get("atf_build_cppflags");
    m_variables["atf_build_cxx"] = atf_config_get("atf_build_cxx");
    m_variables["atf_build_cxxflags"] = atf_config_get("atf_build_cxxflags");
    m_variables["atf_includedir"] = atf_config_get("atf_includedir");
    m_variables["atf_libexecdir"] = atf_config_get("atf_libexecdir");
    m_variables["atf_pkgdatadir"] = atf_config_get("atf_pkgdatadir");
    m_variables["atf_shell"] = atf_config_get("atf_shell");
    m_variables["atf_workdir"] = atf_config_get("atf_workdir");

    POST(!m_variables.empty());
}

const std::string&
atf::config::get(const std::string& varname)
{
    if (m_variables.empty())
        init_variables();

    PRE(has(varname));
    return m_variables[varname];
}

const std::map< std::string, std::string >&
atf::config::get_all(void)
{
    if (m_variables.empty())
        init_variables();

    return m_variables;
}

bool
atf::config::has(const std::string& varname)
{
    if (m_variables.empty())
        init_variables();

    return m_variables.find(varname) != m_variables.end();
}

extern "C" {
void __atf_config_reinit(void);
}

namespace atf {
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
    __atf_config_reinit();
    m_variables.clear();
}
} // namespace config
} // namespace atf
