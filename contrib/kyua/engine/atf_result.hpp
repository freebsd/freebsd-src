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

/// \file engine/atf_result.hpp
/// Functions and types to process the results of ATF-based test cases.

#if !defined(ENGINE_ATF_RESULT_HPP)
#define ENGINE_ATF_RESULT_HPP

#include "engine/atf_result_fwd.hpp"

#include <istream>
#include <ostream>

#include "model/test_result_fwd.hpp"
#include "utils/optional.hpp"
#include "utils/fs/path_fwd.hpp"
#include "utils/process/status_fwd.hpp"

namespace engine {


/// Internal representation of the raw result files of ATF-based tests.
///
/// This class is used exclusively to represent the transient result files read
/// from test cases before generating the "public" version of the result.  This
/// class should actually not be exposed in the header files, but it is for
/// testing purposes only.
class atf_result {
public:
    /// List of possible types for the test case result.
    enum types {
        broken,
        expected_death,
        expected_exit,
        expected_failure,
        expected_signal,
        expected_timeout,
        failed,
        passed,
        skipped,
    };

private:
    /// The test case result.
    types _type;

    /// The optional integral argument that may accompany the result.
    ///
    /// Should only be present if the type is expected_exit or expected_signal.
    utils::optional< int > _argument;

    /// A description of the test case result.
    ///
    /// Should always be present except for the passed type.
    utils::optional< std::string > _reason;

public:
    atf_result(const types);
    atf_result(const types, const std::string&);
    atf_result(const types, const utils::optional< int >&, const std::string&);

    static atf_result parse(std::istream&);
    static atf_result load(const utils::fs::path&);

    types type(void) const;
    const utils::optional< int >& argument(void) const;
    const utils::optional< std::string >& reason(void) const;

    bool good(void) const;
    atf_result apply(const utils::optional< utils::process::status >&) const;
    model::test_result externalize(void) const;

    bool operator==(const atf_result&) const;
    bool operator!=(const atf_result&) const;
};


std::ostream& operator<<(std::ostream&, const atf_result&);


model::test_result calculate_atf_result(
    const utils::optional< utils::process::status >&,
    const utils::fs::path&);


}  // namespace engine

#endif  // !defined(ENGINE_ATF_IFACE_RESULTS_HPP)
