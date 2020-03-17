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

#include "cli/common.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "engine/filters.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "store/layout.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui.hpp"
#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/logging/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"

#if defined(HAVE_CONFIG_H)
#   include "config.h"
#endif

namespace cmdline = utils::cmdline;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace layout = store::layout;

using utils::none;
using utils::optional;


/// Standard definition of the option to specify the build root.
const cmdline::path_option cli::build_root_option(
    "build-root",
    "Path to the built test programs, if different from the location of the "
    "Kyuafile scripts",
    "path");


/// Standard definition of the option to specify a Kyuafile.
const cmdline::path_option cli::kyuafile_option(
    'k', "kyuafile",
    "Path to the test suite definition",
    "file", "Kyuafile");


/// Standard definition of the option to specify filters on test results.
const cmdline::list_option cli::results_filter_option(
    "results-filter", "Comma-separated list of result types to include in "
    "the report", "types", "skipped,xfail,broken,failed");


/// Standard definition of the option to specify the results file.
///
/// TODO(jmmv): Should support a git-like syntax to go back in time, like
/// --results-file=LATEST^N where N indicates how many runs to go back to.
const cmdline::string_option cli::results_file_create_option(
    'r', "results-file",
    "Path to the results file to create; if left to the default value, the "
    "name of the file is automatically computed for the current test suite",
    "file", layout::results_auto_create_name);


/// Standard definition of the option to specify the results file.
///
/// TODO(jmmv): Should support a git-like syntax to go back in time, like
/// --results-file=LATEST^N where N indicates how many runs to go back to.
const cmdline::string_option cli::results_file_open_option(
    'r', "results-file",
    "Path to the results file to open or the identifier of the current test "
    "suite or a previous results file for automatic lookup; if left to the "
    "default value, uses the current directory as the test suite name",
    "file", layout::results_auto_open_name);


namespace {


/// Gets the path to the historical database if it exists.
///
/// TODO(jmmv): This function should go away.  It only exists as a temporary
/// transitional path to force the use of the stale ~/.kyua/store.db if it
/// exists.
///
/// \return A path if the file is found; none otherwise.
static optional< fs::path >
get_historical_db(void)
{
    optional< fs::path > home = utils::get_home();
    if (home) {
        const fs::path old_db = home.get() / ".kyua/store.db";
        if (fs::exists(old_db)) {
            if (old_db.is_absolute())
                return utils::make_optional(old_db);
            else
                return utils::make_optional(old_db.to_absolute());
        } else {
            return none;
        }
    } else {
        return none;
    }
}


/// Converts a set of result type names to identifiers.
///
/// \param names The collection of names to process; may be empty.
///
/// \return The result type identifiers corresponding to the input names.
///
/// \throw std::runtime_error If any name in the input names is invalid.
static cli::result_types
parse_types(const std::vector< std::string >& names)
{
    typedef std::map< std::string, model::test_result_type > types_map;
    types_map valid_types;
    valid_types["broken"] = model::test_result_broken;
    valid_types["failed"] = model::test_result_failed;
    valid_types["passed"] = model::test_result_passed;
    valid_types["skipped"] = model::test_result_skipped;
    valid_types["xfail"] = model::test_result_expected_failure;

    cli::result_types types;
    for (std::vector< std::string >::const_iterator iter = names.begin();
         iter != names.end(); ++iter) {
        const types_map::const_iterator match = valid_types.find(*iter);
        if (match == valid_types.end())
            throw std::runtime_error(F("Unknown result type '%s'") % *iter);
        else
            types.push_back((*match).second);
    }
    return types;
}


}  // anonymous namespace


/// Gets the path to the build root, if any.
///
/// This is just syntactic sugar to simplify quierying the 'build_root_option'.
///
/// \param cmdline The parsed command line.
///
/// \return The path to the build root, if specified; none otherwise.
optional< fs::path >
cli::build_root_path(const cmdline::parsed_cmdline& cmdline)
{
    optional< fs::path > build_root;
    if (cmdline.has_option(build_root_option.long_name()))
        build_root = cmdline.get_option< cmdline::path_option >(
            build_root_option.long_name());
    return build_root;
}


/// Gets the path to the Kyuafile to be loaded.
///
/// This is just syntactic sugar to simplify quierying the 'kyuafile_option'.
///
/// \param cmdline The parsed command line.
///
/// \return The path to the Kyuafile to be loaded.
fs::path
cli::kyuafile_path(const cmdline::parsed_cmdline& cmdline)
{
    return cmdline.get_option< cmdline::path_option >(
        kyuafile_option.long_name());
}


/// Gets the value of the results-file flag for the creation of a new file.
///
/// \param cmdline The parsed command line from which to extract any possible
///     override for the location of the database via the --results-file flag.
///
/// \return The path to the database to be used.
///
/// \throw cmdline::error If the value passed to the flag is invalid.
std::string
cli::results_file_create(const cmdline::parsed_cmdline& cmdline)
{
    std::string results_file = cmdline.get_option< cmdline::string_option >(
        results_file_create_option.long_name());
    if (results_file == results_file_create_option.default_value()) {
        const optional< fs::path > historical_db = get_historical_db();
        if (historical_db)
            results_file = historical_db.get().str();
    } else {
        try {
            (void)fs::path(results_file);
        } catch (const fs::error& e) {
            throw cmdline::usage_error(F("Invalid value passed to --%s") %
                                       results_file_create_option.long_name());
        }
    }
    return results_file;
}


