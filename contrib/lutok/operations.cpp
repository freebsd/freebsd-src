// Copyright 2011 Google Inc.
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

#include <cassert>

#include <lua.hpp>

#include "exceptions.hpp"
#include "operations.hpp"
#include "stack_cleaner.hpp"
#include "state.hpp"


/// Creates a module: i.e. a table with a set of methods in it.
///
/// \param s The Lua state.
/// \param name The name of the module to create.
/// \param members The list of member functions to add to the module.
void
lutok::create_module(state& s, const std::string& name,
                     const std::map< std::string, cxx_function >& members)
{
    stack_cleaner cleaner(s);
    s.new_table();
    for (std::map< std::string, cxx_function >::const_iterator
         iter = members.begin(); iter != members.end(); iter++) {
        s.push_string((*iter).first);
        s.push_cxx_function((*iter).second);
        s.set_table(-3);
    }
    s.set_global(name);
}


/// Loads and processes a Lua file.
///
/// This is a replacement for luaL_dofile but with proper error reporting
/// and stack control.
///
/// \param s The Lua state.
/// \param file The file to load.
/// \param nargs The number of arguments on the stack to pass to the file.
/// \param nresults The number of results to expect; -1 for any.
/// \param errfunc If not 0, index of a function in the stack to act as an
///     error handler.
///
/// \return The number of results left on the stack.
///
/// \throw error If there is a problem processing the file.
unsigned int
lutok::do_file(state& s, const std::string& file, const int nargs,
               const int nresults, const int errfunc)
{
    assert(nresults >= -1);
    const int height = s.get_top() - nargs;

    try {
        s.load_file(file);
        if (nargs > 0)
            s.insert(-nargs - 1);
        s.pcall(nargs, nresults == -1 ? LUA_MULTRET : nresults,
                errfunc == 0 ? 0 : errfunc - 1);
    } catch (const lutok::api_error& e) {
        throw lutok::error("Failed to load Lua file '" + file + "': " +
                           e.what());
    }

    const int actual_results = s.get_top() - height;
    assert(nresults == -1 || actual_results == nresults);
    assert(actual_results >= 0);
    return static_cast< unsigned int >(actual_results);
}


/// Processes a Lua script.
///
/// This is a replacement for luaL_dostring but with proper error reporting
/// and stack control.
///
/// \param s The Lua state.
/// \param str The string to process.
/// \param nargs The number of arguments on the stack to pass to the chunk.
/// \param nresults The number of results to expect; -1 for any.
/// \param errfunc If not 0, index of a function in the stack to act as an
///     error handler.
///
/// \return The number of results left on the stack.
///
/// \throw error If there is a problem processing the string.
unsigned int
lutok::do_string(state& s, const std::string& str, const int nargs,
                 const int nresults, const int errfunc)
{
    assert(nresults >= -1);
    const int height = s.get_top() - nargs;

    try {
        s.load_string(str);
        if (nargs > 0)
            s.insert(-nargs - 1);
        s.pcall(nargs, nresults == -1 ? LUA_MULTRET : nresults,
                errfunc == 0 ? 0 : errfunc - 1);
    } catch (const lutok::api_error& e) {
        throw lutok::error("Failed to process Lua string '" + str + "': " +
                           e.what());
    }

    const int actual_results = s.get_top() - height;
    assert(nresults == -1 || actual_results == nresults);
    assert(actual_results >= 0);
    return static_cast< unsigned int >(actual_results);
}


/// Convenience function to evaluate a Lua expression.
///
/// \param s The Lua state.
/// \param expression The textual expression to evaluate.
/// \param nresults The number of results to leave on the stack.  Must be
///     positive.
///
/// \throw api_error If there is a problem evaluating the expression.
void
lutok::eval(state& s, const std::string& expression, const int nresults)
{
    assert(nresults > 0);
    do_string(s, "return " + expression, 0, nresults, 0);
}
