// Copyright 2024 The Kyua Authors.
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

#include "engine/googletest_result.hpp"

#include <cstdlib>
#include <fstream>
#include <regex>
#include <utility>

#include "engine/exceptions.hpp"
#include "model/test_result.hpp"
#include "utils/fs/path.hpp"
#include "utils/format/macros.hpp"
#include "utils/optional.ipp"
#include "utils/process/status.hpp"
#include "utils/sanity.hpp"
#include "utils/text/exceptions.hpp"
#include "utils/text/operations.ipp"

namespace fs = utils::fs;
namespace process = utils::process;
namespace text = utils::text;

using utils::none;
using utils::optional;


namespace {

/// Internal string for specifying invalid output.
const std::string invalid_output_message = "invalid output";

/// Regular expression for disabled tests line.
const std::regex disabled_re(
    R"RE((YOU HAVE [[:digit:]]+ DISABLED TESTS?))RE"
);

/// Regular expression for starting sentinel of results block.
const std::regex starting_sentinel_re(
    R"(\[[[:space:]]+RUN[[:space:]]+\][[:space:]]+[A-Za-z0-9]+\.[A-Za-z0-9]+)"
);

/// Regular expression for ending sentinel of results block.
const std::regex ending_sentinel_re(
    R"RE(\[[[:space:]]+(FAILED|OK|SKIPPED)[[:space:]]+\])RE"
);

/// Parses a test result that does not accept a reason.
///
/// \param status The result status name.
/// \param rest The rest of the line after the status name.
///
/// \return An object representing the test result.
///
/// \throw format_error If the result is invalid (i.e. rest is invalid).
///
/// \pre status must be "successful".
static engine::googletest_result
parse_without_reason(const std::string& status, const std::string& rest)
{
    if (!rest.empty())
        throw engine::format_error(F("%s cannot have a reason") % status);

    if (status == "skipped")
        return engine::googletest_result(engine::googletest_result::skipped);
    else {
        INV(status == "successful");
        return engine::googletest_result(engine::googletest_result::successful);
    }
}


/// Parses a test result that needs a reason.
///
/// \param status The result status name.
/// \param rest The rest of the line after the status name.
///
/// \return An object representing the test result.
///
/// \throw format_error If the result is invalid (i.e. rest is invalid).
///
/// \pre status must be one of "broken", "disabled", "failed", or "skipped".
static engine::googletest_result
parse_with_reason(const std::string& status, const std::string& rest)
{
    using engine::googletest_result;

    INV(!rest.empty());

    if (status == "broken")
        return googletest_result(googletest_result::broken, rest);
    else if (status == "disabled")
        return googletest_result(googletest_result::disabled, rest);
    else if (status == "failed")
        return googletest_result(googletest_result::failed, rest);
    else if (status == "skipped")
        return googletest_result(googletest_result::skipped, rest);
    else
        PRE_MSG(false, "Unexpected status");
}


/// Formats the termination status of a process to be used with validate_result.
///
/// \param status The status to format.
///
/// \return A string describing the status.
static std::string
format_status(const process::status& status)
{
    if (status.exited())
        return F("exited with code %s") % status.exitstatus();
    else if (status.signaled())
        return F("received signal %s%s") % status.termsig() %
            (status.coredump() ? " (core dumped)" : "");
    else
        return F("terminated in an unknown manner");
}


}  // anonymous namespace


/// Constructs a raw result with a type.
///
/// The reason is left uninitialized.
///
/// \param type_ The type of the result.
engine::googletest_result::googletest_result(const types type_) :
    _type(type_)
{
}


/// Constructs a raw result with a type and a reason.
///
/// \param type_ The type of the result.
/// \param reason_ The reason for the result.
engine::googletest_result::googletest_result(
    const types type_,
    const std::string& reason_) :
    _type(type_), _reason(reason_)
{
}