/// Gets the value of the results-file flag for the lookup of the file.
///
/// \param cmdline The parsed command line from which to extract any possible
///     override for the location of the database via the --results-file flag.
///
/// \return The path to the database to be used.
///
/// \throw cmdline::error If the value passed to the flag is invalid.
std::string
cli::results_file_open(const cmdline::parsed_cmdline& cmdline)
{
    std::string results_file = cmdline.get_option< cmdline::string_option >(
        results_file_open_option.long_name());
    if (results_file == results_file_open_option.default_value()) {
        const optional< fs::path > historical_db = get_historical_db();
        if (historical_db)
            results_file = historical_db.get().str();
    } else {
        try {
            (void)fs::path(results_file);
        } catch (const fs::error& e) {
            throw cmdline::usage_error(F("Invalid value passed to --%s") %
                                       results_file_open_option.long_name());
        }
    }
    return results_file;
}


/// Gets the filters for the result types.
///
/// \param cmdline The parsed command line.
///
/// \return A collection of result types to be used for filtering.
///
/// \throw std::runtime_error If any of the user-provided filters is invalid.
cli::result_types
cli::get_result_types(const utils::cmdline::parsed_cmdline& cmdline)
{
    result_types types = parse_types(
        cmdline.get_option< cmdline::list_option >("results-filter"));
    if (types.empty()) {
        types.push_back(model::test_result_passed);
        types.push_back(model::test_result_skipped);
        types.push_back(model::test_result_expected_failure);
        types.push_back(model::test_result_broken);
        types.push_back(model::test_result_failed);
    }
    return types;
}


/// Parses a set of command-line arguments to construct test filters.
///
/// \param args The command-line arguments representing test filters.
///
/// \return A set of test filters.
///
/// \throw cmdline:error If any of the arguments is invalid, or if they
///     represent a non-disjoint collection of filters.
std::set< engine::test_filter >
cli::parse_filters(const cmdline::args_vector& args)
{
    std::set< engine::test_filter > filters;

    try {
        for (cmdline::args_vector::const_iterator iter = args.begin();
             iter != args.end(); iter++) {
            const engine::test_filter filter(engine::test_filter::parse(*iter));
            if (filters.find(filter) != filters.end())
                throw cmdline::error(F("Duplicate filter '%s'") % filter.str());
            filters.insert(filter);
        }
        check_disjoint_filters(filters);
    } catch (const std::runtime_error& e) {
        throw cmdline::error(e.what());
    }

    return filters;
}


/// Reports the filters that have not matched any tests as errors.
///
/// \param unused The collection of unused filters to report.
/// \param ui The user interface object through which errors are to be reported.
///
/// \return True if there are any unused filters.  The caller should report this
/// as an error to the user by means of a non-successful exit code.
bool
cli::report_unused_filters(const std::set< engine::test_filter >& unused,
                           cmdline::ui* ui)
{
    for (std::set< engine::test_filter >::const_iterator iter = unused.begin();
         iter != unused.end(); iter++) {
        cmdline::print_warning(ui, F("No test cases matched by the filter "
                                     "'%s'.") % (*iter).str());
    }

    return !unused.empty();
}


/// Formats a time delta for user presentation.
///
/// \param delta The time delta to format.
///
/// \return A user-friendly representation of the time delta.
std::string
cli::format_delta(const datetime::delta& delta)
{
    return F("%.3ss") % (delta.seconds + (delta.useconds / 1000000.0));
}


/// Formats a test case result for user presentation.
///
/// \param result The result to format.
///
/// \return A user-friendly representation of the result.
std::string
cli::format_result(const model::test_result& result)
{
    std::string text;

    switch (result.type()) {
    case model::test_result_broken: text = "broken"; break;
    case model::test_result_expected_failure: text = "expected_failure"; break;
    case model::test_result_failed: text = "failed"; break;
    case model::test_result_passed: text = "passed"; break;
    case model::test_result_skipped: text = "skipped"; break;
    }
    INV(!text.empty());

    if (!result.reason().empty())
        text += ": " + result.reason();

    return text;
}


/// Formats the identifier of a test case for user presentation.
///
/// \param test_program The test program containing the test case.
/// \param test_case_name The name of the test case.
///
/// \return A string representing the test case uniquely within a test suite.
std::string
cli::format_test_case_id(const model::test_program& test_program,
                         const std::string& test_case_name)
{
    return F("%s:%s") % test_program.relative_path() % test_case_name;
}


/// Formats a filter using the same syntax of a test case.
///
/// \param test_filter The filter to format.
///
/// \return A string representing the test filter.
std::string
cli::format_test_case_id(const engine::test_filter& test_filter)
{
    return F("%s:%s") % test_filter.test_program % test_filter.test_case;
}


/// Prints the version header information to the interface output.
///
/// \param ui Interface to which to write the version details.
void
cli::write_version_header(utils::cmdline::ui* ui)
{
    ui->out(PACKAGE " (" PACKAGE_NAME ") " PACKAGE_VERSION);
}
