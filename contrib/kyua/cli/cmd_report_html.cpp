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

#include "cli/cmd_report_html.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <set>
#include <stdexcept>

#include "cli/common.ipp"
#include "drivers/scan_results.hpp"
#include "engine/filters.hpp"
#include "model/context.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "store/layout.hpp"
#include "store/read_transaction.hpp"
#include "utils/cmdline/options.hpp"
#include "utils/cmdline/parser.ipp"
#include "utils/cmdline/ui.hpp"
#include "utils/datetime.hpp"
#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/exceptions.hpp"
#include "utils/fs/operations.hpp"
#include "utils/fs/path.hpp"
#include "utils/optional.ipp"
#include "utils/text/operations.hpp"
#include "utils/text/templates.hpp"

namespace cmdline = utils::cmdline;
namespace config = utils::config;
namespace datetime = utils::datetime;
namespace fs = utils::fs;
namespace layout = store::layout;
namespace text = utils::text;

using utils::optional;


namespace {


/// Creates the report's top directory and fails if it exists.
///
/// \param directory The directory to create.
/// \param force Whether to wipe an existing directory or not.
///
/// \throw std::runtime_error If the directory already exists; this is a user
///     error that the user must correct.
/// \throw fs::error If the directory creation fails for any other reason.
static void
create_top_directory(const fs::path& directory, const bool force)
{
    if (force) {
        if (fs::exists(directory))
            fs::rm_r(directory);
    }

    try {
        fs::mkdir(directory, 0755);
    } catch (const fs::system_error& e) {
        if (e.original_errno() == EEXIST)
            throw std::runtime_error(F("Output directory '%s' already exists; "
                                       "maybe use --force?") %
                                     directory);
        else
            throw e;
    }
}


/// Generates a flat unique filename for a given test case.
///
/// \param test_program The test program for which to genereate the name.
/// \param test_case_name The test case name.
///
/// \return A filename unique within a directory with a trailing HTML extension.
static std::string
test_case_filename(const model::test_program& test_program,
                   const std::string& test_case_name)
{
    static const char* special_characters = "/:";

    std::string name = cli::format_test_case_id(test_program, test_case_name);
    std::string::size_type pos = name.find_first_of(special_characters);
    while (pos != std::string::npos) {
        name.replace(pos, 1, "_");
        pos = name.find_first_of(special_characters, pos + 1);
    }
    return name + ".html";
}


/// Adds a string to string map to the templates.
///
/// \param [in,out] templates The templates to add the map to.
/// \param props The map to add to the templates.
/// \param key_vector Name of the template vector that holds the keys.
/// \param value_vector Name of the template vector that holds the values.
static void
add_map(text::templates_def& templates, const config::properties_map& props,
        const std::string& key_vector, const std::string& value_vector)
{
    templates.add_vector(key_vector);
    templates.add_vector(value_vector);

    for (config::properties_map::const_iterator iter = props.begin();
         iter != props.end(); ++iter) {
        templates.add_to_vector(key_vector, (*iter).first);
        templates.add_to_vector(value_vector, (*iter).second);
    }
}


/// Generates an HTML report.
class html_hooks : public drivers::scan_results::base_hooks {
    /// User interface object where to report progress.
    cmdline::ui* _ui;

    /// The top directory in which to create the HTML files.
    fs::path _directory;

    /// Collection of result types to include in the report.
    const cli::result_types& _results_filters;

    /// The start time of the first test.
    optional< utils::datetime::timestamp > _start_time;

    /// The end time of the last test.
    optional< utils::datetime::timestamp > _end_time;

    /// The total run time of the tests.  Note that we cannot subtract _end_time
    /// from _start_time to compute this due to parallel execution.
    utils::datetime::delta _runtime;

    /// Templates accumulator to generate the index.html file.
    text::templates_def _summary_templates;

    /// Mapping of result types to the amount of tests with such result.
    std::map< model::test_result_type, std::size_t > _types_count;

    /// Generates a common set of templates for all of our files.
    ///
    /// \return A new templates object with common parameters.
    static text::templates_def
    common_templates(void)
    {
        text::templates_def templates;
        templates.add_variable("css", "report.css");
        return templates;
    }

