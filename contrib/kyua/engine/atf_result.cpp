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

#include "engine/atf_result.hpp"

#include <cstdlib>
#include <fstream>
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


/// Reads a file and flattens its lines.
///
/// The main purpose of this function is to simplify the parsing of a file
/// containing the result of a test.  Therefore, the return value carries
/// several assumptions.
///
/// \param input The stream to read from.
///
/// \return A pair (line count, contents) detailing how many lines where read
/// and their contents.  If the file contains a single line with no newline
/// character, the line count is 0.  If the file includes more than one line,
/// the lines are merged together and separated by the magic string
/// '&lt;&lt;NEWLINE&gt;&gt;'.
static std::pair< size_t, std::string >
read_lines(std::istream& input)
{
    std::pair< size_t, std::string > ret = std::make_pair(0, "");

    do {
        std::string line;
        std::getline(input, line);
        if (input.eof() && !line.empty()) {
            if (ret.first == 0)
                ret.second = line;
            else {
                ret.second += "<<NEWLINE>>" + line;
                ret.first++;
            }
        } else if (input.good()) {
            if (ret.first == 0)
                ret.second = line;
            else
                ret.second += "<<NEWLINE>>" + line;
            ret.first++;
        }
    } while (input.good());

    return ret;
}


/// Parses a test result that does not accept a reason.
///
/// \param status The result status name.
/// \param rest The rest of the line after the status name.
///
/// \return An object representing the test result.
///
/// \throw format_error If the result is invalid (i.e. rest is invalid).
///
/// \pre status must be "passed".
static engine::atf_result
parse_without_reason(const std::string& status, const std::string& rest)
{
    if (!rest.empty())
        throw engine::format_error(F("%s cannot have a reason") % status);
    PRE(status == "passed");
    return engine::atf_result(engine::atf_result::passed);
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
/// \pre status must be one of "broken", "expected_death", "expected_failure",
/// "expected_timeout", "failed" or "skipped".
static engine::atf_result
parse_with_reason(const std::string& status, const std::string& rest)
{
    using engine::atf_result;

    if (rest.length() < 3 || rest.substr(0, 2) != ": ")
        throw engine::format_error(F("%s must be followed by ': <reason>'") %
                                   status);
    const std::string reason = rest.substr(2);
    INV(!reason.empty());

    if (status == "broken")
        return atf_result(atf_result::broken, reason);
    else if (status == "expected_death")
        return atf_result(atf_result::expected_death, reason);
    else if (status == "expected_failure")
        return atf_result(atf_result::expected_failure, reason);
    else if (status == "expected_timeout")
        return atf_result(atf_result::expected_timeout, reason);
    else if (status == "failed")
        return atf_result(atf_result::failed, reason);
    else if (status == "skipped")
        return atf_result(atf_result::skipped, reason);
    else
        PRE_MSG(false, "Unexpected status");
}


/// Converts a string to an integer.
///
/// \param str The string containing the integer to convert.
///
/// \return The converted integer; none if the parsing fails.
static optional< int >
parse_int(const std::string& str)
{
    try {
        return utils::make_optional(text::to_type< int >(str));
    } catch (const text::value_error& e) {
        return none;
    }
}


/// Parses a test result that needs a reason and accepts an optional integer.
///
/// \param status The result status name.
/// \param rest The rest of the line after the status name.
///
/// \return The parsed test result if the data is valid, or a broken result if
/// the parsing failed.
///
/// \pre status must be one of "expected_exit" or "expected_signal".
static engine::atf_result
parse_with_reason_and_arg(const std::string& status, const std::string& rest)
{
    using engine::atf_result;

    std::string::size_type delim = rest.find_first_of(":(");
    if (delim == std::string::npos)
        throw engine::format_error(F("Invalid format for '%s' test case "
                                     "result; must be followed by '[(num)]: "
                                     "<reason>' but found '%s'") %
                                   status % rest);

    optional< int > arg;
    if (rest[delim] == '(') {
        const std::string::size_type delim2 = rest.find("):", delim);
        if (delim == std::string::npos)
            throw engine::format_error(F("Mismatched '(' in %s") % rest);

        const std::string argstr = rest.substr(delim + 1, delim2 - delim - 1);
        arg = parse_int(argstr);
        if (!arg)
            throw engine::format_error(F("Invalid integer argument '%s' to "
                                         "'%s' test case result") %
                                       argstr % status);
        delim = delim2 + 1;
    }

    const std::string reason = rest.substr(delim + 2);

    if (status == "expected_exit")
        return atf_result(atf_result::expected_exit, arg, reason);
    else if (status == "expected_signal")
        return atf_result(atf_result::expected_signal, arg, reason);
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
/// The reason and the argument are left uninitialized.
///
/// \param type_ The type of the result.
engine::atf_result::atf_result(const types type_) :
    _type(type_)
{
}


/// Constructs a raw result with a type and a reason.
///
/// The argument is left uninitialized.
///
/// \param type_ The type of the result.
/// \param reason_ The reason for the result.
engine::atf_result::atf_result(const types type_, const std::string& reason_) :
    _type(type_), _reason(reason_)
{
}


/// Constructs a raw result with a type, an optional argument and a reason.
///
/// \param type_ The type of the result.
/// \param argument_ The optional argument for the result.
/// \param reason_ The reason for the result.
engine::atf_result::atf_result(const types type_,
                               const utils::optional< int >& argument_,
                               const std::string& reason_) :
    _type(type_), _argument(argument_), _reason(reason_)
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
engine::atf_result
engine::atf_result::parse(std::istream& input)
{
    const std::pair< size_t, std::string > data = read_lines(input);
    if (data.first == 0)
        throw format_error("Empty test result or no new line");
    else if (data.first > 1)
        throw format_error("Test result contains multiple lines: " +
                           data.second);
    else {
        const std::string::size_type delim = data.second.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyz_");
        const std::string status = data.second.substr(0, delim);
        const std::string rest = data.second.substr(status.length());

        if (status == "broken")
            return parse_with_reason(status, rest);
        else if (status == "expected_death")
            return parse_with_reason(status, rest);
        else if (status == "expected_exit")
            return parse_with_reason_and_arg(status, rest);
        else if (status == "expected_failure")
            return parse_with_reason(status, rest);
        else if (status == "expected_signal")
            return parse_with_reason_and_arg(status, rest);
        else if (status == "expected_timeout")
            return parse_with_reason(status, rest);
        else if (status == "failed")
            return parse_with_reason(status, rest);
        else if (status == "passed")
            return parse_without_reason(status, rest);
        else if (status == "skipped")
            return parse_with_reason(status, rest);
        else
            throw format_error(F("Unknown test result '%s'") % status);
    }
}


/// Loads a test case result from a file.
///
/// \param file The file to parse.
///
/// \return The parsed test case result if all goes well.
///
/// \throw std::runtime_error If the file does not exist.
/// \throw engine::format_error If the contents of the file are bogus.
engine::atf_result
engine::atf_result::load(const fs::path& file)
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
engine::atf_result::types
engine::atf_result::type(void) const
{
    return _type;
}


/// Gets the optional argument of the result.
///
/// \return The argument of the result if present; none otherwise.
const optional< int >&
engine::atf_result::argument(void) const
{
    return _argument;
}


/// Gets the optional reason of the result.
///
/// \return The reason of the result if present; none otherwise.
const optional< std::string >&
engine::atf_result::reason(void) const
{
    return _reason;
}


/// Checks whether the result should be reported as good or not.
///
/// \return True if the result can be considered "good", false otherwise.
bool
engine::atf_result::good(void) const
{
    switch (_type) {
    case atf_result::expected_death:
    case atf_result::expected_exit:
    case atf_result::expected_failure:
    case atf_result::expected_signal:
    case atf_result::expected_timeout:
    case atf_result::passed:
    case atf_result::skipped:
        return true;

    case atf_result::broken:
    case atf_result::failed:
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
engine::atf_result
engine::atf_result::apply(const optional< process::status >& status)
    const
{
    if (!status) {
        if (_type != atf_result::expected_timeout)
            return atf_result(atf_result::broken, "Test case body timed out");
        else
            return *this;
    }

    INV(status);
    switch (_type) {
    case atf_result::broken:
        return *this;

    case atf_result::expected_death:
        return *this;

    case atf_result::expected_exit:
        if (status.get().exited()) {
            if (_argument) {
                if (_argument.get() == status.get().exitstatus())
                    return *this;
                else
                    return atf_result(
                        atf_result::failed,
                        F("Test case expected to exit with code %s but got "
                          "code %s") %
                        _argument.get() % status.get().exitstatus());
            } else
                return *this;
        } else
              return atf_result(atf_result::broken, "Expected clean exit but " +
                                format_status(status.get()));

    case atf_result::expected_failure:
        if (status.get().exited() && status.get().exitstatus() == EXIT_SUCCESS)
            return *this;
        else
            return atf_result(atf_result::broken, "Expected failure should "
                              "have reported success but " +
                              format_status(status.get()));

    case atf_result::expected_signal:
        if (status.get().signaled()) {
            if (_argument) {
                if (_argument.get() == status.get().termsig())
                    return *this;
                else
                    return atf_result(
                        atf_result::failed,
                        F("Test case expected to receive signal %s but "
                          "got %s") %
                        _argument.get() % status.get().termsig());
            } else
                return *this;
        } else
            return atf_result(atf_result::broken, "Expected signal but " +
                              format_status(status.get()));

    case atf_result::expected_timeout:
        return atf_result(atf_result::broken, "Expected timeout but " +
                          format_status(status.get()));

    case atf_result::failed:
        if (status.get().exited() && status.get().exitstatus() == EXIT_FAILURE)
            return *this;
        else
            return atf_result(atf_result::broken, "Failed test case should "
                              "have reported failure but " +
                              format_status(status.get()));

    case atf_result::passed:
        if (status.get().exited() && status.get().exitstatus() == EXIT_SUCCESS)
            return *this;
        else
            return atf_result(atf_result::broken, "Passed test case should "
                              "have reported success but " +
                              format_status(status.get()));

    case atf_result::skipped:
        if (status.get().exited() && status.get().exitstatus() == EXIT_SUCCESS)
            return *this;
        else
            return atf_result(atf_result::broken, "Skipped test case should "
                              "have reported success but " +
                              format_status(status.get()));
    }

    UNREACHABLE;
}


/// Converts an internal result to the interface-agnostic representation.
///
/// \return A generic result instance representing this result.
model::test_result
engine::atf_result::externalize(void) const
{
    switch (_type) {
    case atf_result::broken:
        return model::test_result(model::test_result_broken, _reason.get());

    case atf_result::expected_death:
    case atf_result::expected_exit:
    case atf_result::expected_failure:
    case atf_result::expected_signal:
    case atf_result::expected_timeout:
        return model::test_result(model::test_result_expected_failure,
                                  _reason.get());

    case atf_result::failed:
        return model::test_result(model::test_result_failed, _reason.get());

    case atf_result::passed:
        return model::test_result(model::test_result_passed);

    case atf_result::skipped:
        return model::test_result(model::test_result_skipped, _reason.get());

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
engine::atf_result::operator==(const atf_result& other) const
{
    return _type == other._type && _argument == other._argument &&
        _reason == other._reason;
}


/// Compares two raw results for inequality.
///
/// \param other The result to compare to.
///
/// \return True if the two raw results are different; false otherwise.
bool
engine::atf_result::operator!=(const atf_result& other) const
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
engine::operator<<(std::ostream& output, const atf_result& object)
{
    std::string result_name;
    switch (object.type()) {
    case atf_result::broken: result_name = "broken"; break;
    case atf_result::expected_death: result_name = "expected_death"; break;
    case atf_result::expected_exit: result_name = "expected_exit"; break;
    case atf_result::expected_failure: result_name = "expected_failure"; break;
    case atf_result::expected_signal: result_name = "expected_signal"; break;
    case atf_result::expected_timeout: result_name = "expected_timeout"; break;
    case atf_result::failed: result_name = "failed"; break;
    case atf_result::passed: result_name = "passed"; break;
    case atf_result::skipped: result_name = "skipped"; break;
    }

    const optional< int >& argument = object.argument();

    const optional< std::string >& reason = object.reason();

    output << F("model::test_result{type=%s, argument=%s, reason=%s}")
        % text::quote(result_name, '\'')
        % (argument ? (F("%s") % argument.get()).str() : "none")
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
engine::calculate_atf_result(const optional< process::status >& body_status,
                             const fs::path& results_file)
{
    using engine::atf_result;

    atf_result result(atf_result::broken, "Unknown result");
    try {
        result = atf_result::load(results_file);
    } catch (const engine::format_error& error) {
        result = atf_result(atf_result::broken, error.what());
    } catch (const std::runtime_error& error) {
        if (body_status)
            result = atf_result(
                atf_result::broken, F("Premature exit; test case %s") %
                format_status(body_status.get()));
        else {
            // The test case timed out.  apply() handles this case later.
        }
    }

    result = result.apply(body_status);

    return result.externalize();
}
