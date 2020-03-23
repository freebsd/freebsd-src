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

/// \file engine/scanner.hpp
/// Utilities to scan through list of tests in a test suite.

#if !defined(ENGINE_SCANNER_HPP)
#define ENGINE_SCANNER_HPP

#include "engine/scanner_fwd.hpp"

#include <memory>
#include <set>

#include "engine/filters_fwd.hpp"
#include "model/test_program_fwd.hpp"
#include "utils/optional_fwd.hpp"

namespace engine {


/// Scans a list of test programs, yielding one test case at a time.
///
/// This class contains the state necessary to process a collection of test
/// programs (possibly as provided by the Kyuafile) and to extract an arbitrary
/// (test program, test_case) pair out of them one at a time.
///
/// The scanning algorithm guarantees that test programs are initialized
/// dynamically, should they need to load their list of test cases from disk.
///
/// The order of the extraction is not guaranteed.
class scanner {
    struct impl;
    /// Pointer to the internal implementation data.
    std::shared_ptr< impl > _pimpl;

public:
    scanner(const model::test_programs_vector&, const std::set< test_filter >&);
    ~scanner(void);

    bool done(void);
    utils::optional< scan_result > yield(void);

    std::set< test_filter > unused_filters(void) const;
};


}  // namespace engine


#endif  // !defined(ENGINE_SCANNER_HPP)
