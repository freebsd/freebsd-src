// Copyright 2012 The Kyua Authors.
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

#include "engine/requirements.hpp"

#include "engine/execenv/execenv.hpp"
#include "model/metadata.hpp"
#include "model/types.hpp"
#include "utils/config/nodes.ipp"
#include "utils/config/tree.ipp"
#include "utils/format/macros.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/memory.hpp"
#include "utils/passwd.hpp"
#include "utils/sanity.hpp"
#include "utils/units.hpp"

namespace config = utils::config;
namespace fs = utils::fs;
namespace passwd = utils::passwd;
namespace units = utils::units;


namespace {


/// Checks if all required configuration variables are present.
///
/// \param required_configs Set of required variable names.
/// \param user_config Runtime user configuration.
/// \param test_suite_name Name of the test suite the test belongs to.
///
/// \return Empty if all variables are present or an error message otherwise.
static std::string
check_required_configs(const model::strings_set& required_configs,
                       const config::tree& user_config,
                       const std::string& test_suite_name)
{
    for (model::strings_set::const_iterator iter = required_configs.begin();
         iter != required_configs.end(); iter++) {
        std::string property;
        // TODO(jmmv): All this rewrite logic belongs in the ATF interface.
        if ((*iter) == "unprivileged-user" || (*iter) == "unprivileged_user")
            property = "unprivileged_user";
        else
            property = F("test_suites.%s.%s") % test_suite_name % (*iter);

        if (!user_config.is_set(property))
            return F("Required configuration property '%s' not defined") %
                (*iter);
    }
    return "";
}


/// Checks if the allowed architectures match the current architecture.
///
/// \param allowed_architectures Set of allowed architectures.
/// \param user_config Runtime user configuration.
///
/// \return Empty if the current architecture is in the list or an error
/// message otherwise.
static std::string
check_allowed_architectures(const model::strings_set& allowed_architectures,
                            const config::tree& user_config)
{
    if (!allowed_architectures.empty()) {
        const std::string architecture =
            user_config.lookup< config::string_node >("architecture");
        if (allowed_architectures.find(architecture) ==
            allowed_architectures.end())
            return F("Current architecture '%s' not supported") % architecture;
    }
    return "";
}


/// Checks if test's execenv matches the user configuration.
///
/// \param execenv Execution environment name a test is designed for.
/// \param user_config Runtime user configuration.
///
/// \return Empty if the execenv is in the list or an error message otherwise.
static std::string
check_execenv(const std::string& execenv, const config::tree& user_config)
{
    std::string name = execenv;
    if (name.empty())
        name = engine::execenv::default_execenv_name; // if test claims nothing

    std::set< std::string > execenvs;
    try {
        execenvs = user_config.lookup< config::strings_set_node >("execenvs");
    } catch (const config::unknown_key_error&) {
        // okay, user config does not define it, empty set then
    }

    if (execenvs.find(name) == execenvs.end())
        return F("'%s' execenv is not supported or not allowed by "
            "the runtime user configuration") % name;

    return "";
}


/// Checks if the allowed platforms match the current architecture.
///
/// \param allowed_platforms Set of allowed platforms.
/// \param user_config Runtime user configuration.
///
/// \return Empty if the current platform is in the list or an error message
/// otherwise.
static std::string
check_allowed_platforms(const model::strings_set& allowed_platforms,
                        const config::tree& user_config)
{
    if (!allowed_platforms.empty()) {
        const std::string platform =
            user_config.lookup< config::string_node >("platform");
        if (allowed_platforms.find(platform) == allowed_platforms.end())
            return F("Current platform '%s' not supported") % platform;
    }
    return "";
}


/// Checks if the current user matches the required user.
///
/// \param required_user Name of the required user category.
/// \param user_config Runtime user configuration.
///
/// \return Empty if the current user fits the required user characteristics or
/// an error message otherwise.
static std::string
check_required_user(const std::string& required_user,
                    const config::tree& user_config)
{
    if (!required_user.empty()) {
        const passwd::user user = passwd::current_user();
        if (required_user == "root") {
            if (!user.is_root())
                return "Requires root privileges";
        } else if (required_user == "unprivileged") {
            if (user.is_root())
                if (!user_config.is_set("unprivileged_user"))
                    return "Requires an unprivileged user but the "
                        "unprivileged-user configuration variable is not "
                        "defined";
        } else
            UNREACHABLE_MSG("Value of require.user not properly validated");
    }
    return "";
}


/// Checks if all required files exist.
///
/// \param required_files Set of paths.
///
/// \return Empty if the required files all exist or an error message otherwise.
static std::string
check_required_files(const model::paths_set& required_files)
{
    for (model::paths_set::const_iterator iter = required_files.begin();
         iter != required_files.end(); iter++) {
        INV((*iter).is_absolute());
        if (!fs::exists(*iter))
            return F("Required file '%s' not found") % *iter;
    }
    return "";
}


/// Checks if all required programs exist.
///
/// \param required_programs Set of paths.
///
/// \return Empty if the required programs all exist or an error message
/// otherwise.
static std::string
check_required_programs(const model::paths_set& required_programs)
{
    for (model::paths_set::const_iterator iter = required_programs.begin();
         iter != required_programs.end(); iter++) {
        if ((*iter).is_absolute()) {
            if (!fs::exists(*iter))
                return F("Required program '%s' not found") % *iter;
        } else {
            if (!fs::find_in_path((*iter).c_str()))
                return F("Required program '%s' not found in PATH") % *iter;
        }
    }
    return "";
}


/// Checks if the current system has the specified amount of memory.
///
/// \param required_memory Amount of required physical memory, or zero if not
///     applicable.
///
/// \return Empty if the current system has the required amount of memory or an
/// error message otherwise.
static std::string
check_required_memory(const units::bytes& required_memory)
{
    if (required_memory > 0) {
        const units::bytes physical_memory = utils::physical_memory();
        if (physical_memory > 0 && physical_memory < required_memory)
            return F("Requires %s bytes of physical memory but only %s "
                     "available") %
                required_memory.format() % physical_memory.format();
    }
    return "";
}


/// Checks if the work directory's file system has enough free disk space.
///
/// \param required_disk_space Amount of required free disk space, or zero if
///     not applicable.
/// \param work_directory Path to where the test case will be run.
///
/// \return Empty if the file system where the work directory is hosted has
/// enough free disk space or an error message otherwise.
static std::string
check_required_disk_space(const units::bytes& required_disk_space,
                          const fs::path& work_directory)
{
    if (required_disk_space > 0) {
        const units::bytes free_disk_space = fs::free_disk_space(
            work_directory);
        if (free_disk_space < required_disk_space)
            return F("Requires %s bytes of free disk space but only %s "
                     "available") %
                required_disk_space.format() % free_disk_space.format();
    }
    return "";
}


/// List of registered extra requirement checkers.
///
/// Use register_reqs_checker() to add an entry to this global list.
static std::vector< std::shared_ptr< engine::reqs_checker > > _reqs_checkers;


}  // anonymous namespace


