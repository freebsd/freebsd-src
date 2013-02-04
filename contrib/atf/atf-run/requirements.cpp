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

extern "C" {
#include <sys/param.h>
#include <sys/sysctl.h>
}

#include <cerrno>
#include <cstring>
#include <stdexcept>

extern "C" {
#include "atf-c/defs.h"
}

#include "atf-c++/config.hpp"

#include "atf-c++/detail/fs.hpp"
#include "atf-c++/detail/env.hpp"
#include "atf-c++/detail/sanity.hpp"
#include "atf-c++/detail/text.hpp"

#include "requirements.hpp"
#include "user.hpp"

namespace impl = atf::atf_run;

namespace {

static
bool
has_program(const atf::fs::path& program)
{
    bool found = false;

    if (program.is_absolute()) {
        found = atf::fs::is_executable(program);
    } else {
        if (program.str().find('/') != std::string::npos)
            throw std::runtime_error("Relative paths are not allowed "
                                     "when searching for a program (" +
                                     program.str() + ")");

        const std::vector< std::string > dirs = atf::text::split(
            atf::env::get("PATH"), ":");
        for (std::vector< std::string >::const_iterator iter = dirs.begin();
             !found && iter != dirs.end(); iter++) {
            const atf::fs::path& p = atf::fs::path(*iter) / program;
            if (atf::fs::is_executable(p))
                found = true;
        }
    }

    return found;
}

static
std::string
check_arch(const std::string& arches)
{
    const std::vector< std::string > v = atf::text::split(arches, " ");

    for (std::vector< std::string >::const_iterator iter = v.begin();
         iter != v.end(); iter++) {
        if ((*iter) == atf::config::get("atf_arch"))
            return "";
    }

    if (v.size() == 1)
        return "Requires the '" + arches + "' architecture";
    else
        return "Requires one of the '" + arches + "' architectures";
}

static
std::string
check_config(const std::string& variables, const atf::tests::vars_map& config)
{
    const std::vector< std::string > v = atf::text::split(variables, " ");
    for (std::vector< std::string >::const_iterator iter = v.begin();
         iter != v.end(); iter++) {
        if (config.find((*iter)) == config.end())
            return "Required configuration variable '" + (*iter) + "' not "
                "defined";
    }
    return "";
}

static
std::string
check_files(const std::string& progs)
{
    const std::vector< std::string > v = atf::text::split(progs, " ");
    for (std::vector< std::string >::const_iterator iter = v.begin();
         iter != v.end(); iter++) {
        const atf::fs::path file(*iter);
        if (!file.is_absolute())
            throw std::runtime_error("Relative paths are not allowed when "
                "checking for a required file (" + file.str() + ")");
        if (!atf::fs::exists(file))
            return "Required file '" + file.str() + "' not found";
    }
    return "";
}

static
std::string
check_machine(const std::string& machines)
{
    const std::vector< std::string > v = atf::text::split(machines, " ");

    for (std::vector< std::string >::const_iterator iter = v.begin();
         iter != v.end(); iter++) {
        if ((*iter) == atf::config::get("atf_machine"))
            return "";
    }

    if (v.size() == 1)
        return "Requires the '" + machines + "' machine type";
    else
        return "Requires one of the '" + machines + "' machine types";
}

#if defined(__APPLE__) || defined(__NetBSD__)
static
std::string
check_memory_sysctl(const int64_t needed, const char* sysctl_variable)
{
    int64_t available;
    std::size_t available_length = sizeof(available);
    if (::sysctlbyname(sysctl_variable, &available, &available_length,
                       NULL, 0) == -1) {
        const char* e = std::strerror(errno);
        return "Failed to get sysctl(hw.usermem64) value: " + std::string(e);
    }

    if (available < needed) {
        return "Not enough memory; needed " + atf::text::to_string(needed) +
            ", available " + atf::text::to_string(available);
    } else
        return "";
}
#   if defined(__APPLE__)
static
std::string
check_memory_darwin(const int64_t needed)
{
    return check_memory_sysctl(needed, "hw.usermem");
}
#   elif defined(__NetBSD__)
static
std::string
check_memory_netbsd(const int64_t needed)
{
    return check_memory_sysctl(needed, "hw.usermem64");
}
#   else
#      error "Conditional error"
#   endif
#else
static
std::string
check_memory_unknown(const int64_t needed ATF_DEFS_ATTRIBUTE_UNUSED)
{
    return "";
}
#endif

static
std::string
check_memory(const std::string& raw_memory)
{
    const int64_t needed = atf::text::to_bytes(raw_memory);

#if defined(__APPLE__)
    return check_memory_darwin(needed);
#elif defined(__NetBSD__)
    return check_memory_netbsd(needed);
#else
    return check_memory_unknown(needed);
#endif
}

static
std::string
check_progs(const std::string& progs)
{
    const std::vector< std::string > v = atf::text::split(progs, " ");
    for (std::vector< std::string >::const_iterator iter = v.begin();
         iter != v.end(); iter++) {
        if (!has_program(atf::fs::path(*iter)))
            return "Required program '" + (*iter) + "' not found in the PATH";
    }
    return "";
}

static
std::string
check_user(const std::string& user, const atf::tests::vars_map& config)
{
    if (user == "root") {
        if (!impl::is_root())
            return "Requires root privileges";
        else
            return "";
    } else if (user == "unprivileged") {
        if (impl::is_root()) {
            const atf::tests::vars_map::const_iterator iter = config.find(
                "unprivileged-user");
            if (iter == config.end())
                return "Requires an unprivileged user and the "
                    "'unprivileged-user' configuration variable is not set";
            else {
                const std::string& unprivileged_user = (*iter).second;
                try {
                    (void)impl::get_user_ids(unprivileged_user);
                    return "";
                } catch (const std::runtime_error& e) {
                    return "Failed to get information for user " +
                        unprivileged_user;
                }
            }
        } else
            return "";
    } else
        throw std::runtime_error("Invalid value '" + user + "' for property "
                                 "require.user");
}

} // anonymous namespace

