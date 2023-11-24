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

/// \file drivers/list_tests.hpp
/// Driver to obtain a list of test cases out of a test suite.
///
/// This driver module implements the logic to extract a list of test cases out
/// of a particular test suite.

#if !defined(DRIVERS_LIST_TESTS_HPP)
#define DRIVERS_LIST_TESTS_HPP

#include <set>
#include <string>

#include "engine/filters_fwd.hpp"
#include "model/test_program_fwd.hpp"
#include "utils/config/tree_fwd.hpp"
#include "utils/fs/path_fwd.hpp"
#include "utils/optional_fwd.hpp"

namespace drivers {
namespace list_tests {


/// Abstract definition of the hooks for this driver.
class base_hooks {
public:
    virtual ~base_hooks(void) = 0;

    /// Called when a test case is identified in a test suite.
    ///
    /// \param test_program The test program containing the test case.
    /// \param test_case_name The name of the located test case.
    virtual void got_test_case(const model::test_program& test_program,
                               const std::string& test_case_name) = 0;
};


/// Tuple containing the results of this driver.
class result {
public:
    /// Filters that did not match any available test case.
    ///
    /// The presence of any filters here probably indicates a usage error.  If a
    /// test filter does not match any test case, it is probably a typo.
    std::set< engine::test_filter > unused_filters;

    /// Initializer for the tuple's fields.
    ///
    /// \param unused_filters_ The filters that did not match any test case.
    result(const std::set< engine::test_filter >& unused_filters_) :
        unused_filters(unused_filters_)
    {
    }
};


result drive(const utils::fs::path&, const utils::optional< utils::fs::path >,
             const std::set< engine::test_filter >&,
             const utils::config::tree&, base_hooks&);


}  // namespace list_tests
}  // namespace drivers

#endif  // !defined(DRIVERS_LIST_TESTS_HPP)
