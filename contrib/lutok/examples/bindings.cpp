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

/// \file examples/bindings.cpp
/// Showcases how to define Lua functions from C++ code.
///
/// A major selling point of Lua is that it is very easy too hook native C and
/// C++ functions into the runtime environment so that Lua can call them.  The
/// purpose of this example program is to show how this is done by using Lutok.

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

#include <lutok/exceptions.hpp>
#include <lutok/operations.hpp>
#include <lutok/state.ipp>


/// Calculates the factorial of a given number.
///
/// \param i The postivie number to calculate the factorial of.
///
/// \return The factorial of i.
static int
factorial(const int i)
{
    assert(i >= 0);

    if (i == 0)
        return 1;
    else
        return i * factorial(i - 1);
}


/// A custom factorial function for Lua.
///
/// \pre stack(-1) contains the number to calculate the factorial of.
/// \post stack(-1) contains the result of the operation.
///
/// \param state The Lua state from which to get the function arguments and into
///     which to push the results.
///
/// \return The number of results pushed onto the stack, i.e. 1.
///
/// \throw std::runtime_error If the input parameters are invalid.  Note that
///     Lutok will convert this exception to lutok::error.
static int
lua_factorial(lutok::state& state)
{
    if (!state.is_number(-1))
        throw std::runtime_error("Argument to factorial must be an integer");
    const int i = state.to_integer(-1);
    if (i < 0)
        throw std::runtime_error("Argument to factorial must be positive");
    state.push_integer(factorial(i));
    return 1;
}


/// Program's entry point.
///
/// \param argc Length of argv.  Must be 2.
/// \param argv Command-line arguments to the program.  The first argument to
///     the tool has to be a number.
///
/// \return A system exit code.
int
main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: bindings <number>\n";
        return EXIT_FAILURE;
    }

    // Create a new Lua session and load the print() function.
    lutok::state state;
    state.open_base();

    // Construct a 'module' that contains an entry point to our native factorial
    // function.  A module is just a Lua table that contains a mapping of names
    // to functions.  Instead of creating a module by using our create_module()
    // helper function, we could have used push_cxx_function on the state to
    // define the function ourselves.
    std::map< std::string, lutok::cxx_function > module;
    module["factorial"] = lua_factorial;
    lutok::create_module(state, "native", module);

    // Use a little Lua script to call our native factorial function providing
    // it the first argument passed to the program.  Note that this will error
    // out in a controlled manner if the passed argument is not an integer.  The
    // important thing to notice is that the exception comes from our own C++
    // binding and that it has been converted to a lutok::error.
    std::ostringstream script;
    script << "print(native.factorial(" << argv[1] << "))";
    try {
        lutok::do_string(state, script.str(), 0, 0, 0);
        return EXIT_SUCCESS;
    } catch (const lutok::error& e) {
        std::cerr << "ERROR: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
