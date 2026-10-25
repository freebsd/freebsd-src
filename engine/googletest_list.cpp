// Copyright 2026 The Kyua Authors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-2-Clause

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
