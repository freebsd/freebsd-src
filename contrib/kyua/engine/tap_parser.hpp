// Copyright 2015 The Kyua Authors.
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

/// \file engine/tap_parser.hpp
/// Utilities to parse TAP test program output.

#if !defined(ENGINE_TAP_PARSER_HPP)
#define ENGINE_TAP_PARSER_HPP

#include "engine/tap_parser_fwd.hpp"

#include <cstddef>
#include <ostream>
#include <string>

#include "utils/fs/path_fwd.hpp"

namespace engine {


/// TAP plan representing all tests being skipped.
extern const engine::tap_plan all_skipped_plan;


/// TAP output representation and parser.
class tap_summary {
    /// Whether the test program bailed out early or not.
    bool _bailed_out;

    /// The TAP plan.  Only valid if not bailed out.
    tap_plan _plan;

    /// If not empty, the reason why all tests were skipped.
    std::string _all_skipped_reason;

    /// Total number of 'ok' tests.  Only valid if not balied out.
    std::size_t _ok_count;

    /// Total number of 'not ok' tests.  Only valid if not balied out.
    std::size_t _not_ok_count;

    tap_summary(const bool, const tap_plan&, const std::string&,
                const std::size_t, const std::size_t);

public:
    // Yes, these three constructors indicate that we really ought to have three
    // different classes and select between them at runtime.  But doing so would
    // be overly complex for our really simple needs here.
    static tap_summary new_bailed_out(void);
    static tap_summary new_all_skipped(const std::string&);
    static tap_summary new_results(const tap_plan&, const std::size_t,
                                   const std::size_t);

    bool bailed_out(void) const;
    const tap_plan& plan(void) const;
    const std::string& all_skipped_reason(void) const;
    std::size_t ok_count(void) const;
    std::size_t not_ok_count(void) const;

    bool operator==(const tap_summary&) const;
    bool operator!=(const tap_summary&) const;
};


std::ostream& operator<<(std::ostream&, const tap_summary&);


tap_summary parse_tap_output(const utils::fs::path&);


}  // namespace engine


#endif  // !defined(ENGINE_TAP_PARSER_HPP)