const std::vector< std::shared_ptr< engine::reqs_checker > >
engine::reqs_checkers()
{
    return _reqs_checkers;
}

void
engine::register_reqs_checker(
    const std::shared_ptr< engine::reqs_checker > checker)
{
    _reqs_checkers.push_back(checker);
}


/// Checks if all the requirements specified by the test case are met.
///
/// \param md The test metadata.
/// \param cfg The engine configuration.
/// \param test_suite Name of the test suite the test belongs to.
/// \param work_directory Path to where the test case will be run.
///
/// \return A string describing the reason for skipping the test, or empty if
/// the test should be executed.
std::string
engine::check_reqs(const model::metadata& md, const config::tree& cfg,
                   const std::string& test_suite,
                   const fs::path& work_directory)
{
    std::string reason;

    reason = check_required_configs(md.required_configs(), cfg, test_suite);
    if (!reason.empty())
        return reason;

    reason = check_allowed_architectures(md.allowed_architectures(), cfg);
    if (!reason.empty())
        return reason;

    reason = check_execenv(md.execenv(), cfg);
    if (!reason.empty())
        return reason;

    reason = check_allowed_platforms(md.allowed_platforms(), cfg);
    if (!reason.empty())
        return reason;

    reason = check_required_user(md.required_user(), cfg);
    if (!reason.empty())
        return reason;

    reason = check_required_files(md.required_files());
    if (!reason.empty())
        return reason;

    reason = check_required_programs(md.required_programs());
    if (!reason.empty())
        return reason;

    reason = check_required_memory(md.required_memory());
    if (!reason.empty())
        return reason;

    reason = check_required_disk_space(md.required_disk_space(),
                                       work_directory);
    if (!reason.empty())
        return reason;

    // Iterate over extra checkers registered.
    for (auto& checker : engine::reqs_checkers()) {
        reason = checker->exec(md, cfg, test_suite, work_directory);
        if (!reason.empty())
            return reason;
    }

    INV(reason.empty());
    return reason;
}
