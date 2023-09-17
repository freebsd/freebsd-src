// Copyright 2012 Google Inc.
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

/// \file examples/interpreter.cpp
/// Implementation of a basic command-line Lua interpreter.

#include <cstdlib>
#include <iostream>
#include <string>

#include <lutok/exceptions.hpp>
#include <lutok/operations.hpp>
#include <lutok/state.ipp>


/// Executes a Lua statement provided by the user with error checking.
///
/// \param state The Lua state in which to process the statement.
/// \param line The textual statement provided by the user.
static void
run_statement(lutok::state& state, const std::string& line)
{
    try {
        // This utility function allows us to feed a given piece of Lua code to
        // the interpreter and process it.  The piece of code can include
        // multiple statements separated by a semicolon or by a newline
        // character.
        lutok::do_string(state, line, 0, 0, 0);
    } catch (const lutok::error& error) {
        std::cerr << "ERROR: " << error.what() << '\n';
    }
}


/// Program's entry point.
///
/// \return A system exit code.
int
main(void)
{
    // Create a new session and load some standard libraries.
    lutok::state state;
    state.open_base();
    state.open_string();
    state.open_table();

    for (;;) {
        std::cout << "lua> ";
        std::cout.flush();

        std::string line;
        if (!std::getline(std::cin, line).good())
            break;
        run_statement(state, line);
    }

    return EXIT_SUCCESS;
}