std::string
impl::check_requirements(const atf::tests::vars_map& metadata,
                         const atf::tests::vars_map& config)
{
    std::string failure_reason = "";

    for (atf::tests::vars_map::const_iterator iter = metadata.begin();
         failure_reason.empty() && iter != metadata.end(); iter++) {
        const std::string& name = (*iter).first;
        const std::string& value = (*iter).second;
        INV(!value.empty()); // Enforced by application/X-atf-tp parser.

        if (name == "require.arch")
            failure_reason = check_arch(value);
        else if (name == "require.config")
            failure_reason = check_config(value, config);
        else if (name == "require.files")
            failure_reason = check_files(value);
        else if (name == "require.machine")
            failure_reason = check_machine(value);
        else if (name == "require.memory")
            failure_reason = check_memory(value);
        else if (name == "require.progs")
            failure_reason = check_progs(value);
        else if (name == "require.user")
            failure_reason = check_user(value, config);
        else {
            // Unknown require.* properties are forbidden by the
            // application/X-atf-tp parser.
            INV(failure_reason.find("require.") != 0);
        }
    }

    return failure_reason;
}

std::pair< int, int >
impl::get_required_user(const atf::tests::vars_map& metadata,
                        const atf::tests::vars_map& config)
{
    const atf::tests::vars_map::const_iterator user = metadata.find(
        "require.user");
    if (user == metadata.end())
        return std::make_pair(-1, -1);

    if ((*user).second == "unprivileged") {
        if (impl::is_root()) {
            const atf::tests::vars_map::const_iterator iter = config.find(
                "unprivileged-user");
            try {
                return impl::get_user_ids((*iter).second);
            } catch (const std::exception& e) {
                UNREACHABLE;  // This has been validated by check_user.
                throw e;
            }
        } else {
            return std::make_pair(-1, -1);
        }
    } else
        return std::make_pair(-1, -1);
}
