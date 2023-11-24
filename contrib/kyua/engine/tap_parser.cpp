// Copyright 2015 The Kyua Authors.
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

#include "engine/tap_parser.hpp"

#include <fstream>

#include "engine/exceptions.hpp"
#include "utils/format/macros.hpp"
#include "utils/noncopyable.hpp"
#include "utils/optional.ipp"
#include "utils/sanity.hpp"
#include "utils/text/exceptions.hpp"
#include "utils/text/operations.ipp"
#include "utils/text/regex.hpp"

namespace fs = utils::fs;
namespace text = utils::text;

using utils::optional;


/// TAP plan representing all tests being skipped.
const engine::tap_plan engine::all_skipped_plan(1, 0);


namespace {


/// Implementation of the TAP parser.
///
/// This is a class only to simplify keeping global constant values around (like
/// prebuilt regular expressions).
class tap_parser : utils::noncopyable {
    /// Regular expression to match plan lines.
    text::regex _plan_regex;

    /// Regular expression to match a TODO and extract the reason.
    text::regex _todo_regex;

    /// Regular expression to match a SKIP and extract the reason.
    text::regex _skip_regex;

    /// Regular expression to match a single test result.
    text::regex _result_regex;

    /// Checks if a line contains a TAP plan and extracts its data.
    ///
    /// \param line The line to try to parse.
    /// \param [in,out] out_plan Used to store the found plan, if any.  The same
    ///     output variable should be given to all calls to this function so
    ///     that duplicate plan entries can be discovered.
    /// \param [out] out_all_skipped_reason Used to store the reason for all
    ///     tests being skipped, if any.  If this is set to a non-empty value,
    ///     then the out_plan is set to 1..0.
    ///
    /// \return True if the line matched a plan; false otherwise.
    ///
    /// \throw engine::format_error If the input is invalid.
    /// \throw text::error If the input is invalid.
    bool
    try_parse_plan(const std::string& line,
                   optional< engine::tap_plan >& out_plan,
                   std::string& out_all_skipped_reason)
    {
        const text::regex_matches plan_matches = _plan_regex.match(line);
        if (!plan_matches)
            return false;
        const engine::tap_plan plan(
            text::to_type< std::size_t >(plan_matches.get(1)),
            text::to_type< std::size_t >(plan_matches.get(2)));

        if (out_plan)
            throw engine::format_error(
                F("Found duplicate plan %s..%s (saw %s..%s earlier)") %
                plan.first % plan.second %
                out_plan.get().first % out_plan.get().second);

        std::string all_skipped_reason;
        const text::regex_matches skip_matches = _skip_regex.match(line);
        if (skip_matches) {
            if (plan != engine::all_skipped_plan) {
                throw engine::format_error(F("Skipped plan must be %s..%s") %
                                           engine::all_skipped_plan.first %
                                           engine::all_skipped_plan.second);
            }
            all_skipped_reason = skip_matches.get(2);
            if (all_skipped_reason.empty())
                all_skipped_reason = "No reason specified";
        } else {
            if (plan.first > plan.second)
                throw engine::format_error(F("Found reversed plan %s..%s") %
                                           plan.first % plan.second);
        }

        INV(!out_plan);
        out_plan = plan;
        out_all_skipped_reason = all_skipped_reason;

        POST(out_plan);
        POST(out_all_skipped_reason.empty() ||
             out_plan.get() == engine::all_skipped_plan);

        return true;
    }

