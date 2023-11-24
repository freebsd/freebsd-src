// Copyright 2011 The Kyua Authors.
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

#include "cli/config.hpp"

#include "cli/common.hpp"
#include "engine/config.hpp"
#include "engine/exceptions.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/config/tree.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/env.hpp"
#include "utils/logging/macros.hpp"
#include "utils/optional.ipp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace fs = utils::fs;

using utils::optional;


namespace {


/// Basename of the configuration file.
static const char* config_basename = "kyua.conf";


/// Magic string to disable loading of configuration files.
static const char* none_config = "none";


/// Textual description of the default configuration files.
///
/// This is just an auxiliary string required to define the option below, which
/// requires a pointer to a static C string.
///
/// \todo If the user overrides the KYUA_CONFDIR environment variable, we don't
/// reflect this fact here.  We don't want to query the variable during program
/// initialization due to the side-effects it may have.  Therefore, fixing this
/// is tricky as it may require a whole rethink of this module.
static const std::string config_lookup_names =
    (fs::path("~/.kyua") / config_basename).str() + " or " +
    (fs::path(KYUA_CONFDIR) / config_basename).str();


/// Loads the configuration file for this session, if any.
///
/// This is a helper function that does not apply user-specified overrides.  See
/// the documentation for cli::load_config() for more details.
///
/// \param cmdline The parsed command line.
///
/// \return The loaded configuration file, or the configuration defaults if the
/// loading is disabled.
///
/// \throw engine::error If the parsing of the configuration file fails.
///     TODO(jmmv): I'm not sure if this is the raised exception.  And even if
///     it is, we should make it more accurate.
config::tree
load_config_file(const cmdline::parsed_cmdline& cmdline)
{
    // TODO(jmmv): We should really be able to use cmdline.has_option here to
    // detect whether the option was provided or not instead of checking against
    // the default value.
    const fs::path filename = cmdline.get_option< cmdline::path_option >(
        cli::config_option.long_name());
    if (filename.str() == none_config) {
        LD("Configuration loading disabled; using defaults");
        return engine::default_config();
    } else if (filename.str() != cli::config_option.default_value())
        return engine::load_config(filename);

    const optional< fs::path > home = utils::get_home();
    if (home) {
        const fs::path path = home.get() / ".kyua" / config_basename;
        try {
            if (fs::exists(path))
                return engine::load_config(path);
        } catch (const fs::error& e) {
            // Fall through.  If we fail to load the user-specific configuration
            // file because it cannot be openend, we try to load the system-wide
            // one.
            LW(F("Failed to load user-specific configuration file '%s': %s") %
               path % e.what());
        }
    }

    const fs::path confdir(utils::getenv_with_default(
        "KYUA_CONFDIR", KYUA_CONFDIR));

    const fs::path path = confdir / config_basename;
    if (fs::exists(path)) {
        return engine::load_config(path);
    } else {
        return engine::default_config();
    }
}


/// Loads the configuration file for this session, if any.
///
/// This is a helper function for cli::load_config() that attempts to load the
/// configuration unconditionally.
///
/// \param cmdline The parsed command line.
///
/// \return The loaded configuration file data.
///
/// \throw engine::error If the parsing of the configuration file fails.
static config::tree
load_required_config(const cmdline::parsed_cmdline& cmdline)
{
    config::tree user_config = load_config_file(cmdline);

    if (cmdline.has_option(cli::variable_option.long_name())) {
        typedef std::pair< std::string, std::string > override_pair;

        const std::vector< override_pair >& overrides =
            cmdline.get_multi_option< cmdline::property_option >(
                cli::variable_option.long_name());

        for (std::vector< override_pair >::const_iterator
                 iter = overrides.begin(); iter != overrides.end(); iter++) {
            try {
                user_config.set_string((*iter).first, (*iter).second);
            } catch (const config::error& e) {
                // TODO(jmmv): Raising this type from here is obviously the
                // wrong thing to do.
                throw engine::error(e.what());
            }
        }
    }

    return user_config;
}


}  // anonymous namespace


/// Standard definition of the option to specify a configuration file.
///
/// You must use load_config() to load a configuration file while honoring the
/// value of this flag.
const cmdline::path_option cli::config_option(
    'c', "config",
    (std::string("Path to the configuration file; '") + none_config +
     "' to disable loading").c_str(),
    "file", config_lookup_names.c_str());


/// Standard definition of the option to specify a configuration variable.
const cmdline::property_option cli::variable_option(
    'v', "variable",
    "Overrides a particular configuration variable",
    "K=V");


/// Loads the configuration file for this session, if any.
///
/// The algorithm implemented here is as follows:
/// 1) If ~/.kyua/kyua.conf exists, load it.
/// 2) Otherwise, if sysconfdir/kyua.conf exists, load it.
/// 3) Otherwise, use the built-in settings.
/// 4) Lastly, apply any user-provided overrides.
///
/// \param cmdline The parsed command line.
/// \param required Whether the loading of the configuration file must succeed.
///     Some commands should run regardless, and therefore we need to set this
///     to false for those commands.
///
/// \return The loaded configuration file data.  If required was set to false,
/// this might be the default configuration data if the requested file could not
/// be properly loaded.
///
/// \throw engine::error If the parsing of the configuration file fails.
config::tree
cli::load_config(const cmdline::parsed_cmdline& cmdline,
                 const bool required)
{
    try {
        return load_required_config(cmdline);
    } catch (const engine::error& e) {
        if (required) {
            throw;
        } else {
            LW(F("Ignoring failure to load configuration because the requested "
                 "command should not fail: %s") % e.what());
            return engine::default_config();
        }
    }
}
