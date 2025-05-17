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

/// \file model/test_case.hpp
/// Definition of the "test case" concept.

#if !defined(MODEL_TEST_CASE_HPP)
#define MODEL_TEST_CASE_HPP

#include "model/test_case_fwd.hpp"

#include <memory>
#include <ostream>
#include <string>

#include "engine/debugger.hpp"
#include "model/metadata_fwd.hpp"
#include "model/test_result_fwd.hpp"
#include "utils/noncopyable.hpp"
#include "utils/optional_fwd.hpp"


namespace model {


/// Representation of a test case.
///
/// Test cases, on their own, are useless.  They only make sense in the context
/// of the container test program and as such this class should not be used
/// directly.
class test_case {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::shared_ptr< impl > _pimpl;

    test_case(std::shared_ptr< impl >);

public:
    test_case(const std::string&, const metadata&);
    test_case(const std::string&, const std::string&, const test_result&);
    ~test_case(void);

    test_case apply_metadata_defaults(const metadata*) const;

    const std::string& name(void) const;
    metadata get_metadata(void) const;
    const metadata& get_raw_metadata(void) const;
    utils::optional< test_result > fake_result(void) const;

    void attach_debugger(engine::debugger_ptr) const;
    engine::debugger_ptr get_debugger() const;

    bool operator==(const test_case&) const;
    bool operator!=(const test_case&) const;
};


/// Builder for a test_cases_map object.
class test_cases_map_builder : utils::noncopyable {
    /// Accumulator for the map being built.
    test_cases_map _test_cases;

public:
    test_cases_map_builder& add(const test_case&);
    test_cases_map_builder& add(const std::string&);
    test_cases_map_builder& add(const std::string&, const metadata&);

    test_cases_map build(void) const;
};


std::ostream& operator<<(std::ostream&, const test_case&);


}  // namespace model

#endif  // !defined(MODEL_TEST_CASE_HPP)
