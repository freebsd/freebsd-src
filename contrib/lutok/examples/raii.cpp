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

/// \file examples/raii.cpp
/// Demonstrates how RAII helps in keeping the Lua state consistent.
///
/// One of the major complains that is raised against the Lua C API is that it
/// is very hard to ensure it remains consistent during the execution of the
/// program.  In the case of native C code, there exist many tools that help the
/// developer catch memory leaks, access to uninitialized variables, etc.
/// However, when using the Lua C API, none of these tools can validate that,
/// for example, the Lua stack remains balanced across calls.
///
/// Enter RAII.  The RAII pattern, intensively applied by Lutok, helps the
/// developer in maintaining the Lua state consistent at all times in a
/// transparent manner.  This example program attempts to illustrate this.

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

#include <lutok/operations.hpp>
#include <lutok/stack_cleaner.hpp>
#include <lutok/state.ipp>


/// Prints the string-typed field of a table.
///
/// If the field contains a string, this function prints its value.  If the
/// field contains any other type, this prints an error message.
///
/// \pre The top of the Lua stack in 'state' references a table.
///
/// \param state The Lua state.
/// \param field The name of the string-typed field.
static void
print_table_field(lutok::state& state, const std::string& field)
{
    assert(state.is_table(-1));

    // Bring in some RAII magic: the stack_cleaner object captures the current
    // height of the Lua stack at this point.  Whenever the object goes out of
    // scope, it will pop as many entries from the stack as necessary to restore
    // the stack to its previous level.
    //
    // This ensures that, no matter how we exit the function, we do not leak
    // objects in the stack.
    lutok::stack_cleaner cleaner(state);

    // Stack contents: -1: table.
    state.push_string(field);
    // Stack contents: -2: table, -1: field name.
    state.get_table(-2);
    // Stack contents: -2: table, -1: field value.

    if (!state.is_string(-1)) {
        std::cout << "The field " << field << " does not contain a string\n";
        // Stack contents: -2: table, -1: field value.
        //
        // This is different than when we started!  We should pop our extra
        // value from the stack at this point.  However, it is extremely common
        // for software to have bugs (in this case, leaks) in error paths,
        // mostly because such code paths are rarely exercised.
        //
        // By using the stack_cleaner object, we can be confident that the Lua
        // stack will be cleared for us at this point, no matter what happened
        // earlier on the stack nor how we exit the function.
        return;
    }

    std::cout << "String in field " << field << ": " << state.to_string(-1)
              << '\n';
    // A well-behaved program explicitly pops anything extra from the stack to
    // return it to its original state.  Mostly for clarity.
    state.pop(1);

    // Stack contents: -1: table.  Same as when we started.
}


/// Program's entry point.
///
/// \return A system exit code.
int
main(void)
{
    lutok::state state;
    state.open_base();

    lutok::do_string(state, "example = {foo='hello', bar=123, baz='bye'}",
                     0, 0, 0);

    state.get_global("example");
    print_table_field(state, "foo");
    print_table_field(state, "bar");
    print_table_field(state, "baz");
    state.pop(1);

    return EXIT_SUCCESS;
}