    /// Adds a test case result to the summary.
    ///
    /// \param test_program The test program with the test case to be added.
    /// \param test_case_name Name of the test case.
    /// \param result The result of the test case.
    /// \param has_detail If true, the result of the test case has not been
    ///     filtered and therefore there exists a separate file for the test
    ///     with all of its information.
    void
    add_to_summary(const model::test_program& test_program,
                   const std::string& test_case_name,
                   const model::test_result& result,
                   const bool has_detail)
    {
        ++_types_count[result.type()];

        if (!has_detail)
            return;

        std::string test_cases_vector;
        std::string test_cases_file_vector;
        switch (result.type()) {
        case model::test_result_broken:
            test_cases_vector = "broken_test_cases";
            test_cases_file_vector = "broken_test_cases_file";
            break;

        case model::test_result_expected_failure:
            test_cases_vector = "xfail_test_cases";
            test_cases_file_vector = "xfail_test_cases_file";
            break;

        case model::test_result_failed:
            test_cases_vector = "failed_test_cases";
            test_cases_file_vector = "failed_test_cases_file";
            break;

        case model::test_result_passed:
            test_cases_vector = "passed_test_cases";
            test_cases_file_vector = "passed_test_cases_file";
            break;

        case model::test_result_skipped:
            test_cases_vector = "skipped_test_cases";
            test_cases_file_vector = "skipped_test_cases_file";
            break;
        }
        INV(!test_cases_vector.empty());
        INV(!test_cases_file_vector.empty());

        _summary_templates.add_to_vector(
            test_cases_vector,
            cli::format_test_case_id(test_program, test_case_name));
        _summary_templates.add_to_vector(
            test_cases_file_vector,
            test_case_filename(test_program, test_case_name));
    }

    /// Instantiate a template to generate an HTML file in the output directory.
    ///
    /// \param templates The templates to use.
    /// \param template_name The name of the template.  This is automatically
    ///     searched for in the installed directory, so do not provide a path.
    /// \param output_name The name of the output file.  This is a basename to
    ///     be created within the output directory.
    ///
    /// \throw text::error If there is any problem applying the templates.
    void
    generate(const text::templates_def& templates,
             const std::string& template_name,
             const std::string& output_name) const
    {
        const fs::path miscdir(utils::getenv_with_default(
             "KYUA_MISCDIR", KYUA_MISCDIR));
        const fs::path template_file = miscdir / template_name;
        const fs::path output_path(_directory / output_name);

        _ui->out(F("Generating %s") % output_path);
        text::instantiate(templates, template_file, output_path);
    }

    /// Gets the number of tests with a given result type.
    ///
    /// \param type The type to be queried.
    ///
    /// \return The number of tests of the given type, or 0 if none have yet
    /// been registered by add_to_summary().
    std::size_t
    get_count(const model::test_result_type type) const
    {
        const std::map< model::test_result_type, std::size_t >::const_iterator
            iter = _types_count.find(type);
        if (iter == _types_count.end())
            return 0;
        else
            return (*iter).second;
    }

public:
    /// Constructor for the hooks.
    ///
    /// \param ui_ User interface object where to report progress.
    /// \param directory_ The directory in which to create the HTML files.
    /// \param results_filters_ The result types to include in the report.
    ///     Cannot be empty.
    html_hooks(cmdline::ui* ui_, const fs::path& directory_,
               const cli::result_types& results_filters_) :
        _ui(ui_),
        _directory(directory_),
        _results_filters(results_filters_),
        _summary_templates(common_templates())
    {
        PRE(!results_filters_.empty());

        // Keep in sync with add_to_summary().
        _summary_templates.add_vector("broken_test_cases");
        _summary_templates.add_vector("broken_test_cases_file");
        _summary_templates.add_vector("xfail_test_cases");
        _summary_templates.add_vector("xfail_test_cases_file");
        _summary_templates.add_vector("failed_test_cases");
        _summary_templates.add_vector("failed_test_cases_file");
        _summary_templates.add_vector("passed_test_cases");
        _summary_templates.add_vector("passed_test_cases_file");
        _summary_templates.add_vector("skipped_test_cases");
        _summary_templates.add_vector("skipped_test_cases_file");
    }

    /// Callback executed when the context is loaded.
    ///
    /// \param context The context loaded from the database.
    void
    got_context(const model::context& context)
    {
        text::templates_def templates = common_templates();
        templates.add_variable("cwd", context.cwd().str());
        add_map(templates, context.env(), "env_var", "env_var_value");
        generate(templates, "context.html", "context.html");
    }

