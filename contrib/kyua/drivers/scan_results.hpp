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

/// \file drivers/scan_results.hpp
/// Driver to scan the contents of a results file.
///
/// This driver module implements the logic to scan the contents of a results
/// file and to notify the presentation layer as soon as data becomes
/// available.  This is to prevent reading all the data from the file at once,
/// which could take too much memory.

#if !defined(DRIVERS_SCAN_RESULTS_HPP)
#define DRIVERS_SCAN_RESULTS_HPP

extern "C" {
#include <stdint.h>
}

#include <set>

#include "engine/filters.hpp"
#include "model/context_fwd.hpp"
#include "store/read_transaction_fwd.hpp"
#include "utils/datetime_fwd.hpp"
#include "utils/fs/path_fwd.hpp"

namespace drivers {
namespace scan_results {


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


/// Abstract definition of the hooks for this driver.
class base_hooks {
public:
    virtual ~base_hooks(void) = 0;

    virtual void begin(void);

    /// Callback executed when the context is loaded.
    ///
    /// \param context The context loaded from the database.
    virtual void got_context(const model::context& context) = 0;

    /// Callback executed when a test results is found.
    ///
    /// \param iter Container for the test result's data.  Some of the data are
    ///     lazily fetched, hence why we receive the object instead of the
    ///     individual elements.
    virtual void got_result(store::results_iterator& iter) = 0;

    virtual void end(const result& r);
};


result drive(const utils::fs::path&, const std::set< engine::test_filter >&,
             base_hooks&);


}  // namespace scan_results
}  // namespace drivers

#endif  // !defined(DRIVERS_SCAN_RESULTS_HPP)
