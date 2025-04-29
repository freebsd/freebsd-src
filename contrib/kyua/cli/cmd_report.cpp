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

#include "cli/cmd_report.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <map>
#include <ostream>
#include <string>
#include <vector>

#include "cli/common.ipp"
#include "drivers/scan_results.hpp"
#include "model/context.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "model/types.hpp"
#include "store/layout.hpp"
#include "store/read_transaction.hpp"
#include "utils/cmdline/exceptions.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui.hpp"
#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"
#include "utils/stream.hpp"
#include "utils/text/operations.ipp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace layout = store::layout;
namespace text = utils::text;

using cli::cmd_report;
using utils::optional;


namespace {


/// Generates a plain-text report intended to be printed to the console.
class report_console_hooks : public drivers::scan_results::base_hooks {
    /// Stream to which to write the report.
    std::ostream& _output;

    /// Whether to include details in the report or not.
    const bool _verbose;

    /// Collection of result types to include in the report.
    const cli::result_types& _results_filters;

    /// Path to the results file being read.
    const fs::path& _results_file;

    /// The start time of the first test.
    optional< utils::datetime::timestamp > _start_time;

    /// The end time of the last test.
    optional< utils::datetime::timestamp > _end_time;

    /// The total run time of the tests.  Note that we cannot subtract _end_time
    /// from _start_time to compute this due to parallel execution.
    utils::datetime::delta _runtime;

    /// Representation of a single result.
    struct result_data {
        /// The relative path to the test program.
        utils::fs::path binary_path;

        /// The name of the test case.
        std::string test_case_name;

        /// The result of the test case.
        model::test_result result;

        /// The duration of the test case execution.
        utils::datetime::delta duration;

        /// Constructs a new results data.
        ///
        /// \param binary_path_ The relative path to the test program.
        /// \param test_case_name_ The name of the test case.
        /// \param result_ The result of the test case.
        /// \param duration_ The duration of the test case execution.
        result_data(const utils::fs::path& binary_path_,
                    const std::string& test_case_name_,
                    const model::test_result& result_,
                    const utils::datetime::delta& duration_) :
            binary_path(binary_path_), test_case_name(test_case_name_),
            result(result_), duration(duration_)
        {
        }
    };

    /// Results received, broken down by their type.
    ///
    /// Note that this may not include all results, as keeping the whole list in
    /// memory may be too much.
    std::map< model::test_result_type, std::vector< result_data > > _results;

    /// Pretty-prints the value of an environment variable.
    ///
    /// \param indent Prefix for the lines to print.  Continuation lines
    ///     use this indentation twice.
    /// \param name Name of the variable.
    /// \param value Value of the variable.  Can have newlines.
    void
    print_env_var(const char* indent, const std::string& name,
                  const std::string& value)
    {
        const std::vector< std::string > lines = text::split(value, '\n');
        if (lines.size() == 0) {
            _output << F("%s%s=\n") % indent % name;;
        } else {
            _output << F("%s%s=%s\n") % indent % name % lines[0];
            for (std::vector< std::string >::size_type i = 1;
                 i < lines.size(); ++i) {
                _output << F("%s%s%s\n") % indent % indent % lines[i];
            }
        }
    }

    /// Prints the execution context to the output.
    ///
    /// \param context The context to dump.
    void
    print_context(const model::context& context)
    {
        _output << "===> Execution context\n";

        _output << F("Current directory: %s\n") % context.cwd();
        const std::map< std::string, std::string >& env = context.env();
        if (env.empty())
            _output << "No environment variables recorded\n";
        else {
            _output << "Environment variables:\n";
            for (std::map< std::string, std::string >::const_iterator
                     iter = env.begin(); iter != env.end(); iter++) {
                print_env_var("    ", (*iter).first, (*iter).second);
            }
        }
    }