    /// Callback executed when a test results is found.
    ///
    /// \param iter Container for the test result's data.
    void
    got_result(store::results_iterator& iter)
    {
        const model::test_program_ptr test_program = iter.test_program();
        const std::string& test_case_name = iter.test_case_name();
        const model::test_result result = iter.result();

        if (std::find(_results_filters.begin(), _results_filters.end(),
                      result.type()) == _results_filters.end()) {
            add_to_summary(*test_program, test_case_name, result, false);
            return;
        }

        add_to_summary(*test_program, test_case_name, result, true);

        if (!_start_time || _start_time.get() > iter.start_time())
            _start_time = iter.start_time();
        if (!_end_time || _end_time.get() < iter.end_time())
            _end_time = iter.end_time();

        const datetime::delta duration = iter.end_time() - iter.start_time();

        _runtime += duration;

        text::templates_def templates = common_templates();
        templates.add_variable("test_case",
                               cli::format_test_case_id(*test_program,
                                                        test_case_name));
        templates.add_variable("test_program",
                               test_program->absolute_path().str());
        templates.add_variable("result", cli::format_result(result));
        templates.add_variable("start_time",
                               iter.start_time().to_iso8601_in_utc());
        templates.add_variable("end_time",
                               iter.end_time().to_iso8601_in_utc());
        templates.add_variable("duration", cli::format_delta(duration));

        const model::test_case& test_case = test_program->find(test_case_name);
        add_map(templates, test_case.get_metadata().to_properties(),
                "metadata_var", "metadata_value");

        {
            const std::string stdout_text = iter.stdout_contents();
            if (!stdout_text.empty())
                templates.add_variable("stdout", text::escape_xml(stdout_text));
        }
        {
            const std::string stderr_text = iter.stderr_contents();
            if (!stderr_text.empty())
                templates.add_variable("stderr", text::escape_xml(stderr_text));
        }

        generate(templates, "test_result.html",
                 test_case_filename(*test_program, test_case_name));
    }

    /// Writes the index.html file in the output directory.
    ///
    /// This should only be called once all the processing has been done;
    /// i.e. when the scan_results driver returns.
    void
    write_summary(void)
    {
        const std::size_t n_passed = get_count(model::test_result_passed);
        const std::size_t n_failed = get_count(model::test_result_failed);
        const std::size_t n_skipped = get_count(model::test_result_skipped);
        const std::size_t n_xfail = get_count(
            model::test_result_expected_failure);
        const std::size_t n_broken = get_count(model::test_result_broken);

        const std::size_t n_bad = n_broken + n_failed;

        if (_start_time) {
            INV(_end_time);
            _summary_templates.add_variable(
                "start_time", _start_time.get().to_iso8601_in_utc());
            _summary_templates.add_variable(
                "end_time", _end_time.get().to_iso8601_in_utc());
        } else {
            _summary_templates.add_variable("start_time", "No tests run");
            _summary_templates.add_variable("end_time", "No tests run");
        }
        _summary_templates.add_variable("duration",
                                        cli::format_delta(_runtime));
        _summary_templates.add_variable("passed_tests_count",
                                        F("%s") % n_passed);
        _summary_templates.add_variable("failed_tests_count",
                                        F("%s") % n_failed);
        _summary_templates.add_variable("skipped_tests_count",
                                        F("%s") % n_skipped);
        _summary_templates.add_variable("xfail_tests_count",
                                        F("%s") % n_xfail);
        _summary_templates.add_variable("broken_tests_count",
                                        F("%s") % n_broken);
        _summary_templates.add_variable("bad_tests_count", F("%s") % n_bad);

        generate(text::templates_def(), "report.css", "report.css");
        generate(_summary_templates, "index.html", "index.html");
    }
};


}  // anonymous namespace


/// Default constructor for cmd_report_html.
cli::cmd_report_html::cmd_report_html(void) : cli_command(
    "report-html", "", 0, 0,
    "Generates an HTML report with the result of a test suite run")
{
    add_option(results_file_open_option);
    add_option(cmdline::bool_option(
        "force", "Wipe the output directory before generating the new report; "
        "use care"));
    add_option(cmdline::path_option(
        "output", "The directory in which to store the HTML files",
        "path", "html"));
    add_option(cmdline::list_option(
        "results-filter", "Comma-separated list of result types to include in "
        "the report", "types", "skipped,xfail,broken,failed"));
}


/// Entry point for the "report-html" subcommand.
///
/// \param ui Object to interact with the I/O of the program.
/// \param cmdline Representation of the command line to the subcommand.
///
/// \return 0 if everything is OK, 1 if the statement is invalid or if there is
/// any other problem.
int
cli::cmd_report_html::run(cmdline::ui* ui,
                          const cmdline::parsed_cmdline& cmdline,
                          const config::tree& /* user_config */)
{
    const result_types types = get_result_types(cmdline);

    const fs::path results_file = layout::find_results(
        results_file_open(cmdline));

    const fs::path directory =
        cmdline.get_option< cmdline::path_option >("output");
    create_top_directory(directory, cmdline.has_option("force"));
    html_hooks hooks(ui, directory, types);
    drivers::scan_results::drive(results_file,
                                 std::set< engine::test_filter >(),
                                 hooks);
    hooks.write_summary();

    return EXIT_SUCCESS;
}
