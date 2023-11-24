// Copyright 2014 The Kyua Authors.
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

#include "drivers/report_junit.hpp"

#include <algorithm>

#include "model/context.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "model/test_program.hpp"
#include "model/test_result.hpp"
#include "model/types.hpp"
#include "store/read_transaction.hpp"
#include "utils/datetime.hpp"
#include "utils/defs.hpp"
#include "utils/format/macros.hpp"
#include "utils/text/operations.hpp"

namespace config = utils::config;
namespace datetime = utils::datetime;
namespace text = utils::text;


/// Converts a test program name into a class-like name.
///
/// \param test_program Test program from which to extract the name.
///
/// \return A class-like representation of the test program's identifier.
std::string
drivers::junit_classname(const model::test_program& test_program)
{
    std::string classname = test_program.relative_path().str();
    std::replace(classname.begin(), classname.end(), '/', '.');
    return classname;
}


/// Converts a test case's duration to a second-based representation.
///
/// \param delta The duration to convert.
///
/// \return A second-based with millisecond-precision representation of the
/// input duration.
std::string
drivers::junit_duration(const datetime::delta& delta)
{
    return F("%.3s") % (delta.seconds + (delta.useconds / 1000000.0));
}


/// String to prepend to the formatted test case metadata.
const char* const drivers::junit_metadata_header =
    "Test case metadata\n"
    "------------------\n"
    "\n";


/// String to prepend to the formatted test case timing details.
const char* const drivers::junit_timing_header =
    "\n"
    "Timing information\n"
    "------------------\n"
    "\n";


/// String to append to the formatted test case metadata.
const char* const drivers::junit_stderr_header =
    "\n"
    "Original stderr\n"
    "---------------\n"
    "\n";


/// Formats a test's metadata for recording in stderr.
///
/// \param metadata The metadata to format.
///
/// \return A string with the metadata contents that can be prepended to the
/// original test's stderr.
std::string
drivers::junit_metadata(const model::metadata& metadata)
{
    const model::properties_map props = metadata.to_properties();
    if (props.empty())
        return "";

    std::ostringstream output;
    output << junit_metadata_header;
    for (model::properties_map::const_iterator iter = props.begin();
         iter != props.end(); ++iter) {
        if ((*iter).second.empty()) {
            output << F("%s is empty\n") % (*iter).first;
        } else {
            output << F("%s = %s\n") % (*iter).first % (*iter).second;
        }
    }
    return output.str();
}


/// Formats a test's timing information for recording in stderr.
///
/// \param start_time The start time of the test.
/// \param end_time The end time of the test.
///
/// \return A string with the timing information that can be prepended to the
/// original test's stderr.
std::string
drivers::junit_timing(const datetime::timestamp& start_time,
                      const datetime::timestamp& end_time)
{
    std::ostringstream output;
    output << junit_timing_header;
    output << F("Start time: %s\n") % start_time.to_iso8601_in_utc();
    output << F("End time:   %s\n") % end_time.to_iso8601_in_utc();
    output << F("Duration:   %ss\n") % junit_duration(end_time - start_time);
    return output.str();
}


/// Constructor for the hooks.
///
/// \param [out] output_ Stream to which to write the report.
drivers::report_junit_hooks::report_junit_hooks(std::ostream& output_) :
    _output(output_)
{
}


/// Callback executed when the context is loaded.
///
/// \param context The context loaded from the database.
void
drivers::report_junit_hooks::got_context(const model::context& context)
{
    _output << "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n";
    _output << "<testsuite>\n";

    _output << "<properties>\n";
    _output << F("<property name=\"cwd\" value=\"%s\"/>\n")
        % text::escape_xml(context.cwd().str());
    for (model::properties_map::const_iterator iter =
             context.env().begin(); iter != context.env().end(); ++iter) {
        _output << F("<property name=\"env.%s\" value=\"%s\"/>\n")
            % text::escape_xml((*iter).first)
            % text::escape_xml((*iter).second);
    }
    _output << "</properties>\n";
}


/// Callback executed when a test results is found.
///
/// \param iter Container for the test result's data.
void
drivers::report_junit_hooks::got_result(store::results_iterator& iter)
{
    const model::test_result result = iter.result();

    _output << F("<testcase classname=\"%s\" name=\"%s\" time=\"%s\">\n")
        % text::escape_xml(junit_classname(*iter.test_program()))
        % text::escape_xml(iter.test_case_name())
        % junit_duration(iter.end_time() - iter.start_time());

    std::string stderr_contents;

    switch (result.type()) {
    case model::test_result_failed:
        _output << F("<failure message=\"%s\"/>\n")
            % text::escape_xml(result.reason());
        break;

    case model::test_result_expected_failure:
        stderr_contents += ("Expected failure result details\n"
                            "-------------------------------\n"
                            "\n"
                            + result.reason() + "\n"
                            "\n");
        break;

    case model::test_result_passed:
        // Passed results have no status nodes.
        break;

    case model::test_result_skipped:
        _output << "<skipped/>\n";
        stderr_contents += ("Skipped result details\n"
                            "----------------------\n"
                            "\n"
                            + result.reason() + "\n"
                            "\n");
        break;

    default:
        _output << F("<error message=\"%s\"/>\n")
            % text::escape_xml(result.reason());
    }

    const std::string stdout_contents = iter.stdout_contents();
    if (!stdout_contents.empty()) {
        _output << F("<system-out>%s</system-out>\n")
            % text::escape_xml(stdout_contents);
    }

    {
        const model::test_case& test_case = iter.test_program()->find(
            iter.test_case_name());
        stderr_contents += junit_metadata(test_case.get_metadata());
    }
    stderr_contents += junit_timing(iter.start_time(), iter.end_time());
    {
        stderr_contents += junit_stderr_header;
        const std::string real_stderr_contents = iter.stderr_contents();
        if (real_stderr_contents.empty()) {
            stderr_contents += "<EMPTY>\n";
        } else {
            stderr_contents += real_stderr_contents;
        }
    }
    _output << "<system-err>" << text::escape_xml(stderr_contents)
            << "</system-err>\n";

    _output << "</testcase>\n";
}


/// Finalizes the report.
void
drivers::report_junit_hooks::end(const drivers::scan_results::result& /* r */)
{
    _output << "</testsuite>\n";
}
