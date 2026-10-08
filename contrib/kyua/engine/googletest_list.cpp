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

#include "engine/googletest_list.hpp"

#include <fstream>
#include <regex>
#include <string>
#include <utility>

#include "engine/exceptions.hpp"
#include "model/metadata.hpp"
#include "model/test_case.hpp"
#include "utils/config/exceptions.hpp"
#include "utils/format/macros.hpp"

namespace config = utils::config;
namespace fs = utils::fs;


namespace {

/// A regular expression that should match either a test suite or a test case.
const std::string name_expr = "([[:alpha:][:digit:]_]+[[:alpha:][:digit:]_/]*)";

/// The separator between a test suite and a test case.
const std::string testsuite_testcase_separator = ".";

}  // anonymous namespace


/// Parses the googletest list of test cases from an open stream.
///
/// \param input The stream to read from.
///
/// \return The collection of parsed test cases.
///
/// \throw format_error If there is any problem in the input data.
model::test_cases_map
engine::parse_googletest_list(std::istream& input)
{
    std::string line, test_suite;

    // TODO: These were moved here from the anonymous namespace because of a
    // Doxygen bug that's reproducible with the version used by Travis CI.
    //
    /// A complete regular expression representing a line with a test case
    /// definition, e,g., "  TestCase",  "  TestCase/0", or
    /// "  TestCase/0  # GetParam() = 4".
    const std::regex testcase_re(
        "  " + name_expr + "([[:space:]]+# GetParam\\(\\) = .+)?");

    /// A complete regular expression representing a line with a test suite
    /// definition, e,g., * "TestSuite.", "TestSuite/Prefix.", or
    /// "TestSuite/Prefix.    # TypeParam = .+".
    const std::regex testsuite_re(
        name_expr + "\\.([[:space:]]+# TypeParam = .+)?");
    // END TODO

    model::test_cases_map_builder test_cases_builder;
    while (std::getline(input, line).good()) {
        std::smatch match;
        if (std::regex_match(line, match, testcase_re)) {
            if (test_suite.empty()) {
                throw format_error("Invalid testcase definition: not preceded "
                                   "by a test suite definition");
            }
            std::string test_case(match[1]);
            test_cases_builder.add(test_suite + test_case);
        } else if (std::regex_match(line, match, testsuite_re)) {
            test_suite = std::string(match[1]) +
                         testsuite_testcase_separator;
        } else {
            // Ignore the line; something might have used output a diagnostic
            // message to stdout, e.g., gtest_main.
        }
    }
    const model::test_cases_map test_cases = test_cases_builder.build();
    if (test_cases.empty()) {
        // The scheduler interface also checks for the presence of at least one
        // test case.  However, because the atf format itself requires one test
        // case to be always present, we check for this condition here as well.
        throw format_error("No test cases");
    }
    return test_cases;
}
