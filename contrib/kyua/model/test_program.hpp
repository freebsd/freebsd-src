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

/// \file model/test_program.hpp
/// Definition of the "test program" concept.

#if !defined(MODEL_TEST_PROGRAM_HPP)
#define MODEL_TEST_PROGRAM_HPP

#include "model/test_program_fwd.hpp"

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "model/metadata_fwd.hpp"
#include "model/test_case_fwd.hpp"
#include "utils/fs/path_fwd.hpp"
#include "utils/noncopyable.hpp"

namespace model {


/// Representation of a test program.
class test_program {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::shared_ptr< impl > _pimpl;

protected:
    void set_test_cases(const model::test_cases_map&);

public:
    test_program(const std::string&, const utils::fs::path&,
                 const utils::fs::path&, const std::string&,
                 const model::metadata&, const model::test_cases_map&);
    virtual ~test_program(void);

    const std::string& interface_name(void) const;
    const utils::fs::path& root(void) const;
    const utils::fs::path& relative_path(void) const;
    const utils::fs::path absolute_path(void) const;
    const std::string& test_suite_name(void) const;
    const model::metadata& get_metadata(void) const;

    const model::test_case& find(const std::string&) const;
    virtual const model::test_cases_map& test_cases(void) const;

    bool operator==(const test_program&) const;
    bool operator!=(const test_program&) const;
    bool operator<(const test_program&) const;
};


std::ostream& operator<<(std::ostream&, const test_program&);


/// Builder for a test_program object.
class test_program_builder : utils::noncopyable {
    struct impl;

    /// Pointer to the shared internal implementation.
    std::unique_ptr< impl > _pimpl;

public:
    test_program_builder(const std::string&, const utils::fs::path&,
                         const utils::fs::path&, const std::string&);
    ~test_program_builder(void);

    test_program_builder& add_test_case(const std::string&);
    test_program_builder& add_test_case(const std::string&,
                                        const model::metadata&);

    test_program_builder& set_metadata(const model::metadata&);

    test_program build(void) const;
    test_program_ptr build_ptr(void) const;
};


}  // namespace model

#endif  // !defined(MODEL_TEST_PROGRAM_HPP)
