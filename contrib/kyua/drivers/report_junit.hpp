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

/// \file drivers/report_junit.hpp
/// Generates a JUnit report out of a test suite execution.

#if !defined(ENGINE_REPORT_JUNIT_HPP)
#define ENGINE_REPORT_JUNIT_HPP

#include <ostream>
#include <string>

#include "drivers/scan_results.hpp"
#include "model/metadata_fwd.hpp"
#include "model/test_program_fwd.hpp"
#include "utils/datetime_fwd.hpp"

namespace drivers {


extern const char* const junit_metadata_header;
extern const char* const junit_timing_header;
extern const char* const junit_stderr_header;


std::string junit_classname(const model::test_program&);
std::string junit_duration(const utils::datetime::delta&);
std::string junit_metadata(const model::metadata&);
std::string junit_timing(const utils::datetime::timestamp&,
                         const utils::datetime::timestamp&);


/// Hooks for the scan_results driver to generate a JUnit report.
class report_junit_hooks : public drivers::scan_results::base_hooks {
    /// Stream to which to write the report.
    std::ostream& _output;

public:
    report_junit_hooks(std::ostream&);

    void got_context(const model::context&);
    void got_result(store::results_iterator&);

    void end(const drivers::scan_results::result&);
};


}  // namespace drivers

#endif  // !defined(ENGINE_REPORT_JUNIT_HPP)
