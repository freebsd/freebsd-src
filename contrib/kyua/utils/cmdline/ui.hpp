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

/// \file utils/cmdline/ui.hpp
/// Abstractions and utilities to write formatted messages to the console.

#if !defined(UTILS_CMDLINE_UI_HPP)
#define UTILS_CMDLINE_UI_HPP

#include "utils/cmdline/ui_fwd.hpp"

#include <cstddef>
#include <string>

#include "utils/optional_fwd.hpp"
#include "utils/text/table_fwd.hpp"

namespace utils {
namespace cmdline {


/// Interface to interact with the CLI.
///
/// The main purpose of this class is to substitute direct usages of stdout and
/// stderr.  An instance of this class is passed to every command of a CLI,
/// which allows unit testing and validation of the interaction with the user.
///
/// This class writes directly to stdout and stderr.  For testing purposes, see
/// the utils::cmdline::ui_mock class.
class ui {
public:
    virtual ~ui(void);

    virtual void err(const std::string&, const bool = true);
    virtual void out(const std::string&, const bool = true);
    virtual optional< std::size_t > screen_width(void) const;

    void out_wrap(const std::string&);
    void out_tag_wrap(const std::string&, const std::string&,
                      const bool = true);
    void out_table(const utils::text::table&, utils::text::table_formatter,
                   const std::string&);
};


void print_error(ui*, const std::string&);
void print_info(ui*, const std::string&);
void print_warning(ui*, const std::string&);


}  // namespace cmdline
}  // namespace utils

#endif  // !defined(UTILS_CMDLINE_UI_HPP)