    /// Checks if a line contains a TAP test result and extracts its data.
    ///
    /// \param line The line to try to parse.
    /// \param [in,out] out_ok_count Accumulator for 'ok' results.
    /// \param [in,out] out_not_ok_count Accumulator for 'not ok' results.
    /// \param [out] out_bailed_out Set to true if the test bailed out.
    ///
    /// \return True if the line matched a result; false otherwise.
    ///
    /// \throw engine::format_error If the input is invalid.
    /// \throw text::error If the input is invalid.
    bool
    try_parse_result(const std::string& line, std::size_t& out_ok_count,
                     std::size_t& out_not_ok_count, bool& out_bailed_out)
    {
        PRE(!out_bailed_out);

        const text::regex_matches result_matches = _result_regex.match(line);
        if (result_matches) {
            if (result_matches.get(1) == "ok") {
                ++out_ok_count;
            } else {
                INV(result_matches.get(1) == "not ok");
                if (_todo_regex.match(line) || _skip_regex.match(line)) {
                    ++out_ok_count;
                } else {
                    ++out_not_ok_count;
                }
            }
            return true;
        } else {
            if (line.find("Bail out!") == 0) {
                out_bailed_out = true;
                return true;
            } else {
                return false;
            }
        }
    }

public:
    /// Sets up the TAP parser state.
    tap_parser(void) :
        _plan_regex(text::regex::compile("^([0-9]+)\\.\\.([0-9]+)", 2)),
        _todo_regex(text::regex::compile("TODO[ \t]*(.*)$", 2, true)),
        _skip_regex(text::regex::compile("(SKIP|Skipped:?)[ \t]*(.*)$", 2,
                                         true)),
        _result_regex(text::regex::compile("^(not ok|ok)[ \t-]+[0-9]*", 1))
    {
    }

    /// Parses an input file containing TAP output.
    ///
    /// \param input The stream to read from.
    ///
    /// \return The results of the parsing in the form of a tap_summary object.
    ///
    /// \throw engine::format_error If there are any syntax errors in the input.
    /// \throw text::error If there are any syntax errors in the input.
    engine::tap_summary
    parse(std::ifstream& input)
    {
        optional< engine::tap_plan > plan;
        std::string all_skipped_reason;
        bool bailed_out = false;
        std::size_t ok_count = 0, not_ok_count = 0;

        std::string line;
        while (!bailed_out && std::getline(input, line)) {
            if (try_parse_result(line, ok_count, not_ok_count, bailed_out))
                continue;
            (void)try_parse_plan(line, plan, all_skipped_reason);
        }

        if (bailed_out) {
            return engine::tap_summary::new_bailed_out();
        } else {
            if (!plan)
                throw engine::format_error(
                    "Output did not contain any TAP plan and the program did "
                    "not bail out");

            if (plan.get() == engine::all_skipped_plan) {
                return engine::tap_summary::new_all_skipped(all_skipped_reason);
            } else {
                const std::size_t exp_count = plan.get().second -
                    plan.get().first + 1;
                const std::size_t actual_count = ok_count + not_ok_count;
                if (exp_count != actual_count) {
                    throw engine::format_error(
                        "Reported plan differs from actual executed tests");
                }
                return engine::tap_summary::new_results(plan.get(), ok_count,
                                                        not_ok_count);
            }
        }
    }
};


}  // anonymous namespace


/// Constructs a TAP summary with the results of parsing a TAP output.
///
/// \param bailed_out_ Whether the test program bailed out early or not.
/// \param plan_ The TAP plan.
/// \param all_skipped_reason_ The reason for skipping all tests, if any.
/// \param ok_count_ Number of 'ok' test results.
/// \param not_ok_count_ Number of 'not ok' test results.
engine::tap_summary::tap_summary(const bool bailed_out_,
                                 const tap_plan& plan_,
                                 const std::string& all_skipped_reason_,
                                 const std::size_t ok_count_,
                                 const std::size_t not_ok_count_) :
    _bailed_out(bailed_out_), _plan(plan_),
    _all_skipped_reason(all_skipped_reason_),
    _ok_count(ok_count_), _not_ok_count(not_ok_count_)
{
}


/// Constructs a TAP summary for a bailed out test program.
///
/// \return The new tap_summary object.
engine::tap_summary
engine::tap_summary::new_bailed_out(void)
{
    return tap_summary(true, tap_plan(0, 0), "", 0, 0);
}


/// Constructs a TAP summary for a test program that skipped all tests.
///
/// \param reason Textual reason describing why the tests were skipped.
///
/// \return The new tap_summary object.
engine::tap_summary
engine::tap_summary::new_all_skipped(const std::string& reason)
{
    return tap_summary(false, all_skipped_plan, reason, 0, 0);
}