    /// Dumps a detailed view of the test case.
    ///
    /// \param result_iter Results iterator pointing at the test case to be
    ///     dumped.
    void
    print_test_case_and_result(const store::results_iterator& result_iter)
    {
        const model::test_case& test_case =
            result_iter.test_program()->find(result_iter.test_case_name());
        const model::properties_map props =
            test_case.get_metadata().to_properties();

        _output << F("===> %s:%s\n") %
            result_iter.test_program()->relative_path() %
            result_iter.test_case_name();
        _output << F("Result:     %s\n") %
            cli::format_result(result_iter.result());
        _output << F("Start time: %s\n") %
            result_iter.start_time().to_iso8601_in_utc();
        _output << F("End time:   %s\n") %
            result_iter.end_time().to_iso8601_in_utc();
        _output << F("Duration:   %s\n") %
            cli::format_delta(result_iter.end_time() -
                              result_iter.start_time());

        _output << "\n";
        _output << "Metadata:\n";
        for (model::properties_map::const_iterator iter = props.begin();
             iter != props.end(); ++iter) {
            if ((*iter).second.empty()) {
                _output << F("    %s is empty\n") % (*iter).first;
            } else {
                _output << F("    %s = %s\n") % (*iter).first % (*iter).second;
            }
        }

        const std::string stdout_contents = result_iter.stdout_contents();
        if (!stdout_contents.empty()) {
            _output << "\n"
                    << "Standard output:\n"
                    << stdout_contents;
        }

        const std::string stderr_contents = result_iter.stderr_contents();
        if (!stderr_contents.empty()) {
            _output << "\n"
                    << "Standard error:\n"
                    << stderr_contents;
        }
    }

    /// Counts how many results of a given type have been received.
    ///
    /// \param type Test result type to count results for.
    ///
    /// \return The number of test results with \p type.
    std::size_t
    count_results(const model::test_result_type type)
    {
        const std::map< model::test_result_type,
                        std::vector< result_data > >::const_iterator iter =
            _results.find(type);
        if (iter == _results.end())
            return 0;
        else
            return (*iter).second.size();
    }

    /// Prints a set of results.
    ///
    /// \param type Test result type to print results for.
    /// \param title Title used when printing results.
    void
    print_results(const model::test_result_type type,
                  const char* title)
    {
        const std::map< model::test_result_type,
                        std::vector< result_data > >::const_iterator iter2 =
            _results.find(type);
        if (iter2 == _results.end())
            return;
        const std::vector< result_data >& all = (*iter2).second;

        _output << F("===> %s\n") % title;
        for (std::vector< result_data >::const_iterator iter = all.begin();
             iter != all.end(); iter++) {
            _output << F("%s:%s  ->  %s  [%s]\n") % (*iter).binary_path %
                (*iter).test_case_name %
                cli::format_result((*iter).result) %
                cli::format_delta((*iter).duration);
        }
    }

public:
    /// Constructor for the hooks.
    ///
    /// \param [out] output_ Stream to which to write the report.
    /// \param verbose_ Whether to include details in the output or not.
    /// \param results_filters_ The result types to include in the report.
    ///     Cannot be empty.
    /// \param results_file_ Path to the results file being read.
    report_console_hooks(std::ostream& output_, const bool verbose_,
                         const cli::result_types& results_filters_,
                         const fs::path& results_file_) :
        _output(output_),
        _verbose(verbose_),
        _results_filters(results_filters_),
        _results_file(results_file_)
    {
        PRE(!results_filters_.empty());
    }

    /// Callback executed when the context is loaded.
    ///
    /// \param context The context loaded from the database.
    void
    got_context(const model::context& context)
    {
        if (_verbose)
            print_context(context);
    }