/// Parses an input stream to extract a test result.
///
/// If the parsing fails for any reason, the test result is 'broken' and it
/// contains the reason for the parsing failure.  Test cases that report results
/// in an inconsistent state cannot be trusted (e.g. the test program code may
/// have a bug), and thus why they are reported as broken instead of just failed
/// (which is a legitimate result for a test case).
///
/// \param input The stream to read from.
///
/// \return A generic representation of the result of the test case.
///
/// \throw format_error If the input is invalid.
engine::googletest_result
engine::googletest_result::parse(std::istream& input)
{
    std::vector<std::string> lines;

    do {
        std::string line;
        std::getline(input, line, '\n');
        if (!input.eof())
            line.push_back('\n');
        lines.push_back(line);
    } while (input.good());

    bool capture_context = false;
    bool valid_output = false;
    std::string context, status;

    for (auto& line: lines) {
        std::smatch matches;

        if (regex_search(line, matches, disabled_re)) {
            context = matches[1];
            status = "disabled";
            valid_output = true;
            break;
        }
        if (regex_search(line, matches, starting_sentinel_re)) {
            capture_context = true;
            context = "";
            continue;
        }
        if (regex_search(line, matches, ending_sentinel_re)) {
            std::string googletest_res = matches[1];
            if (googletest_res == "OK") {
                context = "";
                status = "successful";
            } else if (googletest_res == "FAILED") {
                status = "failed";
            } else {
                INV(googletest_res == "SKIPPED");
                status = "skipped";
            }
            capture_context = false;
            valid_output = true;
        }
        if (capture_context) {
            context += line;
        }
    }
    if (!valid_output) {
        context = invalid_output_message;
        status = "broken";
    }
    if (status == "skipped" && context.empty())
        context = bogus_googletest_skipped_nul_message;
    if (context.empty())
        return parse_without_reason(status, context);
    else
        return parse_with_reason(status, context);
}


/// Loads a test case result from a file.
///
/// \param file The file to parse.
///
/// \return The parsed test case result if all goes well.
///
/// \throw std::runtime_error If the file does not exist.
/// \throw engine::format_error If the contents of the file are bogus.
engine::googletest_result
engine::googletest_result::load(const fs::path& file)
{
    std::ifstream input(file.c_str());
    if (!input)
        throw std::runtime_error("Cannot open results file");
    else
        return parse(input);
}


/// Gets the type of the result.
///
/// \return A result type.
engine::googletest_result::types
engine::googletest_result::type(void) const
{
    return _type;
}


/// Gets the optional reason of the result.
///
/// \return The reason of the result if present; none otherwise.
const optional< std::string >&
engine::googletest_result::reason(void) const
{
    return _reason;
}


/// Checks whether the result should be reported as good or not.
///
/// \return True if the result can be considered "good", false otherwise.
bool
engine::googletest_result::good(void) const
{
    switch (_type) {
    case googletest_result::disabled:
    case googletest_result::skipped:
    case googletest_result::successful:
        return true;

    case googletest_result::broken:
    case googletest_result::failed:
        return false;

    default:
        UNREACHABLE;
    }
}


/// Reinterprets a raw result based on the termination status of the test case.
///
/// This reinterpretation ensures that the termination conditions of the program
/// match what is expected of the paticular result reported by the test program.
/// If such conditions do not match, the test program is considered bogus and is
/// thus reported as broken.
///
/// This is just a helper function for calculate_result(); the real result of
/// the test case cannot be inferred from apply() only.
///
/// \param status The exit status of the test program, or none if the test
/// program timed out.
///
/// \result The adjusted result.  The original result is transformed into broken
/// if the exit status of the program does not match our expectations.
engine::googletest_result
engine::googletest_result::apply(const optional< process::status >& status)
    const
{
    if (!status) {
        return *this;
    }

    auto check_status = [&status](bool expect_pass) -> bool {
        if (!status.get().exited()) {
            return false;
        }
        auto exit_status = status.get().exitstatus();
        if (expect_pass) {
            return exit_status == EXIT_SUCCESS;
        } else {
            return exit_status == EXIT_FAILURE;
        }
    };

    switch (_type) {
    case googletest_result::broken:
        return *this;

    case googletest_result::disabled:
        if (check_status(true)) {
            return *this;
        }
        return googletest_result(googletest_result::broken,
            "Disabled test case should have reported success but " +
            format_status(status.get()));

    case googletest_result::failed:
        if (check_status(false)) {
            return *this;
        }
        return googletest_result(googletest_result::broken,
            "Failed test case should have reported failure but " +
            format_status(status.get()));

    case googletest_result::skipped:
        if (check_status(true)) {
            return *this;
        }
        return googletest_result(googletest_result::broken,
            "Skipped test case should have reported success but " +
            format_status(status.get()));

    case googletest_result::successful:
        if (check_status(true)) {
            return *this;
        }
        return googletest_result(googletest_result::broken,
            "Passed test case should have reported success but " +
            format_status(status.get()));
    }

    UNREACHABLE;
}


