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

/// \file utils/cmdline/ui_mock.hpp
/// Provides the utils::cmdline::ui_mock class.
///
/// This file is only supposed to be included from test program, never from
/// production code.

#if !defined(UTILS_CMDLINE_UI_MOCK_HPP)
#define UTILS_CMDLINE_UI_MOCK_HPP

#include <cstddef>
#include <string>
#include <vector>

#include "utils/cmdline/ui.hpp"

namespace utils {
namespace cmdline {


/// Testable interface to interact with the CLI.
///
/// This class records all writes to stdout and stderr to allow further
/// inspection for testing purposes.
class ui_mock : public ui {
    /// Fake width of the screen; if 0, represents none.
    std::size_t _screen_width;

    /// Messages sent to stderr.
    std::vector< std::string > _err_log;

    /// Messages sent to stdout.
    std::vector< std::string > _out_log;

public:
    ui_mock(const std::size_t = 0);

    void err(const std::string&, const bool = true);
    void out(const std::string&, const bool = true);
    optional< std::size_t > screen_width(void) const;

    const std::vector< std::string >& err_log(void) const;
    const std::vector< std::string >& out_log(void) const;
};


}  // namespace cmdline
}  // namespace utils


#endif  // !defined(UTILS_CMDLINE_UI_MOCK_HPP)