    /// Callback executed when a test results is found.
    ///
    /// \param iter Container for the test result's data.
    void
    got_result(store::results_iterator& iter)
    {
        if (!_start_time || _start_time.get() > iter.start_time())
            _start_time = iter.start_time();
        if (!_end_time || _end_time.get() < iter.end_time())
            _end_time = iter.end_time();

        const datetime::delta duration = iter.end_time() - iter.start_time();

        _runtime += duration;
        const model::test_result result = iter.result();
        _results[result.type()].push_back(
            result_data(iter.test_program()->relative_path(),
                        iter.test_case_name(), iter.result(), duration));

        if (_verbose) {
            // TODO(jmmv): _results_filters is a list and is small enough for
            // std::find to not be an expensive operation here (probably).  But
            // we should be using a std::set instead.
            if (std::find(_results_filters.begin(), _results_filters.end(),
                          iter.result().type()) != _results_filters.end()) {
                print_test_case_and_result(iter);
            }
        }
    }

    /// Prints the tests summary.
    void
    end(const drivers::scan_results::result& /* r */)
    {
        typedef std::map< model::test_result_type, const char* > types_map;

        types_map titles;
        titles[model::test_result_broken] = "Broken tests";
        titles[model::test_result_expected_failure] = "Expected failures";
        titles[model::test_result_failed] = "Failed tests";
        titles[model::test_result_passed] = "Passed tests";
        titles[model::test_result_skipped] = "Skipped tests";

        for (cli::result_types::const_iterator iter = _results_filters.begin();
             iter != _results_filters.end(); ++iter) {
            const types_map::const_iterator match = titles.find(*iter);
            INV_MSG(match != titles.end(), "Conditional does not match user "
                    "input validation in parse_types()");
            print_results((*match).first, (*match).second);
        }

        const std::size_t broken = count_results(model::test_result_broken);
        const std::size_t failed = count_results(model::test_result_failed);
        const std::size_t passed = count_results(model::test_result_passed);
        const std::size_t skipped = count_results(model::test_result_skipped);
        const std::size_t xfail = count_results(
            model::test_result_expected_failure);
        const std::size_t total = broken + failed + passed + skipped + xfail;

        _output << "===> Summary\n";
        _output << F("Results read from %s\n") % _results_file;
        _output << F("Test cases: %s total, %s skipped, %s expected failures, "
                     "%s broken, %s failed\n") %
            total % skipped % xfail % broken % failed;
        if (_verbose && _start_time) {
            INV(_end_time);
            _output << F("Start time: %s\n") %
                    _start_time.get().to_iso8601_in_utc();
            _output << F("End time:   %s\n") %
                    _end_time.get().to_iso8601_in_utc();
        }
        _output << F("Total time: %s\n") % cli::format_delta(_runtime);
    }
};


}  // anonymous namespace


/// Default constructor for cmd_report.
cmd_report::cmd_report(void) : cli_command(
    "report", "", 0, -1,
    "Generates a report with the results of a test suite run")
{
    add_option(results_file_open_option);
    add_option(cmdline::bool_option(
        "verbose", "Include the execution context and the details of each test "
        "case in the report"));
    add_option(cmdline::path_option("output", "Path to the output file", "path",
                                    "/dev/stdout"));
    add_option(results_filter_option);
}


/// Entry point for the "report" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
///
/// \return 0 if everything is OK, 1 if the statement is invalid or if there is
/// any other problem.
int
cmd_report::run(cmdline::ui* ui,
                const cmdline::parsed_cmdline& cmdline,
                const config::tree& /* user_config */)
{
    std::unique_ptr< std::ostream > output = utils::open_ostream(
        cmdline.get_option< cmdline::path_option >("output"));

    const fs::path results_file = layout::find_results(
        results_file_open(cmdline));

    const result_types types = get_result_types(cmdline);
    report_console_hooks hooks(*output.get(), cmdline.has_option("verbose"),
                               types, results_file);
    const drivers::scan_results::result result = drivers::scan_results::drive(
        results_file, parse_filters(cmdline.arguments()), hooks);

    return report_unused_filters(result.unused_filters, ui) ?
        EXIT_FAILURE : EXIT_SUCCESS;
}
