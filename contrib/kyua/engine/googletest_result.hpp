// Copyright 2026 The Kyua Authors.
// All rights reserved.
//
// SPDX-License-Identifier: BSD-2-Clause

/// \file engine/googletest_result.hpp
/// Functions and types to process the results of googletest-based test cases.

#if !defined(ENGINE_GOOGLETEST_RESULT_HPP)
#define ENGINE_GOOGLETEST_RESULT_HPP

#include "engine/googletest_result_fwd.hpp"

#include <istream>
#include <ostream>

#include "model/test_result_fwd.hpp"
#include "utils/optional.hpp"
#include "utils/fs/path_fwd.hpp"
#include "utils/process/status_fwd.hpp"

namespace engine {


/// Internal representation of the raw result files of googletest-based tests.
///
/// This class is used exclusively to represent the transient result files read
/// from test cases before generating the "public" version of the result.  This
/// class should actually not be exposed in the header files, but it is for
/// testing purposes only.
class googletest_result {
public:
    /// List of possible types for the test case result.
    enum types {
        broken,
        disabled,
        failed,
        skipped,
        successful,
    };

private:
    /// The test case result.
    types _type;

    /// A description of the test case result.
    ///
    /// Should always be present except for the passed type and sometimes with
    /// the skipped type.
    utils::optional< std::string > _reason;

public:
    googletest_result(const types);
    googletest_result(const types, const std::string&);

    static googletest_result parse(std::istream&);
    static googletest_result load(const utils::fs::path&);

    types type(void) const;
    const utils::optional< std::string >& reason(void) const;

    bool good(void) const;
    googletest_result apply(
        const utils::optional< utils::process::status >&) const;
    model::test_result externalize(void) const;

    bool operator==(const googletest_result&) const;
    bool operator!=(const googletest_result&) const;
};


std::ostream& operator<<(std::ostream&, const googletest_result&);


model::test_result calculate_googletest_result(
    const utils::optional< utils::process::status >&,
    const utils::fs::path&);

/// Internal string for specifying invalid output.
const std::string googletest_invalid_output_message = "invalid output";

/// A bogus identifier for nul reasons provided by the test writer.
///
/// TODO: Support nul messages with skipped results in the schema, etc.
const std::string googletest_bogus_skipped_nul_message = "\n";


}  // namespace engine

#endif  // !defined(ENGINE_GOOGLETEST_RESULT_HPP)
