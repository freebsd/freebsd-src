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

#include "utils/cmdline/ui_mock.hpp"

#include <iostream>

#include "utils/optional.ipp"

using utils::cmdline::ui_mock;
using utils::none;
using utils::optional;


/// Constructs a new mock UI.
///
/// \param screen_width_ The width of the screen to use for testing purposes.
///     Defaults to 0 to prevent uncontrolled wrapping on our tests.
ui_mock::ui_mock(const std::size_t screen_width_) :
    _screen_width(screen_width_)
{
}


/// Writes a line to stderr and records it for further inspection.
///
/// \param message The line to print and record, without the trailing newline
///     character.
/// \param newline Whether to append a newline to the message or not.
void
ui_mock::err(const std::string& message, const bool newline)
{
    if (newline)
        std::cerr << message << "\n";
    else {
        std::cerr << message << "\n";
        std::cerr.flush();
    }
    _err_log.push_back(message);
}


/// Writes a line to stdout and records it for further inspection.
///
/// \param message The line to print and record, without the trailing newline
///     character.
/// \param newline Whether to append a newline to the message or not.
void
ui_mock::out(const std::string& message, const bool newline)
{
    if (newline)
        std::cout << message << "\n";
    else {
        std::cout << message << "\n";
        std::cout.flush();
    }
    _out_log.push_back(message);
}


/// Queries the width of the screen.
///
/// \return Always none, as we do not want to depend on line wrapping in our
/// tests.
optional< std::size_t >
ui_mock::screen_width(void) const
{
    return _screen_width > 0 ? optional< std::size_t >(_screen_width) : none;
}


/// Gets all the lines written to stderr.
///
/// \return The printed lines.
const std::vector< std::string >&
ui_mock::err_log(void) const
{
    return _err_log;
}


/// Gets all the lines written to stdout.
///
/// \return The printed lines.
const std::vector< std::string >&
ui_mock::out_log(void) const
{
    return _out_log;
}
