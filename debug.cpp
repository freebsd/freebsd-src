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

#include <lutok/c_gate.hpp>
#include <lutok/debug.hpp>
#include <lutok/exceptions.hpp>
#include <lutok/state.ipp>


/// Internal implementation for lutok::debug.
struct lutok::debug::impl {
    /// The Lua internal debug state.
    lua_Debug lua_debug;
};


/// Constructor for an empty debug structure.
lutok::debug::debug(void) :
    _pimpl(new impl())
{
}


/// Destructor.
lutok::debug::~debug(void)
{
}


/// Wrapper around lua_getinfo.
///
/// \param s The Lua state.
/// \param what_ The second parameter to lua_getinfo.
///
/// \warning Terminates execution if there is not enough memory to manipulate
/// the Lua stack.
void
lutok::debug::get_info(state& s, const std::string& what_)
{
    lua_State* raw_state = state_c_gate(s).c_state();

    if (lua_getinfo(raw_state, what_.c_str(), &_pimpl->lua_debug) == 0)
        throw lutok::api_error::from_stack(s, "lua_getinfo");
}


/// Wrapper around lua_getstack.
///
/// \param s The Lua state.
/// \param level The second parameter to lua_getstack.
void
lutok::debug::get_stack(state& s, const int level)
{
    lua_State* raw_state = state_c_gate(s).c_state();

    lua_getstack(raw_state, level, &_pimpl->lua_debug);
}


/// Accessor for the 'event' field of lua_Debug.
///
/// \return Returns the 'event' field of the internal lua_Debug structure.
int
lutok::debug::event(void) const
{
    return _pimpl->lua_debug.event;
}


/// Accessor for the 'name' field of lua_Debug.
///
/// \return Returns the 'name' field of the internal lua_Debug structure.
std::string
lutok::debug::name(void) const
{
    assert(_pimpl->lua_debug.name != NULL);
    return _pimpl->lua_debug.name;
}


/// Accessor for the 'namewhat' field of lua_Debug.
///
/// \return Returns the 'namewhat' field of the internal lua_Debug structure.
std::string
lutok::debug::name_what(void) const
{
    assert(_pimpl->lua_debug.namewhat != NULL);
    return _pimpl->lua_debug.namewhat;
}


/// Accessor for the 'what' field of lua_Debug.
///
/// \return Returns the 'what' field of the internal lua_Debug structure.
std::string
lutok::debug::what(void) const
{
    assert(_pimpl->lua_debug.what != NULL);
    return _pimpl->lua_debug.what;
}


/// Accessor for the 'source' field of lua_Debug.
///
/// \return Returns the 'source' field of the internal lua_Debug structure.
std::string
lutok::debug::source(void) const
{
    assert(_pimpl->lua_debug.source != NULL);
    return _pimpl->lua_debug.source;
}


/// Accessor for the 'currentline' field of lua_Debug.
///
/// \return Returns the 'currentline' field of the internal lua_Debug structure.
int
lutok::debug::current_line(void) const
{
    return _pimpl->lua_debug.currentline;
}


/// Accessor for the 'nups' field of lua_Debug.
///
/// \return Returns the 'nups' field of the internal lua_Debug structure.
int
lutok::debug::n_ups(void) const
{
    return _pimpl->lua_debug.nups;
}


/// Accessor for the 'linedefined' field of lua_Debug.
///
/// \return Returns the 'linedefined' field of the internal lua_Debug structure.
int
lutok::debug::line_defined(void) const
{
    return _pimpl->lua_debug.linedefined;
}


/// Accessor for the 'lastlinedefined' field of lua_Debug.
///
/// \return Returns the 'lastlinedefined' field of the internal lua_Debug
/// structure.
int
lutok::debug::last_line_defined(void) const
{
    return _pimpl->lua_debug.lastlinedefined;
}


/// Accessor for the 'short_src' field of lua_Debug.
///
/// \return Returns the 'short_src' field of the internal lua_Debug structure.
std::string
lutok::debug::short_src(void) const
{
    assert(_pimpl->lua_debug.short_src != NULL);
    return _pimpl->lua_debug.short_src;
}