/// Converts an internal result to the interface-agnostic representation.
///
/// \return A generic result instance representing this result.
model::test_result
engine::googletest_result::externalize(void) const
{
    switch (_type) {
    case googletest_result::broken:
        return model::test_result(model::test_result_broken, _reason.get());

    case googletest_result::disabled:
        return model::test_result(model::test_result_skipped, _reason.get());

    case googletest_result::failed:
        return model::test_result(model::test_result_failed, _reason.get());

    case googletest_result::skipped:
        return model::test_result(model::test_result_skipped, _reason.get());

    case googletest_result::successful:
        return model::test_result(model::test_result_passed);

    default:
        UNREACHABLE;
    }
}


/// Compares two raw results for equality.
///
/// \param other The result to compare to.
///
/// \return True if the two raw results are equal; false otherwise.
bool
engine::googletest_result::operator==(const googletest_result& other) const
{
    return _type == other._type &&
        _reason == other._reason;
}


/// Compares two raw results for inequality.
///
/// \param other The result to compare to.
///
/// \return True if the two raw results are different; false otherwise.
bool
engine::googletest_result::operator!=(const googletest_result& other) const
{
    return !(*this == other);
}


/// Injects the object into a stream.
///
/// \param output The stream into which to inject the object.
/// \param object The object to format.
///
/// \return The output stream.
std::ostream&
engine::operator<<(std::ostream& output, const googletest_result& object)
{
    std::string result_name;
    switch (object.type()) {
    case googletest_result::broken: result_name = "broken"; break;
    case googletest_result::disabled: result_name = "disabled"; break;
    case googletest_result::failed: result_name = "failed"; break;
    case googletest_result::skipped: result_name = "skipped"; break;
    case googletest_result::successful: result_name = "successful"; break;
    }

    const optional< std::string >& reason = object.reason();

    output << F("model::test_result{type=%s, reason=%s}")
        % text::quote(result_name, '\'')
        % (reason ? text::quote(reason.get(), '\'') : "none");

    return output;
}


/// Calculates the user-visible result of a test case.
///
/// This function needs to perform magic to ensure that what the test case
/// reports as its result is what the user should really see: i.e. it adjusts
/// the reported status of the test to the exit conditions of its body and
/// cleanup parts.
///
/// \param body_status The termination status of the process that executed
///     the body of the test.  None if the body timed out.
/// \param results_file The path to the results file that the test case body is
///     supposed to have created.
///
/// \return The calculated test case result.
model::test_result
engine::calculate_googletest_result(
                             const optional< process::status >& body_status,
                             const fs::path& results_file)
{
    using engine::googletest_result;

    googletest_result result(googletest_result::broken, "Unknown result");
    try {
        result = googletest_result::load(results_file);
        /// atf_result.cpp handles this by side-effect by setting a broken
        /// result with "", which results in a std::runtime_error throw from
        /// atf_result::parse(..). googletest_result::parse doesn't do that and
        /// instead returns a broken result with a reason of
        /// `invalid_output_message`, so that case needs to be tested for after
        /// the fact to make sure the reason why `invalid_output_message` was
        /// because of a program that crashed or failed.
        if (result.type() == googletest_result::broken && body_status) {
            const optional< std::string >& reason = result.reason();
            if (reason && reason.get() == invalid_output_message) {
                result = googletest_result(
                    googletest_result::broken,
                    F("Error: Premature exit. Test case %s") %
                    format_status(body_status.get()));
            }
        }
    } catch (const engine::format_error& error) {
        if (body_status)
            result = googletest_result(googletest_result::broken,
                F("Error: %s. Test case %s") %
		error.what() %
                format_status(body_status.get()));
	else {
            // The test case timed out.  apply() handles this case later.
	}
    } catch (const std::runtime_error& error) {
        if (body_status)
            result = googletest_result(
                googletest_result::broken,
                F("Error: Premature exit. Test case %s") %
                format_status(body_status.get()));
        else {
            // The test case timed out.  apply() handles this case later.
        }
    }

    result = result.apply(body_status);

    return result.externalize();
}
