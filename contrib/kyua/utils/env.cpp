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

#include "utils/env.hpp"

#if defined(HAVE_CONFIG_H)
#  include "config.h"
#endif

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/macros.hpp"
#include "utils/optional.ipp"

namespace fs = utils::fs;

using utils::none;
using utils::optional;


extern "C" {
    extern char** environ;
}


/// Gets all environment variables.
///
/// \return A mapping of (name, value) pairs describing the environment
/// variables.
std::map< std::string, std::string >
utils::getallenv(void)
{
    std::map< std::string, std::string > allenv;
    for (char** envp = environ; *envp != NULL; envp++) {
        const std::string oneenv = *envp;
        const std::string::size_type pos = oneenv.find('=');
        const std::string name = oneenv.substr(0, pos);
        const std::string value = oneenv.substr(pos + 1);

        PRE(allenv.find(name) == allenv.end());
        allenv[name] = value;
    }
    return allenv;
}


/// Gets the value of an environment variable.
///
/// \param name The name of the environment variable to query.
///
/// \return The value of the environment variable if it is defined, or none
/// otherwise.
optional< std::string >
utils::getenv(const std::string& name)
{
    const char* value = std::getenv(name.c_str());
    if (value == NULL) {
        LD(F("Environment variable '%s' is not defined") % name);
        return none;
    } else {
        LD(F("Environment variable '%s' is '%s'") % name % value);
        return utils::make_optional(std::string(value));
    }
}


/// Gets the value of an environment variable with a default fallback.
///
/// \param name The name of the environment variable to query.
/// \param default_value The value to return if the variable is not defined.
///
/// \return The value of the environment variable.
std::string
utils::getenv_with_default(const std::string& name,
                           const std::string& default_value)
{
    const char* value = std::getenv(name.c_str());
    if (value == NULL) {
        LD(F("Environment variable '%s' is not defined; using default '%s'") %
           name % default_value);
        return default_value;
    } else {
        LD(F("Environment variable '%s' is '%s'") % name % value);
        return value;
    }
}


/// Gets the value of the HOME environment variable with path validation.
///
/// \return The value of the HOME environment variable if it is a valid path;
///     none if it is not defined or if it contains an invalid path.
optional< fs::path >
utils::get_home(void)
{
    const optional< std::string > home = utils::getenv("HOME");
    if (home) {
        try {
            return utils::make_optional(fs::path(home.get()));
        } catch (const fs::error& e) {
            LW(F("Invalid value '%s' in HOME environment variable: %s") %
               home.get() % e.what());
            return none;
        }
    } else {
        return none;
    }
}


/// Sets the value of an environment variable.
///
/// \param name The name of the environment variable to set.
/// \param val The value to set the environment variable to.  May be empty.
///
/// \throw std::runtime_error If there is an error setting the environment
///     variable.
void
utils::setenv(const std::string& name, const std::string& val)
{
    LD(F("Setting environment variable '%s' to '%s'") % name % val);
#if defined(HAVE_SETENV)
    if (::setenv(name.c_str(), val.c_str(), 1) == -1) {
        const int original_errno = errno;
        throw std::runtime_error(F("Failed to set environment variable '%s' to "
                                   "'%s': %s") %
                                 name % val % std::strerror(original_errno));
    }
#elif defined(HAVE_PUTENV)
    if (::putenv((F("%s=%s") % name % val).c_str()) == -1) {
        const int original_errno = errno;
        throw std::runtime_error(F("Failed to set environment variable '%s' to "
                                   "'%s': %s") %
                                 name % val % std::strerror(original_errno));
    }
#else
#   error "Don't know how to set an environment variable."
#endif
}


/// Unsets an environment variable.
///
/// \param name The name of the environment variable to unset.
///
/// \throw std::runtime_error If there is an error unsetting the environment
///     variable.
void
utils::unsetenv(const std::string& name)
{
    LD(F("Unsetting environment variable '%s'") % name);
#if defined(HAVE_UNSETENV)
    if (::unsetenv(name.c_str()) == -1) {
        const int original_errno = errno;
        throw std::runtime_error(F("Failed to unset environment variable "
                                   "'%s'") %
                                 name % std::strerror(original_errno));
    }
#elif defined(HAVE_PUTENV)
    if (::putenv((F("%s=") % name).c_str()) == -1) {
        const int original_errno = errno;
        throw std::runtime_error(F("Failed to unset environment variable "
                                   "'%s'") %
                                 name % std::strerror(original_errno));
    }
#else
#   error "Don't know how to unset an environment variable."
#endif
}
