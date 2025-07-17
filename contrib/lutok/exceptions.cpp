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

#include "c_gate.hpp"
#include "exceptions.hpp"
#include "state.ipp"


/// Constructs a new error with a plain-text message.
///
/// \param message The plain-text error message.
lutok::error::error(const std::string& message) :
    std::runtime_error(message)
{
}


/// Destructor for the error.
lutok::error::~error(void) throw()
{
}


/// Constructs a new error.
///
/// \param api_function_ The name of the API function that caused the error.
/// \param message The plain-text error message provided by Lua.
lutok::api_error::api_error(const std::string& api_function_,
                            const std::string& message) :
    error(message),
    _api_function(api_function_)
{
}


/// Destructor for the error.
lutok::api_error::~api_error(void) throw()
{
}


/// Constructs a new api_error with the message on the top of the Lua stack.
///
/// \pre There is an error message on the top of the stack.
/// \post The error message is popped from the stack.
///
/// \param state_ The Lua state.
/// \param api_function_ The name of the Lua API function that caused the error.
///
/// \return A new api_error with the popped message.
lutok::api_error
lutok::api_error::from_stack(state& state_, const std::string& api_function_)
{
    lua_State* raw_state = lutok::state_c_gate(state_).c_state();

    assert(lua_isstring(raw_state, -1));
    const std::string message = lua_tostring(raw_state, -1);
    lua_pop(raw_state, 1);
    return lutok::api_error(api_function_, message);
}


/// Gets the name of the Lua API function that caused this error.
///
/// \return The name of the function.
const std::string&
lutok::api_error::api_function(void) const
{
    return _api_function;
}
 

/// Constructs a new error.
///
/// \param filename_ The file that count not be found.
lutok::file_not_found_error::file_not_found_error(
    const std::string& filename_) :
    error("File '" + filename_ + "' not found"),
    _filename(filename_)
{
}


/// Destructor for the error.
lutok::file_not_found_error::~file_not_found_error(void) throw()
{
}


/// Gets the name of the file that could not be found.
///
/// \return The name of the file.
const std::string&
lutok::file_not_found_error::filename(void) const
{
    return _filename;
}