/// Constructs a TAP summary for a test program that reported results.
///
/// \param plan_ The TAP plan.
/// \param ok_count_ Total number of 'ok' results.
/// \param not_ok_count_ Total number of 'not ok' results.
///
/// \return The new tap_summary object.
engine::tap_summary
engine::tap_summary::new_results(const tap_plan& plan_,
                                 const std::size_t ok_count_,
                                 const std::size_t not_ok_count_)
{
    PRE((plan_.second - plan_.first + 1) == (ok_count_ + not_ok_count_));
    return tap_summary(false, plan_, "", ok_count_, not_ok_count_);
}


/// Checks whether the test program bailed out early or not.
///
/// \return True if the test program aborted execution before completing.
bool
engine::tap_summary::bailed_out(void) const
{
    return _bailed_out;
}


/// Gets the TAP plan of the test program.
///
/// \pre bailed_out() must be false.
///
/// \return The TAP plan.  If 1..0, then all_skipped_reason() will have some
/// contents.
const engine::tap_plan&
engine::tap_summary::plan(void) const
{
    PRE(!_bailed_out);
    return _plan;
}


/// Gets the reason for skipping all the tests, if any.
///
/// \pre bailed_out() must be false.
/// \pre plan() returns 1..0.
///
/// \return The reason for skipping all the tests.
const std::string&
engine::tap_summary::all_skipped_reason(void) const
{
    PRE(!_bailed_out);
    PRE(_plan == all_skipped_plan);
    return _all_skipped_reason;
}


/// Gets the number of 'ok' test results.
///
/// \pre bailed_out() must be false.
///
/// \return The number of test results that reported 'ok'.
std::size_t
engine::tap_summary::ok_count(void) const
{
    PRE(!bailed_out());
    PRE(_all_skipped_reason.empty());
    return _ok_count;
}


/// Gets the number of 'not ok' test results.
///
/// \pre bailed_out() must be false.
///
/// \return The number of test results that reported 'not ok'.
std::size_t
engine::tap_summary::not_ok_count(void) const
{
    PRE(!_bailed_out);
    PRE(_all_skipped_reason.empty());
    return _not_ok_count;
}


/// Checks two tap_summary objects for equality.
///
/// \param other The object to compare this one to.
///
/// \return True if the two objects are equal; false otherwise.
bool
engine::tap_summary::operator==(const tap_summary& other) const
{
    return (_bailed_out == other._bailed_out &&
            _plan == other._plan &&
            _all_skipped_reason == other._all_skipped_reason &&
            _ok_count == other._ok_count &&
            _not_ok_count == other._not_ok_count);
}


/// Checks two tap_summary objects for inequality.
///
/// \param other The object to compare this one to.
///
/// \return True if the two objects are different; false otherwise.
bool
engine::tap_summary::operator!=(const tap_summary& other) const
{
    return !(*this == other);
}


/// Formats a tap_summary into a stream.
///
/// \param output The stream into which to inject the object.
/// \param summary The summary to format.
///
/// \return The output stream.
std::ostream&
engine::operator<<(std::ostream& output, const tap_summary& summary)
{
    output << "tap_summary{";
    if (summary.bailed_out()) {
        output << "bailed_out=true";
    } else {
        const tap_plan& plan = summary.plan();
        output << "bailed_out=false"
               << ", plan=" << plan.first << ".." << plan.second;
        if (plan == all_skipped_plan) {
            output << ", all_skipped_reason=" << summary.all_skipped_reason();
        } else {
            output << ", ok_count=" << summary.ok_count()
                   << ", not_ok_count=" << summary.not_ok_count();
        }
    }
    output << "}";
    return output;
}


/// Parses an input file containing the TAP output of a test program.
///
/// \param filename Path to the file to parse.
///
/// \return The parsed data in the form of a tap_summary.
///
/// \throw load_error If there are any problems parsing the file.  Such problems
///     should be considered as test program breakage.
engine::tap_summary
engine::parse_tap_output(const utils::fs::path& filename)
{
    std::ifstream input(filename.str().c_str());
    if (!input)
        throw engine::load_error(filename, "Failed to open TAP output file");

    try {
        return tap_summary(tap_parser().parse(input));
    } catch (const engine::format_error& e) {
        throw engine::load_error(filename, e.what());
    } catch (const text::error& e) {
        throw engine::load_error(filename, e.what());
    }
}
